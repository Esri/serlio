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

#include "prtModifier/RuleAttributes.h"
#include "util/Utilities.h"
#include "util/LogHandler.h"
#include "PRTContext.h"

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <sstream>


namespace {

PRTContextUPtr prtCtx;
const std::wstring testDataPath = prtu::toUTF16FromOSNarrow(TEST_DATA_PATH);

} // namespace

namespace std {
	// inject comparison for AttributeProperties into std namespace so STL algos can find it
	bool operator==(const AttributeProperties& a, const AttributeProperties& b) {
		return (a.order == b.order)
			&& (a.groupOrder == b.groupOrder)
			&& (a.index == b.index)
			&& (a.name == b.name)
			&& (a.ruleFile == b.ruleFile)
			&& (a.groups == b.groups)
			&& (a.enumAnnotation == b.enumAnnotation)
			&& (a.memberOfStartRuleFile == b.memberOfStartRuleFile);
	}
} // namespace std


TEST_CASE("default attribute values") {
	const std::wstring rpk = testDataPath + L"/CE-6813-wrong-attr-style.rpk";
	ResolveMapCache::LookupResult lookupResult = prtCtx->mResolveMapCache->get(rpk);
	ResolveMapSPtr resolveMap = lookupResult.first;
	std::wstring ruleFile = L"bin/r1.cgb";
	RuleFileInfoUPtr ruleInfo(prt::createRuleFileInfo(resolveMap->getString(ruleFile.c_str())));

	RuleAttributes ruleAttrs = getRuleAttributes(ruleFile, ruleInfo.get());

	for (const auto& ap: ruleAttrs) {
		LOG_DBG << ap.name;
	}

	// TODO: add assertion for value, needs interface into PRTModifierAction.cpp without introducing maya dep here
}

TEST_CASE("global group order") {
	AttributeGroup ag_bk = { L"b", L"k" };
	AttributeGroup ag_bkp = { L"b", L"k", L"p" };
	AttributeGroup ag_a = { L"a"};
	AttributeGroup ag_ak = { L"a", L"k" };

	const RuleAttributes inp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", ag_bk, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", ag_bk, nullptr, true },
			{ ORDER_NONE, 10, 0, L"C", L"foo", ag_bkp, nullptr, true },
			{ ORDER_NONE, 20, 0, L"D", L"foo", ag_a, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"E", L"foo", ag_ak, nullptr, true },
	};

	AttributeGroupOrder ago = getGlobalGroupOrder(inp);
	CHECK(ago.size() == 5);
	CHECK(ago[{ L"b", L"k", L"p" }] == 10);
	CHECK(ago[{ L"b", L"k" }]       == 10);
	CHECK(ago[{ L"b" }]             == 10);
	CHECK(ago[{ L"a", L"k" }]       == ORDER_NONE);
	CHECK(ago[{ L"a" }]             == 20);
}

TEST_CASE("rule attribute sorting") {

	SECTION("rule file 1") {
		RuleAttributes inp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { }, nullptr, false },
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"bar", { }, nullptr, true },
		};

		RuleAttributes exp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"bar", { }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { }, nullptr, false },
		};

		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("rule file 2") {
		RuleAttributes inp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"bar", { }, nullptr, false },
		};

		RuleAttributes exp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"bar", { }, nullptr, false },
		};

		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("group order") {
		RuleAttributes inp = {
			{ ORDER_NONE, 1, 0, L"B", L"foo", { L"foo" }, nullptr, true },
			{ ORDER_NONE, 0, 0, L"A", L"foo", { L"foo" }, nullptr, true },
		};

		RuleAttributes exp = {
			{ ORDER_NONE, 0, 0, L"A", L"foo", { L"foo" }, nullptr, true },
			{ ORDER_NONE, 1, 0, L"B", L"foo", { L"foo" }, nullptr, true },
		};

		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups") {
		RuleAttributes inp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"foo", L"bar" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"foo" }, nullptr, true },
		};

		RuleAttributes exp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"foo" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"foo", L"bar" }, nullptr, true },
		};

		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups disjunct") {
		RuleAttributes inp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"foo1", L"bar" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"foo" }, nullptr, true },
		};

		RuleAttributes exp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"foo" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"foo1", L"bar" }, nullptr, true },
		};

		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups on same level") {
		RuleAttributes inp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"C", L"foo", { L"foo", L"baz" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"foo", L"bar" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"foo" }, nullptr, true },
		};

		RuleAttributes exp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"foo" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"foo", L"bar" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"C", L"foo", { L"foo", L"baz" }, nullptr, true },
		};

		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups with group order") {
		RuleAttributes inp = {
			{ ORDER_NONE, 0,          0, L"C", L"foo", { L"foo", L"baz" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"foo", L"bar" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"foo" }, nullptr, true },
		};

		RuleAttributes exp = {
			{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"foo" }, nullptr, true },
			{ ORDER_NONE, 0,          0, L"C", L"foo", { L"foo", L"baz" }, nullptr, true },
			{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"foo", L"bar" }, nullptr, true },
		};

		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("all properties") {
		RuleAttributes inp = {
				{ ORDER_NONE, 3, 0, L"B", L"foo", { L"First" }, nullptr, true },
				{ ORDER_NONE, 0, 0, L"A", L"foo", { L"First1", L"Second1", L"Third1" }, nullptr, true },
				{ 0,          2, 0, L"C", L"foo", { L"First",  L"Second" }, nullptr, true },
				{ 1,          2, 0, L"D", L"foo", { L"First",  L"Second" }, nullptr, true },
				{ ORDER_NONE, 1, 0, L"E", L"foo", { L"First",  L"Second",  L"Third" }, nullptr, true }
		};
		const RuleAttributes exp = {
				{ ORDER_NONE, 0, 0, L"A", L"foo", { L"First1", L"Second1", L"Third1" }, nullptr, true },
				{ ORDER_NONE, 3, 0, L"B", L"foo", { L"First" }, nullptr, true },
				{ 0,          2, 0, L"C", L"foo", { L"First",  L"Second" }, nullptr, true },
				{ 1,          2, 0, L"D", L"foo", { L"First",  L"Second" }, nullptr, true },
				{ ORDER_NONE, 1, 0, L"E", L"foo", { L"First",  L"Second",  L"Third" }, nullptr, true }
		};

		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("review example") {
		// b k < b k p (group order=10) < a (group order=20) < a k < b k
		RuleAttributes inp = {
				{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"b", L"k" }, nullptr, true },
				{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"b",  L"k" }, nullptr, true },
				{ ORDER_NONE, 10, 0, L"C", L"foo", { L"b", L"k", L"p" }, nullptr, true },
				{ ORDER_NONE, 20, 0, L"D", L"foo", { L"a" }, nullptr, true },
				{ ORDER_NONE, ORDER_NONE, 0, L"E", L"foo", { L"a",  L"k" }, nullptr, true },
		};
		const RuleAttributes exp = {
				{ ORDER_NONE, ORDER_NONE, 0, L"A", L"foo", { L"b", L"k" }, nullptr, true },
				{ ORDER_NONE, ORDER_NONE, 0, L"B", L"foo", { L"b",  L"k" }, nullptr, true },
				{ ORDER_NONE, 10, 0, L"C", L"foo", { L"b", L"k", L"p" }, nullptr, true },
				{ ORDER_NONE, 20, 0, L"D", L"foo", { L"a" }, nullptr, true },
				{ ORDER_NONE, ORDER_NONE, 0, L"E", L"foo", { L"a",  L"k" }, nullptr, true },
		};
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}
}

TEST_CASE("join") {
	const std::vector<std::wstring> input1 = { L"foo" };
	CHECK(join<wchar_t>(input1, L" ") == L"foo");

	const std::vector<std::wstring> input2 = { L"foo", L"bar", L"baz" };
	CHECK(join<wchar_t>(input2, L" ") == L"foo bar baz");
	CHECK(join<wchar_t>(input2, L";") == L"foo;bar;baz");
	CHECK(join<wchar_t>(input2, L"") == L"foobarbaz");
	CHECK(join<wchar_t>(input2) == L"foobarbaz");

	const std::vector<std::wstring> input3 = { };
	CHECK(join<wchar_t>(input3, L" ") == L"");
}

TEST_CASE("toFileURI") {
#if _WIN32
	SECTION("windows") {
		const std::wstring path = L"c:/tmp/foo.bar";
		const std::wstring expected = L"file:/c:/tmp/foo.bar";
		CHECK(prtu::toFileURI(path) == expected);
	}
#else
	SECTION("posix") {
		const std::wstring path = L"/tmp/foo.bar";
		CHECK(prtu::toFileURI(path) == L"file:/tmp/foo.bar");
	}
#endif
}


// we use a custom main function to manage PRT lifetime
int main( int argc, char* argv[] ) {
	const std::vector<std::wstring> addExtDirs = {
		prtu::toUTF16FromOSNarrow(SERLIO_CODEC_PATH) // set to absolute path to houdini encoder lib via cmake
	};

	prtCtx.reset(new PRTContext(addExtDirs));
	int result = Catch::Session().run( argc, argv );
	prtCtx.reset();

	return result;
}
