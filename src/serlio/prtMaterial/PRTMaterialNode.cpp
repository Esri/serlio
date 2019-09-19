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

#include "prtMaterial/PRTMaterialNode.h"
#include "prtModifier/PRTModifierAction.h"

#include "util/Utilities.h"
#include "util/MayaUtilities.h"

#include "prt/StringUtils.h"

#include "maya/MGlobal.h"
#include "maya/MObject.h"
#include "maya/MFnMesh.h"
#include "maya/MFnTypedAttribute.h"
#include "maya/MItDependencyNodes.h"
#include "maya/adskDataStream.h"
#include "maya/adskDataAssociations.h"

#include <cstdio>
#include <sstream>
#include <set>
#include <algorithm>
#include <array>

#ifdef _WIN32
#	include <Windows.h>
#else
#	include <dlfcn.h>
#endif


namespace {

constexpr bool DBG = false;

} // namespace

MTypeId PRTMaterialNode::id(PRT_MATERIAL_TYPE_ID);

MObject	PRTMaterialNode::aInMesh;
MObject PRTMaterialNode::aOutMesh;
MString PRTMaterialNode::sfxFile;
const MString OUTPUT_GEOMETRY = MString("og");

MStatus PRTMaterialNode::initialize()
{
	MStatus status;

	MFnTypedAttribute tAttr;
	aInMesh = tAttr.create("inMesh", "im", MFnData::kMesh, MObject::kNullObj, &status);
	MCHECK(status);
	addAttribute(aInMesh);

	aOutMesh = tAttr.create("outMesh", "om", MFnData::kMesh, MObject::kNullObj, &status);
	MCHECK(status);
	tAttr.setWritable(false);
	tAttr.setStorable(false);
	addAttribute(aOutMesh);

	attributeAffects(aInMesh, aOutMesh);

	return MStatus::kSuccess;
}

MString MaterialInfo::toMString(const std::vector<double>& d, size_t size, size_t offset)
{
	MString colString;
	for (size_t i = offset; i < d.size() && i < offset+size; i++)
	{
		colString += d[i];
		colString += " ";
	}
	return colString;
}

std::string MaterialInfo::getTexture(adsk::Data::Handle sHandle, const std::string& texName)
{
	std::string r;
	if (sHandle.setPositionByMemberName(texName.c_str()))
		r = (char*)sHandle.asUInt8();
	return r;
}

std::vector<double> MaterialInfo::getDoubleVector(adsk::Data::Handle sHandle, const std::string& name, size_t numElements)
{
	std::vector<double> r;
	if (sHandle.setPositionByMemberName(name.c_str()))
	{
		double* data = sHandle.asDouble();
		if (sHandle.dataLength() >= numElements && data) {
			r.reserve(numElements);
			std::copy(data, data + numElements, std::back_inserter(r));
		}
	}
	return r;
}

double MaterialInfo::getDouble(adsk::Data::Handle sHandle, const std::string& name)
{
	if (sHandle.setPositionByMemberName(name.c_str()))
	{
		double* data = sHandle.asDouble();
		if (sHandle.dataLength() >= 1 && data) {
			return *data;
		}
	}
	return NAN;
}

MaterialInfo::MaterialInfo(adsk::Data::Handle sHandle) {
	bumpMap = getTexture(sHandle, "bumpMap");
	colormap = getTexture(sHandle, "diffuseMap");
	dirtmap = getTexture(sHandle, "diffuseMap1");
	emissiveMap = getTexture(sHandle, "emissiveMap");
	metallicMap = getTexture(sHandle, "metallicMap");
	normalMap = getTexture(sHandle, "normalMap");
	occlusionMap = getTexture(sHandle, "occlusionMap");
	opacityMap = getTexture(sHandle, "opacityMap");
	roughnessMap = getTexture(sHandle, "roughnessMap");
	specularMap = getTexture(sHandle, "specularMap");

	opacity = getDouble(sHandle, "opacity");
	metallic = getDouble(sHandle, "metallic");
	roughness = getDouble(sHandle, "roughness");

	ambientColor = getDoubleVector(sHandle, "ambientColor", 3);
	bumpmapTrafo = getDoubleVector(sHandle, "bumpmapTrafo", 5);
	colormapTrafo = getDoubleVector(sHandle, "colormapTrafo", 5);
	diffuseColor = getDoubleVector(sHandle, "diffuseColor", 3);
	dirtmapTrafo = getDoubleVector(sHandle, "dirtmapTrafo", 5);
	emissiveColor = getDoubleVector(sHandle, "emissiveColor", 3);
	emissivemapTrafo = getDoubleVector(sHandle, "emissivemapTrafo", 5);
	metallicmapTrafo = getDoubleVector(sHandle, "metallicmapTrafo", 5);
	normalmapTrafo = getDoubleVector(sHandle, "normalmapTrafo", 5);
	occlusionmapTrafo = getDoubleVector(sHandle, "occlusionmapTrafo", 5);
	opacitymapTrafo = getDoubleVector(sHandle, "opacitymapTrafo", 5);
	roughnessmapTrafo = getDoubleVector(sHandle, "roughnessmapTrafo", 5);
	specularColor = getDoubleVector(sHandle, "specularColor", 3);
	specularmapTrafo = getDoubleVector(sHandle, "specularmapTrafo", 5);
}

bool MaterialInfo::equals(const MaterialInfo& o) const {
	return
		bumpMap == o.bumpMap &&
		colormap == o.colormap &&
		dirtmap == o.dirtmap &&
		emissiveMap == o.emissiveMap &&
		metallicMap == o.metallicMap &&
		normalMap == o.normalMap &&
		occlusionMap == o.occlusionMap &&
		opacityMap == o.opacityMap &&
		roughnessMap == o.roughnessMap &&
		specularMap == o.specularMap &&
		opacity == o.opacity &&
		metallic == o.metallic &&
		roughness == o.roughness &&
		ambientColor == o.ambientColor &&
		bumpmapTrafo == o.bumpmapTrafo &&
		colormapTrafo == o.colormapTrafo &&
		diffuseColor == o.diffuseColor &&
		dirtmapTrafo == o.dirtmapTrafo &&
		emissiveColor == o.emissiveColor &&
		emissivemapTrafo == o.emissivemapTrafo &&
		metallicmapTrafo == o.metallicmapTrafo &&
		normalmapTrafo == o.normalmapTrafo &&
		occlusionmapTrafo == o.occlusionmapTrafo &&
		opacitymapTrafo == o.opacitymapTrafo &&
		roughnessmapTrafo == o.roughnessmapTrafo &&
		specularColor == o.specularColor &&
		specularmapTrafo == o.specularmapTrafo;
}

MStatus PRTMaterialNode::compute(const MPlug& plug, MDataBlock& block)
{
	MStatus status = MS::kSuccess;


	MObject thisNode = thisMObject();

	MPlug inMeshPlug(thisNode, aInMesh);
	MDataHandle inMeshHandle = block.inputValue(inMeshPlug, &status);
	MObject inMeshObj = inMeshHandle.asMesh();
	MFnMesh inputMesh(inMeshObj);
	MCHECK(status);

	// Create a copy of the mesh object. Rely on the underlying geometry
	// object to minimize the amount of duplication that will happen.
	MPlug outMeshPlug(thisNode, aOutMesh);
	if (plug != outMeshPlug)
		return status;

	MDataHandle outMeshHandle = block.outputValue(outMeshPlug, &status);
	MCHECK(status);
	outMeshHandle.set(inMeshObj);
	MObject outMeshObj = outMeshHandle.asMesh();
	MFnMesh outputMesh(outMeshObj);

	const adsk::Data::Associations* inputAssociations = inputMesh.metadata();

	MStatus stat;
	MString meshName;
	bool    meshFound = false;

	//find connected mesh node
	for (MPlug plug(thisNode, aOutMesh); !(meshFound); ) {
		MPlugArray plugs;
		if (plug.connectedTo(plugs, false, true, &stat)) {
			MCHECK(stat);
			for (unsigned int p = 0; p < plugs.length(); p++) {
				MFnDependencyNode node(plugs[p].node(), &stat);
				if (node.object().hasFn(MFn::kMesh)) {
					meshName = node.name();
					meshFound = true;
					break;
				}
				MCHECK(stat);
				for (int pOut = static_cast<int>(node.attributeCount()); --pOut >= 0;) {
					const MObject attr = node.attribute(pOut, &stat);
					MCHECK(stat);
					if (attr.apiType() == MFn::kGenericAttribute) {
						const MPlug oPlug(node.object(), attr);
						if (oPlug.isSource() && OUTPUT_GEOMETRY == oPlug.partialName()) {
							plug = oPlug;
							p = plugs.length();
							break;
						}
					}
				}
			}
		}
		else {
			return MS::kFailure;
		}
	}

	if (inputAssociations && meshFound)
	{
		adsk::Data::Associations outputAssociations(inputMesh.metadata(&status));
		MCHECK(status);

		//find all existing prt materials
		adsk::Data::Structure* fStructure = adsk::Data::Structure::structureByName(gPRTMatStructure.c_str());
		std::set<std::string> shaderNames;
		std::list<std::pair<const MObject, const MaterialInfo>> existingMaterialInfos;
		for (auto itHwShaders = MItDependencyNodes(MFn::kPluginHardwareShader); !itHwShaders.isDone(); itHwShaders.next()) {
			MObject obj = itHwShaders.thisNode();
			MFnDependencyNode n(obj);
			shaderNames.insert(std::string(n.name().asChar()));
			const adsk::Data::Associations* materialMetadata = n.metadata(&status);
			MCHECK(status);

			if (!materialMetadata) {
				continue;
			}

			adsk::Data::Associations materialAssociations(materialMetadata);
			adsk::Data::Channel* matChannel = materialAssociations.findChannel(gPRTMatChannel);

			if (!matChannel) {
				continue;
			}

			adsk::Data::Stream*	matStream = matChannel->findDataStream(gPRTMatStream);
			if (matStream && matStream->elementCount() == 1) {
				adsk::Data::Handle matSHandle = matStream->element(0);
				if (!matSHandle.usesStructure(*fStructure)) continue;
				auto p = std::pair<const MObject, const MaterialInfo>(obj, matSHandle);
				existingMaterialInfos.push_back(p);
			}
		}

		// determine path of shader fx file
		if (sfxFile.length() == 0) {
			// mel command wants forward slashes
			const std::wstring shadersPath = prtu::toGenericPath(mPRTCtx.mPluginRootPath + L"../shaders/");
			sfxFile = MString(shadersPath.c_str()) + "serlioShaderStingray.sfx";
			LOG_DBG << "stingray shader located at " << sfxFile.asWChar();
		}

		adsk::Data::Channel* channel = outputAssociations.findChannel(gPRTMatChannel);
		if (channel) {
			adsk::Data::Stream*	stream = channel->findDataStream(gPRTMatStream);
			if (stream) {


				MString mShadingCmd;
				mShadingCmd += "string $sgName;\n";
				mShadingCmd += "string $shName;\n";
				mShadingCmd += "string $colormap;\n";
				mShadingCmd += "string $nodeName;\n";
				mShadingCmd += "int $shadingNodeIndex;\n";

				std::array<wchar_t, 512> buf;

				for (adsk::Data::Handle& sHandle : *stream) {
					if (!sHandle.hasData()) continue;
					
					if (!sHandle.usesStructure(*fStructure)) continue;

					MaterialInfo matInfo(sHandle);

					//material with same metadata already exists?
					MObject matchingMaterial = MObject::kNullObj;

					for (const auto& kv : existingMaterialInfos) {
						if (matInfo.equals(kv.second)) {
							matchingMaterial = kv.first;
							break;
						}
					}

					sHandle.setPositionByMemberName(gPRTMatMemberFaceStart.c_str());
					int faceStart = sHandle.asInt32()[0];

					sHandle.setPositionByMemberName(gPRTMatMemberFaceEnd.c_str());
					int faceEnd = sHandle.asInt32()[0];

					if (matchingMaterial != MObject::kNullObj) {
						std::wstring matName(MFnDependencyNode(matchingMaterial).name().asWChar());
						size_t idx = matName.find_last_of(L"Sh");
						if (idx != std::wstring::npos)
							matName[idx] = L'g';
						swprintf(buf.data(), buf.size()-1, L"sets -forceElement %ls %ls.f[%d:%d];\n", matName.c_str(), MString(meshName).asWChar(), faceStart, faceEnd);
						mShadingCmd += buf.data();
						continue;
					}

					//get unique name
					std::string shaderName = "serlioGeneratedMaterialSh";
					std::string shadingGroupName = "serlioGeneratedMaterialSg";

					int shIdx = 0;
					while (shaderNames.find(shaderName + std::to_string(shIdx)) != shaderNames.end()) {
						shIdx++;
					}
					shaderName += std::to_string(shIdx);
					shadingGroupName += std::to_string(shIdx);
					shaderNames.insert(shaderName);

					MString shaderNameMstr = MString(shaderName.c_str());
					MString shadingGroupNameMstr = MString(shadingGroupName.c_str());

					MString shaderCmd = "shadingNode -asShader StingrayPBS -n " + shaderNameMstr + " -ss;\n";
					shaderCmd += "shaderfx -sfxnode \"" + shaderNameMstr + "\" -loadGraph  \"" + sfxFile + "\";\n";

					//create shadingnode and add metadata
					MCHECK(MGlobal::executeCommand(shaderCmd, DBG));
					MItDependencyNodes itHwShaders(MFn::kPluginHardwareShader);
					while (!itHwShaders.isDone())
					{
						MObject obj = itHwShaders.thisNode();
						MFnDependencyNode n(obj);

						if (n.name() == shaderNameMstr) {
							adsk::Data::Associations newMetadata;
							MCHECK(stat);
							adsk::Data::Channel newChannel = newMetadata.channel(gPRTMatChannel);
							adsk::Data::Stream newStream(*fStructure, gPRTMatStream);
							newChannel.setDataStream(newStream);
							newMetadata.setChannel(newChannel);
							adsk::Data::Handle handle(sHandle);
							handle.makeUnique();
							newStream.setElement(0, handle);
							n.setMetadata(newMetadata);
							break;
						}
						itHwShaders.next();
					}

					mShadingCmd += "$shName = \""+ shaderNameMstr + "\";\n";
					mShadingCmd += "$sgName = \""+ shadingGroupNameMstr +"\";\n";

					mShadingCmd += "sets -empty -renderable true -noSurfaceShader true -name $sgName;\n";
					mShadingCmd += "setAttr ($shName+\".initgraph\") true;\n";
					mShadingCmd += "connectAttr -force ($shName + \".outColor\") ($sgName + \".surfaceShader\");\n";

					MString blendMode = (matInfo.opacityMap.empty() && (matInfo.opacity >= 1.0)) ? "0": "1";
					mShadingCmd += "$shadingNodeIndex = `shaderfx -sfxnode $shName -getNodeIDByName \"Standard_Base\"`;\n";
					mShadingCmd += "shaderfx -sfxnode $shName -edit_stringlist $shadingNodeIndex blendmode "+ blendMode +";\n";

					//ignored: ambientColor, specularColor
					setAttribute(mShadingCmd, matInfo.diffuseColor, 3, "diffuse_color");
					setAttribute(mShadingCmd, matInfo.emissiveColor, 3, "emissive_color");
					setAttribute(mShadingCmd, matInfo.opacity, "opacity");
					setAttribute(mShadingCmd, matInfo.roughness, "roughness");
					setAttribute(mShadingCmd, matInfo.metallic, "metallic");

					//ignored: specularmapTrafo, bumpmapTrafo, occlusionmapTrafo
					//shaderfx does not support 5 values per input, that's why we split it up in tuv and swuv
					setAttribute(mShadingCmd, matInfo.colormapTrafo, 2, 3, "colormap_trafo_tuv");
					setAttribute(mShadingCmd, matInfo.dirtmapTrafo, 2, 3, "dirtmap_trafo_tuv");
					setAttribute(mShadingCmd, matInfo.emissivemapTrafo, 2, 3, "emissivemap_trafo_tuv");
					setAttribute(mShadingCmd, matInfo.metallicmapTrafo, 2, 3, "metallicmap_trafo_tuv");
					setAttribute(mShadingCmd, matInfo.normalmapTrafo, 2, 3, "normalmap_trafo_tuv");
					setAttribute(mShadingCmd, matInfo.opacitymapTrafo, 2, 3, "opacitymap_trafo_tuv");
					setAttribute(mShadingCmd, matInfo.roughnessmapTrafo, 2, 3, "roughnessmap_trafo_tuv");

					setAttribute(mShadingCmd, matInfo.colormapTrafo, 3, 0, "colormap_trafo_suvw");
					setAttribute(mShadingCmd, matInfo.dirtmapTrafo, 3, 0, "dirtmap_trafo_suvw");
					setAttribute(mShadingCmd, matInfo.emissivemapTrafo, 3, 0, "emissivemap_trafo_suvw");
					setAttribute(mShadingCmd, matInfo.metallicmapTrafo, 3, 0, "metallicmap_trafo_suvw");
					setAttribute(mShadingCmd, matInfo.normalmapTrafo, 3, 0, "normalmap_trafo_suvw");
					setAttribute(mShadingCmd, matInfo.opacitymapTrafo, 3, 0, "opacitymap_trafo_suvw");
					setAttribute(mShadingCmd, matInfo.roughnessmapTrafo, 3, 0, "roughnessmap_trafo_suvw");

					//ignored: bumpMap, specularMap, occlusionmap
					setTexture(mShadingCmd, matInfo.colormap, "color_map");
					setTexture(mShadingCmd, matInfo.dirtmap, "dirt_map");
					setTexture(mShadingCmd, matInfo.emissiveMap, "emissive_map");
					setTexture(mShadingCmd, matInfo.metallicMap, "metallic_map");
					setTexture(mShadingCmd, matInfo.normalMap, "normal_map");
					setTexture(mShadingCmd, matInfo.roughnessMap, "roughness_map");
					setTexture(mShadingCmd, matInfo.opacityMap, "opacity_map");

					swprintf(buf.data(), buf.size()-1, L"sets -forceElement $sgName %ls.f[%d:%d];\n", MString(meshName).asWChar(), faceStart, faceEnd);
					mShadingCmd += buf.data();
				}

				MCHECK(MGlobal::executeCommandOnIdle(mShadingCmd, DBG));

			}
		}

		outputMesh.setMetadata(outputAssociations);
	}
	outMeshHandle.setClean();
	return status;
}

void PRTMaterialNode::setAttribute(MString& mShadingCmd, const std::vector<double>& vec, size_t elements, const std::string& target)
{
	if (vec.size() >= elements) {
		MString colString = MaterialInfo::toMString(vec, elements, 0);
		mShadingCmd += "setAttr ($shName+\"."+ MString(target.c_str()) +"\") -type double"+ MString(std::to_string(elements).c_str()) + " " + colString + ";\n";
	}
}

void PRTMaterialNode::setAttribute(MString& mShadingCmd, const std::vector<double>& vec, size_t elements, size_t offset, const std::string& target)
{
	if (vec.size()+offset >= elements) {
		MString colString = MaterialInfo::toMString(vec, elements,offset);
		mShadingCmd += "setAttr ($shName+\"." + MString(target.c_str()) + "\") -type double" + MString(std::to_string(elements).c_str()) + " " + colString + ";\n";
	}
}

void PRTMaterialNode::setAttribute(MString& mShadingCmd, double vec, const std::string& target)
{
	mShadingCmd += "setAttr ($shName+\"." + MString(target.c_str()) + "\") " + MString(std::to_string(vec).c_str()) + ";\n";
}

void PRTMaterialNode::setTexture(MString& mShadingCmd, const std::string& tex, const std::string& target)
{
	if (tex.size() > 0) {
		mShadingCmd += "$colormap = \"" + MString(tex.c_str()) + "\";\n";
		mShadingCmd += "$nodeName = $sgName +\"" + MString(target.c_str()) + "\";\n";
		mShadingCmd += "shadingNode -asTexture file -n $nodeName -ss;\n";
		mShadingCmd += R"foo(setAttr($nodeName + ".fileTextureName") -type "string" $colormap ;)foo" "\n";

		mShadingCmd += R"foo(connectAttr -force ($nodeName + ".outColor") ($shName + ".TEX_)foo" + MString(target.c_str()) +"\");\n";
		mShadingCmd += "setAttr ($shName+\".use_"+ MString(target.c_str())+"\") 1;\n";
	}
	else {
		mShadingCmd += "setAttr ($shName+\".use_" + MString(target.c_str()) + "\") 0;\n";
	}
}

