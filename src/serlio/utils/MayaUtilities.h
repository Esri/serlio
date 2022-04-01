#pragma once

#include "utils/LogHandler.h"

#include "maya/MFloatPointArray.h"
#include "maya/MFnAttribute.h"
#include "maya/MFnDependencyNode.h"
#include "maya/MObject.h"
#include "maya/MStatus.h"
#include "maya/MString.h"

#define MCHECK(status) mu::statusCheck((status), __FILE__, __LINE__);

// utility functions with dependencies on the Maya API
namespace mu {
// Standard conversion from meters (PRT) to centimeters (maya)
constexpr double PRT_TO_SERLIO_SCALE = 100.0f;

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

// credits: https://www.fluentcpp.com/2016/12/08/strong-types-for-strong-interfaces/
template <typename T, typename Parameter>
class NamedType {
public:
	explicit NamedType(T const& value) : value_(value) {}
	explicit NamedType(T&& value) : value_(std::move(value)) {}
	T& get() {
		return value_;
	}
	T const& get() const noexcept {
		return value_;
	}

private:
	T value_;
};

std::filesystem::path getWorkspaceRoot(MStatus& status);

} // namespace mu

bool operator==(const MStringArray& lhs, const MStringArray& rhs);
bool operator!=(const MStringArray& lhs, const MStringArray& rhs);