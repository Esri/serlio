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

#include "util/Utilities.h"

#include "prt/StringUtils.h"
#include "prt/API.h"

#ifdef _WIN32
// workaround for  "combaseapi.h(229): error C2187: syntax error: 'identifier' was unexpected here" when using /permissive-
struct IUnknown;
#   include <windows.h>
//#   include <tchar.h>
//#   include <shellapi.h>
#else
#   include <ftw.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <cwchar>
#include <memory>
#include <sstream>
#include <string>
#include <stdexcept>


namespace prtu {

	const std::wstring filename(const std::wstring& path) {
		size_t pos = path.find_last_of(L'/');
		if (pos != std::string::npos)
		{
			return path.substr(pos+1);
		}
		else return path;
	}


	template<> char getDirSeparator() {
#ifdef _WIN32
		static const char SEPARATOR = '\\';
#else
		static const char SEPARATOR = '/';
#endif
		return SEPARATOR;
	}


	template<> wchar_t getDirSeparator() {
#ifdef _WIN32
		static const wchar_t SEPARATOR = L'\\';
#else
		static const wchar_t SEPARATOR = L'/';
#endif
		return SEPARATOR;
	}

	template<> std::string getDirSeparator() {
		return std::string(1, getDirSeparator<char>());
	}

	template<> std::wstring getDirSeparator() {
		return std::wstring(1, getDirSeparator<wchar_t>());
	}

#if DO_DBG == 1

	void dbg(const char* fmt, ...) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
		printf("\n");
		fflush(0);
	}

	void wdbg(const wchar_t* fmt, ...) {
		va_list args;

		va_start(args, fmt);
		vwprintf(fmt, args);
		va_end(args);
		wprintf(L"\n");
		fflush(0);
	}

#else
	void dbg(const char*, ...) { }
	void wdbg(const wchar_t*, ...) { }
#endif

	int fromHex(wchar_t c) {
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
	}

	const wchar_t HEXTAB[] = L"0123456789ABCDEF";

	wchar_t toHex(int i) {
		return HEXTAB[i & 0xF];
	}

	std:: wstring toHex(double r, double g, double b) {
		std::wstringstream color;
		color << toHex(((int)(r * 255)) >> 4);
		color << toHex((int)(r * 255));
		color << toHex(((int)(g * 255)) >> 4);
		color << toHex((int)(g * 255));
		color << toHex(((int)(b * 255)) >> 4);
		color << toHex((int)(b * 255));
		return color.str();
	}

	template<typename CO, typename CI, typename AF>
	std::basic_string<CO> stringConversionWrapper(AF apiFunc, const std::basic_string<CI>& inputString) {
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

	void remove_all(const std::wstring& path) {
#ifdef _WIN32
		std::wstring pc = path;
		std::replace(pc.begin(), pc.end(), L'/', L'\\');
		const wchar_t* lpszDir = pc.c_str();

		size_t len = wcslen(lpszDir);
		wchar_t *pszFrom = new wchar_t[len + 2];
		wcscpy_s(pszFrom, len + 2, lpszDir);
		pszFrom[len] = 0;
		pszFrom[len + 1] = 0;

		SHFILEOPSTRUCTW fileop;
		fileop.hwnd = NULL;    // no status display
		fileop.wFunc = FO_DELETE;  // delete operation
		fileop.pFrom = pszFrom;  // source file name as double null terminated string
		fileop.pTo = NULL;    // no destination needed
		fileop.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;  // do not prompt the user
		fileop.fAnyOperationsAborted = FALSE;
		fileop.lpszProgressTitle = NULL;
		fileop.hNameMappings = NULL;

		int ret = SHFileOperationW(&fileop);
		delete[] pszFrom;
#else
		system((std::string("rm -rf ")+ toOSNarrowFromUTF16(path)).c_str());
#endif

	}

	std::wstring temp_directory_path() {
#ifdef _WIN32
		DWORD dwRetVal = 0;
		wchar_t lpTempPathBuffer[MAX_PATH];

		dwRetVal = GetTempPathW(MAX_PATH,
			lpTempPathBuffer);
		if (dwRetVal > MAX_PATH || (dwRetVal == 0))
		{
			return L".\tmp";
		}
		else {
			return std::wstring(lpTempPathBuffer);
		}

#else

		char const *folder = getenv("TMPDIR");
		if (folder == nullptr) {
			folder = getenv("TMP");
			if (folder == nullptr)
			{
				folder = getenv("TEMP");
				if (folder == nullptr)
				{
					folder = getenv("TEMPDIR");
					if (folder == nullptr)
						folder = "/tmp";
				}
			}
		}

		return toUTF16FromOSNarrow(std::string(folder));
#endif

	}

	time_t getFileModificationTime(const std::wstring& p) {
		std::wstring pn = p;

#ifdef _WIN32
		std::replace(pn.begin(), pn.end(), L'/', L'\\');
#endif

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
} // namespace prtu
