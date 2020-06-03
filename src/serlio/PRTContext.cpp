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

#include "PRTContext.h"

#include "utils/LogHandler.h"

#include <mutex>

namespace {

constexpr bool DBG = false;

constexpr const wchar_t* SRL_TMP_PREFIX = L"serlio_";
constexpr const wchar_t* PRT_EXT_SUBDIR = L"ext";
constexpr prt::LogLevel PRT_LOG_LEVEL = prt::LOG_INFO;
constexpr bool ENABLE_LOG_CONSOLE = true;
constexpr bool ENABLE_LOG_FILE = false;

bool verifyMayaEncoder() {
	constexpr const wchar_t* ENC_ID_MAYA = L"MayaEncoder";
	const auto mayaEncOpts = prtu::createValidatedOptions(ENC_ID_MAYA);
	return static_cast<bool>(mayaEncOpts);
}

} // namespace

PRTContext& PRTContext::get() {
	static PRTContext prtCtx;
	return prtCtx;
}

PRTContext::PRTContext(const std::vector<std::wstring>& addExtDirs) : mPluginRootPath(prtu::getPluginRoot()) {
	if (ENABLE_LOG_CONSOLE) {
		theLogHandler = prt::ConsoleLogHandler::create(prt::LogHandler::ALL, prt::LogHandler::ALL_COUNT);
		prt::addLogHandler(theLogHandler);
	}

	if (ENABLE_LOG_FILE) {
		const std::wstring logPath = mPluginRootPath + prtu::getDirSeparator<wchar_t>() + L"serlio.log";
		theFileLogHandler =
		        prt::FileLogHandler::create(prt::LogHandler::ALL, prt::LogHandler::ALL_COUNT, logPath.c_str());
		prt::addLogHandler(theFileLogHandler);
	}

	// Not the best place, but here we are sure the console logger is running and we are before PRT init info
	LOG_INF << "Initializing Serlio Version " << SRL_VERSION << " ...";

	if (DBG)
		LOG_DBG << "initialized prt logger, plugin root path is " << mPluginRootPath;

	std::vector<std::wstring> extensionPaths = {mPluginRootPath + PRT_EXT_SUBDIR};
	extensionPaths.insert(extensionPaths.end(), addExtDirs.begin(), addExtDirs.end());
	if (DBG)
		LOG_DBG << "looking for prt extensions at\n" << extensionPaths;

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	const auto extensionPathPtrs = prtu::toPtrVec(extensionPaths);
	thePRT.reset(prt::init(extensionPathPtrs.data(), extensionPathPtrs.size(), PRT_LOG_LEVEL, &status));

	// early sanity check for maya encoder
	if (!verifyMayaEncoder()) {
		LOG_FTL << "Unable to load Maya encoder extension!";
		status = prt::STATUS_ENCODER_NOT_FOUND;
	}

	if (!thePRT || status != prt::STATUS_OK) {
		LOG_FTL << "Could not initialize PRT: " << prt::getStatusDescription(status);
		thePRT.reset();
	}
	else {
		theCache.reset(prt::CacheObject::create(prt::CacheObject::CACHE_TYPE_DEFAULT));
		mResolveMapCache = std::make_unique<ResolveMapCache>(prtu::getProcessTempDir(SRL_TMP_PREFIX));
	}
}

PRTContext::~PRTContext() {

	// the cache needs to be destructed before PRT, so reset them explicitely in the right order here
	theCache.reset();
	thePRT.reset();

	if (ENABLE_LOG_CONSOLE && (theLogHandler != nullptr)) {
		prt::removeLogHandler(theLogHandler);
		theLogHandler->destroy();
		theLogHandler = nullptr;
	}

	if (ENABLE_LOG_FILE && (theFileLogHandler != nullptr)) {
		prt::removeLogHandler(theFileLogHandler);
		theFileLogHandler->destroy();
		theFileLogHandler = nullptr;
	}
}
