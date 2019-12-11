#pragma once

#include "maya/MPlug.h"
#include "maya/MStatus.h"
#include "maya/MString.h"

namespace MaterialUtils {

MStatus getMeshName(MString& meshName, const MPlug& plug);

} // namespace MaterialUtils