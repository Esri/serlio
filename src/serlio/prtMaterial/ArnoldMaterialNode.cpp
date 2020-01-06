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
#include "prtMaterial/MaterialInfo.h"
#include "prtMaterial/MaterialNameSource.h"
#include "prtMaterial/MaterialUtils.h"

#include "util/MArrayWrapper.h"
#include "util/MELScriptBuilder.h"
#include "util/MayaUtilities.h"

#include "serlioPlugin.h"

#include "prt/StringUtils.h"

#include "maya/MFnGenericAttribute.h"
#include "maya/MFnMesh.h"
#include "maya/MFnTypedAttribute.h"
#include "maya/MGlobal.h"
#include "maya/MPlugArray.h"
#include "maya/adskDataAssociations.h"
#include "maya/adskDataStream.h"
#include "maya/adskDataStructure.h"

#include <cassert>
#include <set>
#include <sstream>

namespace {

const std::wstring MATERIAL_BASE_NAME(L"serlioGeneratedArnoldMaterial");

void synchronouslyCreateShadingEngine(const std::wstring& shadingEngineName) {
	MELScriptBuilder sbSync;
	sbSync.setVar(L"$shadingGroup", shadingEngineName);
	sbSync.setsCreate(L"$shadingGroup");
	sbSync.executeSync();
}

} // namespace

MTypeId ArnoldMaterialNode::id(SerlioNodeIDs::SERLIO_PREFIX, SerlioNodeIDs::ARNOLD_MATERIAL_NODE);

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
	if (plug != aOutMesh)
		return MStatus::kUnknownParameter;

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
		return MStatus::kSuccess;

	MString meshName;
	MStatus meshNameStatus = MaterialUtils::getMeshName(meshName, plug);
	if (meshNameStatus != MStatus::kSuccess || meshName.length() == 0)
		return meshNameStatus;

	adsk::Data::Associations inAssociations(inMetadata);
	adsk::Data::Channel* inMatChannel = inAssociations.findChannel(gPRTMatChannel);
	if (inMatChannel == nullptr)
		return MStatus::kSuccess;

	adsk::Data::Stream* inMatStream = inMatChannel->findDataStream(gPRTMatStream);
	if (inMatStream == nullptr)
		return MStatus::kSuccess;

	const adsk::Data::Structure* fStructure = adsk::Data::Structure::structureByName(gPRTMatStructure.c_str());
	if (fStructure == nullptr)
		return MStatus::kFailure;

	MaterialUtils::MaterialCache matCache = MaterialUtils::getMaterialsByStructure(fStructure);
	MaterialNameSource nameSource;

	MELScriptBuilder sb;
	sb.declString(L"$shadingGroup");
	sb.declString(L"$shaderNode");
	sb.declString(L"$mapFile");
	sb.declString(L"$mapNode");
	sb.declString(L"$bumpLuminanceNode");
	sb.declString(L"$bumpValueNode");
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

		if (!inMatStreamHandle.setPositionByMemberName(gPRTMatMemberFaceStart.c_str()))
			continue;
		const int faceStart = *inMatStreamHandle.asInt32();

		if (!inMatStreamHandle.setPositionByMemberName(gPRTMatMemberFaceEnd.c_str()))
			continue;
		const int faceEnd = *inMatStreamHandle.asInt32();

		std::wstring shadingEngineName;
		MaterialInfo matInfo(inMatStreamHandle);
		auto matIt = matCache.find(matInfo);
		if (matIt != matCache.end()) {
			assert(matInfo.equals(matIt->first));
			shadingEngineName = matIt->second;
		}
		else {
			auto newMaterialName = nameSource.getUniqueName(MATERIAL_BASE_NAME);
			shadingEngineName = newMaterialName.first;
			const std::wstring& shaderName = newMaterialName.second;

			synchronouslyCreateShadingEngine(shadingEngineName);
			MaterialUtils::assignMaterialMetadata(fStructure, inMatStreamHandle, shadingEngineName);
			appendToMaterialScriptBuilder(sb, matInfo, shaderName, shadingEngineName);
			matCache.emplace(matInfo, shadingEngineName);
			LOG_INF << "new arnold shading engine: " << shadingEngineName << " (surface shader: " << shaderName << ")";
		}
		sb.setsAddFaceRange(shadingEngineName, meshName.asWChar(), faceStart, faceEnd);
		LOG_INF << "assigned arnold shading engine (" << faceStart << ":" << faceEnd << "): " << shadingEngineName;
	}

	sb.execute();

	return MStatus::kSuccess;
}

void ArnoldMaterialNode::appendToMaterialScriptBuilder(MELScriptBuilder& sb, const MaterialInfo& matInfo,
                                                       const std::wstring& shaderName,
                                                       const std::wstring& shadingGroupName) const {
	// create shader
	sb.setVar(L"$shaderNode", shaderName);
	sb.setVar(L"$shadingGroup", shadingGroupName);
	sb.createShader(L"aiStandardSurface", L"$shaderNode");

	// connect to shading group
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
		std::wstring shaderName = shadingGroupName + L"_color_map";
		std::wstring shaderNode =
		        createMapShader(sb, matInfo.colormap, matInfo.colormapTrafo, shaderName, L"map1", false, false);
		sb.connectAttr(L"(" + shaderNode + L" + \".outColor\")", L"($colorMapBlendNode + \".input2\")");
	}

	// bump map
	sb.setVar(L"$bumpValueNode", shadingGroupName + L"_bump_value");
	sb.createShader(L"bump2d", L"$bumpValueNode");
	sb.connectAttr(L"($bumpValueNode + \".outNormal\")", L"($shaderNode + \".normalCamera\")");

	if (matInfo.bumpMap.empty()) {
		sb.setAttr(L"($bumpValueNode + \".bumpValue\")", 0.0);
	}
	else {
		std::wstring shaderName = shadingGroupName + L"_bump_map";
		std::wstring shaderNode =
		        createMapShader(sb, matInfo.bumpMap, matInfo.bumpmapTrafo, shaderName, L"bumpMap", true, false);

		sb.setVar(L"$bumpLuminanceNode", shadingGroupName + L"_bump_luminance");
		sb.createShader(L"luminance", L"$bumpLuminanceNode");
		sb.connectAttr(L"(" + shaderNode + L" + \".outColor\")", L"($bumpLuminanceNode + \".value\")");
		sb.connectAttr(L"($bumpLuminanceNode + \".outValue\")", L"($bumpValueNode + \".bumpValue\")");
	}

	// dirt map
	if (matInfo.dirtmap.empty()) {
		sb.setAttr(L"($dirtMapBlendNode + \".input2\")", 1.0, 1.0, 1.0);
	}
	else {
		std::wstring shaderName = shadingGroupName + L"_dirt_map";
		std::wstring shaderNode =
		        createMapShader(sb, matInfo.dirtmap, matInfo.dirtmapTrafo, shaderName, L"dirtMap", false, false);
		sb.connectAttr(L"(" + shaderNode + L" + \".outColor\")", L"($dirtMapBlendNode + \".input2\")");
	}

	// reflectivity
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
		std::wstring shaderName = shadingGroupName + L"_specular_map";
		std::wstring shaderNode = createMapShader(sb, matInfo.specularMap, matInfo.specularmapTrafo, shaderName,
		                                          L"specularMap", false, false);
		sb.connectAttr(L"(" + shaderNode + L" + \".outColor\")", L"($specularMapBlendNode + \".input2\")");
	}

	// opacity/opacity map multiply node
	sb.setVar(L"$opacityMapBlendNode", shadingGroupName + L"_opacity_map_blend");
	sb.createShader(L"aiMultiply", L"$opacityMapBlendNode");
	sb.connectAttr(L"($opacityMapBlendNode + \".outColorR\")", L"($shaderNode + \".opacityR\")");
	sb.connectAttr(L"($opacityMapBlendNode + \".outColorR\")", L"($shaderNode + \".opacityG\")");
	sb.connectAttr(L"($opacityMapBlendNode + \".outColorR\")", L"($shaderNode + \".opacityB\")");

	// opacity
	sb.setAttr(L"($opacityMapBlendNode + \".input1R\")", matInfo.opacity);

	// opacity map
	if (matInfo.opacityMap.empty()) {
		sb.setAttr(L"($opacityMapBlendNode + \".input2R\")", 1.0);
	}
	else {
		std::wstring shaderName = shadingGroupName + L"_opacity_map";
		std::wstring shaderNode = createMapShader(sb, matInfo.opacityMap, matInfo.opacitymapTrafo, shaderName,
		                                          L"opacityMap", false, true);
		sb.connectAttr(L"(" + shaderNode + L" + \".outColorR\")", L"($opacityMapBlendNode + \".input2R\")");
	}

	// normal map
	if (!matInfo.normalMap.empty()) {
		std::wstring shaderName = shadingGroupName + L"_normal_map";
		std::wstring shaderNode =
		        createMapShader(sb, matInfo.normalMap, matInfo.normalmapTrafo, shaderName, L"normalMap", true, false);

		sb.setVar(L"$normalMapConvertNode", shadingGroupName + L"_normal_map_convert");
		sb.createShader(L"aiNormalMap", L"$normalMapConvertNode");
		sb.setAttr(L"($normalMapConvertNode + \".colorToSigned\")", true);
		sb.connectAttr(L"(" + shaderNode + L" + \".outColor\")", L"($normalMapConvertNode + \".input\")");
		sb.connectAttr(L"($normalMapConvertNode + \".outValue\")", L"($bumpValueNode + \".normalCamera\")");
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
		std::wstring shaderName = shadingGroupName + L"_emissive_map";
		std::wstring shaderNode = createMapShader(sb, matInfo.emissiveMap, matInfo.emissivemapTrafo, shaderName,
		                                          L"emissiveMap", false, false);
		sb.connectAttr(L"(" + shaderNode + L" + \".outColor\")", L"($emissiveMapBlendNode + \".input2\")");
	}

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
		std::wstring shaderName = shadingGroupName + L"_roughness_map";
		std::wstring shaderNode = createMapShader(sb, matInfo.roughnessMap, matInfo.roughnessmapTrafo, shaderName,
		                                          L"roughnessMap", true, false);

		// in PRT the roughness map only uses the green channel
		sb.connectAttr(L"(" + shaderNode + L" + \".outColorG\")", L"($roughnessMapBlendNode + \".input2R\")");
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
		std::wstring shaderName = shadingGroupName + L"_metallic_map";
		std::wstring shaderNode = createMapShader(sb, matInfo.metallicMap, matInfo.metallicmapTrafo, shaderName,
		                                          L"metallicMap", true, false);

		// in PRT the metallic map only uses the blue channel
		sb.connectAttr(L"(" + shaderNode + L" + \".outColorB\")", L"($metallicMapBlendNode + \".input2R\")");
	}
}

void ArnoldMaterialNode::setUvTransformAttrs(MELScriptBuilder& sb, const std::wstring& uvSet,
                                             const MaterialTrafo& trafo) const {
	sb.setAttr(L"($uvTrafoNode + \".uvset\")", L"\"" + uvSet + L"\"");
	sb.setAttr(L"($uvTrafoNode + \".pivotFrame\")", 0.0, 0.0);
	sb.setAttr(L"($uvTrafoNode + \".scaleFrame\")", 1.0 / trafo.su(), 1.0 / trafo.sv());
	sb.setAttr(L"($uvTrafoNode + \".translateFrame\")", -trafo.tu() / trafo.su(), -trafo.tv() / trafo.sv());
	if (trafo.rw() != 0.0) {
		LOG_WRN << "rotation (material.map.rw) is not yet supported\n";
	}
}

std::wstring ArnoldMaterialNode::createMapShader(MELScriptBuilder& sb, const std::string& mapFile,
                                                 const MaterialTrafo& mapTrafo, const std::wstring& shaderName,
                                                 const std::wstring& uvSet, const bool raw, const bool alpha) const {
	sb.setVar(L"$mapNode", shaderName);
	sb.setVar(L"$mapFile", prtu::toUTF16FromOSNarrow(mapFile));
	sb.createTexture(L"$mapNode");
	sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");

	if (raw) {
		sb.setAttr(L"($mapNode + \".colorSpace\")", L"Raw");
		sb.setAttr(L"($mapNode + \".ignoreColorSpaceFileRules\")", true);
	}

	sb.setVar(L"$uvTrafoNode", shaderName + L"_trafo");
	sb.createShader(L"aiUvTransform", L"$uvTrafoNode");
	setUvTransformAttrs(sb, uvSet, mapTrafo);

	if (alpha) {
		sb.connectAttr(L"($mapNode + \".outAlpha\")", L"($uvTrafoNode + \".passthroughR\")");
	}
	else {
		sb.connectAttr(L"($mapNode + \".outColor\")", L"($uvTrafoNode + \".passthrough\")");
	}
	return L"$uvTrafoNode";
}
