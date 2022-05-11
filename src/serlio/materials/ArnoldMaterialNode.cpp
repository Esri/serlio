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

#include "materials/ArnoldMaterialNode.h"
#include "materials/MaterialInfo.h"
#include "materials/MaterialUtils.h"

#include "utils/MELScriptBuilder.h"

namespace {

const std::wstring MATERIAL_BASE_NAME = L"serlioArnoldMaterial";
const std::vector<std::string> PLUGIN_DEPENDENCIES = {"mtoa"};

const MELVariable MEL_VAR_SHADER_NODE(L"shaderNode");
const MELVariable MEL_VAR_MAP_FILE(L"mapFile");
const MELVariable MEL_VAR_MAP_NODE(L"mapNode");
const MELVariable MEL_VAR_BUMP_LUMINANCE_NODE(L"bumpLuminanceNode");
const MELVariable MEL_VAR_BUMP_VALUE_NODE(L"bumpValueNode");
const MELVariable MEL_VAR_DISPLACEMENT_NODE(L"displacementNode");
const MELVariable MEL_VAR_NORMAL_MAP_CONVERT_NODE(L"normalMapConvertNode");
const MELVariable MEL_VAR_COLOR_MAP_BLEND_NODE(L"colorMapBlendNode");
const MELVariable MEL_VAR_DIRTMAP_BLEND_NODE(L"dirtMapBlendNode");
const MELVariable MEL_VAR_OPACITYMAP_BLEND_NODE(L"opacityMapBlendNode");
const MELVariable MEL_VAR_SPECULARMAP_BLEND_NODE(L"specularMapBlendNode");
const MELVariable MEL_VAR_EMISSIVEMAP_BLEND_NODE(L"emissiveMapBlendNode");
const MELVariable MEL_VAR_ROUGHNESSMAP_BLEND_NODE(L"roughnessMapBlendNode");
const MELVariable MEL_VAR_METALLICMAP_BLEND_NODE(L"metallicMapBlendNode");
const MELVariable MEL_VAR_UV_TRAFO_NODE(L"uvTrafoNode");

void setUvTransformAttrs(MELScriptBuilder& sb, const std::wstring& uvSet, const MaterialTrafo& trafo) {
	sb.setAttr(MEL_VAR_UV_TRAFO_NODE, L"uvset", MELStringLiteral(uvSet));
	sb.setAttr(MEL_VAR_UV_TRAFO_NODE, L"pivotFrame", 0.0, 0.0);
	sb.setAttr(MEL_VAR_UV_TRAFO_NODE, L"scaleFrame", 1.0 / trafo.su(), 1.0 / trafo.sv());
	sb.setAttr(MEL_VAR_UV_TRAFO_NODE, L"translateFrame", -trafo.tu() / trafo.su(), -trafo.tv() / trafo.sv());
	if (trafo.rw() != 0.0) {
		LOG_WRN << "rotation (material.map.rw) is not yet supported\n";
	}
}

void createMapShader(MELScriptBuilder& sb, const std::string& tex, const MaterialTrafo& mapTrafo,
                     const std::wstring& shaderName, const std::wstring& uvSet, const bool raw, const bool alpha) {
	std::filesystem::path texPath(tex);
	const std::wstring nodeName = prtu::cleanNameForMaya(texPath.stem().wstring());
	sb.setVar(MEL_VAR_MAP_NODE, MELStringLiteral(nodeName));

	sb.setVar(MEL_VAR_MAP_FILE, MELStringLiteral(prtu::toUTF16FromOSNarrow(tex)));
	sb.createTextureShadingNode(MEL_VAR_MAP_NODE);
	sb.setAttr(MEL_VAR_MAP_NODE, L"fileTextureName", MEL_VAR_MAP_FILE);

	if (raw) {
		sb.setAttr(MEL_VAR_MAP_NODE, L"colorSpace", MELStringLiteral(L"Raw"));
		sb.setAttr(MEL_VAR_MAP_NODE, L"ignoreColorSpaceFileRules", true);
	}

	sb.setVar(MEL_VAR_UV_TRAFO_NODE, MELStringLiteral(shaderName + L"_trafo"));
	sb.createShader(L"aiUvTransform", MEL_VAR_UV_TRAFO_NODE);
	setUvTransformAttrs(sb, uvSet, mapTrafo);

	if (alpha) {
		sb.connectAttr(MEL_VAR_MAP_NODE, L"outAlpha", MEL_VAR_UV_TRAFO_NODE, L"passthroughR");
		sb.forceValidTextureAlphaChannel(MEL_VAR_MAP_NODE);
		sb.setAttr(MEL_VAR_MAP_NODE, L"alphaIsLuminance", !MaterialUtils::textureHasAlphaChannel(texPath.wstring()));
	}
	else
		sb.connectAttr(MEL_VAR_MAP_NODE, L"outColor", MEL_VAR_UV_TRAFO_NODE, L"passthrough");
}

} // namespace

MTypeId ArnoldMaterialNode::id(SerlioNodeIDs::SERLIO_PREFIX, SerlioNodeIDs::ARNOLD_MATERIAL_NODE);

MObject ArnoldMaterialNode::mInMesh;
MObject ArnoldMaterialNode::mOutMesh;

MStatus ArnoldMaterialNode::initialize() {
	return initializeAttributes(mInMesh, mOutMesh);
}

void ArnoldMaterialNode::declareMaterialStrings(MELScriptBuilder& sb) {
	sb.declString(MEL_VAR_SHADER_NODE);
	sb.declString(MEL_VAR_MAP_FILE);
	sb.declString(MEL_VAR_MAP_NODE);
	sb.declString(MEL_VAR_BUMP_LUMINANCE_NODE);
	sb.declString(MEL_VAR_BUMP_VALUE_NODE);
	sb.declString(MEL_VAR_DISPLACEMENT_NODE);
	sb.declString(MEL_VAR_NORMAL_MAP_CONVERT_NODE);
	sb.declString(MEL_VAR_COLOR_MAP_BLEND_NODE);
	sb.declString(MEL_VAR_DIRTMAP_BLEND_NODE);
	sb.declString(MEL_VAR_OPACITYMAP_BLEND_NODE);
	sb.declString(MEL_VAR_SPECULARMAP_BLEND_NODE);
	sb.declString(MEL_VAR_EMISSIVEMAP_BLEND_NODE);
	sb.declString(MEL_VAR_ROUGHNESSMAP_BLEND_NODE);
	sb.declString(MEL_VAR_METALLICMAP_BLEND_NODE);
	sb.declString(MEL_VAR_UV_TRAFO_NODE);
}

void ArnoldMaterialNode::appendToMaterialScriptBuilder(MELScriptBuilder& sb, const MaterialInfo& matInfo,
                                                       const std::wstring& shaderBaseName,
                                                       const std::wstring& shadingEngineName) {
	// create shader
	sb.setVar(MEL_VAR_SHADER_NODE, MELStringLiteral(shaderBaseName));
	sb.setVar(MEL_VARIABLE_SHADING_ENGINE, MELStringLiteral(shadingEngineName));
	sb.createShader(L"aiStandardSurface", MEL_VAR_SHADER_NODE); // note: name might change to be unique

	// connect to shading group
	sb.connectAttr(MEL_VAR_SHADER_NODE, L"outColor", MEL_VARIABLE_SHADING_ENGINE, L"surfaceShader");

	sb.setAttr(MEL_VAR_SHADER_NODE, L"base", 1.0);

	// color/dirt map multiply node
	sb.setVar(MEL_VAR_DIRTMAP_BLEND_NODE, MELStringLiteral(shadingEngineName + L"_dirt_multiply"));
	sb.createShader(L"aiMultiply", MEL_VAR_DIRTMAP_BLEND_NODE);
	sb.connectAttr(MEL_VAR_DIRTMAP_BLEND_NODE, L"outColor", MEL_VAR_SHADER_NODE, L"baseColor");

	// color/color map multiply node
	sb.setVar(MEL_VAR_COLOR_MAP_BLEND_NODE, MELStringLiteral(shadingEngineName + L"_color_map_blend"));
	sb.createShader(L"aiMultiply", MEL_VAR_COLOR_MAP_BLEND_NODE);
	sb.connectAttr(MEL_VAR_COLOR_MAP_BLEND_NODE, L"outColor", MEL_VAR_DIRTMAP_BLEND_NODE, L"input1");

	// color
	sb.setAttr(MEL_VAR_COLOR_MAP_BLEND_NODE, L"input1", matInfo.diffuseColor);

	// color map
	if (matInfo.colormap.empty()) {
		sb.setAttr(MEL_VAR_COLOR_MAP_BLEND_NODE, L"input2", 1.0, 1.0, 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_color_map";
		createMapShader(sb, matInfo.colormap, matInfo.colormapTrafo, shaderName, L"map1", false, false);
		sb.connectAttr(MEL_VAR_UV_TRAFO_NODE, L"outColor", MEL_VAR_COLOR_MAP_BLEND_NODE, L"input2");
	}

	// bump map
	sb.setVar(MEL_VAR_BUMP_VALUE_NODE, MELStringLiteral(shadingEngineName + L"_bump_value"));
	sb.createShader(L"bump2d", MEL_VAR_BUMP_VALUE_NODE);
	sb.connectAttr(MEL_VAR_BUMP_VALUE_NODE, L"outNormal", MEL_VAR_SHADER_NODE, L"normalCamera");

	if (matInfo.bumpMap.empty()) {
		sb.setAttr(MEL_VAR_BUMP_VALUE_NODE, L"bumpValue", 0.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_bump_map";
		createMapShader(sb, matInfo.bumpMap, matInfo.bumpmapTrafo, shaderName, L"bumpMap", true, false);

		sb.setVar(MEL_VAR_BUMP_LUMINANCE_NODE, MELStringLiteral(shadingEngineName + L"_bump_luminance"));
		sb.createShader(L"luminance", MEL_VAR_BUMP_LUMINANCE_NODE);
		sb.connectAttr(MEL_VAR_UV_TRAFO_NODE, L"outColor", MEL_VAR_BUMP_LUMINANCE_NODE, L"value");
		sb.connectAttr(MEL_VAR_BUMP_LUMINANCE_NODE, L"outValue", MEL_VAR_BUMP_VALUE_NODE, L"bumpValue");
	}

	// dirt map
	if (matInfo.dirtmap.empty()) {
		sb.setAttr(MEL_VAR_DIRTMAP_BLEND_NODE, L"input2", 1.0, 1.0, 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_dirt_map";
		createMapShader(sb, matInfo.dirtmap, matInfo.dirtmapTrafo, shaderName, L"dirtMap", false, false);
		sb.connectAttr(MEL_VAR_UV_TRAFO_NODE, L"outColor", MEL_VAR_DIRTMAP_BLEND_NODE, L"input2");
	}

	// reflectivity
	sb.setAttr(MEL_VAR_SHADER_NODE, L"specular", 1.0);

	// specular/specular map multiply node
	sb.setVar(MEL_VAR_SPECULARMAP_BLEND_NODE, MELStringLiteral(shadingEngineName + L"_specular_map_blend"));
	sb.createShader(L"aiMultiply", MEL_VAR_SPECULARMAP_BLEND_NODE);
	sb.connectAttr(MEL_VAR_SPECULARMAP_BLEND_NODE, L"outColor", MEL_VAR_SHADER_NODE, L"specularColor");

	// ignore the specular color for now (matInfo.specularColor), since in the metallic-roughness
	// model of glTF specularity is controlled entirely via the roughness which requires the specular
	// color of the aiStandardSurface shader to be set to white, however, the default value for
	// matInfo.specularColor is black
	sb.setAttr(MEL_VAR_SPECULARMAP_BLEND_NODE, L"input1", 1.0, 1.0, 1.0);

	// specular map
	if (matInfo.specularMap.empty()) {
		sb.setAttr(MEL_VAR_SPECULARMAP_BLEND_NODE, L"input2", 1.0, 1.0, 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_specular_map";
		createMapShader(sb, matInfo.specularMap, matInfo.specularmapTrafo, shaderName, L"specularMap", false, false);
		sb.connectAttr(MEL_VAR_UV_TRAFO_NODE, L"outColor", MEL_VAR_SPECULARMAP_BLEND_NODE, L"input2");
	}

	// opacity/opacity map multiply node
	sb.setVar(MEL_VAR_OPACITYMAP_BLEND_NODE, MELStringLiteral(shadingEngineName + L"_opacity_map_blend"));
	sb.createShader(L"aiMultiply", MEL_VAR_OPACITYMAP_BLEND_NODE);
	sb.connectAttr(MEL_VAR_OPACITYMAP_BLEND_NODE, L"outColorR", MEL_VAR_SHADER_NODE, L"opacityR");
	sb.connectAttr(MEL_VAR_OPACITYMAP_BLEND_NODE, L"outColorR", MEL_VAR_SHADER_NODE, L"opacityG");
	sb.connectAttr(MEL_VAR_OPACITYMAP_BLEND_NODE, L"outColorR", MEL_VAR_SHADER_NODE, L"opacityB");

	// opacity
	sb.setAttr(MEL_VAR_OPACITYMAP_BLEND_NODE, L"input1R", matInfo.opacity);

	// opacity map
	if (matInfo.opacityMap.empty()) {
		sb.setAttr(MEL_VAR_OPACITYMAP_BLEND_NODE, L"input2R", 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_opacity_map";
		createMapShader(sb, matInfo.opacityMap, matInfo.opacitymapTrafo, shaderName, L"opacityMap", false, true);
		sb.connectAttr(MEL_VAR_UV_TRAFO_NODE, L"outColorR", MEL_VAR_OPACITYMAP_BLEND_NODE, L"input2R");
	}

	// normal map
	if (!matInfo.normalMap.empty()) {
		std::wstring shaderName = shadingEngineName + L"_normal_map";
		createMapShader(sb, matInfo.normalMap, matInfo.normalmapTrafo, shaderName, L"normalMap", true, false);
		sb.setVar(MEL_VAR_NORMAL_MAP_CONVERT_NODE, MELStringLiteral(shadingEngineName + L"_normal_map_convert"));
		sb.createShader(L"aiNormalMap", MEL_VAR_NORMAL_MAP_CONVERT_NODE);
		sb.setAttr(MEL_VAR_NORMAL_MAP_CONVERT_NODE, L"colorToSigned", true);
		sb.connectAttr(MEL_VAR_UV_TRAFO_NODE, L"outColor", MEL_VAR_NORMAL_MAP_CONVERT_NODE, L"input");
		sb.connectAttr(MEL_VAR_NORMAL_MAP_CONVERT_NODE, L"outValue", MEL_VAR_BUMP_VALUE_NODE, L"normalCamera");
	}

	// emission
	sb.setAttr(MEL_VAR_SHADER_NODE, L"emission", 1.0);

	// emission/emissive map multiply node
	sb.setVar(MEL_VAR_EMISSIVEMAP_BLEND_NODE, MELStringLiteral(shadingEngineName + L"_emissive_map_blend"));
	sb.createShader(L"aiMultiply", MEL_VAR_EMISSIVEMAP_BLEND_NODE);
	sb.connectAttr(MEL_VAR_EMISSIVEMAP_BLEND_NODE, L"outColor", MEL_VAR_SHADER_NODE, L"emissionColor");

	// emissive color
	sb.setAttr(MEL_VAR_EMISSIVEMAP_BLEND_NODE, L"input1", matInfo.emissiveColor);

	// emissive map
	if (matInfo.emissiveMap.empty()) {
		sb.setAttr(MEL_VAR_EMISSIVEMAP_BLEND_NODE, L"input2", 1.0, 1.0, 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_emissive_map";
		createMapShader(sb, matInfo.emissiveMap, matInfo.emissivemapTrafo, shaderName, L"emissiveMap", false, false);
		sb.connectAttr(MEL_VAR_UV_TRAFO_NODE, L"outColor", MEL_VAR_EMISSIVEMAP_BLEND_NODE, L"input2");
	}

	// roughness/roughness map multiply node
	sb.setVar(MEL_VAR_ROUGHNESSMAP_BLEND_NODE, MELStringLiteral(shadingEngineName + L"_roughness_map_blend"));
	sb.createShader(L"aiMultiply", MEL_VAR_ROUGHNESSMAP_BLEND_NODE);
	sb.connectAttr(MEL_VAR_ROUGHNESSMAP_BLEND_NODE, L"outColorR", MEL_VAR_SHADER_NODE, L"specularRoughness");

	// roughness
	sb.setAttr(MEL_VAR_ROUGHNESSMAP_BLEND_NODE, L"input1R", matInfo.roughness);

	// roughness map
	if (matInfo.roughnessMap.empty()) {
		sb.setAttr(MEL_VAR_ROUGHNESSMAP_BLEND_NODE, L"input2R", 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_roughness_map";
		createMapShader(sb, matInfo.roughnessMap, matInfo.roughnessmapTrafo, shaderName, L"roughnessMap", true, false);

		// in PRT the roughness map only uses the green channel
		sb.connectAttr(MEL_VAR_UV_TRAFO_NODE, L"outColorG", MEL_VAR_ROUGHNESSMAP_BLEND_NODE, L"input2R");
	}

	// metallic/metallic map multiply node
	sb.setVar(MEL_VAR_METALLICMAP_BLEND_NODE, MELStringLiteral(shadingEngineName + L"_metallic_map_blend"));
	sb.createShader(L"aiMultiply", MEL_VAR_METALLICMAP_BLEND_NODE);
	sb.connectAttr(MEL_VAR_METALLICMAP_BLEND_NODE, L"outColorR", MEL_VAR_SHADER_NODE, L"metalness");

	// metallic
	sb.setAttr(MEL_VAR_METALLICMAP_BLEND_NODE, L"input1R", matInfo.metallic);

	// metallic map
	if (matInfo.metallicMap.empty()) {
		sb.setAttr(MEL_VAR_METALLICMAP_BLEND_NODE, L"input2R", 1.0);
	}
	else {
		std::wstring shaderName = shadingEngineName + L"_metallic_map";
		createMapShader(sb, matInfo.metallicMap, matInfo.metallicmapTrafo, shaderName, L"metallicMap", true, false);

		// in PRT the metallic map only uses the blue channel
		sb.connectAttr(MEL_VAR_UV_TRAFO_NODE, L"outColorB", MEL_VAR_METALLICMAP_BLEND_NODE, L"input2R");
	}
}

std::wstring ArnoldMaterialNode::getBaseName() const {
	return MATERIAL_BASE_NAME;
}

MObject ArnoldMaterialNode::getInMesh() const {
	return mInMesh;
}

MObject ArnoldMaterialNode::getOutMesh() const {
	return mOutMesh;
}

std::vector<std::string> ArnoldMaterialNode::getPluginDependencies() const {
	return PLUGIN_DEPENDENCIES;
}
