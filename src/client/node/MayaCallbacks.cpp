/**
 * Esri CityEngine SDK Maya Plugin Example
 *
 * This example demonstrates the main functionality of the Procedural Runtime API.
 *
 * See README.md in https://github.com/Esri/esri-cityengine-sdk for build instructions.
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

#include "node/MayaCallbacks.h"
#include "node/Utilities.h"
#include "prtModifier/PRTModifierNode.h"
#include "node/PRTMaterialNode.h"

#include "prt/StringUtils.h"
#include "prtx/Material.h"

#include <maya/adskDataStream.h>
#include <maya/adskDataChannel.h>
#include <maya/adskDataAssociations.h>
#include <maya/adskDataAccessorMaya.h>

#include <sstream>

namespace {

	const bool TRACE = false;
	void prtTrace(const std::wstring& arg1, std::size_t arg2) {
		if (TRACE) {
			std::wostringstream wostr;
			wostr << L"[MOH] " << arg1 << arg2;
			prt::log(wostr.str().c_str(), prt::LOG_TRACE);
		}
	}

	void checkStringLength(const wchar_t* string, const size_t &maxStringLength)
	{
		if (wcslen(string) >= maxStringLength) {
			const std::wstring msg = L"Maximum texture path size is " + std::to_wstring(maxStringLength);
			prt::log(msg.c_str(), prt::LOG_ERROR);
		}
	}

}

struct TextureUVOrder {
	MString      mayaUvSetName;
	uint8_t       mayaUvSetIndex;
	uint8_t      prtUvSetIndex;
};

//maya pbr stingray shader only supports first 4 uvsets -> reoder so first 4 are most important ones
//other shaders support >4 sets
const std::vector<TextureUVOrder> TEXTURE_UV_ORDERS = []() -> std::vector<TextureUVOrder> {
	return {
		// maya uvset name | maya idx | prt idx  | CGA key
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
}();

void MayaCallbacks::addMesh(
		const wchar_t* name,
		const double* vtx, size_t vtxSize,
		const double* nrm, size_t nrmSize,
		const uint32_t* faceSizes, size_t numFaces,
		const uint32_t* indices, size_t indicesSize,
		double const* const* uvs, size_t const* uvsSizes, uint32_t uvSetsCount,
		const uint32_t* faceRanges, size_t faceRangesSize,
		const prt::AttributeMap** materials,
		const prt::AttributeMap** reports,
		const int32_t* shapeIDs)
{


	MFloatPointArray mVertices;
	for (size_t i = 0; i < vtxSize; i += 3)
		mVertices.append(static_cast<float>(vtx[i]), static_cast<float>(vtx[i + 1]), static_cast<float>(vtx[i + 2]));

	MFloatVectorArray mNormals;
	for (size_t i = 0; i < nrmSize; i += 3)
		mNormals.append(MVector(nrm[i], nrm[i + 1], nrm[i + 2]));

	MIntArray mVerticesCounts;
	for (size_t i = 0; i < numFaces; ++i)
		mVerticesCounts.append(faceSizes[i]);
	prtTrace(L"countsSize = ", numFaces);

	MIntArray mVerticesIndices;
	for (size_t i = 0; i < indicesSize; ++i)
		mVerticesIndices.append(indices[i]);
	prtTrace(L"indicesSize = ", indicesSize);

	MStatus stat;

	prtu::dbg("--- MayaData::createMesh begin");

	MCHECK(stat);

	MFnMeshData dataCreator;
	MObject newOutputData = dataCreator.create(&stat);
	MCHECK(stat);

	prtu::dbg("    mVertices.length         = %d", mVertices.length());
	prtu::dbg("    mVerticesCounts.length   = %d", mVerticesCounts.length());
	prtu::dbg("    mVerticesConnects.length = %d", mVerticesIndices.length());
	prtu::dbg("    mNormals.length          = %d", mNormals.length());

	MFnMesh mFnMesh1;
	MObject oMesh = mFnMesh1.create(mVertices.length(), mVerticesCounts.length(), mVertices, mVerticesCounts, mVerticesIndices, newOutputData, &stat);
	MCHECK(stat);
	MFnMesh mFnMesh(oMesh);



	mFnMesh.clearUVs();

	// -- add texture coordinates
	for (const TextureUVOrder& o : TEXTURE_UV_ORDERS) {
		uint8_t uvSet = o.prtUvSetIndex;

		if (uvSetsCount > uvSet && uvsSizes[uvSet] > 0) {

			MFloatArray mU;
			MFloatArray mV;
			for (size_t uvIdx = 0; uvIdx < uvsSizes[uvSet]; ++uvIdx) {
				mU.append(static_cast<float>(uvs[uvSet][uvIdx * 2 + 0])); //maya mesh only supports float uvs
				mV.append(static_cast<float>(uvs[uvSet][uvIdx * 2 + 1]));
			}

			MString uvSetName = o.mayaUvSetName;

			if (uvSet != 0) {
				mFnMesh.createUVSetDataMeshWithName(uvSetName, &stat);
				MCHECK(stat);
			}

			MIntArray mUVCounts;
			for (size_t i = 0; i < numFaces; ++i)
				mUVCounts.append(faceSizes[i]);

			MIntArray  mUVIndices;
			for (size_t i = 0; i < indicesSize; ++i)
				mUVIndices.append(indices[i]);

			MCHECK(mFnMesh.assignUVs(mUVCounts, mUVIndices, &uvSetName));

		}
		else {
			if (uvSet > 0) {
				//add empty set to keep order consistent
				mFnMesh.createUVSetDataMeshWithName(o.mayaUvSetName, &stat);
				MCHECK(stat);
			}
		}
	}

	if (mNormals.length() > 0) {
		prtu::dbg("    mNormals.length        = %d", mNormals.length());

		// NOTE: this assumes that vertices and vertex normals use the same index domain (-> maya encoder)
		MVectorArray expandedNormals(mVerticesIndices.length());
		for (unsigned int i = 0; i < mVerticesIndices.length(); i++)
			expandedNormals[i] = mNormals[mVerticesIndices[i]];

		prtu::dbg("    expandedNormals.length = %d", expandedNormals.length());
		MCHECK(mFnMesh.setVertexNormals(expandedNormals, mVerticesIndices));
	}

	MFnMesh outputMesh(outMeshObj);
	outputMesh.copyInPlace(oMesh);

	// create material metadata
	unsigned int maxStringLength = 400;
	unsigned int maxFloatArrayLength = 5;
	unsigned int maxStringArrayLength = 2;

	adsk::Data::Structure* fStructure;	  // Structure to use for creation
	fStructure = adsk::Data::Structure::structureByName(gPRTMatStructure.c_str());
	if (fStructure == NULL)
	{
		// Register our structure since it is not registered yet.
		fStructure = adsk::Data::Structure::create();
		fStructure->setName(gPRTMatStructure.c_str());

		fStructure->addMember(adsk::Data::Member::kInt32, 1, gPRTMatMemberFaceStart.c_str());
		fStructure->addMember(adsk::Data::Member::kInt32, 1, gPRTMatMemberFaceEnd.c_str());

		prtx::MaterialBuilder mb;
		prtx::MaterialPtr m = mb.createShared();
		const prtx::WStringVector&    keys = m->getKeys();

		for (const auto& key : keys) {

			adsk::Data::Member::eDataType type;
			unsigned int size = 0;
			unsigned int arrayLength = 1;

			switch (m->getType(key)) {
			case prt::Attributable::PT_BOOL: type = adsk::Data::Member::kBoolean; size = 1;  break;
			case prt::Attributable::PT_FLOAT: type = adsk::Data::Member::kDouble; size = 1; break;
			case prt::Attributable::PT_INT: type = adsk::Data::Member::kInt32; size = 1; break;

			//workaround: using kString type crashes maya when setting metadata elememts. Therefore we use array of kUInt8
			case prt::Attributable::PT_STRING: type = adsk::Data::Member::kUInt8; size = maxStringLength;  break;
			case prt::Attributable::PT_BOOL_ARRAY: type = adsk::Data::Member::kBoolean; size = maxStringLength; break;
			case prt::Attributable::PT_INT_ARRAY: type = adsk::Data::Member::kInt32; size = maxStringLength; break;
			case prt::Attributable::PT_FLOAT_ARRAY: type = adsk::Data::Member::kDouble; size = maxFloatArrayLength; break;
			case prt::Attributable::PT_STRING_ARRAY: type = adsk::Data::Member::kUInt8; size = maxStringLength; arrayLength = maxStringArrayLength; break;

			case prtx::Material::PT_TEXTURE: type = adsk::Data::Member::kUInt8; size = maxStringLength;  break;
			case prtx::Material::PT_TEXTURE_ARRAY: type = adsk::Data::Member::kUInt8; size = maxStringLength; arrayLength = maxStringArrayLength; break;

			default:
				break;
			}

			if (size > 0) {
				for (unsigned int i=0; i<arrayLength; i++) {
					size_t maxStringLengthTmp = maxStringLength;
					char* tmp = new char[maxStringLength];
					std::wstring keyToUse = key;
					if (i>0)
						keyToUse = key + std::to_wstring(i);
					prt::StringUtils::toOSNarrowFromUTF16(keyToUse.c_str(), tmp, &maxStringLengthTmp);
					fStructure->addMember(type, size, tmp);
					delete tmp;
				}
			}
		}

		adsk::Data::Structure::registerStructure(*fStructure);

	}

	MCHECK(stat);
	MFnMesh inputMesh(inMeshObj);

	adsk::Data::Associations newMetadata(inputMesh.metadata(&stat));
	newMetadata.makeUnique();
	MCHECK(stat);
	adsk::Data::Channel newChannel = newMetadata.channel(gPRTMatChannel);
	adsk::Data::Stream newStream(*fStructure, gPRTMatStream);

	newChannel.setDataStream(newStream);
	newMetadata.setChannel(newChannel);

	if (faceRangesSize > 1) {

		for (size_t fri = 0; fri < faceRangesSize - 1; fri++) {

			if (materials != nullptr) {
				adsk::Data::Handle handle(*fStructure);


				const prt::AttributeMap* mat = materials[fri];

				char* tmp = new char[maxStringLength];

				size_t keyCount = 0;
				wchar_t const* const* keys = mat->getKeys(&keyCount);

				for (int k = 0; k < keyCount; k++) {

					wchar_t const* key = keys[k];

					size_t maxStringLengthTmp = maxStringLength;
					prt::StringUtils::toOSNarrowFromUTF16(key, tmp, &maxStringLengthTmp);

					if (!handle.setPositionByMemberName(tmp))
						continue;

					maxStringLengthTmp = maxStringLength;
					size_t arraySize = 0;

					switch (mat->getType(key)) {
					case prt::Attributable::PT_BOOL: handle.asBoolean()[0] = mat->getBool(key);  break;
					case prt::Attributable::PT_FLOAT: handle.asDouble()[0] = mat->getFloat(key);  break;
					case prt::Attributable::PT_INT: handle.asInt32()[0] = mat->getInt(key);  break;

						//workaround: transporting string as uint8 array, because using asString crashes maya
					case prt::Attributable::PT_STRING: {
						const wchar_t* str = mat->getString(key);
						if (wcslen(str) == 0)
							break;
						checkStringLength(str, maxStringLength);
						prt::StringUtils::toOSNarrowFromUTF16(str, (char*)handle.asUInt8(), &maxStringLengthTmp);
						break;
					}
					case prt::Attributable::PT_BOOL_ARRAY:
					{
						const bool* boolArray;
						boolArray = mat->getBoolArray(key, &arraySize);
						for (unsigned int i = 0; i < arraySize && i < maxStringLength; i++)
							handle.asBoolean()[i] = boolArray[i];
						break;
					}
					case prt::Attributable::PT_INT_ARRAY:
					{
						const int* intArray;
						intArray = mat->getIntArray(key, &arraySize);
						for (unsigned int i = 0; i < arraySize && i < maxStringLength; i++)
							handle.asInt32()[i] = intArray[i];
						break;
					}
					case prt::Attributable::PT_FLOAT_ARRAY: {
						const double* floatArray;
						floatArray = mat->getFloatArray(key, &arraySize);
						for (unsigned int i = 0; i < arraySize && i < maxStringLength && i < maxFloatArrayLength; i++)
							handle.asDouble()[i] = floatArray[i];
						break;
					}
					case prt::Attributable::PT_STRING_ARRAY: {

						const wchar_t* const* stringArray = mat->getStringArray(key, &arraySize);

						for (unsigned int i = 0; i < arraySize && i < maxStringLength; i++)
						{
							if (wcslen(stringArray[i]) == 0)
								continue;

							if (i > 0) {
								std::wstring keyToUse = key + std::to_wstring(i);
								maxStringLengthTmp = maxStringLength;
								prt::StringUtils::toOSNarrowFromUTF16(keyToUse.c_str(), tmp, &maxStringLengthTmp);
								if (!handle.setPositionByMemberName(tmp))
									continue;
							}

							maxStringLengthTmp = maxStringLength;
							checkStringLength(stringArray[i], maxStringLength);
							prt::StringUtils::toOSNarrowFromUTF16(stringArray[i], (char*)handle.asUInt8(), &maxStringLengthTmp);
						}
						break;
					}
					}

				}
				delete tmp;



				handle.setPositionByMemberName(gPRTMatMemberFaceStart.c_str());
				handle.asInt32()[0] = faceRanges[fri];

				handle.setPositionByMemberName(gPRTMatMemberFaceEnd.c_str());
				handle.asInt32()[0] = faceRanges[fri+1];

				newStream.setElement(fri, handle);

			}

			if (reports != nullptr) {
				//todo
			}


		}
	}

	outputMesh.setMetadata(newMetadata);
}

prt::Status MayaCallbacks::attrBool(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key, bool value) {
	mAttrs[key].mBool = value;
	return prt::STATUS_OK;
}

prt::Status MayaCallbacks::attrFloat(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key, double value) {
	mAttrs[key].mFloat = value;
	return prt::STATUS_OK;
}

prt::Status MayaCallbacks::attrString(size_t /*isIndex*/, int32_t /*shapeID*/, const wchar_t* key, const wchar_t* value) {
	mAttrs[key].mString = value;
	return prt::STATUS_OK;
}
