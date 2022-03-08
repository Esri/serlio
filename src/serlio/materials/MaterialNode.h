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

class MaterialInfo;
class MELScriptBuilder;

class MaterialNode : public MPxNode {

public:
	MStatus compute(const MPlug& plug, MDataBlock& data) override;

	MPxNode::SchedulingType schedulingType() const noexcept override {
		return SchedulingType::kGloballySerial;
	}

protected:
	static MStatus initializeAttributes(MObject& inMesh, MObject& outMesh);

private:
	virtual void declareMaterialStrings(MELScriptBuilder& sb) = 0;
	virtual void appendToMaterialScriptBuilder(MELScriptBuilder& sb, const MaterialInfo& matInfo,
	                                           const std::wstring& shaderBaseName,
	                                           const std::wstring& shadingEngineName) = 0;
	virtual std::wstring getBaseName() = 0;
	virtual MObject getInMesh() = 0;
	virtual MObject getOutMesh() = 0;
};
