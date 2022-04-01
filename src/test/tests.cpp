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

#include "modifiers/RuleAttributes.h"

#include "utils/LogHandler.h"
#include "utils/Utilities.h"

#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_FAST_COMPILE
#include "catch2/catch.hpp"

#include <sstream>

namespace {

PRTContextUPtr prtCtx;
const std::wstring testDataPath = prtu::toUTF16FromOSNarrow(TEST_DATA_PATH);

} // namespace

namespace std {
// inject comparison for AttributeProperties into std namespace so STL algos can find it
bool operator==(const RuleAttribute& a, const RuleAttribute& b) {
	// clang-format off
	return (a.order == b.order)
	        && (a.groupOrder == b.groupOrder)
	        && (a.fqName == b.fqName)
	        && (a.mayaBriefName == b.mayaBriefName)
	        && (a.mayaFullName == b.mayaFullName)
	        && (a.mayaNiceName == b.mayaNiceName)
	        && (a.ruleFile == b.ruleFile)
	        && (a.groups == b.groups)
	        && (a.memberOfStartRuleFile == b.memberOfStartRuleFile);
	// clang-format on
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

	RuleAttributeSet ruleAttrs = getRuleAttributes(ruleFile, ruleInfo.get());

	for (const auto& ap : ruleAttrs) {
		LOG_DBG << ap.fqName;
	}

	// TODO: add assertion for value, needs interface into PRTModifierAction.cpp without introducing maya dep here
}

const AttributeGroup AG_NONE = {};
const AttributeGroup AG_A = {L"a"};
const AttributeGroup AG_AK = {L"a", L"k"};
const AttributeGroup AG_B = {L"b"};
const AttributeGroup AG_BK = {L"b", L"k"};
const AttributeGroup AG_BKP = {L"b", L"k", L"p"};

RuleAttribute getAttr(std::wstring fqName, AttributeGroup ag, int o, int go, std::wstring rf,int ro, bool sr) {
	return RuleAttribute{fqName, L"", L"", L"", prt::AAT_UNKNOWN, ag, o, go, go, rf,ro, sr};
}

TEST_CASE("global group order") {
	const RuleAttribute A = getAttr(L"style$A", AG_BK, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
	const RuleAttribute B = getAttr(L"style$B", AG_BK, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
	const RuleAttribute C = getAttr(L"style$C", AG_BKP, ORDER_NONE, 10, L"foo", ORDER_NONE, true);
	const RuleAttribute D = getAttr(L"style$D", AG_A, ORDER_NONE, 20, L"foo", ORDER_NONE, true);
	const RuleAttribute E = getAttr(L"style$E", AG_AK, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);

	RuleAttributeVec inp = {A, B, C, D, E};
	setGlobalGroupOrder(inp);
	CHECK(inp.size() == 5);
	CHECK(inp[0].globalGroupOrder == 10);
	CHECK(inp[1].globalGroupOrder == 10);
	CHECK(inp[2].globalGroupOrder == 10);
	CHECK(inp[3].globalGroupOrder == 20);
	CHECK(inp[4].globalGroupOrder == ORDER_NONE);
}

TEST_CASE("rule attribute sorting") {

	SECTION("rule file 1") {
		const RuleAttribute A = getAttr(L"style$A", AG_NONE, ORDER_NONE, ORDER_NONE, L"bar", ORDER_NONE, true);
		const RuleAttribute B = getAttr(L"style$B", AG_NONE, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, false);

		const RuleAttributeSet inp = {B, A};
		const RuleAttributeVec inpAsVec(inp.begin(), inp.end());
		const RuleAttributeVec exp = {A, B};
		CHECK(inpAsVec == exp);
	}

	SECTION("rule file 2") {
		const RuleAttribute A = getAttr(L"style$A", AG_NONE, ORDER_NONE, ORDER_NONE, L"bar", ORDER_NONE, false);
		const RuleAttribute B = getAttr(L"style$B", AG_NONE, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);

		const RuleAttributeSet inp = {B, A};
		const RuleAttributeVec inpAsVec(inp.begin(), inp.end());
		const RuleAttributeVec exp = {B, A};
		CHECK(inpAsVec == exp);
	}

	SECTION("group order") {
		const RuleAttribute A = getAttr(L"style$A", {L"foo"}, ORDER_NONE, 0, L"foo", ORDER_NONE, true);
		const RuleAttribute B = getAttr(L"style$B", {L"foo"}, ORDER_NONE, 1, L"foo", ORDER_NONE, true);

		const RuleAttributeSet inp = {B, A};
		const RuleAttributeVec inpAsVec(inp.begin(), inp.end());
		const RuleAttributeVec exp = {A, B};
		CHECK(inpAsVec == exp);
	}

	SECTION("nested groups") {
		const RuleAttribute A = getAttr(L"style$A", {L"foo", L"bar"}, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
		const RuleAttribute B = getAttr(L"style$B", {L"foo"}, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);

		const RuleAttributeSet inp = {A, B};
		const RuleAttributeVec inpAsVec(inp.begin(), inp.end());
		const RuleAttributeVec exp = {B, A};
		CHECK(inpAsVec == exp);
	}

	SECTION("nested groups disjunct") {
		const RuleAttribute A = getAttr(L"style$A", {L"foo1", L"bar"}, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
		const RuleAttribute B = getAttr(L"style$B", {L"foo"}, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);

		const RuleAttributeSet inp = {A, B};
		const RuleAttributeVec inpAsVec(inp.begin(), inp.end());
		const RuleAttributeVec exp = {B, A};
		CHECK(inpAsVec == exp);
	}

	SECTION("nested groups on same level") {
		const RuleAttribute A = getAttr(L"style$A", {L"foo", L"bar"}, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
		const RuleAttribute B = getAttr(L"style$B", {L"foo"}, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
		const RuleAttribute C = getAttr(L"style$C", {L"foo", L"baz"}, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);

		const RuleAttributeSet inp = {C, A, B};
		const RuleAttributeVec inpAsVec(inp.begin(), inp.end());
		const RuleAttributeVec exp = {B, A, C};
		CHECK(inpAsVec == exp);
	}

	SECTION("nested groups with group order") {
		const RuleAttribute A = getAttr(L"style$A", {L"foo", L"bar"}, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
		const RuleAttribute B = getAttr(L"style$B", {L"foo"}, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
		const RuleAttribute C = getAttr(L"style$C", {L"foo", L"baz"}, ORDER_NONE, 0, L"foo", ORDER_NONE, true);

		const RuleAttributeSet inp = {C, A, B};
		const RuleAttributeVec inpAsVec(inp.begin(), inp.end());
		const RuleAttributeVec exp = {B, C, A};
		for (const auto& ref: inpAsVec){
			std::wcout << ref;
		}
		CHECK(inpAsVec == exp);
	}

	SECTION("all properties") {
		const RuleAttribute A = getAttr(L"style$A", {L"First1", L"Second1", L"Third1"}, ORDER_NONE, 0, L"foo", ORDER_NONE, true);
		const RuleAttribute B = getAttr(L"style$B", {L"First"}, ORDER_NONE, 3, L"foo", ORDER_NONE, true);
		const RuleAttribute C = getAttr(L"style$C", {L"First", L"Second"}, 0, 2, L"foo", ORDER_NONE, true);
		const RuleAttribute D = getAttr(L"style$D", {L"First", L"Second"}, 1, 2, L"foo", ORDER_NONE, true);
		const RuleAttribute E = getAttr(L"style$E", {L"First", L"Second", L"Third"}, ORDER_NONE, 1, L"foo", ORDER_NONE, true);

		const RuleAttributeSet inp = {B, A, C, D, E};
		const RuleAttributeVec inpAsVec(inp.begin(), inp.end());
		const RuleAttributeVec exp = {A, B, C, D, E};
		CHECK(inpAsVec == exp);
	}

	SECTION("review example") {
		// b k < b k p (group order=10) < a (group order=20) < a k < b k

		const RuleAttribute A = getAttr(L"style$A", AG_BK, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
		const RuleAttribute B = getAttr(L"style$B", AG_BK, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);
		const RuleAttribute C = getAttr(L"style$C", AG_BKP, ORDER_NONE, 10, L"foo", ORDER_NONE, true);
		const RuleAttribute D = getAttr(L"style$D", AG_A, ORDER_NONE, 20, L"foo", ORDER_NONE, true);
		const RuleAttribute E = getAttr(L"style$E", AG_AK, ORDER_NONE, ORDER_NONE, L"foo", ORDER_NONE, true);

		const RuleAttributeSet inp = {A, B, C, D, E};
		const RuleAttributeVec inpAsVec(inp.begin(), inp.end());
		const RuleAttributeVec exp = {A, B, C, D, E};
		CHECK(inpAsVec == exp);
	}
}

TEST_CASE("join") {
	const std::vector<std::wstring> input1 = {L"foo"};
	CHECK(join<wchar_t>(input1, L" ") == L"foo");

	const std::vector<std::wstring> input2 = {L"foo", L"bar", L"baz"};
	CHECK(join<wchar_t>(input2, L" ") == L"foo bar baz");
	CHECK(join<wchar_t>(input2, L";") == L"foo;bar;baz");
	CHECK(join<wchar_t>(input2, L"") == L"foobarbaz");
	CHECK(join<wchar_t>(input2) == L"foobarbaz");

	const std::vector<std::wstring> input3 = {};
	CHECK(join<wchar_t>(input3, L" ") == L"");
}

TEST_CASE("replaceCGACWithCEVersion") {
	SECTION("do nothing") {
		std::wstring inp = L"No CGA and CGAC versions found - assuming unreleased CGA 2020.0 and CGAC 1.14";
		const std::wstring exp = L"No CGA and CGAC versions found - assuming unreleased CGA 2020.0 and CGAC 1.14";
		prtu::replaceCGACWithCEVersion(inp);
		CHECK(inp == exp);
	}

	SECTION("major number larger than current") {
		std::wstring inp = L"Unsupported CGAC version 2.0 : major number larger than current (1.17)";
		const std::wstring exp = L"Unsupported CityEngine version newer than 2021.1 : major number larger than current (2021.1)";
		prtu::replaceCGACWithCEVersion(inp);
		CHECK(inp == exp);
	}

	SECTION("major number smaller than current") {
		std::wstring inp = L"Potentially unsupported CGAC version 1.0 : major number smaller than current (2.0)";
		const std::wstring exp = L"Potentially unsupported CityEngine version 2013.0 : major number smaller than current (newer than 2021.1)";
		prtu::replaceCGACWithCEVersion(inp);
		CHECK(inp == exp);
	}

	SECTION("minor number larger than current") {
		std::wstring inp = L"Potentially unsupported CGAC version 1.17 : newer than current (1.5)";
		const std::wstring exp = L"Potentially unsupported CityEngine version 2021.1 : newer than current (2015.0 - 2015.2)";
		prtu::replaceCGACWithCEVersion(inp);
		CHECK(inp == exp);
	}

	SECTION("problematic CityEngine version") {
		std::wstring inp = L"Potentially problematic CGAC version 1.3 : recompiling with current CGA Compiler (1.17) is recommended.";
		const std::wstring exp = L"Potentially problematic CityEngine version 2014.1 : recompiling with current CGA Compiler (2021.1) is recommended.";
		prtu::replaceCGACWithCEVersion(inp);
		CHECK(inp == exp);
	}
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

TEST_CASE("getDuplicateCountSuffix") {
	std::map<std::wstring, int> duplicateCountMap;

	// Fully quantified attributename: Default$import1.myAttr
	const std::wstring briefAttr1 = L"import1_myAttr";
	const std::wstring fullAttr1 = L"Default_import1_myAttr";
	const std::wstring briefAttr1Suffix = prtu::getDuplicateCountSuffix(briefAttr1, duplicateCountMap);
	const std::wstring fullAttr1Suffix = prtu::getDuplicateCountSuffix(fullAttr1, duplicateCountMap);
	const std::wstring expectedBriefAttr1Suffix = L"_0";
	const std::wstring expectedfullAttr1Suffix = L"_0";
	CHECK(briefAttr1Suffix == expectedBriefAttr1Suffix);
	CHECK(fullAttr1Suffix == expectedfullAttr1Suffix);

	// Fully quantified attributename: Default$import1_myAttr
	const std::wstring briefAttr2 = L"import1_myAttr";
	const std::wstring fullAttr2 = L"Default_import1_myAttr";
	const std::wstring briefAttr2Suffix = prtu::getDuplicateCountSuffix(briefAttr2, duplicateCountMap);
	const std::wstring fullAttr2Suffix = prtu::getDuplicateCountSuffix(fullAttr2, duplicateCountMap);
	const std::wstring expectedBriefAttr2Suffix = L"_1";
	const std::wstring expectedfullAttr2Suffix = L"_1";
	CHECK(briefAttr2Suffix == expectedBriefAttr2Suffix);
	CHECK(fullAttr2Suffix == expectedfullAttr2Suffix);

	// Fully quantified attributename: Default_import1$myAttr
	const std::wstring briefAttr3 = L"myAttr";
	const std::wstring fullAttr3 = L"Default_import1_myAttr";
	const std::wstring briefAttr3Suffix = prtu::getDuplicateCountSuffix(briefAttr3, duplicateCountMap);
	const std::wstring fullAttr3Suffix = prtu::getDuplicateCountSuffix(fullAttr3, duplicateCountMap);
	const std::wstring expectedBriefAttr3Suffix = L"_0";
	const std::wstring expectedfullAttr3Suffix = L"_2";
	CHECK(briefAttr3Suffix == expectedBriefAttr3Suffix);
	CHECK(fullAttr3Suffix == expectedfullAttr3Suffix);
}

TEST_CASE("replaceAllNotOf") {
	const std::wstring allowedChars = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	SECTION("empty") {
		std::wstring testString;
		const std::wstring expected;
		replaceAllNotOf(testString, allowedChars);
		CHECK(testString == expected);
	}
	SECTION("replace nothing") {
		std::wstring testString = L"The_quick_brown_fox_jumps_over_the_lazy_dog";
		const std::wstring expected = L"The_quick_brown_fox_jumps_over_the_lazy_dog";
		replaceAllNotOf(testString, allowedChars);
		CHECK(testString == expected);
	}
	SECTION("replace some") {
		std::wstring testString = L"Replace:all\r\nbut=alpha^numerical.characters;";
		const std::wstring expected = L"Replace_all__but_alpha_numerical_characters_";
		replaceAllNotOf(testString, allowedChars);
		CHECK(testString == expected);
	}
	SECTION("replace all") {
		std::wstring testString = L"/:\r^?=-\\%`*\"+-";
		const std::wstring expected = L"______________";
		replaceAllNotOf(testString, allowedChars);
		CHECK(testString == expected);
	}
}

TEST_CASE("replaceAllOf") {
	const std::wstring bannedChars = L"=:\\;\r\n";
	SECTION("empty") {
		std::wstring testString;
		const std::wstring expected;
		replaceAllOf(testString, bannedChars);
		CHECK(testString == expected);
	}
	SECTION("replace nothing") {
		std::wstring testString = L"The quick brown fox jumps over the lazy dog";
		const std::wstring expected = L"The quick brown fox jumps over the lazy dog";
		replaceAllOf(testString, bannedChars);
		CHECK(testString == expected);
	}
	SECTION("replace some") {
		std::wstring testString = L"A=B+C;\r\nE:F";
		const std::wstring expected = L"A_B+C___E_F";
		replaceAllOf(testString, bannedChars);
		CHECK(testString == expected);
	}
	SECTION("replace all") {
		std::wstring testString = L"=:\\;\r\n";
		const std::wstring expected = L"______";
		replaceAllOf(testString, bannedChars);
		CHECK(testString == expected);
	}
}

TEST_CASE("cleanNameForMaya") {
	SECTION("empty") {
		std::wstring testString;
		const std::wstring expected;
		const std::wstring output = prtu::cleanNameForMaya(testString);
		CHECK(output == expected);
	}
	SECTION("replace nothing") {
		std::wstring testString = L"The_quick_brown_fox_jumps_over_the_lazy_dog";
		const std::wstring expected = L"The_quick_brown_fox_jumps_over_the_lazy_dog";
		const std::wstring output = prtu::cleanNameForMaya(testString);
		CHECK(output == expected);
	}
	SECTION("replace some") {
		std::wstring testString = L"Replace:all\r\nbut=alpha^numerical.characters;";
		const std::wstring expected = L"Replace_all__but_alpha_numerical_characters_";
		const std::wstring output = prtu::cleanNameForMaya(testString);
		CHECK(output == expected);
	}
	SECTION("replace all") {
		std::wstring testString = L"/:\r^?=-\\%`*\"+-";
		const std::wstring expected = L"______________";
		const std::wstring output = prtu::cleanNameForMaya(testString);
		CHECK(output == expected);
	}
	SECTION("add prefix") {
		std::wstring testString = L"42";
		const std::wstring expected = L"_42";
		const std::wstring output = prtu::cleanNameForMaya(testString);
		CHECK(output == expected);
	}
}

// we use a custom main function to manage PRT lifetime
int main(int argc, char* argv[]) {
	const std::vector<std::wstring> addExtDirs = {
	        prtu::toUTF16FromOSNarrow(SERLIO_CODEC_PATH) // set to absolute path to serlio encoder lib via cmake
	};

	prtCtx.reset(new PRTContext(addExtDirs));
	if (!prtCtx->isAlive())
		return 1;

	int result = Catch::Session().run(argc, argv);
	prtCtx.reset();

	return result;
}
