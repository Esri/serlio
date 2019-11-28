#include "util/MayaUtilities.h"

#include <memory>

namespace mu {

int32_t computeSeed(const MFloatPoint& p) {
	float x = p[0];
	float z = p[2];
	int32_t seed = *(int32_t*)(&x);
	seed ^= *(int32_t*)(&z);
	seed %= 714025;
	return seed;
}

int32_t computeSeed(const MFloatPointArray& vertices) {
	MFloatPoint a(0.0, 0.0, 0.0);
	for (unsigned int vi = 0; vi < vertices.length(); vi++) {
		a += vertices[vi];
	}
	a = a / static_cast<float>(vertices.length());
	return computeSeed(a);
}

int32_t computeSeed(const double* vertices, size_t count) {
	MFloatPoint a(0.0, 0.0, 0.0);
	for (unsigned int vi = 0; vi < count; vi += 3) {
		a[0] += static_cast<float>(vertices[vi + 0]);
		a[1] += static_cast<float>(vertices[vi + 1]);
		a[2] += static_cast<float>(vertices[vi + 2]);
	}
	a = a / static_cast<float>(count);
	return computeSeed(a);
}

void statusCheck(MStatus status, const char* file, int line) {
	if (MS::kSuccess != status) {
		LOG_ERR << "maya status error at " << file << ":" << line << ": " << status.errorString().asChar() << " (code "
		        << status.statusCode() << ")";
	}
}

} // namespace mu