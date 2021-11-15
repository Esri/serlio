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

#include "materials/StingrayMaterialNode.h"
#include "materials/ArnoldMaterialNode.h"
#include "materials/MaterialCommand.h"

#include "utils/MayaUtilities.h"

#include "maya/MArgList.h"
#include "maya/MFloatPointArray.h"
#include "maya/MFnMesh.h"
#include "maya/MGlobal.h"
#include "maya/MItDependencyGraph.h"
#include "maya/MItSelectionList.h"

bool MaterialCommand::isUndoable() const {
	return true;
}

MStatus MaterialCommand::doIt(const MArgList& argList) {
	MStatus status;

	if (argList.length() == 0) {
		cerr << "Material type expected (stingray/arnold)" << endl;
		displayError("Material type expected (stingray/arnold)");
		return MS::kFailure;
	}
	else if (argList.length() > 1) {
		cerr << "Only one material type expected" << endl;
		displayError("Only one material type expected");
		return MS::kFailure;
	}
	
	MString materialType = argList.asString(0, &status);
	MCHECK(status);

	MTypeId materialTypeId;
	if (materialType == "stingray") {
		materialTypeId = StingrayMaterialNode::id;
	}
	else if (materialType == "arnold") {
		materialTypeId = ArnoldMaterialNode::id;
	}
	else {
		cerr << "Material type expected (stingray/arnold)" << endl;
		displayError("Material type expected (stingray/arnold)");
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
				fDagPath = dagPath;
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

	if (found) {
		MObject materialDependencyNodeObject = fDGModifier.createNode(materialTypeId);
		MFnDependencyNode materialDependencyNode(materialDependencyNodeObject);

		MPlug materialInMesh = materialDependencyNode.findPlug("inMesh", true, &status);
		MPlug materialOutMesh = materialDependencyNode.findPlug("outMesh", true, &status);

		MObject rootObj = fDagPath.node();
		MFnDependencyNode rootNode(rootObj);

		MPlug geometryInMesh = rootNode.findPlug("inMesh", true, &status);
		MPlug geometryOutMesh = rootNode.findPlug("outMesh", true, &status);

		if (MS::kSuccess != status) {
			MGlobal::displayError("Status failed: no inMesh Attribute on node \"" + rootNode.name() + "\"");
			return status;
		} 
		//has construction history
		if (geometryInMesh.isConnected()) {
			MPlugArray parentPlugArray;
			MPlug serlioOutMesh;

			geometryInMesh.connectedTo(parentPlugArray, true, false);

			serlioOutMesh = parentPlugArray[0];

			fDGModifier.disconnect(serlioOutMesh, geometryInMesh);
			fDGModifier.connect(serlioOutMesh, materialInMesh);
			fDGModifier.connect(materialOutMesh, geometryInMesh);
		} 
		// has no construction history
		else {
			MDataHandle meshHandle = geometryOutMesh.asMDataHandle();
			materialInMesh.setMDataHandle(meshHandle);
			fDGModifier.connect(materialOutMesh, geometryInMesh);
		}

		status = fDGModifier.doIt();

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

MStatus MaterialCommand::redoIt() {
	MStatus status;
	status = fDGModifier.doIt();

	if (status == MS::kSuccess) {
		setResult("PRT command succeeded!");
	}
	else {
		displayError("PRT command failed!");
	}

	return status;
}

MStatus MaterialCommand::undoIt() {
	MStatus status;
	status = fDGModifier.undoIt();
	if (status == MS::kSuccess) {
		setResult("PRT undo succeeded!");
	}
	else {
		setResult("PRT undo failed!");
	}
	return status;
}