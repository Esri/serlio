/**
 * Serlio - Esri CityEngine Plugin for Autodesk Maya
 *
 * See https://github.com/esri/serlio for build and usage instructions.
 *
 * Copyright (c) 2012-2019 Esri R&D Center Zurich
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "materials/MaterialInfo.h"

#include <cmath>

namespace {

template <size_t N>
void getDoubleArray(std::array<double, N>& array, adsk::Data::Handle& sHandle, const std::string& name) {
	if (sHandle.setPositionByMemberName(name.c_str())) {
		double* data = sHandle.asDouble();
		if (sHandle.dataLength() >= N && data) {
			std::copy(data, data + N, array.begin());
			return;
		}
	}
	array.fill(0.0);
}

std::filesystem::path getTexture(adsk::Data::Handle& sHandle, const std::string& texName) {
	std::filesystem::path r;
	if (sHandle.setPositionByMemberName(texName.c_str()))
		r.assign((char*)sHandle.asUInt8());
	return r;
}

double getDouble(adsk::Data::Handle& sHandle, const std::string& name) {
	if (sHandle.setPositionByMemberName(name.c_str())) {
		double* data = sHandle.asDouble();
		if (sHandle.dataLength() >= 1 && data != nullptr) {
			return *data;
		}
	}
	return NAN;
}

} // namespace

MaterialColor::MaterialColor(adsk::Data::Handle& sHandle, const std::string& name) {
	getDoubleArray(data, sHandle, name);
}

double MaterialColor::r() const noexcept {
	return data[0];
}

double MaterialColor::g() const noexcept {
	return data[1];
}

double MaterialColor::b() const noexcept {
	return data[2];
}

bool MaterialColor::operator==(const MaterialColor& other) const noexcept {
	return this->data == other.data;
}

bool MaterialColor::operator<(const MaterialColor& rhs) const noexcept {
	return this->data < rhs.data;
}

bool MaterialColor::operator>(const MaterialColor& rhs) const noexcept {
	return rhs < *this;
}

MaterialTrafo::MaterialTrafo(adsk::Data::Handle& sHandle, const std::string& name) {
	getDoubleArray(data, sHandle, name);
}

double MaterialTrafo::su() const noexcept {
	return data[0];
}

double MaterialTrafo::sv() const noexcept {
	return data[1];
}

double MaterialTrafo::tu() const noexcept {
	return data[2];
}

double MaterialTrafo::tv() const noexcept {
	return data[3];
}

double MaterialTrafo::rw() const noexcept {
	return data[4];
}

std::array<double, 2> MaterialTrafo::tuv() const noexcept {
	return {tu(), tv()};
}

std::array<double, 3> MaterialTrafo::suvw() const noexcept {
	return {su(), sv(), rw()};
}

bool MaterialTrafo::operator==(const MaterialTrafo& other) const noexcept {
	return this->data == other.data;
}

bool MaterialTrafo::operator!=(const MaterialTrafo& other) const noexcept {
	return this->data != other.data;
}

bool MaterialTrafo::operator<(const MaterialTrafo& rhs) const noexcept {
	return this->data < rhs.data;
}

bool MaterialTrafo::operator>(const MaterialTrafo& rhs) const noexcept {
	return rhs < *this;
}

MaterialInfo::MaterialInfo(adsk::Data::Handle& handle)
    : texturePaths{getTexture(handle, "bumpMap"),      getTexture(handle, "diffuseMap"),
                   getTexture(handle, "diffuseMap1"),  getTexture(handle, "emissiveMap"),
                   getTexture(handle, "metallicMap"),  getTexture(handle, "normalMap"),
                   getTexture(handle, "occlusionMap"), getTexture(handle, "opacityMap"),
                   getTexture(handle, "roughnessMap"), getTexture(handle, "specularMap")},

      opacity(getDouble(handle, "opacity")), metallic(getDouble(handle, "metallic")),
      roughness(getDouble(handle, "roughness")),

      ambientColor(handle, "ambientColor"), diffuseColor(handle, "diffuseColor"),
      emissiveColor(handle, "emissiveColor"), specularColor(handle, "specularColor"),

      textureTrafos{{{handle, "bumpmapTrafo"},
                     {handle, "colormapTrafo"},
                     {handle, "dirtmapTrafo"},
                     {handle, "emissivemapTrafo"},
                     {handle, "metallicmapTrafo"},
                     {handle, "normalmapTrafo"},
                     {handle, "occlusionmapTrafo"},
                     {handle, "opacitymapTrafo"},
                     {handle, "roughnessmapTrafo"},
                     {handle, "specularmapTrafo"}}} {}

bool MaterialInfo::equals(const MaterialInfo& o) const {
	for (size_t t = 0; t < static_cast<size_t>(TextureSemantic::COUNT); t++) {
		if (texturePaths[t] != o.texturePaths[t])
			return false;
		if (textureTrafos[t] != o.textureTrafos[t])
			return false;
	}

	// clang-format off
	return	opacity == o.opacity &&
	        metallic == o.metallic &&
	        roughness == o.roughness &&
	        ambientColor == o.ambientColor &&
			diffuseColor == o.diffuseColor &&
			emissiveColor == o.emissiveColor &&
			specularColor == o.specularColor;
	// clang-format on
}

bool MaterialInfo::operator<(const MaterialInfo& rhs) const {
	for (size_t t = 0; t < static_cast<size_t>(TextureSemantic::COUNT); t++) {
		const int c = texturePaths[t].compare(rhs.texturePaths[t]);
		if (c != 0)
			return (c < 0);
		if (textureTrafos[t] > rhs.textureTrafos[t])
			return false;
		if (textureTrafos[t] < rhs.textureTrafos[t])
			return true;
	}

	// clang-format off
	if (opacity > rhs.opacity) return false;
	if (opacity < rhs.opacity) return true;

	if (metallic > rhs.metallic) return false;
	if (metallic < rhs.metallic) return true;

	if (roughness > rhs.roughness) return false;
	if (roughness < rhs.roughness) return true;

	if (ambientColor > rhs.ambientColor) return false;
	if (ambientColor < rhs.ambientColor) return true;

	if (diffuseColor > rhs.diffuseColor) return false;
	if (diffuseColor < rhs.diffuseColor) return true;

	if (emissiveColor > rhs.emissiveColor) return false;
	if (emissiveColor < rhs.emissiveColor) return true;

	if (specularColor > rhs.specularColor) return false;
	if (specularColor < rhs.specularColor) return true;
	// clang-format on

	return false; // equality
}
