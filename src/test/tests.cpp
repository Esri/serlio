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
	RuleAttributes ra = {
			{ NO_ORDER, 0, 0, L"Default$grouped_A_123", L"foo", L"First1 Second1 Third1 " },
			{ NO_ORDER, 3, 0, L"Default$grouped_B_1", L"foo", L"First " },
			{0, 2, 0, L"Default$grouped_C_12", L"foo", L"First Second " },
			{ 1, 2, 0, L"Default$grouped_D_12", L"foo", L"First Second " },
			{ NO_ORDER, 1, 0, L"Default$grouped_E_123", L"foo", L"First Second Third " }
	};

	sortRuleAttributes(ra, L"foo");

	const RuleAttributes expected = {
			{ NO_ORDER, 0, 0, L"Default$grouped_A_123", L"foo", L"First1 Second1 Third1 " },
			{ NO_ORDER, 3, 0, L"Default$grouped_B_1", L"foo", L"First " },
			{0, 2, 0, L"Default$grouped_C_12", L"foo", L"First Second " },
			{ 1, 2, 0, L"Default$grouped_D_12", L"foo", L"First Second " },
			{ NO_ORDER, 1, 0, L"Default$grouped_E_123", L"foo", L"First Second Third " }
	};

	CHECK(ra == expected);
}

int main( int argc, char* argv[] ) {
	int result = Catch::Session().run( argc, argv );
	return result;
}