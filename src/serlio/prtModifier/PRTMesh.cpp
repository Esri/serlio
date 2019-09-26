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

#include "prtModifier/PRTMesh.h"

#include "util/MArrayIteratorTraits.h"
#include "util/MayaUtilities.h"

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

	const size_t vertexArrayLength = vertexArray.length();
	vertexCoordsVec.reserve(vertexArrayLength);
	for (unsigned int i = 0; i < vertexArrayLength; ++i) {
		vertexCoordsVec.push_back(vertexArray[i].x);
		vertexCoordsVec.push_back(vertexArray[i].y);
		vertexCoordsVec.push_back(vertexArray[i].z);
	}

	// faces
	MIntArray vertexCount;
	MIntArray vertexList;
	meshFn.getVertices(vertexCount, vertexList);

	faceCountsVec.reserve(vertexCount.length());
	std::copy(vertexCount.begin(), vertexCount.end(), std::back_inserter(faceCountsVec));

	indicesVec.reserve(vertexList.length());
	std::copy(vertexList.begin(), vertexList.end(), std::back_inserter(indicesVec));
}
