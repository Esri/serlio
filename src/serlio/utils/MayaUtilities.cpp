#include "utils/MayaUtilities.h"
#include "utils/MELScriptBuilder.h"

#include "maya/MStringResource.h"
#include "maya/MStringResourceId.h"

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
        {CGA_REFERENCE_KEY.asChar(), "https://doc.arcgis.com/en/cityengine/2021.1/cga/cityengine-cga-introduction.htm"},
        {RPK_MANUAL_KEY.asChar(), "https://doc.arcgis.com/en/cityengine/2021.1/help/help-rule-package.htm"}};

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
	for (int i = 0; i + 1 < result.length(); i += 2) {
		const std::string key = result[i].asChar();
		const std::string value = result[i + 1].asChar();
		keyToUrlMap[key] = value;
	}
	
	return keyToUrlMap;
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
} // namespace mu