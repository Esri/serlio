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

#pragma once

#include "maya/MString.h"
#include "maya/adskDataHandle.h"

#include <array>

const std::string gPRTMatStructure = "prtMaterialStructure";
const std::string gPRTMatChannel = "prtMaterialChannel";
const std::string gPRTMatStream = "prtMaterialStream";
const std::string gPRTMatMemberFaceStart = "faceIndexStart";
const std::string gPRTMatMemberFaceEnd = "faceIndexEnd";

class MaterialColor {

public:
	double r() const noexcept;
	double g() const noexcept;
	double b() const noexcept;

private:
	friend class MaterialInfo;

	bool operator==(const MaterialColor& other) const noexcept;

	bool operator<(const MaterialColor& rhs) const noexcept;

	bool operator>(const MaterialColor& rhs) const noexcept;

	std::array<double, 3> data;
};

class MaterialTrafo {

public:
	double su() const noexcept;
	double sv() const noexcept;
	double tu() const noexcept;
	double tv() const noexcept;
	double rw() const noexcept;

	std::array<double, 2> tuv() const noexcept;
	std::array<double, 3> suvw() const noexcept;

private:
	friend class MaterialInfo;

	bool operator==(const MaterialTrafo& other) const noexcept;

	bool operator<(const MaterialTrafo& rhs) const noexcept;

	bool operator>(const MaterialTrafo& rhs) const noexcept;

	std::array<double, 5> data;
};

class MaterialInfo {
public:
	explicit MaterialInfo(adsk::Data::Handle sHandle);

	std::string bumpMap;
	std::string colormap;
	std::string dirtmap;
	std::string emissiveMap;
	std::string metallicMap;
	std::string normalMap;
	std::string occlusionMap;
	std::string opacityMap;
	std::string roughnessMap;
	std::string specularMap;

	double opacity;
	double metallic;
	double roughness;

	MaterialColor ambientColor;
	MaterialColor diffuseColor;
	MaterialColor emissiveColor;
	MaterialColor specularColor;

	MaterialTrafo specularmapTrafo;
	MaterialTrafo bumpmapTrafo;
	MaterialTrafo colormapTrafo;
	MaterialTrafo dirtmapTrafo;
	MaterialTrafo emissivemapTrafo;
	MaterialTrafo metallicmapTrafo;
	MaterialTrafo normalmapTrafo;
	MaterialTrafo occlusionmapTrafo;
	MaterialTrafo opacitymapTrafo;
	MaterialTrafo roughnessmapTrafo;

	bool equals(const MaterialInfo& o) const;

	bool operator<(const MaterialInfo& rhs) const;

private:
	static std::string getTexture(adsk::Data::Handle& sHandle, const std::string& texName);
	static double getDouble(adsk::Data::Handle& sHandle, const std::string& name);
	static MaterialColor getColor(adsk::Data::Handle& sHandle, const std::string& name);
	static MaterialTrafo getTrafo(adsk::Data::Handle& sHandle, const std::string& name);

	template <size_t N>
	static void getDoubleArray(std::array<double, N>& array, adsk::Data::Handle& sHandle, const std::string& name) {
		if (sHandle.setPositionByMemberName(name.c_str())) {
			double* data = sHandle.asDouble();
			if (sHandle.dataLength() >= N && data) {
				std::copy(data, data + N, array.begin());
				return;
			}
		}
		array.fill(0.0);
	}
};
