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

#include "prt/Callbacks.h"

constexpr const wchar_t* ENCODER_ID_Maya = L"MayaEncoder";
constexpr const wchar_t* EO_EMIT_ATTRIBUTES = L"emitAttributes";
constexpr const wchar_t* EO_EMIT_MATERIALS = L"emitMaterials";
constexpr const wchar_t* EO_EMIT_REPORTS = L"emitReports";

class IMayaCallbacks : public prt::Callbacks {
public:
	~IMayaCallbacks() override = default;

	/**
	 * @param name initial shape (primitive group) name, optionally used to create primitive groups on output
	 * @param vtx vertex coordinate array
	 * @param length of vertex coordinate array
	 * @param nrm vertex normal array
	 * @param nrmSize length of vertex normal array
	 * @param faceCounts vertex counts per face
	 * @param faceCountsSize number of faces (= size of faceCounts)
	 * @param indices vertex attribute index array (grouped by counts)
	 * @param indicesSize vertex attribute index array
	 * @param uvs array of texture coordinate arrays (same indexing as vertices per uv set)
	 * @param uvsSizes lengths of uv arrays per uv set
	 * @param uvSetsCount number of uv sets
	 * @param faceRanges ranges for materials and reports
	 * @param materials contains faceRangesSize-1 attribute maps (all materials must have an identical set of keys and
	 * types)
	 * @param reports contains faceRangesSize-1 attribute maps
	 * @param shapeIDs shape ids per face, contains faceRangesSize-1 values
	 */
	// clang-format off
	virtual void addMesh(const wchar_t* name,
	                     const double* vtx, size_t vtxSize,
	                     const double* nrm, size_t nrmSize,
	                     const uint32_t* faceCounts, size_t faceCountsSize,
	                     const uint32_t* vertexIndices, size_t vertexIndicesSize,
	                     const uint32_t* normalIndices, size_t normalIndicesSize,

	                     double const* const* uvs, size_t const* uvsSizes,
	                     uint32_t const* const* uvCounts, size_t const* uvCountsSizes,
	                     uint32_t const* const* uvIndices, size_t const* uvIndicesSizes,
	                     size_t uvSets,

	                     const uint32_t* faceRanges, size_t faceRangesSize,
	                     const prt::AttributeMap** materials,
	                     const prt::AttributeMap** reports,
	                     const int32_t* shapeIDs
	) = 0;
	// clang-format on

	/**
	 * Writes an asset (e.g. in-memory texture) to an implementation-defined path. Assets with same uri will be assumed
	 * to contain identical data.
	 *
	 * @param uri the original asset within the RPK
	 * @param fileName local fileName derived from the URI by the asset encoder. can be used to cache the asset.
	 * @param [out] result file system path of the locally cached asset. Expected to be valid for the whole process
	 * life-time.
	 */
	virtual void addAsset(const wchar_t* uri, const wchar_t* fileName, const uint8_t* buffer, size_t size,
	                      wchar_t* result, size_t& resultSize) = 0;
};
