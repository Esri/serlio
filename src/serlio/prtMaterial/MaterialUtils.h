#pragma once

#include "prtMaterial/MaterialInfo.h"
#include "util/MELScriptBuilder.h"

#include "maya/MPlug.h"
#include "maya/MStatus.h"
#include "maya/MString.h"
#include "maya/adskDataStream.h"

#include <map>

namespace MaterialUtils {

void forwardGeometry(const MObject& aInMesh, const MObject& aOutMesh, MDataBlock& data);
adsk::Data::Stream* getMaterialStream(const MObject& aInMesh, MDataBlock& data);

MStatus getMeshName(MString& meshName, const MPlug& plug);

using MaterialCache = std::map<MaterialInfo, std::wstring>;
MaterialCache getMaterialsByStructure(const adsk::Data::Structure& materialStructure, const std::wstring& baseName);

bool getFaceRange(adsk::Data::Handle& handle, std::pair<int, int>& faceRange);

void assignMaterialMetadata(const adsk::Data::Structure& materialStructure, const adsk::Data::Handle& streamHandle,
                            const std::wstring& shadingEngineName);

std::wstring synchronouslyCreateShadingEngine(const std::wstring& desiredShadingEngineName,
                                              const MELVariable& shadingEngineVariable, MStatus& status);

std::wstring getStingrayShaderPath();

} // namespace MaterialUtils