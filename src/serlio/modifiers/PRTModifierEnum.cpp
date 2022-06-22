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

#include "modifiers/PRTModifierEnum.h"

#include "utils/MayaUtilities.h"

#include "maya/MFnDependencyNode.h"

#include <cassert>

namespace {

constexpr const wchar_t* NULL_KEY = L"#NULL#";
constexpr const wchar_t* RESTRICTED_KEY = L"restricted";
constexpr const wchar_t* VALUES_ATTR_KEY = L"valuesAttr";

} // namespace

// This function updates all enum options and returns a pair, where the first argument indicates if the options have
// changed and the second argument corresponds to the new index of the currently selected item
short PRTModifierEnum::updateOptions(const MObject& node, const RuleAttributeMap& mRuleAttributes,
                                     const prt::AttributeMap& defaultAttributeValues, short selectedEnumIdx) {
	const MString fullAttrName = mAttr.name();
	const auto ruleAttrIt = mRuleAttributes.find(fullAttrName.asWChar());
	assert(ruleAttrIt != mRuleAttributes.end()); // Rule not found
	const RuleAttribute ruleAttr = (ruleAttrIt != mRuleAttributes.end()) ? ruleAttrIt->second : RuleAttribute();

	const std::vector<std::wstring>& newEnumOptions = getEnumOptions(ruleAttr, defaultAttributeValues);

	const bool hasNewCustomDefaultValue = updateCustomEnumValue(ruleAttr, defaultAttributeValues);

	if ((newEnumOptions == mEnumOptions) && !hasNewCustomDefaultValue)
		return selectedEnumIdx;

	const std::wstring oldSelectedOption = getOptionName(selectedEnumIdx).asWChar();

	mEnumOptions = newEnumOptions;

	int customDefaultIdx = 0;
	int newSelectedEnumIdx = 0;
	int currIdx = 1;

	for (const std::wstring& option : mEnumOptions) {
		if (option == mCustomDefaultValue)
			customDefaultIdx = currIdx;
		if (option == oldSelectedOption)
			newSelectedEnumIdx = currIdx;
		currIdx++;
	}

	if (customDefaultIdx == 0) {
		MCHECK(mu::setEnumOptions(node, mAttr, mEnumOptions, mCustomDefaultValue));
	}
	else {
		MCHECK(mu::setEnumOptions(node, mAttr, mEnumOptions, {}));
	}

	return newSelectedEnumIdx;
}

bool PRTModifierEnum::isDynamic() const {
	return mValuesAttr.length() > 0;
}

size_t PRTModifierEnum::getOptionIndex(const std::wstring& optionName) const {
	const auto iter = std::find(mEnumOptions.begin(), mEnumOptions.end(), optionName);
	// return first index, if element is not available
	if (iter == mEnumOptions.end())
		return 0;
	return std::distance(mEnumOptions.begin(), iter) + 1;
}

MString PRTModifierEnum::getOptionName(size_t optionIndex) const {
	if (optionIndex == 0) {
		return MString(mCustomDefaultValue.c_str());
	}
	else {
		if (optionIndex > mEnumOptions.size())
			return {};
		return MString(mEnumOptions[optionIndex - 1].c_str());
	}
}

short PRTModifierEnum::getDefaultEnumValue(const prt::AttributeMap& defaultAttributeValues,
                                           const RuleAttribute& ruleAttr) const {
	const std::wstring fqAttrName = ruleAttr.fqName;
	const prt::AnnotationArgumentType ruleAttrType = ruleAttr.mType;

	switch (ruleAttrType) {
		case prt::AAT_STR: {
			const wchar_t* defStringVal = defaultAttributeValues.getString(fqAttrName.c_str());

			if (defStringVal != nullptr)
				return static_cast<short>(getOptionIndex(defStringVal));
			break;
		}
		case prt::AAT_FLOAT: {
			const double defDoubleVal = defaultAttributeValues.getFloat(fqAttrName.c_str());
			return static_cast<short>(getOptionIndex(std::to_wstring(defDoubleVal)));
			break;
		}
		case prt::AAT_BOOL: {
			const bool defBoolVal = defaultAttributeValues.getBool(fqAttrName.c_str());
			return static_cast<short>(getOptionIndex(std::to_wstring(defBoolVal)));
			break;
		}
		default:
			LOG_ERR << "Cannot handle attribute type " << ruleAttrType << " for attr " << fqAttrName;
	}
	return 0;
}

std::vector<std::wstring> PRTModifierEnum::getEnumOptions(const RuleAttribute& ruleAttr,
                                                          const prt::AttributeMap& defaultAttributeValues) {
	if (isDynamic()) {
		return getDynamicEnumOptions(ruleAttr, defaultAttributeValues);
	}
	else {
		std::vector<std::wstring> enumOptions;

		short minVal;
		short maxVal;
		MCHECK(mAttr.getMin(minVal));
		MCHECK(mAttr.getMax(maxVal));

		for (short currIdx = 1; currIdx <= maxVal; currIdx++)
			enumOptions.emplace_back(mAttr.fieldName(currIdx).asWChar());

		return enumOptions;
	}
}

bool PRTModifierEnum::updateCustomEnumValue(const RuleAttribute& ruleAttr,
                                            const prt::AttributeMap& defaultAttributeValues) {
	const std::wstring fqAttrName = ruleAttr.fqName;
	const prt::AnnotationArgumentType ruleAttrType = ruleAttr.mType;

	std::wstring defWStringVal;

	switch (ruleAttrType) {
		case prt::AAT_STR: {
			const wchar_t* defStringVal = defaultAttributeValues.getString(fqAttrName.c_str());
			if (defStringVal == nullptr)
				return false;
			defWStringVal = defStringVal;
			break;
		}
		case prt::AAT_FLOAT: {
			const double defDoubleVal = defaultAttributeValues.getFloat(fqAttrName.c_str());
			defWStringVal = std::to_wstring(defDoubleVal).c_str();
			break;
		}
		case prt::AAT_BOOL: {
			const bool defBoolVal = defaultAttributeValues.getBool(fqAttrName.c_str());
			defWStringVal = std::to_wstring(defBoolVal).c_str();
			break;
		}
		default: {
			LOG_ERR << "Cannot handle attribute type " << ruleAttrType << " for attr " << fqAttrName;
			return false;
		}
	}
	if (mCustomDefaultValue == defWStringVal)
		return false;
	mCustomDefaultValue = defWStringVal;
	return true;
}

std::vector<std::wstring> PRTModifierEnum::getDynamicEnumOptions(const RuleAttribute& ruleAttr,
                                                                 const prt::AttributeMap& defaultAttributeValues) {
	std::vector<std::wstring> enumOptions;
	if (!isDynamic())
		return enumOptions;
	const MString fullAttrName = mAttr.name();

	const std::wstring attrStyle = prtu::getStyle(ruleAttr.fqName);
	std::wstring attrImport = prtu::getImport(ruleAttr.fqName);
	if (!attrImport.empty())
		attrImport += prtu::IMPORT_DELIMITER;

	std::wstring valuesAttr = attrStyle + prtu::STYLE_DELIMITER + attrImport;
	valuesAttr.append(mValuesAttr);

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
				enumOptions.emplace_back(currStringView);
			}
			break;
		}
		case prt::Attributable::PT_FLOAT_ARRAY: {
			size_t arr_length = 0;
			const double* doubleArray = defaultAttributeValues.getFloatArray(valuesAttr.c_str(), &arr_length);

			for (short enumIndex = 0; enumIndex < arr_length; enumIndex++) {
				const double currDouble = doubleArray[enumIndex];
				enumOptions.emplace_back(std::to_wstring(currDouble));
			}
			break;
		}
		case prt::Attributable::PT_BOOL_ARRAY: {
			size_t arr_length = 0;
			const bool* boolArray = defaultAttributeValues.getBoolArray(valuesAttr.c_str(), &arr_length);

			for (short enumIndex = 0; enumIndex < arr_length; enumIndex++) {
				const bool currBool = boolArray[enumIndex];
				enumOptions.emplace_back(std::to_wstring(currBool));
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
			const double currFloat = defaultAttributeValues.getFloat(valuesAttr.c_str());
			enumOptions.emplace_back(std::to_wstring(currFloat));
			break;
		}
		case prt::Attributable::PT_BOOL: {
			const bool currBool = defaultAttributeValues.getBool(valuesAttr.c_str());
			enumOptions.emplace_back(std::to_wstring(currBool));
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

			if (std::wcscmp(key, VALUES_ATTR_KEY) == 0) {
				mValuesAttr = annot->getArgument(arg)->getStr();
				// Add empty option, because it is not considered an enum by addAttr in mel otherwise (bug)
				MCHECK(mAttr.addField(" ", 1));
			}
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