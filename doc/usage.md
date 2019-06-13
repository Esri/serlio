# Quick Start

## Installation

### Windows
1. Locate the directory where you copied the plugin (or keep `<your path to>\esri-cityengine-sdk\examples\serlio\install`), let's call it `PLUGINDIR`
1. Locate the Maya.env file in your home, usually its in `<home directory>\Documents\maya\2018`
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
1. The plugin should load and a new menu item `PRT` should appear in Maya.


### Linux
1. Locate the absolute path to the `install` directory created above (or keep `<your path to>/esri-cityengine-sdk/examples/serlio/install`), let's call it `PLUGINDIR`
1. Locate the Maya.env file in your home: `~/maya/2018/Maya.env`
1. Edit Maya.env as follows:
   ```
   PLUGINDIR=<PLUGINDIR> # replace <PLUGINDIR> with the actual path
   MAYA_PLUG_IN_PATH=$PLUGINDIR/plug-ins
   MAYA_SCRIPT_PATH=$PLUGINDIR/scripts
   ```
1. Start maya (note: you may want to start maya from a shell to see the serlio log output)
1. Open the plugin manager: Windows -> Settings/Preferences -> Plug-in Manager
1. Enable `libserlio.so`
1. The plugin should load and a new menu item `PRT` should appear in Maya.

## Usage Instructions

1. Use CityEngine to export a Rule Package (RPK).
1. In Maya, select a mesh and use the PRT menu to assign the RPK. This will run the rules for each face in the mesh.
2. Use the Hypergraph to navigate to the PRT node where you can edit the rule parameter.
