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

#include "serlioPlugin.h"

#include "prt/API.h"
#include "prt/AttributeMap.h"
#include "prt/EncoderInfo.h"
#include "prt/InitialShape.h"
#include "prt/Object.h"
#include "prt/OcclusionSet.h"
#include "prt/RuleFileInfo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

// PRT version >= VERSION_MAJOR.VERSION_MINOR
#define PRT_VERSION_GTE(VERSION_MAJOR, VERSION_MINOR)                                                                  \
	((PRT_VERSION_MAJOR >= (VERSION_MAJOR)) &&                                                                         \
	 ((PRT_VERSION_MAJOR > (VERSION_MAJOR)) || (PRT_VERSION_MINOR >= (VERSION_MINOR))))
// PRT version <= VERSION_MAJOR.VERSION_MINOR
#define PRT_VERSION_LTE(VERSION_MAJOR, VERSION_MINOR)                                                                  \
	((PRT_VERSION_MAJOR <= (VERSION_MAJOR)) &&                                                                         \
	 ((PRT_VERSION_MAJOR < (VERSION_MAJOR)) || (PRT_VERSION_MINOR <= (VERSION_MINOR))))

#if defined(_MSC_VER) && (_MSC_VER <= 1700)
#	include <cfloat>
#endif

struct PRTDestroyer {
	void operator()(prt::Object const* p) {
		if (p != nullptr)
			p->destroy();
	}
};

using ObjectUPtr = std::unique_ptr<const prt::Object, PRTDestroyer>;
using InitialShapeNOPtrVector = std::vector<const prt::InitialShape*>;
using AttributeMapNOPtrVector = std::vector<const prt::AttributeMap*>;
using CacheObjectUPtr = std::unique_ptr<prt::CacheObject, PRTDestroyer>;
using AttributeMapUPtr = std::unique_ptr<const prt::AttributeMap, PRTDestroyer>;
using AttributeMapVector = std::vector<AttributeMapUPtr>;
using AttributeMapBuilderUPtr = std::unique_ptr<prt::AttributeMapBuilder, PRTDestroyer>;
using AttributeMapBuilderVector = std::vector<AttributeMapBuilderUPtr>;
using InitialShapeUPtr = std::unique_ptr<const prt::InitialShape, PRTDestroyer>;
using InitialShapeBuilderUPtr = std::unique_ptr<prt::InitialShapeBuilder, PRTDestroyer>;
using InitialShapeBuilderVector = std::vector<InitialShapeBuilderUPtr>;
using ResolveMapBuilderUPtr = std::unique_ptr<prt::ResolveMapBuilder, PRTDestroyer>;
using RuleFileInfoUPtr = std::unique_ptr<const prt::RuleFileInfo, PRTDestroyer>;
using EncoderInfoUPtr = std::unique_ptr<const prt::EncoderInfo, PRTDestroyer>;
using OcclusionSetUPtr = std::unique_ptr<prt::OcclusionSet, PRTDestroyer>;
using ResolveMapSPtr = std::shared_ptr<const prt::ResolveMap>;

namespace prtu {

std::wstring getPluginRoot();

template <typename C>
std::vector<const C*> toPtrVec(const std::vector<std::basic_string<C>>& sv) {
	std::vector<const C*> pv(sv.size());
	std::transform(sv.begin(), sv.end(), pv.begin(), [](const auto& s) { return s.c_str(); });
	return pv;
}

template <typename C, typename D>
std::vector<const C*> toPtrVec(const std::vector<std::unique_ptr<C, D>>& sv) {
	std::vector<const C*> pv(sv.size());
	std::transform(sv.begin(), sv.end(), pv.begin(), [](const std::unique_ptr<C, D>& s) { return s.get(); });
	return pv;
}

// poor mans std::filesystem - we don't want boost or c++17 dependency right now
SRL_TEST_EXPORTS_API std::wstring filename(const std::wstring& path);
time_t getFileModificationTime(const std::wstring& p);
std::wstring temp_directory_path();
std::wstring getProcessTempDir(const std::wstring& prefix);
std::wstring toGenericPath(const std::wstring& osPath);

template <typename C>
C getDirSeparator();
template <>
char getDirSeparator();
template <>
wchar_t getDirSeparator();

int fromHex(wchar_t c);
wchar_t toHex(int i);

inline bool isnan(double d) {
#if defined(_MSC_VER) && (_MSC_VER <= 1700)
	return ::isnan(d);
#else
	return (std::isnan(d) != 0);
#endif
}

using Color = std::array<float, 3>;
Color parseColor(const wchar_t* colorString);
std::wstring getColorString(const Color& c);

SRL_TEST_EXPORTS_API std::string toOSNarrowFromUTF16(const std::wstring& u16String);
SRL_TEST_EXPORTS_API std::wstring toUTF16FromOSNarrow(const std::string& osString);
SRL_TEST_EXPORTS_API std::wstring toUTF16FromUTF8(const std::string& u8String);
SRL_TEST_EXPORTS_API std::string toUTF8FromUTF16(const std::wstring& u16String);

SRL_TEST_EXPORTS_API std::wstring toFileURI(const std::wstring& p);
std::string percentEncode(const std::string& utf8String);

std::string objectToXML(prt::Object const* obj);
template <typename T>
std::string objectToXML(const std::unique_ptr<T, PRTDestroyer>& ptr) {
	return objectToXML(ptr.get());
}
template <typename T>
std::string objectToXML(std::unique_ptr<T, PRTDestroyer>& ptr) {
	return objectToXML(ptr.get());
}

AttributeMapUPtr createValidatedOptions(const wchar_t* encID, const prt::AttributeMap* unvalidatedOptions = nullptr);

inline std::wstring getRuleFileEntry(ResolveMapSPtr resolveMap) {
	const std::wstring sCGB(L".cgb");

	size_t nKeys;
	wchar_t const* const* keys = resolveMap->getKeys(&nKeys);
	for (size_t k = 0; k < nKeys; k++) {
		const std::wstring key(keys[k]);
		if (std::equal(sCGB.rbegin(), sCGB.rend(), key.rbegin()))
			return key;
	}

	return {};
}

constexpr const wchar_t* ANNOT_START_RULE = L"@StartRule";

inline std::wstring detectStartRule(const RuleFileInfoUPtr& ruleFileInfo) {
	for (size_t r = 0; r < ruleFileInfo->getNumRules(); r++) {
		const auto* rule = ruleFileInfo->getRule(r);

		// start rules must not have any parameters
		if (rule->getNumParameters() > 0)
			continue;

		for (size_t a = 0; a < rule->getNumAnnotations(); a++) {
			if (std::wcscmp(rule->getAnnotation(a)->getName(), ANNOT_START_RULE) == 0) {
				return rule->getName();
			}
		}
	}
	return {};
}

constexpr const wchar_t STYLE_DELIMITER = L'$';
constexpr const wchar_t IMPORT_DELIMITER = L'.';

SRL_TEST_EXPORTS_API inline std::wstring getStyle(const std::wstring& fqRuleName) {
	const auto sepPos = fqRuleName.find(STYLE_DELIMITER);
	if (sepPos == std::wstring::npos || sepPos == 0)
		return {};
	return fqRuleName.substr(0, sepPos);
}

SRL_TEST_EXPORTS_API inline std::wstring removePrefix(const std::wstring& fqRuleName, wchar_t delim) {
	const auto sepPos = fqRuleName.find(delim);
	if (sepPos == std::wstring::npos)
		return fqRuleName;
	if (sepPos == fqRuleName.length() - 1)
		return {};
	if (fqRuleName.length() <= 1)
		return {};
	return fqRuleName.substr(sepPos + 1);
}

SRL_TEST_EXPORTS_API inline std::wstring removeStyle(const std::wstring& fqRuleName) {
	return removePrefix(fqRuleName, STYLE_DELIMITER);
}

SRL_TEST_EXPORTS_API inline std::wstring removeImport(const std::wstring& fqRuleName) {
	return removePrefix(fqRuleName, IMPORT_DELIMITER);
}

} // namespace prtu

inline void replace_all_not_of(std::wstring& s, const std::wstring& allowedChars) {
	std::wstring::size_type pos = 0;
	while (pos < s.size()) {
		pos = s.find_first_not_of(allowedChars, pos);
		if (pos == std::wstring::npos)
			break;
		s[pos++] = L'_';
	}
}

inline bool startsWithAnyOf(const std::string& s, const std::vector<std::string>& sv) {
	for (const auto& v : sv) {
		if (s.find(v) == 0)
			return true;
	}
	return false;
}

template <typename C, typename Container, typename ElementType = typename Container::value_type>
std::basic_string<C> join(Container const& container, const std::basic_string<C>& delimiter = {}) {
	std::basic_ostringstream<C> os;
	auto b = std::begin(container), e = std::end(container);
	if (b != e) {
		std::copy(b, std::prev(e), std::ostream_iterator<ElementType, C>(os, delimiter.c_str()));
		b = std::prev(e);
	}
	if (b != e) {
		os << *b;
	}
	return os.str();
}

#if defined(__clang__)
#	define MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__GNUG__)
#	define MAYBE_UNUSED __attribute__((unused)) // [[maybe_unused]] not availble in GCC < 7
#elif defined(_MSC_VER)
#	define MAYBE_UNUSED // [[maybe_unused]] would require /std:c++latest i.e. C++17
#endif

template <typename M, typename K, typename F, typename... ARGS,
          std::enable_if_t<std::is_convertible<std::decay_t<K>, typename M::key_type>::value>* = nullptr>
auto getCachedValue(M& cache, K&& key, F valueFunc, ARGS&&... valueFuncArgs) {
	auto p = cache.find(key);
	if (p == cache.end()) {
		auto&& value = valueFunc(std::forward<ARGS>(valueFuncArgs)...);
		p = cache.emplace(std::forward<K>(key), std::forward<decltype(value)>(value)).first;
	}
	return p->second;
}