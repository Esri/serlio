/**
 * Serlio - Esri CityEngine Plugin for Autodesk Maya
 *
 * See https://github.com/esri/serlio for build and usage instructions.
 *
 * Copyright (c) 2012-2022 Esri R&D Center Zurich
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

#include "utils/MELScriptBuilder.h"
#include "utils/MItDependencyNodesWrapper.h"
#include "utils/MayaUtilities.h"

#include "maya/MItDependencyNodes.h"
#include "maya/MSelectionList.h"
#include "maya/MStringResource.h"
#include "maya/MStringResourceId.h"
#include "maya/MUuid.h"

#include <map>
#include <memory>
#include <string>

namespace {
constexpr const wchar_t KEY_URL_SEPARATOR = L'=';
const MString INDIRECTION_URL = L"https://raw.githubusercontent.com/Esri/serlio/data/urls.json";
const MString SERLIO_HOME_KEY = "SERLIO_HOME";
const MString CGA_REFERENCE_KEY = "CGA_REFERENCE";
const MString RPK_MANUAL_KEY = "RPK_MANUAL";

const std::map<std::string, std::string> fallbackKeyToUrlMap = {
        {SERLIO_HOME_KEY.asChar(), "https://esri.github.io/cityengine/serlio"},
        {CGA_REFERENCE_KEY.asChar(), "https://doc.arcgis.com/en/cityengine/latest/cga/cityengine-cga-introduction.htm"},
        {RPK_MANUAL_KEY.asChar(), "https://doc.arcgis.com/en/cityengine/latest/help/help-rule-package.htm"}};

std::map<std::string, std::string> getKeyToUrlMap() {
	MString pyCmd1;
	pyCmd1 += "def getIndirectionStrings():\n";
	pyCmd1 += " from six.moves import urllib\n";
	pyCmd1 += " import json\n";
	// Download indirection links
	pyCmd1 += " url = \"" + INDIRECTION_URL + "\"\n";
	pyCmd1 += " try:\n";
	pyCmd1 += "  response = urllib.request.urlopen(url, timeout=3)\n";
	pyCmd1 += "  jsonString = response.read()\n";
	// Parse json into array
	pyCmd1 += "  jsonObject = json.loads(jsonString)\n";
	pyCmd1 += "  serlioHomeKey = \"" + SERLIO_HOME_KEY + "\"\n";
	pyCmd1 += "  cgaReferenceKey = \"" + CGA_REFERENCE_KEY + "\"\n";
	pyCmd1 += "  rpkManualKey = \"" + RPK_MANUAL_KEY + "\"\n";
	pyCmd1 += "  serlioVersionKey = \"" SRL_VERSION_MAJOR "." SRL_VERSION_MINOR "\"\n";
	pyCmd1 += "  serlioHome = jsonObject[serlioVersionKey][serlioHomeKey]\n";
	pyCmd1 += "  cgaReference = jsonObject[serlioVersionKey][cgaReferenceKey]\n";
	pyCmd1 += "  rpkManual = jsonObject[serlioVersionKey][rpkManualKey]\n";
	pyCmd1 += "  return [serlioHomeKey, serlioHome, cgaReferenceKey, cgaReference, rpkManualKey, rpkManual]\n";
	pyCmd1 += " except:\n";
	pyCmd1 += "  return []";

	MStatus status = MGlobal::executePythonCommand(pyCmd1);
	if (status != MStatus::kSuccess)
		return {};

	MString pyCmd2 = "getIndirectionStrings()";
	MStringArray result;
	status = MGlobal::executePythonCommand(pyCmd2, result);
	if ((status != MStatus::kSuccess) || result.length() < 6)
		return {};

	std::map<std::string, std::string> keyToUrlMap;
	for (uint32_t i = 0; i + 1 < result.length(); i += 2) {
		const std::string key = result[i].asChar();
		const std::string value = result[i + 1].asChar();
		keyToUrlMap[key] = value;
	}

	return keyToUrlMap;
}

MObject findNamedObject(const MString& name, MFn::Type fnType) {
	MStatus status;
	MItDependencyNodes nodeIt(fnType, &status);
	MCHECK(status);

	for (const auto& nodeObj : MItDependencyNodesWrapper(nodeIt)) {
		MFnDependencyNode node(nodeObj);
		if (node.name() == name)
			return nodeObj;
	}

	return MObject::kNullObj;
}
} // namespace

namespace mu {

int32_t computeSeed(const MFloatPoint& p) {
	float x = p[0];
	float z = p[2];
	int32_t seed = *(int32_t*)(&x);
	seed ^= *(int32_t*)(&z);
	seed %= 714025;
	return seed;
}

int32_t computeSeed(const MFloatPointArray& vertices) {
	MFloatPoint a(0.0, 0.0, 0.0);
	for (unsigned int vi = 0; vi < vertices.length(); vi++) {
		a += vertices[vi];
	}
	a = a / static_cast<float>(vertices.length());
	return computeSeed(a);
}

void statusCheck(const MStatus& status, const char* file, int line) {
	if (MS::kSuccess != status) {
		LOG_ERR << "maya status error at " << file << ":" << line << ": " << status.errorString().asChar() << " (code "
		        << status.statusCode() << ")";
	}
}

std::filesystem::path getWorkspaceRoot(MStatus& status) {
	MELScriptBuilder scriptBuilder;
	scriptBuilder.getWorkspaceDir();

	std::wstring output;
	status = scriptBuilder.executeSync(output);

	if (status == MS::kSuccess) {
		return std::filesystem::path(output).make_preferred();
	}
	else {
		return {};
	}
}

MStatus registerMStringResources() {
	std::map<std::string, std::string> keyToUrlMap = getKeyToUrlMap();

	for (const auto& [key, url] : fallbackKeyToUrlMap) {
		auto it = keyToUrlMap.find(key);
		if (it == keyToUrlMap.end()) {
			const MStringResourceId SerlioHomeURL(SRL_PROJECT_NAME, key.c_str(), url.c_str());
			MStringResource::registerString(SerlioHomeURL);
		}
		else {
			const MStringResourceId SerlioHomeURL(SRL_PROJECT_NAME, key.c_str(), it->second.c_str());
			MStringResource::registerString(SerlioHomeURL);
		}
	}

	return MS::kSuccess;
}

MStatus setEnumOptions(const MObject& node, MFnEnumAttribute& enumAttr, const std::vector<std::wstring>& enumOptions,
                       const std::optional<std::wstring>& customDefaultOption) {
	MStatus stat;
	const MFnDependencyNode fNode(node, &stat);
	if (stat != MStatus::kSuccess)
		return stat;

	const MELVariable melSerlioNode(L"serlioNode");

	MELScriptBuilder scriptBuilder;
	const std::wstring nodeName = fNode.name().asWChar();
	const std::wstring attrName = enumAttr.name().asWChar();
	scriptBuilder.setVar(melSerlioNode, MELStringLiteral(nodeName));
	scriptBuilder.setAttrEnumOptions(melSerlioNode, attrName, enumOptions, customDefaultOption);

	return scriptBuilder.execute();
}

MUuid getNodeUuid(const MString& nodeName) {
	MObject shadingEngineObj = findNamedObject(nodeName, MFn::kShadingEngine);
	MFnDependencyNode shadingEngine(shadingEngineObj);
	return shadingEngine.uuid();
}

MObject getNodeObjFromUuid(const MUuid& nodeUuid, MStatus& status) {
	MSelectionList selList;
	status = selList.add(MUuid(nodeUuid));
	MObject shadingEngineNodeObj;

	if (status != MS::kSuccess)
		return shadingEngineNodeObj;

	status = selList.getDependNode(0, shadingEngineNodeObj);
	return shadingEngineNodeObj;
}

} // namespace mu

bool operator==(const MStringArray& lhs, const MStringArray& rhs) {
	if (lhs.length() != rhs.length())
		return false;
	for (uint32_t index = 0; index < lhs.length(); index++) {
		if (lhs[index] != rhs[index])
			return false;
	}
	return true;
}

bool operator!=(const MStringArray& lhs, const MStringArray& rhs) {
	return !(lhs == rhs);
}
