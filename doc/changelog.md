# Serlio ChangeLog

## v2.0.0 (2022-05-XX)

### Added:
* Support for Maya 2020 and 2022. (#73)
* Command to remove serlio nodes and materials from mesh.
* Support and UI for resetting user-set attributes. (#50)
* Support for dynamic enums.
* Support for RGB-based opacity maps. (#15, #33, #18)
* UX to warn user in case of:
  * RPK/CGB and PRT version mismatch
  * CGA generation errors
  * Asset errors
* Help tab with links to: 
  * Serlio Website
  * CGA Reference
  * RPK Manual
* About dialog with version number and license info.

### Changed:
* Updated Procedural Runtime (PRT) to 2.6 (corresponds to CityEngine 2021.1) and exclude non-relevant extensions.
* Avoid unpacking of rule package (performance improvement). (#74)
* Improved rule attribute ordering (matching CityEngine).
* Stopped supporting generation of rules without Maya construction history.
* Automatically switch viewport to textured mode after generating materials.
* Replace rule package instead of stacking serlio nodes, when calling "Add rule package..". (#13)
* Improved UI feedback on node for invalid rule package paths
* Improved generation performance.
* Fixed scaling of geometry.
* Fixed random seed handling. (#51)
* Fixed rule package reloading, when start rule changes. (#17)
* Fixed undo/redo for material creation and rule switching.
* Fixed crash in case PRT generates an empty mesh.
* Reset material to default, when PRT generates an empty mesh.
* Fixed enum attributes. (#23)
* Fixed file picker. (#55)
* Fixed error message in MEL console.
* Fixed faintly visible transparent stingray materials.
* Fixed Maya attribute naming issues.

### Development:
* Updated compilers (now using C++ 17)
* Updated test framework and build system

## v1.1.0 (2020-06-02)
* BREAKING CHANGE: Switched to official Autodesk IDs for Serlio custom nodes. (#26)
* Added creation of Arnold materials. (#11, #40, #65)
* Correctly compute default values of rule attributes based on initial shape geometry.
* Show error message if required Maya plugins (e.g. 'shaderFXPlugin') are not loaded. (#65)
* Updated to CityEngine SDK 2019.1. (#42)
* Introduced clang-format. (#37)

## v1.0.1 (2019-08-21)
* Merged support for creating MSI installers via CMake.
* Fixed wrong build type of codec library in MSI installers.
* Improved error handling when loading the plugin (check codec library).
* Do not install unnecessary PRT codecs for Serlio.
* Improved error handling (prevent crash) if a specified Rule Package (RPK) is not readable.

## v1.0.0 (2019-07-31)
* Optimized the encoding of PRT geometry with many meshes (e.g. results in a speedup of about 5x for the "Parthenon" CityEngine example).
* Do not change maya selection after user touches the attribute sliders.
* Do not pass maya's automatically created color child attributes to PRT.

## v1.0.0-beta.1 (2019-07-26)
* Added attribute sorting in the Maya node editor like in CityEngine.
* Fixed saving of string rule attributes in Maya scene.
* Fixed connectivity of generated meshes and normal orientation.
* Fixed handling of rule attribute annotations (enum, range).
* Fixed tex-coords handling in case some meshes do not have any.
* Performance optimizations.
* Rebranding to "Serlio" in UI.
* Publication of source code on github.

## v0.5.0 (2019-06-12)
* First release after complete rewrite based on Maya "modifier node" example.
* Better UX and conformance with behavior of other Maya geometry tools (e.g. undo). 
* Improved performance and Material support. 

## v0.0.0 (2012-2018)
* Experimental maya plugin released as source code example for CityEngine SDK.