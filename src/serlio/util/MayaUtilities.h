#pragma once

#include "util/LogHandler.h"

#include "maya/MStatus.h"
#include "maya/MString.h"
#include "maya/MFloatPointArray.h"


#define MCHECK(status) mu::statusCheck((status), __FILE__, __LINE__);

// utility functions with dependencies on the Maya API
namespace mu {

MString toCleanId(const MString& name);

int32_t computeSeed(const MFloatPointArray& vertices);
int32_t computeSeed(const double* vertices, size_t count);

void statusCheck(MStatus status, const char* file, int line);

} // namespace mu