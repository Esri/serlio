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

#include "modifiers/PRTModifierAction.h"
#include "modifiers/PRTModifierCommand.h"
#include "modifiers/RuleAttributes.h"

#include "utils/LogHandler.h"
#include "utils/MayaUtilities.h"
#include "utils/Utilities.h"

#include "prt/StringUtils.h"

#include "maya/MDataHandle.h"
#include "maya/MFloatPointArray.h"
#include "maya/MFnCompoundAttribute.h"
#include "maya/MFnMesh.h"
#include "maya/MFnNumericAttribute.h"
#include "maya/MFnStringData.h"
#include "maya/MFnStringArrayData.h"
#include "maya/MFnTypedAttribute.h"
#include "maya/MGlobal.h"

#include <cassert>
#include <variant>

namespace {

constexpr bool DBG = false;

constexpr const wchar_t* ENC_ID_MAYA = L"MayaEncoder";
constexpr const wchar_t* ENC_ID_ATTR_EVAL = L"com.esri.prt.core.AttributeEvalEncoder";
constexpr const wchar_t* ENC_ID_CGA_ERROR = L"com.esri.prt.core.CGAErrorEncoder";
constexpr const wchar_t* ENC_ID_CGA_PRINT = L"com.esri.prt.core.CGAPrintEncoder";
constexpr const wchar_t* FILE_CGA_ERROR = L"CGAErrors.txt";
constexpr const wchar_t* FILE_CGA_PRINT = L"CGAPrint.txt";

constexpr const wchar_t* NULL_KEY = L"#NULL#";
constexpr const wchar_t* MIN_KEY = L"min";
constexpr const wchar_t* MAX_KEY = L"max";
constexpr const wchar_t* RESTRICTED_KEY = L"restricted";
constexpr const wchar_t* VALUES_ATTR_KEY = L"valuesAttr";

constexpr const wchar_t* ATTRIBUTE_USER_SET_SUFFIX = L"_user_set";
constexpr const wchar_t* ATTRIBUTE_FORCE_DEFAULT_SUFFIX = L"_force_default";

const AttributeMapUPtr
        EMPTY_ATTRIBUTES(AttributeMapBuilderUPtr(prt::AttributeMapBuilder::create())->createAttributeMap());

AttributeMapUPtr getDefaultAttributeValues(const std::wstring& ruleFile, const std::wstring& startRule,
                                           const prt::ResolveMap& resolveMap, prt::CacheObject& cache,
                                           const PRTMesh& prtMesh, const int32_t seed,
                                           const prt::AttributeMap& attributeMap) {
	AttributeMapBuilderUPtr mayaCallbacksAttributeBuilder(prt::AttributeMapBuilder::create());
	MayaCallbacks mayaCallbacks(MObject::kNullObj, MObject::kNullObj, mayaCallbacksAttributeBuilder);

	InitialShapeBuilderUPtr isb(prt::InitialShapeBuilder::create());

	isb->setGeometry(prtMesh.vertexCoords(), prtMesh.vcCount(), prtMesh.indices(), prtMesh.indicesCount(),
	                 prtMesh.faceCounts(), prtMesh.faceCountsCount());

	isb->setAttributes(ruleFile.c_str(), startRule.c_str(), seed, L"", &attributeMap, &resolveMap);

	const InitialShapeUPtr shape(isb->createInitialShapeAndReset());
	const InitialShapeNOPtrVector shapes = {shape.get()};

	const std::vector<const wchar_t*> encIDs = {ENC_ID_ATTR_EVAL};
	const AttributeMapUPtr attrEncOpts = prtu::createValidatedOptions(ENC_ID_ATTR_EVAL);
	const AttributeMapNOPtrVector encOpts = {attrEncOpts.get()};
	assert(encIDs.size() == encOpts.size());

	prt::generate(shapes.data(), shapes.size(), nullptr, encIDs.data(), encIDs.size(), encOpts.data(), &mayaCallbacks,
	              &cache, nullptr);

	return AttributeMapUPtr(mayaCallbacksAttributeBuilder->createAttributeMap());
}

bool getIsUserSet(const MFnDependencyNode& node, const MFnAttribute& attribute) {
	MString userSetAttributeName = attribute.name() + ATTRIBUTE_USER_SET_SUFFIX;

	MStatus attrStat;
	MObject userSetAttributeObj = node.attribute(userSetAttributeName, &attrStat);
	if (attrStat == MS::kSuccess) {
		const MPlug plug(node.object(), userSetAttributeObj);
		bool isUserSet = plug.asBool(&attrStat);
		MCHECK(attrStat);
		return isUserSet;
	}
	return false;
}

MStatus setIsUserSet(const MFnDependencyNode& node, const MFnAttribute& attribute, bool value) {
	MString userSetAttributeName = attribute.name() + ATTRIBUTE_USER_SET_SUFFIX;

	MStatus attrStat;
	MObject userSetAttributeObj = node.attribute(userSetAttributeName, &attrStat);
	if (attrStat == MS::kSuccess) {
		MPlug plug(node.object(), userSetAttributeObj);

		MCHECK(plug.setBool(value));
	}
	return attrStat;
}

bool getAndResetForceDefault(const MFnDependencyNode& node, const MFnAttribute& attribute) {
	MString userSetAttributeName = attribute.name() + ATTRIBUTE_FORCE_DEFAULT_SUFFIX;

	MStatus attrStat;
	MObject userSetAttributeObj = node.attribute(userSetAttributeName, &attrStat);
	if (attrStat == MS::kSuccess) {
		MPlug plug(node.object(), userSetAttributeObj);
		bool isUserSet = plug.asBool(&attrStat);
		MCHECK(attrStat);
		MCHECK(plug.setBool(false));
		return isUserSet;
	}
	return false;
}

std::wstring removeSuffix(std::wstring const& fullString) {
	for (const std::wstring suffix : {ATTRIBUTE_USER_SET_SUFFIX, ATTRIBUTE_FORCE_DEFAULT_SUFFIX}) {
		if ((fullString.length() >= suffix.length()) &&
		    (fullString.compare(fullString.length() - suffix.length(), suffix.length(), suffix) == 0))
			return fullString.substr(0, fullString.length() - suffix.length());
	}
	return fullString;
}

enum class PrtAttributeType { BOOL, FLOAT, COLOR, STRING, ENUM };

std::list<MObject> getNodeAttributesCorrespondingToCGA(const MFnDependencyNode& node) {
	std::list<MObject> rawAttrs;
	std::list<MObject> ignoreList;

	for (unsigned int i = 0, numAttrs = node.attributeCount(); i < numAttrs; i++) {
		MStatus attrStat;
		const MObject attrObj = node.attribute(i, &attrStat);
		if (attrStat != MS::kSuccess)
			continue;

		const MFnAttribute attr(attrObj);

		// CGA rule attributes are maya dynamic attributes and not hidden
		if (!attr.isDynamic() || attr.isHidden())
			continue;

		// maya annoyance: color attributes automatically get per-component plugs/child attrs
		if (attr.isUsedAsColor()) {
			MFnCompoundAttribute compAttr(attrObj);
			for (unsigned int ci = 0, numChildren = compAttr.numChildren(); ci < numChildren; ci++)
				ignoreList.emplace_back(compAttr.child(ci));
		}

		rawAttrs.push_back(attrObj);
	}

	const auto isIgnored = [&ignoreList](const auto& attr) {
		return std::find(ignoreList.begin(), ignoreList.end(), attr) != ignoreList.end();
	};
	rawAttrs.erase(std::remove_if(rawAttrs.begin(), rawAttrs.end(), isIgnored), rawAttrs.end());

	return rawAttrs;
}

template <typename T>
MStatus iterateThroughAttributesAndApply(const MObject& node, const RuleAttributeMap& ruleAttributes, T attrFunction) {
	MStatus stat;
	const MFnDependencyNode fNode(node, &stat);
	MCHECK(stat);

	const std::list<MObject> cgaAttributes = getNodeAttributesCorrespondingToCGA(fNode);

	for (const auto& attrObj : cgaAttributes) {
		MFnAttribute fnAttr(attrObj);

		const MString fullAttrName = fnAttr.name();
		auto ruleAttrIt = ruleAttributes.find(fullAttrName.asWChar());
		assert(ruleAttrIt != ruleAttributes.end()); // Rule not found
		const RuleAttribute ruleAttr = (ruleAttrIt != ruleAttributes.end()) ? ruleAttrIt->second : RuleAttribute();

		[[maybe_unused]] const auto ruleAttrType = ruleAttr.mType;

		if (attrObj.hasFn(MFn::kNumericAttribute)) {
			MFnNumericAttribute nAttr(attrObj);

			if (nAttr.unitType() == MFnNumericData::kBoolean) {
				assert(ruleAttrType == prt::AAT_BOOL);

				attrFunction(fNode, fnAttr, ruleAttr, PrtAttributeType::BOOL);
			}
			else if (nAttr.unitType() == MFnNumericData::kDouble) {
				assert(ruleAttrType == prt::AAT_FLOAT);

				attrFunction(fNode, fnAttr, ruleAttr, PrtAttributeType::FLOAT);
			}
			else if (nAttr.isUsedAsColor()) {
				assert(ruleAttrType == prt::AAT_STR);

				attrFunction(fNode, fnAttr, ruleAttr, PrtAttributeType::COLOR);
			}
		}
		else if (attrObj.hasFn(MFn::kTypedAttribute)) {
			assert(ruleAttrType == prt::AAT_STR);

			attrFunction(fNode, fnAttr, ruleAttr, PrtAttributeType::STRING);
		}
		else if (attrObj.hasFn(MFn::kEnumAttribute)) {
			attrFunction(fNode, fnAttr, ruleAttr, PrtAttributeType::ENUM);
		}
	}
	return MStatus::kSuccess;
}

MStatus addHiddenBoolParameter(MFnDependencyNode& node, MFnAttribute& tAttr, const MString& suffix) {
	MStatus stat;

	MFnNumericAttribute nAttr;
	MObject attr =
	        nAttr.create(tAttr.name() + suffix, tAttr.shortName() + suffix, MFnNumericData::kBoolean, false, &stat);
	MCHECK(stat);

	if (!(node.hasAttribute(nAttr.shortName()))) {
		MCHECK(nAttr.setHidden(true));
		MCHECK(nAttr.setStorable(true));
		stat = node.addAttribute(attr);
	}
	return stat;
}

short getDefaultEnumValue(const prt::AttributeMap& defaultAttributeValues, const MFnEnumAttribute& eAttr,
                          const RuleAttribute& ruleAttr) {
	const std::wstring fqAttrName = ruleAttr.fqName;
	const prt::AnnotationArgumentType ruleAttrType = ruleAttr.mType;

	short minVal;
	short maxVal;
	MCHECK(eAttr.getMin(minVal));
	MCHECK(eAttr.getMax(maxVal));

	switch (ruleAttrType) {
		case prt::AAT_STR: {
			const wchar_t* defStringVal = defaultAttributeValues.getString(fqAttrName.c_str());

			for (short currIdx = minVal; currIdx <= maxVal; currIdx++) {
				const wchar_t* currString = eAttr.fieldName(currIdx).asWChar();
				if (std::wcscmp(currString, defStringVal) == 0)
					return currIdx;
			}
			break;
		}
		case prt::AAT_FLOAT: {
			const auto defDoubleVal = defaultAttributeValues.getFloat(fqAttrName.c_str());

			for (short currIdx = minVal; currIdx <= maxVal; currIdx++) {
				const double currDouble = eAttr.fieldName(currIdx).asDouble();
				if (currDouble == defDoubleVal)
					return currIdx;
			}
			break;
		}
		case prt::AAT_BOOL: {
			const auto defBoolVal = defaultAttributeValues.getBool(fqAttrName.c_str());

			for (short currIdx = minVal; currIdx <= maxVal; currIdx++) {
				const bool currBool = (eAttr.fieldName(currIdx).asInt() != 0);
				if (currBool == defBoolVal)
					return currIdx;
			}
			break;
		}
		default:
			LOG_ERR << "Cannot handle attribute type " << ruleAttrType << " for attr " << fqAttrName;
	}
	return 0;
}

using PRTEnumDefaultValue = std::variant<bool, double, MString>;

short getDefaultEnumIdx(const prt::Annotation* annot, const PRTEnumDefaultValue& defaultValue) {
	short idx = 0;
	for (size_t arg = 0; arg < annot->getNumArguments(); arg++) {

		const wchar_t* key = annot->getArgument(arg)->getKey();
		if (std::wcscmp(key, NULL_KEY) != 0) {
			continue;
		}

		switch (annot->getArgument(arg)->getType()) {
			case prt::AAT_BOOL: {
				const bool val = annot->getArgument(arg)->getBool();
				if (val == std::get<bool>(defaultValue))
					return idx;
				idx++;
				break;
			}
			case prt::AAT_FLOAT: {
				const double val = annot->getArgument(arg)->getFloat();
				if (val == std::get<double>(defaultValue))
					return idx;
				idx++;
				break;
			}
			case prt::AAT_STR: {
				const MString val = annot->getArgument(arg)->getStr();
				if (val == std::get<MString>(defaultValue))
					return idx;
				idx++;
				break;
			}
			default:
				break;
		}
	}
	return 0;
}

bool cgacLogProblems(CGACErrors errorList) {
	for (const auto& [error, count] : errorList) {
		if (error.shouldBeLogged) {
			if (error.errorLevel == prt::CGAErrorLevel::CGAERROR)
				MGlobal::displayError(error.errorString.c_str());
			else
				MGlobal::displayWarning(error.errorString.c_str());
		}
	}
	return false;
}

MStringArray cgacProblemsToStringArray(CGACErrors errorList) {
	MStringArray errorStringArrray;
	for (const auto& [error, count] : errorList) {
		errorStringArrray.append(std::to_wstring(count).c_str());
		const MString errorLevel = (error.errorLevel == prt::CGAErrorLevel::CGAERROR) ? "Error" : "Warning";
		errorStringArrray.append(errorLevel);
		errorStringArrray.append(error.errorString.c_str());
	}
	return errorStringArrray;
}

void updateCgacProblemData(const CGACErrors& cgacProblems, MDataHandle& cgacProblemData) {
	MStringArray newCgacErrorStringArray = cgacProblemsToStringArray(cgacProblems);

	MObject errorDataObject = cgacProblemData.data();
	MFnStringArrayData stringArrayData(errorDataObject);
	MStringArray oldCgacErrorStringArray = stringArrayData.array();

	if (!mu::mStringArraysAreEqual(oldCgacErrorStringArray, newCgacErrorStringArray)) {
		cgacLogProblems(cgacProblems);

		MFnStringArrayData newStringArrayData;
		MObject newErrorDataObject = newStringArrayData.create(newCgacErrorStringArray);
		cgacProblemData.setMObject(newErrorDataObject);
	}
}

template <typename T>
T getPlugValueAndRemoveAttr(MFnDependencyNode& node, const MString& briefName, const T& defaultValue) {
	T plugValue = defaultValue;

	if (DBG) {
		LOG_DBG << "node attrs:";
		mu::forAllAttributes(node, [&node](const MFnAttribute& a) {
			MString val;
			node.findPlug(a.object(), true).getValue(val);
			LOG_DBG << a.name().asWChar() << " = " << val.asWChar();
		});
	}

	if (node.hasAttribute(briefName)) {
		const MPlug plug = node.findPlug(briefName, true);
		if (plug.isDynamic()) {
			T d;
			if (plug.getValue(d) == MS::kSuccess)
				plugValue = d;
		}
		node.removeAttribute(node.attribute(briefName));
	}

	return plugValue;
}
} // namespace

PRTModifierAction::PRTModifierAction() {
	AttributeMapBuilderUPtr optionsBuilder(prt::AttributeMapBuilder::create());

	mMayaEncOpts = prtu::createValidatedOptions(ENC_ID_MAYA);

	optionsBuilder->setString(L"name", FILE_CGA_ERROR);
	const AttributeMapUPtr errOptions(optionsBuilder->createAttributeMapAndReset());
	mCGAErrorOptions = prtu::createValidatedOptions(ENC_ID_CGA_ERROR, errOptions.get());

	optionsBuilder->setString(L"name", FILE_CGA_PRINT);
	const AttributeMapUPtr printOptions(optionsBuilder->createAttributeMapAndReset());
	mCGAPrintOptions = prtu::createValidatedOptions(ENC_ID_CGA_PRINT, printOptions.get());
}

MStatus PRTModifierAction::fillAttributesFromNode(const MObject& node) {
	AttributeMapBuilderSPtr aBuilder(prt::AttributeMapBuilder::create(), PRTDestroyer());

	const auto fillAttributeFromNode = [this, aBuilder](const MFnDependencyNode& fnNode,
	                                                    const MFnAttribute& fnAttribute,
	                                                    const RuleAttribute& ruleAttribute,
	                                                    const PrtAttributeType attrType) mutable {
		MPlug plug(fnNode.object(), fnAttribute.object());
		const std::wstring fqAttrName = ruleAttribute.fqName;
		const prt::AnnotationArgumentType ruleAttrType = ruleAttribute.mType;

		switch (attrType) {
			case PrtAttributeType::BOOL: {
				bool boolVal;
				MCHECK(plug.getValue(boolVal));

				if (getIsUserSet(fnNode, fnAttribute))
					aBuilder->setBool(fqAttrName.c_str(), boolVal);
				break;
			}
			case PrtAttributeType::FLOAT: {
				double doubleVal;
				MCHECK(plug.getValue(doubleVal));

				if (getIsUserSet(fnNode, fnAttribute))
					aBuilder->setFloat(fqAttrName.c_str(), doubleVal);
				break;
			}
			case PrtAttributeType::COLOR: {
				MObject rgb;
				MCHECK(plug.getValue(rgb));
				MFnNumericData fRGB(rgb);

				prtu::Color col;
				MCHECK(fRGB.getData3Float(col[0], col[1], col[2]));
				const std::wstring colStr = prtu::getColorString(col);

				if (getIsUserSet(fnNode, fnAttribute))
					aBuilder->setString(fqAttrName.c_str(), colStr.c_str());
				break;
			}
			case PrtAttributeType::STRING: {
				MString stringVal;
				MCHECK(plug.getValue(stringVal));

				if (getIsUserSet(fnNode, fnAttribute))
					aBuilder->setString(fqAttrName.c_str(), stringVal.asWChar());
				break;
			}
			case PrtAttributeType::ENUM: {
				MFnEnumAttribute eAttr(fnAttribute.object());

				short enumVal;
				MCHECK(plug.getValue(enumVal));

				if (getIsUserSet(fnNode, fnAttribute)) {
					switch (ruleAttrType) {
						case prt::AAT_STR:
							aBuilder->setString(fqAttrName.c_str(), eAttr.fieldName(enumVal).asWChar());
							break;
						case prt::AAT_FLOAT:
							aBuilder->setFloat(fqAttrName.c_str(), eAttr.fieldName(enumVal).asDouble());
							break;
						case prt::AAT_BOOL:
							aBuilder->setBool(fqAttrName.c_str(), eAttr.fieldName(enumVal).asInt() != 0);
							break;
						default:
							LOG_ERR << "Cannot handle attribute type " << ruleAttrType << " for attr " << fqAttrName;
					}
				}
				break;
			}

			default:
				break;
		}
	};

	iterateThroughAttributesAndApply(node, mRuleAttributes, fillAttributeFromNode);
	mGenerateAttrs.reset(aBuilder->createAttributeMap());

	return MStatus::kSuccess;
}

MStatus PRTModifierAction::updateUserSetAttributes(const MObject& node) {
	const auto updateUserSetAttribute = [this](const MFnDependencyNode& fnNode, const MFnAttribute& fnAttribute,
	                                           const RuleAttribute& ruleAttribute, const PrtAttributeType attrType) {
		const AttributeMapUPtr defaultAttributeValues =
		        getDefaultAttributeValues(mRuleFile, mStartRule, *getResolveMap(), *PRTContext::get().mPRTCache,
		                                  *inPrtMesh, mRandomSeed, *mGenerateAttrs);

		if (getAndResetForceDefault(fnNode, fnAttribute)) {
			setIsUserSet(fnNode, fnAttribute, false);
			return;
		}

		const MPlug plug(fnNode.object(), fnAttribute.object());
		bool isDefaultValue = false;
		const std::wstring fqAttrName = ruleAttribute.fqName;

		switch (attrType) {
			case PrtAttributeType::BOOL: {
				const auto defBoolVal = defaultAttributeValues->getBool(fqAttrName.c_str());
				bool boolVal;
				MCHECK(plug.getValue(boolVal));

				isDefaultValue = (defBoolVal == boolVal);
				break;
			}
			case PrtAttributeType::FLOAT: {
				const auto defDoubleVal = defaultAttributeValues->getFloat(fqAttrName.c_str());
				double doubleVal;
				MCHECK(plug.getValue(doubleVal));

				isDefaultValue = (defDoubleVal == doubleVal);
				break;
			}
			case PrtAttributeType::COLOR: {
				const prtu::Color defCol = prtu::parseColor(defaultAttributeValues->getString(fqAttrName.c_str()));

				MObject rgb;
				MCHECK(plug.getValue(rgb));
				MFnNumericData fRGB(rgb);

				prtu::Color col;
				MCHECK(fRGB.getData3Float(col[0], col[1], col[2]));

				isDefaultValue = (defCol == col);
				break;
			}
			case PrtAttributeType::STRING: {
				const auto defStringVal = defaultAttributeValues->getString(fqAttrName.c_str());

				MString stringVal;
				MCHECK(plug.getValue(stringVal));

				isDefaultValue = (std::wcscmp(stringVal.asWChar(), defStringVal) == 0);
				break;
			}
			case PrtAttributeType::ENUM: {
				MFnEnumAttribute eAttr(fnAttribute.object());

				const short defEnumVal = getDefaultEnumValue(*defaultAttributeValues, eAttr, ruleAttribute);
				short enumVal;
				MCHECK(plug.getValue(enumVal));

				isDefaultValue = (defEnumVal == enumVal);
				break;
			}

			default:
				break;
		}

		if (!isDefaultValue)
			setIsUserSet(fnNode, fnAttribute, true);
	};

	iterateThroughAttributesAndApply(node, mRuleAttributes, updateUserSetAttribute);

	return MStatus::kSuccess;
}

MStatus PRTModifierAction::updateUI(const MObject& node, MDataHandle& cgacProblemData) {
	const auto updateUIFromAttributes = [this](const MFnDependencyNode& fnNode, const MFnAttribute& fnAttribute,
	                                           const RuleAttribute& ruleAttribute, const PrtAttributeType attrType) {
		const AttributeMapUPtr defaultAttributeValues =
		        getDefaultAttributeValues(mRuleFile, mStartRule, *getResolveMap(), *PRTContext::get().mPRTCache,
		                                  *inPrtMesh, mRandomSeed, *mGenerateAttrs);
		MPlug plug(fnNode.object(), fnAttribute.object());
		const std::wstring fqAttrName = ruleAttribute.fqName;

		switch (attrType) {
			case PrtAttributeType::BOOL: {
				const auto defBoolVal = defaultAttributeValues->getBool(fqAttrName.c_str());
				bool boolVal;
				MCHECK(plug.getValue(boolVal));

				const bool isDefaultValue = (defBoolVal == boolVal);

				if (!getIsUserSet(fnNode, fnAttribute) && !isDefaultValue) {
					plug.setBool(defBoolVal);
				}
				break;
			}
			case PrtAttributeType::FLOAT: {
				const auto defDoubleVal = defaultAttributeValues->getFloat(fqAttrName.c_str());
				double doubleVal;
				MCHECK(plug.getValue(doubleVal));

				const bool isDefaultValue = (defDoubleVal == doubleVal);

				if (!getIsUserSet(fnNode, fnAttribute) && !isDefaultValue) {
					plug.setDouble(defDoubleVal);
				}
				break;
			}
			case PrtAttributeType::COLOR: {
				const wchar_t* defColStr = defaultAttributeValues->getString(fqAttrName.c_str());

				MObject rgb;
				MCHECK(plug.getValue(rgb));
				MFnNumericData fRGB(rgb);

				prtu::Color col;
				MCHECK(fRGB.getData3Float(col[0], col[1], col[2]));
				const std::wstring colStr = prtu::getColorString(col);

				prtu::Color defaultColor = prtu::parseColor(defColStr);
				MFnNumericData fdefaultColor;
				MObject defaultColorObj = fdefaultColor.create(MFnNumericData::Type::k3Float);
				fdefaultColor.setData3Float(defaultColor[0], defaultColor[1], defaultColor[2]);

				const bool isDefaultValue = (std::wcscmp(colStr.c_str(), defColStr) == 0);

				if (!getIsUserSet(fnNode, fnAttribute) && !isDefaultValue) {
					plug.setMObject(defaultColorObj);
				}
				break;
			}
			case PrtAttributeType::STRING: {
				const auto defStringVal = defaultAttributeValues->getString(fqAttrName.c_str());

				MString stringVal;
				MCHECK(plug.getValue(stringVal));

				const bool isDefaultValue = (std::wcscmp(stringVal.asWChar(), defStringVal) == 0);

				if (!getIsUserSet(fnNode, fnAttribute) && !isDefaultValue) {
					plug.setString(defStringVal);
				}
				break;
			}
			case PrtAttributeType::ENUM: {
				MFnEnumAttribute eAttr(fnAttribute.object());

				const short defEnumVal = getDefaultEnumValue(*defaultAttributeValues, eAttr, ruleAttribute);
				short enumVal;
				MCHECK(plug.getValue(enumVal));

				const bool isDefaultValue = (defEnumVal == enumVal);

				if (!getIsUserSet(fnNode, fnAttribute) && !isDefaultValue) {
					plug.setShort(defEnumVal);
				}
				break;
			}

			default:
				break;
		}
	};

	updateCgacProblemData(mCGACProblems, cgacProblemData);
	iterateThroughAttributesAndApply(node, mRuleAttributes, updateUIFromAttributes);

	return MStatus::kSuccess;
}

void PRTModifierAction::updateDynamicEnums() {
	for (auto& e : mEnums) {
		if (e.mValuesAttr.length() == 0)
			continue;

		const MString fullAttrName = e.mAttr.name();
		const auto ruleAttrIt = mRuleAttributes.find(fullAttrName.asWChar());
		assert(ruleAttrIt != ruleAttributes.end()); // Rule not found
		const RuleAttribute ruleAttr = (ruleAttrIt != mRuleAttributes.end()) ? ruleAttrIt->second : RuleAttribute();

		const std::wstring attrStyle = prtu::getStyle(ruleAttr.fqName).c_str();
		std::wstring attrImport = prtu::getImport(ruleAttr.fqName).c_str();
		if (!attrImport.empty())
			attrImport += prtu::IMPORT_DELIMITER;

		std::wstring valuesAttr = attrStyle + prtu::STYLE_DELIMITER + attrImport;
		valuesAttr.append(e.mValuesAttr.asWChar());

		const prt::Attributable::PrimitiveType type = mGenerateAttrs->getType(valuesAttr.c_str());

		switch (type) {
			case prt::Attributable::PT_STRING_ARRAY: {
				size_t arr_length = 0;
				const wchar_t* const* stringArray = mGenerateAttrs->getStringArray(valuesAttr.c_str(), &arr_length);

				for (short enumIndex = 0; enumIndex < arr_length; enumIndex++) {
					if (stringArray[enumIndex] == nullptr)
						continue;
					std::wstring_view currStringView = stringArray[enumIndex];

					// remove newlines from strings, because they break the maya UI
					const size_t cutoffIndex = currStringView.find_first_of(L"\r\n");
					currStringView = currStringView.substr(0, cutoffIndex);

					const MString mCurrString(currStringView.data(), static_cast<int>(currStringView.length()));
					MCHECK(e.mAttr.addField(mCurrString, enumIndex));
				}
				break;
			}
			case prt::Attributable::PT_FLOAT_ARRAY: {
				size_t arr_length = 0;
				const double* doubleArray = mGenerateAttrs->getFloatArray(valuesAttr.c_str(), &arr_length);

				for (short enumIndex = 0; enumIndex < arr_length; enumIndex++) {
					const double currDouble = doubleArray[enumIndex];

					const MString mCurrString(std::to_wstring(currDouble).c_str());
					MCHECK(e.mAttr.addField(mCurrString, enumIndex));
				}
				break;
			}
			case prt::Attributable::PT_BOOL_ARRAY: {
				size_t arr_length = 0;
				const bool* boolArray = mGenerateAttrs->getBoolArray(valuesAttr.c_str(), &arr_length);

				for (short enumIndex = 0; enumIndex < arr_length; enumIndex++) {
					const bool currBool = boolArray[enumIndex];

					const MString mCurrString(std::to_wstring(currBool).c_str());
					MCHECK(e.mAttr.addField(mCurrString, enumIndex));
				}
				break;
			}
			case prt::Attributable::PT_STRING: {
				const MString mCurrString = mGenerateAttrs->getString(valuesAttr.c_str());

				MCHECK(e.mAttr.addField(mCurrString, 0));
				break;
			}
			case prt::Attributable::PT_FLOAT: {
				const bool currFloat = mGenerateAttrs->getFloat(valuesAttr.c_str());

				const MString mCurrString(std::to_wstring(currFloat).c_str());
				MCHECK(e.mAttr.addField(mCurrString, 0));
				break;
			}
			case prt::Attributable::PT_BOOL: {
				const bool currBool = mGenerateAttrs->getBool(valuesAttr.c_str());

				const MString mCurrString(std::to_wstring(currBool).c_str());
				MCHECK(e.mAttr.addField(mCurrString, 0));
				break;
			}
			default:
				break;
		}
	}
}

// Sets the mesh object for the action  to operate on
void PRTModifierAction::setMesh(MObject& _inMesh, MObject& _outMesh) {
	inMesh = _inMesh;
	outMesh = _outMesh;

	inPrtMesh = std::make_unique<PRTMesh>(_inMesh);
}

ResolveMapSPtr PRTModifierAction::getResolveMap() {
	ResolveMapCache::LookupResult lookupResult =
	        PRTContext::get().mResolveMapCache->get(std::wstring(mRulePkg.asWChar()));
	ResolveMapSPtr resolveMap = lookupResult.first;
	return resolveMap;
}

MStatus PRTModifierAction::updateRuleFiles(const MObject& node, const MString& rulePkg) {
	mRulePkg = rulePkg;

	mEnums.clear();
	mRuleFile.clear();
	mStartRule.clear();
	mRuleAttributes.clear();
	PRTContext::get().mPRTCache.get()->flushAll();

	ResolveMapSPtr resolveMap = getResolveMap();
	if (!resolveMap) {
		LOG_ERR << "failed to get resolve map from rule package " << mRulePkg.asWChar();
		return MS::kFailure;
	}

	mRuleFile = prtu::getRuleFileEntry(resolveMap);
	if (mRuleFile.empty()) {
		LOG_ERR << "could not find rule file in rule package " << mRulePkg.asWChar();
		return MS::kFailure;
	}

	prt::Status infoStatus = prt::STATUS_UNSPECIFIED_ERROR;
	const wchar_t* ruleFileURI = resolveMap->getString(mRuleFile.c_str());
	if (ruleFileURI == nullptr) {
		LOG_ERR << "could not find rule file URI in resolve map of rule package " << mRulePkg.asWChar();
		return MS::kFailure;
	}

	RuleFileInfoUPtr info(prt::createRuleFileInfo(ruleFileURI, PRTContext::get().mPRTCache.get(), &infoStatus));
	if (!info || infoStatus != prt::STATUS_OK) {
		LOG_ERR << "could not get rule file info from rule file " << mRuleFile;
		return MS::kFailure;
	}

	mStartRule = prtu::detectStartRule(info);
	mGenerateAttrs = getDefaultAttributeValues(mRuleFile, mStartRule, *getResolveMap(), *PRTContext::get().mPRTCache,
	                                           *inPrtMesh, mRandomSeed, *EMPTY_ATTRIBUTES);
	if (DBG)
		LOG_DBG << "default attrs: " << prtu::objectToXML(mGenerateAttrs);

	if (node != MObject::kNullObj) {
		// derive necessary data from PRT rule info to populate node with dynamic rule attributes
		RuleAttributeSet ruleAttributes = getRuleAttributes(mRuleFile, info.get());
		for (const RuleAttribute& ruleAttr : ruleAttributes) {
			mRuleAttributes[ruleAttr.mayaFullName] = ruleAttr;
		}

		createNodeAttributes(ruleAttributes, node, info.get());
		updateDynamicEnums();
	}

	return MS::kSuccess;
}

MStatus PRTModifierAction::doIt() {
	MStatus status;

	AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
	std::unique_ptr<MayaCallbacks> outputHandler(new MayaCallbacks(inMesh, outMesh, amb));

	InitialShapeBuilderUPtr isb(prt::InitialShapeBuilder::create());
	const prt::Status setGeoStatus =
	        isb->setGeometry(inPrtMesh->vertexCoords(), inPrtMesh->vcCount(), inPrtMesh->indices(),
	                         inPrtMesh->indicesCount(), inPrtMesh->faceCounts(), inPrtMesh->faceCountsCount());
	if (setGeoStatus != prt::STATUS_OK)
		LOG_ERR << "InitialShapeBuilder setGeometry failed status = " << prt::getStatusDescription(setGeoStatus);

	isb->setAttributes(mRuleFile.c_str(), mStartRule.c_str(), mRandomSeed, L"", mGenerateAttrs.get(),
	                   getResolveMap().get());

	std::unique_ptr<const prt::InitialShape, PRTDestroyer> shape(isb->createInitialShapeAndReset());

	const std::vector<const wchar_t*> encIDs = {ENC_ID_MAYA, ENC_ID_CGA_ERROR, ENC_ID_CGA_PRINT};
	const AttributeMapNOPtrVector encOpts = {mMayaEncOpts.get(), mCGAErrorOptions.get(), mCGAPrintOptions.get()};
	assert(encIDs.size() == encOpts.size());

	InitialShapeNOPtrVector shapes = {shape.get()};
	const prt::Status generateStatus =
	        prt::generate(shapes.data(), shapes.size(), nullptr, encIDs.data(), encIDs.size(), encOpts.data(),
	                      outputHandler.get(), PRTContext::get().mPRTCache.get(), nullptr);

	mCGACProblems = outputHandler->getCGACErrors();

	if (generateStatus != prt::STATUS_OK) {
		std::string generateFailedMessage = "prt generate failed: ";
		generateFailedMessage.append(prt::getStatusDescription(generateStatus));

		LOG_ERR << generateFailedMessage;
		MGlobal::displayError(generateFailedMessage.c_str());
	}

	return status;
}

MStatus PRTModifierAction::createNodeAttributes(const RuleAttributeSet& ruleAttributes, const MObject& nodeObj,
                                                const prt::RuleFileInfo* info) {
	MStatus stat;
	MFnDependencyNode node(nodeObj, &stat);
	MCHECK(stat);

	for (const RuleAttribute& p : ruleAttributes) {
		const std::wstring fqName = p.fqName;

		// only use attributes of current style
		const std::wstring style = prtu::getStyle(fqName);
		if (style != mRuleStyle)
			continue;

		const prt::Attributable::PrimitiveType attrType = mGenerateAttrs->getType(fqName.c_str());

		// TMP: detect attribute traits based on rule info / annotations
		enum class AttributeTrait { ENUM, RANGE, FILE, DIR, COLOR, PLAIN };
		struct AttributeTraitPayload { // intermediate representation of annotation data, poor man's std::variant
			std::wstring mString;
			const prt::Annotation* mAnnot = nullptr; // TODO: just while refactoring...
		};

		auto detectAttributeTrait =
		        [&info](const std::wstring& key) -> std::pair<AttributeTrait, AttributeTraitPayload> {
			for (size_t ai = 0, numAttrs = info->getNumAttributes(); ai < numAttrs; ai++) {
				const auto* attr = info->getAttribute(ai);
				if (std::wcscmp(key.c_str(), attr->getName()) != 0)
					continue;

				for (size_t a = 0; a < attr->getNumAnnotations(); a++) {
					const prt::Annotation* an = attr->getAnnotation(a);
					const wchar_t* anName = an->getName();
					if (std::wcscmp(anName, ANNOT_ENUM) == 0)
						return {AttributeTrait::ENUM, {{}, an}};
					else if (std::wcscmp(anName, ANNOT_RANGE) == 0)
						return {AttributeTrait::RANGE, {{}, an}};
					else if (std::wcscmp(anName, ANNOT_COLOR) == 0)
						return {AttributeTrait::COLOR, {}};
					else if (std::wcscmp(anName, ANNOT_DIR) == 0) {
						return {AttributeTrait::DIR, {}};
					}
					else if (std::wcscmp(anName, ANNOT_FILE) == 0) {
						std::wstring exts;
						for (size_t arg = 0; arg < an->getNumArguments(); arg++) {
							if (an->getArgument(arg)->getType() == prt::AAT_STR) {
								exts += an->getArgument(arg)->getStr();
								exts += L" (*.";
								exts += an->getArgument(arg)->getStr();
								exts += L");";
							}
						}
						exts += L"All Files (*.*)";
						return {AttributeTrait::FILE, {exts, nullptr}};
					}
				}
			}
			return {AttributeTrait::PLAIN, {}};
		};

		auto attrTrait = detectAttributeTrait(fqName);

		MObject attr;

		switch (attrType) {
			case prt::Attributable::PT_BOOL: {
				const bool value = mGenerateAttrs->getBool(fqName.c_str());
				if (attrTrait.first == AttributeTrait::ENUM) {
					mEnums.emplace_front();
					const short enumIndex = getDefaultEnumIdx(attrTrait.second.mAnnot, value);
					MCHECK(addEnumParameter(attrTrait.second.mAnnot, node, attr, p, enumIndex, mEnums.front()));
				}
				else {
					MCHECK(addBoolParameter(node, attr, p, value));
				}
				break;
			}
			case prt::Attributable::PT_FLOAT: {
				const double value = mGenerateAttrs->getFloat(fqName.c_str());

				switch (attrTrait.first) {
					case AttributeTrait::ENUM: {
						mEnums.emplace_front();
						const short enumIndex = getDefaultEnumIdx(attrTrait.second.mAnnot, value);
						MCHECK(addEnumParameter(attrTrait.second.mAnnot, node, attr, p, enumIndex, mEnums.front()));
						break;
					}
					case AttributeTrait::RANGE: {
						auto tryParseRangeAnnotation = [](const prt::Annotation* an) -> std::pair<double, double> {
							auto minMax = std::make_pair(std::numeric_limits<double>::quiet_NaN(),
							                             std::numeric_limits<double>::quiet_NaN());
							for (int argIdx = 0; argIdx < an->getNumArguments(); argIdx++) {
								const prt::AnnotationArgument* arg = an->getArgument(argIdx);
								const wchar_t* key = arg->getKey();
								if (std::wcscmp(key, MIN_KEY) == 0) {
									minMax.first = arg->getFloat();
								}
								else if (std::wcscmp(key, MAX_KEY) == 0) {
									minMax.second = arg->getFloat();
								}
							}
							return minMax;
						};

						std::pair<double, double> minMax = tryParseRangeAnnotation(attrTrait.second.mAnnot);
						MCHECK(addFloatParameter(node, attr, p, value, minMax.first, minMax.second));
						break;
					}
					case AttributeTrait::PLAIN: {
						MCHECK(addFloatParameter(node, attr, p, value, std::numeric_limits<double>::quiet_NaN(),
						                         std::numeric_limits<double>::quiet_NaN()));
						break;
					}
					default:
						LOG_WRN << "Encountered unsupported annotation on float attribute " << fqName;
						break;
				} // switch attrTrait

				break;
			}
			case prt::Attributable::PT_STRING: {
				const std::wstring value = mGenerateAttrs->getString(fqName.c_str());

				// special case: detect color trait from value
				if (attrTrait.first == AttributeTrait::PLAIN) {
					if (value.length() == 7 && value[0] == L'#')
						attrTrait.first = AttributeTrait::COLOR;
				}

				const MString mvalue(value.c_str());

				switch (attrTrait.first) {
					case AttributeTrait::ENUM: {
						mEnums.emplace_front();
						const short enumIndex = getDefaultEnumIdx(attrTrait.second.mAnnot, mvalue);
						MCHECK(addEnumParameter(attrTrait.second.mAnnot, node, attr, p, enumIndex, mEnums.front()));
						break;
					}
					case AttributeTrait::FILE:
					case AttributeTrait::DIR:
						MCHECK(addFileParameter(node, attr, p, mvalue, attrTrait.second.mString));
						break;
					case AttributeTrait::COLOR:
						MCHECK(addColorParameter(node, attr, p, mvalue));
						break;
					case AttributeTrait::PLAIN:
						MCHECK(addStrParameter(node, attr, p, mvalue));
						break;
					default:
						LOG_WRN << "Encountered unsupported annotation on string attribute " << fqName;
						break;
				}
				break;
			}
			default:
				break;
		}

		MFnAttribute fnAttr(attr, &stat);
		if (stat == MS::kSuccess) {
			fnAttr.addToCategory(MString(p.ruleFile.c_str()));
			fnAttr.addToCategory(MString(join<wchar_t>(p.groups, L" > ").c_str()));
		}
	} // for all mGenerateAttrs keys

	removeUnusedAttribs(node);

	return MS::kSuccess;
}

void PRTModifierAction::removeUnusedAttribs(MFnDependencyNode& node) {
	auto isInUse = [this](const MString& attrName) {
		const std::wstring attrNameWithoutSuffix = removeSuffix(attrName.asWChar());
		auto it = mRuleAttributes.find(attrNameWithoutSuffix);
		return (it != mRuleAttributes.end());
	};

	std::list<MObject> attrToRemove;
	std::list<MObject> ignoreList;

	for (unsigned int i = 0; i < node.attributeCount(); i++) {
		const MObject attrObj = node.attribute(i);
		const MFnAttribute attr(attrObj);
		const MString attrName = attr.name();

		// all dynamic attributes of this node are CGA rule attributes per design
		if (!attr.isDynamic())
			continue;

		if (attr.isUsedAsColor()) {
			MFnCompoundAttribute compAttr(attrObj);
			for (unsigned int ci = 0, numChildren = compAttr.numChildren(); ci < numChildren; ci++)
				ignoreList.emplace_back(compAttr.child(ci));
		}

		if (isInUse(attrName))
			continue;

		attrToRemove.push_back(attrObj);
	}

	for (auto& attr : attrToRemove) {
		if (std::count(std::begin(ignoreList), std::end(ignoreList), attr) > 0)
			continue;
		node.removeAttribute(attr);
	}
}

MStatus PRTModifierEnum::fill(const prt::Annotation* annot) {
	mRestricted = true;
	MStatus stat;

	uint32_t enumIndex = 0;
	for (size_t arg = 0; arg < annot->getNumArguments(); arg++) {

		const wchar_t* key = annot->getArgument(arg)->getKey();
		if (std::wcscmp(key, NULL_KEY) != 0) {
			if (std::wcscmp(key, RESTRICTED_KEY) == 0) {
				mRestricted = annot->getArgument(arg)->getBool();
			}
			if (std::wcscmp(key, VALUES_ATTR_KEY) == 0) {
				mValuesAttr = annot->getArgument(arg)->getStr();
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

MStatus PRTModifierAction::addParameter(MFnDependencyNode& node, MObject& attr, MFnAttribute& tAttr) {
	if (!(node.hasAttribute(tAttr.shortName()))) {
		MStatus stat;

		MCHECK(tAttr.setKeyable(true));
		MCHECK(tAttr.setHidden(false));
		MCHECK(tAttr.setStorable(true));
		stat = node.addAttribute(attr);
		if (stat != MS::kSuccess)
			return stat;

		// add hidden user_set attribute
		stat = addHiddenBoolParameter(node, tAttr, ATTRIBUTE_USER_SET_SUFFIX);
		if (stat != MS::kSuccess)
			return stat;

		// add hidden force_default attribute
		return addHiddenBoolParameter(node, tAttr, ATTRIBUTE_FORCE_DEFAULT_SUFFIX);
	}
	return MS::kSuccess;
}

MStatus PRTModifierAction::addBoolParameter(MFnDependencyNode& node, MObject& attr, const RuleAttribute& ruleAttr,
                                            bool defaultValue) {
	MStatus stat;
	MFnNumericAttribute nAttr;

	bool plugValue = getPlugValueAndRemoveAttr(node, ruleAttr.mayaBriefName.c_str(), defaultValue);
	attr = nAttr.create(ruleAttr.mayaFullName.c_str(), ruleAttr.mayaBriefName.c_str(), MFnNumericData::kBoolean,
	                    defaultValue, &stat);
	nAttr.setNiceNameOverride(ruleAttr.mayaNiceName.c_str());
	MCHECK(stat);

	stat = addParameter(node, attr, nAttr);

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return stat;
}

MStatus PRTModifierAction::addFloatParameter(MFnDependencyNode& node, MObject& attr, const RuleAttribute& ruleAttr,
                                             double defaultValue, double min, double max) {
	MStatus stat;
	MFnNumericAttribute nAttr;

	double plugValue = getPlugValueAndRemoveAttr(node, ruleAttr.mayaBriefName.c_str(), defaultValue);
	attr = nAttr.create(ruleAttr.mayaFullName.c_str(), ruleAttr.mayaBriefName.c_str(), MFnNumericData::kDouble,
	                    defaultValue, &stat);
	nAttr.setNiceNameOverride(ruleAttr.mayaNiceName.c_str());
	MCHECK(stat);

	if (!prtu::isnan(min)) {
		MCHECK(nAttr.setMin(min));
	}

	if (!prtu::isnan(max)) {
		MCHECK(nAttr.setMax(max));
	}

	stat = addParameter(node, attr, nAttr);

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return stat;
}

MStatus PRTModifierAction::addEnumParameter(const prt::Annotation* annot, MFnDependencyNode& node, MObject& attr,
                                            const RuleAttribute& ruleAttr, short defaultValue, PRTModifierEnum& e) {
	MStatus stat;

	short plugValue = getPlugValueAndRemoveAttr(node, ruleAttr.mayaBriefName.c_str(), defaultValue);
	attr = e.mAttr.create(ruleAttr.mayaFullName.c_str(), ruleAttr.mayaBriefName.c_str(), defaultValue, &stat);
	e.mAttr.setNiceNameOverride(ruleAttr.mayaNiceName.c_str());
	MCHECK(stat);

	MCHECK(e.fill(annot));

	stat = addParameter(node, attr, e.mAttr);

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return stat;
}

MStatus PRTModifierAction::addFileParameter(MFnDependencyNode& node, MObject& attr, const RuleAttribute& ruleAttr,
                                            const MString& defaultValue, const std::wstring& /*exts*/) {
	MStatus stat;
	MFnTypedAttribute sAttr;

	MString plugValue = getPlugValueAndRemoveAttr(node, ruleAttr.mayaBriefName.c_str(), defaultValue);

	attr = sAttr.create(ruleAttr.mayaFullName.c_str(), ruleAttr.mayaBriefName.c_str(), MFnData::kString,
	                    MObject::kNullObj, &stat);
	// NOTE: we must not set the default string above, otherwise the value will not be stored, relying on setValue below
	// see http://ewertb.mayasound.com/api/api.017.php

	MCHECK(sAttr.setNiceNameOverride(ruleAttr.mayaNiceName.c_str()));
	MCHECK(stat);
	MCHECK(sAttr.setUsedAsFilename(true));
	stat = addParameter(node, attr, sAttr);

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return stat;
}

MStatus PRTModifierAction::addColorParameter(MFnDependencyNode& node, MObject& attr, const RuleAttribute& ruleAttr,
                                             const MString& defaultValue) {
	MStatus stat;
	MFnNumericAttribute nAttr;

	const wchar_t* s = defaultValue.asWChar();
	const prtu::Color color = prtu::parseColor(s);

	MFnNumericData fnData;
	MObject rgb = fnData.create(MFnNumericData::k3Float, &stat);
	MCHECK(stat);
	fnData.setData3Float(color[0], color[1], color[2]);

	MObject plugValue = getPlugValueAndRemoveAttr(node, ruleAttr.mayaBriefName.c_str(), rgb);
	attr = nAttr.createColor(ruleAttr.mayaFullName.c_str(), ruleAttr.mayaBriefName.c_str(), &stat);
	nAttr.setNiceNameOverride(ruleAttr.mayaNiceName.c_str());
	nAttr.setDefault(color[0], color[1], color[2]);

	MCHECK(stat);
	stat = addParameter(node, attr, nAttr);

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return stat;
}

MStatus PRTModifierAction::addStrParameter(MFnDependencyNode& node, MObject& attr, const RuleAttribute& ruleAttr,
                                           const MString& defaultValue) {
	MStatus stat;
	MFnTypedAttribute sAttr;

	MString plugValue = getPlugValueAndRemoveAttr(node, ruleAttr.mayaBriefName.c_str(), defaultValue);

	attr = sAttr.create(ruleAttr.mayaFullName.c_str(), ruleAttr.mayaBriefName.c_str(), MFnData::kString,
	                    MObject::kNullObj, &stat);
	// NOTE: we must not set the default string above, otherwise the value will not be stored, relying on setValue below
	// see http://ewertb.mayasound.com/api/api.017.php

	sAttr.setNiceNameOverride(ruleAttr.mayaNiceName.c_str());
	MCHECK(stat);
	stat = addParameter(node, attr, sAttr);

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	if (DBG)
		LOG_DBG << sAttr.name().asWChar() << " = " << plugValue.asWChar();

	return stat;
}
