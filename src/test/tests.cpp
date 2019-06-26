#include "prtModifier/AttributeProperties.h"
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
			&& (a.enumAnnotation == b.enumAnnotation);
	}
} // namespace std


TEST_CASE("rule attribute sorting") {

	SECTION("rule file 1") {
		RuleAttributes inp = {
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { } },
			{ NO_ORDER, NO_ORDER, 0, L"A", L"bar", { } },
		};

		RuleAttributes exp = {
			{ NO_ORDER, NO_ORDER, 0, L"A", L"bar", { } },
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { } },
		};

		sortRuleAttributes(inp, L"bar");
		CHECK(inp == exp);
	}

	SECTION("rule file 2") {
		RuleAttributes inp = {
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { } },
			{ NO_ORDER, NO_ORDER, 0, L"A", L"bar", { } },
		};

		RuleAttributes exp = {
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { } },
			{ NO_ORDER, NO_ORDER, 0, L"A", L"bar", { } },
		};

		sortRuleAttributes(inp, L"foo");
		CHECK(inp == exp);
	}

	SECTION("group order") {
		RuleAttributes inp = {
			{ NO_ORDER, 1, 0, L"B", L"foo", { L"foo" } },
			{ NO_ORDER, 0, 0, L"A", L"foo", { L"foo" } },
		};

		RuleAttributes exp = {
			{ NO_ORDER, 0, 0, L"A", L"foo", { L"foo" } },
			{ NO_ORDER, 1, 0, L"B", L"foo", { L"foo" } },
		};

		sortRuleAttributes(inp, L"foo");
		CHECK(inp == exp);
	}

	SECTION("nested groups") {
		RuleAttributes inp = {
			{ NO_ORDER, NO_ORDER, 0, L"A", L"foo", { L"foo", L"bar" } },
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { L"foo" } },
		};

		RuleAttributes exp = {
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { L"foo" } },
			{ NO_ORDER, NO_ORDER, 0, L"A", L"foo", { L"foo", L"bar" } },
		};

		sortRuleAttributes(inp, L"foo");
		CHECK(inp == exp);
	}

	SECTION("nested groups disjunct") {
		RuleAttributes inp = {
			{ NO_ORDER, NO_ORDER, 0, L"A", L"foo", { L"foo1", L"bar" } },
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { L"foo" } },
		};

		RuleAttributes exp = {
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { L"foo" } },
			{ NO_ORDER, NO_ORDER, 0, L"A", L"foo", { L"foo1", L"bar" } },
		};

		sortRuleAttributes(inp, L"foo");
		CHECK(inp == exp);
	}

	SECTION("nested groups on same level") {
		RuleAttributes inp = {
			{ NO_ORDER, NO_ORDER, 0, L"C", L"foo", { L"foo", L"baz" } },
			{ NO_ORDER, NO_ORDER, 0, L"A", L"foo", { L"foo", L"bar" } },
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { L"foo" } },
		};

		RuleAttributes exp = {
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { L"foo" } },
			{ NO_ORDER, NO_ORDER, 0, L"A", L"foo", { L"foo", L"bar" } },
			{ NO_ORDER, NO_ORDER, 0, L"C", L"foo", { L"foo", L"baz" } },
		};

		sortRuleAttributes(inp, L"foo");
		CHECK(inp == exp);
	}

	SECTION("nested groups with group order") {
		RuleAttributes inp = {
			{ NO_ORDER, 0, 0, L"C", L"foo", { L"foo", L"baz" } },
			{ NO_ORDER, NO_ORDER, 0, L"A", L"foo", { L"foo", L"bar" } },
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { L"foo" } },
		};

		RuleAttributes exp = {
			{ NO_ORDER, NO_ORDER, 0, L"B", L"foo", { L"foo" } },
			{ NO_ORDER, 0, 0, L"C", L"foo", { L"foo", L"baz" } },
			{ NO_ORDER, NO_ORDER, 0, L"A", L"foo", { L"foo", L"bar" } },
		};

		sortRuleAttributes(inp, L"foo");
		CHECK(inp == exp);
	}

	SECTION("all properties") {
		RuleAttributes inp = {
				{ NO_ORDER, 3, 0, L"B", L"foo", { L"First" } },
				{ NO_ORDER, 0, 0, L"A", L"foo", { L"First1", L"Second1", L"Third1" } },
				{0, 2, 0, L"C", L"foo", { L"First", L"Second" } },
				{ 1, 2, 0, L"D", L"foo", { L"First", L"Second" } },
				{ NO_ORDER, 1, 0, L"E", L"foo", { L"First", L"Second", L"Third" } }
		};
		const RuleAttributes exp = {
				{ NO_ORDER, 0, 0, L"A", L"foo", { L"First1", L"Second1", L"Third1" } },
				{ NO_ORDER, 3, 0, L"B", L"foo", { L"First" } },
				{0, 2, 0, L"C", L"foo", { L"First", L"Second" } },
				{ 1, 2, 0, L"D", L"foo", { L"First", L"Second" } },
				{ NO_ORDER, 1, 0, L"E", L"foo", { L"First", L"Second", L"Third" } }
		};

		sortRuleAttributes(inp, L"foo");
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