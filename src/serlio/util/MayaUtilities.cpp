#include "util/MayaUtilities.h"

#include <memory>


namespace mu {

MString toCleanId(const MString& name) {
	const unsigned int len = name.numChars();
	const wchar_t*     wname = name.asWChar();
	auto dst = std::make_unique<wchar_t[]>(len + 1);
	for (unsigned int i = 0; i < len; i++) {
		wchar_t c = wname[i];
		if ((c >= '0' && c <= '9') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z'))
			dst[i] = c;
		else
			dst[i] = '_';
	}
	dst[len] = L'\0';
	MString result(dst.get());
	return result;
}

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
		LOG_ERR << "maya status error at " << file << ":" << line << ": " << status.errorString().asChar() << " (code " << status.statusCode() << ")";
	}
}

} // namespace mu