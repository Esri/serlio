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
#include "prtModifier/RuleAttributes.h"
#include "util/Utilities.h"
#include "PRTContext.h"

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


class PRTModifierAction : public polyModifierFty {
	friend class PRTModifierEnum;

public:
	PRTModifierAction(PRTContextUPtr& prtCtx);

	MStatus updateRuleFiles(MObject& node, const MString& rulePkg);
	MStatus fillAttributesFromNode(const MObject& node);
	void setMesh(MObject& _inMesh, MObject& _outMesh);
	void setRandomSeed(int32_t randomSeed) { mRandomSeed = randomSeed; };

	// polyModifierFty inherited methods
	MStatus		doIt() override;

private:
	PRTContextUPtr& mPRTCtx;

	//init in PRTModifierAction::PRTModifierAction()
	AttributeMapUPtr mMayaEncOpts;
	AttributeMapUPtr mCGAPrintOptions;
	AttributeMapUPtr mCGAErrorOptions;

	// Mesh Nodes: only used during doIt
	MObject inMesh;
	MObject outMesh;

	// Set in updateRuleFiles(rulePkg)
	MString                       mRulePkg;
	std::wstring                  mRuleFile;
	std::wstring                  mStartRule;
	const std::wstring            mRuleStyle = L"Default"; // Serlio atm only supports the "Default" style
	int32_t                       mRandomSeed;
	RuleAttributes                mRuleAttributes; // TODO: could be cached together with ResolveMap

	ResolveMapSPtr getResolveMap();

	//init in fillAttributesFromNode()
	AttributeMapUPtr mGenerateAttrs;

	std::list<PRTModifierEnum> mEnums;
//	std::map<std::wstring, std::wstring> mBriefName2prtAttr;
	MStatus createNodeAttributes(MObject& node, const prt::RuleFileInfo* info);
	void removeUnusedAttribs(MFnDependencyNode& node);

	static MStatus  addParameter(MFnDependencyNode & node, MObject & attr, MFnAttribute& tAttr);
	static MStatus  addBoolParameter(MFnDependencyNode& node, MObject& attr, const RuleAttribute& name, bool defaultValue);
	static MStatus  addFloatParameter(MFnDependencyNode & node, MObject & attr, const RuleAttribute& name, double defaultValue, double min, double max);
	static MStatus  addStrParameter(MFnDependencyNode & node, MObject & attr, const RuleAttribute& name, const MString & defaultValue);
	static MStatus  addFileParameter(MFnDependencyNode & node, MObject & attr, const RuleAttribute& name, const MString & defaultValue, const std::wstring& ext);
	static MStatus  addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const RuleAttribute& name, bool defaultValue, PRTModifierEnum & e);
	static MStatus  addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const RuleAttribute& name, double defaultValue, PRTModifierEnum & e);
	static MStatus  addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const RuleAttribute& name, MString defaultValue, PRTModifierEnum & e);
	static MStatus  addEnumParameter(const prt::Annotation* annot, MFnDependencyNode & node, MObject & attr, const RuleAttribute& name, short defaultValue, PRTModifierEnum & e);
	static MStatus  addColorParameter(MFnDependencyNode & node, MObject & attr, const RuleAttribute& name, const MString& defaultValue);
	template<typename T> static T getPlugValueAndRemoveAttr(MFnDependencyNode & node, const MString & briefName, const T & defaultValue);
};

