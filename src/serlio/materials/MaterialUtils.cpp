#include "materials/MaterialUtils.h"

#include "utils/MArrayWrapper.h"
#include "utils/MELScriptBuilder.h"
#include "utils/MItDependencyNodesWrapper.h"
#include "utils/MayaUtilities.h"

#include "PRTContext.h"

#include "maya/MDataBlock.h"
#include "maya/MDataHandle.h"
#include "maya/MFnMesh.h"
#include "maya/MItDependencyNodes.h"
#include "maya/MPlugArray.h"
#include "maya/adskDataAssociations.h"

namespace {

MObject findNamedObject(const std::wstring& name, MFn::Type fnType) {
	MStatus status;
	MItDependencyNodes nodeIt(fnType, &status);
	MCHECK(status);

	for (const auto& nodeObj : MItDependencyNodesWrapper(nodeIt)) {
		MFnDependencyNode node(nodeObj);
		if (std::wcscmp(node.name().asWChar(), name.c_str()) == 0)
			return nodeObj;
	}

	return MObject::kNullObj;
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

MaterialCache getMaterialsByStructure(const adsk::Data::Structure& materialStructure, const std::wstring& baseName) {
	MaterialCache existingMaterialInfos;

	MStatus status;
	MItDependencyNodes shaderIt(MFn::kShadingEngine, &status);
	MCHECK(status);
	for (const auto& nodeObj : MItDependencyNodesWrapper(shaderIt)) {
		MFnDependencyNode node(nodeObj);

		const adsk::Data::Associations* materialMetadata = node.metadata(&status);
		MCHECK(status);

		if (materialMetadata == nullptr)
			continue;

		adsk::Data::Associations materialAssociations(materialMetadata);
		adsk::Data::Channel* matChannel = materialAssociations.findChannel(PRT_MATERIAL_CHANNEL);

		if (matChannel == nullptr)
			continue;

		adsk::Data::Stream* matStream = matChannel->findDataStream(PRT_MATERIAL_STREAM);
		if ((matStream == nullptr) || (matStream->elementCount() != 1))
			continue;

		adsk::Data::Handle matSHandle = matStream->element(0);
		if (!matSHandle.usesStructure(materialStructure))
			continue;

		if (std::wcsncmp(node.name().asWChar(), baseName.c_str(), baseName.length()) != 0)
			continue;

		existingMaterialInfos.emplace(matSHandle, node.name().asWChar());
	}

	return existingMaterialInfos;
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

void assignMaterialMetadata(const adsk::Data::Structure& materialStructure, const adsk::Data::Handle& streamHandle,
                            const std::wstring& shadingEngineName) {
	MObject shadingEngineObj = findNamedObject(shadingEngineName, MFn::kShadingEngine);
	MFnDependencyNode shadingEngine(shadingEngineObj);

	adsk::Data::Associations newMetadata;
	adsk::Data::Channel newChannel = newMetadata.channel(PRT_MATERIAL_CHANNEL);
	adsk::Data::Stream newStream(materialStructure, PRT_MATERIAL_STREAM);
	newChannel.setDataStream(newStream);
	newMetadata.setChannel(newChannel);
	adsk::Data::Handle handle(streamHandle);
	handle.makeUnique();
	newStream.setElement(0, handle);
	shadingEngine.setMetadata(newMetadata);
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

} // namespace MaterialUtils
