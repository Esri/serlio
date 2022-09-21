/**
 * Serlio - Esri CityEngine Plugin for Autodesk Maya
 *
 * See https://github.com/esri/serlio for build and usage instructions.
 *
 * Copyright (c) 2012-2022 Esri R&D Center Zurich
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

#include "modifiers/RuleAttributes.h"

#include "utils/LogHandler.h"
#include "utils/Utilities.h"

#include "prt/Annotation.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr bool DBG = false;

constexpr const wchar_t* PRT_ATTR_FULL_NAME_PREFIX = L"PRT_";

std::wstring getFullName(const std::wstring& fqAttrName, std::map<std::wstring, int>& mayaNameDuplicateCountMap) {
	const std::wstring fullName = PRT_ATTR_FULL_NAME_PREFIX + prtu::cleanNameForMaya(fqAttrName);
	// make sure maya names are unique
	return fullName + prtu::getDuplicateCountSuffix(fullName, mayaNameDuplicateCountMap);
}

std::wstring getBriefName(const std::wstring& fqAttrName, std::map<std::wstring, int>& mayaNameDuplicateCountMap) {
	const std::wstring briefName = prtu::cleanNameForMaya(prtu::removeStyle(fqAttrName));
	// make sure maya names are unique
	return briefName + prtu::getDuplicateCountSuffix(briefName, mayaNameDuplicateCountMap);
}

std::wstring getAttrBaseName(const std::wstring& fqAttrName) {
	return prtu::removeImport(prtu::removeStyle(fqAttrName));
}

std::wstring getNiceName(const std::wstring& fqAttrName) {
	return prtu::cleanNameForMaya(getAttrBaseName(fqAttrName));
}

} // namespace

std::map<std::wstring, int> getImportOrderMap(const prt::RuleFileInfo* ruleFileInfo) {
	std::map<std::wstring, int> importOrderMap;
	int importOrder = 0;
	for (size_t i = 0; i < ruleFileInfo->getNumAnnotations(); i++) {
		const prt::Annotation* an = ruleFileInfo->getAnnotation(i);
		const wchar_t* anName = an->getName();
		if (std::wcscmp(anName, ANNOT_IMPORTS) == 0) {
			for (int argIdx = 0; argIdx < an->getNumArguments(); argIdx++) {
				const prt::AnnotationArgument* anArg = an->getArgument(argIdx);
				if (anArg->getType() == prt::AAT_STR) {
					const wchar_t* anKey = anArg->getKey();
					if (std::wcscmp(anKey, ANNOT_IMPORTS_KEY) == 0) {
						const wchar_t* importRuleCharPtr = anArg->getStr();
						if (importRuleCharPtr != nullptr) {
							std::wstring importRule = importRuleCharPtr;
							importOrderMap[importRule] = importOrder++;
						}
					}
				}
			}
		}
	}
	return importOrderMap;
}

void setGlobalGroupOrder(RuleAttributeVec& ruleAttributes) {
	AttributeGroupOrder globalGroupOrder;
	for (const auto& attribute : ruleAttributes) {
		for (auto it = std::rbegin(attribute.groups); it != std::rend(attribute.groups); ++it) {
			std::vector<std::wstring> g(it, std::rend(attribute.groups));
			std::reverse(g.begin(), g.end());
			auto ggoIt = globalGroupOrder.emplace(std::make_pair(attribute.ruleFile, g), ORDER_NONE).first;
			ggoIt->second = std::min(attribute.groupOrder, ggoIt->second);
		}
	}

	for (auto& attribute : ruleAttributes) {
		const auto it = globalGroupOrder.find(std::make_pair(attribute.ruleFile, attribute.groups));
		attribute.globalGroupOrder = (it != globalGroupOrder.end()) ? it->second : ORDER_NONE;
	}
}

RuleAttributeSet getRuleAttributes(const std::wstring& ruleFile, const prt::RuleFileInfo* ruleFileInfo) {
	RuleAttributeVec ra;

	std::wstring mainCgaRuleName = std::filesystem::path(ruleFile).stem().wstring();

	const std::map<std::wstring, int> importOrderMap = getImportOrderMap(ruleFileInfo);

	std::map<std::wstring, int> mayaNameDuplicateCountMap;

	for (size_t i = 0; i < ruleFileInfo->getNumAttributes(); i++) {
		const prt::RuleFileInfo::Entry* attr = ruleFileInfo->getAttribute(i);

		if (attr->getNumParameters() != 0)
			continue;

		RuleAttribute p;
		p.fqName = attr->getName();
		p.mayaNiceName = getNiceName(p.fqName);
		p.mType = attr->getReturnType();
		p.mayaBriefName = getBriefName(p.fqName, mayaNameDuplicateCountMap);
		p.mayaFullName = getFullName(p.fqName, mayaNameDuplicateCountMap);

		// TODO: is this correct? import name != rule file name
		std::wstring ruleName = p.fqName;
		size_t idxStyle = ruleName.find(L'$');
		if (idxStyle != std::wstring::npos)
			ruleName = ruleName.substr(idxStyle + 1);
		size_t idxDot = ruleName.find_last_of(L'.');
		if (idxDot != std::wstring::npos) {
			p.ruleFile = ruleName.substr(0, idxDot);
		}
		else {
			p.ruleFile = mainCgaRuleName;
			p.memberOfStartRuleFile = true;
		}

		const auto importOrder = importOrderMap.find(p.ruleFile);
		p.ruleOrder = (importOrder != importOrderMap.end()) ? importOrder->second : ORDER_NONE;

		bool hidden = false;
		for (size_t a = 0; a < attr->getNumAnnotations(); a++) {
			const prt::Annotation* an = attr->getAnnotation(a);
			const wchar_t* anName = an->getName();
			if (std::wcscmp(anName, ANNOT_HIDDEN) == 0)
				hidden = true;
			else if (std::wcscmp(anName, ANNOT_ORDER) == 0) {
				if (an->getNumArguments() >= 1 && an->getArgument(0)->getType() == prt::AAT_FLOAT) {
					p.order = static_cast<int>(an->getArgument(0)->getFloat());
				}
			}
			else if (std::wcscmp(anName, ANNOT_GROUP) == 0) {
				for (int argIdx = 0; argIdx < an->getNumArguments(); argIdx++) {
					if (an->getArgument(argIdx)->getType() == prt::AAT_STR) {
						p.groups.push_back(an->getArgument(argIdx)->getStr());
					}
					else if (argIdx == an->getNumArguments() - 1 &&
					         an->getArgument(argIdx)->getType() == prt::AAT_FLOAT) {
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
		if (DBG)
			LOG_DBG << p;
	}

	setGlobalGroupOrder(ra);
	RuleAttributeSet sortedRuleAttributes(ra.begin(), ra.end());

	return sortedRuleAttributes;
}

bool RuleAttributeCmp::operator()(const RuleAttribute& lhs, const RuleAttribute& rhs) const {
	auto compareRuleFile = [&](const RuleAttribute& a, const RuleAttribute& b) {
		// sort main rule attributes before the rest
		if (a.memberOfStartRuleFile && !b.memberOfStartRuleFile)
			return true;
		if (b.memberOfStartRuleFile && !a.memberOfStartRuleFile)
			return false;

		if (a.ruleOrder != b.ruleOrder)
			return a.ruleOrder < b.ruleOrder;

		return a.ruleFile < b.ruleFile;
	};

	auto isChildOf = [](const RuleAttribute& child, const RuleAttribute& parent) {
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

	auto compareGroups = [](const RuleAttribute& a, const RuleAttribute& b) {
		const size_t GroupSizeA = a.groups.size();
		const size_t GroupSizeB = b.groups.size();

		for (size_t i = 0; i < std::max(GroupSizeA, GroupSizeB); ++i) {
			// a descendant of b
			if (i >= GroupSizeA)
				return false;

			// b descendant of a
			if (i >= GroupSizeB)
				return true;

			// difference in groups
			if (a.groups[i] != b.groups[i])
				return a.groups[i] < b.groups[i];
		}
		return false;
	};

	auto compareOrderToGroupOrder = [&](const RuleAttribute& ruleAttrWithGroups,
	                                    const RuleAttribute& ruleAttrWithoutGroups) {
		if ((ruleAttrWithGroups.groups.size() > 0) &&
		    (ruleAttrWithGroups.globalGroupOrder == ruleAttrWithoutGroups.order))
			return ruleAttrWithGroups.groups[0] <= getAttrBaseName(ruleAttrWithoutGroups.fqName);

		return ruleAttrWithGroups.globalGroupOrder < ruleAttrWithoutGroups.order;
	};

	auto compareGroupOrder = [&](const RuleAttribute& a, const RuleAttribute& b) {
		const bool hasGroupsA = !a.groups.empty();
		const bool hasGroupsB = !b.groups.empty();

		if (!hasGroupsB)
			return compareOrderToGroupOrder(a, b);

		if (!hasGroupsA)
			return !compareOrderToGroupOrder(b, a);

		if (isChildOf(a, b))
			return false; // child a should be sorted after parent b

		if (isChildOf(b, a))
			return true; // child b should be sorted after parent a

		const auto globalOrderA = a.globalGroupOrder;
		const auto globalOrderB = b.globalGroupOrder;
		if (globalOrderA != globalOrderB)
			return (globalOrderA < globalOrderB);

		return compareGroups(a, b);
	};

	auto compareAttributeOrder = [&](const RuleAttribute& a, const RuleAttribute& b) {
		if (a.order == b.order)
			return getAttrBaseName(a.fqName) < getAttrBaseName(b.fqName);

		return a.order < b.order;
	};

	auto attributeOrder = [&](const RuleAttribute& a, const RuleAttribute& b) {
		if (a.ruleFile != b.ruleFile)
			return compareRuleFile(a, b);

		if (a.groups != b.groups)
			return compareGroupOrder(a, b);

		return compareAttributeOrder(a, b);
	};

	return attributeOrder(lhs, rhs);
}

std::wostream& operator<<(std::wostream& ostr, const RuleAttribute& ap) {
	auto orderVal = [](int order) { return (order == ORDER_NONE) ? L"none" : std::to_wstring(order); };
	ostr << L"RuleAttribute '" << ap.fqName << L"':" << L" order = " << orderVal(ap.order) << L", groupOrder = "
	     << orderVal(ap.groupOrder) << L", globalGroupOrder = " << orderVal(ap.globalGroupOrder) << L", ruleFile = '"
	     << ap.ruleFile << L"'" << L", groups = [ " << join<wchar_t>(ap.groups, L" ") << L" ]\n";
	return ostr;
}

std::ostream& operator<<(std::ostream& ostr, const RuleAttribute& ap) {
	std::wostringstream wostr;
	wostr << ap;
	ostr << prtu::toOSNarrowFromUTF16(wostr.str());
	return ostr;
}

std::wostream& operator<<(std::wostream& wostr, const AttributeGroupOrder& ago) {
	for (const auto& i : ago) {
		wostr << L"[ " << i.first.first << " " << join<wchar_t>(i.first.second, L" ") << L"] = " << i.second << L"\n";
	}
	return wostr;
}
