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

#include "maya/MPxNode.h"
#include "maya/MString.h"
#include "maya/adskDataHandle.h"

#include <array>

class MaterialColor;
struct PRTContext;
class MaterialInfo;
class MaterialTrafo;
class MELScriptBuilder;

class StingrayMaterialNode : public MPxNode {
public:
	static MStatus initialize();
	MStatus compute(const MPlug& plug, MDataBlock& data) override;

	static MTypeId id;
	static MObject aInMesh;
	static MObject aOutMesh;
};
