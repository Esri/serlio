/**
 * Serlio - Esri CityEngine Plugin for Autodesk Maya
 *
 * See https://github.com/esri/serlio for build and usage instructions.
 *
 * Copyright (c) 2012-2022 Esri R&D Center Zurich
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

#include "modifiers/PRTMesh.h"

#include "utils/MArrayWrapper.h"
#include "utils/MayaUtilities.h"

#include "maya/MFloatPointArray.h"
#include "maya/MFnMesh.h"
#include "maya/MIntArray.h"

#include <cassert>

PRTMesh::PRTMesh(const MObject& mesh) {
	assert(mesh.hasFn(MFn::kMesh));

	MStatus status;
	const MFnMesh meshFn(mesh, &status);
	MCHECK(status);

	// vertex coordinates
	MFloatPointArray vertexArray;
	meshFn.getPoints(vertexArray);

	const unsigned int vertexArrayLength = vertexArray.length();
	mVertexCoordsVec.reserve(3 * vertexArrayLength);
	for (unsigned int i = 0; i < vertexArrayLength; ++i) {
		mVertexCoordsVec.push_back(vertexArray[i].x / mu::PRT_TO_SERLIO_SCALE);
		mVertexCoordsVec.push_back(vertexArray[i].y / mu::PRT_TO_SERLIO_SCALE);
		mVertexCoordsVec.push_back(vertexArray[i].z / mu::PRT_TO_SERLIO_SCALE);
	}

	// faces
	MIntArray vertexCount;
	MIntArray vertexList;
	meshFn.getVertices(vertexCount, vertexList);

	mFaceCountsVec.reserve(vertexCount.length());
	const auto vertexCountWrapper = mu::makeMArrayConstWrapper(vertexCount);
	std::copy(vertexCountWrapper.begin(), vertexCountWrapper.end(), std::back_inserter(mFaceCountsVec));

	mIndicesVec.reserve(vertexList.length());
	const auto vertexListWrapper = mu::makeMArrayConstWrapper(vertexList);
	std::copy(vertexListWrapper.begin(), vertexListWrapper.end(), std::back_inserter(mIndicesVec));
}
