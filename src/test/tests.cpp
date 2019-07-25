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
			&& (a.mayaBriefName == b.mayaBriefName)
			&& (a.mayaFullName == b.mayaFullName)
			&& (a.mayaNiceName == b.mayaNiceName)
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

const AttributeGroup AG_NONE = {};
const AttributeGroup AG_A    = { L"a"};
const AttributeGroup AG_AK   = { L"a", L"k" };
const AttributeGroup AG_B    = { L"b"};
const AttributeGroup AG_BK   = { L"b", L"k" };
const AttributeGroup AG_BKP  = { L"b", L"k", L"p" };

RuleAttribute getAttr(std::wstring fqName, AttributeGroup ag, int o, int go, std::wstring rf, bool sr) {
	return RuleAttribute{ fqName, L"", L"", L"", prt::AAT_UNKNOWN, ag, o, go, rf, sr };
}

TEST_CASE("global group order") {
	const RuleAttribute A = getAttr(L"style$A", AG_BK, ORDER_NONE, ORDER_NONE, L"foo", true );
	const RuleAttribute B = getAttr(L"style$B", AG_BK, ORDER_NONE, ORDER_NONE, L"foo", true );
	const RuleAttribute C = getAttr(L"style$C", AG_BKP, ORDER_NONE, 10, L"foo", true );
	const RuleAttribute D = getAttr(L"style$D", AG_A, ORDER_NONE, 20, L"foo", true );
	const RuleAttribute E = getAttr(L"style$E", AG_AK, ORDER_NONE, ORDER_NONE, L"foo", true );

	const RuleAttributes inp = { A, B, C, D, E };
	const AttributeGroupOrder ago = getGlobalGroupOrder(inp);
	CHECK(ago.size()     == 5);
	CHECK(ago.at(AG_BKP) == 10);
	CHECK(ago.at(AG_BK)  == 10);
	CHECK(ago.at(AG_B)   == 10);
	CHECK(ago.at(AG_AK)  == ORDER_NONE);
	CHECK(ago.at(AG_A)   == 20);
}

TEST_CASE("rule attribute sorting") {

	SECTION("rule file 1") {
		const RuleAttribute A = getAttr(L"style$A", AG_NONE, ORDER_NONE, ORDER_NONE, L"bar", true);
		const RuleAttribute B = getAttr(L"style$B", AG_NONE, ORDER_NONE, ORDER_NONE, L"foo", false);

		RuleAttributes inp = { B, A };
		const RuleAttributes exp = { A, B };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("rule file 2") {
		const RuleAttribute A = getAttr(L"style$A", AG_NONE, ORDER_NONE, ORDER_NONE, L"bar", false);
		const RuleAttribute B = getAttr(L"style$B", AG_NONE, ORDER_NONE, ORDER_NONE, L"foo", true);

		RuleAttributes inp = { B, A };
		const RuleAttributes exp = { B, A };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("group order") {
		const RuleAttribute A = getAttr(L"style$A", { L"foo" }, ORDER_NONE, 0, L"foo", true);
		const RuleAttribute B = getAttr(L"style$B", { L"foo" }, ORDER_NONE, 1, L"foo", true);

		RuleAttributes inp = { B, A };
		const RuleAttributes exp = { A, B };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups") {
		const RuleAttribute A = getAttr(L"style$A", { L"foo", L"bar" }, ORDER_NONE, ORDER_NONE, L"bar", true);
		const RuleAttribute B = getAttr(L"style$B", { L"foo" }, ORDER_NONE, ORDER_NONE, L"foo", true);

		RuleAttributes inp = { A, B };
		const RuleAttributes exp = { B, A };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups disjunct") {
		const RuleAttribute A = getAttr(L"style$A", { L"foo1", L"bar" }, ORDER_NONE, ORDER_NONE, L"bar", true);
		const RuleAttribute B = getAttr(L"style$B", { L"foo" }, ORDER_NONE, ORDER_NONE, L"foo", true);

		RuleAttributes inp = { A, B };
		const RuleAttributes exp = { B, A };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups on same level") {
		const RuleAttribute A = getAttr(L"style$A", { L"foo", L"bar" }, ORDER_NONE, ORDER_NONE, L"foo", true);
		const RuleAttribute B = getAttr(L"style$B", { L"foo" }, ORDER_NONE, ORDER_NONE, L"foo", true);
		const RuleAttribute C = getAttr(L"style$C", { L"foo", L"baz" }, ORDER_NONE, ORDER_NONE, L"foo", true);

		RuleAttributes inp = { C, A, B };
		const RuleAttributes exp = { B, A, C };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("nested groups with group order") {
		const RuleAttribute A = getAttr(L"style$A", { L"foo", L"bar" }, ORDER_NONE, ORDER_NONE, L"foo", true);
		const RuleAttribute B = getAttr(L"style$B", { L"foo" }, ORDER_NONE, ORDER_NONE, L"foo", true);
		const RuleAttribute C = getAttr(L"style$C", { L"foo", L"baz" }, ORDER_NONE, 0, L"foo", true);

		RuleAttributes inp = { C, A, B };
		const RuleAttributes exp = { B, C, A };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("all properties") {
		const RuleAttribute A = getAttr(L"style$A", { L"First1", L"Second1", L"Third1" }, ORDER_NONE, 0, L"foo", true);
		const RuleAttribute B = getAttr(L"style$B", { L"First" }, ORDER_NONE, 3, L"foo", true);
		const RuleAttribute C = getAttr(L"style$C", { L"First",  L"Second" }, 0, 2, L"foo", true);
		const RuleAttribute D = getAttr(L"style$D", { L"First",  L"Second" }, 1, 2, L"foo", true);
		const RuleAttribute E = getAttr(L"style$E", { L"First",  L"Second",  L"Third" }, ORDER_NONE, 1, L"foo", true);

		RuleAttributes inp = { B, A, C, D, E };
		const RuleAttributes exp = { A, B, C, D, E };
		sortRuleAttributes(inp);
		CHECK(inp == exp);
	}

	SECTION("review example") {
		// b k < b k p (group order=10) < a (group order=20) < a k < b k

		const RuleAttribute A = getAttr(L"style$A", AG_BK, ORDER_NONE, ORDER_NONE, L"foo", true);
		const RuleAttribute B = getAttr(L"style$B", AG_BK, ORDER_NONE, ORDER_NONE, L"foo", true);
		const RuleAttribute C = getAttr(L"style$C", AG_BKP, ORDER_NONE, 10, L"foo", true);
		const RuleAttribute D = getAttr(L"style$D", AG_A, ORDER_NONE, 20, L"foo", true);
		const RuleAttribute E = getAttr(L"style$E", AG_AK, ORDER_NONE, ORDER_NONE, L"foo", true);

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
		prtu::toUTF16FromOSNarrow(SERLIO_CODEC_PATH) // set to absolute path to serlio encoder lib via cmake
	};

	prtCtx.reset(new PRTContext(addExtDirs));
	int result = Catch::Session().run( argc, argv );
	prtCtx.reset();

	return result;
}
