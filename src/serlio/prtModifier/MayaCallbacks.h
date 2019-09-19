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

#include "util/Utilities.h"
#include "util/LogHandler.h"

#include "maya/MObject.h"

#include <memory>
#include <stdexcept>
#include <map>
#include <vector>
#include <string>
#include <iostream>


class MayaCallbacks : public IMayaCallbacks {
public:
	MayaCallbacks(MObject inMesh, MObject outMesh, AttributeMapBuilderUPtr& amb)
		: inMeshObj(inMesh), outMeshObj(outMesh), mAttributeMapBuilder(amb) { }

	// prt::Callbacks interface
	virtual prt::Status generateError(size_t /*isIndex*/, prt::Status /*status*/, const wchar_t* message) override {
		LOG_ERR << "GENERATE ERROR: " << message;
		return prt::STATUS_OK;
	}
	virtual prt::Status assetError(size_t /*isIndex*/, prt::CGAErrorLevel /*level*/, const wchar_t* /*key*/, const wchar_t* /*uri*/, const wchar_t* message) override {
		LOG_ERR << "ASSET ERROR: " << message;
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaError(size_t /*isIndex*/, int32_t /*shapeID*/, prt::CGAErrorLevel /*level*/, int32_t /*methodId*/, int32_t /*pc*/, const wchar_t* message) override {
		LOG_ERR << "CGA ERROR: " << message;
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaPrint(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* txt) override {
		LOG_INF << "CGA PRINT: " << txt;
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaReportBool(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, bool /*value*/) override {
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaReportFloat(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, double /*value*/) override {
		return prt::STATUS_OK;
	}
	virtual prt::Status cgaReportString(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, const wchar_t* /*value*/) override {
		return prt::STATUS_OK;
	}
	virtual prt::Status attrBool(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, bool /*value*/) override;
	virtual prt::Status attrFloat(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, double /*value*/) override;
	virtual prt::Status attrString(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* /*key*/, const wchar_t* /*value*/) override;

public:

	virtual void addMesh(
		const wchar_t* name,
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
	) override;

private:
	MObject                 outMeshObj;
	MObject                 inMeshObj;

	AttributeMapBuilderUPtr& mAttributeMapBuilder;
};
