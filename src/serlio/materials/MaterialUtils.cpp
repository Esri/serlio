#include "materials/MaterialUtils.h"

#include "utils/MArrayWrapper.h"
#include "utils/MELScriptBuilder.h"
#include "utils/MayaUtilities.h"
#include "utils/Utilities.h"

#include "PRTContext.h"

#include "maya/MDataBlock.h"
#include "maya/MDataHandle.h"
#include "maya/MFileIO.h"
#include "maya/MFnMesh.h"
#include "maya/MPlugArray.h"
#include "maya/MUuid.h"
#include "maya/adskDataAssociations.h"

namespace {
const MELVariable MEL_UNDO_STATE(L"materialUndoState");

constexpr const wchar_t* RGBA8_FORMAT = L"RGBA8";
constexpr const wchar_t* FORMAT_STRING = L"format";

adsk::Data::Structure* createNewMaterialInfoMapStructure() {
	// Register our structure since it is not registered yet.
	adsk::Data::Structure* fStructure = adsk::Data::Structure::create();
	fStructure->setName(PRT_MATERIALINFO_MAP_STRUCTURE.c_str());

	fStructure->addMember(adsk::Data::Member::eDataType::kUInt64, 1, PRT_MATERIALINFO_MAP_KEY.c_str());
	fStructure->addMember(adsk::Data::Member::eDataType::kString, 1, PRT_MATERIALINFO_MAP_VALUE.c_str());

	adsk::Data::Structure::registerStructure(*fStructure);

	return fStructure;
}

adsk::Data::Handle getMaterialInfoMapHandle(const adsk::Data::Structure* fStructure, size_t materialInfoHash,
                                            const MUuid& shadingEngineUuid, MStatus& status) {
	adsk::Data::Handle handle(*fStructure);

	handle.setPositionByMemberName(PRT_MATERIALINFO_MAP_KEY.c_str());
	uint64_t* materialInfoHashPtr = handle.asUInt64();
	if (materialInfoHashPtr == nullptr) {
		LOG_ERR << "Failed to parse handle value as UInt64";
		status = MS::kFailure;
		return handle;
	}
	*materialInfoHashPtr = materialInfoHash;

	handle.setPositionByMemberName(PRT_MATERIALINFO_MAP_VALUE.c_str());
	std::string errors;
	if (0 != handle.fromStr(shadingEngineUuid.asString().asChar(), 0, errors)) {
		LOG_ERR << "Failed to parse handle value from string: " << errors;
		status = MS::kFailure;
		return handle;
	}

	status = MS::kSuccess;
	return handle;
}

adsk::Data::IndexCount getMaterialInfoMapIndex(const adsk::Data::Stream& stream, const size_t materialInfoHash) {
	// Check if there is an obsolete matching duplicate.
	for (adsk::Data::Stream::iterator iterator = stream.cbegin(); iterator != stream.cend(); ++iterator) {
		iterator->setPositionByMemberName(PRT_MATERIALINFO_MAP_KEY.c_str());
		size_t* hashPtr = iterator->asUInt64();

		if ((hashPtr != nullptr) && (*hashPtr == materialInfoHash))
			return iterator.index();
	}

	adsk::Data::IndexCount elementCount = stream.elementCount();

	// Check if there is an unused index in the defined range.
	for (adsk::Data::IndexCount i = 0; i < elementCount; ++i) {
		if (!stream.hasElement(i)) {
			return i;
		}
	}

	return elementCount;
}
} // namespace

namespace MaterialUtils {

void forwardGeometry(const MObject& aInMesh, const MObject& aOutMesh, MDataBlock& data) {
	MStatus status;

	const MDataHandle inMeshHandle = data.inputValue(aInMesh, &status);
	MCHECK(status);

	MDataHandle outMeshHandle = data.outputValue(aOutMesh, &status);
	MCHECK(status);

	status = outMeshHandle.set(inMeshHandle.asMesh());
	MCHECK(status);

	outMeshHandle.setClean();
}

adsk::Data::Stream* getMaterialStream(const MObject& aInMesh, MDataBlock& data) {
	MStatus status;

	const MDataHandle inMeshHandle = data.inputValue(aInMesh, &status);
	MCHECK(status);

	const MFnMesh inMesh(inMeshHandle.asMesh(), &status);
	MCHECK(status);

	const adsk::Data::Associations* inMetadata = inMesh.metadata(&status);
	MCHECK(status);
	if (inMetadata == nullptr)
		return nullptr;

	adsk::Data::Associations inAssociations(inMetadata);
	adsk::Data::Channel* inMatChannel = inAssociations.findChannel(PRT_MATERIAL_CHANNEL);
	if (inMatChannel == nullptr)
		return nullptr;

	return inMatChannel->findDataStream(PRT_MATERIAL_STREAM);
}

MStatus getMeshName(MString& meshName, const MPlug& plug) {
	MStatus status;
	bool searchEnded = false;

	for (MPlug curPlug = plug; !searchEnded;) {
		searchEnded = true;
		MPlugArray connectedPlugs;
		curPlug.connectedTo(connectedPlugs, false, true, &status);
		MCHECK(status);
		if (!connectedPlugs.length()) {
			return MStatus::kFailure;
		}
		for (const auto& connectedPlug : mu::makeMArrayConstWrapper(connectedPlugs)) {
			MFnDependencyNode connectedDepNode(connectedPlug.node(), &status);
			MCHECK(status);
			MObject connectedDepNodeObj = connectedDepNode.object(&status);
			MCHECK(status);
			if (connectedDepNodeObj.hasFn(MFn::kMesh)) {
				meshName = connectedDepNode.name(&status);
				MCHECK(status);
				break;
			}
			if (connectedDepNodeObj.hasFn(MFn::kGroupParts)) {
				curPlug = connectedDepNode.findPlug("outputGeometry", true, &status);
				MCHECK(status);
				searchEnded = false;
				break;
			}
		}
	}

	return MStatus::kSuccess;
}

MaterialCache getMaterialCache() {
	const adsk::Data::Associations* metadata = MFileIO::metadata();
	adsk::Data::Associations materialAssociations(metadata);
	adsk::Data::Channel* matChannel = materialAssociations.findChannel(PRT_MATERIALINFO_MAP_CHANNEL);

	MaterialUtils::MaterialCache existingMaterialInfos;

	if (matChannel == nullptr)
		return existingMaterialInfos;

	adsk::Data::Stream* matStream = matChannel->findDataStream(PRT_MATERIALINFO_MAP_STREAM);
	if (matStream == nullptr)
		return existingMaterialInfos;

	for (adsk::Data::Stream::iterator iterator = matStream->begin(); iterator != matStream->end(); ++iterator) {
		iterator->setPositionByMemberName(PRT_MATERIALINFO_MAP_KEY.c_str());
		size_t* hashPtr = iterator->asUInt64();
		if (hashPtr == nullptr)
			continue;

		iterator->setPositionByMemberName(PRT_MATERIALINFO_MAP_VALUE.c_str());
		MString uuid = iterator->str(0).c_str();

		existingMaterialInfos.emplace(*hashPtr, uuid);
	}

	return existingMaterialInfos;
}

MStatus addMaterialInfoMapMetadata(size_t materialInfoHash, const MUuid& shadingEngineUuid) {
	const adsk::Data::Associations* metadata = MFileIO::metadata();
	adsk::Data::Associations newMetadata(metadata);

	adsk::Data::Structure* fStructure = adsk::Data::Structure::structureByName(PRT_MATERIALINFO_MAP_STRUCTURE.c_str());

	if (fStructure == nullptr)
		fStructure = createNewMaterialInfoMapStructure();

	adsk::Data::Stream newStream(*fStructure, PRT_MATERIALINFO_MAP_STREAM);
	adsk::Data::Channel newChannel = newMetadata.channel(PRT_MATERIALINFO_MAP_CHANNEL);
	adsk::Data::Stream* newStreamPtr = newChannel.findDataStream(PRT_MATERIALINFO_MAP_STREAM);
	if (newStreamPtr != nullptr)
		newStream = *newStreamPtr;

	MStatus status;
	adsk::Data::Handle handle = getMaterialInfoMapHandle(fStructure, materialInfoHash, shadingEngineUuid, status);
	adsk::Data::IndexCount index = getMaterialInfoMapIndex(newStream, materialInfoHash);

	newStream.setElement(index, handle);
	newChannel.setDataStream(newStream);
	newMetadata.setChannel(newChannel);

	MFileIO::setMetadata(newMetadata);

	return status;
}

bool getFaceRange(adsk::Data::Handle& handle, std::pair<int, int>& faceRange) {
	if (!handle.setPositionByMemberName(PRT_MATERIAL_FACE_INDEX_START.c_str()))
		return false;
	faceRange.first = *handle.asInt32();

	if (!handle.setPositionByMemberName(PRT_MATERIAL_FACE_INDEX_END.c_str()))
		return false;
	faceRange.second = *handle.asInt32();

	return true;
}

std::wstring synchronouslyCreateShadingEngine(const std::wstring& desiredShadingEngineName,
                                              const MELVariable& shadingEngineVariable, MStatus& status) {
	MELScriptBuilder scriptBuilder;
	scriptBuilder.setVar(shadingEngineVariable, MELStringLiteral(desiredShadingEngineName));
	scriptBuilder.setsCreate(shadingEngineVariable);

	std::wstring output;
	status = scriptBuilder.executeSync(output);

	return output;
}

std::filesystem::path getStingrayShaderPath() {
	static const std::filesystem::path sfxFile = []() {
		const std::filesystem::path shaderPath =
		        (PRTContext::get().mPluginRootPath.parent_path() / L"shaders/serlioShaderStingray.sfx");
		LOG_DBG << "stingray shader located at " << shaderPath;
		return shaderPath;
	}();
	return sfxFile;
}

bool textureHasAlphaChannel(const std::wstring& path) {
	const AttributeMapUPtr textureMetadata(prt::createTextureMetadata(prtu::toFileURI(path).c_str()));
	if (textureMetadata == nullptr)
		return false;
	const wchar_t* format = textureMetadata->getString(FORMAT_STRING);
	if (format != nullptr && std::wcscmp(format, RGBA8_FORMAT) == 0)
		return true;
	return false;
}

void resetMaterial(const std::wstring& meshName) {
	MELScriptBuilder scriptBuilder;
	scriptBuilder.declInt(MEL_UNDO_STATE);
	scriptBuilder.getUndoState(MEL_UNDO_STATE);
	scriptBuilder.setUndoState(false);
	scriptBuilder.setsUseInitialShadingGroup(meshName);
	scriptBuilder.setUndoState(MEL_UNDO_STATE);
	scriptBuilder.execute();
}
} // namespace MaterialUtils
