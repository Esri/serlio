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

#include "util/MELScriptBuilder.h"

#include "prtMaterial/MaterialInfo.h"

#include "util/MayaUtilities.h"

namespace {
constexpr bool MEL_ENABLE_DISPLAY = false;
} // namespace

void MELScriptBuilder::setAttr(const std::wstring& attribute, const bool val) {
	commandStream << "setAttr " << attribute << " " << (val ? 1 : 0) << ";\n";
}

void MELScriptBuilder::setAttr(const std::wstring& attribute, const double val) {
	commandStream << "setAttr " << attribute << " " << val << ";\n";
}

void MELScriptBuilder::setAttr(const std::wstring& attribute, const double val1, const double val2) {
	commandStream << "setAttr -type double2 " << attribute << " " << val1 << " " << val2 << ";\n";
}

void MELScriptBuilder::setAttr(const std::wstring& attribute, const double val1, const double val2, const double val3) {
	commandStream << "setAttr -type double3 " << attribute << " " << val1 << " " << val2 << " " << val3 << ";\n";
}

void MELScriptBuilder::setAttr(const std::wstring& attribute, const wchar_t* val) {
	setAttr(attribute, std::wstring(val));
}

void MELScriptBuilder::setAttr(const std::wstring& attribute, const std::wstring& val) {
	commandStream << "setAttr -type \"string\" " << attribute << " " << val << ";\n";
}

void MELScriptBuilder::setAttr(const std::wstring& attribute, const MaterialColor& color) {
	setAttr(attribute, color.r(), color.g(), color.b());
}

void MELScriptBuilder::connectAttr(const std::wstring& source, const std::wstring& dest) {
	commandStream << "connectAttr -force " << source << " " << dest << ";\n";
}

void MELScriptBuilder::python(const std::wstring& pythonCmd) {
	commandStream << "python(\"" << pythonCmd << "\");\n";
}

void MELScriptBuilder::declString(const std::wstring& varName) {
	commandStream << "string " << varName << ";\n";
}

void MELScriptBuilder::setVar(const std::wstring& varName, const std::wstring& val) {
	commandStream << varName << " = \"" << val << "\";\n";
}

void MELScriptBuilder::setsCreate(const std::wstring& setName) {
	commandStream << "sets -empty -renderable true -noSurfaceShader true -name " << setName << ";\n";
}

void MELScriptBuilder::setsAddFaceRange(const std::wstring& setName, const std::wstring& meshName, const int faceStart,
                                        const int faceEnd) {
	commandStream << "sets -forceElement " << setName << " " << meshName << ".f[" << faceStart << ":" << faceEnd
	              << "];\n";
}

void MELScriptBuilder::createShader(const std::wstring& shaderType, const std::wstring& shaderName) {
	commandStream << shaderName << " = `shadingNode -asShader -skipSelect -name " << shaderName << " " << shaderType
	              << "`;\n";
}

void MELScriptBuilder::createTexture(const std::wstring& textureName) {
	commandStream << "shadingNode -asTexture -skipSelect -name " << textureName << " file;\n";
}

std::wstring MELScriptBuilder::executeSync() {
	MStatus status;
	MString result =
	        MGlobal::executeCommandStringResult(commandStream.str().c_str(), MEL_ENABLE_DISPLAY, false, &status);
	MCHECK(status);
	commandStream.clear();
	return result.asWChar();
}

void MELScriptBuilder::execute() {
	MCHECK(MGlobal::executeCommandOnIdle(commandStream.str().c_str(), MEL_ENABLE_DISPLAY));
	commandStream.clear();
}
