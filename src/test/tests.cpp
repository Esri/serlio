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

int main( int argc, char* argv[] ) {
	int result = Catch::Session().run( argc, argv );
	return result;
}