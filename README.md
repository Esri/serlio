# ![](doc/img/serlio_32.png)  Serlio - CityEngine Plugin for Autodesk Maya

Serlio is a plugin for [Autodesk Maya](https://www.autodesk.com/maya). It provides a modifier node which enables the execution of [CityEngine](https://www.esri.com/software/cityengine) ‘rules’ within a Maya scene. Therefore, a 3D environment artist does not have to leave their familiar Maya toolset anymore to make use of CityEngine’s procedural modeling power. Complicated export-import pipelines are no longer needed, which also means that the procedural building models do not need to be “baked” anymore. The buildings stay procedural during the entire modeling workflow. Consequently, the 3D environment artist can change the height, style and appearance of buildings easily with a parametric interface at any point during production.

Serlio requires so-called rule packages (RPK) as input, which are authored in CityEngine. An RPK includes assets and a CGA rule file which encodes an architectural style. Comprehensive RPK examples are available below and can be used “out-of-the-box” in Serlio.

Serlio is well suited for managing the procedural generation of architectural 3D content in digital sets. However, Serlio is restricted to the procedural generation of single buildings / objects. Serlio does not include the city layouting and street network editing tools of CityEngine i.e. the rich CityEngine toolset to design a city from scratch (or based on geographic data) is still needed.


## Quick Start
1. Use CityEngine to [export a Rule Package](https://doc.arcgis.com/en/cityengine/latest/help/help-rule-package.htm) (RPK).
1. In Maya, select a mesh and use the `Serlio` menu to assign the RPK. This will run the rules for each face in the mesh.
1. Use the Hypergraph to navigate to the "serlio" node where you can edit the rule parameters.
1. To create materials, apply one of the two commands in the serlio menu on the generated model. Please note the the two material systems are mutually exclusive at this point.

1. Acquire the Serlio binaries, either: 
    * ... download the [desired release](https://github.com/Esri/serlio/releases)
    (on linux you still need to manually [install the local build](#linux-2))
    * ... or [build](#build-instructions) them yourself, which will result in an `install` directory
1. Start Maya (Note: you may want to start maya from a shell to see the serlio log output).
1. Open the plugin manager: Windows -> Settings/Preferences -> Plug-in Manager
1. Enable `serlio.so` / `serlio.mll`
1. The plugin should load and a new menu item `Serlio` should appear in Maya.

## Table of Contents

* [User Manual](#user-manual)
* [Examples](https://esri.github.io/cityengine/serlio)
* [Developer Manual](#developer-manual)
* [Release Notes](#release-notes)
* [Known Limitations](#known-limitations)
* [Licensing information](#licensing-information)

External documentation:
* [CityEngine Tutorials](https://doc.arcgis.com/en/cityengine/latest/tutorials/introduction-to-the-cityengine-tutorials.htm)
* [CityEngine CGA Reference](https://doc.arcgis.com/en/cityengine/latest/cga/cityengine-cga-introduction.htm)
* [CityEngine Manual](https://doc.arcgis.com/en/cityengine/latest/help/cityengine-help-intro.htm)
* [CityEngine SDK API Reference](https://esri.github.io/cityengine-sdk/html/index.html)

## User Manual

### Installation

#### Windows: Provided Binaries
1. Download the MSI installer for your Maya version from the [desired release](https://github.com/Esri/serlio/releases)
1. Run the MSI installer
1. Start Maya
1. You should now see the new menu item "Serlio".

#### Windows: Install local build
See [below](#windows-2) in the developer documentation

#### Linux: Provided Binaries
1. Acquire the Serlio binaries by downloading the [desired release](https://github.com/Esri/serlio/releases)
2. Proceed like [below](#linux-2) in the developer documentation

#### Linux: Install local build
See [below](#linux-2) in the developer documentation

### Usage Instructions

## Developer Manual

### Software Requirements

#### All Platforms
* License for CityEngine (2019.0 or later), e.g. to author Rule Packages.
* CMake 3.13 or later (http://www.cmake.org)
* Autodesk Maya 2019 or later or the corresponding development kit

#### Windows
* Windows 7, 8.1 or 10 (64bit)
* Required C++ compiler: Visual Studio 2019 with Toolset MSVC 14.27 or later
* WiX Toolset 3.11.1: Optional, required for building .msi installers
* Required flags for extension libraries release mode: `/bigobj /GR /EHsc /MD`

#### Linux
* RedHat Enterprise Linux 7.x or compatible
* Required C++ compiler: GCC 9.3 or later (RedHat Enterprise Linux DevToolSet 9.1)
* Required flags for extension libraries: `-std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 -march=nocona -fvisibility=hidden -fvisibility-inlines-hidden -Wl,--exclude-libs,ALL`

### Build Instructions 

#### Windows

##### Quick build for the impatient
1. Open a Visual Studio 2017 x64 Command Shell in the `serlio` root directory.
2. Call `powershell .\build.ps1`.

##### Building with Visual Studio
1. Open a Visual Studio 2017 x64 Command Shell in the `serlio` root directory, i.e. at `<your path to>\esri-cityengine-sdk\examples\serlio`.
2. Create a build directory with `mkdir build` and change into it with `cd build`.
3. Run `cmake` to generate a Visual Studio solution:
   ```
   cmake -G "Visual Studio 16 2019" ../src
   ```
   Use options `-Dprt_DIR=<ce sdk root>\cmake` and `-Dmaya_DIR=<maya installation root>` to override the default locations of CityEngine SDK and Maya.
1. Open the generated `serlio_parent.sln` in Visual Studio.
2. Switch the solution configuration to "Release" or "RelWithDebInfo" ("Debug" is not supported with release CE SDK).
3. Call `build` on the `INSTALL` project.
1. Proceed with the Installation Instructions below.

##### Building on the Command Line
1. Open a Visual Studio 2019 x64 Command Shell in the `serlio` root directory, i.e. at `<your path to>\esri-cityengine-sdk\examples\serlio`.
2. Create a build directory with `mkdir build` and change into it with `cd build`.
3. Run `cmake` to generate the Makefiles:
   ```
   cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo ../src
   ```
   Use options `-Dprt_DIR=<ce sdk root>\cmake` and `-Dmaya_DIR=<maya installation root>` to override the default locations of CityEngine SDK and Maya.
4. Run make to build the desired target, e.g.
   ```
   nmake install
   ```
5. The build result will appear in the `install` directory in parallel to the `build` directory. We will use this as the plugin directory below.

##### Building MSI Installers
1. WiX Toolset 3 (3.11.1 or later) and Apache Ant (1.10.x or later) is required for building MSI installers.
2. Start by building serlio with release configurations as described above
3. Open a Command Shell in the `serlio\deploy` directory
4. Run `ant` in the current directioy. Make sure to set at least one valid install path:
   ```
   ant -D"maya2022.dir"=../install
   ```
5. The installer will appear in `serlio\build\build_msi`.

#### Linux
1. Open a terminal (e.g. bash)
1. Change into the example directory: `cd <your path to>/esri-cityengine-sdk/examples/serlio`
1. Create a build directory and change into it: `mkdir build && cd build`
1. Run cmake (adjust the maya path if necessary): `cmake -Dmaya_DIR=/usr/autodesk/maya2022 ../src`
1. Compile: `make install`
1. The build result will appear in the `install` directory in parallel to the `build` directory. We will use this as the plugin directory below.

### Install local build
#### Windows
1. Locate the `install` directory where you [built](build.md) the plugin, let's call it `PLUGINDIR`
1. Locate the Maya.env file in your home, usually its in `<home directory>\Documents\maya\2020`
1. Edit Maya.env as follows:
   ```
   :: replace <PLUGINDIR> with the actual path
   PATH=<PLUGINDIR>\plug-ins;%PATH%
   MAYA_PLUG_IN_PATH=<PLUGINDIR>\plug-ins
   MAYA_SCRIPT_PATH=<PLUGINDIR>\scripts
   XBMLANGPATH=<PLUGINDIR>\icons
   ```
1. Start maya
1. Open the plugin manager: Windows -> Settings/Preferences -> Plug-in Manager
1. Enable `serlio.mll`
1. The plugin should load and a new menu item `Serlio` should appear in Maya.

#### Linux
1. Let's call the directory with the Serlio binaries `PLUGINDIR` (downloaded binaries or locally built `install` folder)
1. Locate the Maya.env file in your home, e.g.: `~/maya/2020/Maya.env`
1. Edit Maya.env as follows:
   ```
   PLUGINDIR=<PLUGINDIR> # replace <PLUGINDIR> with the actual path
   MAYA_PLUG_IN_PATH=$PLUGINDIR/plug-ins
   MAYA_SCRIPT_PATH=$PLUGINDIR/scripts
   XBMLANGPATH=$PLUGINDIR/icons/%B
   ```
1. Start Maya (Note: you may want to start maya from a shell to see the serlio log output).
1. Open the plugin manager: Windows -> Settings/Preferences -> Plug-in Manager
1. Enable `serlio.so`
1. The plugin should load and a new menu item `Serlio` should appear in Maya.

## Release Notes

### v2.0.0 (2022-05-XX)
#### Added:
* Support for Maya 2020 and 2022. (#73)
* Command to remove serlio nodes and materials from mesh.
* Support and UI for resetting user-set attributes. (#50)
* Support for dynamic enums.
* Support for RGB-based opacity maps. (#15, #33, #18)
* Support for legacy usage of the _@Range_ annotation
* UX to display warnings and errors in rule files:
  * RPK/CGB and PRT version mismatch
  * Asset errors
  * CGA errors
  * Generate errors
* Help tab with links to: 
  * Serlio Website
  * CGA Reference
  * RPK Manual
* About dialog with version number and license info.
* Node Behavior and UUID tab to attributes

#### Changed:
* Updated Procedural Runtime (PRT) to 2.6 (corresponds to CityEngine 2021.1) and exclude non-relevant extensions.
* Avoid unpacking of rule package (performance improvement). (#74)
* Improved rule attribute ordering (matching CityEngine).
* Stopped supporting generation of rules without Maya construction history.
* Automatically switch viewport to textured mode after generating materials.
* Replace rule package instead of stacking serlio nodes, when calling "Add rule package..". (#13)
* Improved UI feedback on node for invalid rule package paths
* Improved generation performance.
* Fixed metadata storage on scene save/load
* Fixed scaling of geometry.
* Fixed random seed handling. (#51)
* Fixed rule package reloading, when start rule changes. (#17)
* Fixed undo/redo for material creation and rule switching.
* Fixed crash in case PRT generates an empty mesh.
* Fixed crash when modifying rule package path.
* Reset material to default, when PRT generates an empty mesh.
* Fixed enum attributes. (#23)
* Fixed file picker. (#55)
* Fixed error message in MEL console.
* Fixed faintly visible transparent stingray materials.
* Fixed Maya attribute naming issues.
* Fixed bug that could create duplicate tweak nodes.

#### Development:
* Updated compilers (now using C++ 17)
* Updated test framework and build system 
* Bundled installers for all maya versions (2019-2022) into one

#### Examples:
* Added Street Segment example
* Updated Favela example

### v1.1.0 (2020-06-02)
* BREAKING CHANGE: Switched to official Autodesk IDs for Serlio custom nodes. (#26)
* Added creation of Arnold materials. (#11, #40, #65)
* Correctly compute default values of rule attributes based on initial shape geometry.
* Show error message if required Maya plugins (e.g. 'shaderFXPlugin') are not loaded. (#65)
* Updated to CityEngine SDK 2019.1. (#42)
* Introduced clang-format. (#37)

### v1.0.1 (2019-08-21)
* Merged support for creating MSI installers via CMake.
* Fixed wrong build type of codec library in MSI installers.
* Improved error handling when loading the plugin (check codec library).
* Do not install unnecessary PRT codecs for Serlio.
* Improved error handling (prevent crash) if a specified Rule Package (RPK) is not readable.

### v1.0.0 (2019-07-31)
* Optimized the encoding of PRT geometry with many meshes (e.g. results in a speedup of about 5x for the "Parthenon" CityEngine example).
* Do not change maya selection after user touches the attribute sliders.
* Do not pass maya's automatically created color child attributes to PRT.

### v1.0.0-beta.1 (2019-07-26)
* Added attribute sorting in the Maya node editor like in CityEngine.
* Fixed saving of string rule attributes in Maya scene.
* Fixed connectivity of generated meshes and normal orientation.
* Fixed handling of rule attribute annotations (enum, range).
* Fixed tex-coords handling in case some meshes do not have any.
* Performance optimizations.
* Rebranding to "Serlio" in UI.
* Publication of source code on github.

### v0.5.0 (2019-06-12)
* First release after complete rewrite based on Maya "modifier node" example.
* Better UX and conformance with behavior of other Maya geometry tools (e.g. undo). 
* Improved performance and Material support. 

### v0.0.0 (2012-2018)
* Experimental maya plugin released as source code example for CityEngine SDK.

## Known Limitations

* Serlio 2.0: In Maya 2019 on Windows, Serlio does not support reading USD assets from RPKs.
* Modifying the input shape after a Serlio node is attached is not supported
* Scaling transformations on the initial shape mesh will scale the entire generated model. Make sure that all scaling transformations on the initial shape mesh are frozen before applying a rule ("Modify" > "Freeze Transformations")
* Maya uses centimeters as default working units, we recommend changing these to meters in
  "Windows" > "Settings/Preferences" > "Preferences" > "Settings" > "Working Units" 
   * We also recommend multiplying the value of the "Near Clip Plane" and "Far Clip Plane" attributes  in the `perspShape` of the perspective camera by a factor of 100
* Duplicating a generated Serlio building multiple times with copy/paste can lead to wrong materials due to a naming bug in maya. However, there are two workarounds: 
   1. Use smart duplicate instead: Go to "Edit" > "Duplicate Special" and tick "Assign unique name to children nodes"
   1. Manually rename all pasted meshes to something unique and regenerate the rule

## Licensing Information

Serlio is free for personal, educational, and non-commercial use. Commercial use requires at least one commercial license of the latest CityEngine version installed in the organization. Redistribution or web service offerings are not allowed unless expressly permitted.

Serlio is under the same license as the included [CityEngine SDK](https://github.com/esri/cityengine-sdk#licensing). An exception is the Serlio source code (without CityEngine SDK, binaries, or object code), which is licensed under the Apache License, Version 2.0 (the “License”); you may not use this work except in compliance with the License. You may obtain a copy of the License at https://www.apache.org/licenses/LICENSE-2.0

All content in the "Examples" directory/section is licensed under the APACHE 2.0 license as well.

For questions or enquiries regarding licensing, please contact the Esri CityEngine team (cityengine-info@esri.com).
