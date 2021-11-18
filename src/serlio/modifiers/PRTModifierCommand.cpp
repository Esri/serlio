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

#include "modifiers/PRTModifierCommand.h"
#include "modifiers/PRTModifierNode.h"

#include "utils/MayaUtilities.h"

#include "maya/MArgList.h"
#include "maya/MFloatPointArray.h"
#include "maya/MFnMesh.h"
#include "maya/MGlobal.h"
#include "maya/MItSelectionList.h"

bool PRTModifierCommand::isUndoable() const {
	return true;
}

// implements the MEL PRT command, based on the Maya example splitUvCmd
MStatus PRTModifierCommand::doIt(const MArgList& argList) {
	MStatus status;

	if (argList.length() == 1) {
		mRulePkg = argList.asString(0);
	}
	else {
		displayError(" Expecting one parameter: the operation type.");
		return MS::kFailure;
	}

	// Parse the selection list for selected components of the right type.
	MSelectionList selList;
	MGlobal::getActiveSelectionList(selList);
	MItSelectionList selListIter(selList);
	selListIter.setFilter(MFn::kMesh);

	bool found = false;
	bool foundMultiple = false;

	for (; !selListIter.isDone(); selListIter.next()) {
		MDagPath dagPath;
		MObject component;
		selListIter.getDagPath(dagPath, component);

		if (!found) {
			// Ensure that this DAG path will point to the shape
			// of our object. Set the DAG path for the polyModifierCmd.
			if ((dagPath.extendToShape() == MStatus::kSuccess) ||
			    (dagPath.extendToShapeDirectlyBelow(0) == MStatus::kSuccess)) {
				setMeshNode(dagPath);
				found = true;
			}
		}
		else {
			foundMultiple = true;
			break;
		}
	}
	if (foundMultiple) {
		displayWarning("Found more than one object with selected components.");
		displayWarning("Only operating on first found object.");
	}

	// Initialize the polyModifierCmd node type - mesh node already set
	//
	setModifierNodeType(PRTModifierNode::id);

	if (found) {
		MFnMesh meshFn(getMeshNode());

		MFloatPointArray vertices;
		MCHECK(meshFn.getPoints(vertices, MSpace::kWorld));
		mInitialSeed = mu::computeSeed(vertices);

		// Now, pass control over to the polyModifierCmd::doModifyPoly() method
		// to handle the operation.
		status = doModifyPoly();

		if (status == MS::kSuccess) {
			setResult("PRT command succeeded!");
		}
		else {
			displayError("PRT command failed!");
		}
	}
	else {
		displayError("PRT command failed: Unable to find selected components");
		status = MS::kFailure;
	}

	return status;
}

MStatus PRTModifierCommand::redoIt() {
	MStatus status;
	status = redoModifyPoly();

	if (status == MS::kSuccess) {
		setResult("PRT command succeeded!");
	}
	else {
		displayError("PRT command failed!");
	}

	return status;
}

MStatus PRTModifierCommand::undoIt() {
	MStatus status;
	status = undoModifyPoly();
	if (status == MS::kSuccess) {
		setResult("PRT undo succeeded!");
	}
	else {
		setResult("PRT undo failed!");
	}
	return status;
}

MStatus PRTModifierCommand::initModifierNode(MObject modifierNode) {
	MStatus status;
	MFnDependencyNode depNodeFn(modifierNode);

	MObject attr = depNodeFn.attribute("Rule_Package");
	MPlug plug(modifierNode, attr);
	status = plug.setValue(mRulePkg);

	MObject attrSeed = depNodeFn.attribute("Random_Seed");
	MPlug plugRnd(modifierNode, attrSeed);
	plugRnd.setValue(mInitialSeed);

	return status;
}

MStatus PRTModifierCommand::directModifier(MObject mesh) {
	MStatus status;
	PRTModifierAction fPRTModifierAction;

	fPRTModifierAction.setMesh(mesh, mesh);
	fPRTModifierAction.setRandomSeed(mInitialSeed);
	fPRTModifierAction.updateRuleFiles(MObject::kNullObj, mRulePkg);
	fPRTModifierAction.clearTweaks(mesh);

	// Now, perform the PRT action
	status = fPRTModifierAction.doIt();

	return status;
}
