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

#include <chrono>
#include <map>
#include <filesystem>

class ResolveMapCache {
public:
	using KeyType = std::wstring;

	explicit ResolveMapCache(const std::wstring& unpackPath) : mRPKUnpackPath{unpackPath} {}
	ResolveMapCache(const ResolveMapCache&) = delete;
	ResolveMapCache(ResolveMapCache&&) = delete;
	ResolveMapCache& operator=(ResolveMapCache const&) = delete;
	ResolveMapCache& operator=(ResolveMapCache&&) = delete;
	~ResolveMapCache();

	enum class CacheStatus { HIT, MISS };
	using LookupResult = std::pair<ResolveMapSPtr, CacheStatus>;
	LookupResult get(const std::wstring& rpk);

private:
	struct ResolveMapCacheEntry {
		ResolveMapSPtr mResolveMap;
		time_t mTimeStamp;
	};
	using Cache = std::map<KeyType, ResolveMapCacheEntry>;
	Cache mCache;

	const std::filesystem::path mRPKUnpackPath;
};

using ResolveMapCacheUPtr = std::unique_ptr<ResolveMapCache>;