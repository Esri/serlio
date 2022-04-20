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

#include "utils/Utilities.h"
#include "utils/LogHandler.h"

#include "prt/API.h"
#include "prt/StringUtils.h"

#ifdef _WIN32
// workaround for  "combaseapi.h(229): error C2187: syntax error: 'identifier' was unexpected here" when using
// /permissive-
struct IUnknown;
#	include <process.h>
#	include <windows.h>
#	include <shellapi.h>
#else
#	include <cerrno>
#	include <dlfcn.h>
#	include <unistd.h>
#endif

#include <cstring>
#include <cwchar>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

namespace {
const std::wstring MAYA_SEPARATOR = L"_";
const std::wstring MAYA_COMPATIBLE_CHARS = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
const std::wstring DIGIT_CHARS = L"0123456789";

const std::wstring TOO_NEW_CE_VERSION = L"newer than 2021.1";
const std::wstring CGAC_VERSION_STRING = L"CGAC version ";
const std::wstring CE_VERSION_STRING = L"CityEngine version ";

const std::map<std::wstring, std::wstring> cgacToCEVersion = {
        // clang-format off
	{L"1.17", L"2021.1"},
	{L"1.16", L"2021.0"},
	{L"1.15", L"2020.1"},
	{L"1.14", L"2020.0"},
	{L"1.13", L"2019.1"},
	{L"1.12", L"2019.0"},
	{L"1.11", L"2018.1"},
	{L"1.10", L"2018.0"},
	{L"1.9", L"2017.1"},
	{L"1.8", L"2017.0"},
	{L"1.7", L"2016.1"},
	{L"1.6", L"2016.0"},
	{L"1.5", L"2015.0 - 2015.2"},
	{L"1.4", L"2014.1"},
	{L"1.3", L"2014.1"},
	{L"1.2", L"2014.0"},
	{L"1.1", L"2013.1"},
	{L"1.0", L"2013.0"}
        // clang-format on
};

void replaceCGACVersionBetween(std::wstring& errorString, const std::wstring prefix, const std::wstring suffix) {
	size_t versionStartPos = errorString.find(prefix);
	if (versionStartPos != std::wstring::npos)
		versionStartPos += prefix.length();

	const size_t versionEndPos = errorString.find(suffix, versionStartPos);

	if ((versionStartPos == std::wstring::npos) || (versionEndPos == std::wstring::npos))
		return;

	const size_t versionLength = versionEndPos - versionStartPos;
	const std::wstring cgacV1 = errorString.substr(versionStartPos, versionLength);

	std::wstring CEVersion;
	const auto it = cgacToCEVersion.find(cgacV1);
	if (it != cgacToCEVersion.end()) {
		CEVersion = it->second;
	}
	else {
		CEVersion = TOO_NEW_CE_VERSION;
	}
	errorString.replace(versionStartPos, versionLength, CEVersion);
}
} // namespace

namespace prtu {

// plugin root = location of serlio shared library
std::filesystem::path getPluginRoot() {
	std::filesystem::path rootPath;
#ifdef _WIN32
	char dllPath[_MAX_PATH];
	HMODULE hModule = 0;

	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                  (LPCSTR)getPluginRoot, &hModule);
	const DWORD res = ::GetModuleFileName(hModule, dllPath, _MAX_PATH);
	if (res == 0) {
		// TODO DWORD e = ::GetLastError();
		throw std::runtime_error("failed to get plugin location");
	}

	rootPath = std::filesystem::path(dllPath).parent_path();
#else
	Dl_info dl_info;
	dladdr((const void*)getPluginRoot, &dl_info);
	const std::string tmp(dl_info.dli_fname);
	rootPath = std::filesystem::path(tmp).parent_path();
#endif

	return rootPath;
}

int fromHex(wchar_t c) {
	// clang-format off
	switch (c) {
		case '0': return 0;	case '1': return 1;	case '2': return 2;	case '3': return 3;	case '4': return 4;
		case '5': return 5;	case '6': return 6;	case '7': return 7;	case '8': return 8;	case '9': return 9;
		case 'a': case 'A': return 0xa;
		case 'b': case 'B': return 0xb;
		case 'c': case 'C': return 0xc;
		case 'd': case 'D': return 0xd;
		case 'e': case 'E': return 0xe;
		case 'f': case 'F': return 0xf;
		default:
			return 0;
	}
	// clang-format on
}

const wchar_t HEXTAB[] = L"0123456789ABCDEF";

wchar_t toHex(int i) {
	return HEXTAB[i & 0xF];
}

Color parseColor(const wchar_t* colorString) {
	Color c{0.0, 0.0, 0.0};
	if (std::wcslen(colorString) >= 7 && colorString[0] == '#') {
		c[0] = static_cast<float>((prtu::fromHex(colorString[1]) << 4) + prtu::fromHex(colorString[2])) / 255.0f;
		c[1] = static_cast<float>((prtu::fromHex(colorString[3]) << 4) + prtu::fromHex(colorString[4])) / 255.0f;
		c[2] = static_cast<float>((prtu::fromHex(colorString[5]) << 4) + prtu::fromHex(colorString[6])) / 255.0f;
	}
	return c;
}

std::wstring getColorString(const Color& c) {
	std::wstringstream colStr;
	colStr << L'#';
	colStr << toHex(((int)(c[0] * 255.0f)) >> 4);
	colStr << toHex((int)(c[0] * 255.0f));
	colStr << toHex(((int)(c[1] * 255.0f)) >> 4);
	colStr << toHex((int)(c[1] * 255));
	colStr << toHex(((int)(c[2] * 255.0f)) >> 4);
	colStr << toHex((int)(c[2] * 255.0f));
	return colStr.str();
}

template <typename CO, typename CI, typename AF>
std::basic_string<CO> stringConversionWrapper(AF apiFunc, const std::basic_string<CI>& inputString) {
	if (inputString.empty())
		return std::basic_string<CO>();

	std::vector<CO> temp(2 * inputString.size(), 0);
	size_t size = temp.size();
	prt::Status status = prt::STATUS_OK;
	apiFunc(inputString.c_str(), temp.data(), &size, &status);
	if (status != prt::STATUS_OK)
		throw std::runtime_error(prt::getStatusDescription(status));
	if (size > temp.size()) {
		temp.resize(size);
		apiFunc(inputString.c_str(), temp.data(), &size, &status);
		if (status != prt::STATUS_OK)
			throw std::runtime_error(prt::getStatusDescription(status));
	}
	return std::basic_string<CO>(temp.data());
}

std::string toOSNarrowFromUTF16(const std::wstring& u16String) {
	return stringConversionWrapper<char, wchar_t>(prt::StringUtils::toOSNarrowFromUTF16, u16String);
}

std::wstring toUTF16FromOSNarrow(const std::string& osString) {
	return stringConversionWrapper<wchar_t, char>(prt::StringUtils::toUTF16FromOSNarrow, osString);
}

std::wstring toUTF16FromUTF8(const std::string& u8String) {
	return stringConversionWrapper<wchar_t, char>(prt::StringUtils::toUTF16FromUTF8, u8String);
}

std::string toUTF8FromUTF16(const std::wstring& u16String) {
	return stringConversionWrapper<char, wchar_t>(prt::StringUtils::toUTF8FromUTF16, u16String);
}

std::string percentEncode(const std::string& utf8String) {
	return stringConversionWrapper<char, char>(prt::StringUtils::percentEncode, utf8String);
}

std::wstring toFileURI(const std::wstring& p) {
#ifdef _WIN32
	static const std::wstring schema = L"file:/";
#else
	static const std::wstring schema = L"file:";
#endif
	std::string utf8Path = prtu::toUTF8FromUTF16(p);
	std::string pecString = percentEncode(utf8Path);
	std::wstring u16String = toUTF16FromUTF8(pecString);
	return schema + u16String;
}

time_t getFileModificationTime(const std::wstring& p) {
	std::wstring pn = std::filesystem::path(p).make_preferred().wstring();

#ifdef _WIN32
	struct _stat st;
	int ierr = _wstat(pn.c_str(), &st);
#else
	struct stat st;
	int ierr = stat(prtu::toOSNarrowFromUTF16(pn).c_str(), &st);
#endif

	if (ierr == 0) {
		return st.st_mtime;
	}
	return -1;
}

std::string objectToXML(prt::Object const* obj) {
	if (obj == nullptr)
		throw std::invalid_argument("object pointer is not valid");
	constexpr size_t SIZE = 4096;
	size_t actualSize = SIZE;
	std::vector<char> buffer(SIZE, ' ');
	obj->toXML(buffer.data(), &actualSize);
	buffer.resize(actualSize);
	if (actualSize > SIZE)
		obj->toXML(buffer.data(), &actualSize);
	return std::string(buffer.data());
}

AttributeMapUPtr createValidatedOptions(const wchar_t* encID, const prt::AttributeMap* unvalidatedOptions) {
	const EncoderInfoUPtr encInfo(prt::createEncoderInfo(encID));
	if (!encInfo)
		return {};
	const prt::AttributeMap* validatedOptions = nullptr;
	const prt::AttributeMap* optionStates = nullptr;
	const prt::Status s =
	        encInfo->createValidatedOptionsAndStates(unvalidatedOptions, &validatedOptions, &optionStates);
	if (optionStates != nullptr)
		optionStates->destroy(); // we don't need that atm
	if (s != prt::STATUS_OK)
		return {};
	return AttributeMapUPtr(validatedOptions);
}

void replaceCGACWithCEVersion(std::wstring& errorString) {
	// a typical CGAC version error string looks like:
	// Potentially unsupported CGAC version X.YY : major number smaller than current (A.BB)
	replaceAllSubstrings(errorString, CGAC_VERSION_STRING, CE_VERSION_STRING);

	replaceCGACVersionBetween(errorString, CE_VERSION_STRING, L" ");
	replaceCGACVersionBetween(errorString, L"(", L")");
}

std::wstring getDuplicateCountSuffix(const std::wstring& name, std::map<std::wstring, int>& duplicateCountMap) {
	auto [iterator, isFirstEntry] = duplicateCountMap.try_emplace(name, 0);
	if (!isFirstEntry)
		iterator->second++;
	return MAYA_SEPARATOR + std::to_wstring(iterator->second);
}

std::wstring cleanNameForMaya(const std::wstring& name) {
	std::wstring r = name;
	replaceAllNotOf(r, MAYA_COMPATIBLE_CHARS);

	if (!r.empty() && (DIGIT_CHARS.find(r.front()) != std::wstring::npos))
		return MAYA_SEPARATOR + r;

	return r;
}
} // namespace prtu
