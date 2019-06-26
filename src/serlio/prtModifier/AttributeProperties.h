#pragma once

#include "util/Utilities.h"

#include "prt/Annotation.h"

#include <limits>
#include <string>
#include <ostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cassert>
#include <unordered_map>


constexpr int ORDER_FIRST = std::numeric_limits<int>::min();
constexpr int NO_ORDER = std::numeric_limits<int>::max();

struct AttributeProperties {
	int order = NO_ORDER;
	int groupOrder = NO_ORDER;
	size_t index;
	std::wstring name;
	std::wstring ruleFile;
	std::vector<std::wstring> groups; // groups can be nested
	const prt::Annotation* enumAnnotation = nullptr; // TODO: avoid this, leads to lifetime issues
};

using RuleAttributes = std::vector<AttributeProperties>;

void sortRuleAttributes(RuleAttributes& ra, const std::wstring& mainCgaRuleName) {
	auto lowerCaseOrdering = [](std::wstring a, std::wstring b) {
		std::transform(a.begin(), a.end(), a.begin(), ::tolower);
		std::transform(b.begin(), b.end(), b.begin(), ::tolower);
		return a < b;
	};

	auto compareRuleFile = [&](const AttributeProperties& a, const AttributeProperties& b) {
		if (a.ruleFile == mainCgaRuleName)
			return true; // force main rule to be first

		if (b.ruleFile == mainCgaRuleName)
			return false;

		return lowerCaseOrdering(a.ruleFile, b.ruleFile);
	};

	auto isChildOf = [](const AttributeProperties& child, const AttributeProperties& parent) {
		const size_t np = parent.groups.size();
		const size_t nc = child.groups.size();

		// parent path must be shorter
		if (np >= nc)
			return false;

		// parent and child paths must be identical
		for (size_t i = 0; i < np; i++) {
			if (parent.groups[i] != child.groups[i])
				return false;
		}

		return true;
	};

	auto compareGroups = [&](const AttributeProperties& a, const AttributeProperties& b) {
		if (isChildOf(a, b))
			return false; // child a should be sorted after parent b

		if (isChildOf(b, a))
			return true; // child b should be sorted after parent a

		// we're on the same level, check group order property
		if (a.groupOrder == NO_ORDER && b.groupOrder == NO_ORDER) {

			if (a.groups.size() != b.groups.size())
				return (a.groups.size() < b.groups.size()); // sort higher level before lower level

			assert(a.groups.size() == b.groups.size());
			size_t i = 0;
			while ((i < a.groups.size()) && (a.groups[i] == b.groups[i])) { i++; }
			return lowerCaseOrdering(a.groups[i], b.groups[i]);
		}
		return a.groupOrder < b.groupOrder;
	};

	auto compareAttributeOrder = [&](const AttributeProperties& a, const AttributeProperties& b) {
		if (a.order == NO_ORDER && b.order == NO_ORDER)
			return lowerCaseOrdering(a.name, b.name);

		return a.order < b.order;
	};

	auto attributeOrder = [&](const AttributeProperties& a, const AttributeProperties& b) {
		if (a.ruleFile != b.ruleFile)
			return compareRuleFile(a, b);

		if (a.groups != b.groups)
			return compareGroups(a, b);

		return compareAttributeOrder(a, b);
	};

	std::sort(ra.begin(), ra.end(), attributeOrder);
}

std::wostream& operator<<(std::wostream& ostr, const AttributeProperties& ap) {
	auto orderVal = [](int order) {
		std::wostringstream ostr;
		if (order == NO_ORDER)
			ostr << L"none";
		else
			ostr << order;
		return ostr.str();
	};
	ostr << L"AttributeProperties '" << ap.name << L"':"
		 << L" order = " << orderVal(ap.order)
		 << L", groupOrder = " << orderVal(ap.groupOrder)
		 << L", ruleFile = '" << ap.ruleFile << L"'"
		 << L", groups = [ " << join<wchar_t>(ap.groups, L" ") << L" ]\n";
	return ostr;
}

std::ostream& operator<<(std::ostream& ostr, const AttributeProperties& ap) {
	std::wostringstream wostr;
	wostr << ap;
	ostr << prtu::toOSNarrowFromUTF16(wostr.str());
	return ostr;
}
