#pragma once

#include "maya/MStatus.h"
#include "maya/MString.h"
#include "maya/MFloatPointArray.h"


// utility functions with dependencies on the Maya API
namespace mu {

MString toCleanId(const MString& name);

int32_t computeSeed(const MFloatPointArray& vertices);
int32_t computeSeed(const double* vertices, size_t count);

} // namespace mu