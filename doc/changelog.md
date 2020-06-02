# Serlio ChangeLog

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