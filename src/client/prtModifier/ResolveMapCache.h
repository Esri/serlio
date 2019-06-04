#pragma once

#include "node/Utilities.h"

#include <map>
#include <chrono>


class ResolveMapCache {
public:
	using KeyType = std::wstring;

	explicit ResolveMapCache(const std::wstring unpackPath) : mRPKUnpackPath{unpackPath} { }
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

	const std::wstring mRPKUnpackPath;
};

using ResolveMapCacheUPtr = std::unique_ptr<ResolveMapCache>;