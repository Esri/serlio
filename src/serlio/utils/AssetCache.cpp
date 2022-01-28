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

#include "AssetCache.h"

#include "utils/LogHandler.h"
#include "utils/MayaUtilities.h"

#include <cassert>
#include <fstream>
#include <functional>
#include <ostream>
#include <string_view>

namespace {
constexpr const wchar_t* MAYA_ASSET_FOLDER = L"assets";
constexpr const wchar_t* SERLIO_ASSET_FOLDER = L"serlio_assets";

bool writeCacheEntry(const std::filesystem::path& assetPath, const uint8_t* buffer, size_t size) noexcept {
	if (std::filesystem::exists(assetPath))
		return true;
	std::ofstream stream(assetPath, std::ofstream::binary | std::ofstream::trunc);
	if (!stream)
		return false;
	stream.write(reinterpret_cast<const char*>(buffer), size);
	if (!stream)
		return false;
	stream.close();
	return true;
}

} // namespace

std::filesystem::path AssetCache::put(const wchar_t* uri, const wchar_t* fileName, const uint8_t* buffer, size_t size) {
	assert(uri != nullptr);
	std::wstring stringUri(uri);

	const std::string_view bufferView(reinterpret_cast<const char*>(buffer), size);
	const size_t hash = std::hash<std::string_view>{}(bufferView);
	const auto key = std::make_pair(stringUri, hash);

	const auto it = mCache.find(key);

	// reuse cached asset if uri and hash match
	if (it != mCache.end()) {
		const std::filesystem::path& assetPath = it->second;
		if (std::filesystem::exists(assetPath)) {
			return assetPath;
		}
	}

	const std::filesystem::path newAssetPath = getCachedPath(fileName, hash);

	if (newAssetPath.empty()) {
		LOG_ERR << "Invalid URI, cannot cache the asset: " << uri;
		return {};
	}

	if (!writeCacheEntry(newAssetPath, buffer, size)) {
		LOG_ERR << "Failed to put asset into cache, skipping asset: " << newAssetPath;
		return {};
	}

	if (it == mCache.end()) {
		mCache.emplace(key, newAssetPath);
	}
	else {
		it->second = newAssetPath;
	}

	return newAssetPath;
}

std::filesystem::path AssetCache::getCachedPath(const wchar_t* fileName, const size_t hash) const {
	// we start with the root folder in the current workspace
	MStatus status;
	std::filesystem::path workspaceDir = mu::getWorkspaceRoot(status);
	MCHECK(status);
	std::filesystem::path assetsDir = workspaceDir.make_preferred() / MAYA_ASSET_FOLDER / SERLIO_ASSET_FOLDER;

	// create dir if it does not exist
	try {
		std::filesystem::create_directories(assetsDir);
	}
	catch (std::exception& e) {
		LOG_ERR << "Error while creating the asset cache directory at " << assetsDir << ": " << e.what();
	}

	// we then get the filename constructed by the encoder from the URI
	assert(fileName != nullptr);
	std::filesystem::path assetFile(fileName);
	std::filesystem::path cachedAssetName = assetFile.stem();
	std::wstring hashString = std::to_wstring(hash);

	// we then append the hash constructed from the texturecontent
	cachedAssetName += L"_" + hashString;

	std::filesystem::path extension = assetFile.extension();
	cachedAssetName += extension;

	const std::filesystem::path cachedAssetPath = assetsDir / cachedAssetName;
	return cachedAssetPath;
}
