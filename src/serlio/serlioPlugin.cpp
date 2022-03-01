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

#include "serlioPlugin.h"
#include "PRTContext.h"

#include "modifiers/PRTModifierCommand.h"
#include "modifiers/PRTModifierNode.h"

#include "materials/ArnoldMaterialNode.h"
#include "materials/StingrayMaterialNode.h"
#include "materials/MaterialCommand.h"

#include "utils/MayaUtilities.h"

#include "maya/MFnPlugin.h"
#include "maya/MGlobal.h"
#include "maya/MSceneMessage.h"
#include "maya/MStatus.h"
#include "maya/MString.h"

#include <mutex>

namespace {
constexpr bool DBG = false;

constexpr const char* NODE_MODIFIER = "serlio";
constexpr const char* NODE_MATERIAL = "serlioMaterial";
constexpr const char* NODE_ARNOLD_MATERIAL = "serlioArnoldMaterial";
constexpr const char* CMD_CREATE_MATERIAL = "serlioCreateMaterial";
constexpr const char* CMD_ASSIGN = "serlioAssign";
constexpr const char* MEL_PROC_CREATE_UI = "serlioCreateUI";
constexpr const char* MEL_PROC_DELETE_UI = "serlioDeleteUI";
constexpr const char* SERLIO_VENDOR = "Esri R&D Center Zurich";

std::once_flag callbackRegisterFlag;

} // namespace

// called when the plug-in is loaded into Maya.
MStatus initializePlugin(MObject obj) {
	if (!PRTContext::get().isAlive()) // this will implicitly load PRT
		return MS::kFailure;

	// maya exit does not call uninitializePlugin automatically, therefore use addCallback
	// we only do this once in case the serlio plugin is unload and loaded again
	// (which is not recommended but allowed by the Maya UI)
	std::call_once(callbackRegisterFlag, []() {
		auto mayaExitCallback = [](void*) { uninitializePlugin(MObject::kNullObj); }; // TODO: correct obj value?
		MStatus mayaStatus = MStatus::kFailure;
		MSceneMessage::addCallback(MSceneMessage::kMayaExiting, mayaExitCallback, nullptr, &mayaStatus);
		MCHECK(mayaStatus);
	});

	MFnPlugin plugin(obj, SERLIO_VENDOR, SRL_VERSION);

	auto createModifierCommand = []() { return (void*)new PRTModifierCommand(); };
	MCHECK(plugin.registerCommand(CMD_ASSIGN, createModifierCommand));

	auto createMaterialCommand = []() { return (void*)new MaterialCommand(); };
	MCHECK(plugin.registerCommand(CMD_CREATE_MATERIAL, createMaterialCommand));

	auto createModifierNode = []() { return (void*)new PRTModifierNode(); };
	MCHECK(plugin.registerNode(NODE_MODIFIER, PRTModifierNode::id, createModifierNode, PRTModifierNode::initialize));

	auto createMaterialNode = []() { return (void*)new StingrayMaterialNode(); };
	MCHECK(plugin.registerNode(NODE_MATERIAL, StingrayMaterialNode::id, createMaterialNode,
	                           &StingrayMaterialNode::initialize));

	auto createArnoldMaterialNode = []() { return (void*)new ArnoldMaterialNode(); };
	MCHECK(plugin.registerNode(NODE_ARNOLD_MATERIAL, ArnoldMaterialNode::id, createArnoldMaterialNode,
	                           &ArnoldMaterialNode::initialize));

	MCHECK(plugin.registerUI(MEL_PROC_CREATE_UI, MEL_PROC_DELETE_UI));

	MCHECK(plugin.registerUIStrings(mu::registerMStringResources, ""));

	return MStatus::kSuccess;
}

// called when the plug-in is unloaded from Maya.
MStatus uninitializePlugin(MObject obj) {
	// note: we do not shutdown PRT here because:
	// * maya may unload/load serlio
	// * PRT only supports initializing once per process life time

	MStatus status;
	if (obj != MObject::kNullObj) { // TODO
		MFnPlugin plugin(obj);
		MCHECK(plugin.deregisterCommand(CMD_ASSIGN));
		MCHECK(plugin.deregisterNode(PRTModifierNode::id));
		MCHECK(plugin.deregisterNode(StingrayMaterialNode::id));
		MCHECK(plugin.deregisterNode(ArnoldMaterialNode::id));
	}
	return status;
}

namespace MayaPluginUtilities {

bool pluginDependencyCheck(const std::vector<std::string>& dependencies) {
	for (const auto& d : dependencies) {
		MString md(d.c_str());
		auto p = MFnPlugin::findPlugin(md);
		if (p.isNull()) {
			MGlobal::displayError("Serlio: the required dependency '" + md +
			                      "' is not loaded, please activate it and restart Maya!");
			return false;
		}
	}
	return true;
}

} // namespace MayaPluginUtilities