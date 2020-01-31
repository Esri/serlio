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

const std::wstring MATERIAL_BASE_NAME = L"serlioGeneratedMaterial";
const std::wstring MEL_VARIABLE_SHADING_ENGINE = L"$shadingGroup"; // FIXME: duplicate

std::once_flag pluginDependencyCheckFlag;
const std::vector<std::string> PLUGIN_DEPENDENCIES = {"shaderFXPlugin"};

} // namespace

MTypeId PRTMaterialNode::id(SerlioNodeIDs::SERLIO_PREFIX, SerlioNodeIDs::STRINGRAY_MATERIAL_NODE);

MObject PRTMaterialNode::aInMesh;
MObject PRTMaterialNode::aOutMesh;
std::wstring PRTMaterialNode::sfxFile;
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

	adsk::Data::Stream* inMatStream = MaterialUtils::getMaterialStream(aOutMesh, aInMesh, data);
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
	scriptBuilder.declString(L"$shaderNode");
	scriptBuilder.declString(L"$mapFile");
	scriptBuilder.declString(L"$mapNode");
	scriptBuilder.declInt(L"$shadingNodeIndex");

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

			const std::wstring shadingEngineName =
			        MaterialUtils::synchronouslyCreateShadingEngine(shadingEngineBaseName, MEL_VARIABLE_SHADING_ENGINE);
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

void setTexture(MELScriptBuilder& sb, const std::wstring& target, const std::wstring& shaderBaseName,
                const std::wstring& tex) {
	if (!tex.empty()) {
		sb.setVar(L"$mapNode", shaderBaseName);
		sb.setVar(L"$mapFile", tex);
		sb.createTexture(L"$mapNode");
		sb.setAttr(L"($mapNode + \".fileTextureName\")", L"$mapFile");

		sb.connectAttr(L"($mapNode + \".outColor\")", L"($shaderNode + \".TEX_" + target + L"\")");
		sb.setAttr(L"($shaderNode + \".use_" + target + L"\")", 1);
	}
	else {
		sb.setAttr(L"($shaderNode + \".use_" + target + L"\")", 0);
	}
}

} // namespace

void PRTMaterialNode::appendToMaterialScriptBuilder(MELScriptBuilder& sb, const MaterialInfo& matInfo,
                                                    const std::wstring& shaderBaseName,
                                                    const std::wstring& shadingEngineName) const {
	// determine path of shader fx file
	// TODO: move out of here
	if (sfxFile.length() == 0) {
		// mel command wants forward slashes
		const std::wstring shadersPath = prtu::toGenericPath(mPRTCtx.mPluginRootPath + L"../shaders/");
		sfxFile = shadersPath + L"serlioShaderStingray.sfx";
		LOG_DBG << "stingray shader located at " << sfxFile;
	}

	// create shader
	sb.setVar(L"$shaderNode", shaderBaseName);
	sb.setVar(MEL_VARIABLE_SHADING_ENGINE, shadingEngineName);
	sb.createShader(L"StingrayPBS", L"$shaderNode");

	// connect to shading group
	sb.connectAttr(L"($shaderNode + \".outColor\")", L"(" + MEL_VARIABLE_SHADING_ENGINE + L" + \".surfaceShader\")");

	// stingray specifics
	sb.addCmdLine(L"shaderfx -sfxnode $shaderNode -loadGraph  \"" + sfxFile + L"\";");
	sb.setAttr(L"($shaderNode + \".initgraph\")", true);

	sb.addCmdLine(L"$shadingNodeIndex = `shaderfx -sfxnode $shaderNode -getNodeIDByName \"Standard_Base\"`;");

	const std::wstring blendMode = (matInfo.opacityMap.empty() && (matInfo.opacity >= 1.0)) ? L"0" : L"1";
	sb.addCmdLine(L"shaderfx -sfxnode $shaderNode -edit_stringlist $shadingNodeIndex blendmode " + blendMode + L";");

	// ignored: ambientColor, specularColor
	sb.setAttr(L"($shaderNode + \".diffuse_color\")", matInfo.diffuseColor);
	sb.setAttr(L"($shaderNode + \".emissive_color\")", matInfo.emissiveColor);
	sb.setAttr(L"($shaderNode + \".opacity\")", matInfo.opacity);
	sb.setAttr(L"($shaderNode + \".roughness\")", matInfo.roughness);
	sb.setAttr(L"($shaderNode + \".metallic\")", matInfo.metallic);

	// ignored: specularmapTrafo, bumpmapTrafo, occlusionmapTrafo
	// shaderfx does not support 5 values per input, that's why we split it up in tuv and swuv
	sb.setAttr(L"($shaderNode + \".colormap_trafo_tuv\")", matInfo.colormapTrafo.tuv());
	sb.setAttr(L"($shaderNode + \".dirtmap_trafo_tuv\")", matInfo.dirtmapTrafo.tuv());
	sb.setAttr(L"($shaderNode + \".emissivemap_trafo_tuv\")", matInfo.emissivemapTrafo.tuv());
	sb.setAttr(L"($shaderNode + \".metallicmap_trafo_tuv\")", matInfo.metallicmapTrafo.tuv());
	sb.setAttr(L"($shaderNode + \".normalmap_trafo_tuv\")", matInfo.normalmapTrafo.tuv());
	sb.setAttr(L"($shaderNode + \".opacitymap_trafo_tuv\")", matInfo.opacitymapTrafo.tuv());
	sb.setAttr(L"($shaderNode + \".roughnessmap_trafo_tuv\")", matInfo.roughnessmapTrafo.tuv());

	sb.setAttr(L"($shaderNode + \".colormap_trafo_suvw\")", matInfo.colormapTrafo.suvw());
	sb.setAttr(L"($shaderNode + \".dirtmap_trafo_suvw\")", matInfo.dirtmapTrafo.suvw());
	sb.setAttr(L"($shaderNode + \".emissivemap_trafo_suvw\")", matInfo.emissivemapTrafo.suvw());
	sb.setAttr(L"($shaderNode + \".metallicmap_trafo_suvw\")", matInfo.metallicmapTrafo.suvw());
	sb.setAttr(L"($shaderNode + \".normalmap_trafo_suvw\")", matInfo.normalmapTrafo.suvw());
	sb.setAttr(L"($shaderNode + \".opacitymap_trafo_suvw\")", matInfo.opacitymapTrafo.suvw());
	sb.setAttr(L"($shaderNode + \".roughnessmap_trafo_suvw\")", matInfo.roughnessmapTrafo.suvw());

	// ignored: bumpMap, specularMap, occlusionmap
	// TODO: avoid wide/narrow conversion of map strings
	setTexture(sb, L"color_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.colormap));
	setTexture(sb, L"dirt_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.dirtmap));
	setTexture(sb, L"emissive_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.emissiveMap));
	setTexture(sb, L"metallic_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.metallicMap));
	setTexture(sb, L"normal_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.normalMap));
	setTexture(sb, L"roughness_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.roughnessMap));
	setTexture(sb, L"opacity_map", shaderBaseName, prtu::toUTF16FromOSNarrow(matInfo.opacityMap));
}
