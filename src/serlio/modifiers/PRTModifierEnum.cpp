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

#include "modifiers/PRTModifierEnum.h"

#include "utils/MayaUtilities.h"

#include "maya/MFnDependencyNode.h"

#include <cassert>

namespace {

constexpr const wchar_t* NULL_KEY = L"#NULL#";
constexpr const wchar_t* RESTRICTED_KEY = L"restricted";
constexpr const wchar_t* VALUES_ATTR_KEY = L"valuesAttr";

void clearEnumValues(const MObject& node, const MFnEnumAttribute& enumAttr) {
	// Workaround: since setting enum values through mel, when none are available causes an Error...
	MString defaultValue;
	if (enumAttr.getDefault(defaultValue) != MStatus::kSuccess)
		return;
	MStatus stat;
	const MFnDependencyNode fNode(node, &stat);
	MCHECK(stat);
	// clear enum options
	MCHECK(mu::setEnumOptions(fNode.name().asWChar(), enumAttr.name().asWChar(), {}));
}

} // namespace

// This function updates all enum options and returns a pair, where the first argument indicates if the options have
// changed and the second argument corresponds to the new index of the currently selected item
std::pair<bool, short> PRTModifierEnum::updateOptions(const MObject& node, const RuleAttributeMap& mRuleAttributes,
                                                      const prt::AttributeMap& defaultAttributeValues,
                                                      short selectedEnumIdx) {
	const MString fullAttrName = mAttr.name();
	const auto ruleAttrIt = mRuleAttributes.find(fullAttrName.asWChar());
	assert(ruleAttrIt != mRuleAttributes.end()); // Rule not found
	const RuleAttribute ruleAttr = (ruleAttrIt != mRuleAttributes.end()) ? ruleAttrIt->second : RuleAttribute();

	const std::vector<MString>& newEnumOptions = getEnumOptions(ruleAttr, defaultAttributeValues);

	updateCustomEnumValue(ruleAttr, defaultAttributeValues);

	MStatus status;
	if ((status == MStatus::kSuccess) && (newEnumOptions == mEnumOptions))
		return std::make_pair(false, selectedEnumIdx);

	const MString oldSelectedOption = mAttr.fieldName(selectedEnumIdx);

	clearEnumValues(node, mAttr);

	mEnumOptions = newEnumOptions;

	int customDefaultIdx = 0;
	int newSelectedEnumIdx = 0;
	int currIdx = 1;

	for (const MString& option : mEnumOptions) {
		MCHECK(mAttr.addField(option, currIdx));
		if (option == mCustomDefaultValue)
			customDefaultIdx = currIdx;
		if (option == oldSelectedOption)
			newSelectedEnumIdx = currIdx;
		currIdx++;
	}

	if (customDefaultIdx == 0)
		MCHECK(mAttr.addField(mCustomDefaultValue, 0));

	return std::make_pair(true, newSelectedEnumIdx);
}

bool PRTModifierEnum::isDynamic() {
	return mValuesAttr.length() > 0;
}

const std::vector<MString> PRTModifierEnum::getEnumOptions(const RuleAttribute& ruleAttr,
                                                           const prt::AttributeMap& defaultAttributeValues) {
	if (isDynamic()) {
		return getDynamicEnumOptions(ruleAttr, defaultAttributeValues);
	}
	else {
		std::vector<MString> enumOptions;

		short minVal;
		short maxVal;
		MCHECK(mAttr.getMin(minVal));
		MCHECK(mAttr.getMax(maxVal));

		for (short currIdx = 1; currIdx <= maxVal; currIdx++)
			enumOptions.emplace_back(mAttr.fieldName(currIdx));

		return enumOptions;
	}
}

void PRTModifierEnum::updateCustomEnumValue(const RuleAttribute& ruleAttr,
                                            const prt::AttributeMap& defaultAttributeValues) {
	const std::wstring fqAttrName = ruleAttr.fqName;
	const prt::AnnotationArgumentType ruleAttrType = ruleAttr.mType;

	MString defMStringVal;

	switch (ruleAttrType) {
		case prt::AAT_STR: {
			const wchar_t* defStringVal = defaultAttributeValues.getString(fqAttrName.c_str());
			if (defStringVal == nullptr)
				return;
			defMStringVal = defStringVal;
			break;
		}
		case prt::AAT_FLOAT: {
			const auto defDoubleVal = defaultAttributeValues.getFloat(fqAttrName.c_str());
			defMStringVal = std::to_wstring(defDoubleVal).c_str();
			break;
		}
		case prt::AAT_BOOL: {
			const auto defBoolVal = defaultAttributeValues.getBool(fqAttrName.c_str());
			defMStringVal = std::to_wstring(defBoolVal).c_str();
			break;
		}
		default: {
			LOG_ERR << "Cannot handle attribute type " << ruleAttrType << " for attr " << fqAttrName;
			return;
		}
	}

	mCustomDefaultValue = defMStringVal;
}

const std::vector<MString> PRTModifierEnum::getDynamicEnumOptions(const RuleAttribute& ruleAttr,
                                                                  const prt::AttributeMap& defaultAttributeValues) {
	std::vector<MString> enumOptions;
	if (!isDynamic())
		return enumOptions;
	const MString fullAttrName = mAttr.name();

	const std::wstring attrStyle = prtu::getStyle(ruleAttr.fqName).c_str();
	std::wstring attrImport = prtu::getImport(ruleAttr.fqName).c_str();
	if (!attrImport.empty())
		attrImport += prtu::IMPORT_DELIMITER;

	std::wstring valuesAttr = attrStyle + prtu::STYLE_DELIMITER + attrImport;
	valuesAttr.append(mValuesAttr.asWChar());

	const prt::Attributable::PrimitiveType type = defaultAttributeValues.getType(valuesAttr.c_str());

	switch (type) {
		case prt::Attributable::PT_STRING_ARRAY: {
			size_t arr_length = 0;
			const wchar_t* const* stringArray = defaultAttributeValues.getStringArray(valuesAttr.c_str(), &arr_length);

			for (short enumIndex = 0; enumIndex < arr_length; enumIndex++) {
				if (stringArray[enumIndex] == nullptr)
					continue;
				std::wstring_view currStringView = stringArray[enumIndex];

				// remove newlines from strings, because they break the maya UI
				const size_t cutoffIndex = currStringView.find_first_of(L"\r\n");
				currStringView = currStringView.substr(0, cutoffIndex);

				const MString currMString(currStringView.data(), static_cast<int>(currStringView.length()));
				enumOptions.emplace_back(currMString);
			}
			break;
		}
		case prt::Attributable::PT_FLOAT_ARRAY: {
			size_t arr_length = 0;
			const double* doubleArray = defaultAttributeValues.getFloatArray(valuesAttr.c_str(), &arr_length);

			for (short enumIndex = 0; enumIndex < arr_length; enumIndex++) {
				const double currDouble = doubleArray[enumIndex];

				const MString currMString(std::to_wstring(currDouble).c_str());
				enumOptions.emplace_back(currMString);
			}
			break;
		}
		case prt::Attributable::PT_BOOL_ARRAY: {
			size_t arr_length = 0;
			const bool* boolArray = defaultAttributeValues.getBoolArray(valuesAttr.c_str(), &arr_length);

			for (short enumIndex = 0; enumIndex < arr_length; enumIndex++) {
				const bool currBool = boolArray[enumIndex];

				const MString currMString(std::to_wstring(currBool).c_str());
				enumOptions.emplace_back(currMString);
			}
			break;
		}
		case prt::Attributable::PT_STRING: {
			const wchar_t* currString = defaultAttributeValues.getString(valuesAttr.c_str());
			if (currString == nullptr)
				break;

			enumOptions.emplace_back(currString);
			break;
		}
		case prt::Attributable::PT_FLOAT: {
			const bool currFloat = defaultAttributeValues.getFloat(valuesAttr.c_str());

			const MString currMString(std::to_wstring(currFloat).c_str());
			enumOptions.emplace_back(currMString);
			break;
		}
		case prt::Attributable::PT_BOOL: {
			const bool currBool = defaultAttributeValues.getBool(valuesAttr.c_str());

			const MString currMString(std::to_wstring(currBool).c_str());
			enumOptions.emplace_back(currMString);
			break;
		}
		default:
			break;
	}
	return enumOptions;
}

MStatus PRTModifierEnum::fill(const prt::Annotation* annot) {
	mRestricted = true;
	MStatus stat;

	uint32_t enumIndex = 1;
	for (size_t arg = 0; arg < annot->getNumArguments(); arg++) {

		const wchar_t* key = annot->getArgument(arg)->getKey();
		if (std::wcscmp(key, NULL_KEY) != 0) {
			if (std::wcscmp(key, RESTRICTED_KEY) == 0)
				mRestricted = annot->getArgument(arg)->getBool();

			if (std::wcscmp(key, VALUES_ATTR_KEY) == 0)
				mValuesAttr = annot->getArgument(arg)->getStr();
			continue;
		}

		switch (annot->getArgument(arg)->getType()) {
			case prt::AAT_BOOL: {
				const bool val = annot->getArgument(arg)->getBool();
				MCHECK(mAttr.addField(MString(std::to_wstring(val).c_str()), enumIndex++));
				break;
			}
			case prt::AAT_FLOAT: {
				const double val = annot->getArgument(arg)->getFloat();
				MCHECK(mAttr.addField(MString(std::to_wstring(val).c_str()), enumIndex++));
				break;
			}
			case prt::AAT_STR: {
				const wchar_t* val = annot->getArgument(arg)->getStr();
				MCHECK(mAttr.addField(MString(val), enumIndex++));
				break;
			}
			default:
				break;
		}
	}

	return MS::kSuccess;
}