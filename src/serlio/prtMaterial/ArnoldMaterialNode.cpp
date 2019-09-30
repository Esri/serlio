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

#include "prtMaterial/ArnoldMaterialNode.h"

#include "prt/StringUtils.h"

#include "prtMaterial/MaterialInfo.h"

#include "util/MELScriptBuilder.h"
#include "util/MItDependencyNodesWrapper.h"
#include "util/MPlugArrayWrapper.h"
#include "util/MayaUtilities.h"

#include <maya/MFnGenericAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MPlugArray.h>
#include <maya/adskDataAssociations.h>
#include <maya/adskDataStream.h>
#include <maya/adskDataStructure.h>

#include <list>
#include <sstream>

MTypeId ArnoldMaterialNode::id(0x12345); //TODO: register block ID if not done yet and set unique IDs for all our nodes! http://mayaid.herokuapp.com/

MObject ArnoldMaterialNode::aInMesh;
MObject ArnoldMaterialNode::aOutMesh;

MStatus ArnoldMaterialNode::initialize() {
	MStatus status;

	MFnTypedAttribute tAttr;
	aInMesh = tAttr.create("inMesh", "im", MFnData::kMesh, MObject::kNullObj, &status);
	MCHECK(status);
	status = addAttribute(aInMesh);
	MCHECK(status);

	aOutMesh = tAttr.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &status);
	MCHECK(status);
	status = tAttr.setWritable(false);
	MCHECK(status);
	status = tAttr.setStorable(false);
	MCHECK(status);
	status = addAttribute(aOutMesh);
	MCHECK(status);

	status = attributeAffects(aInMesh, aOutMesh);
	MCHECK(status);

	return MStatus::kSuccess;
}

MStatus ArnoldMaterialNode::compute(const MPlug& plug, MDataBlock& data) {
	if (plug != aOutMesh) {
		return MStatus::kUnknownParameter;
	}

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
	if (inMetadata == nullptr) {
		return MStatus::kSuccess;
	}

	// TODO: this should really be a traversal over the (potentially cyclic!) dependency
	// graph to find all the relevant directly and indirectly connected mesh nodes
	// for now, just follow direct or indirect connections via group parts nodes
	MString meshName;
	bool meshFound = false;
	bool searchEnded = false;

	for (MPlug curPlug = plug; !searchEnded;) {
		searchEnded = true;
		MPlugArray connectedPlugs;
		curPlug.connectedTo(connectedPlugs, false, true, &status);
		MCHECK(status);
		if (!connectedPlugs.length()) {
			return MStatus::kFailure;
		}
		for (const auto& connectedPlug : makePlugArrayConstWrapper(connectedPlugs)) {
			MFnDependencyNode connectedDepNode(connectedPlug.node(), &status);
			MCHECK(status);
			MObject connectedDepNodeObj = connectedDepNode.object(&status);
			MCHECK(status);
			if (connectedDepNodeObj.hasFn(MFn::kMesh)) {
				meshName = connectedDepNode.name(&status);
				MCHECK(status);
				meshFound = true;
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

	if (!meshFound) {
		return MStatus::kSuccess;
	}

	adsk::Data::Associations inAssociations(inMetadata);
	adsk::Data::Channel* inMatChannel = inAssociations.findChannel(gPRTMatChannel);

	if (inMatChannel == nullptr) {
		return MStatus::kSuccess;
	}

	adsk::Data::Stream* inMatStream = inMatChannel->findDataStream(gPRTMatStream);
	if (inMatStream == nullptr) {
		return MStatus::kSuccess;
	}

	//find all existing prt materials
	const adsk::Data::Structure* fStructure = adsk::Data::Structure::structureByName(gPRTMatStructure.c_str());
	if (fStructure == nullptr) {
		return MStatus::kFailure;
	}

	std::set<std::wstring> shaderNames;
	std::list<std::pair<const MObject, const MaterialInfo>> existingMaterialInfos;
	MItDependencyNodes itHwShaders(MFn::kPluginHardwareShader, &status);
	MCHECK(status);
	for (const auto& hwShaderNode : MItDependencyNodesWrapper(itHwShaders)) {
		MFnDependencyNode fnDependencyNode(hwShaderNode, &status);
		MCHECK(status);
		MString shaderNodeName = fnDependencyNode.name(&status);
		MCHECK(status);
		shaderNames.insert(shaderNodeName.asWChar());

		const adsk::Data::Associations* materialMetadata = fnDependencyNode.metadata(&status);
		MCHECK(status);

		if (materialMetadata == nullptr) {
			continue;
		}

		adsk::Data::Associations materialAssociations(materialMetadata);
		adsk::Data::Channel* matChannel = materialAssociations.findChannel(gPRTMatChannel);
		if (matChannel == nullptr) {
			continue;
		}

		adsk::Data::Stream* matStream = matChannel->findDataStream(gPRTMatStream);
		if ((matStream == nullptr) || matStream->elementCount() != 1) {
			continue;
		}

		adsk::Data::Handle matSHandle = matStream->element(0);
		if (!matSHandle.usesStructure(*fStructure)) {
			continue;
		}

		existingMaterialInfos.emplace_back(hwShaderNode, matSHandle);
	}

	MELScriptBuilder sb;

	// TODO: we probably don't want to add a sky, if there are existing light sources in the scene already
	sb.python(L"import mtoa.ui.arnoldmenu as arnoldmenu");
	sb.python(L"arnoldmenu.doCreatePhysicalSky()");

	sb.declString(L"$shadingGroup");
	sb.declString(L"$shaderNode");
	sb.declString(L"$mapFile");
	sb.declString(L"$mapNode");
	sb.declString(L"$displacementNode");
	sb.declString(L"$normalMapConvertNode");
	sb.declString(L"$colorMapBlendNode");
	sb.declString(L"$dirtMapBlendNode");
	sb.declString(L"$opacityMapBlendNode");
	sb.declString(L"$specularMapBlendNode");
	sb.declString(L"$emissiveMapBlendNode");
	sb.declString(L"$roughnessMapBlendNode");
	sb.declString(L"$metallicMapBlendNode");
	sb.declString(L"$uvTrafoNode");

	for (adsk::Data::Handle& inMatStreamHandle : *inMatStream) {
		if (!inMatStreamHandle.hasData())
			continue;

		if (!inMatStreamHandle.usesStructure(*fStructure))
			continue;

		MaterialInfo matInfo(inMatStreamHandle);

		//material with same metadata already exists?
		auto matchingMatIt = std::find_if(existingMaterialInfos.begin(), existingMaterialInfos.end(), [&matInfo](auto& p) {
			return matInfo.equals(p.second);
		});

		MObject matchingMaterial = MObject::kNullObj;
		if (matchingMatIt != existingMaterialInfos.end()) {
			matchingMaterial = matchingMatIt->first;
		}

		if (!inMatStreamHandle.setPositionByMemberName(gPRTMatMemberFaceStart.c_str())) {
			continue;
		}
		const int faceStart = *inMatStreamHandle.asInt32();

		if (!inMatStreamHandle.setPositionByMemberName(gPRTMatMemberFaceEnd.c_str())) {
			continue;
		}
		const int faceEnd = *inMatStreamHandle.asInt32();

		if (matchingMaterial != MObject::kNullObj) {
			MFnDependencyNode matchingMaterialDependencyNode(matchingMaterial, &status);
			MCHECK(status);
			MString matchingMaterialNodeName = matchingMaterialDependencyNode.name(&status);
			MCHECK(status);
			std::wstring matchingMaterialName(matchingMaterialNodeName.asWChar());
			// TODO: try to find a more reliable way than comparing node names
			const size_t idx = matchingMaterialName.find_last_of(L"Sh");
			if (idx != std::wstring::npos) {
				matchingMaterialName[idx] = L'g';
			}
			sb.setsAddFaceRange(matchingMaterialName, meshName.asWChar(), faceStart, faceEnd);
			continue;
		}

		//get unique name
		std::wstring shaderName(L"serlioGeneratedArnoldMaterialSh");
		std::wstring shadingGroupName(L"serlioGeneratedArnoldMaterialSg");

		int shIdx = 0;
		while (shaderNames.find(shaderName + std::to_wstring(shIdx)) != shaderNames.end()) {
			shIdx++;
		}
		shaderName += std::to_wstring(shIdx);
		shadingGroupName += std::to_wstring(shIdx);
		shaderNames.insert(shaderName);

		buildMaterialShaderScript(sb, matInfo, shaderName, shadingGroupName, meshName.asWChar(), faceStart, faceEnd);
	}

	sb.execute();

	return MStatus::kSuccess;
}

void ArnoldMaterialNode::buildMaterialShaderScript(MELScriptBuilder& sb,
												   const MaterialInfo& matInfo,
												   const std::wstring& shaderName,
												   const std::wstring& shadingGroupName,
												   const std::wstring& meshName,
												   const int faceStart,
												   const int faceEnd) {
	// TODO: combine texture nodes with same texture
	// TODO: investigate using place2dtexture nodes to transform textures instead of using the attributes of aiUvTransform

	std::array<wchar_t, 512> buf{};

	sb.setVar(L"$shaderNode", shaderName);
	sb.setVar(L"$shadingGroup", shadingGroupName);
	sb.createShader(L"aiStandardSurface", L"$shaderNode");

	// build shading group
	sb.setsCreate(L"$shadingGroup");
	sb.setsAddFaceRange(L"$shadingGroup", meshName, faceStart, faceEnd);
	sb.connectAttr(L"($shaderNode + \".outColor\")", L"($shadingGroup + \".surfaceShader\")");

	sb.setAttr(L"($shaderNode + \".base\")", 1.0);

	// color/dirt map multiply node
	sb.setVar(L"$dirtMapBlendNode", shadingGroupName + L"_dirt_multiply");
	sb.createShader(L"aiMultiply", L"$dirtMapBlendNode");
	sb.connectAttr(L"($dirtMapBlendNode + \".outColor\")", L"($shaderNode + \".baseColor\")");

	// color/color map multiply node
	sb.setVar(L"$colorMapBlendNode", shadingGroupName + L"_color_map_blend");
	sb.createShader(L"aiMultiply", L"$colorMapBlendNode");
	sb.connectAttr(L"($colorMapBlendNode + \".outColor\")", L"($dirtMapBlendNode + \".input1\")");

	// color
	sb.setAttr(L"($colorMapBlendNode + \".input1\")", matInfo.diffuseColor);

	// color map
	if (matInfo.colormap.empty()) {
		sb.setAttr(L"($colorMapBlendNode + \".input2\")", 1.0, 1.0, 1.0);
	}
	else {
		size_t tmpSize = buf.size();
		sb.setVar(L"$mapNode", shadingGroupName + L"_color_map");
		sb.setVar(L"$mapFile", prt::StringUtils::toUTF16FromOSNarrow(matInfo.colormap.c_str(), buf.data(), &tmpSize));
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");

		sb.setVar(L"$uvTrafoNode", shadingGroupName + L"_color_map_trafo");
		sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
		setUvTransformAttrs(sb, L"map1", matInfo.colormapTrafo);

		sb.connectAttr(L"($mapNode + \".outColor\")", L"($uvTrafoNode + \".passthrough\")");
		sb.connectAttr(L"($uvTrafoNode + \".outColor\")", L"($colorMapBlendNode + \".input2\")");
	}

	// bump map
	if (!matInfo.bumpMap.empty()) {
		size_t tmpSize = buf.size();
		sb.setVar(L"$mapNode", shadingGroupName + L"_bump_map");
		sb.setVar(L"$mapFile", prt::StringUtils::toUTF16FromOSNarrow(matInfo.bumpMap.c_str(), buf.data(), &tmpSize));
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");
		sb.setAttr(L"($mapNode + \".colorSpace\")", L"Raw");
		sb.setAttr(L"($mapNode + \".ignoreColorSpaceFileRules\")", true);

		sb.setVar(L"$uvTrafoNode", shadingGroupName + L"_bump_map_trafo");
		sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
		setUvTransformAttrs(sb, L"bumpMap", matInfo.bumpmapTrafo);

		sb.setVar(L"$displacementNode", shadingGroupName + L"_bump_displacement");
		sb.createShader(L"displacementShader", L"$displacementNode");

		sb.connectAttr(L"($mapNode + \".outColor\")", L"($uvTrafoNode + \".passthrough\")");
		sb.connectAttr(L"($uvTrafoNode + \".outColor\")", L"($displacementNode + \".vectorDisplacement\")");
		sb.connectAttr(L"($displacementNode + \".displacement\")", L"($shadingGroup + \".displacementShader\")");
	}

	// dirt map
	if (matInfo.dirtmap.empty()) {
		sb.setAttr(L"($dirtMapBlendNode + \".input2\")", 1.0, 1.0, 1.0);
	}
	else {
		size_t tmpSize = buf.size();
		sb.setVar(L"$mapNode", shadingGroupName + L"_dirt_map");
		sb.setVar(L"$mapFile", prt::StringUtils::toUTF16FromOSNarrow(matInfo.dirtmap.c_str(), buf.data(), &tmpSize));
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");

		sb.setVar(L"$uvTrafoNode", shadingGroupName + L"_dirt_map_trafo");
		sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
		setUvTransformAttrs(sb, L"dirtMap", matInfo.dirtmapTrafo);

		sb.connectAttr(L"($mapNode + \".outColor\")", L"($uvTrafoNode + \".passthrough\")");
		sb.connectAttr(L"($uvTrafoNode + \".outColor\")", L"($dirtMapBlendNode + \".input2\")");
	}

	// shininess
	sb.setAttr(L"($shaderNode + \".specular\")", 1.0);

	// specular/specular map multiply node
	sb.setVar(L"$specularMapBlendNode", shadingGroupName + L"_specular_map_blend");
	sb.createShader(L"aiMultiply", L"$specularMapBlendNode");
	sb.connectAttr(L"($specularMapBlendNode + \".outColor\")", L"($shaderNode + \".specularColor\")");

	// ignore the specular color for now (matInfo.specularColor), since in the metallic-roughness
	// model of glTF specularity is controlled entirely via the roughness which requires the specular
	// color of the aiStandardSurface shader to be set to white, however, the default value for
	// matInfo.specularColor is black
	sb.setAttr(L"($specularMapBlendNode + \".input1\")", 1.0, 1.0, 1.0);

	// specular map
	if (matInfo.specularMap.empty()) {
		sb.setAttr(L"($specularMapBlendNode + \".input2\")", 1.0, 1.0, 1.0);
	}
	else {
		size_t tmpSize = buf.size();
		sb.setVar(L"$mapNode", shadingGroupName + L"_specular_map");
		sb.setVar(L"$mapFile", prt::StringUtils::toUTF16FromOSNarrow(matInfo.specularMap.c_str(), buf.data(), &tmpSize));
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");

		sb.setVar(L"$uvTrafoNode", shadingGroupName + L"_specular_map_trafo");
		sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
		setUvTransformAttrs(sb, L"specularMap", matInfo.specularmapTrafo);

		sb.connectAttr(L"($mapNode + \".outColor\")", L"($uvTrafoNode + \".passthrough\")");
		sb.connectAttr(L"($uvTrafoNode + \".outColor\")", L"($specularMapBlendNode + \".input2\")");
	}

	// opacity/opacity map multiply node
	sb.setVar(L"$opacityMapBlendNode", shadingGroupName + L"_opacity_map_blend");
	sb.createShader(L"aiMultiply", L"$opacityMapBlendNode");
	sb.connectAttr(L"($opacityMapBlendNode + \".outColorR\")", L"($shaderNode + \".opacityR\")");
	sb.connectAttr(L"($opacityMapBlendNode + \".outColorR\")", L"($shaderNode + \".opacityG\")");
	sb.connectAttr(L"($opacityMapBlendNode + \".outColorR\")", L"($shaderNode + \".opacityB\")");

	// opacity
	sb.setAttr(L"($opacityMapBlendNode + \".input1R\")", matInfo.opacity);

	// TODO: support opacitymap mode (opaque, blend, mask)
	// opacity map
	if (matInfo.opacityMap.empty()) {
		sb.setAttr(L"($opacityMapBlendNode + \".input2R\")", 1.0);
	}
	else {
		size_t tmpSize = buf.size();
		sb.setVar(L"$mapNode", shadingGroupName + L"_opacity_map");
		sb.setVar(L"$mapFile", prt::StringUtils::toUTF16FromOSNarrow(matInfo.opacityMap.c_str(), buf.data(), &tmpSize));
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");

		sb.setVar(L"$uvTrafoNode", shadingGroupName + L"_opacity_map_trafo");
		sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
		setUvTransformAttrs(sb, L"opacityMap", matInfo.opacitymapTrafo);

		// TODO: handle RGB textures, where the luminance should be the opacity
		sb.connectAttr(L"($mapNode + \".outAlpha\")", L"($uvTrafoNode + \".passthroughR\")");
		sb.connectAttr(L"($uvTrafoNode + \".outColorR\")", L"($opacityMapBlendNode + \".input2R\")");
	}

	// normal map
	if (!matInfo.normalMap.empty()) {
		size_t tmpSize = buf.size();
		sb.setVar(L"$mapNode", shadingGroupName + L"_normal_map");
		sb.setVar(L"$mapFile", prt::StringUtils::toUTF16FromOSNarrow(matInfo.normalMap.c_str(), buf.data(), &tmpSize));
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");
		sb.setAttr(L"($mapNode + \".colorSpace\")", L"Raw");
		sb.setAttr(L"($mapNode + \".ignoreColorSpaceFileRules\")", true);

		sb.setVar(L"$uvTrafoNode", shadingGroupName + L"_normal_map_trafo");
		sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
		setUvTransformAttrs(sb, L"normalMap", matInfo.normalmapTrafo);

		sb.connectAttr(L"($mapNode + \".outColor\")", L"($uvTrafoNode + \".passthrough\")");

		sb.setVar(L"$normalMapConvertNode", shadingGroupName + L"_normal_map_convert");
		sb.createShader(L"aiNormalMap", L"$normalMapConvertNode");
		sb.setAttr(L"($normalMapConvertNode + \".colorToSigned\")", true);
		sb.connectAttr(L"($uvTrafoNode + \".outColor\")", L"($normalMapConvertNode + \".input\")");
		sb.connectAttr(L"($normalMapConvertNode + \".outValue\")", L"($shaderNode + \".normalCamera\")");
	}

	// emission
	sb.setAttr(L"($shaderNode + \".emission\")", 1.0);

	// emission/emissive map multiply node
	sb.setVar(L"$emissiveMapBlendNode", shadingGroupName + L"_emissive_map_blend");
	sb.createShader(L"aiMultiply", L"$emissiveMapBlendNode");
	sb.connectAttr(L"($emissiveMapBlendNode + \".outColor\")", L"($shaderNode + \".emissionColor\")");

	// emissive color
	sb.setAttr(L"($emissiveMapBlendNode + \".input1\")", matInfo.emissiveColor);

	// emissive map
	if (matInfo.emissiveMap.empty()) {
		sb.setAttr(L"($emissiveMapBlendNode + \".input2\")", 1.0, 1.0, 1.0);
	}
	else {
		size_t tmpSize = buf.size();
		sb.setVar(L"$mapNode", shadingGroupName + L"_emissive_map");
		sb.setVar(L"$mapFile", prt::StringUtils::toUTF16FromOSNarrow(matInfo.emissiveMap.c_str(), buf.data(), &tmpSize));
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");

		sb.setVar(L"$uvTrafoNode", shadingGroupName + L"_emissive_map_trafo");
		sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
		setUvTransformAttrs(sb, L"emissiveMap", matInfo.emissivemapTrafo);

		sb.connectAttr(L"($mapNode + \".outColor\")", L"($uvTrafoNode + \".passthrough\")");
		sb.connectAttr(L"($uvTrafoNode + \".outColor\")", L"($emissiveMapBlendNode + \".input2\")");
	}

	// TODO: support occlusion map

	// roughness/roughness map multiply node
	sb.setVar(L"$roughnessMapBlendNode", shadingGroupName + L"_roughness_map_blend");
	sb.createShader(L"aiMultiply", L"$roughnessMapBlendNode");
	sb.connectAttr(L"($roughnessMapBlendNode + \".outColorR\")", L"($shaderNode + \".specularRoughness\")");

	// roughness
	sb.setAttr(L"($roughnessMapBlendNode + \".input1R\")", matInfo.roughness);

	// roughness map
	if (matInfo.roughnessMap.empty()) {
		sb.setAttr(L"($roughnessMapBlendNode + \".input2R\")", 1.0);
	}
	else {
		size_t tmpSize = buf.size();
		sb.setVar(L"$mapNode", shadingGroupName + L"_roughness_map");
		sb.setVar(L"$mapFile", prt::StringUtils::toUTF16FromOSNarrow(matInfo.roughnessMap.c_str(), buf.data(), &tmpSize));
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");
		sb.setAttr(L"($mapNode + \".colorSpace\")", L"Raw");
		sb.setAttr(L"($mapNode + \".ignoreColorSpaceFileRules\")", true);

		sb.setVar(L"$uvTrafoNode", shadingGroupName + L"_roughness_map_trafo");
		sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
		setUvTransformAttrs(sb, L"roughnessMap", matInfo.roughnessmapTrafo);

		// in PRT the roughness map only uses the green channel
		sb.connectAttr(L"($mapNode + \".outColorG\")", L"($uvTrafoNode + \".passthroughR\")");
		sb.connectAttr(L"($uvTrafoNode + \".outColorR\")", L"($roughnessMapBlendNode + \".input2R\")");
	}

	// metallic/metallic map multiply node
	sb.setVar(L"$metallicMapBlendNode", shadingGroupName + L"_metallic_map_blend");
	sb.createShader(L"aiMultiply", L"$metallicMapBlendNode");
	sb.connectAttr(L"($metallicMapBlendNode + \".outColorR\")", L"($shaderNode + \".metalness\")");

	// metallic
	sb.setAttr(L"($metallicMapBlendNode + \".input1R\")", matInfo.metallic);

	// metallic map
	if (matInfo.metallicMap.empty()) {
		sb.setAttr(L"($metallicMapBlendNode + \".input2R\")", 1.0);
	}
	else {
		size_t tmpSize = buf.size();
		sb.setVar(L"$mapNode", shadingGroupName + L"_metallic_map");
		sb.setVar(L"$mapFile", prt::StringUtils::toUTF16FromOSNarrow(matInfo.metallicMap.c_str(), buf.data(), &tmpSize));
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");
		sb.setAttr(L"($mapNode + \".colorSpace\")", L"Raw");
		sb.setAttr(L"($mapNode + \".ignoreColorSpaceFileRules\")", true);

		sb.setVar(L"$uvTrafoNode", shadingGroupName + L"_metallic_map_trafo");
		sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
		setUvTransformAttrs(sb, L"metallicMap", matInfo.metallicmapTrafo);

		// in PRT the metallic map only uses the blue channel
		sb.connectAttr(L"($mapNode + \".outColorB\")", L"($uvTrafoNode + \".passthroughR\")");
		sb.connectAttr(L"($uvTrafoNode + \".outColorR\")", L"($metallicMapBlendNode + \".input2R\")");
	}
}

void ArnoldMaterialNode::setUvTransformAttrs(MELScriptBuilder& sb,
											 const std::wstring& uvSet,
											 const MaterialTrafo& trafo) {
	sb.setAttr(L"($uvTrafoNode + \".uvset\")", L"\"" + uvSet + L"\"");
	sb.setAttr(L"($uvTrafoNode + \".pivotFrame\")", 0.0, 0.0);
	sb.setAttr(L"($uvTrafoNode + \".scaleFrame\")", 1.0 / trafo.su(), 1.0 / trafo.sv());
	sb.setAttr(L"($uvTrafoNode + \".translateFrame\")", -trafo.tu() / trafo.su(), -trafo.tv() / trafo.sv());
	if (trafo.rw() != 0.0) {
		LOG_WRN << "rotation (material.map.rw) is not yet supported\n";
	}
}
