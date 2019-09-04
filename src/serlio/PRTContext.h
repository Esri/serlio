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

#include "serlioPlugin.h"

#include "util/ResolveMapCache.h"
#include "util/Utilities.h"

#include <memory>
#include <vector>


struct SRL_TEST_EXPORTS_API PRTContext final {
	explicit PRTContext(const std::vector<std::wstring>& addExtDirs = {});
	PRTContext(const PRTContext&) = delete;
	PRTContext(PRTContext&&) = delete;
	PRTContext& operator=(PRTContext&) = delete;
	PRTContext& operator=(PRTContext&&) = delete;
	~PRTContext();

	ResolveMapSPtr getResolveMap(const std::wstring& rpk);
	bool isAlive() const { return thePRT.operator bool(); }

	const std::wstring      mPluginRootPath; // the path where serlio dso resides
	ObjectUPtr              thePRT;
	CacheObjectUPtr         theCache;
	prt::ConsoleLogHandler* theLogHandler = nullptr;
	prt::FileLogHandler*    theFileLogHandler = nullptr;
	ResolveMapCacheUPtr     mResolveMapCache;
};

using PRTContextUPtr = std::unique_ptr<PRTContext>;
