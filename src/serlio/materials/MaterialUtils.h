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