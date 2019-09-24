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

#include "prtMaterial/MaterialInfo.h"

#include <cmath>

double MaterialColor::r() const noexcept {
	return (*this)[0];
}

double MaterialColor::g() const noexcept {
	return (*this)[1];
}

double MaterialColor::b() const noexcept {
	return (*this)[2];
}

double MaterialTrafo::su() const noexcept {
	return (*this)[0];
}

double MaterialTrafo::sv() const noexcept {
	return (*this)[1];
}

double MaterialTrafo::tu() const noexcept {
	return (*this)[2];
}

double MaterialTrafo::tv() const noexcept {
	return (*this)[3];
}

double MaterialTrafo::rw() const noexcept {
	return (*this)[4];
}

std::array<double, 2> MaterialTrafo::tuv() const noexcept {
	return {tu(), tv()};
}

std::array<double, 3> MaterialTrafo::suvw() const noexcept {
	return {su(), sv(), rw()};
}

MaterialColor MaterialInfo::getColor(adsk::Data::Handle sHandle, const std::string& name) {
	MaterialColor color;
	getDoubleArray(color, sHandle, name);
	return color;
}

MaterialTrafo MaterialInfo::getTrafo(adsk::Data::Handle sHandle, const std::string& name) {
	MaterialTrafo trafo;
	getDoubleArray(trafo, sHandle, name);
	return trafo;
}

std::string MaterialInfo::getTexture(adsk::Data::Handle sHandle, const std::string& texName) {
	std::string r;
	if (sHandle.setPositionByMemberName(texName.c_str()))
		r = (char*)sHandle.asUInt8();
	return r;
}

double MaterialInfo::getDouble(adsk::Data::Handle sHandle, const std::string& name) {
	if (sHandle.setPositionByMemberName(name.c_str())) {
		double* data = sHandle.asDouble();
		if (sHandle.dataLength() >= 1 && data != nullptr) {
			return *data;
		}
	}
	return NAN;
}

MaterialInfo::MaterialInfo(adsk::Data::Handle sHandle) {
	bumpMap = getTexture(sHandle, "bumpMap");
	colormap = getTexture(sHandle, "diffuseMap");
	dirtmap = getTexture(sHandle, "diffuseMap1");
	emissiveMap = getTexture(sHandle, "emissiveMap");
	metallicMap = getTexture(sHandle, "metallicMap");
	normalMap = getTexture(sHandle, "normalMap");
	occlusionMap = getTexture(sHandle, "occlusionMap");
	opacityMap = getTexture(sHandle, "opacityMap");
	roughnessMap = getTexture(sHandle, "roughnessMap");
	specularMap = getTexture(sHandle, "specularMap");

	opacity = getDouble(sHandle, "opacity");
	metallic = getDouble(sHandle, "metallic");
	roughness = getDouble(sHandle, "roughness");

	ambientColor = getColor(sHandle, "ambientColor");
	bumpmapTrafo = getTrafo(sHandle, "bumpmapTrafo");
	colormapTrafo = getTrafo(sHandle, "colormapTrafo");
	diffuseColor = getColor(sHandle, "diffuseColor");
	dirtmapTrafo = getTrafo(sHandle, "dirtmapTrafo");
	emissiveColor = getColor(sHandle, "emissiveColor");
	emissivemapTrafo = getTrafo(sHandle, "emissivemapTrafo");
	metallicmapTrafo = getTrafo(sHandle, "metallicmapTrafo");
	normalmapTrafo = getTrafo(sHandle, "normalmapTrafo");
	occlusionmapTrafo = getTrafo(sHandle, "occlusionmapTrafo");
	opacitymapTrafo = getTrafo(sHandle, "opacitymapTrafo");
	roughnessmapTrafo = getTrafo(sHandle, "roughnessmapTrafo");
	specularColor = getColor(sHandle, "specularColor");
	specularmapTrafo = getTrafo(sHandle, "specularmapTrafo");
}

bool MaterialInfo::equals(const MaterialInfo& o) const {
	return bumpMap == o.bumpMap &&
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
}
