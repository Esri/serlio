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
#include "prtMaterial/MaterialUtils.h"

#include "util/MELScriptBuilder.h"
#include "util/MayaUtilities.h"

#include "serlioPlugin.h"

#include "maya/MFnTypedAttribute.h"
#include "maya/adskDataStream.h"

#include <mutex>

namespace {

const std::wstring MATERIAL_BASE_NAME = L"serlioArnoldMaterial";

std::once_flag pluginDependencyCheckFlag;
const std::vector<std::string> PLUGIN_DEPENDENCIES = {"mtoa"};

void setUvTransformAttrs(MELScriptBuilder& sb, const MELVariable& node, const std::wstring& uvSet,
                         const MaterialTrafo& trafo) {
	sb.setAttr(node, L"uvset", L"\"" + uvSet + L"\"");
	sb.setAttr(node, L"pivotFrame", 0.0, 0.0);
	sb.setAttr(node, L"scaleFrame", 1.0 / trafo.su(), 1.0 / trafo.sv());
	sb.setAttr(node, L"translateFrame", -trafo.tu() / trafo.su(), -trafo.tv() / trafo.sv());
	if (trafo.rw() != 0.0) {
		LOG_WRN << "rotation (material.map.rw) is not yet supported\n";
	}
}

MELVariable createMapShader(MELScriptBuilder& sb, const std::string& mapFile, const MaterialTrafo& mapTrafo,
                            const std::wstring& shaderName, const std::wstring& uvSet, const bool raw,
                            const bool alpha) {
	MELVariable mapNode(L"mapNode");
	sb.setVar(mapNode, shaderName);
	sb.setVar(MELVariable(L"mapFile"), prtu::toUTF16FromOSNarrow(mapFile));
	sb.createTextureShadingNode(mapNode);
	sb.setAttr(mapNode, L"fileTextureName", L"$mapFile");

	if (raw) {
		sb.setAttr(mapNode, L"colorSpace", L"Raw");
		sb.setAttr(mapNode, L"ignoreColorSpaceFileRules", true);
	}

	MELVariable uvTrafoNode(L"uvTrafoNode");
	sb.setVar(uvTrafoNode, shaderName + L"_trafo");
	sb.createShader(L"aiUvTransform", uvTrafoNode);
	setUvTransformAttrs(sb, uvTrafoNode, uvSet, mapTrafo);

	if (alpha)
		sb.connectAttr(mapNode, L"outAlpha", uvTrafoNode, L"passthroughR");
	else
		sb.connectAttr(mapNode, L"outColor", uvTrafoNode, L"passthrough");
	return uvTrafoNode;
}

void appendToMaterialScriptBuilder(MELScriptBuilder& sb, const MaterialInfo& matInfo,
                                   const std::wstring& shaderBaseName, const std::wstring& shadingEngineName) {
	// create shader
	MELVariable shaderNode(L"shaderNode");
	sb.setVar(shaderNode, shaderBaseName);
	sb.setVar(MEL_VARIABLE_SHADING_ENGINE, shadingEngineName);
	sb.createShader(L"aiStandardSurface", shaderNode); // note: name might change to be unique

	// connect to shading group
	sb.connectAttr(shaderNode, L"outColor", MEL_VARIABLE_SHADING_ENGINE, L"surfaceShader");

	sb.setAttr(shaderNode, L"base", 1.0);

	// color/dirt map multiply node
	MELVariable dirtMapBlendNode(L"dirtMapBlendNode");
	sb.setVar(dirtMapBlendNode, shadingEngineName + L"_dirt_multiply");
	sb.createShader(L"aiMultiply", dirtMapBlendNode);
	sb.connectAttr(dirtMapBlendNode, L"outColor", shaderNode, L"baseColor");

	// color/color map multiply node
	MELVariable colorMapBlendNode(L"colorMapBlendNode");
	sb.setVar(colorMapBlendNode, shadingEngineName + L"_color_map_blend");
	sb.createShader(L"aiMultiply", colorMapBlendNode);
	sb.connectAttr(colorMapBlendNode, L"outColor", dirtMapBlendNode, L"input1");

	// color
	sb.setAttr(colorMapBlendNode, L"input1", matInfo.diffuseColor);

	// color map
	if (matInfo.colormap.empty()) {
		sb.setAttr(colorMapBlendNode, L"input2", 1.0, 1.0, 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_color_map";
		const MELVariable mapShaderNode =
		        createMapShader(sb, matInfo.colormap, matInfo.colormapTrafo, shaderName, L"map1", false, false);
		sb.connectAttr(mapShaderNode, L"outColor", colorMapBlendNode, L"input2");
	}

	// bump map
	MELVariable bumpValueNode(L"bumpValueNode");
	sb.setVar(bumpValueNode, shadingEngineName + L"_bump_value");
	sb.createShader(L"bump2d", bumpValueNode);
	sb.connectAttr(bumpValueNode, L"outNormal", shaderNode, L"normalCamera");

	if (matInfo.bumpMap.empty()) {
		sb.setAttr(bumpValueNode, L"bumpValue", 0.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_bump_map";
		const MELVariable mapShaderNode =
		        createMapShader(sb, matInfo.bumpMap, matInfo.bumpmapTrafo, shaderName, L"bumpMap", true, false);

		MELVariable bumpLuminanceNode(L"bumpLuminanceNode");
		sb.setVar(bumpLuminanceNode, shadingEngineName + L"_bump_luminance");
		sb.createShader(L"luminance", bumpLuminanceNode);
		sb.connectAttr(mapShaderNode, L"outColor", bumpLuminanceNode, L"value");
		sb.connectAttr(bumpLuminanceNode, L"outValue", bumpValueNode, L"bumpValue");
	}

	// dirt map
	if (matInfo.dirtmap.empty()) {
		sb.setAttr(dirtMapBlendNode, L"input2", 1.0, 1.0, 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_dirt_map";
		const MELVariable mapShaderNode =
		        createMapShader(sb, matInfo.dirtmap, matInfo.dirtmapTrafo, shaderName, L"dirtMap", false, false);
		sb.connectAttr(mapShaderNode, L"outColor", dirtMapBlendNode, L"input2");
	}

	// reflectivity
	sb.setAttr(shaderNode, L"specular", 1.0);

	// specular/specular map multiply node
	MELVariable specularMapBlendNode(L"specularMapBlendNode");
	sb.setVar(specularMapBlendNode, shadingEngineName + L"_specular_map_blend");
	sb.createShader(L"aiMultiply", specularMapBlendNode);
	sb.connectAttr(specularMapBlendNode, L"outColor", shaderNode, L"specularColor");

	// ignore the specular color for now (matInfo.specularColor), since in the metallic-roughness
	// model of glTF specularity is controlled entirely via the roughness which requires the specular
	// color of the aiStandardSurface shader to be set to white, however, the default value for
	// matInfo.specularColor is black
	sb.setAttr(specularMapBlendNode, L"input1", 1.0, 1.0, 1.0);

	// specular map
	if (matInfo.specularMap.empty()) {
		sb.setAttr(specularMapBlendNode, L"input2", 1.0, 1.0, 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_specular_map";
		const MELVariable mapShaderNode = createMapShader(sb, matInfo.specularMap, matInfo.specularmapTrafo, shaderName,
		                                                  L"specularMap", false, false);
		sb.connectAttr(mapShaderNode, L"outColor", specularMapBlendNode, L"input2");
	}

	// opacity/opacity map multiply node
	MELVariable opacityMapBlendNode(L"opacityMapBlendNode");
	sb.setVar(opacityMapBlendNode, shadingEngineName + L"_opacity_map_blend");
	sb.createShader(L"aiMultiply", opacityMapBlendNode);
	sb.connectAttr(opacityMapBlendNode, L"outColorR", shaderNode, L"opacityR");
	sb.connectAttr(opacityMapBlendNode, L"outColorR", shaderNode, L"opacityG");
	sb.connectAttr(opacityMapBlendNode, L"outColorR", shaderNode, L"opacityB");

	// opacity
	sb.setAttr(opacityMapBlendNode, L"input1R", matInfo.opacity);

	// opacity map
	if (matInfo.opacityMap.empty()) {
		sb.setAttr(opacityMapBlendNode, L"input2R", 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_opacity_map";
		const MELVariable mapShaderNode = createMapShader(sb, matInfo.opacityMap, matInfo.opacitymapTrafo, shaderName,
		                                                  L"opacityMap", false, true);
		sb.connectAttr(mapShaderNode, L"outColorR", opacityMapBlendNode, L"input2R");
	}

	// normal map
	if (!matInfo.normalMap.empty()) {
		std::wstring shaderName = shadingEngineName + L"_normal_map";
		const MELVariable mapShaderNode =
		        createMapShader(sb, matInfo.normalMap, matInfo.normalmapTrafo, shaderName, L"normalMap", true, false);

		MELVariable normalMapConvertNode(L"normalMapConvertNode");
		sb.setVar(normalMapConvertNode, shadingEngineName + L"_normal_map_convert");
		sb.createShader(L"aiNormalMap", normalMapConvertNode);
		sb.setAttr(normalMapConvertNode, L"colorToSigned", true);
		sb.connectAttr(mapShaderNode, L"outColor", normalMapConvertNode, L"input");
		sb.connectAttr(normalMapConvertNode, L"outValue", bumpValueNode, L"normalCamera");
	}

	// emission
	sb.setAttr(shaderNode, L"emission", 1.0);

	// emission/emissive map multiply node
	MELVariable emissiveMapBlendNode(L"emissiveMapBlendNode");
	sb.setVar(emissiveMapBlendNode, shadingEngineName + L"_emissive_map_blend");
	sb.createShader(L"aiMultiply", emissiveMapBlendNode);
	sb.connectAttr(emissiveMapBlendNode, L"outColor", shaderNode, L"emissionColor");

	// emissive color
	sb.setAttr(emissiveMapBlendNode, L"input1", matInfo.emissiveColor);

	// emissive map
	if (matInfo.emissiveMap.empty()) {
		sb.setAttr(emissiveMapBlendNode, L"input2", 1.0, 1.0, 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_emissive_map";
		const MELVariable mapShaderNode = createMapShader(sb, matInfo.emissiveMap, matInfo.emissivemapTrafo, shaderName,
		                                                  L"emissiveMap", false, false);
		sb.connectAttr(mapShaderNode, L"outColor", emissiveMapBlendNode, L"input2");
	}

	// roughness/roughness map multiply node
	MELVariable roughnessMapBlendNode(L"roughnessMapBlendNode");
	sb.setVar(roughnessMapBlendNode, shadingEngineName + L"_roughness_map_blend");
	sb.createShader(L"aiMultiply", roughnessMapBlendNode);
	sb.connectAttr(roughnessMapBlendNode, L"outColorR", shaderNode, L"specularRoughness");

	// roughness
	sb.setAttr(roughnessMapBlendNode, L"input1R", matInfo.roughness);

	// roughness map
	if (matInfo.roughnessMap.empty()) {
		sb.setAttr(roughnessMapBlendNode, L"input2R", 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_roughness_map";
		const MELVariable mapShaderNode = createMapShader(sb, matInfo.roughnessMap, matInfo.roughnessmapTrafo,
		                                                  shaderName, L"roughnessMap", true, false);

		// in PRT the roughness map only uses the green channel
		sb.connectAttr(mapShaderNode, L"outColorG", roughnessMapBlendNode, L"input2R");
	}

	// metallic/metallic map multiply node
	MELVariable metallicMapBlendNode(L"metallicMapBlendNode");
	sb.setVar(metallicMapBlendNode, shadingEngineName + L"_metallic_map_blend");
	sb.createShader(L"aiMultiply", metallicMapBlendNode);
	sb.connectAttr(metallicMapBlendNode, L"outColorR", shaderNode, L"metalness");

	// metallic
	sb.setAttr(metallicMapBlendNode, L"input1R", matInfo.metallic);

	// metallic map
	if (matInfo.metallicMap.empty()) {
		sb.setAttr(metallicMapBlendNode, L"input2R", 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_metallic_map";
		const MELVariable mapShaderNode = createMapShader(sb, matInfo.metallicMap, matInfo.metallicmapTrafo, shaderName,
		                                                  L"metallicMap", true, false);

		// in PRT the metallic map only uses the blue channel
		sb.connectAttr(mapShaderNode, L"outColorB", metallicMapBlendNode, L"input2R");
	}
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

	MStatus status = MStatus::kSuccess;

	std::call_once(pluginDependencyCheckFlag, [&status]() {
		const bool b = MayaPluginUtilities::pluginDependencyCheck(PLUGIN_DEPENDENCIES);
		status = b ? MStatus::kSuccess : MStatus::kFailure;
	});
	if (status != MStatus::kSuccess)
		return status;

	MaterialUtils::forwardGeometry(aInMesh, aOutMesh, data);

	adsk::Data::Stream* inMatStream = MaterialUtils::getMaterialStream(aInMesh, data);
	if (inMatStream == nullptr)
		return MStatus::kSuccess;

	const adsk::Data::Structure* materialStructure = adsk::Data::Structure::structureByName(PRT_MATERIAL_STRUCTURE.c_str());
	if (materialStructure == nullptr)
		return MStatus::kFailure;

	MString meshName;
	const MStatus meshNameStatus = MaterialUtils::getMeshName(meshName, plug);
	if (meshNameStatus != MStatus::kSuccess || meshName.length() == 0)
		return meshNameStatus;

	MaterialUtils::MaterialCache matCache =
	        MaterialUtils::getMaterialsByStructure(*materialStructure, MATERIAL_BASE_NAME);

	MELScriptBuilder scriptBuilder;
	scriptBuilder.declString(MEL_VARIABLE_SHADING_ENGINE);
	scriptBuilder.declString(MELVariable(L"shaderNode"));
	scriptBuilder.declString(MELVariable(L"mapFile"));
	scriptBuilder.declString(MELVariable(L"mapNode"));
	scriptBuilder.declString(MELVariable(L"bumpLuminanceNode"));
	scriptBuilder.declString(MELVariable(L"bumpValueNode"));
	scriptBuilder.declString(MELVariable(L"displacementNode"));
	scriptBuilder.declString(MELVariable(L"normalMapConvertNode"));
	scriptBuilder.declString(MELVariable(L"colorMapBlendNode"));
	scriptBuilder.declString(MELVariable(L"dirtMapBlendNode"));
	scriptBuilder.declString(MELVariable(L"opacityMapBlendNode"));
	scriptBuilder.declString(MELVariable(L"specularMapBlendNode"));
	scriptBuilder.declString(MELVariable(L"emissiveMapBlendNode"));
	scriptBuilder.declString(MELVariable(L"roughnessMapBlendNode"));
	scriptBuilder.declString(MELVariable(L"metallicMapBlendNode"));
	scriptBuilder.declString(MELVariable(L"uvTrafoNode"));

	for (adsk::Data::Handle& inMatStreamHandle : *inMatStream) {
		if (!inMatStreamHandle.hasData())
			continue;

		if (!inMatStreamHandle.usesStructure(*materialStructure))
			continue;

		std::pair<int, int> faceRange;
		if (!MaterialUtils::getFaceRange(inMatStreamHandle, faceRange))
			continue;

		auto createShadingEngine = [this, &materialStructure, &scriptBuilder,
		                            &inMatStreamHandle](const MaterialInfo& matInfo) {
			const std::wstring shadingEngineBaseName = MATERIAL_BASE_NAME + L"Sg";
			const std::wstring shaderBaseName = MATERIAL_BASE_NAME + L"Sh";

			MStatus status;
			const std::wstring shadingEngineName = MaterialUtils::synchronouslyCreateShadingEngine(
			        shadingEngineBaseName, MEL_VARIABLE_SHADING_ENGINE, status);
			MCHECK(status);

			MaterialUtils::assignMaterialMetadata(*materialStructure, inMatStreamHandle, shadingEngineName);
			appendToMaterialScriptBuilder(scriptBuilder, matInfo, shaderBaseName, shadingEngineName);
			LOG_DBG << "new arnold shading engine: " << shadingEngineName;

			return shadingEngineName;
		};

		MaterialInfo matInfo(inMatStreamHandle);
		std::wstring shadingEngineName = getCachedValue(matCache, matInfo, createShadingEngine, matInfo);
		scriptBuilder.setsAddFaceRange(shadingEngineName, meshName.asWChar(), faceRange.first, faceRange.second);
		LOG_DBG << "assigned arnold shading engine (" << faceRange.first << ":" << faceRange.second
		        << "): " << shadingEngineName;
	}

	return scriptBuilder.execute();
}
