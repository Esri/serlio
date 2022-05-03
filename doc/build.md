# Build Serlio from Source


## Software Requirements

### All Platforms
* License for CityEngine (2019.0 or later), e.g. to author Rule Packages.
* CMake 3.13 or later (http://www.cmake.org)
* Autodesk Maya 2018 or later or the corresponding development kit

### Windows
* Windows 7, 8.1 or 10 (64bit)
* Required C++ compiler: Visual Studio 2017 with Toolset MSVC 14.11 or later
* WiX Toolset 3.11.1: Optional, required for building .msi installers
* Required flags for extension libraries release mode: `/bigobj /GR /EHsc /MD`

### Linux
* RedHat Enterprise Linux 7.x or compatible
* Required C++ compiler: GCC 6.3 or later (RedHat Enterprise Linux DevToolSet 6.1)
* Required flags for extension libraries: `-std=c++14 -D_GLIBCXX_USE_CXX11_ABI=0 -march=nocona -fvisibility=hidden -fvisibility-inlines-hidden -Wl,--exclude-libs,ALL`


## Build Instructions 

### Windows

#### Quick build for the impatient
1. Open a Visual Studio 2017 x64 Command Shell in the `serlio` root directory.
2. Call `powershell .\build.ps1`.

#### Building with Visual Studio
1. Open a Visual Studio 2017 x64 Command Shell in the `serlio` root directory, i.e. at `<your path to>\esri-cityengine-sdk\examples\serlio`.
2. Create a build directory with `mkdir build` and change into it with `cd build`.
3. Run `cmake` to generate a Visual Studio solution:
   ```
   cmake -G "Visual Studio 15 2017 Win64" ../src
   ```
   Use options `-Dprt_DIR=<ce sdk root>\cmake` and `-Dmaya_DIR=<maya installation root>` to override the default locations of CityEngine SDK and Maya.
1. Open the generated `serlio_parent.sln` in Visual Studio.
2. Switch the solution configuration to "Release" or "RelWithDebInfo" ("Debug" is not supported with release CE SDK).
3. Call `build` on the `INSTALL` project.
1. Proceed with the Installation Instructions below.

#### Building on the Command Line
1. Open a Visual Studio 2017 x64 Command Shell in the `serlio` root directory, i.e. at `<your path to>\esri-cityengine-sdk\examples\serlio`.
2. Create a build directory with `mkdir build` and change into it with `cd build`.
3. Run `cmake` to generate the Makefiles:
   ```
   cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo ../src
   ```
   Use options `-Dprt_DIR=<ce sdk root>\cmake` and `-Dmaya_DIR=<maya installation root>` to override the default locations of CityEngine SDK and Maya.
4. Run make to build the desired target, e.g.
   ```
   make install
   ```
5. The build result will appear in the `install` directory in parallel to the `build` directory. We will use this as the plugin directory below.

#### Building MSI Installers
1. WiX Toolset 3 (3.11.1 or later) and Apache Ant (1.10.x or later) is required for building MSI installers.
2. Start by building serlio with release configurations as described above
3. Open a Command Shell in the `serlio\deploy` directory
4. Run `ant` in the current directioy. Make sure to set at least one valid install path:
   ```
   ant -D"maya2022.dir"=../install
   ```
5. The installer will appear in `serlio\build\build_msi`.

### Linux
1. Open a terminal (e.g. bash)
1. Change into the example directory: `cd <your path to>/esri-cityengine-sdk/examples/serlio`
1. Create a build directory and change into it: `mkdir build && cd build`
1. Run cmake (adjust the maya path if necessary): `cmake -Dmaya_DIR=/usr/autodesk/maya2022 ../src`
1. Compile: `make install`
1. The build result will appear in the `install` directory in parallel to the `build` directory. We will use this as the plugin directory below.
