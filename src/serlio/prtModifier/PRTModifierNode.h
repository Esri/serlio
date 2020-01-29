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

#include "PRTContext.h"

#include "polyModifier/polyModifierNode.h"
#include "prtModifier/PRTModifierAction.h"

#include "maya/MObject.h"
#include "maya/MStatus.h"
#include "maya/MTypeId.h"

class PRTModifierNode : public polyModifierNode {
public:
	explicit PRTModifierNode(const PRTContext& prtCtx) : mPRTCtx(prtCtx), fPRTModifierAction(prtCtx) {}

	MStatus compute(const MPlug& plug, MDataBlock& data) override;
	MStatus setDependentsDirty(const MPlug& plugBeingDirtied, MPlugArray& affectedPlugs) override;

	static MStatus initialize();

public:
	// non-dynamic node attributes
	static MObject rulePkg;
	static MObject currentRulePkg;
	static MTypeId id;
	static MObject mRandomSeed;

	const PRTContext& mPRTCtx;
	PRTModifierAction fPRTModifierAction;
};
