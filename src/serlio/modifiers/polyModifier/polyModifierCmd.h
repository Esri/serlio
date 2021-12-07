//-
// ==========================================================================
// Copyright 1995,2006,2008 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk
// license agreement provided at the time of installation or download,
// or which otherwise accompanies this software in either electronic
// or hard copy form.
// ==========================================================================
//+

#ifndef _polyModifierCmd
#define _polyModifierCmd
//
// 
// File: polyModifierCmd.h
//
// MEL Command: polyModifierCmd
//
// Author: Lonnie Li
//
// Overview:
//
//		polyModifierCmd is a generic base class designed to aid in modifying
//		polygonal meshes. All polys in Maya possess two features: construction
//		history and tweaks. Both of these have a large impact on the structure
//		of the object as well as how it can be further manipulated. However,
//		they cannot be easily implemented, which is the why we need this abstracted
//		class. polyModifierCmd will automatically handle the DG maintenance of
//		construction history and tweaks on a polygonal mesh.
//
//		To understand what effect both construction history and tweaks have on
//		a mesh, we need to understand the states of a node. There are three things
//		which will affect the state of a node regarding construction history and
//		tweaks. That is:
//
//		(1) Does construction history exist?
//		(2) Do tweaks exist?
//		(3) Is construction history turned on?
//
//		The answer to each of these questions changes how the mesh is interpreted,
//		which in turn affects how the mesh can be accessed/modified. Under each
//		circumstance, new modifications on the mesh go through a standard series
//		of events. To further illustrate how these affect the interpretation of
//		a mesh, we'll delve into each case separately looking at the problems
//		that we face in each case.
//
//		In the case where construction history exists, the existence of construction
//		history informs us that there is a single linear DG chain of nodes upstream
//		from the mesh node. That chain is the history chain. At the top of the chain
//		we have the "original" mesh and at the bottom we have the "final" mesh,
//		where "original" and "final" represent the respective state of the node with
//		regards to mesh's history. Each of these nodes are adjoined via the
//		inMesh/outMesh attributes, where in and out designate the dataflow into
//		and out of the node. Now, with that aside, attempting to modify a node
//		via mutator methods will always write the information onto the inMesh
//		attribute (except in the case of tweaks, where it writes to the cachedInMesh).
//		This presents a problem if history exists since a DG evaluation will overwrite
//		the inMesh of the mesh node, due to the connection from the outMesh of the
//		node directly upstream from the mesh. This will discard any modifications
//		made to the mesh.
//
//		So obviously modifying a mesh directly isn't possible when history exists.
//		To properly modify a mesh with history, we introduce the concept of a modifier
//		node. This polyModifierNode will encapsulate the operations on the mesh
//		and behave similarly to the other nodes in the history chain. The node will
//		then be inserted into the history chain so that on each DG evaluation, it
//		is always accounted for. The diagram below shows the before and after
//		of modifying a mesh with history.
//
//
//		Before modification:
//
//        ____                   ____
//       /    \                 /    \
//      | Hist | O --------> O | mesh | O
//       \____/  |           |  \____/  |
//            outMesh      inMesh    outMesh
//
//
//		After modification:
//
//        ____                   ________                   ____
//       /    \                 /        \                 /    \
//      | Hist | O --------> O | modifier | O --------> O | mesh | O
//       \____/  |           |  \________/  |           |  \____/  |
//            outMesh      inMesh        outMesh      inMesh    outMesh
//
//
//		(Figure 1. Nodes with History)
//
//
//		In the case of tweaks: Tweaks are stored on a hidden attribute on the
//		mesh. Tweaks are manual component modifications on a mesh (eg. repositioning
//		a vertex). During a DG evaluation, the DG takes the inMesh attribute of
//		the node and adds the tweak values onto it to get the final value. From this
//		knowledge we can see that inserting a modifier node ahead of the mesh node
//		reverses the order of operations which can be crucial to the structure of the
//		resulting mesh if the modification is a topological change. To avoid this
//		problem, we retrieve the tweaks off of the mesh, remove it from the mesh and
//		place the tweaks into a polyTweak node. The tweak node is then inserted ahead
//		of the modifier node to preserve the order of operations:
//
//
//		Before modification:
//
//                               Tweak
//        ____                   __O__
//       /    \                 /     \
//      | Hist | O --------> O | mesh  | O
//       \____/  |           |  \_____/  |
//            outMesh      inMesh     outMesh
//
//
//		After modification:
//
//                                                                      Empty Tweak
//        ____                _____                ________                __O__
//       /    \              /     \              /        \              /     \
//      | Hist | O -----> O | Tweak | O -----> O | modifier | O -----> O | mesh  | O
//       \____/  |        |  \_____/  |        |  \________/  |        |  \_____/  |
//            outMesh   inMesh     outMesh   inMesh        outMesh   inMesh     outMesh
//
//
//		(Figure 2. Node with Tweaks)
//		
//
//		Since we don't care if the user has turned the construction history off, we 
//		always generate an modifier node. Otherwise, the user is unable to make any
//		changes to the serlio node. The user is imposed with the task of deleting
//		the history on the object first if they would not like to continue
//		using history. This deviates from the original polyModifierCmd implementation,
//		where the history is collapsed, but makes more sense for our use case.
//
//
//		This section has described the "why" part of the question regarding this command.
//		Following sections will provide a more in depth look at "how" this command
//		treats each of these situations and what it really does behind the scenes
//		to handle the above cases.
//
//
// How it works:
//
//		This command approaches the various node state cases similarly to the way
//		Maya works with construction history and tweaks in polygons. It is important
//		to note that history and tweaks are independent states having no effect on
//		each other (in terms of their state). Thus this section will describe each
//		case independently:
//
//		1) History
//
//			For history, there are 2 cases that need to be considered:
//
//			(a) History (yes)
//			(b) History (no)
//
//			For (a), The command performs the following steps:
//
//				(i)		Create a modifier node.
//				(ii)	Find the node directly upstream to the mesh node.
//				(iii)	Disconnect the upstream node and the mesh node.
//				(iv)	Connect the upstream node to the modifier node.
//				(v)		Connect the modifier node to the mesh node.
//				(vi)	Done!
//
//			For (b), polyModifierCmd needs to generate an input mesh to drive the
//			modifier node. To do this, the mesh node is duplicated and connected
//			like the upstream node in the previous two cases:
//
//				(i)		Create a modifier node.
//				(ii)	Duplicate the mesh node.
//				(iii)	Connect the duplicate mesh node to the modifier node
//				(iv)	Connect the modifier node to the mesh node
//				(v)		Done!
//
//		2) Tweaks
//
//			Tweaks are handled as noted above in the description section. However, how
//			they are treated is dependent on the state of history. Using the four cases
//			above:
//
//			It is as described in the description section:
//
//				(i)		Create a tweak node.
//				(ii)	Extract the tweaks from the mesh node.
//				(iii)	Copy the tweaks onto the tweak node.
//				(iv)	Clear the tweaks from the mesh node.
//				(v)		Clear the tweaks from the duplicate mesh node (for case (b) only!)
//
//
// How to use:
//
//		To use this command there are several things that are required based on the needs
//		of the command:
//
//		Step 1: polyModifierFty
//
//		1) Create a factory derived from polyModifierFty
//		2) Find and assign any inputs that your modifier will need onto the factory.
//		3) Override the polyModifierFty::doIt() method of the factory
//		4) Place your modifier code into the doIt() method of the factory
//
//		Step 2: polyModifierNode
//
//		1) Create a node derived from polyModifierNode
//		2) Add any additional input attributes onto the node
//		3) Associate the attributes (ie. inMesh --> affects --> outMesh)
//		4) Add an instance of your polyModifierFty to the node
//		5) Override the MPxNode::compute() method
//		6) Retrieve inputs from attributes, setup the factory and call its doIt() in compute()
//
//		Step 3: polyModifierCmd
//
//		1) Create a command derived from polyModifierCmd
//
//		---
//
//		2) Override the polyModifierCmd::initModifierNode() method
//		3) Place your node setup code inside the initModifierNode()
//
//		---
//
//		4) Add an instance of your polyModifierFty to the command
//		5) Cache any input parameters for the factory on the command
//
//		---
//
//		6) Override the MPxCommand::doIt() method
//		7) Place your setup code inside the doIt()
//		8) Place the polyModifierCmd setup code inside the doIt()
//		    (ie. setMeshNode(), setModifierNodeType())
//		9) Call polyModifierCmd::doModifyPoly() inside the doIt()
//
//		---
//
//		10) Override the MPxCommand::redoIt() method
//		11) Call polyModifierCmd::redoModifyPoly() in redoIt()
//
//		---
//
//		12) Override the MPxCommand::undoIt() method
//
//		For more details on each of these steps, please visit the associated method/class
//		headers.
//
//
// Limitations:
//
//		There is one limitation in polyModifierCmd:
//
//		(1) Duplicate mesh created under the "No History" case not undoable
//
//		Case (1):
//
//			Under the "No History" case, history is allowed so the DG
//			is used to perform the operation. However, every polyModifierNode requires
//			an input mesh and without any prior history, a mesh input needs to be created.
//			polyModifierCmd compensates for this by duplicating the meshNode and marking
//			it as an intermediate object.
//
//			The problem with this duplication is that the only duplicate method in the
//			Maya API resides in MFnDagNode, which does not have an associated undo/redo
//			mechanism. Attempting to manually delete the node by use of a DGmodifier or
//			the MEL delete command will break the undo/redo mechanism for the entire
//			command. As a result, this duplicate mesh is a remnant of each instance of the
//			command excluding undo/redo.
//
//			To work past this limitation, a manual delete from the MEL command line is
//			required.
//

// General Includes
//
#include <maya/MIntArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MDagPath.h>
#include <maya/MDGModifier.h>
#include <maya/MDagModifier.h>
#include <maya/MPlug.h>

// Proxies
//
#include <maya/MPxCommand.h>


class polyModifierCmd : public MPxCommand
{
public:

                        	polyModifierCmd();
						~polyModifierCmd() override;

// Restrict access to derived classes only
//
protected:

	////////////////////////////////////
	// polyModifierCmd Initialization //
	////////////////////////////////////

	// Target polyMesh to modify
	//
	void							setMeshNode( MDagPath mesh );
	MDagPath						getMeshNode() const;

	// Modifier Node Type
	//
	void							setModifierNodeType( MTypeId type );
	void							setModifierNodeName( MString name );
	MTypeId							getModifierNodeType() const;
	MString							getModifierNodeName() const;

	///////////////////////////////
	// polyModifierCmd Execution //
	///////////////////////////////

	// initModifierNode - Derived classes should override this method if
	//					  they wish to initialize input attributes on the 
	//					  modifierNode
	//
	virtual MStatus					initModifierNode( MObject modifierNode );

	MStatus							doModifyPoly();
	MStatus							redoModifyPoly();
	MStatus							undoModifyPoly();

private:

	//////////////////////////////////////////////
	// polyModifierCmd Internal Processing Data //
	//////////////////////////////////////////////

	// This structure is used to maintain the data vital to the modifyPoly method.
	// It is necessary to simplify parameter passing between the methods used inside
	// modifyPoly (specifically inside connectNodes()). The diagram below dictates
	// the naming style used:
	//
	// NOTE: modifierNode is intentionally left out of this structure since it
	//		 is given protected access to derived classes.
	//
	// Before:
	//
	// (upstreamNode) *src -> dest* (meshNode)
	//
	// After:
	//
	// (upstreamNode) *src -> dest* (modifierNode) *src -> dest* (meshNode)
	//
	struct modifyPolyData
	{
		MObject	meshNodeTransform;
		MObject	meshNodeShape;
		MPlug	meshNodeDestPlug;
		MObject	meshNodeDestAttr;

		MObject	upstreamNodeTransform;
		MObject	upstreamNodeShape;
		MPlug	upstreamNodeSrcPlug;
		MObject	upstreamNodeSrcAttr;

		MObject	modifierNodeSrcAttr;
		MObject	modifierNodeDestAttr;

		MObject	tweakNode;
		MObject tweakNodeSrcAttr;
		MObject tweakNodeDestAttr;
	};

	//////////////////////////////////////
	// polyModifierCmd Internal Methods //
	//////////////////////////////////////

	bool					isCommandDataValid();
	void					collectNodeState();

	// Modifier node methods
	//
	MStatus					createModifierNode( MObject& modifierNode );

	// Node processing methods (need to be executed in this order)
	//
	MStatus					processMeshNode( modifyPolyData& data );
	MStatus					processUpstreamNode( modifyPolyData& data );
	MStatus					processModifierNode( MObject modifierNode,
												 modifyPolyData& data );
	MStatus					processTweaks( modifyPolyData& data );

	// Node connection method
	//
	MStatus					connectNodes( MObject modifierNode );

	// Undo methods
	//
	MStatus					undoCachedMesh();
	MStatus					undoTweakProcessing();

	/////////////////////////////////////
	// polyModifierCmd Utility Methods //
	/////////////////////////////////////

	MStatus					getFloat3PlugValue( MPlug plug, MFloatVector& value );
	MStatus					getFloat3asMObject( MFloatVector value, MObject& object );

	//////////////////////////
	// polyModifierCmd Data //
	//////////////////////////

	// polyMesh
	//
	bool				fDagPathInitialized;
	MDagPath			fDagPath;
	MDagPath			fDuplicateDagPath;

	// Modifier Node Type
	//
	bool				fModifierNodeTypeInitialized;
	bool				fModifierNodeNameInitialized;
	MTypeId				fModifierNodeType;
	MString				fModifierNodeName;

	// Node State Information
	//
	bool				fHasHistory;
	bool				fHasTweaks;

	// Cached Tweak Data (for undo)
	//
	MIntArray			fTweakIndexArray;
	MFloatVectorArray	fTweakVectorArray;

	// DG and DAG Modifier
	//
	//	  - We need both DAG and DG modifiers since the MDagModifier::createNode()
	//		method is overridden and specific to DAG nodes. So to keep
	//		the operations consistent we will only use the fDagModifier
	//		when dealing with the DAG.
	//
	//	  - There is a limitation between the reparentNode() and deleteNode()
	//		methods on the MDagModifier. The deleteNode() method does some
	//		preparation work before it enqueues itself in the MDagModifier list
	//		of operations, namely, it looks at it's parents and children and
	//		deletes them as well if they are the only parent/child of the node
	//		scheduled to be deleted.
	//
	//		This conflicts with our call to MDagModifier::reparentNode(),
	//		since we want to reparent the shape of a duplicated node under
	//		another node and then delete the transform of that node. Now you 
	//		can see that since the reparentNode() doesn't execute until after
	//		the MDagModifier::doIt() call, the scheduled deleteNode() call
	//		still sees the child and marks it for delete. The subsequent
	//		doIt() call reparents the shape and then deletes both it and the
	//		transform.
	//
	//		To avoid this conflict, we separate the calls individually and
	//		perform the reparenting (by calling a doIt()) before the deleteNode()
	//		method is enqueued on the modifier.
	//
	MDGModifier			fDGModifier;
	MDagModifier		fDagModifier;
};

//
// Inlines
//

// polyMesh
//
inline void polyModifierCmd::setMeshNode( MDagPath mesh )
{
	fDagPath = mesh;
	fDagPathInitialized = true;
}

inline MDagPath polyModifierCmd::getMeshNode() const
{
	return fDagPath;
}

// Modifier Node Type
//
inline void polyModifierCmd::setModifierNodeType( MTypeId type )
{
	fModifierNodeType = type;
	fModifierNodeTypeInitialized = true;
}

inline void polyModifierCmd::setModifierNodeName( MString name )
{
	fModifierNodeName = name;
	fModifierNodeNameInitialized = true;
}

inline MTypeId polyModifierCmd::getModifierNodeType() const
{
	return fModifierNodeType;
}

inline MString polyModifierCmd::getModifierNodeName() const
{
	return fModifierNodeName;
}

#endif
