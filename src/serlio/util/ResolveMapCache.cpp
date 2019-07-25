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

#include "ResolveMapCache.h"
#include "util/LogHandler.h"
#include "Utilities.h"

#include <mutex>


namespace {

constexpr bool DBG = false;

const ResolveMapSPtr RESOLVE_MAP_NONE;
const ResolveMapCache::LookupResult LOOKUP_FAILURE = { RESOLVE_MAP_NONE, ResolveMapCache::CacheStatus::MISS };
std::mutex resolveMapCacheMutex;

} // namespace


ResolveMapCache::~ResolveMapCache() {
	if (!mRPKUnpackPath.empty())
		prtu::remove_all(mRPKUnpackPath);
	if (DBG) LOG_DBG << "Removed RPK unpack directory";
}

ResolveMapCache::LookupResult ResolveMapCache::get(const std::wstring& rpk) {

	std::lock_guard<std::mutex> lock(resolveMapCacheMutex);

	const time_t timeStamp = prtu::getFileModificationTime(rpk);
	if (DBG) LOG_DBG << "rpk: " << rpk << " current timestamp: " << timeStamp;

	// verify timestamp
	if (timeStamp == -1)
		return LOOKUP_FAILURE;

	CacheStatus cs = CacheStatus::HIT;
	auto it = mCache.find(rpk);
	if (it != mCache.end()) {
		if (DBG) LOG_DBG << "rpk: cache timestamp: " << it->second.mTimeStamp;
		if (it->second.mTimeStamp != timeStamp) {
			mCache.erase(it);
			std::wstring filename = prtu::filename(rpk);

			if (!mRPKUnpackPath.empty() && !filename.empty())
				prtu::remove_all(mRPKUnpackPath + prtu::getDirSeparator<std::wstring>() + prtu::filename(rpk));

			if (DBG) LOG_DBG << "RPK change detected, forcing reload and clearing cache for " << rpk;
			cs = CacheStatus::MISS;
		}
	}
	else
		cs = CacheStatus::MISS;

	if (cs == CacheStatus::MISS) {

		const auto rpkURI = prtu::toFileURI(rpk);

		ResolveMapCacheEntry rmce;
		rmce.mTimeStamp = timeStamp;

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		if (DBG) LOG_DBG << "createResolveMap from " << rpk;
		rmce.mResolveMap.reset(prt::createResolveMap(rpkURI.c_str(), mRPKUnpackPath.c_str(), &status), PRTDestroyer());
		if (status != prt::STATUS_OK)
			return LOOKUP_FAILURE;

		it = mCache.emplace(rpk, std::move(rmce)).first;
		if (DBG) LOG_DBG << "Upacked RPK " << rpk << " to " << mRPKUnpackPath;
	}

	return { it->second.mResolveMap, cs };
}

