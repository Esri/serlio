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

MString MaterialInfo::toMString(const std::vector<double>& d, size_t size, size_t offset) {
	MString colString;
	for (size_t i = offset; i < d.size() && i < offset + size; i++) {
		colString += d[i];
		colString += " ";
	}
	return colString;
}

std::string MaterialInfo::getTexture(adsk::Data::Handle sHandle, const std::string& texName) {
	std::string r;
	if (sHandle.setPositionByMemberName(texName.c_str()))
		r = (char*)sHandle.asUInt8();
	return r;
}

std::vector<double> MaterialInfo::getDoubleVector(adsk::Data::Handle sHandle, const std::string& name, size_t numElements) {
	std::vector<double> r;
	if (sHandle.setPositionByMemberName(name.c_str())) {
		double* data = sHandle.asDouble();
		if (sHandle.dataLength() >= numElements && data != nullptr) {
			r.reserve(numElements);
			std::copy(data, data + numElements, std::back_inserter(r));
		}
	}
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

	ambientColor = getDoubleVector(sHandle, "ambientColor", 3);
	bumpmapTrafo = getDoubleVector(sHandle, "bumpmapTrafo", 5);
	colormapTrafo = getDoubleVector(sHandle, "colormapTrafo", 5);
	diffuseColor = getDoubleVector(sHandle, "diffuseColor", 3);
	dirtmapTrafo = getDoubleVector(sHandle, "dirtmapTrafo", 5);
	emissiveColor = getDoubleVector(sHandle, "emissiveColor", 3);
	emissivemapTrafo = getDoubleVector(sHandle, "emissivemapTrafo", 5);
	metallicmapTrafo = getDoubleVector(sHandle, "metallicmapTrafo", 5);
	normalmapTrafo = getDoubleVector(sHandle, "normalmapTrafo", 5);
	occlusionmapTrafo = getDoubleVector(sHandle, "occlusionmapTrafo", 5);
	opacitymapTrafo = getDoubleVector(sHandle, "opacitymapTrafo", 5);
	roughnessmapTrafo = getDoubleVector(sHandle, "roughnessmapTrafo", 5);
	specularColor = getDoubleVector(sHandle, "specularColor", 3);
	specularmapTrafo = getDoubleVector(sHandle, "specularmapTrafo", 5);
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
