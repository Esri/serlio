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

#include "encoder/IMayaCallbacks.h"
#include "encoder/MayaEncoder.h"
#include "encoder/TextureEncoder.h"

#include "prtx/Attributable.h"
#include "prtx/DataBackend.h"
#include "prtx/Exception.h"
#include "prtx/ExtensionManager.h"
#include "prtx/GenerateContext.h"
#include "prtx/Geometry.h"
#include "prtx/Log.h"
#include "prtx/Material.h"
#include "prtx/Mesh.h"
#include "prtx/ReportsCollector.h"
#include "prtx/Shape.h"
#include "prtx/ShapeIterator.h"
#include "prtx/URI.h"

#include "prt/MemoryOutputCallbacks.h"
#include "prt/prt.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <vector>

// PRT version < 2.1
#if ((PRT_VERSION_MAJOR <= 2) && ((PRT_VERSION_MAJOR < 2) || (PRT_VERSION_MINOR < 1)))
using srl_log_fatal = log_wfatal;
using srl_log_error = log_werror;
using srl_log_warn = log_wwarn;
using srl_log_info = log_winfo;
using srl_log_debug = log_wdebug;
using srl_log_trace = log_wtrace;
#else
using srl_log_fatal = log_fatal;
using srl_log_error = log_error;
using srl_log_warn = log_warn;
using srl_log_info = log_info;
using srl_log_debug = log_debug;
using srl_log_trace = log_trace;
#endif // PRT version < 2.1

namespace {

constexpr bool DBG = false;

constexpr const wchar_t* ENC_NAME = L"Autodesk(tm) Maya(tm) Encoder";
constexpr const wchar_t* ENC_DESCRIPTION = L"Encodes geometry into the Maya format.";

const prtx::EncodePreparator::PreparationFlags PREP_FLAGS =
        prtx::EncodePreparator::PreparationFlags()
                .instancing(false)
                .meshMerging(prtx::MeshMerging::ALL_OF_SAME_MATERIAL_AND_TYPE)
                .triangulate(false)
                .processHoles(prtx::HoleProcessor::TRIANGULATE_FACES_WITH_HOLES)
                .mergeVertices(true)
                .cleanupVertexNormals(true)
                .cleanupUVs(true)
                .processVertexNormals(prtx::VertexNormalProcessor::SET_MISSING_TO_FACE_NORMALS)
                .indexSharing(prtx::EncodePreparator::PreparationFlags::INDICES_SEPARATE_FOR_ALL_VERTEX_ATTRIBUTES);

std::vector<const wchar_t*> toPtrVec(const prtx::WStringVector& wsv) {
	std::vector<const wchar_t*> pw(wsv.size());
	for (size_t i = 0; i < wsv.size(); i++)
		pw[i] = wsv[i].c_str();
	return pw;
}

template <typename T>
std::pair<std::vector<const T*>, std::vector<size_t>> toPtrVec(const std::vector<std::vector<T>>& v) {
	std::vector<const T*> pv(v.size());
	std::vector<size_t> ps(v.size());
	for (size_t i = 0; i < v.size(); i++) {
		pv[i] = v[i].data();
		ps[i] = v[i].size();
	}
	return std::make_pair(pv, ps);
}

template <typename C, typename FUNC, typename OBJ, typename... ARGS>
std::basic_string<C> callAPI(FUNC f, OBJ& obj, ARGS&&... args) {
	std::vector<C> buffer(1024, 0x0);
	size_t size = buffer.size();
	std::invoke(f, obj, args..., buffer.data(), size);
	if (size == 0)
		return {}; // error case
	else if (size > buffer.size()) {
		buffer.resize(size);
		std::invoke(f, obj, args..., buffer.data(), size);
	}
	return {buffer.data()};
}

std::wstring getTexturePath(const prtx::TexturePtr& texture, IMayaCallbacks* callbacks, prt::Cache* cache) {
	if (!texture || !texture->isValid())
		return {};

	const prtx::URIPtr& uri = texture->getURI();
	const std::wstring& uriStr = uri->wstring();
	const std::wstring& scheme = uri->getScheme();

	if (!uri->isComposite() && (scheme == prtx::URI::SCHEME_FILE || scheme == prtx::URI::SCHEME_UNC)) {
		// textures from the local file system or a mounted share on Windows can be directly passed to Serlio
		return uri->getNativeFormat();
	}
	else if (uri->isComposite() && (scheme == prtx::URI::SCHEME_RPK)) {
		// textures from within an RPK can be directly copied out, no need for encoding
		// just need to make sure we have useful filename for embedded texture blocks without names

		const prtx::BinaryVectorPtr data = prtx::DataBackend::resolveBinaryData(cache, uriStr);
		const std::wstring fileName = uri->getBaseName() + uri->getExtension();
		const std::wstring assetPath = callAPI<wchar_t>(&IMayaCallbacks::addAsset, *callbacks, uriStr.c_str(),
		                                                fileName.c_str(), data->data(), data->size());
		return assetPath;
	}
	else {
		// all other textures (builtin or from memory) need to be extracted and potentially re-encoded
		try {
			prtx::PRTUtils::MemoryOutputCallbacksUPtr moc(prt::MemoryOutputCallbacks::create());

			prtx::AsciiFileNamePreparator namePrep;
			const prtx::NamePreparator::NamespacePtr& namePrepNamespace = namePrep.newNamespace();
			const std::wstring validatedFilename =
			        TextureEncoder::encode(texture, moc.get(), namePrep, namePrepNamespace, {});

			if (moc->getNumBlocks() == 1) {
				size_t bufferSize = 0;
				const uint8_t* buffer = moc->getBlock(0, &bufferSize);

				const std::wstring assetPath = callAPI<wchar_t>(&IMayaCallbacks::addAsset, *callbacks, uriStr.c_str(),
				                                                validatedFilename.c_str(), buffer, bufferSize);
				if (!assetPath.empty())
					return assetPath;
				else
					srl_log_warn("Received invalid asset path while trying to write asset with URI: %1%") % uriStr;
			}
			else {
				srl_log_warn("Failed to get texture at %1%, texture will be missing") % uriStr;
			}
		}
		catch (std::exception& e) {
			srl_log_warn("Failed to encode or write texture at %1% to the local filesystem: %2%") % uriStr % e.what();
		}
	}

	return {};
}

// we blacklist all CGA-style material attribute keys, see prtx/Material.h
const std::set<std::wstring> MATERIAL_ATTRIBUTE_BLACKLIST = {
        L"ambient.b",
        L"ambient.g",
        L"ambient.r",
        L"bumpmap.rw",
        L"bumpmap.su",
        L"bumpmap.sv",
        L"bumpmap.tu",
        L"bumpmap.tv",
        L"color.a",
        L"color.b",
        L"color.g",
        L"color.r",
        L"color.rgb",
        L"colormap.rw",
        L"colormap.su",
        L"colormap.sv",
        L"colormap.tu",
        L"colormap.tv",
        L"dirtmap.rw",
        L"dirtmap.su",
        L"dirtmap.sv",
        L"dirtmap.tu",
        L"dirtmap.tv",
        L"normalmap.rw",
        L"normalmap.su",
        L"normalmap.sv",
        L"normalmap.tu",
        L"normalmap.tv",
        L"opacitymap.rw",
        L"opacitymap.su",
        L"opacitymap.sv",
        L"opacitymap.tu",
        L"opacitymap.tv",
        L"specular.b",
        L"specular.g",
        L"specular.r",
        L"specularmap.rw",
        L"specularmap.su",
        L"specularmap.sv",
        L"specularmap.tu",
        L"specularmap.tv",
        L"bumpmap",
        L"colormap",
        L"dirtmap",
        L"normalmap",
        L"opacitymap",
        L"opacitymap.mode",
        L"specularmap"

#if PRT_VERSION_MAJOR > 1
        // also blacklist CGA-style PBR attrs from CE 2019.0, PRT 2.x
        ,
        L"emissive.b",
        L"emissive.g",
        L"emissive.r",
        L"emissivemap.rw",
        L"emissivemap.su",
        L"emissivemap.sv",
        L"emissivemap.tu",
        L"emissivemap.tv",
        L"metallicmap.rw",
        L"metallicmap.su",
        L"metallicmap.sv",
        L"metallicmap.tu",
        L"metallicmap.tv",
        L"occlusionmap.rw",
        L"occlusionmap.su",
        L"occlusionmap.sv",
        L"occlusionmap.tu",
        L"occlusionmap.tv",
        L"roughnessmap.rw",
        L"roughnessmap.su",
        L"roughnessmap.sv",
        L"roughnessmap.tu",
        L"roughnessmap.tv",
        L"emissivemap",
        L"metallicmap",
        L"occlusionmap",
        L"roughnessmap"
#endif
};

void convertMaterialToAttributeMap(prtx::PRTUtils::AttributeMapBuilderPtr& aBuilder, const prtx::Material& prtxAttr,
                                   const prtx::WStringVector& keys, IMayaCallbacks* cb, prt::Cache* cache) {
	if constexpr (DBG)
		srl_log_debug(L"-- converting material: %1%") % prtxAttr.name();
	for (const auto& key : keys) {
		if (MATERIAL_ATTRIBUTE_BLACKLIST.count(key) > 0)
			continue;

		if constexpr (DBG)
			srl_log_debug(L"   key: %1%") % key;

		switch (prtxAttr.getType(key)) {
			case prt::Attributable::PT_BOOL:
				aBuilder->setBool(key.c_str(), prtxAttr.getBool(key) == prtx::PRTX_TRUE);
				break;

			case prt::Attributable::PT_FLOAT:
				aBuilder->setFloat(key.c_str(), prtxAttr.getFloat(key));
				break;

			case prt::Attributable::PT_INT:
				aBuilder->setInt(key.c_str(), prtxAttr.getInt(key));
				break;

			case prt::Attributable::PT_STRING: {
				const std::wstring& v = prtxAttr.getString(key); // explicit copy
				aBuilder->setString(key.c_str(), v.c_str());     // also passing on empty strings
				break;
			}

			case prt::Attributable::PT_BOOL_ARRAY: {
				const std::vector<uint8_t>& ba = prtxAttr.getBoolArray(key);
				auto boo = std::unique_ptr<bool[]>(new bool[ba.size()]);
				for (size_t i = 0; i < ba.size(); i++)
					boo[i] = (ba[i] == prtx::PRTX_TRUE);
				aBuilder->setBoolArray(key.c_str(), boo.get(), ba.size());
				break;
			}

			case prt::Attributable::PT_INT_ARRAY: {
				const std::vector<int32_t>& array = prtxAttr.getIntArray(key);
				aBuilder->setIntArray(key.c_str(), &array[0], array.size());
				break;
			}

			case prt::Attributable::PT_FLOAT_ARRAY: {
				const std::vector<double>& array = prtxAttr.getFloatArray(key);
				aBuilder->setFloatArray(key.c_str(), array.data(), array.size());
				break;
			}

			case prt::Attributable::PT_STRING_ARRAY: {
				const prtx::WStringVector& a = prtxAttr.getStringArray(key);
				std::vector<const wchar_t*> pw = toPtrVec(a);
				aBuilder->setStringArray(key.c_str(), pw.data(), pw.size());
				break;
			}

			case prtx::Material::PT_TEXTURE: {
				const auto& t = prtxAttr.getTexture(key);
				const std::wstring p = getTexturePath(t, cb, cache);
				aBuilder->setString(key.c_str(), p.c_str());
				break;
			}

			case prtx::Material::PT_TEXTURE_ARRAY: {
				const auto& ta = prtxAttr.getTextureArray(key);

				prtx::WStringVector texPaths;
				texPaths.reserve(ta.size());

				for (const auto& tex : ta) {
					const std::wstring texPath = getTexturePath(tex, cb, cache);
					if (!texPath.empty())
						texPaths.push_back(texPath);
				}

				const std::vector<const wchar_t*> pTexPaths = toPtrVec(texPaths);
				aBuilder->setStringArray(key.c_str(), pTexPaths.data(), pTexPaths.size());

				break;
			}

			default:
				if constexpr (DBG)
					srl_log_debug(L"ignored atttribute '%s' with type %d") % key % prtxAttr.getType(key);
				break;
		}
	}
}

void convertReportsToAttributeMap(prtx::PRTUtils::AttributeMapBuilderPtr& amb, const prtx::ReportsPtr& r) {
	if (!r)
		return;

	for (const auto& b : r->mBools)
		amb->setBool(b.first->c_str(), b.second);
	for (const auto& f : r->mFloats)
		amb->setFloat(f.first->c_str(), f.second);
	for (const auto& s : r->mStrings)
		amb->setString(s.first->c_str(), s.second->c_str());
}

template <typename F>
void forEachKey(prt::Attributable const* a, F f) {
	if (a == nullptr)
		return;

	size_t keyCount = 0;
	wchar_t const* const* keys = a->getKeys(&keyCount);

	for (size_t k = 0; k < keyCount; k++) {
		wchar_t const* const key = keys[k];
		f(a, key);
	}
}

void forwardGenericAttributes(IMayaCallbacks* hc, size_t initialShapeIndex, const prtx::InitialShape& initialShape,
                              const prtx::ShapePtr& shape) {
	forEachKey(initialShape.getAttributeMap(),
	           [&hc, &shape, &initialShapeIndex, &initialShape](prt::Attributable const* a, wchar_t const* key) {
		           switch (shape->getType(key)) {
			           case prtx::Attributable::PT_STRING: {
				           const auto v = shape->getString(key);
				           hc->attrString(initialShapeIndex, shape->getID(), key, v.c_str());
				           break;
			           }
			           case prtx::Attributable::PT_FLOAT: {
				           const auto v = shape->getFloat(key);
				           hc->attrFloat(initialShapeIndex, shape->getID(), key, v);
				           break;
			           }
			           case prtx::Attributable::PT_BOOL: {
				           const auto v = shape->getBool(key);
				           hc->attrBool(initialShapeIndex, shape->getID(), key, (v == prtx::PRTX_TRUE));
				           break;
			           }
			           default:
				           break;
		           }
	           });
}

using AttributeMapNOPtrVector = std::vector<const prt::AttributeMap*>;

struct AttributeMapNOPtrVectorOwner {
	AttributeMapNOPtrVector v;
	~AttributeMapNOPtrVectorOwner() {
		for (const auto& m : v) {
			if (m != nullptr)
				m->destroy();
		}
	}
};

class SerializedGeometry {
public:
	SerializedGeometry(const prtx::GeometryPtrVector& geometries,
	                   const std::vector<prtx::MaterialPtrVector>& materials) {
		reserveMemory(geometries, materials);
		serialize(geometries, materials);
	}

	bool isEmpty() const {
		return mCoords.empty() || mCounts.empty() || mVertexIndices.empty();
	}

private:
	void reserveMemory(const prtx::GeometryPtrVector& geometries,
	                   const std::vector<prtx::MaterialPtrVector>& materials) {
		// Allocate memory for geometry
		uint32_t numCounts = 0;
		uint32_t numIndices = 0;
		uint32_t maxNumUVSets = 0;

		auto matsIt = materials.cbegin();
		for (const auto& geo : geometries) {
			const prtx::MeshPtrVector& meshes = geo->getMeshes();
			const prtx::MaterialPtrVector& mats = *matsIt;
			auto matIt = mats.cbegin();
			for (const auto& mesh : meshes) {
				numCounts += mesh->getFaceCount();
				const auto& vtxCnts = mesh->getFaceVertexCounts();
				numIndices = std::accumulate(vtxCnts.begin(), vtxCnts.end(), numIndices);

				const prtx::MaterialPtr& mat = *matIt;
				const uint32_t requiredUVSetsByMaterial = scanValidTextures(mat);
				maxNumUVSets = std::max(maxNumUVSets, std::max(mesh->getUVSetsCount(), requiredUVSetsByMaterial));
				++matIt;
			}
			++matsIt;
		}

		mCounts.reserve(numCounts);
		mVertexIndices.reserve(numIndices);
		mNormalIndices.reserve(numIndices);

		// Allocate memory for uvs
		std::vector<uint32_t> numUvs(maxNumUVSets);
		std::vector<uint32_t> numUvCounts(maxNumUVSets);
		std::vector<uint32_t> numUvIndices(maxNumUVSets);

		for (const auto& geo : geometries) {
			const prtx::MeshPtrVector& meshes = geo->getMeshes();
			for (const auto& mesh : meshes) {
				const uint32_t numUVSets = mesh->getUVSetsCount();

				for (uint32_t uvSet = 0; uvSet < numUVSets; uvSet++) {
					numUvs[uvSet] += static_cast<uint32_t>(mesh->getUVCoords(uvSet).size());

					const auto& faceUVCounts = mesh->getFaceUVCounts(uvSet);
					numUvCounts[uvSet] += static_cast<uint32_t>(faceUVCounts.size());
					numUvIndices[uvSet] =
					        std::accumulate(faceUVCounts.begin(), faceUVCounts.end(), numUvIndices[uvSet]);
				}
			}
		}

		mUvs.resize(maxNumUVSets);
		mUvCounts.resize(maxNumUVSets);
		mUvIndices.resize(maxNumUVSets);

		for (uint32_t uvSet = 0; uvSet < maxNumUVSets; uvSet++) {
			mUvs[uvSet].reserve(numUvs[uvSet]);
			mUvCounts[uvSet].reserve(numUvCounts[uvSet]);
			mUvIndices[uvSet].reserve(numUvIndices[uvSet]);
		}
	}

	void serialize(const prtx::GeometryPtrVector& geometries, const std::vector<prtx::MaterialPtrVector>& materials) {
		const uint32_t maxNumUVSets = static_cast<uint32_t>(mUvs.size());

		const prtx::DoubleVector EMPTY_UVS;
		const prtx::IndexVector EMPTY_IDX;

		// Copy data into serialized geometry
		uint32_t vertexIndexBase = 0u;
		uint32_t normalIndexBase = 0u;
		std::vector<uint32_t> uvIndexBases(maxNumUVSets, 0u);
		for (const auto& geo : geometries) {
			const prtx::MeshPtrVector& meshes = geo->getMeshes();
			for (const auto& mesh : meshes) {
				// append points
				const prtx::DoubleVector& verts = mesh->getVertexCoords();
				mCoords.insert(mCoords.end(), verts.begin(), verts.end());

				// append normals
				const prtx::DoubleVector& norms = mesh->getVertexNormalsCoords();
				mNormals.insert(mNormals.end(), norms.begin(), norms.end());

				// append uv sets (uv coords, counts, indices) with special cases:
				// - if mesh has no uv sets but maxNumUVSets is > 0, insert "0" uv face counts to keep in sync
				// - if mesh has less uv sets than maxNumUVSets, copy uv set 0 to the missing higher sets
				const uint32_t numUVSets = mesh->getUVSetsCount();
				const prtx::DoubleVector& uvs0 = (numUVSets > 0) ? mesh->getUVCoords(0) : EMPTY_UVS;
				const prtx::IndexVector faceUVCounts0 =
				        (numUVSets > 0) ? mesh->getFaceUVCounts(0) : prtx::IndexVector(mesh->getFaceCount(), 0);
				if constexpr (DBG)
					srl_log_debug("-- mesh: numUVSets = %1%") % numUVSets;

				for (uint32_t uvSet = 0; uvSet < mUvs.size(); uvSet++) {
					// append texture coordinates
					const prtx::DoubleVector& uvs = (uvSet < numUVSets) ? mesh->getUVCoords(uvSet) : EMPTY_UVS;
					const auto& src = uvs.empty() ? uvs0 : uvs;
					auto& tgt = mUvs[uvSet];
					tgt.insert(tgt.end(), src.begin(), src.end());

					// append uv face counts
					const prtx::IndexVector& faceUVCounts =
					        (uvSet < numUVSets && !uvs.empty()) ? mesh->getFaceUVCounts(uvSet) : faceUVCounts0;
					assert(faceUVCounts.size() == mesh->getFaceCount());
					auto& tgtCnts = mUvCounts[uvSet];
					tgtCnts.insert(tgtCnts.end(), faceUVCounts.begin(), faceUVCounts.end());
					if constexpr (DBG)
						srl_log_debug("   -- uvset %1%: face counts size = %2%") % uvSet % faceUVCounts.size();

					// append uv vertex indices
					for (uint32_t fi = 0, faceCount = static_cast<uint32_t>(faceUVCounts.size()); fi < faceCount;
					     ++fi) {
						const uint32_t* faceUVIdx0 = (numUVSets > 0) ? mesh->getFaceUVIndices(fi, 0) : EMPTY_IDX.data();
						const uint32_t* faceUVIdx =
						        (uvSet < numUVSets && !uvs.empty()) ? mesh->getFaceUVIndices(fi, uvSet) : faceUVIdx0;
						const uint32_t faceUVCnt = faceUVCounts[fi];
						if constexpr (DBG)
							srl_log_debug("      fi %1%: faceUVCnt = %2%, faceVtxCnt = %3%") % fi % faceUVCnt %
							        mesh->getFaceVertexCount(fi);
						for (uint32_t vi = 0; vi < faceUVCnt; vi++)
							mUvIndices[uvSet].push_back(uvIndexBases[uvSet] + faceUVIdx[vi]);
					}

					uvIndexBases[uvSet] += static_cast<uint32_t>(src.size()) / 2;
				} // for all uv sets

				// append counts and indices for vertices and vertex normals
				for (uint32_t fi = 0, faceCount = mesh->getFaceCount(); fi < faceCount; ++fi) {
					const uint32_t vtxCnt = mesh->getFaceVertexCount(fi);
					mCounts.push_back(vtxCnt);
					const uint32_t* vtxIdx = mesh->getFaceVertexIndices(fi);
					const uint32_t* nrmIdx = mesh->getFaceVertexNormalIndices(fi);
					const size_t nrmCnt = mesh->getFaceVertexNormalCount(fi);
					for (uint32_t vi = 0; vi < vtxCnt; vi++) {
						mVertexIndices.push_back(vertexIndexBase + vtxIdx[vi]);
						if (nrmCnt > vi && nrmIdx != nullptr)
							mNormalIndices.push_back(normalIndexBase + nrmIdx[vi]);
					}
				}

				vertexIndexBase += (uint32_t)verts.size() / 3u;
				normalIndexBase += (uint32_t)norms.size() / 3u;
			} // for all meshes
		}     // for all geometries
	}

	struct TextureUVMapping {
		std::wstring key;
		uint8_t index;
		int8_t uvSet;
	};

	// return the highest required uv set (where a valid texture is present)
	uint32_t scanValidTextures(const prtx::MaterialPtr& mat) {

		// clang-format off
		static const std::vector<TextureUVMapping> TEXTURE_UV_MAPPINGS = []() -> std::vector<TextureUVMapping> {
			return {
					// shader key | idx | uv set  | CGA key
					{ L"diffuseMap",   0,    0 },  // colormap
					{ L"bumpMap",      0,    1 },  // bumpmap
					{ L"diffuseMap",   1,    2 },  // dirtmap
					{ L"specularMap",  0,    3 },  // specularmap
					{ L"opacityMap",   0,    4 },  // opacitymap
					{ L"normalMap",    0,    5 }   // normalmap

					#if PRT_VERSION_MAJOR > 1
					,
					{ L"emissiveMap",  0,    6 },  // emissivemap
					{ L"occlusionMap", 0,    7 },  // occlusionmap
					{ L"roughnessMap", 0,    8 },  // roughnessmap
					{ L"metallicMap",  0,    9 }   // metallicmap
					#endif
			};
		}();
		// clang-format on

		int8_t highestUVSet = -1;
		for (const auto& t : TEXTURE_UV_MAPPINGS) {
			const auto& ta = mat->getTextureArray(t.key);
			if (ta.size() > t.index && ta[t.index]->isValid())
				highestUVSet = std::max(highestUVSet, t.uvSet);
		}
		if (highestUVSet < 0)
			return 0;
		else
			return highestUVSet + 1;
	}

public:
	prtx::DoubleVector mCoords;
	prtx::DoubleVector mNormals;
	std::vector<uint32_t> mCounts;
	std::vector<uint32_t> mVertexIndices;
	std::vector<uint32_t> mNormalIndices;

	std::vector<prtx::DoubleVector> mUvs;
	std::vector<prtx::IndexVector> mUvCounts;
	std::vector<prtx::IndexVector> mUvIndices;
};

} // namespace

MayaEncoder::MayaEncoder(const std::wstring& id, const prt::AttributeMap* options, prt::Callbacks* callbacks)
    : prtx::GeometryEncoder(id, options, callbacks) {}

void MayaEncoder::init(prtx::GenerateContext&) {
	prt::Callbacks* cb = getCallbacks();
	if constexpr (DBG)
		srl_log_debug(L"MayaEncoder::init: cb = %x") % (size_t)cb;
	auto* oh = dynamic_cast<IMayaCallbacks*>(cb);
	if constexpr (DBG)
		srl_log_debug(L"                   oh = %x") % (size_t)oh;
	if (oh == nullptr)
		throw prtx::StatusException(prt::STATUS_ILLEGAL_CALLBACK_OBJECT);
}

void MayaEncoder::encode(prtx::GenerateContext& context, size_t initialShapeIndex) {
	const prtx::InitialShape& initialShape = *context.getInitialShape(initialShapeIndex);
	auto* cb = dynamic_cast<IMayaCallbacks*>(getCallbacks());

	const bool emitAttrs = getOptions()->getBool(EO_EMIT_ATTRIBUTES);

	prtx::DefaultNamePreparator namePrep;
	prtx::NamePreparator::NamespacePtr nsMesh = namePrep.newNamespace();
	prtx::NamePreparator::NamespacePtr nsMaterial = namePrep.newNamespace();
	prtx::EncodePreparatorPtr encPrep = prtx::EncodePreparator::create(true, namePrep, nsMesh, nsMaterial);

	// generate geometry
	prtx::ReportsAccumulatorPtr reportsAccumulator{prtx::WriteFirstReportsAccumulator::create()};
	prtx::ReportingStrategyPtr reportsCollector{
	        prtx::LeafShapeReportingStrategy::create(context, initialShapeIndex, reportsAccumulator)};
	prtx::LeafIteratorPtr li = prtx::LeafIterator::create(context, initialShapeIndex);
	for (prtx::ShapePtr shape = li->getNext(); shape; shape = li->getNext()) {
		prtx::ReportsPtr r = reportsCollector->getReports(shape->getID());
		encPrep->add(context.getCache(), shape, initialShape.getAttributeMap(), r);

		// get final values of generic attributes
		if (emitAttrs)
			forwardGenericAttributes(cb, initialShapeIndex, initialShape, shape);
	}

	prtx::EncodePreparator::InstanceVector instances;
	encPrep->fetchFinalizedInstances(instances, PREP_FLAGS);
	convertGeometry(initialShape, instances, cb, context.getCache());
}

void MayaEncoder::convertGeometry(const prtx::InitialShape& initialShape,
                                  const prtx::EncodePreparator::InstanceVector& instances, IMayaCallbacks* cb,
                                  prt::Cache* cache) {
	if (instances.empty())
		return;

	const bool emitMaterials = getOptions()->getBool(EO_EMIT_MATERIALS);
	const bool emitReports = getOptions()->getBool(EO_EMIT_REPORTS);

	prtx::GeometryPtrVector geometries;
	std::vector<prtx::MaterialPtrVector> materials;
	std::vector<prtx::ReportsPtr> reports;
	std::vector<int32_t> shapeIDs;

	geometries.reserve(instances.size());
	materials.reserve(instances.size());
	reports.reserve(instances.size());
	shapeIDs.reserve(instances.size());

	for (const auto& inst : instances) {
		geometries.push_back(inst.getGeometry());
		materials.push_back(inst.getMaterials());
		reports.push_back(inst.getReports());
		shapeIDs.push_back(inst.getShapeId());
	}

	const SerializedGeometry sg(geometries, materials);

	if (sg.isEmpty())
		return;

	if constexpr (DBG) {
		srl_log_debug("resolvemap: %s") % prtx::PRTUtils::objectToXML(initialShape.getResolveMap());
		srl_log_debug("encoder #materials = %s") % materials.size();
	}

	uint32_t faceCount = 0;
	std::vector<uint32_t> faceRanges;
	AttributeMapNOPtrVectorOwner matAttrMaps;
	AttributeMapNOPtrVectorOwner reportAttrMaps;

	assert(geometries.size() == reports.size());
	assert(materials.size() == reports.size());
	auto matIt = materials.cbegin();
	auto repIt = reports.cbegin();
	prtx::PRTUtils::AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
	for (const auto& geo : geometries) {
		const prtx::MeshPtrVector& meshes = geo->getMeshes();

		for (size_t mi = 0; mi < meshes.size(); mi++) {
			const prtx::MeshPtr& m = meshes.at(mi);
			const prtx::MaterialPtr& mat = matIt->at(mi);

			faceRanges.push_back(faceCount);

			if (emitMaterials) {
				convertMaterialToAttributeMap(amb, *(mat.get()), mat->getKeys(), cb, cache);
				matAttrMaps.v.push_back(amb->createAttributeMapAndReset());
			}

			if (emitReports) {
				convertReportsToAttributeMap(amb, *repIt);
				reportAttrMaps.v.push_back(amb->createAttributeMapAndReset());
				if constexpr (DBG)
					srl_log_debug("report attr map: %1%") % prtx::PRTUtils::objectToXML(reportAttrMaps.v.back());
			}

			faceCount += m->getFaceCount();
		}

		++matIt;
		++repIt;
	}
	faceRanges.push_back(faceCount); // close last range

	assert(matAttrMaps.v.empty() || matAttrMaps.v.size() == faceRanges.size() - 1);
	assert(reportAttrMaps.v.empty() || reportAttrMaps.v.size() == faceRanges.size() - 1);
	assert(shapeIDs.size() == faceRanges.size() - 1);

	auto puvs = toPtrVec(sg.mUvs);
	auto puvCounts = toPtrVec(sg.mUvCounts);
	auto puvIndices = toPtrVec(sg.mUvIndices);

	cb->addMesh(initialShape.getName(), sg.mCoords.data(), sg.mCoords.size(), sg.mNormals.data(), sg.mNormals.size(),
	            sg.mCounts.data(), sg.mCounts.size(), sg.mVertexIndices.data(), sg.mVertexIndices.size(),
	            sg.mNormalIndices.data(), sg.mNormalIndices.size(),

	            puvs.first.data(), puvs.second.data(), puvCounts.first.data(), puvCounts.second.data(),
	            puvIndices.first.data(), puvIndices.second.data(), sg.mUvs.size(),

	            faceRanges.data(), faceRanges.size(), matAttrMaps.v.empty() ? nullptr : matAttrMaps.v.data(),
	            reportAttrMaps.v.empty() ? nullptr : reportAttrMaps.v.data(), shapeIDs.data());

	if constexpr (DBG)
		srl_log_debug(L"MayaEncoder::convertGeometry: end");
}

void MayaEncoder::finish(prtx::GenerateContext& /*context*/) {}

MayaEncoderFactory* MayaEncoderFactory::createInstance() {
	prtx::EncoderInfoBuilder encoderInfoBuilder;

	encoderInfoBuilder.setID(ENCODER_ID_Maya);
	encoderInfoBuilder.setName(ENC_NAME);
	encoderInfoBuilder.setDescription(ENC_DESCRIPTION);
	encoderInfoBuilder.setType(prt::CT_GEOMETRY);

	prtx::PRTUtils::AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
	amb->setBool(EO_EMIT_ATTRIBUTES, prtx::PRTX_TRUE);
	amb->setBool(EO_EMIT_MATERIALS, prtx::PRTX_TRUE);
	amb->setBool(EO_EMIT_REPORTS, prtx::PRTX_FALSE);
	encoderInfoBuilder.setDefaultOptions(amb->createAttributeMap());

	return new MayaEncoderFactory(encoderInfoBuilder.create());
}
