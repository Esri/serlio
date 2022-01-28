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

#include "modifiers/MayaCallbacks.h"
#include "modifiers/PRTModifierNode.h"

#include "materials/MaterialInfo.h"

#include "PRTContext.h"
#include "utils/AssetCache.h"
#include "utils/LogHandler.h"
#include "utils/MayaUtilities.h"
#include "utils/Utilities.h"

#include "prt/StringUtils.h"

#include "maya/MFloatArray.h"
#include "maya/MFloatPointArray.h"
#include "maya/MFloatVectorArray.h"
#include "maya/MFnDependencyNode.h"
#include "maya/MFnMesh.h"
#include "maya/MFnMeshData.h"
#include "maya/adskDataAssociations.h"
#include "maya/adskDataStream.h"

#include <cassert>
#include <sstream>

namespace {

constexpr bool DBG = false;

void checkStringLength(const wchar_t* string, const size_t& maxStringLength) {
	if (wcslen(string) >= maxStringLength) {
		const std::wstring msg = L"Maximum texture path size is " + std::to_wstring(maxStringLength);
		prt::log(msg.c_str(), prt::LOG_ERROR);
	}
}

MIntArray toMayaIntArray(uint32_t const* a, size_t s) {
	MIntArray mia(static_cast<unsigned int>(s), 0);
	for (unsigned int i = 0; i < s; ++i)
		mia.set(a[i], i);
	return mia;
}

MFloatPointArray toMayaFloatPointArray(double const* a, size_t s) {
	assert(s % 3 == 0);
	const unsigned int numPoints = static_cast<unsigned int>(s) / 3;
	MFloatPointArray mfpa(numPoints);
	for (unsigned int i = 0; i < numPoints; ++i) {
		mfpa.set(MFloatPoint(static_cast<float>(a[i * 3 + 0] * mu::PRT_TO_SERLIO_SCALE),
		                     static_cast<float>(a[i * 3 + 1] * mu::PRT_TO_SERLIO_SCALE),
		                     static_cast<float>(a[i * 3 + 2] * mu::PRT_TO_SERLIO_SCALE)),
		         i);
	}
	return mfpa;
}

struct TextureUVOrder {
	MString mayaUvSetName;
	uint8_t mayaUvSetIndex;
	uint8_t prtUvSetIndex;
};

const std::vector<TextureUVOrder> TEXTURE_UV_ORDERS = []() -> std::vector<TextureUVOrder> {
	// clang-format off
	return {
	        // first 4 uv sets are selected to be compatible with the Maya PBR Stingray shader
	        { L"map1",         0,    0 },  // colormap
	        { L"dirtMap",      1,    2 },  // dirtmap
	        { L"normalMap",    2,    5 },  // normalmap
	        { L"opacityMap",   3,    4 },  // opacitymap

	        { L"bumpMap",      4,    1 },  // bumpmap
	        { L"specularMap",  5,    3 },  // specularmap
	        { L"emissiveMap",  6,    6 },  // emissivemap
	        { L"occlusionMap", 7,    7 },  // occlusionmap
	        { L"roughnessMap", 8,    8 },  // roughnessmap
	        { L"metallicMap",  9,    9 }   // metallicmap
	};
	// clang-format on
}();

void assignTextureCoordinates(MFnMesh& fnMesh, double const* const* uvs, size_t const* uvsSizes,
                              uint32_t const* const* uvCounts, size_t const* uvCountsSizes,
                              uint32_t const* const* uvIndices, size_t const* uvIndicesSizes, size_t uvSetsCount) {
	if (uvSetsCount == 0)
		return;

	fnMesh.clearUVs();

	for (const TextureUVOrder& o : TEXTURE_UV_ORDERS) {
		const uint8_t uvSet = o.prtUvSetIndex;
		const MString uvSetName = o.mayaUvSetName;

		if (uvSetsCount > uvSet && uvsSizes[uvSet] > 0) {
			MFloatArray mU;
			MFloatArray mV;
			for (size_t uvIdx = 0; uvIdx < uvsSizes[uvSet] / 2; ++uvIdx) {
				mU.append(static_cast<float>(uvs[uvSet][uvIdx * 2 + 0])); // maya mesh only supports float uvs
				mV.append(static_cast<float>(uvs[uvSet][uvIdx * 2 + 1]));
			}

			if (uvSet > 0) {
				MStatus status;
				fnMesh.createUVSetDataMeshWithName(uvSetName, &status);
				MCHECK(status);
			}

			MCHECK(fnMesh.setUVs(mU, mV, &uvSetName));

			MIntArray mUVCounts = toMayaIntArray(uvCounts[uvSet], uvCountsSizes[uvSet]);
			MIntArray mUVIndices = toMayaIntArray(uvIndices[uvSet], uvIndicesSizes[uvSet]);
			MCHECK(fnMesh.assignUVs(mUVCounts, mUVIndices, &uvSetName));
		}
		else {
			if (uvSet > 0) {
				// add empty set to keep order consistent
				MStatus status;
				fnMesh.createUVSetDataMeshWithName(uvSetName, &status);
				MCHECK(status);
			}
		}
	}
}

void assignVertexNormals(MFnMesh& mFnMesh, const MIntArray& mayaFaceCounts, MIntArray& mayaVertexIndices,
                         const double* nrm, size_t nrmSize, const uint32_t* normalIndices,
                         MAYBE_UNUSED size_t normalIndicesSize) {
	if (nrmSize == 0)
		return;

	assert(normalIndicesSize == mayaVertexIndices.length());
	// guaranteed by MayaEncoder, see prtx::VertexNormalProcessor::SET_MISSING_TO_FACE_NORMALS

	// convert to native maya normal layout
	MVectorArray expandedNormals(static_cast<unsigned int>(mayaVertexIndices.length()));
	MIntArray faceList(static_cast<unsigned int>(mayaVertexIndices.length()));

	int indexCount = 0;
	for (uint32_t i = 0; i < mayaFaceCounts.length(); i++) {
		int faceLength = mayaFaceCounts[i];

		for (int j = 0; j < faceLength; j++) {
			faceList[indexCount] = i;
			int idx = normalIndices[indexCount];
			expandedNormals.set(&nrm[idx * 3], indexCount);
			indexCount++;
		}
	}

	MCHECK(mFnMesh.setFaceVertexNormals(expandedNormals, faceList, mayaVertexIndices));
}

constexpr unsigned int MATERIAL_MAX_STRING_LENGTH = 400;
constexpr unsigned int MATERIAL_MAX_FLOAT_ARRAY_LENGTH = 5;
constexpr unsigned int MATERIAL_MAX_STRING_ARRAY_LENGTH = 2;

adsk::Data::Structure* createNewMayaStructure(const prt::AttributeMap** materials) {
	adsk::Data::Structure* fStructure;

	const prt::AttributeMap* mat = materials[0];

	// Register our structure since it is not registered yet.
	fStructure = adsk::Data::Structure::create();
	fStructure->setName(PRT_MATERIAL_STRUCTURE.c_str());

	fStructure->addMember(adsk::Data::Member::kInt32, 1, PRT_MATERIAL_FACE_INDEX_START.c_str());
	fStructure->addMember(adsk::Data::Member::kInt32, 1, PRT_MATERIAL_FACE_INDEX_END.c_str());

	size_t keyCount = 0;
	wchar_t const* const* keys = mat->getKeys(&keyCount);
	for (int k = 0; k < keyCount; k++) {
		wchar_t const* key = keys[k];

		adsk::Data::Member::eDataType type;
		unsigned int size = 0;
		unsigned int arrayLength = 1;

		// clang-format off
		switch (mat->getType(key)) {
			case prt::Attributable::PT_BOOL: type = adsk::Data::Member::kBoolean; size = 1;  break;
			case prt::Attributable::PT_FLOAT: type = adsk::Data::Member::kDouble; size = 1; break;
			case prt::Attributable::PT_INT: type = adsk::Data::Member::kInt32; size = 1; break;

			//workaround: using kString type crashes maya when setting metadata elememts. Therefore we use array of kUInt8
			case prt::Attributable::PT_STRING: type = adsk::Data::Member::kUInt8; size = MATERIAL_MAX_STRING_LENGTH;  break;
			case prt::Attributable::PT_BOOL_ARRAY: type = adsk::Data::Member::kBoolean; size = MATERIAL_MAX_STRING_LENGTH; break;
			case prt::Attributable::PT_INT_ARRAY: type = adsk::Data::Member::kInt32; size = MATERIAL_MAX_STRING_LENGTH; break;
			case prt::Attributable::PT_FLOAT_ARRAY: type = adsk::Data::Member::kDouble; size = MATERIAL_MAX_FLOAT_ARRAY_LENGTH; break;
			case prt::Attributable::PT_STRING_ARRAY: type = adsk::Data::Member::kUInt8; size = MATERIAL_MAX_STRING_LENGTH; arrayLength = MATERIAL_MAX_STRING_ARRAY_LENGTH; break;

			case prt::Attributable::PT_UNDEFINED: break;
			case prt::Attributable::PT_BLIND_DATA: break;
			case prt::Attributable::PT_BLIND_DATA_ARRAY: break;
			case prt::Attributable::PT_COUNT: break;
		}
		// clang-format on

		if (size > 0) {
			for (unsigned int i = 0; i < arrayLength; i++) {
				std::wstring keyToUse = key;
				if (i > 0)
					keyToUse = key + std::to_wstring(i);
				const std::string keyToUseNarrow = prtu::toOSNarrowFromUTF16(keyToUse);
				fStructure->addMember(type, size, keyToUseNarrow.c_str());
			}
		}
	}

	adsk::Data::Structure::registerStructure(*fStructure);

	return fStructure;
}

void fillMetadata(adsk::Data::Structure* fStructure, const uint32_t* faceRanges, size_t faceRangesSize,
                  const prt::AttributeMap** materials, const prt::AttributeMap** reports,
                  adsk::Data::Associations& newMetadata) {
	assert(fStructure != nullptr);
	assert(faceRangesSize > 1);

	adsk::Data::Stream newStream(*fStructure, PRT_MATERIAL_STREAM);
	adsk::Data::Channel newChannel = newMetadata.channel(PRT_MATERIAL_CHANNEL);
	newChannel.setDataStream(newStream);
	newMetadata.setChannel(newChannel);

	for (size_t fri = 0; fri < faceRangesSize - 1; fri++) {

		if (materials != nullptr) {
			adsk::Data::Handle handle(*fStructure);

			const prt::AttributeMap* mat = materials[fri];

			size_t keyCount = 0;
			wchar_t const* const* keys = mat->getKeys(&keyCount);

			for (int k = 0; k < keyCount; k++) {

				wchar_t const* key = keys[k];

				const std::string keyNarrow = prtu::toOSNarrowFromUTF16(key);

				if (!handle.setPositionByMemberName(keyNarrow.c_str()))
					continue;

				size_t arraySize = 0;

				switch (mat->getType(key)) {
					case prt::Attributable::PT_BOOL:
						handle.asBoolean()[0] = mat->getBool(key);
						break;
					case prt::Attributable::PT_FLOAT:
						handle.asDouble()[0] = mat->getFloat(key);
						break;
					case prt::Attributable::PT_INT:
						handle.asInt32()[0] = mat->getInt(key);
						break;

					// workaround: transporting string as uint8 array, because using asString crashes maya
					case prt::Attributable::PT_STRING: {
						const wchar_t* str = mat->getString(key);
						if (wcslen(str) == 0)
							break;
						checkStringLength(str, MATERIAL_MAX_STRING_LENGTH);
						size_t maxStringLengthTmp = MATERIAL_MAX_STRING_LENGTH;
						prt::StringUtils::toOSNarrowFromUTF16(str, (char*)handle.asUInt8(), &maxStringLengthTmp);
						break;
					}
					case prt::Attributable::PT_BOOL_ARRAY: {
						const bool* boolArray;
						boolArray = mat->getBoolArray(key, &arraySize);
						for (unsigned int i = 0; i < arraySize && i < MATERIAL_MAX_STRING_LENGTH; i++)
							handle.asBoolean()[i] = boolArray[i];
						break;
					}
					case prt::Attributable::PT_INT_ARRAY: {
						const int* intArray;
						intArray = mat->getIntArray(key, &arraySize);
						for (unsigned int i = 0; i < arraySize && i < MATERIAL_MAX_STRING_LENGTH; i++)
							handle.asInt32()[i] = intArray[i];
						break;
					}
					case prt::Attributable::PT_FLOAT_ARRAY: {
						const double* floatArray;
						floatArray = mat->getFloatArray(key, &arraySize);
						for (unsigned int i = 0;
						     i < arraySize && i < MATERIAL_MAX_STRING_LENGTH && i < MATERIAL_MAX_FLOAT_ARRAY_LENGTH;
						     i++)
							handle.asDouble()[i] = floatArray[i];
						break;
					}
					case prt::Attributable::PT_STRING_ARRAY: {

						const wchar_t* const* stringArray = mat->getStringArray(key, &arraySize);

						for (unsigned int i = 0; i < arraySize && i < MATERIAL_MAX_STRING_LENGTH; i++) {
							if (wcslen(stringArray[i]) == 0)
								continue;

							if (i > 0) {
								std::wstring keyToUse = key + std::to_wstring(i);
								const std::string keyToUseNarrow = prtu::toOSNarrowFromUTF16(keyToUse);
								if (!handle.setPositionByMemberName(keyToUseNarrow.c_str()))
									continue;
							}

							checkStringLength(stringArray[i], MATERIAL_MAX_STRING_LENGTH);
							size_t maxStringLengthTmp = MATERIAL_MAX_STRING_LENGTH;
							prt::StringUtils::toOSNarrowFromUTF16(stringArray[i], (char*)handle.asUInt8(),
							                                      &maxStringLengthTmp);
						}
						break;
					}

					case prt::Attributable::PT_UNDEFINED:
						break;
					case prt::Attributable::PT_BLIND_DATA:
						break;
					case prt::Attributable::PT_BLIND_DATA_ARRAY:
						break;
					case prt::Attributable::PT_COUNT:
						break;
				}
			}

			handle.setPositionByMemberName(PRT_MATERIAL_FACE_INDEX_START.c_str());
			*handle.asInt32() = faceRanges[fri];

			handle.setPositionByMemberName(PRT_MATERIAL_FACE_INDEX_END.c_str());
			*handle.asInt32() = faceRanges[fri + 1];

			newStream.setElement(static_cast<adsk::Data::IndexCount>(fri), handle);
		}

		if (reports != nullptr) {
			// todo
		}
	}
}

void updateMayaMesh(double const* const* uvs, size_t const* uvsSizes, uint32_t const* const* uvCounts,
                    size_t const* uvCountsSizes, uint32_t const* const* uvIndices, size_t const* uvIndicesSizes,
                    size_t uvSetsCount, const double* nrm, size_t nrmSize, const uint32_t* normalIndices,
                    size_t normalIndicesSize, const MFloatPointArray& mayaVertices, const MIntArray& mayaFaceCounts,
                    MIntArray& mayaVertexIndices, const MObject& outMeshObj,
                    const adsk::Data::Associations& newMetadata) {
	MStatus stat;

	MFnMeshData dataCreator;
	MObject newOutputData = dataCreator.create(&stat);
	MCHECK(stat);

	MFnMesh mFnMesh1;
	MObject newMeshObj = mFnMesh1.create(mayaVertices.length(), mayaFaceCounts.length(), mayaVertices, mayaFaceCounts,
	                                     mayaVertexIndices, newOutputData, &stat);
	MCHECK(stat);

	MFnMesh newMesh(newMeshObj);
	assignTextureCoordinates(newMesh, uvs, uvsSizes, uvCounts, uvCountsSizes, uvIndices, uvIndicesSizes, uvSetsCount);
	assignVertexNormals(newMesh, mayaFaceCounts, mayaVertexIndices, nrm, nrmSize, normalIndices, normalIndicesSize);

	MFnMesh outputMesh(outMeshObj);
	outputMesh.copyInPlace(newMeshObj);

	outputMesh.setMetadata(newMetadata);
}

void copyStringToWCharPtr(const std::wstring input, wchar_t* result, size_t& resultSize) {
	if (resultSize <= input.size())    // also check for null-terminator
		resultSize = input.size() + 1; // ask for space for null-terminator
#if _MSC_VER >= 1400
	wcsncpy_s(result, resultSize, input.c_str(), resultSize);
#else
	wcsncpy(result, pathStr.c_str(), resultSize);
#endif
	result[resultSize - 1] = 0x0;
	resultSize = input.length() + 1;
}
} // namespace

void MayaCallbacks::addMesh(const wchar_t*, const double* vtx, size_t vtxSize, const double* nrm, size_t nrmSize,
                            const uint32_t* faceCounts, size_t faceCountsSize, const uint32_t* vertexIndices,
                            size_t vertexIndicesSize, const uint32_t* normalIndices, size_t normalIndicesSize,
                            double const* const* uvs, size_t const* uvsSizes, uint32_t const* const* uvCounts,
                            size_t const* uvCountsSizes, uint32_t const* const* uvIndices, size_t const* uvIndicesSizes,
                            size_t uvSetsCount, const uint32_t* faceRanges, size_t faceRangesSize,
                            const prt::AttributeMap** materials, const prt::AttributeMap** reports, const int32_t*) {
	MStatus stat;
	adsk::Data::Structure* fStructure = adsk::Data::Structure::structureByName(PRT_MATERIAL_STRUCTURE.c_str());

	if ((fStructure == nullptr) && (materials != nullptr) && (faceRangesSize > 1)) {
		fStructure = createNewMayaStructure(materials); // Structure to use for creation
	}

	MFnMesh inputMesh(inMeshObj);

	adsk::Data::Associations newMetadata(inputMesh.metadata(&stat));
	newMetadata.makeUnique();
	MCHECK(stat);

	if (fStructure != nullptr && faceRangesSize > 1) {
		fillMetadata(fStructure, faceRanges, faceRangesSize, materials, reports, newMetadata);
	}

	MFloatPointArray mayaVertices = toMayaFloatPointArray(vtx, vtxSize);
	MIntArray mayaFaceCounts = toMayaIntArray(faceCounts, faceCountsSize);
	MIntArray mayaVertexIndices = toMayaIntArray(vertexIndices, vertexIndicesSize);

	if (DBG) {
		LOG_DBG << "-- MayaCallbacks::addMesh";
		LOG_DBG << "   faceCountsSize = " << faceCountsSize;
		LOG_DBG << "   vertexIndicesSize = " << vertexIndicesSize;
		LOG_DBG << "   mayaVertices.length = " << mayaVertices.length();
		LOG_DBG << "   mayaFaceCounts.length   = " << mayaFaceCounts.length();
		LOG_DBG << "   mayaVertexIndices.length = " << mayaVertexIndices.length();
	}

	updateMayaMesh(uvs, uvsSizes, uvCounts, uvCountsSizes, uvIndices, uvIndicesSizes, uvSetsCount, nrm, nrmSize,
	               normalIndices, normalIndicesSize, mayaVertices, mayaFaceCounts, mayaVertexIndices, outMeshObj,
	               newMetadata);
}

prt::Status MayaCallbacks::attrBool(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key, bool value) {
	mAttributeMapBuilder->setBool(key, value);
	return prt::STATUS_OK;
}

prt::Status MayaCallbacks::attrFloat(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key, double value) {
	mAttributeMapBuilder->setFloat(key, value);
	return prt::STATUS_OK;
}

prt::Status MayaCallbacks::attrString(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key,
                                      const wchar_t* value) {
	mAttributeMapBuilder->setString(key, value);
	return prt::STATUS_OK;
}

void MayaCallbacks::addAsset(const wchar_t* uri, const wchar_t* fileName, const uint8_t* buffer, size_t size,
                             wchar_t* result, size_t& resultSize) {
	if (uri == nullptr || std::wcslen(uri) == 0 || fileName == nullptr || std::wcslen(fileName) == 0) {
		LOG_WRN << "Skipping asset caching for invalid uri '" << uri << "' or filename '" << fileName << '"';
		resultSize = 0;
		return;
	}

	const std::filesystem::path& assetPath = PRTContext::get().mAssetCache.put(uri, fileName, buffer, size);
	if (assetPath.empty()) {
		resultSize = 0;
		return;
	}

	const std::wstring pathStr = assetPath.generic_wstring();

	copyStringToWCharPtr(pathStr, result, resultSize);
}

// PRT version >= 2.3
#if PRT_VERSION_GTE(2, 3)

prt::Status MayaCallbacks::attrBoolArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key,
                                         const bool* values, size_t size, size_t /*nRows*/) {
	mAttributeMapBuilder->setBoolArray(key, values, size);
	return prt::STATUS_OK;
}

prt::Status MayaCallbacks::attrFloatArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key,
                                          const double* values, size_t size, size_t /*nRows*/) {
	mAttributeMapBuilder->setFloatArray(key, values, size);
	return prt::STATUS_OK;
}

prt::Status MayaCallbacks::attrStringArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key,
                                           const wchar_t* const* values, size_t size, size_t /*nRows*/) {
	mAttributeMapBuilder->setStringArray(key, values, size);
	return prt::STATUS_OK;
}

// PRT version >= 2.1
#elif PRT_VERSION_GTE(2, 1)

prt::Status MayaCallbacks::attrBoolArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key,
                                         const bool* values, size_t size) {
	mAttributeMapBuilder->setBoolArray(key, values, size);
	return prt::STATUS_OK;
}

prt::Status MayaCallbacks::attrFloatArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key,
                                          const double* values, size_t size) {
	mAttributeMapBuilder->setFloatArray(key, values, size);
	return prt::STATUS_OK;
}

prt::Status MayaCallbacks::attrStringArray(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key,
                                           const wchar_t* const* values, size_t size) {
	mAttributeMapBuilder->setStringArray(key, values, size);
	return prt::STATUS_OK;
}

#endif // PRT version >= 2.1
