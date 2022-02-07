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

#include "utils/MELScriptBuilder.h"

#include "maya/MString.h"
#include "maya/adskDataHandle.h"

#include <array>
#include <filesystem>

const std::string PRT_MATERIAL_STRUCTURE = "prtMaterialStructure";
const std::string PRT_MATERIAL_CHANNEL = "prtMaterialChannel";
const std::string PRT_MATERIAL_STREAM = "prtMaterialStream";
const std::string PRT_MATERIAL_FACE_INDEX_START = "faceIndexStart";
const std::string PRT_MATERIAL_FACE_INDEX_END = "faceIndexEnd";
const MELVariable MEL_VARIABLE_SHADING_ENGINE(L"shadingGroup");

class MaterialColor {
public:
	MaterialColor(adsk::Data::Handle& sHandle, const std::string& name);

	double r() const noexcept;
	double g() const noexcept;
	double b() const noexcept;

	bool operator==(const MaterialColor& other) const noexcept;
	bool operator<(const MaterialColor& rhs) const noexcept;
	bool operator>(const MaterialColor& rhs) const noexcept;

private:
	std::array<double, 3> data;
};

class MaterialTrafo {
public:
	MaterialTrafo(adsk::Data::Handle& sHandle, const std::string& name);

	double su() const noexcept;
	double sv() const noexcept;
	double tu() const noexcept;
	double tv() const noexcept;
	double rw() const noexcept;

	// shaderfx does not support 5 values per input, that's why we split it up in tuv and suvw
	std::array<double, 2> tuv() const noexcept;
	std::array<double, 3> suvw() const noexcept;

	bool operator==(const MaterialTrafo& other) const noexcept;
	bool operator!=(const MaterialTrafo& other) const noexcept;
	bool operator<(const MaterialTrafo& rhs) const noexcept;
	bool operator>(const MaterialTrafo& rhs) const noexcept;

private:
	std::array<double, 5> data;
};

class MaterialInfo {
public:
	explicit MaterialInfo(adsk::Data::Handle& handle);

	enum class TextureSemantic {
		BUMP,
		COLOR,
		DIRT,
		EMISSIVE,
		METALLIC,
		NORMAL,
		OCCLUSION,
		OPACITY,
		ROUGHNESS,
		SPECULAR,
		COUNT
	};

	const std::filesystem::path& getTexturePath(TextureSemantic textureSemantic) const {
		return texturePaths.at(static_cast<size_t>(textureSemantic));
	}

	const MaterialTrafo& getTextureTrafo(TextureSemantic textureSemantic) const {
		return textureTrafos.at(static_cast<size_t>(textureSemantic));
	}

	double opacity = 1.0;
	double metallic = 0.0;
	double roughness = 1.0;

	MaterialColor ambientColor;
	MaterialColor diffuseColor;
	MaterialColor emissiveColor;
	MaterialColor specularColor;

	bool equals(const MaterialInfo& o) const;

	bool operator<(const MaterialInfo& rhs) const;

private:
	std::array<std::filesystem::path, static_cast<size_t>(TextureSemantic::COUNT)> texturePaths;
	std::array<MaterialTrafo, static_cast<size_t>(TextureSemantic::COUNT)> textureTrafos;
};
