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
	bool operator==(const RuleAttribute& a, const RuleAttribute& b) {
		return (a.order == b.order)
			&& (a.groupOrder == b.groupOrder)
			&& (a.fqName == b.fqName)
			&& (a.ruleFile == b.ruleFile)
			&& (a.groups == b.groups)
			&& (a.memberOfStartRuleFile == b.memberOfStartRuleFile);
	}
} // namespace std


TEST_CASE("get rule style") {
	SECTION("standard") {
		const std::wstring fqRule = L"bar$foo";
		const std::wstring expStyle = L"bar";
		CHECK(prtu::getStyle(fqRule) == expStyle);
	}

	SECTION("no style") {
		const std::wstring fqRule = L"foo";
		CHECK(prtu::getStyle(fqRule).empty());
	}

	SECTION("empty rule") {
		const std::wstring fqRule;
		CHECK(prtu::getStyle(fqRule).empty());
	}

	SECTION("invalid 1") {
		const std::wstring fqRule = L"$foo";
		CHECK(prtu::getStyle(fqRule).empty());
	}

	SECTION("invalid 2") {
		const std::wstring fqRule = L"foo$";
		const std::wstring expStyle = L"foo";
		CHECK(prtu::getStyle(fqRule) == expStyle);
	}

	SECTION("invalid 3") {
		const std::wstring fqRule = L"$";
		CHECK(prtu::getStyle(fqRule).empty());
	}
}

TEST_CASE("remove rule style") {
	SECTION("standard") {
		const std::wstring fqRule = L"bar$foo";
		const std::wstring expName = L"foo";
		CHECK(prtu::removeStyle(fqRule) == expName);
	}

	SECTION("no style") {
		const std::wstring fqRule = L"foo";
		CHECK(prtu::removeStyle(fqRule) == fqRule);
	}

	SECTION("empty rule") {
		const std::wstring fqRule;
		CHECK(prtu::removeStyle(fqRule).empty());
	}

	SECTION("invalid 1") {
		const std::wstring fqRule = L"$foo";
		const std::wstring expName = L"foo";
		CHECK(prtu::removeStyle(fqRule) == expName);
	}

	SECTION("invalid 2") {
		const std::wstring fqRule = L"foo$";
		CHECK(prtu::removeStyle(fqRule).empty());
	}

	SECTION("invalid 3") {
		const std::wstring fqRule = L"$";
		CHECK(prtu::removeStyle(fqRule).empty());
	}
}

TEST_CASE("default attribute values") {
	const std::wstring rpk = testDataPath + L"/CE-6813-wrong-attr-style.rpk";
	ResolveMapCache::LookupResult lookupResult = prtCtx->mResolveMapCache->get(rpk);
	ResolveMapSPtr resolveMap = lookupResult.first;
	std::wstring ruleFile = L"bin/r1.cgb";
	RuleFileInfoUPtr ruleInfo(prt::createRuleFileInfo(resolveMap->getString(ruleFile.c_str())));

	RuleAttributes ruleAttrs = getRuleAttributes(ruleFile, ruleInfo.get());

	for (const auto& ap: ruleAttrs) {
		LOG_DBG << ap.fqName;
	}

	// TODO: add assertion for value, needs interface into PRTModifierAction.cpp without introducing maya dep here
}

const AttributeGroup AG_EMPTY = {};
const AttributeGroup ag_a = { L"a"};
const AttributeGroup ag_ak = { L"a", L"k" };
const AttributeGroup ag_b = { L"b"};
const AttributeGroup ag_bk = { L"b", L"k" };
const AttributeGroup ag_bkp = { L"b", L"k", L"p" };

TEST_CASE("global group order") {
	const RuleAttribute A = { L"style$A", L"A", ag_bk, ORDER_NONE, ORDER_NONE, L"foo", true };
	const RuleAttribute B = { L"style$B", L"B", ag_bk, ORDER_NONE, ORDER_NONE, L"foo", true };
	const RuleAttribute C = { L"style$C", L"C", ag_bkp, ORDER_NONE, 10, L"foo", true };
	const RuleAttribute D = { L"style$D", L"D", ag_a, ORDER_NONE, 20, L"foo", true };
	const RuleAttribute E = { L"style$E", L"E", ag_ak, ORDER_NONE, ORDER_NONE, L"foo", true };

	const RuleAttributes inp = { A, B, C, D, E };
	const AttributeGroupOrder ago = getGlobalGroupOrder(inp);
	CHECK(ago.size()     == 5);
	CHECK(ago.at(ag_bkp) == 10);
	CHECK(ago.at(ag_bk)  == 10);
	CHECK(ago.at(ag_b)   == 10);
	CHECK(ago.at(ag_ak)  == ORDER_NONE);
	CHECK(ago.at(ag_a)   == 20);
}

TEST_CASE("rule attribute sorting") {

	SECTION("rule file 1") {
		const RuleAttribute A = { L"style$A", L"A", {}, ORDER_NONE, ORDER_NONE, L"bar", true };
		const RuleAttribute B = { L"style$B", L"B", {}, ORDER_NONE, ORDER_NONE, L"foo", false };

		RuleAttributes inp = { B, A };
		const RuleAttributes exp = { A, B };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("rule file 2") {
		const RuleAttribute A = { L"style$A", L"A", {}, ORDER_NONE, ORDER_NONE, L"bar", false };
		const RuleAttribute B = { L"style$B", L"B", {}, ORDER_NONE, ORDER_NONE, L"foo", true };

		RuleAttributes inp = { B, A };
		const RuleAttributes exp = { B, A };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("group order") {
		const RuleAttribute A = { L"style$A", L"A", { L"foo" }, ORDER_NONE, 0, L"foo", true };
		const RuleAttribute B = { L"style$B", L"B", { L"foo" }, ORDER_NONE, 1, L"foo", true };

		RuleAttributes inp = { B, A };
		const RuleAttributes exp = { A, B };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups") {
		const RuleAttribute A = { L"style$A", L"A", { L"foo", L"bar" }, ORDER_NONE, ORDER_NONE, L"bar", true };
		const RuleAttribute B = { L"style$B", L"B", { L"foo" }, ORDER_NONE, ORDER_NONE, L"foo", true };

		RuleAttributes inp = { A, B };
		const RuleAttributes exp = { B, A };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups disjunct") {
		const RuleAttribute A = { L"style$A", L"A", { L"foo1", L"bar" }, ORDER_NONE, ORDER_NONE, L"bar", true };
		const RuleAttribute B = { L"style$B", L"B", { L"foo" }, ORDER_NONE, ORDER_NONE, L"foo", true };

		RuleAttributes inp = { A, B };
		const RuleAttributes exp = { B, A };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups on same level") {
		const RuleAttribute A = { L"style$A", L"A", { L"foo", L"bar" }, ORDER_NONE, ORDER_NONE, L"foo", true };
		const RuleAttribute B = { L"style$B", L"B", { L"foo" }, ORDER_NONE, ORDER_NONE, L"foo", true };
		const RuleAttribute C = { L"style$C", L"C", { L"foo", L"baz" }, ORDER_NONE, ORDER_NONE, L"foo", true };

		RuleAttributes inp = { C, A, B };
		const RuleAttributes exp = { B, A, C };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups with group order") {
		const RuleAttribute A = { L"style$A", L"A", { L"foo", L"bar" }, ORDER_NONE, ORDER_NONE, L"foo", true };
		const RuleAttribute B = { L"style$B", L"B", { L"foo" }, ORDER_NONE, ORDER_NONE, L"foo", true };
		const RuleAttribute C = { L"style$C", L"C", { L"foo", L"baz" }, ORDER_NONE, 0, L"foo", true };

		RuleAttributes inp = { C, A, B };
		const RuleAttributes exp = { B, C, A };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("all properties") {
		const RuleAttribute A = { L"style$A", L"A", { L"First1", L"Second1", L"Third1" }, ORDER_NONE, 0, L"foo", true };
		const RuleAttribute B = { L"style$B", L"B", { L"First" }, ORDER_NONE, 3, L"foo", true };
		const RuleAttribute C = { L"style$C", L"C", { L"First",  L"Second" }, 0, 2, L"foo", true };
		const RuleAttribute D = { L"style$D", L"D", { L"First",  L"Second" }, 1, 2, L"foo", true };
		const RuleAttribute E = { L"style$E", L"E", { L"First",  L"Second",  L"Third" }, ORDER_NONE, 1, L"foo", true };

		RuleAttributes inp = { B, A, C, D, E };
		const RuleAttributes exp = { A, B, C, D, E };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("review example") {
		// b k < b k p (group order=10) < a (group order=20) < a k < b k

		const RuleAttribute A = { L"style$A", L"A", ag_bk, ORDER_NONE, ORDER_NONE, L"foo", true };
		const RuleAttribute B = { L"style$B", L"B", ag_bk, ORDER_NONE, ORDER_NONE, L"foo", true };
		const RuleAttribute C = { L"style$C", L"C", ag_bkp, ORDER_NONE, 10, L"foo", true };
		const RuleAttribute D = { L"style$D", L"D", ag_a, ORDER_NONE, 20, L"foo", true };
		const RuleAttribute E = { L"style$E", L"E", ag_ak, ORDER_NONE, ORDER_NONE, L"foo", true };

		RuleAttributes inp = { A, B, C, D, E };
		const RuleAttributes exp = { A, B, C, D, E };
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
