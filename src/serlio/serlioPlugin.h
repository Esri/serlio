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

#include <cstdint>
#include <vector>
#include <string>

#ifdef _WIN32
#	ifdef SRL_TEST_EXPORTS
#		define SRL_TEST_EXPORTS_API __declspec(dllexport)
#	else
#		define SRL_TEST_EXPORTS_API
#	endif
#else
#	define SRL_TEST_EXPORTS_API __attribute__((visibility("default")))
#endif

namespace SerlioNodeIDs {

// our registered node id range is: 0x00132980 - 0x001329bf
constexpr std::uint32_t SERLIO_PREFIX = 0x00132980;
constexpr std::uint32_t PRT_GEOMETRY_NODE = 0x5;
constexpr std::uint32_t STRINGRAY_MATERIAL_NODE = 0xA;
constexpr std::uint32_t ARNOLD_MATERIAL_NODE = 0xF;

} // namespace SerlioNodeIDs

namespace MayaPluginUtilities {

// defined here because of limitations of including MFnPlugin.h multiple times
bool pluginDependencyCheck(const std::vector<std::string>& dependencies);

} // namespace MayaPluginUtilities