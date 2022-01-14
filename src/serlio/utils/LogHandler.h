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

#pragma once

#include "utils/Utilities.h"

#include "prt/API.h"
#include "prt/LogHandler.h"

#include <iostream>
#include <iterator>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace logging {

struct Logger {};
// log through the prt logger
template <prt::LogLevel L>
struct PRTLogger : Logger {
	PRTLogger() : Logger() {}
	virtual ~PRTLogger() {
		prt::log(wstr.str().c_str(), L);
	}
	PRTLogger<L>& operator<<(std::wostream& (*x)(std::wostream&)) {
		wstr << x;
		return *this;
	}
	PRTLogger<L>& operator<<(const std::string& x) {
		wstr << prtu::toUTF16FromOSNarrow(x);
		return *this;
	}
	template <typename T>
	PRTLogger<L>& operator<<(const std::vector<T>& v) {
		wstr << L"[ ";
		for (const T& x : v) {
			wstr << x << L" ";
		}
		wstr << L"]";
		return *this;
	}
	template <typename T>
	PRTLogger<L>& operator<<(const T& x) {
		wstr << x;
		return *this;
	}
	std::wostringstream wstr;
};

class LogHandler : public prt::LogHandler {
public:
	void handleLogEvent(const wchar_t* msg, prt::LogLevel) override {
		std::cout << prtu::toOSNarrowFromUTF16(msg) << std::endl;
	}

	const prt::LogLevel* getLevels(size_t* count) override {
		*count = prt::LogHandler::ALL_COUNT;
		return prt::LogHandler::ALL;
	}

	void getFormat(bool* dateTime, bool* level) override {
		*dateTime = true;
		*level = true;
	}
};

using LogHandlerUPtr = std::unique_ptr<LogHandler>;

} // namespace logging

// switch logger here
template <prt::LogLevel L>
using LT = logging::PRTLogger<L>;

using _LOG_DBG = LT<prt::LOG_DEBUG>;
using _LOG_INF = LT<prt::LOG_INFO>;
using _LOG_WRN = LT<prt::LOG_WARNING>;
using _LOG_ERR = LT<prt::LOG_ERROR>;
using _LOG_FTL = LT<prt::LOG_FATAL>;

// convenience shortcuts in global namespace
#define LOG_DBG _LOG_DBG() << __FUNCTION__ << ": "
#define LOG_INF _LOG_INF()
#define LOG_WRN _LOG_WRN()
#define LOG_ERR _LOG_ERR()
#define LOG_FTL _LOG_FTL()
