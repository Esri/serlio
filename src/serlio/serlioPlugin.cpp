/**
 * Serlio - Esri CityEngine Plugin for Autodesk Maya
 *
 * See https://github.com/esri/serlio for build and usage instructions.
 *
 * Copyright (c) 2012-2019 Esri R&D Center Zurich
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "serlioPlugin.h"

#include "prtModifier/PRTModifierAction.h"
#include "prtModifier/PRTModifierCommand.h"
#include "prtModifier/PRTModifierNode.h"

#include "prtMaterial/PRTMaterialNode.h"

#include "util/Utilities.h"
#include "util/MayaUtilities.h"
#include "util/LogHandler.h"

#include "maya/MFnPlugin.h"
#include "maya/MSceneMessage.h"

#ifdef _WIN32
#   include <process.h>
#else
#   include <unistd.h>
#endif


namespace {
	constexpr bool DBG = false;

	constexpr const char* MODIFIER_NODE = "serlio";
	constexpr const char* CMD_ASSIGN    = "serlioAssign";

	const wchar_t*      PRT_EXT_SUBDIR = L"ext";
	const prt::LogLevel PRT_LOG_LEVEL = prt::LOG_DEBUG;
	const bool          ENABLE_LOG_CONSOLE = true;
	const bool          ENABLE_LOG_FILE = false;

	std::wstring getProcessTempDir() {
        std::wstring tp = prtu::temp_directory_path();
		wchar_t sep = prtu::getDirSeparator<wchar_t>();
		if (tp[tp.size()-1] != sep)
			tp += sep;
        std::wstring n = std::wstring(L"serlio_") +
#ifdef _WIN32
			std::to_wstring(::_getpid()); //prevent warning in win32
#else
			std::to_wstring(::getpid());
#endif
        return { tp.append(n) };
    }

} // namespace

// called when the plug-in is loaded into Maya.
MStatus initializePlugin(MObject obj)
{

	if (ENABLE_LOG_CONSOLE) {
		PRTModifierAction::theLogHandler = prt::ConsoleLogHandler::create(prt::LogHandler::ALL, prt::LogHandler::ALL_COUNT);
		prt::addLogHandler(PRTModifierAction::theLogHandler);
	}

	if (ENABLE_LOG_FILE) {
		const std::string logPath = PRTModifierAction::getPluginRoot() + prtu::getDirSeparator<char>() + "serlio.log";
		std::wstring wLogPath(logPath.length(), L' ');
		std::copy(logPath.begin(), logPath.end(), wLogPath.begin());
		PRTModifierAction::theFileLogHandler = prt::FileLogHandler::create(prt::LogHandler::ALL, prt::LogHandler::ALL_COUNT, wLogPath.c_str());
		prt::addLogHandler(PRTModifierAction::theFileLogHandler);
	}

	if (DBG) LOG_DBG << "initialized prt logger";

	const std::string& pluginRoot = PRTModifierAction::getPluginRoot();
	std::wstring wPluginRoot(pluginRoot.length(), L' ');
	std::copy(pluginRoot.begin(), pluginRoot.end(), wPluginRoot.begin());

	const std::wstring prtExtPath = wPluginRoot + PRT_EXT_SUBDIR;
	if (DBG) LOG_DBG << "looking for prt extensions at " << prtExtPath;

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const wchar_t* prtExtPathPOD = prtExtPath.c_str();
	PRTModifierAction::thePRT = prt::init(&prtExtPathPOD, 1, PRT_LOG_LEVEL, &status);

	if (PRTModifierAction::thePRT == nullptr || status != prt::STATUS_OK)
		return MS::kFailure;

	PRTModifierAction::theCache = prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT);
    PRTModifierAction::mResolveMapCache = new ResolveMapCache(getProcessTempDir());

	MFnPlugin plugin(obj, "Esri R&D Center Zurich", SRL_VERSION, "Any");

	MCHECK(plugin.registerCommand(CMD_ASSIGN, PRTModifierCommand::creator));

	MCHECK(plugin.registerNode(MODIFIER_NODE,
		PRTModifierNode::id,
		PRTModifierNode::creator,
		PRTModifierNode::initialize));

	MCHECK(plugin.registerNode("serlioMaterial", PRTMaterialNode::id, &PRTMaterialNode::creator, &PRTMaterialNode::initialize, MPxNode::kDependNode));
	MCHECK(plugin.registerUI("serlioCreateUI", "serlioDeleteUI"));

	MStatus mayaStatus = MStatus::kFailure; //maya exit does not call uninitializePlugin, therefore addCallback
	auto mayaExitCallback = [](void*) { uninitializePlugin(MObject::kNullObj); };
	MSceneMessage::addCallback(MSceneMessage::kMayaExiting, mayaExitCallback, nullptr, &mayaStatus);
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
		MCHECK(plugin.deregisterCommand(CMD_ASSIGN));
		MCHECK(plugin.deregisterNode(PRTModifierNode::id));
		MCHECK(plugin.deregisterNode(PRTMaterialNode::id));
	}

	return status;
}
