//
// Created by shaegler on 3/3/18.
//

#include "ResolveMapCache.h"
#include "LogHandler.h"


namespace {

const ResolveMapUPtr RESOLVE_MAP_NONE;
const ResolveMapCache::LookupResult LOOKUP_FAILURE = { RESOLVE_MAP_NONE, ResolveMapCache::CacheStatus::MISS };
std::filesystem::file_time_type INVALID_TIMESTAMP;


std::filesystem::file_time_type getFileModificationTime(const std::filesystem::path& p) {
	auto actualPath = p;
	const bool pathExists = std::filesystem::exists(actualPath);
	const bool isRegularFile = std::filesystem::is_regular_file(actualPath);
    if (!actualPath.empty() && pathExists && isRegularFile) {
        auto ftime = std::filesystem::last_write_time(actualPath);
        return std::filesystem::last_write_time(actualPath);
    }
	else
		return INVALID_TIMESTAMP;
}

ResolveMapCache::KeyType createCacheKey(const std::filesystem::path& rpk) {
	return rpk.string(); // TODO: try FS_Reader::splitIndexFileSectionPath for embedded resources
}

struct PathRemover {
	void operator()(std::filesystem::path const* p) {
		if (p && std::filesystem::exists(*p)) {
            std::filesystem::remove(*p);
			LOG_DBG << "Removed file " << *p;
			delete p;
		}
	}
};
using ScopedPath = std::unique_ptr<std::filesystem::path,PathRemover>;

} // namespace


ResolveMapCache::~ResolveMapCache() {
    std::filesystem::remove_all(mRPKUnpackPath);
    LOG_INF << "Removed RPK unpack directory";
}

ResolveMapCache::LookupResult ResolveMapCache::get(const std::filesystem::path& rpk) {
	const auto cacheKey = createCacheKey(rpk);

	const auto timeStamp = getFileModificationTime(rpk);
	LOG_DBG << "rpk: current timestamp: " << std::chrono::duration_cast<std::chrono::nanoseconds>(timeStamp.time_since_epoch()).count() << "ns";

	// verify timestamp
	if (timeStamp == INVALID_TIMESTAMP)
		return LOOKUP_FAILURE;

	CacheStatus cs = CacheStatus::HIT;
	auto it = mCache.find(cacheKey);
	if (it != mCache.end()) {
		LOG_DBG << "rpk: cache timestamp: " << std::chrono::duration_cast<std::chrono::nanoseconds>(it->second.mTimeStamp.time_since_epoch()).count() << "ns";
		if (it->second.mTimeStamp != timeStamp) {
			mCache.erase(it);
			const auto cnt = std::filesystem::remove_all(mRPKUnpackPath / rpk.filename());
			LOG_INF << "RPK change detected, forcing reload and clearing cache for " << rpk << " (removed " << cnt << " files)";
			cs = CacheStatus::MISS;
		}
	}
	else
		cs = CacheStatus::MISS;

	if (cs == CacheStatus::MISS) {
		ScopedPath extractedPath; // if set, will resolve the extracted RPK from HDA
		const auto actualRPK = [&extractedPath](const std::filesystem::path& p) {
				return p;
		}(rpk);

		const auto rpkURI = prtu::toFileURI(actualRPK);

		ResolveMapCacheEntry rmce;
		rmce.mTimeStamp = timeStamp;

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		LOG_DBG << "createResolveMap from " << rpkURI;
		rmce.mResolveMap.reset(prt::createResolveMap(rpkURI.c_str(), mRPKUnpackPath.wstring().c_str(), &status));
		if (status != prt::STATUS_OK)
			return LOOKUP_FAILURE;

		it = mCache.emplace(cacheKey, std::move(rmce)).first;
		LOG_INF << "Upacked RPK " << actualRPK << " to " << mRPKUnpackPath;
	}

	return { it->second.mResolveMap, cs };
}

