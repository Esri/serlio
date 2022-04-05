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

#include "utils/MayaUtilities.h"

#include "maya/MGlobal.h"

#include <array>
#include <cassert>
#include <sstream>
#include <string>

class MaterialColor;

class MELVariable : public mu::NamedType<std::wstring, MELVariable> {
public:
	using mu::NamedType<std::wstring, MELVariable>::NamedType;
	std::wstring mel() const {
		assert(!get().empty() && get()[0] != L'$');
		return L'$' + get();
	}
};

class MELStringLiteral : public mu::NamedType<std::wstring, MELStringLiteral> {
public:
	using mu::NamedType<std::wstring, MELStringLiteral>::NamedType;
	std::wstring mel() const {
		return L"\"" + get() + L"\"";
	}
};

class MELScriptBuilder {
public:
	void setAttr(const MELVariable& node, const std::wstring& attribute, bool val);
	void setAttr(const MELVariable& node, const std::wstring& attribute, int val);
	void setAttr(const MELVariable& node, const std::wstring& attribute, double val);
	void setAttr(const MELVariable& node, const std::wstring& attribute, double val1, double val2);
	void setAttr(const MELVariable& node, const std::wstring& attribute, const std::array<double, 2>& val);
	void setAttr(const MELVariable& node, const std::wstring& attribute, double val1, double val2, double val3);
	void setAttr(const MELVariable& node, const std::wstring& attribute, const std::array<double, 3>& val);
	void setAttr(const MELVariable& node, const std::wstring& attribute, const MELVariable& val);
	void setAttr(const MELVariable& node, const std::wstring& attribute, const MELStringLiteral& val);
	void setAttr(const MELVariable& node, const std::wstring& attribute, const MaterialColor& color);

	void setAttr(const MELVariable& node, const std::wstring& attribute, const wchar_t* val) = delete;
	void setAttr(const MELVariable& node, const std::wstring& attribute, const char* val) = delete;

	void setAttrEnumOptions(const MELVariable& node, const std::wstring& attribute,
	                        const std::vector<std::wstring>& enumOptions,
	                        const std::optional<std::wstring>& customDefaultOption);

	void connectAttr(const MELVariable& srcNode, const std::wstring& srcAttr, const MELVariable& dstNode,
	                 const std::wstring& dstAttr);

	void declInt(const MELVariable& varName);
	void declString(const MELVariable& varName);

	void setVar(const MELVariable& varName, const MELStringLiteral& val);

	void setsCreate(const MELVariable& setName);
	void setsAddFaceRange(const std::wstring& setName, const std::wstring& meshName, int faceStart, int faceEnd);
	void setsUseInitialShadingGroup(const std::wstring& meshName);

	void createShader(const std::wstring& shaderType, const MELVariable& nodeName);
	void createTextureShadingNode(const MELVariable& nodeName);
	void forceValidTextureAlphaChannel(const MELVariable& nodeName);

	void getUndoState(const MELVariable& undoName);
	void setUndoState(const MELVariable& undoName);
	void setUndoState(bool undoState);

	void getWorkspaceDir();

	void python(const std::wstring& pythonCmd);
	void addCmdLine(const std::wstring& line);

	MStatus executeSync(std::wstring& output);
	MStatus execute();

private:
	std::wstringstream commandStream;
};
