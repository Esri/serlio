#pragma once

#include "maya/MGlobal.h"
#include "maya/MStatus.h"

#include <set>

class MaterialNameSource final {
public:
	MaterialNameSource();
	MaterialNameSource(const MaterialNameSource&) = delete;
	MaterialNameSource(MaterialNameSource&&) = delete;
	~MaterialNameSource() = default;

	std::pair<std::wstring, std::wstring> getUniqueName(const std::wstring& baseName);

private:
	std::set<std::wstring> names;
};
