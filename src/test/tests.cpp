#include "prtModifier/RuleAttributes.h"
#include "util/Utilities.h"

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#include <sstream>

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

int main( int argc, char* argv[] ) {
	int result = Catch::Session().run( argc, argv );
	return result;
}