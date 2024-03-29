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

#pragma once

#include <maya/MDGModifier.h>
#include <maya/MDagModifier.h>
#include <maya/MDagPath.h>
#include <maya/MPxCommand.h>

#include "PRTContext.h"

class MaterialCommand : public MPxCommand {
public:
	bool isUndoable() const override;

	MStatus doIt(const MArgList&) override;
	MStatus redoIt() override;
	MStatus undoIt() override;

private:
	MDGModifier fDGModifier;
	MDagModifier fDagModifier;
	MDagPath fDagPath;
	MString fGeometryName;
};
