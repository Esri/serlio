#include "utils/MayaUtilities.h"
#include "utils/MELScriptBuilder.h"

#include "maya/MStringResource.h"
#include "maya/MStringResourceId.h"

#include <map>
#include <memory>
#include <string>

namespace {
constexpr const wchar_t KEY_URL_SEPARATOR = L'=';
const std::wstring INDIRECTION_URL = L"https://raw.githubusercontent.com/Esri/serlio/indirection_urls/urls.txt";

const std::map<std::string, std::string> fallbackKeyToUrlMap = {
        {"SERLIO_HOME", "https://esri.github.io/cityengine/serlio"},
        {"CGA_REFERENCE", "https://doc.arcgis.com/en/cityengine/latest/cga/cityengine-cga-introduction.htm"},
        {"RPK_MANUAL", "https://doc.arcgis.com/en/cityengine/latest/help/help-rule-package.htm"}};

std::map<std::string, std::string> getKeyToUrlMap() {
	MStatus status;
	const std::wstring& indirectionWString = mu::getStringFromURL(INDIRECTION_URL, status);
	if (status != MStatus::kSuccess)
		return {};

	const std::string& indirectionString = prtu::toUTF8FromUTF16(indirectionWString);
	std::map<std::string, std::string> keyToUrlMap;

	std::istringstream wiss(indirectionString);
	for (std::string line; std::getline(wiss, line);) {
		size_t idx = line.find_first_of(KEY_URL_SEPARATOR);
		if ((idx == std::string::npos) || ((idx + 1) >= line.length()))
			continue;

		const std::string key = line.substr(0, idx);
		const std::string url = line.substr(idx + 1);

		keyToUrlMap[key] = url;
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

std::wstring getStringFromURL(const std::wstring& url, MStatus& status) {
	MELScriptBuilder scriptBuilder;
	scriptBuilder.getStringFromURL(url);

	std::wstring output;
	status = scriptBuilder.executeSync(output);

	if (status == MS::kSuccess) {
		return output;
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