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

#include "prt/Annotation.h"

#include <string>
#include <ostream>
#include <vector>
#include <algorithm>
#include <cassert>

namespace {
constexpr bool DBG = false;
}


RuleAttributes getRuleAttributes(const std::wstring& ruleFile, const prt::RuleFileInfo* ruleFileInfo) {
	RuleAttributes ra;

	std::wstring mainCgaRuleName = prtu::filename(ruleFile);
	size_t idxExtension = mainCgaRuleName.find(L".cgb");
	if (idxExtension != std::wstring::npos)
		mainCgaRuleName = mainCgaRuleName.substr(0, idxExtension);

	for (size_t i = 0; i < ruleFileInfo->getNumAttributes(); i++) {

		if (ruleFileInfo->getAttribute(i)->getNumParameters() != 0) continue;

		AttributeProperties p;
		p.index = i;
		p.name = ruleFileInfo->getAttribute(i)->getName();

		std::wstring ruleName = p.name;
		size_t idxStyle = ruleName.find(L"$");
		if (idxStyle != std::wstring::npos)
			ruleName = ruleName.substr(idxStyle + 1);
		size_t idxDot = ruleName.find_last_of(L".");
		if (idxDot != std::wstring::npos) {
			p.ruleFile = ruleName.substr(0, idxDot);
		}
		else {
			p.ruleFile = mainCgaRuleName;
			p.memberOfStartRuleFile = true;
		}

		bool hidden = false;
		for (size_t a = 0; a < ruleFileInfo->getAttribute(i)->getNumAnnotations(); a++) {
			const prt::Annotation* an = ruleFileInfo->getAttribute(i)->getAnnotation(a);
			const wchar_t* anName = an->getName();
			if (!(std::wcscmp(anName, ANNOT_ENUM)))
				p.enumAnnotation = an;
			else if (!(std::wcscmp(anName, ANNOT_HIDDEN)))
				hidden = true;
			else if (!(std::wcscmp(anName, ANNOT_ORDER)))
			{
				if (an->getNumArguments() >= 1 && an->getArgument(0)->getType() == prt::AAT_FLOAT) {
					p.order = static_cast<int>(an->getArgument(0)->getFloat());
				}
			}
			else if (!(std::wcscmp(anName, ANNOT_GROUP)))
			{
				for (int argIdx = 0; argIdx < an->getNumArguments(); argIdx++) {
					if (an->getArgument(argIdx)->getType() == prt::AAT_STR) {
						p.groups.push_back(an->getArgument(argIdx)->getStr());
					}
					else if (argIdx == an->getNumArguments() - 1 && an->getArgument(argIdx)->getType() == prt::AAT_FLOAT) {
						p.groupOrder = static_cast<int>(an->getArgument(argIdx)->getFloat());
					}
				}
			}
		}
		if (hidden)
			continue;

		// no group? put to front
		if (p.groups.empty())
			p.groupOrder = ORDER_FIRST;

		ra.push_back(p);
		if (DBG) LOG_DBG << p;
	}

	return ra;
}

AttributeGroupOrder getGlobalGroupOrder(const RuleAttributes& ruleAttributes) {
	AttributeGroupOrder globalGroupOrder;
	for (const auto& attribute: ruleAttributes) {
		for (auto it = std::rbegin(attribute.groups); it != std::rend(attribute.groups); ++it) {
			std::vector<std::wstring> g(it, std::rend(attribute.groups));
			std::reverse(g.begin(), g.end());
			auto ggoIt = globalGroupOrder.emplace(g, ORDER_NONE).first;
			ggoIt->second = std::min(attribute.groupOrder, ggoIt->second);
		}
	}
	return globalGroupOrder;
}

void sortRuleAttributes(RuleAttributes& ra) {
	auto lowerCaseOrdering = [](std::wstring a, std::wstring b) {
		std::transform(a.begin(), a.end(), a.begin(), ::tolower);
		std::transform(b.begin(), b.end(), b.begin(), ::tolower);
		return a < b;
	};

	auto compareRuleFile = [&](const AttributeProperties& a, const AttributeProperties& b) {
		// sort main rule attributes before the rest
		if (a.memberOfStartRuleFile)
			return true;
		if (b.memberOfStartRuleFile)
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

	auto firstDifferentGroupInA = [](const AttributeProperties& a, const AttributeProperties& b) {
		assert(a.groups.size() == b.groups.size());
		size_t i = 0;
		while ((i < a.groups.size()) && (a.groups[i] == b.groups[i])) { i++; }
		return a.groups[i];
	};

	const AttributeGroupOrder globalGroupOrder = getGlobalGroupOrder(ra);
	if (DBG) LOG_DBG << "globalGroupOrder:\n" << globalGroupOrder;

	auto getGroupOrder = [&globalGroupOrder](const AttributeProperties& ap){
		const auto it = globalGroupOrder.find(ap.groups);
		return (it != globalGroupOrder.end()) ? it->second : ORDER_NONE;
	};

	auto compareGroups = [&](const AttributeProperties& a, const AttributeProperties& b) {
		if (isChildOf(a, b))
			return false; // child a should be sorted after parent b

		if (isChildOf(b, a))
			return true; // child b should be sorted after parent a

		const auto globalOrderA = getGroupOrder(a);
		const auto globalOrderB = getGroupOrder(b);
		if (globalOrderA != globalOrderB)
			return (globalOrderA < globalOrderB);

		// sort higher level before lower level
		if (a.groups.size() != b.groups.size())
			return (a.groups.size() < b.groups.size());

		return lowerCaseOrdering(firstDifferentGroupInA(a, b), firstDifferentGroupInA(b, a));
	};

	auto compareAttributeOrder = [&](const AttributeProperties& a, const AttributeProperties& b) {
		if (a.order == ORDER_NONE && b.order == ORDER_NONE)
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
		if (order == ORDER_NONE)
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

std::wostream& operator<<(std::wostream& wostr, const AttributeGroupOrder& ago) {
	for (const auto& i: ago) {
		wostr << L"[ " << join<wchar_t>(i.first, L" ") << L"] = " << i.second << L"\n";
	}
	return wostr;
}
