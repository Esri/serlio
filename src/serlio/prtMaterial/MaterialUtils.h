#pragma once

#include "prtMaterial/MaterialInfo.h"

#include "maya/MPlug.h"
#include "maya/MStatus.h"
#include "maya/MString.h"
#include "maya/adskDataStream.h"

#include <map>

namespace MaterialUtils {

adsk::Data::Stream* getMaterialStream(MObject& aOutMesh, MObject& aInMesh, MDataBlock& data);

MStatus getMeshName(MString& meshName, const MPlug& plug);

using MaterialCache = std::map<MaterialInfo, std::wstring>;
MaterialCache getMaterialsByStructure(const adsk::Data::Structure& materialStructure);

void assignMaterialMetadata(const adsk::Data::Structure& materialStructure, const adsk::Data::Handle& streamHandle,
                            const std::wstring& shadingEngineName);

} // namespace MaterialUtils