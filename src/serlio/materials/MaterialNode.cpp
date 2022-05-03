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

#include "materials/MaterialNode.h"
#include "materials/MaterialUtils.h"

#include "utils/MELScriptBuilder.h"

#include "maya/MFnTypedAttribute.h"
#include "maya/MUuid.h"

#include <mutex>

namespace {
std::once_flag pluginDependencyCheckFlag;

const MELVariable MEL_UNDO_STATE(L"serlioMaterialUndoState");
} // namespace

MStatus MaterialNode::initializeAttributes(MObject& inMesh, MObject& outMesh) {
	MStatus status;

	MFnTypedAttribute tAttr;
	inMesh = tAttr.create("inMesh", "im", MFnData::kMesh, MObject::kNullObj, &status);
	MCHECK(status);
	status = addAttribute(inMesh);
	MCHECK(status);

	outMesh = tAttr.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &status);
	MCHECK(status);
	status = tAttr.setWritable(false);
	MCHECK(status);
	status = tAttr.setStorable(false);
	MCHECK(status);
	status = addAttribute(outMesh);
	MCHECK(status);

	status = attributeAffects(inMesh, outMesh);
	MCHECK(status);

	return MStatus::kSuccess;
}

MStatus MaterialNode::compute(const MPlug& plug, MDataBlock& data) {
	MObject inMesh = getInMesh();
	MObject outMesh = getOutMesh();
	const std::wstring baseName = getBaseName();
	if (plug != outMesh)
		return MStatus::kUnknownParameter;

	MStatus status = MStatus::kSuccess;
	const std::vector<std::string> pluginDependencies = getPluginDependencies();

	std::call_once(pluginDependencyCheckFlag, [pluginDependencies, &status]() {
		const bool b = MayaPluginUtilities::pluginDependencyCheck(pluginDependencies);
		status = b ? MStatus::kSuccess : MStatus::kFailure;
	});
	if (status != MStatus::kSuccess)
		return status;

	MaterialUtils::forwardGeometry(inMesh, outMesh, data);

	MString meshName;
	const MStatus meshNameStatus = MaterialUtils::getMeshName(meshName, plug);
	if (meshNameStatus != MStatus::kSuccess || meshName.length() == 0)
		return meshNameStatus;

	adsk::Data::Stream* inMatStream = MaterialUtils::getMaterialStream(inMesh, data);
	if (inMatStream == nullptr) {
		MaterialUtils::resetMaterial(meshName.asWChar());
		return MStatus::kSuccess;
	}

	const adsk::Data::Structure* materialStructure =
	        adsk::Data::Structure::structureByName(PRT_MATERIAL_STRUCTURE.c_str());
	if (materialStructure == nullptr)
		return MStatus::kFailure;

	MaterialUtils::MaterialCache matCache = MaterialUtils::getMaterialCache();

	MELScriptBuilder scriptBuilder;
	scriptBuilder.declInt(MEL_UNDO_STATE);
	scriptBuilder.getUndoState(MEL_UNDO_STATE);
	scriptBuilder.setUndoState(false);

	declareMaterialStrings(scriptBuilder);

	for (adsk::Data::Handle& inMatStreamHandle : *inMatStream) {
		if (!inMatStreamHandle.hasData())
			continue;

		if (!inMatStreamHandle.usesStructure(*materialStructure))
			continue;

		std::pair<int, int> faceRange;
		if (!MaterialUtils::getFaceRange(inMatStreamHandle, faceRange))
			continue;

		auto createShadingEngine = [this, baseName, &materialStructure, &scriptBuilder,
		                            &inMatStreamHandle](const MaterialInfo& matInfo) {
			const std::wstring shadingEngineBaseName = baseName + L"Sg";
			const std::wstring shaderBaseName = baseName + L"Sh";

			MStatus status;
			const std::wstring shadingEngineName = MaterialUtils::synchronouslyCreateShadingEngine(
			        shadingEngineBaseName, MEL_VARIABLE_SHADING_ENGINE, status);
			MCHECK(status);

			MString shadingEngineNameUuid = mu::getNodeUuid(MString(shadingEngineName.c_str()));
			MCHECK(MaterialUtils::addMaterialInfoMapMetadata(matInfo.getHash(), shadingEngineNameUuid));
			appendToMaterialScriptBuilder(scriptBuilder, matInfo, shaderBaseName, shadingEngineName);
			LOG_DBG << "new shading engine: " << shadingEngineName;

			return shadingEngineNameUuid;
		};

		MaterialInfo matInfo(inMatStreamHandle);
		const MString shadingEngineUuid = getCachedValue(matCache, matInfo.getHash(), createShadingEngine, matInfo);

		MObject shadingEngineNodeObj = mu::getNodeObjFromUuid(shadingEngineUuid, status);

		if (status != MS::kSuccess) {
			const MString newUuid = createShadingEngine(matInfo);
			shadingEngineNodeObj = mu::getNodeObjFromUuid(newUuid, status);
		}

		MFnDependencyNode shadingEngineNode(shadingEngineNodeObj);
		const std::wstring shadingEngineName = shadingEngineNode.name().asWChar();

		scriptBuilder.setsAddFaceRange(shadingEngineName, meshName.asWChar(), faceRange.first, faceRange.second);
		LOG_DBG << "assigned shading engine (" << faceRange.first << ":" << faceRange.second
		        << "): " << shadingEngineName;
	}
	scriptBuilder.setUndoState(MEL_UNDO_STATE);
	return scriptBuilder.execute();
}
