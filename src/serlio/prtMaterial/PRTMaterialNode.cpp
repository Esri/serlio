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

#include "prtMaterial/PRTMaterialNode.h"

#include "prtMaterial/MaterialInfo.h"
#include "prtMaterial/MaterialUtils.h"

#include "prtModifier/PRTModifierAction.h"

#include "util/MELScriptBuilder.h"
#include "util/MItDependencyNodesWrapper.h"
#include "util/MayaUtilities.h"

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
const MELVariable MEL_VARIABLE_SHADING_ENGINE(L"shadingGroup"); // FIXME: duplicate

std::once_flag pluginDependencyCheckFlag;
const std::vector<std::string> PLUGIN_DEPENDENCIES = {"shaderFXPlugin"};

} // namespace

MTypeId PRTMaterialNode::id(SerlioNodeIDs::SERLIO_PREFIX, SerlioNodeIDs::STRINGRAY_MATERIAL_NODE);

MObject PRTMaterialNode::aInMesh;
MObject PRTMaterialNode::aOutMesh;
const MString OUTPUT_GEOMETRY = MString("og");

MStatus PRTMaterialNode::initialize() {
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

MStatus PRTMaterialNode::compute(const MPlug& plug, MDataBlock& data) {
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

	const adsk::Data::Structure* materialStructure = adsk::Data::Structure::structureByName(gPRTMatStructure.c_str());
	if (materialStructure == nullptr)
		return MStatus::kFailure;

	MString meshName;
	MStatus meshNameStatus = MaterialUtils::getMeshName(meshName, plug);
	if (meshNameStatus != MStatus::kSuccess || meshName.length() == 0)
		return meshNameStatus;

	MaterialUtils::MaterialCache matCache =
	        MaterialUtils::getMaterialsByStructure(*materialStructure, MATERIAL_BASE_NAME);

	MELScriptBuilder scriptBuilder;
	scriptBuilder.declString(MEL_VARIABLE_SHADING_ENGINE);

	// declare MEL variables required by appendToMaterialScriptBuilder()
	scriptBuilder.declString(MELVariable(L"shaderNode"));
	scriptBuilder.declString(MELVariable(L"mapFile"));
	scriptBuilder.declString(MELVariable(L"mapNode"));
	scriptBuilder.declInt(MELVariable(L"shadingNodeIndex"));

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

	LOG_DBG << "scheduling stringray material script";
	return scriptBuilder.execute(); // note: script is executed asynchronously
}

namespace {

void setTexture(MELScriptBuilder& sb, const MELVariable& shaderNode, const std::wstring& target,
                const std::wstring& textureNodeBaseName, const std::wstring& tex) {
	if (!tex.empty()) {
		MELVariable mapNode(L"mapNode");
		sb.setVar(mapNode, textureNodeBaseName);

		MELVariable mapFile(L"mapFile");
		sb.setVar(mapFile, tex);

		sb.createTextureShadingNode(mapNode);
		sb.setAttr(mapNode, L"fileTextureName", mapFile.mel());

		sb.connectAttr(mapNode, L"outColor", shaderNode, L"TEX_" + target);
		sb.setAttr(shaderNode, L"use_" + target, 1);
	}
	else {
		sb.setAttr(shaderNode, L"use_" + target, 0);
	}
}

} // namespace

void PRTMaterialNode::appendToMaterialScriptBuilder(MELScriptBuilder& sb, const MaterialInfo& matInfo,
                                                    const std::wstring& shaderBaseName,
                                                    const std::wstring& shadingEngineName) const {
	// create shader
	MELVariable shaderNode(L"shaderNode");
	sb.setVar(shaderNode, shaderBaseName);
	sb.setVar(MEL_VARIABLE_SHADING_ENGINE, shadingEngineName);
	sb.createShader(L"StingrayPBS", shaderNode);

	// connect to shading group
	sb.connectAttr(shaderNode, L"outColor", MEL_VARIABLE_SHADING_ENGINE, L"surfaceShader");

	// stingray specifics
	const std::wstring sfxFile = MaterialUtils::getStingrayShaderPath();
	sb.addCmdLine(L"shaderfx -sfxnode $shaderNode -loadGraph  \"" + sfxFile + L"\";");
	sb.setAttr(shaderNode, L"initgraph", true);

	sb.addCmdLine(L"$shadingNodeIndex = `shaderfx -sfxnode $shaderNode -getNodeIDByName \"Standard_Base\"`;");

	const std::wstring blendMode = (matInfo.opacityMap.empty() && (matInfo.opacity >= 1.0)) ? L"0" : L"1";
	sb.addCmdLine(L"shaderfx -sfxnode $shaderNode -edit_stringlist $shadingNodeIndex blendmode " + blendMode + L";");

	// ignored: ambientColor, specularColor
	sb.setAttr(shaderNode, L"diffuse_color", matInfo.diffuseColor);
	sb.setAttr(shaderNode, L"emissive_color", matInfo.emissiveColor);
	sb.setAttr(shaderNode, L"opacity", matInfo.opacity);
	sb.setAttr(shaderNode, L"roughness", matInfo.roughness);
	sb.setAttr(shaderNode, L"metallic", matInfo.metallic);

	// ignored: specularmapTrafo, bumpmapTrafo, occlusionmapTrafo
	// shaderfx does not support 5 values per input, that's why we split it up in tuv and swuv
	sb.setAttr(shaderNode, L"colormap_trafo_tuv", matInfo.colormapTrafo.tuv());
	sb.setAttr(shaderNode, L"dirtmap_trafo_tuv", matInfo.dirtmapTrafo.tuv());
	sb.setAttr(shaderNode, L"emissivemap_trafo_tuv", matInfo.emissivemapTrafo.tuv());
	sb.setAttr(shaderNode, L"metallicmap_trafo_tuv", matInfo.metallicmapTrafo.tuv());
	sb.setAttr(shaderNode, L"normalmap_trafo_tuv", matInfo.normalmapTrafo.tuv());
	sb.setAttr(shaderNode, L"opacitymap_trafo_tuv", matInfo.opacitymapTrafo.tuv());
	sb.setAttr(shaderNode, L"roughnessmap_trafo_tuv", matInfo.roughnessmapTrafo.tuv());

	sb.setAttr(shaderNode, L"colormap_trafo_suvw", matInfo.colormapTrafo.suvw());
	sb.setAttr(shaderNode, L"dirtmap_trafo_suvw", matInfo.dirtmapTrafo.suvw());
	sb.setAttr(shaderNode, L"emissivemap_trafo_suvw", matInfo.emissivemapTrafo.suvw());
	sb.setAttr(shaderNode, L"metallicmap_trafo_suvw", matInfo.metallicmapTrafo.suvw());
	sb.setAttr(shaderNode, L"normalmap_trafo_suvw", matInfo.normalmapTrafo.suvw());
	sb.setAttr(shaderNode, L"opacitymap_trafo_suvw", matInfo.opacitymapTrafo.suvw());
	sb.setAttr(shaderNode, L"roughnessmap_trafo_suvw", matInfo.roughnessmapTrafo.suvw());

	// ignored: bumpMap, specularMap, occlusionmap
	// TODO: avoid wide/narrow conversion of map strings
	setTexture(sb, shaderNode, L"color_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.colormap));
	setTexture(sb, shaderNode, L"dirt_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.dirtmap));
	setTexture(sb, shaderNode, L"emissive_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.emissiveMap));
	setTexture(sb, shaderNode, L"metallic_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.metallicMap));
	setTexture(sb, shaderNode, L"normal_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.normalMap));
	setTexture(sb, shaderNode, L"roughness_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.roughnessMap));
	setTexture(sb, shaderNode, L"opacity_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.opacityMap));
}
