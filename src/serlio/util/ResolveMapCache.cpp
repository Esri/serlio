//
// Created by shaegler on 3/3/18.
//

#include "ResolveMapCache.h"
#include "util/LogHandler.h"
#include "Utilities.h"

#include <mutex>

namespace {

const ResolveMapSPtr RESOLVE_MAP_NONE;
const ResolveMapCache::LookupResult LOOKUP_FAILURE = { RESOLVE_MAP_NONE, ResolveMapCache::CacheStatus::MISS };
std::mutex resolveMapCacheMutex;

} // namespace


ResolveMapCache::~ResolveMapCache() {
	if (mRPKUnpackPath.size()>0)
		prtu::remove_all(mRPKUnpackPath);
	LOG_DBG << "Removed RPK unpack directory";
}

ResolveMapCache::LookupResult ResolveMapCache::get(const std::wstring& rpk) {

	std::lock_guard<std::mutex> lock(resolveMapCacheMutex);

	const time_t timeStamp = prtu::getFileModificationTime(rpk);
	LOG_DBG << "rpk: " << rpk << " current timestamp: " << timeStamp;

	// verify timestamp
	if (timeStamp == -1)
		return LOOKUP_FAILURE;

	CacheStatus cs = CacheStatus::HIT;
	auto it = mCache.find(rpk);
	if (it != mCache.end()) {
		LOG_DBG << "rpk: cache timestamp: " << it->second.mTimeStamp;
		if (it->second.mTimeStamp != timeStamp) {
			mCache.erase(it);
			std::wstring filename = prtu::filename(rpk);

			if (mRPKUnpackPath.size() > 0 && filename.size()>0)
				prtu::remove_all(mRPKUnpackPath + prtu::getDirSeparator<std::wstring>() + prtu::filename(rpk));

			LOG_INF << "RPK change detected, forcing reload and clearing cache for " << rpk;
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
		LOG_DBG << "createResolveMap from " << rpk;
		rmce.mResolveMap.reset(prt::createResolveMap(rpkURI.c_str(), mRPKUnpackPath.c_str(), &status), PRTDestroyer());
		if (status != prt::STATUS_OK)
			return LOOKUP_FAILURE;

		it = mCache.emplace(rpk, std::move(rmce)).first;
		LOG_INF << "Upacked RPK " << rpk << " to " << mRPKUnpackPath;
	}

	return { it->second.mResolveMap, cs };
}

