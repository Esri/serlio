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

#include "materials/MaterialInfo.h"

#include "utils/MELScriptBuilder.h"

#include "maya/MPlug.h"
#include "maya/MStatus.h"
#include "maya/MString.h"
#include "maya/adskDataStream.h"

#include <map>

namespace MaterialUtils {

void forwardGeometry(const MObject& aInMesh, const MObject& aOutMesh, MDataBlock& data);
adsk::Data::Stream* getMaterialStream(const MObject& aInMesh, MDataBlock& data);

MStatus getMeshName(MString& meshName, const MPlug& plug);

using MaterialCache = std::map<size_t, MUuid>;

MaterialCache getMaterialCache();

bool getFaceRange(adsk::Data::Handle& handle, std::pair<int, int>& faceRange);

MStatus addMaterialInfoMapMetadata(size_t materialInfoHash, const MUuid& shadingEngineUuid);

std::wstring synchronouslyCreateShadingEngine(const std::wstring& desiredShadingEngineName,
                                              const MELVariable& shadingEngineVariable, MStatus& status);

std::filesystem::path getStingrayShaderPath();

bool textureHasAlphaChannel(const std::wstring& path);

void resetMaterial(const std::wstring& meshName);
} // namespace MaterialUtils