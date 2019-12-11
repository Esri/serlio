#pragma once

#include "prtMaterial/MaterialInfo.h"

#include "maya/MPlug.h"
#include "maya/MStatus.h"
#include "maya/MString.h"

#include <map>

namespace MaterialUtils {

MStatus getMeshName(MString& meshName, const MPlug& plug);

using MaterialCache = std::map<MaterialInfo, MObject>;
MaterialCache getMaterialsByStructure(const std::string& structureName, MFn::Type nodeFilter, const MString& typeName);

void assignMaterialMetadata(const adsk::Data::Structure* materialStructure, const adsk::Data::Handle& streamHandle,
                            const std::wstring& shaderName);

} // namespace MaterialUtils