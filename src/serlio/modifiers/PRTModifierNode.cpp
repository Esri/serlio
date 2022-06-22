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

#include "modifiers/PRTModifierNode.h"

#include "utils/MayaUtilities.h"

#include "serlioPlugin.h"

#include "maya/MDataHandle.h"
#include "maya/MFnMeshData.h"
#include "maya/MFnNumericAttribute.h"
#include "maya/MFnStringArrayData.h"
#include "maya/MFnStringData.h"
#include "maya/MFnTypedAttribute.h"

#define MCheckStatus(status, message)                                                                                  \
	if (MStatus::kSuccess != (status)) {                                                                               \
		cerr << (message) << "\n";                                                                                     \
		return (status);                                                                                               \
	}

namespace {
const MString NAME_RULE_PKG = "Rule_Package";
const MString NAME_RANDOM_SEED = "Random_Seed";
const MString CGAC_PROBLEMS = "CGAC_Problems";
} // namespace

// Unique Node TypeId
MTypeId PRTModifierNode::id(SerlioNodeIDs::SERLIO_PREFIX, SerlioNodeIDs::PRT_GEOMETRY_NODE);
MObject PRTModifierNode::rulePkg;
MObject PRTModifierNode::cgacProblems;
MObject PRTModifierNode::currentRulePkg;
MObject PRTModifierNode::mRandomSeed;

// make sure the dynamically added plugs affect the outMesh
MStatus PRTModifierNode::setDependentsDirty(const MPlug& /*plugBeingDirtied*/, MPlugArray& affectedPlugs) {
	const MPlug pOutMesh(thisMObject(), outMesh);
	affectedPlugs.append(pOutMesh);
	return MS::kSuccess;
}

// This method computes the value of the given output plug based
// on the values of the input attributes. Based on the Maya example splitUvCmd
MStatus PRTModifierNode::compute(const MPlug& plug, MDataBlock& data) {
	MStatus status = MS::kSuccess;

	MDataHandle stateData = data.outputValue(state, &status);
	MCheckStatus(status, "ERROR getting state");

	// Check for the HasNoEffect/PassThrough flag on the node.
	// (stateData is an enumeration standard in all depend nodes)
	//
	// (0 = Normal)
	// (1 = HasNoEffect/PassThrough)
	// (2 = Blocking)
	if (stateData.asShort() == 1) {
		MDataHandle inputData = data.inputValue(inMesh, &status);
		MCheckStatus(status, "ERROR getting inMesh");

		MDataHandle outputData = data.outputValue(outMesh, &status);
		MCheckStatus(status, "ERROR getting outMesh");

		// Simply redirect the inMesh to the outMesh for the PassThrough effect
		outputData.set(inputData.asMesh());
	}
	else {
		// Check which output attribute we have been asked to
		// compute. If this node doesn't know how to compute it,
		// we must return MS::kUnknownParameter
		if (plug == outMesh) {
			MDataHandle inputData = data.inputValue(inMesh, &status);
			MCheckStatus(status, "ERROR getting inMesh");

			MDataHandle outputData = data.outputValue(outMesh, &status);
			MCheckStatus(status, "ERROR getting outMesh");

			MDataHandle rulePkgData = data.inputValue(rulePkg, &status);
			MCheckStatus(status, "ERROR getting rulePkg");

			MDataHandle currentRulePkgData = data.inputValue(currentRulePkg, &status);
			MCheckStatus(status, "ERROR getting currentRulePkg");

			const bool ruleFileWasChanged = (rulePkgData.asString() != currentRulePkgData.asString());
			currentRulePkgData.setString(rulePkgData.asString());

			// Copy the inMesh to the outMesh, so you can
			// perform operations directly on outMesh
			//
			outputData.set(inputData.asMesh());
			MObject iMesh = outputData.asMesh();
			MObject oMesh = outputData.asMesh();

			// Set the mesh object and component List on the factory
			fPRTModifierAction.setMesh(iMesh, oMesh);

			if (!ruleFileWasChanged)
				fPRTModifierAction.updateUserSetAttributes(thisMObject());

			MDataHandle randomSeed = data.inputValue(mRandomSeed, &status);
			fPRTModifierAction.setRandomSeed(randomSeed.asInt());

			if (ruleFileWasChanged) {
				status = fPRTModifierAction.updateRuleFiles(thisMObject(), rulePkgData.asString(), cgacProblems);

				if (status != MStatus::kSuccess) {
					return status;
				}
			}

			status = fPRTModifierAction.fillAttributesFromNode(thisMObject());
			if (status != MStatus::kSuccess)
				return status;

			// Now, perform the PRT
			status = fPRTModifierAction.doIt();

			fPRTModifierAction.updateUI(thisMObject(), cgacProblems);

			// Mark the output mesh as clean
			outputData.setClean();
		}
		else {
			status = MS::kUnknownParameter;
		}
	}

	return status;
}

MStatus PRTModifierNode::initialize()
// Description:
//  This method is called to create and initialize all of the attributes
//  and attribute dependencies for this node type.  This is only called
//  once when the node type is registered with Maya.
//
// Return Values:
//  MS::kSuccess
//  MS::kFailure
{
	MStatus status;

	MFnTypedAttribute attrFn;
	MFnEnumAttribute enumFn;

	inMesh = attrFn.create("inMesh", "im", MFnMeshData::kMesh);
	attrFn.setStorable(true); // To be stored during file-save

	// Attribute is read-only because it is an output attribute
	outMesh = attrFn.create("outMesh", "om", MFnMeshData::kMesh);
	attrFn.setStorable(false);
	attrFn.setWritable(false);

	// Add the attributes we have created to the node

	status = addAttribute(inMesh);
	if (!status) {
		status.perror("addAttribute");
		return status;
	}
	status = addAttribute(outMesh);
	if (!status) {
		status.perror("addAttribute");
		return status;
	}

	MStatus stat2;
	MStatus stat;
	MFnStringData stringData;
	MFnStringArrayData stringArrayData;
	MFnTypedAttribute fAttr;

	rulePkg = fAttr.create(NAME_RULE_PKG, "rulePkg", MFnData::kString, stringData.create(&stat2), &stat);
	MCHECK(stat2);
	MCHECK(stat);
	MCHECK(fAttr.setUsedAsFilename(true));
	MCHECK(fAttr.setCached(true));
	MCHECK(fAttr.setStorable(true));
	MCHECK(fAttr.setNiceNameOverride(MString("Rule Package(*.rpk)")));
	MCHECK(addAttribute(rulePkg));
	MCHECK(attributeAffects(rulePkg, outMesh));

	MFnNumericAttribute nAttr;
	mRandomSeed = nAttr.create(NAME_RANDOM_SEED, "randomSeed", MFnNumericData::kInt, 0, &stat);
	MCHECK(stat);
	MCHECK(nAttr.setUsedAsFilename(false));
	MCHECK(nAttr.setCached(true));
	MCHECK(nAttr.setStorable(true));
	MCHECK(nAttr.setNiceNameOverride(MString("Random Seed")));
	MCHECK(addAttribute(mRandomSeed));
	MCHECK(attributeAffects(mRandomSeed, outMesh));

	currentRulePkg = fAttr.create("current" + NAME_RULE_PKG, "currentRulePkg", MFnData::kString,
	                              stringData.create(&stat2), &stat);
	MCHECK(stat2);
	MCHECK(stat);
	MCHECK(fAttr.setCached(true));
	MCHECK(fAttr.setStorable(false));
	MCHECK(fAttr.setHidden(true));
	MCHECK(fAttr.setConnectable(false));
	MCHECK(addAttribute(currentRulePkg));

	cgacProblems =
	        fAttr.create(CGAC_PROBLEMS, "cgacProblems", MFnData::kStringArray, stringArrayData.create(&stat2), &stat);
	MCHECK(stat2);
	MCHECK(stat);
	MCHECK(fAttr.setHidden(true));
	MCHECK(fAttr.setConnectable(false));
	MCHECK(addAttribute(cgacProblems));

	// Set up a dependency between the input and the output.  This will cause
	// the output to be marked dirty when the input changes.  The output will
	// then be recomputed the next time the value of the output is requested.
	//
	status = attributeAffects(inMesh, outMesh);
	if (!status) {
		status.perror("attributeAffects");
		return status;
	}

	return MS::kSuccess;
}
