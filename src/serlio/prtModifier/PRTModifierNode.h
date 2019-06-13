#pragma once

#include "polyModifier/polyModifierNode.h"
#include "prtModifier/PRTModifierAction.h"

#include "maya/MObject.h"
#include "maya/MTypeId.h"
#include "maya/MStatus.h"


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
	static MObject      currentRulePkg;
	static MTypeId      id;
	static MObject      mRandomSeed;

	PRTModifierAction   fPRTModifierAction;

};

