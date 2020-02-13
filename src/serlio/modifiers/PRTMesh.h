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
	std::vector<double> mVertexCoordsVec;
	std::vector<uint32_t> mIndicesVec;
	std::vector<uint32_t> mFaceCountsVec;

public:
	explicit PRTMesh(const MObject& mesh);

	const double* vertexCoords() const noexcept {
		return mVertexCoordsVec.data();
	}

	size_t vcCount() const noexcept {
		return mVertexCoordsVec.size();
	}

	const uint32_t* indices() const noexcept {
		return mIndicesVec.data();
	}

	size_t indicesCount() const noexcept {
		return mIndicesVec.size();
	}

	const uint32_t* faceCounts() const noexcept {
		return mFaceCountsVec.data();
	}

	size_t faceCountsCount() const noexcept {
		return mFaceCountsVec.size();
	}
};
