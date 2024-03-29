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

#pragma once

#include "serlioPlugin.h"

#include "utils/AssetCache.h"
#include "utils/LogHandler.h"
#include "utils/ResolveMapCache.h"
#include "utils/Utilities.h"

#include <memory>
#include <vector>

class PRTContext;
using PRTContextUPtr = std::unique_ptr<PRTContext>;

class SRL_TEST_EXPORTS_API PRTContext final {
public:
	static PRTContext& get();

	explicit PRTContext(const std::vector<std::wstring>& addExtDirs = {});
	PRTContext(const PRTContext&) = delete;
	PRTContext(PRTContext&&) = delete;
	PRTContext& operator=(PRTContext&) = delete;
	PRTContext& operator=(PRTContext&&) = delete;
	~PRTContext();

	bool isAlive() const;

	const std::filesystem::path mPluginRootPath; // the path where serlio dso resides
	AssetCache mAssetCache;
	ObjectUPtr mPRTHandle;
	CacheObjectUPtr mPRTCache;
	logging::LogHandlerUPtr mLogHandler;
	prt::FileLogHandler* mFileLogHandler = nullptr;
	ResolveMapCacheUPtr mResolveMapCache;
};
