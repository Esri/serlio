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

#include <filesystem>
#include <string>
#include <unordered_map>

class AssetCache {
public:
	std::filesystem::path put(const wchar_t* uri, const wchar_t* fileName, const uint8_t* buffer, size_t size);

private:
	std::filesystem::path getCachedPath(const wchar_t* fileName, const size_t hash) const;

	std::unordered_map<std::pair<std::wstring, size_t>, std::filesystem::path, prtu::pair_hash> mCache;
};
