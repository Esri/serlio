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

#include "encoder/IMayaCallbacks.h"

#include "utils/LogHandler.h"
#include "utils/Utilities.h"

#include "maya/MObject.h"

#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class MayaCallbacks : public IMayaCallbacks {
public:
	MayaCallbacks(const MObject& inMesh, const MObject& outMesh, AttributeMapBuilderUPtr& amb)
	    : inMeshObj(inMesh), outMeshObj(outMesh), mAttributeMapBuilder(amb) {}

	// prt::Callbacks interface
	prt::Status generateError(size_t /*isIndex*/, prt::Status /*status*/, const wchar_t* message) override {
		LOG_ERR << "GENERATE ERROR: " << message;
		return prt::STATUS_OK;
	}
	prt::Status assetError(size_t /*isIndex*/, prt::CGAErrorLevel /*level*/, const wchar_t* /*key*/,
	                       const wchar_t* /*uri*/, const wchar_t* message) override {
		LOG_ERR << "ASSET ERROR: " << message;
		return prt::STATUS_OK;
	}
	prt::Status cgaError(size_t /*isIndex*/, int32_t /*shapeID*/, prt::CGAErrorLevel /*level*/, int32_t /*methodId*/,
	                     int32_t /*pc*/, const wchar_t* message) override {
		LOG_ERR << "CGA ERROR: " << message;
		return prt::STATUS_OK;
	}
	prt::Status cgaPrint(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* txt) override {
		LOG_INF << "CGA PRINT: " << txt;
		return prt::STATUS_OK;
	}
	prt::Status cgaReportBool(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/,
	                          bool /*value*/) override {
		return prt::STATUS_OK;
	}
	prt::Status cgaReportFloat(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/,
	                           double /*value*/) override {
		return prt::STATUS_OK;
	}
	prt::Status cgaReportString(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/,
	                            const wchar_t* /*value*/) override {
		return prt::STATUS_OK;
	}
	prt::Status attrBool(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, bool /*value*/) override;
	prt::Status attrFloat(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, double /*value*/) override;
	prt::Status attrString(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/,
	                       const wchar_t* /*value*/) override;

// PRT version >= 2.3
#if PRT_VERSION_GTE(2, 3)

	prt::Status attrBoolArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, const bool* /*values*/,
	                          size_t /*size*/, size_t /*nRows*/) override;
	prt::Status attrFloatArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/,
	                           const double* /*values*/, size_t /*size*/, size_t /*nRows*/) override;
	prt::Status attrStringArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/,
	                            const wchar_t* const* /*values*/, size_t /*size*/, size_t /*nRows*/) override;

#elif PRT_VERSION_GTE(2, 1)

	prt::Status attrBoolArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, const bool* /*values*/,
	                          size_t /*size*/) override;
	prt::Status attrFloatArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/,
	                           const double* /*values*/, size_t /*size*/) override;
	prt::Status attrStringArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/,
	                            const wchar_t* const* /*values*/, size_t /*size*/) override;

#endif // PRT version >= 2.1

public:
	// clang-format off
	void addMesh(const wchar_t* name,
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
	                     const int32_t* shapeIDs) override;
	// clang-format on

private:
	MObject outMeshObj;
	MObject inMeshObj;

	AttributeMapBuilderUPtr& mAttributeMapBuilder;
};
