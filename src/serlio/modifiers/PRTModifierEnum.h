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

#include "modifiers/RuleAttributes.h"

#include "utils/Utilities.h"

#include "maya/MFnEnumAttribute.h"
#include "maya/MObject.h"
#include "maya/MString.h"

class PRTModifierEnum {
	friend class PRTModifierAction;

public:
	PRTModifierEnum() = default;

	MStatus fill(const prt::Annotation* annot);
	std::pair<bool, short> updateOptions(const MObject& node, const RuleAttributeMap& ruleAttributes,
	                                     const prt::AttributeMap& defaultAttributeValues, short selectedEnumIdx = 0);

	bool isDynamic();

public:
	MFnEnumAttribute mAttr;

private:
	bool mRestricted = true;
	MString mValuesAttr;
	MString mCustomDefaultValue;
	std::vector<MString> mEnumOptions;

	std::vector<MString> getEnumOptions(const RuleAttribute& ruleAttr,
	                                          const prt::AttributeMap& defaultAttributeValues);
	std::vector<MString> getDynamicEnumOptions(const RuleAttribute& ruleAttr,
	                                                 const prt::AttributeMap& defaultAttributeValues);
	void updateCustomEnumValue(const RuleAttribute& ruleAttr, const prt::AttributeMap& defaultAttributeValues);
}; // class PRTModifierEnum
