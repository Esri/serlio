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

#pragma once

#include "serlioPlugin.h"

#include "prt/Annotation.h"

#include <limits>
#include <map>
#include <string>
#include <vector>

namespace prt {
class RuleFileInfo;
}

constexpr const wchar_t* ANNOT_RANGE = L"@Range";
constexpr const wchar_t* ANNOT_ENUM = L"@Enum";
constexpr const wchar_t* ANNOT_HIDDEN = L"@Hidden";
constexpr const wchar_t* ANNOT_COLOR = L"@Color";
constexpr const wchar_t* ANNOT_DIR = L"@Directory";
constexpr const wchar_t* ANNOT_FILE = L"@File";
constexpr const wchar_t* ANNOT_ORDER = L"@Order";
constexpr const wchar_t* ANNOT_GROUP = L"@Group";

constexpr int ORDER_FIRST = std::numeric_limits<int>::min();
constexpr int ORDER_NONE = std::numeric_limits<int>::max();

using AttributeGroup = std::vector<std::wstring>;
using AttributeGroupOrder = std::map<AttributeGroup, int>;

struct RuleAttribute {
	std::wstring fqName;        // fully qualified rule name (i.e. including style prefix)
	std::wstring mayaBriefName; // see Maya MFnAttribute create() method
	std::wstring mayaFullName;  // "
	std::wstring mayaNiceName;  // see Maya MFnAtribute setNiceNameOverride() method
	prt::AnnotationArgumentType mType;

	AttributeGroup groups; // groups can be nested
	int order = ORDER_NONE;
	int groupOrder = ORDER_NONE;

	std::wstring ruleFile;
	bool memberOfStartRuleFile = false;
};

using RuleAttributes = std::vector<RuleAttribute>;

SRL_TEST_EXPORTS_API RuleAttributes getRuleAttributes(const std::wstring& ruleFile,
                                                      const prt::RuleFileInfo* ruleFileInfo);
AttributeGroupOrder getGlobalGroupOrder(const RuleAttributes& ruleAttributes);
void sortRuleAttributes(RuleAttributes& ra);
std::wostream& operator<<(std::wostream& ostr, const RuleAttribute& ap);
std::ostream& operator<<(std::ostream& ostr, const RuleAttribute& ap);
std::wostream& operator<<(std::wostream& wostr, const AttributeGroupOrder& ago);