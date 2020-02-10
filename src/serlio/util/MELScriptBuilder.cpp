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

#include <iomanip>

namespace {

constexpr bool MEL_ENABLE_DISPLAY = false;

std::wstring composeAttributeExpression(const MELVariable& node, const std::wstring& attribute) {
	assert(!attribute.empty() && attribute[0] != L'.'); // to catch refactoring bugs
	std::wostringstream out;
	out << "(" << node.mel() << " + " << std::quoted(L'.' + attribute) << ")";
	return out.str();
}

} // namespace

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute, const bool val) {
	commandStream << "setAttr " << composeAttributeExpression(node, attribute) << " " << (val ? 1 : 0) << ";\n";
}

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute, const int val) {
	commandStream << "setAttr " << composeAttributeExpression(node, attribute) << " " << val << ";\n";
}

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute, const double val) {
	commandStream << "setAttr " << composeAttributeExpression(node, attribute) << " " << val << ";\n";
}

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute, const double val1,
                               const double val2) {
	commandStream << "setAttr -type double2 " << composeAttributeExpression(node, attribute) << " " << val1 << " "
	              << val2 << ";\n";
}

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute,
                               const std::array<double, 2>& val) {
	setAttr(node, attribute, val[0], val[1]);
}

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute, const double val1,
                               const double val2, const double val3) {
	commandStream << "setAttr -type double3 " << composeAttributeExpression(node, attribute) << " " << val1 << " "
	              << val2 << " " << val3 << ";\n";
}

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute,
                               const std::array<double, 3>& val) {
	setAttr(node, attribute, val[0], val[1], val[2]);
}

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute, const wchar_t* val) {
	setAttr(node, attribute, std::wstring(val));
}

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute, const std::wstring& val) {
	commandStream << "setAttr -type \"string\" " << composeAttributeExpression(node, attribute) << " " << val << ";\n";
}

void MELScriptBuilder::setAttr(const MELVariable& node, const std::wstring& attribute, const MaterialColor& color) {
	setAttr(node, attribute, color.r(), color.g(), color.b());
}

void MELScriptBuilder::connectAttr(const std::wstring& source, const std::wstring& dest) {
	commandStream << "connectAttr -force " << source << " " << dest << ";\n";
}

void MELScriptBuilder::python(const std::wstring& pythonCmd) {
	commandStream << "python(\"" << pythonCmd << "\");\n";
}

void MELScriptBuilder::declInt(const MELVariable& varName) {
	commandStream << "int " << varName.mel() << ";\n";
}

void MELScriptBuilder::declString(const MELVariable& varName) {
	commandStream << "string " << varName.mel() << ";\n";
}

void MELScriptBuilder::setVar(const MELVariable& varName, const std::wstring& val) {
	commandStream << varName.mel() << " = \"" << val << "\";\n";
}

void MELScriptBuilder::setsCreate(const std::wstring& setName) {
	commandStream << "sets -empty -renderable true -noSurfaceShader true -name " << setName << ";\n";
}

void MELScriptBuilder::setsAddFaceRange(const std::wstring& setName, const std::wstring& meshName, const int faceStart,
                                        const int faceEnd) {
	commandStream << "sets -forceElement " << setName << " " << meshName << ".f[" << faceStart << ":" << faceEnd
	              << "];\n";
}

void MELScriptBuilder::createShader(const std::wstring& shaderType, const MELVariable& nodeName) {
	const auto mel = nodeName.mel();
	commandStream << mel << " = `shadingNode -asShader -skipSelect -name " << mel << " " << shaderType << "`;\n";
}

void MELScriptBuilder::createTextureShadingNode(const MELVariable& nodeName) {
	const auto mel = nodeName.mel();
	commandStream << mel << "= `shadingNode -asTexture -skipSelect -name " << mel << " file`;\n";
}

void MELScriptBuilder::addCmdLine(const std::wstring& line) {
	commandStream << line << L"\n";
}

MStatus MELScriptBuilder::executeSync(std::wstring& output) {
	MStatus status;
	MString result =
	        MGlobal::executeCommandStringResult(commandStream.str().c_str(), MEL_ENABLE_DISPLAY, false, &status);
	commandStream.clear();
	output.assign(result.asWChar());
	return status;
}

MStatus MELScriptBuilder::execute() {
	MStatus status = MGlobal::executeCommandOnIdle(commandStream.str().c_str(), MEL_ENABLE_DISPLAY);
	commandStream.clear();
	return status;
}
