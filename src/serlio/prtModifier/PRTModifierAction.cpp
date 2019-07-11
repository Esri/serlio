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

#include "prtModifier/PRTModifierAction.h"
#include "prtModifier/PRTModifierCommand.h"
#include "prtModifier/MayaCallbacks.h"
#include "prtModifier/RuleAttributes.h"

#include "util/Utilities.h"
#include "util/MayaUtilities.h"
#include "util/LogHandler.h"

#include "prt/StringUtils.h"

#include "maya/MFloatPointArray.h"
#include "maya/MFnMesh.h"
#include "maya/MFnNumericAttribute.h"
#include "maya/MFnTypedAttribute.h"
#include "maya/MFnStringData.h"
#include "maya/MFnCompoundAttribute.h"

#include <cassert>


#define CHECK_STATUS(st) if ((st) != MS::kSuccess) { break; }
namespace {

constexpr bool DBG = false;

constexpr const wchar_t* ENC_ID_MAYA      = L"MayaEncoder";
constexpr const wchar_t* ENC_ID_ATTR_EVAL = L"com.esri.prt.core.AttributeEvalEncoder";
constexpr const wchar_t* ENC_ID_CGA_ERROR = L"com.esri.prt.core.CGAErrorEncoder";
constexpr const wchar_t* ENC_ID_CGA_PRINT = L"com.esri.prt.core.CGAPrintEncoder";
constexpr const wchar_t* FILE_CGA_ERROR   = L"CGAErrors.txt";
constexpr const wchar_t* FILE_CGA_PRINT   = L"CGAPrint.txt";

constexpr const wchar_t* NULL_KEY = L"#NULL#";
constexpr const wchar_t* MIN_KEY = L"min";
constexpr const wchar_t* MAX_KEY = L"max";
const MString  PRT("PRT");
constexpr const wchar_t* RESTRICTED_KEY = L"restricted";

const AttributeMapUPtr EMPTY_ATTRIBUTES(AttributeMapBuilderUPtr(prt::AttributeMapBuilder::create())->createAttributeMap());

namespace UnitQuad {
	const double   vertices[] = { 0, 0, 0,  0, 0, 1,  1, 0, 1,  1, 0, 0 };
	const size_t   vertexCount = 12;
	const uint32_t indices[] = { 0, 1, 2, 3 };
	const size_t   indexCount = 4;
	const uint32_t faceCounts[] = { 4 };
	const size_t   faceCountsCount = 1;
	const int32_t  seed = mu::computeSeed(vertices, vertexCount);
}

AttributeMapUPtr getDefaultAttributeValues(const std::wstring& ruleFile, const std::wstring& startRule, ResolveMapSPtr resolveMap, CacheObjectUPtr& cache) {
	AttributeMapBuilderUPtr mayaCallbacksAttributeBuilder(prt::AttributeMapBuilder::create());
	MayaCallbacks mayaCallbacks(MObject::kNullObj, MObject::kNullObj, mayaCallbacksAttributeBuilder);

	InitialShapeBuilderUPtr isb(prt::InitialShapeBuilder::create());

	isb->setGeometry(
		UnitQuad::vertices,
		UnitQuad::vertexCount,
		UnitQuad::indices,
		UnitQuad::indexCount,
		UnitQuad::faceCounts,
		UnitQuad::faceCountsCount
	);

	isb->setAttributes(ruleFile.c_str(), startRule.c_str(), UnitQuad::seed,L"", EMPTY_ATTRIBUTES.get(), resolveMap.get());

	const InitialShapeUPtr shape(isb->createInitialShapeAndReset());
	const InitialShapeNOPtrVector shapes = { shape.get() };

	const std::vector<const wchar_t*> encIDs = { ENC_ID_ATTR_EVAL };
	const AttributeMapUPtr attrEncOpts = prtu::createValidatedOptions(ENC_ID_ATTR_EVAL);
	const AttributeMapNOPtrVector encOpts = { attrEncOpts.get() };
	assert(encIDs.size() == encOpts.size());

	prt::generate(shapes.data(), shapes.size(), nullptr, encIDs.data(), encIDs.size(), encOpts.data(), &mayaCallbacks, cache.get(), nullptr);

	return AttributeMapUPtr(mayaCallbacksAttributeBuilder->createAttributeMap());
}

} // namespace

PRTModifierAction::PRTModifierAction(PRTContextUPtr& prtCtx) : mPRTCtx(prtCtx) {
	AttributeMapBuilderUPtr optionsBuilder(prt::AttributeMapBuilder::create());

	mMayaEncOpts = prtu::createValidatedOptions(ENC_ID_MAYA);

	optionsBuilder->setString(L"name", FILE_CGA_ERROR);
	const AttributeMapUPtr errOptions(optionsBuilder->createAttributeMapAndReset());
	mCGAErrorOptions = prtu::createValidatedOptions(ENC_ID_CGA_ERROR, errOptions.get());

	optionsBuilder->setString(L"name", FILE_CGA_PRINT);
	const AttributeMapUPtr printOptions(optionsBuilder->createAttributeMapAndReset());
	mCGAPrintOptions = prtu::createValidatedOptions(ENC_ID_CGA_PRINT, printOptions.get());
}

void PRTModifierAction::fillAttributesFromNode(const MObject& node) {
	MStatus           stat;
	const MFnDependencyNode fNode(node, &stat);
	MCHECK(stat);

	auto resolveMap = getResolveMap();
	const RuleFileInfoUPtr info(prt::createRuleFileInfo(resolveMap->getString(mRuleFile.c_str())));
	auto getRuleAttributeType = [&info](const std::wstring& name){
		for (size_t ai = 0, numAttrs = info->getNumAttributes(); ai < numAttrs; ai++) {
			const auto* a = info->getAttribute(ai);
			if (std::wcscmp(a->getName(), name.c_str()) == 0) {
				return a->getReturnType();
			}
		}
		return prt::AAT_UNKNOWN;
	};

	const AttributeMapUPtr defaultAttributeValues = getDefaultAttributeValues(mRuleFile, mStartRule, getResolveMap(), mPRTCtx->theCache);

	const unsigned int count = fNode.attributeCount(&stat);
	MCHECK(stat);

	AttributeMapBuilderUPtr aBuilder(prt::AttributeMapBuilder::create());

	for (unsigned int i = 0; i < count; i++) {
		const MObject attr = fNode.attribute(i, &stat);
		if (stat != MS::kSuccess) continue;

		const MPlug plug(node, attr);
		if (!(plug.isDynamic())) continue;

		const MString       briefName = plug.partialName();
		const std::wstring  name = mBriefName2prtAttr[briefName.asWChar()];
		const auto ruleAttrType = getRuleAttributeType(name);

		if (attr.hasFn(MFn::kNumericAttribute)) {
			MFnNumericAttribute nAttr(attr);

			if (nAttr.unitType() == MFnNumericData::kBoolean) {
				assert(ruleAttrType == prt::AAT_BOOL);

				bool val;
				MCHECK(plug.getValue(val));

				const auto defVal = defaultAttributeValues->getBool(name.c_str());
				if (val != defVal)
					aBuilder->setBool(name.c_str(), val);
			}
			else if (nAttr.unitType() == MFnNumericData::kDouble) {
				assert(ruleAttrType == prt::AAT_FLOAT);

				double val;
				MCHECK(plug.getValue(val));

				const auto defVal = defaultAttributeValues->getFloat(name.c_str());
				if (val != defVal)
					aBuilder->setFloat(name.c_str(), val);
			}
			else if (nAttr.isUsedAsColor()) {
				assert(ruleAttrType == prt::AAT_STR);
				const wchar_t* defColStr = defaultAttributeValues->getString(name.c_str());

				MObject rgb;
				MCHECK(plug.getValue(rgb));
				MFnNumericData fRGB(rgb);

				prtu::Color col;
				MCHECK(fRGB.getData(col[0], col[1], col[2]));
				const std::wstring colStr = prtu::getColorString(col);

				if (std::wcscmp(colStr.c_str(), defColStr) != 0)
					aBuilder->setString(name.c_str(), colStr.c_str());
			}
		}
		else if (attr.hasFn(MFn::kTypedAttribute)) {
			assert(ruleAttrType == prt::AAT_STR);

			MString val;
			MCHECK(plug.getValue(val));

			const auto defVal = defaultAttributeValues->getString(name.c_str());
			if (std::wcscmp(val.asWChar(), defVal) != 0)
				aBuilder->setString(name.c_str(), val.asWChar());
		}
		else if (attr.hasFn(MFn::kEnumAttribute)) {
			MFnEnumAttribute eAttr(attr);

			short di;
			short i;
			MCHECK(eAttr.getDefault(di));
			MCHECK(plug.getValue(i));
			if (i != di) {
				switch (ruleAttrType) {
					case prt::AAT_STR:
						aBuilder->setString(name.c_str(), eAttr.fieldName(i).asWChar());
						break;
					case prt::AAT_FLOAT:
						aBuilder->setFloat(name.c_str(), eAttr.fieldName(i).asDouble());
						break;
					case prt::AAT_BOOL:
						aBuilder->setBool(name.c_str(), eAttr.fieldName(i).asInt() != 0);
						break;
					default:
						LOG_ERR << "Cannot handle attribute type " << ruleAttrType << " for attr " << name;
				}
			}
		}
	}

	mGenerateAttrs.reset(aBuilder->createAttributeMap());
}

// Sets the mesh object for the action  to operate on
void PRTModifierAction::setMesh(MObject& _inMesh, MObject& _outMesh)
{
	inMesh = _inMesh;
	outMesh = _outMesh;
}

ResolveMapSPtr PRTModifierAction::getResolveMap() {
	ResolveMapCache::LookupResult lookupResult = mPRTCtx->mResolveMapCache->get(std::wstring(mRulePkg.asWChar()));
	ResolveMapSPtr resolveMap = lookupResult.first;
	return resolveMap;
}

MStatus PRTModifierAction::updateRuleFiles(MObject& node, const MString& rulePkg) {
	mRulePkg = rulePkg;

	mEnums.clear();
	mRuleFile.clear();
	mStartRule.clear();

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

	RuleFileInfoUPtr info(prt::createRuleFileInfo(ruleFileURI, mPRTCtx->theCache.get(), &infoStatus));
	if (!info || infoStatus != prt::STATUS_OK) {
		LOG_ERR << "could not get rule file info from rule file " << mRuleFile;
		return MS::kFailure;
	}

	mStartRule = prtu::detectStartRule(info);

	if (node != MObject::kNullObj)
		createNodeAttributes(node, mRuleFile, mStartRule, info.get());

	return MS::kSuccess;
}

MStatus PRTModifierAction::doIt()
{
	MStatus status;

	// Get access to the mesh's function set
	const MFnMesh iMeshFn(inMesh);

	MFloatPointArray vertices;
	MIntArray        pcounts;
	MIntArray        pconnect;

	iMeshFn.getPoints(vertices);
	iMeshFn.getVertices(pcounts, pconnect);

	std::vector<double> va(vertices.length()*3);
	for (int i = static_cast<int>(vertices.length()); --i >= 0;) {
		va[i * 3 + 0] = vertices[i].x;
		va[i * 3 + 1] = vertices[i].y;
		va[i * 3 + 2] = vertices[i].z;
	}
	
	std::vector <uint32_t> ia(pconnect.length());
	pconnect.get((int*)ia.data());
	std::vector <uint32_t> ca(pcounts.length());
	pcounts.get((int*)ca.data());

	AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
	std::unique_ptr<MayaCallbacks> outputHandler(new MayaCallbacks(inMesh, outMesh, amb));

	InitialShapeBuilderUPtr isb(prt::InitialShapeBuilder::create());
	prt::Status setGeoStatus = isb->setGeometry(
		va.data(),
		va.size(),
		ia.data(),
		ia.size(),
		ca.data(),
		ca.size()
	);
	if (setGeoStatus != prt::STATUS_OK)
		std::cerr << "InitialShapeBuilder setGeometry failed status = " << prt::getStatusDescription(setGeoStatus) << std::endl;

	isb->setAttributes(
		mRuleFile.c_str(),
		mStartRule.c_str(),
		mRandomSeed,
		L"",
		mGenerateAttrs.get(),
		getResolveMap().get()
	);

	std::unique_ptr <const prt::InitialShape, PRTDestroyer> shape(isb->createInitialShapeAndReset());

	const std::vector<const wchar_t*> encIDs = { ENC_ID_MAYA, ENC_ID_CGA_ERROR, ENC_ID_CGA_PRINT };
	const AttributeMapNOPtrVector encOpts = { mMayaEncOpts.get(), mCGAErrorOptions.get(), mCGAPrintOptions.get() };
	assert(encIDs.size() == encOpts.size());

	InitialShapeNOPtrVector shapes = { shape.get() };
	const prt::Status generateStatus = prt::generate(shapes.data(), shapes.size(), 0, encIDs.data(), encIDs.size(), encOpts.data(), outputHandler.get(), mPRTCtx->theCache.get(), 0);
	if (generateStatus != prt::STATUS_OK)
		std::cerr << "prt generate failed: " << prt::getStatusDescription(generateStatus) << std::endl;

	return status;
}


MStatus PRTModifierAction::createNodeAttributes(MObject& nodeObj, const std::wstring & ruleFile, const std::wstring & startRule, const prt::RuleFileInfo* info) {
	mGenerateAttrs = getDefaultAttributeValues(ruleFile, startRule, getResolveMap(), mPRTCtx->theCache);

	MStatus stat;
	MFnDependencyNode node(nodeObj, &stat);
	MCHECK(stat);

	RuleAttributes sortedAttributes = getRuleAttributes(ruleFile, info);
	sortRuleAttributes(sortedAttributes);

	for (AttributeProperties p: sortedAttributes) {

		size_t i = p.index;

		const MString name = MString(p.name.c_str());
		MObject attr;

		mBriefName2prtAttr[briefName(name).asWChar()] = name.asWChar();

		switch (info->getAttribute(i)->getReturnType()) {
		case prt::AAT_BOOL: {
			const bool value = mGenerateAttrs->getBool(name.asWChar());
			if (p.enumAnnotation) {
				mEnums.emplace_front();
				MCHECK(addEnumParameter(p.enumAnnotation, node, attr, name, value, mEnums.front()));
			}
			else {
				MCHECK(addBoolParameter(node, attr, name, value));
			}
			break;
		}
		case prt::AAT_FLOAT: {
			double min = std::numeric_limits<double>::quiet_NaN();
			double max = std::numeric_limits<double>::quiet_NaN();
			for (size_t a = 0; a < info->getAttribute(i)->getNumAnnotations(); a++) {
				const prt::Annotation* an = info->getAttribute(i)->getAnnotation(a);
				if (!(std::wcscmp(an->getName(), ANNOT_RANGE))) {
					for (int argIdx = 0; argIdx < an->getNumArguments(); argIdx++) {
						const prt::AnnotationArgument* arg = an->getArgument(argIdx);
						const wchar_t* key = arg->getKey();
						if (std::wcscmp(key, MIN_KEY) == 0) {
							min = arg->getFloat();
						}
						else if (std::wcscmp(key, MAX_KEY) == 0) {
							max = arg->getFloat();
						}
					}
				}
			}

			const double value = mGenerateAttrs->getFloat(name.asWChar());

			if (p.enumAnnotation) {
				mEnums.emplace_front();
				MCHECK(addEnumParameter(p.enumAnnotation, node, attr, name, value, mEnums.front()));
			}
			else {
				MCHECK(addFloatParameter(node, attr, name, value, min, max));
			}
			break;
		}
		case prt::AAT_STR: {
			MString exts;
			bool    asFile = false;
			bool    asColor = false;
			for (size_t a = 0; a < info->getAttribute(i)->getNumAnnotations(); a++) {
				const prt::Annotation* an = info->getAttribute(i)->getAnnotation(a);
				const wchar_t* anName = an->getName();
				if (!(std::wcscmp(anName, ANNOT_COLOR)))
					asColor = true;
				else if (!(std::wcscmp(anName, ANNOT_DIR))) {
					exts = MString(anName);
					asFile = true;
				}
				else if (!(std::wcscmp(anName, ANNOT_FILE))) {
					asFile = true;
					for (size_t arg = 0; arg < an->getNumArguments(); arg++) {
						if (an->getArgument(arg)->getType() == prt::AAT_STR) {
							exts += MString(an->getArgument(arg)->getStr());
							exts += " (*.";
							exts += MString(an->getArgument(arg)->getStr());
							exts += ");;";
						}
					}
					exts += "All Files (*.*)";
				}
			}

			const std::wstring value = mGenerateAttrs->getString(name.asWChar());
			const MString mvalue(value.c_str());
			if (!(asColor) && mvalue.length() == 7 && value[0] == L'#')
				asColor = true;

			if (p.enumAnnotation) {
				mEnums.emplace_front();
				MCHECK(addEnumParameter(p.enumAnnotation, node, attr, name, mvalue, mEnums.front()));
			}
			else if (asFile) {
				MCHECK(addFileParameter(node, attr, name, mvalue, exts));
			}
			else if (asColor) {
				MCHECK(addColorParameter(node, attr, name, mvalue));
			}
			else {
				MCHECK(addStrParameter(node, attr, name, mvalue));
			}
			break;
		}
		default:
			break;
		}

		MStatus stat;
		MFnAttribute fnAttr(attr, &stat);
		if (stat == MS::kSuccess) {
			fnAttr.addToCategory(MString(p.ruleFile.c_str()));
			fnAttr.addToCategory(MString(join<wchar_t>(p.groups, L" > ").c_str()));
		}
	}

	removeUnusedAttribs(info, node);

	return MS::kSuccess;
}

void PRTModifierAction::removeUnusedAttribs(const prt::RuleFileInfo* info, MFnDependencyNode &node) {
	auto isInUse = [&info](const MString& attrName) {
		for (size_t j = 0; j < info->getNumAttributes(); j++) {
			const MString name = longName(info->getAttribute(j)->getName());
			if (name == attrName)
				return true;
		}
		return false;
	};

	std::list<MObject> attrToRemove;
	std::list<MObject> ignoreList;

	for (size_t i = 0; i < node.attributeCount(); i++) {
		const MObject attrObj = node.attribute(i);
		const MFnAttribute attr(attrObj);
		const MString attrName = attr.name();

		// all dynamic attributes of this node are CGA rule attributes per design
		if (!attr.isDynamic())
			continue;

		if (attr.isUsedAsColor()) {
			MFnCompoundAttribute compAttr(attrObj);
			for (size_t ci = 0, numChildren = compAttr.numChildren(); ci < numChildren; ci++)
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

		for (size_t arg = 0; arg < annot->getNumArguments(); arg++) {

			const wchar_t* key = annot->getArgument(arg)->getKey();
		if (std::wcscmp(key, NULL_KEY)!=0) {
			if (std::wcscmp(key, RESTRICTED_KEY) == 0) {
				mRestricted = annot->getArgument(arg)->getBool();
			}
			continue;
		}

			switch (annot->getArgument(arg)->getType()) {
		case prt::AAT_BOOL: {
			bool val = annot->getArgument(arg)->getBool();
			MCHECK(mAttr.addField(MString(std::to_wstring(val).c_str()), mBVals.length()));
			mBVals.append(val);
				mFVals.append(std::numeric_limits<double>::quiet_NaN());
				mSVals.append("");
				break;
		}
		case prt::AAT_FLOAT: {
			double val = annot->getArgument(arg)->getFloat();
			MCHECK(mAttr.addField(MString(std::to_wstring(val).c_str()), mFVals.length()));
				mBVals.append(false);
			mFVals.append(val);
				mSVals.append("");
				break;
		}
		case prt::AAT_STR: {
			const wchar_t* val = annot->getArgument(arg)->getStr();
			MCHECK(mAttr.addField(MString(val), mSVals.length()));
				mBVals.append(false);
				mFVals.append(std::numeric_limits<double>::quiet_NaN());
			mSVals.append(MString(val));
				break;
		}
			default:
				break;
			}
		}
	

	return MS::kSuccess;
}

template<typename T> T PRTModifierAction::getPlugValueAndRemoveAttr(MFnDependencyNode & node, const MString & briefName, const T& defaultValue) {
	T plugValue = defaultValue;

	if (DBG) {
		LOG_DBG << "node attrs:";
		mu::forAllAttributes(node, [&node](const MFnAttribute &a) {
			MString val;
			node.findPlug(a.object(), true).getValue(val);
			LOG_DBG << a.name().asWChar() << " = " << val.asWChar();
		});
	}

	if (node.hasAttribute(briefName)) {
		const MPlug plug = node.findPlug(briefName, true);
		if (plug.isDynamic())
		{
			T d;
			if (plug.getValue(d) == MS::kSuccess)
				plugValue = d;
		}
		node.removeAttribute(node.attribute(briefName));
	}
	return plugValue;
}

MString PRTModifierAction::longName(const MString& attrName) {
	return PRT + mu::toCleanId(attrName);
}

MString PRTModifierAction::briefName(const MString & attrName) {
	return mu::toCleanId(attrName.substring(attrName.indexW(L'$') + 1, attrName.length())); //remove style
}

MString PRTModifierAction::niceName(const MString & attrName) { //remove style and import rule name
	return mu::toCleanId(attrName.substring(std::max(attrName.indexW(L'$'), attrName.rindexW(L'.')) + 1, attrName.length()));
}

MStatus PRTModifierAction::addParameter(MFnDependencyNode & node, MObject & attr, MFnAttribute& tAttr) {
	if (!(node.hasAttribute(tAttr.shortName()))) {
		MCHECK(tAttr.setKeyable(true));
		MCHECK(tAttr.setHidden(false));
		MCHECK(tAttr.setStorable(true));
		MCHECK(node.addAttribute(attr));
	}
	return MS::kSuccess;
}

MStatus PRTModifierAction::addBoolParameter(MFnDependencyNode & node, MObject & attr, const MString & name, bool defaultValue) {
	MStatus stat;
	MFnNumericAttribute nAttr;

	bool plugValue = getPlugValueAndRemoveAttr(node, briefName(name), defaultValue);
	attr = nAttr.create(longName(name), briefName(name), MFnNumericData::kBoolean, defaultValue, &stat);
	nAttr.setNiceNameOverride(niceName(name));
	if (stat != MS::kSuccess) throw stat;

	MCHECK(addParameter(node, attr, nAttr));

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return MS::kSuccess;
}

MStatus PRTModifierAction::addFloatParameter(MFnDependencyNode & node, MObject & attr, const MString & name, double defaultValue, double min, double max) {
	MStatus stat;
	MFnNumericAttribute nAttr;

	double plugValue = getPlugValueAndRemoveAttr(node, briefName(name), defaultValue);
	attr = nAttr.create(longName(name), briefName(name), MFnNumericData::kDouble, defaultValue, &stat);
	nAttr.setNiceNameOverride(niceName(name));
	if (stat != MS::kSuccess)
		throw stat;

	if (!prtu::isnan(min)) {
		MCHECK(nAttr.setMin(min));
	}

	if (!prtu::isnan(max)) {
		MCHECK(nAttr.setMax(max));
	}

	MCHECK(addParameter(node, attr, nAttr));

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return MS::kSuccess;
}

MStatus PRTModifierAction::addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const MString & name, bool defaultValue, PRTModifierEnum & e) {
	short idx = 0;
	for (int i = static_cast<int>(e.mBVals.length()); --i >= 0;) {
		if ((e.mBVals[i] != 0) == defaultValue) {
			idx = static_cast<short>(i);
			break;
		}
	}

	return addEnumParameter(annot, node, attr, name, idx, e);
}

MStatus PRTModifierAction::addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const MString & name, double defaultValue, PRTModifierEnum & e) {
	short idx = 0;
	for (int i = static_cast<int>(e.mFVals.length()); --i >= 0;) {
		if (e.mFVals[i] == defaultValue) {
			idx = static_cast<short>(i);
			break;
		}
	}

	return addEnumParameter(annot, node, attr, name, idx, e);
}

MStatus PRTModifierAction::addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const MString & name, MString defaultValue, PRTModifierEnum & e) {
	short idx = 0;
	for (int i = static_cast<int>(e.mSVals.length()); --i >= 0;) {
		if (e.mSVals[i] == defaultValue) {
			idx = static_cast<short>(i);
			break;
		}
	}

	return addEnumParameter(annot, node, attr, name, idx, e);
}

MStatus PRTModifierAction::addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const MString & name, short defaultValue, PRTModifierEnum & e) {
	MStatus stat;

	short plugValue = getPlugValueAndRemoveAttr(node, briefName(name), defaultValue);
	attr = e.mAttr.create(longName(name), briefName(name), defaultValue, &stat);
	e.mAttr.setNiceNameOverride(niceName(name));
	MCHECK(stat);

	MCHECK(e.fill(annot));

	MCHECK(addParameter(node, attr, e.mAttr));

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return MS::kSuccess;
}

MStatus PRTModifierAction::addFileParameter(MFnDependencyNode & node, MObject & attr, const MString & name, const MString & defaultValue, const MString & /*exts*/) {
	MStatus           stat;
	MFnTypedAttribute sAttr;

	MString plugValue = getPlugValueAndRemoveAttr(node, briefName(name), defaultValue);

	attr = sAttr.create(longName(name), briefName(name), MFnData::kString, MObject::kNullObj, &stat);
	// NOTE: we must not set the default string above, otherwise the value will not be stored, relying on setValue below
	// see http://ewertb.mayasound.com/api/api.017.php

	MCHECK(sAttr.setNiceNameOverride(niceName(name)));
	MCHECK(stat);
	MCHECK(sAttr.setUsedAsFilename(true));
	MCHECK(addParameter(node, attr, sAttr));

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return MS::kSuccess;
}

MStatus PRTModifierAction::addColorParameter(MFnDependencyNode & node, MObject & attr, const MString & name, const MString & defaultValue) {
	MStatus             stat;
	MFnNumericAttribute nAttr;

	const wchar_t* s = defaultValue.asWChar();
	const prtu::Color color = prtu::parseColor(s);

	MFnNumericData fnData;
	MObject        rgb = fnData.create(MFnNumericData::k3Double, &stat);
	MCHECK(stat);
	fnData.setData(color[0], color[1], color[2]);

	MObject plugValue = getPlugValueAndRemoveAttr(node, briefName(name), rgb);
	attr = nAttr.createColor(longName(name), briefName(name), &stat);
	nAttr.setNiceNameOverride(niceName(name));
	nAttr.setDefault(color[0], color[1], color[2]);

	MCHECK(stat);
	MCHECK(addParameter(node, attr, nAttr));

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	return MS::kSuccess;
}

MStatus PRTModifierAction::addStrParameter(MFnDependencyNode & node, MObject & attr, const MString & name, const MString & defaultValue) {
	MStatus           stat;
	MFnTypedAttribute sAttr;

	MString plugValue = getPlugValueAndRemoveAttr(node, briefName(name), defaultValue);

	attr = sAttr.create(longName(name), briefName(name), MFnData::kString, MObject::kNullObj, &stat);
	// NOTE: we must not set the default string above, otherwise the value will not be stored, relying on setValue below
	// see http://ewertb.mayasound.com/api/api.017.php

	sAttr.setNiceNameOverride(niceName(name));
	MCHECK(stat);
	MCHECK(addParameter(node, attr, sAttr));

	MPlug plug(node.object(), attr);
	MCHECK(plug.setValue(plugValue));

	LOG_DBG << sAttr.name().asWChar() << " = " << plugValue.asWChar();

	return MS::kSuccess;
}
