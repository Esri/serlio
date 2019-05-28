#include "PRTModifierAction.h"
#include "PRTModifierCommand.h"
#include "PRTModifierNode.h"
#include "node/PRTMaterialNode.h"
#include "node/Utilities.h"

#include <maya/MFnPlugin.h>
#include <maya/MSceneMessage.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif



namespace {
	const wchar_t*      PRT_EXT_SUBDIR = L"ext";
	const prt::LogLevel PRT_LOG_LEVEL = prt::LOG_DEBUG;
	const bool          ENABLE_LOG_CONSOLE = true;
	const bool          ENABLE_LOG_FILE = false;

	std::wstring getProcessTempDir() {
        std::wstring tp = prtu::temp_directory_path();
		wchar_t sep = prtu::getDirSeparator<wchar_t>();
		if (tp[tp.size()-1] != sep)
			tp += sep;
        std::wstring n = std::wstring(L"maya_") +
#ifdef _WIN32
			std::to_wstring(::_getpid()); //prevent warning in win32
#else
			std::to_wstring(::getpid());
#endif
        return { tp.append(n) };
    }
} // namespace

static void mayaExiting(void*)
{
	uninitializePlugin(MObject::kNullObj);
}

// called when the plug-in is loaded into Maya.
MStatus initializePlugin(MObject obj)
{

	if (ENABLE_LOG_CONSOLE) {
		PRTModifierAction::theLogHandler = prt::ConsoleLogHandler::create(prt::LogHandler::ALL, prt::LogHandler::ALL_COUNT);
		prt::addLogHandler(PRTModifierAction::theLogHandler);
	}

	if (ENABLE_LOG_FILE) {
		const std::string logPath = PRTModifierAction::getPluginRoot() + prtu::getDirSeparator<char>() + "prt4maya.log";
		std::wstring wLogPath(logPath.length(), L' ');
		std::copy(logPath.begin(), logPath.end(), wLogPath.begin());
		PRTModifierAction::theFileLogHandler = prt::FileLogHandler::create(prt::LogHandler::ALL, prt::LogHandler::ALL_COUNT, wLogPath.c_str());
		prt::addLogHandler(PRTModifierAction::theFileLogHandler);
	}


	prtu::dbg("initialized prt logger");

	const std::string& pluginRoot = PRTModifierAction::getPluginRoot();
	std::wstring wPluginRoot(pluginRoot.length(), L' ');
	std::copy(pluginRoot.begin(), pluginRoot.end(), wPluginRoot.begin());

	const std::wstring prtExtPath = wPluginRoot + PRT_EXT_SUBDIR;
	prtu::wdbg(L"looking for prt extensions at %ls", prtExtPath.c_str());

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const wchar_t* prtExtPathPOD = prtExtPath.c_str();
	PRTModifierAction::thePRT = prt::init(&prtExtPathPOD, 1, PRT_LOG_LEVEL, &status);

	if (PRTModifierAction::thePRT == nullptr || status != prt::STATUS_OK)
		return MS::kFailure;

	PRTModifierAction::theCache = prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT);
    PRTModifierAction::mResolveMapCache = new ResolveMapCache(getProcessTempDir());

	MFnPlugin plugin(obj, "Esri R&D Center Zurich", "1.0", "Any");

	MCHECK(plugin.registerCommand("prtAssign", PRTModifierCommand::creator));

	MCHECK(plugin.registerNode("prt",
		PRTModifierNode::id,
		PRTModifierNode::creator,
		PRTModifierNode::initialize));

	MCHECK(plugin.registerNode("prtMaterial", PRTMaterialNode::id, &PRTMaterialNode::creator, &PRTMaterialNode::initialize, MPxNode::kDependNode));
	MCHECK(plugin.registerUI("prt4mayaCreateUI", "prt4mayaDeleteUI"));

	MStatus mayaStatus = MStatus::kFailure; //maya exit does not call uninitializePlugin, therefore addCallback
	MSceneMessage::addCallback(MSceneMessage::kMayaExiting, mayaExiting, NULL, &mayaStatus);
	MCHECK(mayaStatus);

	return MStatus::kSuccess;
}



// called when the plug-in is unloaded from Maya.
MStatus uninitializePlugin(MObject obj)
{
	if (PRTModifierAction::theCache) {
		PRTModifierAction::theCache->destroy();
		PRTModifierAction::theCache = nullptr;
	}
	if (PRTModifierAction::thePRT) {
		PRTModifierAction::thePRT->destroy();
		PRTModifierAction::thePRT = nullptr;
	}
	if (PRTModifierAction::mResolveMapCache) {
		delete PRTModifierAction::mResolveMapCache;
		PRTModifierAction::mResolveMapCache = nullptr;
	}

	if (ENABLE_LOG_CONSOLE && PRTModifierAction::theLogHandler) {
		prt::removeLogHandler(PRTModifierAction::theLogHandler);
		PRTModifierAction::theLogHandler->destroy();
		PRTModifierAction::theLogHandler = nullptr;
	}
	if (ENABLE_LOG_FILE && PRTModifierAction::theFileLogHandler) {
		prt::removeLogHandler(PRTModifierAction::theFileLogHandler);
		PRTModifierAction::theFileLogHandler->destroy();
		PRTModifierAction::theFileLogHandler = nullptr;
	}

	MStatus   status;

	if (obj != MObject::kNullObj) {
		MFnPlugin plugin(obj);
		MCHECK(plugin.deregisterCommand("prtAssign"));
		MCHECK(plugin.deregisterNode(PRTModifierNode::id));
		MCHECK(plugin.deregisterNode(PRTMaterialNode::id));
	}

	return status;
}
