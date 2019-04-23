#pragma once

#include "node/Utilities.h"

#include <filesystem>
#include <map>
#include <chrono>


class ResolveMapCache {
public:
	using KeyType = std::string;

	explicit ResolveMapCache(const std::filesystem::path& unpackPath) : mRPKUnpackPath{unpackPath} { }
	ResolveMapCache(const ResolveMapCache&) = delete;
	ResolveMapCache(ResolveMapCache&&) = delete;
	ResolveMapCache& operator=(ResolveMapCache const&) = delete;
	ResolveMapCache& operator=(ResolveMapCache&&) = delete;
	~ResolveMapCache();

	enum class CacheStatus { HIT, MISS };
	using LookupResult = std::pair<const ResolveMapUPtr&, CacheStatus>;
	LookupResult get(const std::filesystem::path& rpk);

private:
	struct ResolveMapCacheEntry {
		ResolveMapUPtr mResolveMap;
		std::filesystem::file_time_type mTimeStamp;
	};
	using Cache = std::map<KeyType, ResolveMapCacheEntry>;
	Cache mCache;

	const std::filesystem::path mRPKUnpackPath;
};

using ResolveMapCacheUPtr = std::unique_ptr<ResolveMapCache>;