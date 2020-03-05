# Quick Start

## Installation

### Windows: Provided Binaries
1. Download the MSI installer for your Maya version (2018 or 2019) from the [desired release](https://github.com/Esri/serlio/releases)
1. Run the MSI installer
1. Start Maya
1. You should now see the new menu item "Serlio".

### Windows: Compile from Source
1. Locate the `install` directory where you [built](build.md) the plugin, let's call it `PLUGINDIR`
1. Locate the Maya.env file in your home, usually its in `<home directory>\Documents\maya\2019`
1. Edit Maya.env as follows:
   ```
   :: replace <PLUGINDIR> with the actual path
   PATH=<PLUGINDIR>\plug-ins;%PATH%
   MAYA_PLUG_IN_PATH=<PLUGINDIR>\plug-ins
   MAYA_SCRIPT_PATH=<PLUGINDIR>\scripts
   ```
1. Start maya
1. Open the plugin manager: Windows -> Settings/Preferences -> Plug-in Manager
1. Enable `serlio.mll`
1. The plugin should load and a new menu item `Serlio` should appear in Maya.

### Linux
1. Acquire the Serlio binaries, either: 
    * ... download the [desired release](https://github.com/Esri/serlio/releases)
    * ... or [build](build.md) them yourself, which will result in an `install` directory
1. Either way, let's call the directory with the Serlio binaries `PLUGINDIR`
1. Locate the Maya.env file in your home, e.g.: `~/maya/2019/Maya.env`
1. Edit Maya.env as follows:
   ```
   PLUGINDIR=<PLUGINDIR> # replace <PLUGINDIR> with the actual path
   MAYA_PLUG_IN_PATH=$PLUGINDIR/plug-ins
   MAYA_SCRIPT_PATH=$PLUGINDIR/scripts
   ```
1. Start Maya (Note: you may want to start maya from a shell to see the serlio log output).
1. Open the plugin manager: Windows -> Settings/Preferences -> Plug-in Manager
1. Enable `libserlio.so`
1. The plugin should load and a new menu item `Serlio` should appear in Maya.

## Usage Instructions

1. Use CityEngine to export a Rule Package (RPK).
1. In Maya, select a mesh and use the `Serlio` menu to assign the RPK. This will run the rules for each face in the mesh.
1. Use the Hypergraph to navigate to the "serlio" node where you can edit the rule parameters.
1. To create materials, apply one of the two commands in the serlio menu on the generated model. Please note the the two material systems are mutually exclusive at this point.
