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

bool MaterialTrafo::operator<(const MaterialTrafo& rhs) const noexcept {
	return this->data < rhs.data;
}

bool MaterialTrafo::operator>(const MaterialTrafo& rhs) const noexcept {
	return rhs < *this;
}

MaterialInfo::MaterialInfo(adsk::Data::Handle& handle)
    : bumpMap(getTexture(handle, "bumpMap")), colormap(getTexture(handle, "diffuseMap")),
      dirtmap(getTexture(handle, "diffuseMap1")), emissiveMap(getTexture(handle, "emissiveMap")),
      metallicMap(getTexture(handle, "metallicMap")), normalMap(getTexture(handle, "normalMap")),
      occlusionMap(getTexture(handle, "occlusionMap")), opacityMap(getTexture(handle, "opacityMap")),
      roughnessMap(getTexture(handle, "roughnessMap")), specularMap(getTexture(handle, "specularMap")),

      opacity(getDouble(handle, "opacity")), metallic(getDouble(handle, "metallic")),
      roughness(getDouble(handle, "roughness")),

      ambientColor(handle, "ambientColor"), bumpmapTrafo(handle, "bumpmapTrafo"),
      colormapTrafo(handle, "colormapTrafo"), diffuseColor(handle, "diffuseColor"),
      dirtmapTrafo(handle, "dirtmapTrafo"), emissiveColor(handle, "emissiveColor"),
      emissivemapTrafo(handle, "emissivemapTrafo"), metallicmapTrafo(handle, "metallicmapTrafo"),
      normalmapTrafo(handle, "normalmapTrafo"), occlusionmapTrafo(handle, "occlusionmapTrafo"),
      opacitymapTrafo(handle, "opacitymapTrafo"), roughnessmapTrafo(handle, "roughnessmapTrafo"),
      specularColor(handle, "specularColor"), specularmapTrafo(handle, "specularmapTrafo") {}

bool MaterialInfo::equals(const MaterialInfo& o) const {
	// clang-format off
	return
	        bumpMap == o.bumpMap &&
	        colormap == o.colormap &&
	        dirtmap == o.dirtmap &&
	        emissiveMap == o.emissiveMap &&
	        metallicMap == o.metallicMap &&
	        normalMap == o.normalMap &&
	        occlusionMap == o.occlusionMap &&
	        opacityMap == o.opacityMap &&
	        roughnessMap == o.roughnessMap &&
	        specularMap == o.specularMap &&
	        opacity == o.opacity &&
	        metallic == o.metallic &&
	        roughness == o.roughness &&
	        ambientColor == o.ambientColor &&
	        bumpmapTrafo == o.bumpmapTrafo &&
	        colormapTrafo == o.colormapTrafo &&
	        diffuseColor == o.diffuseColor &&
	        dirtmapTrafo == o.dirtmapTrafo &&
	        emissiveColor == o.emissiveColor &&
	        emissivemapTrafo == o.emissivemapTrafo &&
	        metallicmapTrafo == o.metallicmapTrafo &&
	        normalmapTrafo == o.normalmapTrafo &&
	        occlusionmapTrafo == o.occlusionmapTrafo &&
	        opacitymapTrafo == o.opacitymapTrafo &&
	        roughnessmapTrafo == o.roughnessmapTrafo &&
	        specularColor == o.specularColor &&
	        specularmapTrafo == o.specularmapTrafo;
	// clang-format on
}

bool MaterialInfo::operator<(const MaterialInfo& rhs) const {
	// clang-format off
	{ int c = bumpMap.compare(rhs.bumpMap);           if (c != 0) return (c < 0); }
	{ int c = colormap.compare(rhs.colormap);         if (c != 0) return (c < 0); }
	{ int c = dirtmap.compare(rhs.dirtmap);           if (c != 0) return (c < 0); }
	{ int c = emissiveMap.compare(rhs.emissiveMap);   if (c != 0) return (c < 0); }
	{ int c = metallicMap.compare(rhs.metallicMap);   if (c != 0) return (c < 0); }
	{ int c = normalMap.compare(rhs.normalMap);       if (c != 0) return (c < 0); }
	{ int c = occlusionMap.compare(rhs.occlusionMap); if (c != 0) return (c < 0); }
	{ int c = opacityMap.compare(rhs.opacityMap);     if (c != 0) return (c < 0); }
	{ int c = roughnessMap.compare(rhs.roughnessMap); if (c != 0) return (c < 0); }
	{ int c = specularMap.compare(rhs.specularMap);   if (c != 0) return (c < 0); }

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

	if (specularmapTrafo > rhs.specularmapTrafo) return false;
	if (specularmapTrafo < rhs.specularmapTrafo) return true;

	if (bumpmapTrafo > rhs.bumpmapTrafo) return false;
	if (bumpmapTrafo < rhs.bumpmapTrafo) return true;

	if (dirtmapTrafo > rhs.dirtmapTrafo) return false;
	if (dirtmapTrafo < rhs.dirtmapTrafo) return true;

	if (emissivemapTrafo > rhs.emissivemapTrafo) return false;
	if (emissivemapTrafo < rhs.emissivemapTrafo) return true;

	if (metallicmapTrafo > rhs.metallicmapTrafo) return false;
	if (metallicmapTrafo < rhs.metallicmapTrafo) return true;

	if (normalmapTrafo > rhs.normalmapTrafo) return false;
	if (normalmapTrafo < rhs.normalmapTrafo) return true;

	if (occlusionmapTrafo > rhs.occlusionmapTrafo) return false;
	if (occlusionmapTrafo < rhs.occlusionmapTrafo) return true;

	if (opacitymapTrafo > rhs.opacitymapTrafo) return false;
	if (opacitymapTrafo < rhs.opacitymapTrafo) return true;

	if (roughnessmapTrafo > rhs.roughnessmapTrafo) return false;
	if (roughnessmapTrafo < rhs.roughnessmapTrafo) return true;

	if (specularmapTrafo > rhs.specularmapTrafo) return false;
	if (specularmapTrafo < rhs.specularmapTrafo) return true;
	// clang-format on

	return false; // equality
}
