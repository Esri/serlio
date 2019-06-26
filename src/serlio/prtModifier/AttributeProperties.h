#pragma once

#include "prt/Annotation.h"

#include <limits>
#include <string>
#include <ostream>
#include <vector>
#include <algorithm>

constexpr int NO_ORDER = std::numeric_limits<int>::max();

struct AttributeProperties {
	int order = NO_ORDER;
	int groupOrder = NO_ORDER;
	size_t index;
	std::wstring name;
	std::wstring ruleFile;
	std::wstring groups;
	const prt::Annotation* enumAnnotation = nullptr; // TODO: avoid this, leads to lifetime issues
};

using RuleAttributes = std::vector<AttributeProperties>;

void sortRuleAttributes(RuleAttributes& ra, const std::wstring& mainCgaRuleName) {
	auto lowerCaseOrdering = [](std::wstring a, std::wstring b) {
		std::transform(a.begin(), a.end(), a.begin(), ::tolower);
		std::transform(b.begin(), b.end(), b.begin(), ::tolower);
		return a < b;
	};

	auto comparator = [&](const AttributeProperties& a, const AttributeProperties& b) {
		if (a.ruleFile != b.ruleFile) {
			if (a.ruleFile == mainCgaRuleName)
				return true; //force main rule to be first
			else if (b.ruleFile == mainCgaRuleName)
				return false;
			return lowerCaseOrdering(a.ruleFile, b.ruleFile);
		}
		else if (a.groups != b.groups) {
			if (a.groupOrder == NO_ORDER && b.groupOrder == NO_ORDER)
				return lowerCaseOrdering(a.groups, b.groups);
			return a.groupOrder < b.groupOrder;
		}
		else {
			if (a.order == NO_ORDER && b.order == NO_ORDER)
				return lowerCaseOrdering(a.name, b.name);
			return a.order < b.order;
		}
	};

	std::sort(ra.begin(), ra.end(), comparator);
}

std::wostream& operator<<(std::wostream& ostr, const AttributeProperties& ap) {
	ostr << L"AttributeProperties '" << ap.name << L"':"
		 << L" order = " << ap.order
		 << L", groupOrder = " << ap.groupOrder
		 << L", ruleFile = '" << ap.ruleFile << L"'"
		 << L", groups = '" << ap.groups << L"'";
	return ostr;
}
