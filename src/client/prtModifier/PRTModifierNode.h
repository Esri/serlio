#pragma once

#include "polyModifierBase/polyModifierNode.h"
#include "PRTModifierAction.h"

#include <maya/MTypeId.h>

class PRTModifierNode : public polyModifierNode
{
public:
	PRTModifierNode() = default;

	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	virtual MStatus setDependentsDirty(const MPlug &plugBeingDirtied, MPlugArray &affectedPlugs) override;

	static  void*   creator();
	static  MStatus initialize();

public:

	static MObject      rulePkg;
	static MTypeId      id;
	MString             currentRulePkg;

	PRTModifierAction   fPRTModifierAction;

};

