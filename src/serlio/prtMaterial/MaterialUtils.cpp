#include "prtMaterial/MaterialUtils.h"

#include "util/MArrayWrapper.h"
#include "util/MELScriptBuilder.h"
#include "util/MItDependencyNodesWrapper.h"
#include "util/MayaUtilities.h"

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

adsk::Data::Stream* getMaterialStream(MObject& aOutMesh, MObject& aInMesh, MDataBlock& data) {
	MStatus status;

	const MDataHandle inMeshHandle = data.inputValue(aInMesh, &status);
	MCHECK(status);

	MDataHandle outMeshHandle = data.outputValue(aOutMesh, &status);
	MCHECK(status);

	status = outMeshHandle.set(inMeshHandle.asMesh());
	MCHECK(status);

	const MFnMesh inMesh(inMeshHandle.asMesh(), &status);
	MCHECK(status);

	const adsk::Data::Associations* inMetadata = inMesh.metadata(&status);
	MCHECK(status);
	if (inMetadata == nullptr)
		return nullptr;

	adsk::Data::Associations inAssociations(inMetadata);
	adsk::Data::Channel* inMatChannel = inAssociations.findChannel(gPRTMatChannel);
	if (inMatChannel == nullptr)
		return nullptr;

	return inMatChannel->findDataStream(gPRTMatStream);
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
		adsk::Data::Channel* matChannel = materialAssociations.findChannel(gPRTMatChannel);

		if (matChannel == nullptr)
			continue;

		adsk::Data::Stream* matStream = matChannel->findDataStream(gPRTMatStream);
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
	if (!handle.setPositionByMemberName(gPRTMatMemberFaceStart.c_str()))
		return false;
	faceRange.first = *handle.asInt32();

	if (!handle.setPositionByMemberName(gPRTMatMemberFaceEnd.c_str()))
		return false;
	faceRange.second = *handle.asInt32();

	return true;
}

void assignMaterialMetadata(const adsk::Data::Structure& materialStructure, const adsk::Data::Handle& streamHandle,
                            const std::wstring& shadingEngineName) {
	MObject shadingEngineObj = findNamedObject(shadingEngineName, MFn::kShadingEngine);
	MFnDependencyNode shadingEngine(shadingEngineObj);

	adsk::Data::Associations newMetadata;
	adsk::Data::Channel newChannel = newMetadata.channel(gPRTMatChannel);
	adsk::Data::Stream newStream(materialStructure, gPRTMatStream);
	newChannel.setDataStream(newStream);
	newMetadata.setChannel(newChannel);
	adsk::Data::Handle handle(streamHandle);
	handle.makeUnique();
	newStream.setElement(0, handle);
	shadingEngine.setMetadata(newMetadata);
}

std::wstring synchronouslyCreateShadingEngine(const std::wstring& desiredShadingEngineName,
                                              const std::wstring& shadingEngineVariable) {
	MELScriptBuilder scriptBuilder;
	scriptBuilder.setVar(shadingEngineVariable, desiredShadingEngineName);
	scriptBuilder.setsCreate(shadingEngineVariable);

	std::wstring output;
	MStatus status = scriptBuilder.executeSync(output);
	MCHECK(status); // TODO: return status

	return output;
}

} // namespace MaterialUtils
