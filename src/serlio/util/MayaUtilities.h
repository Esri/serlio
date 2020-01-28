#pragma once

#include "util/LogHandler.h"

#include "maya/MFloatPointArray.h"
#include "maya/MFnAttribute.h"
#include "maya/MFnDependencyNode.h"
#include "maya/MObject.h"
#include "maya/MStatus.h"
#include "maya/MString.h"

#define MCHECK(status) mu::statusCheck((status), __FILE__, __LINE__);

// utility functions with dependencies on the Maya API
namespace mu {

int32_t computeSeed(const MFloatPointArray& vertices);
int32_t computeSeed(const double* vertices, size_t count);

void statusCheck(const MStatus& status, const char* file, int line);

template <typename F>
void forAllAttributes(const MFnDependencyNode& node, F func) {
	for (unsigned int i = 0; i < node.attributeCount(); i++) {
		const MObject attrObj = node.attribute(i);
		const MFnAttribute attr(attrObj);
		func(attr);
	}
}

} // namespace mu