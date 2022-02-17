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

#include "materials/StingrayMaterialNode.h"
#include "materials/MaterialInfo.h"
#include "materials/MaterialUtils.h"

#include "modifiers/PRTModifierAction.h"

#include "utils/MELScriptBuilder.h"
#include "utils/MItDependencyNodesWrapper.h"
#include "utils/MayaUtilities.h"

#include "PRTContext.h"
#include "serlioPlugin.h"

#include "maya/MFnMesh.h"
#include "maya/MFnTypedAttribute.h"
#include "maya/MGlobal.h"
#include "maya/MItDependencyNodes.h"
#include "maya/MObject.h"
#include "maya/adskDataAssociations.h"
#include "maya/adskDataStream.h"

#include <array>
#include <mutex>
#include <set>

namespace {

constexpr bool DBG = false;

const std::wstring MATERIAL_BASE_NAME = L"serlioStingrayMaterial";

std::once_flag pluginDependencyCheckFlag;
const std::vector<std::string> PLUGIN_DEPENDENCIES = {"shaderFXPlugin"};

const MELVariable MEL_UNDO_STATE(L"serlioMaterialUndoState");
const MELVariable MEL_VAR_SHADER_NODE(L"shaderNode");
const MELVariable MEL_VAR_MAP_FILE(L"mapFile");
const MELVariable MEL_VAR_MAP_NODE(L"mapNode");
const MELVariable MEL_VAR_SHADING_NODE_INDEX(L"shadingNodeIndex");

void setTexture(MELScriptBuilder& sb, const std::wstring& target, const std::wstring& tex,
                const std::wstring& alphaTarget = {}) {
	if (!tex.empty()) {
		std::filesystem::path texPath(tex);
		sb.setVar(MEL_VAR_MAP_NODE, MELStringLiteral(texPath.stem().wstring()));

		sb.setVar(MEL_VAR_MAP_FILE, MELStringLiteral(tex));

		sb.createTextureShadingNode(MEL_VAR_MAP_NODE);
		sb.setAttr(MEL_VAR_MAP_NODE, L"fileTextureName", MEL_VAR_MAP_FILE);

		sb.connectAttr(MEL_VAR_MAP_NODE, L"outColor", MEL_VAR_SHADER_NODE, L"TEX_" + target);

		if (!alphaTarget.empty())
			sb.setAttr(MEL_VAR_SHADER_NODE, alphaTarget,
			           MaterialUtils::textureHasAlphaChannel(texPath));

		sb.setAttr(MEL_VAR_SHADER_NODE, L"use_" + target, 1);
	}
	else {
		sb.setAttr(MEL_VAR_SHADER_NODE, L"use_" + target, 0);
	}
}

void appendToMaterialScriptBuilder(MELScriptBuilder& sb, const MaterialInfo& matInfo,
                                   const std::wstring& shaderBaseName, const std::wstring& shadingEngineName) {
	// create shader
	sb.setVar(MEL_VAR_SHADER_NODE, MELStringLiteral(shaderBaseName));
	sb.setVar(MEL_VARIABLE_SHADING_ENGINE, MELStringLiteral(shadingEngineName));
	sb.createShader(L"StingrayPBS", MEL_VAR_SHADER_NODE);

	// connect to shading group
	sb.connectAttr(MEL_VAR_SHADER_NODE, L"outColor", MEL_VARIABLE_SHADING_ENGINE, L"surfaceShader");

	// stingray specifics
	const MELStringLiteral sfxFile(MaterialUtils::getStingrayShaderPath().generic_wstring());
	sb.addCmdLine(L"shaderfx -sfxnode " + MEL_VAR_SHADER_NODE.mel() + L" -loadGraph  " + sfxFile.mel() + L";");
	sb.setAttr(MEL_VAR_SHADER_NODE, L"initgraph", true);

	const MELStringLiteral nodeIDName(L"Standard_Base");
	sb.addCmdLine(MEL_VAR_SHADING_NODE_INDEX.mel() + L" = `shaderfx -sfxnode " + MEL_VAR_SHADER_NODE.mel() +
	              L" -getNodeIDByName " + nodeIDName.mel() + L"`;");

	const std::wstring blendMode = (matInfo.opacityMap.empty() && (matInfo.opacity >= 1.0)) ? L"0" : L"1";
	sb.addCmdLine(L"shaderfx -sfxnode " + MEL_VAR_SHADER_NODE.mel() + L" -edit_stringlist " +
	              MEL_VAR_SHADING_NODE_INDEX.mel() + L" blendmode " + blendMode + L";");

	// ignored: ambientColor, specularColor
	sb.setAttr(MEL_VAR_SHADER_NODE, L"diffuse_color", matInfo.diffuseColor);
	sb.setAttr(MEL_VAR_SHADER_NODE, L"emissive_color", matInfo.emissiveColor);
	sb.setAttr(MEL_VAR_SHADER_NODE, L"opacity", matInfo.opacity);
	sb.setAttr(MEL_VAR_SHADER_NODE, L"roughness", matInfo.roughness);
	sb.setAttr(MEL_VAR_SHADER_NODE, L"metallic", matInfo.metallic);

	// ignored: specularmapTrafo, bumpmapTrafo, occlusionmapTrafo
	// shaderfx does not support 5 values per input, that's why we split it up in tuv and swuv
	sb.setAttr(MEL_VAR_SHADER_NODE, L"colormap_trafo_tuv", matInfo.colormapTrafo.tuv());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"dirtmap_trafo_tuv", matInfo.dirtmapTrafo.tuv());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"emissivemap_trafo_tuv", matInfo.emissivemapTrafo.tuv());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"metallicmap_trafo_tuv", matInfo.metallicmapTrafo.tuv());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"normalmap_trafo_tuv", matInfo.normalmapTrafo.tuv());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"opacitymap_trafo_tuv", matInfo.opacitymapTrafo.tuv());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"roughnessmap_trafo_tuv", matInfo.roughnessmapTrafo.tuv());

	sb.setAttr(MEL_VAR_SHADER_NODE, L"colormap_trafo_suvw", matInfo.colormapTrafo.suvw());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"dirtmap_trafo_suvw", matInfo.dirtmapTrafo.suvw());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"emissivemap_trafo_suvw", matInfo.emissivemapTrafo.suvw());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"metallicmap_trafo_suvw", matInfo.metallicmapTrafo.suvw());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"normalmap_trafo_suvw", matInfo.normalmapTrafo.suvw());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"opacitymap_trafo_suvw", matInfo.opacitymapTrafo.suvw());
	sb.setAttr(MEL_VAR_SHADER_NODE, L"roughnessmap_trafo_suvw", matInfo.roughnessmapTrafo.suvw());

	// ignored: bumpMap, specularMap, occlusionmap
	// TODO: avoid wide/narrow conversion of map strings
	setTexture(sb, L"color_map", prtu::toUTF16FromOSNarrow(matInfo.colormap));
	setTexture(sb, L"dirt_map", prtu::toUTF16FromOSNarrow(matInfo.dirtmap));
	setTexture(sb, L"emissive_map", prtu::toUTF16FromOSNarrow(matInfo.emissiveMap));
	setTexture(sb, L"metallic_map", prtu::toUTF16FromOSNarrow(matInfo.metallicMap));
	setTexture(sb, L"normal_map", prtu::toUTF16FromOSNarrow(matInfo.normalMap));
	setTexture(sb, L"roughness_map", prtu::toUTF16FromOSNarrow(matInfo.roughnessMap));
	setTexture(sb, L"opacity_map", prtu::toUTF16FromOSNarrow(matInfo.opacityMap), L"opacity_map_uses_alpha_channel");
}

} // namespace

MTypeId StingrayMaterialNode::id(SerlioNodeIDs::SERLIO_PREFIX, SerlioNodeIDs::STRINGRAY_MATERIAL_NODE);

MObject StingrayMaterialNode::aInMesh;
MObject StingrayMaterialNode::aOutMesh;
const MString OUTPUT_GEOMETRY = MString("og");

MStatus StingrayMaterialNode::initialize() {
	MStatus status;

	MFnTypedAttribute tAttr;
	aInMesh = tAttr.create("inMesh", "im", MFnData::kMesh, MObject::kNullObj, &status);
	MCHECK(status);
	addAttribute(aInMesh);

	aOutMesh = tAttr.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &status);
	MCHECK(status);
	tAttr.setWritable(false);
	tAttr.setStorable(false);
	addAttribute(aOutMesh);

	attributeAffects(aInMesh, aOutMesh);

	return MStatus::kSuccess;
}

MStatus StingrayMaterialNode::compute(const MPlug& plug, MDataBlock& data) {
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

	const adsk::Data::Structure* materialStructure =
	        adsk::Data::Structure::structureByName(PRT_MATERIAL_STRUCTURE.c_str());
	if (materialStructure == nullptr)
		return MStatus::kFailure;

	MString meshName;
	MStatus meshNameStatus = MaterialUtils::getMeshName(meshName, plug);
	if (meshNameStatus != MStatus::kSuccess || meshName.length() == 0)
		return meshNameStatus;

	MaterialUtils::MaterialCache matCache =
	        MaterialUtils::getMaterialsByStructure(*materialStructure, MATERIAL_BASE_NAME);

	MELScriptBuilder scriptBuilder;
	scriptBuilder.declInt(MEL_UNDO_STATE);
	scriptBuilder.getUndoState(MEL_UNDO_STATE);
	scriptBuilder.setUndoState(false);

	// declare MEL variables required by appendToMaterialScriptBuilder()
	scriptBuilder.declString(MEL_VAR_SHADER_NODE);
	scriptBuilder.declString(MEL_VAR_MAP_FILE);
	scriptBuilder.declString(MEL_VAR_MAP_NODE);
	scriptBuilder.declInt(MEL_VAR_SHADING_NODE_INDEX);

	for (adsk::Data::Handle& materialHandle : *inMatStream) {
		if (!materialHandle.hasData())
			continue;

		if (!materialHandle.usesStructure(*materialStructure))
			continue;

		std::pair<int, int> faceRange;
		if (!MaterialUtils::getFaceRange(materialHandle, faceRange))
			continue;

		auto createShadingEngine = [this, &materialStructure, &scriptBuilder,
		                            &materialHandle](const MaterialInfo& matInfo) {
			const std::wstring shadingEngineBaseName = MATERIAL_BASE_NAME + L"Sg";
			const std::wstring shaderBaseName = MATERIAL_BASE_NAME + L"Sh";

			MStatus status;
			const std::wstring shadingEngineName = MaterialUtils::synchronouslyCreateShadingEngine(
			        shadingEngineBaseName, MEL_VARIABLE_SHADING_ENGINE, status);
			MCHECK(status);

			MaterialUtils::assignMaterialMetadata(*materialStructure, materialHandle, shadingEngineName);
			appendToMaterialScriptBuilder(scriptBuilder, matInfo, shaderBaseName, shadingEngineName);
			LOG_DBG << "new stingray shading engine: " << shadingEngineName;

			return shadingEngineName;
		};

		MaterialInfo matInfo(materialHandle);
		std::wstring shadingEngineName = getCachedValue(matCache, matInfo, createShadingEngine, matInfo);
		scriptBuilder.setsAddFaceRange(shadingEngineName, meshName.asWChar(), faceRange.first, faceRange.second);
		LOG_DBG << "assigned stingray shading engine (" << faceRange.first << ":" << faceRange.second
		        << "): " << shadingEngineName;
	}
	scriptBuilder.setUndoState(MEL_UNDO_STATE);
	LOG_DBG << "scheduling stringray material script";
	return scriptBuilder.execute(); // note: script is executed asynchronously
}
