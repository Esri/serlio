#include "prtMaterial/MaterialUtils.h"

#include "util/MArrayWrapper.h"
#include "util/MItDependencyNodesWrapper.h"
#include "util/MayaUtilities.h"

#include "maya/MItDependencyNodes.h"
#include "maya/MPlugArray.h"
#include "maya/adskDataAssociations.h"
#include "maya/adskDataStream.h"

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

MaterialCache getMaterialsByStructure(const adsk::Data::Structure& materialStructure) {
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
		if ((matStream == nullptr) || (matStream->elementCount() == 0))
			continue;

		const adsk::Data::Handle matSHandle = matStream->element(0);
		if (!matSHandle.usesStructure(materialStructure))
			continue;

		existingMaterialInfos.emplace(matSHandle, node.name().asWChar());
	}

	return existingMaterialInfos;
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

} // namespace MaterialUtils
