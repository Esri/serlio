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

#include "prtMaterial/MaterialInfo.h"

#include "prtModifier/PRTModifierAction.h"

#include "util/MArrayWrapper.h"
#include "util/MItDependencyNodesWrapper.h"
#include "util/MayaUtilities.h"
#include "util/Utilities.h"

#include "serlioPlugin.h"

#include "prt/StringUtils.h"

#include "maya/MFnMesh.h"
#include "maya/MFnTypedAttribute.h"
#include "maya/MGlobal.h"
#include "maya/MItDependencyNodes.h"
#include "maya/MObject.h"
#include "maya/adskDataAssociations.h"
#include "maya/adskDataStream.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <set>
#include <sstream>

namespace {

constexpr bool DBG = false;

} // namespace

MTypeId PRTMaterialNode::id(SerlioNodeIDs::SERLIO_PREFIX, SerlioNodeIDs::STRINGRAY_MATERIAL_NODE);

MObject PRTMaterialNode::aInMesh;
MObject PRTMaterialNode::aOutMesh;
MString PRTMaterialNode::sfxFile;
const MString OUTPUT_GEOMETRY = MString("og");

MStatus PRTMaterialNode::initialize() {
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

MStatus PRTMaterialNode::compute(const MPlug& plug, MDataBlock& block) {
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
	bool meshFound = false;
	bool searchEnded = false;

	for (MPlug curPlug = plug; !searchEnded;) {
		searchEnded = true;
		MPlugArray connectedPlugs;
		curPlug.connectedTo(connectedPlugs, false, true, &status);
		MCHECK(status);
		if (!connectedPlugs.length()) {
			return MStatus::kFailure;
		}
		for (const auto& connectedPlug : mu::makeMArrayConstWrapper(connectedPlugs)) {
			MFnDependencyNode connectedDepNode(connectedPlug.node(), &status);
			MCHECK(status);
			MObject connectedDepNodeObj = connectedDepNode.object(&status);
			MCHECK(status);
			if (connectedDepNodeObj.hasFn(MFn::kMesh)) {
				meshName = connectedDepNode.name(&status);
				MCHECK(status);
				meshFound = true;
				break;
			}
			if (connectedDepNodeObj.hasFn(MFn::kGroupParts)) {
				curPlug = connectedDepNode.findPlug("outputGeometry", true, &status);
				MCHECK(status);
				searchEnded = false;
				break;
			}
		}
	}

	if (!meshFound) {
		return MStatus::kSuccess;
	}

	if ((inputAssociations != nullptr) && meshFound) {
		adsk::Data::Associations outputAssociations(inputMesh.metadata(&status));
		MCHECK(status);

		// find all existing prt materials
		adsk::Data::Structure* fStructure = adsk::Data::Structure::structureByName(gPRTMatStructure.c_str());
		std::set<std::string> shaderNames;
		std::list<std::pair<const MObject, const MaterialInfo>> existingMaterialInfos;
		MItDependencyNodes itHwShaders(MFn::kPluginHardwareShader, &status);
		MCHECK(status);
		for (const auto& hwShaderNode : MItDependencyNodesWrapper(itHwShaders)) {
			MFnDependencyNode n(hwShaderNode);
			shaderNames.insert(std::string(n.name().asChar()));
			const adsk::Data::Associations* materialMetadata = n.metadata(&status);
			MCHECK(status);

			if (materialMetadata == nullptr) {
				continue;
			}

			adsk::Data::Associations materialAssociations(materialMetadata);
			adsk::Data::Channel* matChannel = materialAssociations.findChannel(gPRTMatChannel);

			if (matChannel == nullptr) {
				continue;
			}

			adsk::Data::Stream* matStream = matChannel->findDataStream(gPRTMatStream);
			if ((matStream != nullptr) && matStream->elementCount() == 1) {
				adsk::Data::Handle matSHandle = matStream->element(0);
				if (!matSHandle.usesStructure(*fStructure))
					continue;
				auto p = std::pair<const MObject, const MaterialInfo>(hwShaderNode, matSHandle);
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
		if (channel != nullptr) {
			adsk::Data::Stream* stream = channel->findDataStream(gPRTMatStream);
			if (stream != nullptr) {

				MString mShadingCmd;
				mShadingCmd += "string $sgName;\n";
				mShadingCmd += "string $shName;\n";
				mShadingCmd += "string $colormap;\n";
				mShadingCmd += "string $nodeName;\n";
				mShadingCmd += "int $shadingNodeIndex;\n";

				std::array<wchar_t, 512> buf;

				for (adsk::Data::Handle& sHandle : *stream) {
					if (!sHandle.hasData())
						continue;

					if (!sHandle.usesStructure(*fStructure))
						continue;

					MaterialInfo matInfo(sHandle);

					// material with same metadata already exists?
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
						swprintf(buf.data(), buf.size() - 1, L"sets -forceElement %ls %ls.f[%d:%d];\n", matName.c_str(),
						         MString(meshName).asWChar(), faceStart, faceEnd);
						mShadingCmd += buf.data();
						continue;
					}

					// get unique name
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

					// create shadingnode and add metadata
					MCHECK(MGlobal::executeCommand(shaderCmd, DBG));
					MItDependencyNodes itHwShaders(MFn::kPluginHardwareShader, &status);
					MCHECK(status);
					for (const auto& hwShaderNode : MItDependencyNodesWrapper(itHwShaders)) {
						MFnDependencyNode n(hwShaderNode);

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
					}

					mShadingCmd += "$shName = \"" + shaderNameMstr + "\";\n";
					mShadingCmd += "$sgName = \"" + shadingGroupNameMstr + "\";\n";

					mShadingCmd += "sets -empty -renderable true -noSurfaceShader true -name $sgName;\n";
					mShadingCmd += "setAttr ($shName+\".initgraph\") true;\n";
					mShadingCmd += "connectAttr -force ($shName + \".outColor\") ($sgName + \".surfaceShader\");\n";

					MString blendMode = (matInfo.opacityMap.empty() && (matInfo.opacity >= 1.0)) ? "0" : "1";
					mShadingCmd +=
					        "$shadingNodeIndex = `shaderfx -sfxnode $shName -getNodeIDByName \"Standard_Base\"`;\n";
					mShadingCmd += "shaderfx -sfxnode $shName -edit_stringlist $shadingNodeIndex blendmode " +
					               blendMode + ";\n";

					// ignored: ambientColor, specularColor
					setAttribute(mShadingCmd, "diffuse_color", matInfo.diffuseColor);
					setAttribute(mShadingCmd, "emissive_color", matInfo.emissiveColor);
					setAttribute(mShadingCmd, "opacity", matInfo.opacity);
					setAttribute(mShadingCmd, "roughness", matInfo.roughness);
					setAttribute(mShadingCmd, "metallic", matInfo.metallic);

					// ignored: specularmapTrafo, bumpmapTrafo, occlusionmapTrafo
					// shaderfx does not support 5 values per input, that's why we split it up in tuv and swuv
					setAttribute(mShadingCmd, "colormap_trafo_tuv", matInfo.colormapTrafo.tuv());
					setAttribute(mShadingCmd, "dirtmap_trafo_tuv", matInfo.dirtmapTrafo.tuv());
					setAttribute(mShadingCmd, "emissivemap_trafo_tuv", matInfo.emissivemapTrafo.tuv());
					setAttribute(mShadingCmd, "metallicmap_trafo_tuv", matInfo.metallicmapTrafo.tuv());
					setAttribute(mShadingCmd, "normalmap_trafo_tuv", matInfo.normalmapTrafo.tuv());
					setAttribute(mShadingCmd, "opacitymap_trafo_tuv", matInfo.opacitymapTrafo.tuv());
					setAttribute(mShadingCmd, "roughnessmap_trafo_tuv", matInfo.roughnessmapTrafo.tuv());

					setAttribute(mShadingCmd, "colormap_trafo_suvw", matInfo.colormapTrafo.suvw());
					setAttribute(mShadingCmd, "dirtmap_trafo_suvw", matInfo.dirtmapTrafo.suvw());
					setAttribute(mShadingCmd, "emissivemap_trafo_suvw", matInfo.emissivemapTrafo.suvw());
					setAttribute(mShadingCmd, "metallicmap_trafo_suvw", matInfo.metallicmapTrafo.suvw());
					setAttribute(mShadingCmd, "normalmap_trafo_suvw", matInfo.normalmapTrafo.suvw());
					setAttribute(mShadingCmd, "opacitymap_trafo_suvw", matInfo.opacitymapTrafo.suvw());
					setAttribute(mShadingCmd, "roughnessmap_trafo_suvw", matInfo.roughnessmapTrafo.suvw());

					// ignored: bumpMap, specularMap, occlusionmap
					setTexture(mShadingCmd, "color_map", matInfo.colormap);
					setTexture(mShadingCmd, "dirt_map", matInfo.dirtmap);
					setTexture(mShadingCmd, "emissive_map", matInfo.emissiveMap);
					setTexture(mShadingCmd, "metallic_map", matInfo.metallicMap);
					setTexture(mShadingCmd, "normal_map", matInfo.normalMap);
					setTexture(mShadingCmd, "roughness_map", matInfo.roughnessMap);
					setTexture(mShadingCmd, "opacity_map", matInfo.opacityMap);

					swprintf(buf.data(), buf.size() - 1, L"sets -forceElement $sgName %ls.f[%d:%d];\n",
					         MString(meshName).asWChar(), faceStart, faceEnd);
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

void PRTMaterialNode::setAttribute(MString& mShadingCmd, const std::string& target, const double val) {
	mShadingCmd += ("setAttr ($shName + \"." + target + "\") " + std::to_string(val) + ";\n").c_str();
}

void PRTMaterialNode::setAttribute(MString& mShadingCmd, const std::string& target, const double val1,
                                   double const val2) {
	mShadingCmd += ("setAttr ($shName + \"." + target + "\") -type double2 " + std::to_string(val1) + " " +
	                std::to_string(val2) + ";\n")
	                       .c_str();
}

void PRTMaterialNode::setAttribute(MString& mShadingCmd, const std::string& target, const double val1,
                                   double const val2, double const val3) {
	mShadingCmd += ("setAttr ($shName + \"." + target + "\") -type double3 " + std::to_string(val1) + " " +
	                std::to_string(val2) + " " + std::to_string(val3) + ";\n")
	                       .c_str();
}

void PRTMaterialNode::setAttribute(MString& mShadingCmd, const std::string& target, const std::array<double, 2>& val) {
	setAttribute(mShadingCmd, target, val[0], val[1]);
}

void PRTMaterialNode::setAttribute(MString& mShadingCmd, const std::string& target, const std::array<double, 3>& val) {
	setAttribute(mShadingCmd, target, val[0], val[1], val[2]);
}

void PRTMaterialNode::setAttribute(MString& mShadingCmd, const std::string& target, const MaterialColor& color) {
	setAttribute(mShadingCmd, target, color.r(), color.g(), color.b());
}

void PRTMaterialNode::setTexture(MString& mShadingCmd, const std::string& target, const std::string& tex) {
	if (tex.size() > 0) {
		mShadingCmd += "$colormap = \"" + MString(tex.c_str()) + "\";\n";
		mShadingCmd += "$nodeName = $sgName +\"" + MString(target.c_str()) + "\";\n";
		mShadingCmd += "shadingNode -asTexture file -n $nodeName -ss;\n";
		mShadingCmd += R"foo(setAttr($nodeName + ".fileTextureName") -type "string" $colormap ;)foo"
		               "\n";

		mShadingCmd += R"foo(connectAttr -force ($nodeName + ".outColor") ($shName + ".TEX_)foo" +
		               MString(target.c_str()) + "\");\n";
		mShadingCmd += "setAttr ($shName+\".use_" + MString(target.c_str()) + "\") 1;\n";
	}
	else {
		mShadingCmd += "setAttr ($shName+\".use_" + MString(target.c_str()) + "\") 0;\n";
	}
}
