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

#include "polyModifier/polyModifierFty.h"

#include "util/Utilities.h"
#include "util/ResolveMapCache.h"

#include "prt/API.h"

#include "maya/MObject.h"
#include "maya/MString.h"
#include "maya/MStringArray.h"
#include "maya/MIntArray.h"
#include "maya/MDoubleArray.h"
#include "maya/MPlugArray.h"
#include "maya/MFnEnumAttribute.h"

#include <map>
#include <list>


class PRTModifierAction;

class PRTModifierEnum {
	friend class PRTModifierAction;

public:
	PRTModifierEnum() = default;

	MStatus fill(const prt::Annotation*  annot);

public:
	MFnEnumAttribute        mAttr;

private:
	MStringArray            mSVals;
	MDoubleArray            mFVals;
	MIntArray               mBVals;
	bool                    mRestricted;
}; // class PRTModifierEnum


class PRTModifierAction : public polyModifierFty
{
	friend class PRTModifierEnum;

public:
	PRTModifierAction();

	MStatus updateRuleFiles(MObject& node, const MString& rulePkg);
	void fillAttributesFromNode(const MObject& node);
	void setMesh(MObject& _inMesh, MObject& _outMesh);
	void setRandomSeed(int32_t randomSeed) { mRandomSeed = randomSeed; };

	// polyModifierFty inherited methods
	MStatus		doIt() override;

	//init in PRTModifierPlugin::initializePlugin, destroyed in PRTModifierPlugin::uninitializePlugin
	static const prt::Object*       thePRT;
	static prt::CacheObject*        theCache;
	static prt::ConsoleLogHandler*  theLogHandler;
	static prt::FileLogHandler*     theFileLogHandler;
	static const std::string&       getPluginRoot();
	static ResolveMapCache*         mResolveMapCache;

private:

	//init in PRTModifierAction::PRTModifierAction()
	AttributeMapUPtr mMayaEncOpts;
	AttributeMapUPtr mAttrEncOpts;
	AttributeMapUPtr mCGAPrintOptions;
	AttributeMapUPtr mCGAErrorOptions;

	// Mesh Nodes: only used during doIt
	MObject inMesh;
	MObject outMesh;

	// Set in updateRuleFiles(rulePkg)
	MString                       mRulePkg;
	std::wstring                  mRuleFile;
	std::wstring                  mStartRule;
	int32_t                       mRandomSeed;

	ResolveMapSPtr getResolveMap();

	//init in fillAttributesFromNode()
	AttributeMapUPtr mGenerateAttrs;

	std::list<PRTModifierEnum> mEnums;
	std::map<std::wstring, std::wstring> mBriefName2prtAttr;
	MStatus createNodeAttributes(MObject& node, const std::wstring & ruleFile, const std::wstring & startRule, prt::AttributeMapBuilder* aBuilder, const prt::RuleFileInfo* info);
	void removeUnusedAttribs(const prt::RuleFileInfo* info, MFnDependencyNode &node);

	static MStatus  addParameter(MFnDependencyNode & node, MObject & attr, MFnAttribute& tAttr);
	static MStatus  addBoolParameter(MFnDependencyNode & node, MObject & attr, const MString & name, bool defaultValue);
	static MStatus  addFloatParameter(MFnDependencyNode & node, MObject & attr, const MString & name, double defaultValue, double min, double max);
	static MStatus  addStrParameter(MFnDependencyNode & node, MObject & attr, const MString & name, const MString & defaultValue);
	static MStatus  addFileParameter(MFnDependencyNode & node, MObject & attr, const MString & name, const MString & defaultValue, const MString & ext);
	static MStatus  addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const MString & name, bool defaultValue, PRTModifierEnum & e);
	static MStatus  addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const MString & name, double defaultValue, PRTModifierEnum & e);
	static MStatus  addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const MString & name, MString defaultValue, PRTModifierEnum & e);
	static MStatus  addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const MString & name, short defaultValue, PRTModifierEnum & e);
	static MStatus  addColorParameter(MFnDependencyNode & node, MObject & attr, const MString & name, const MString& defaultValue);
	static MString  longName(const MString & attrName);
	static MString  briefName(const MString & attrName);
	static MString  niceName(const MString & attrName);
	template<typename T> static T getPlugValueAndRemoveAttr(MFnDependencyNode & node, const MString & briefName, const T & defaultValue);
};

