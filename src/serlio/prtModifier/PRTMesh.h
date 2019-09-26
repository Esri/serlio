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

#include "maya/MTypes.h"

#include <vector>

class PRTMesh {

private:
	std::vector<double> vertexCoordsVec;
	std::vector<uint32_t> indicesVec;
	std::vector<uint32_t> faceCountsVec;

public:
	PRTMesh(const MObject& mesh);

	const double* vertexCoords() const noexcept {
		return vertexCoordsVec.data();
	}

	size_t vcCount() const noexcept {
		return vertexCoordsVec.size();
	}

	const uint32_t* indices() const noexcept {
		return indicesVec.data();
	}

	size_t indicesCount() const noexcept {
		return indicesVec.size();
	}

	const uint32_t* faceCounts() const noexcept {
		return faceCountsVec.data();
	}

	size_t faceCountsCount() const noexcept {
		return faceCountsVec.size();
	}
};
