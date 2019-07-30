param(
# Target. One of 'All', 'Archive', 'Installer' plus the optional suffix 'FromScratch'.
[String]$TARGET = 'AllFromScratch',
# Release With Debug Info yes/no. (Ignored by installer builds.)
[Bool]$DEBUGINFO = $false,
# MAYA installation directory. If left empty, cmake will try to find it on its own
[String]$MAYA_DIR = '',
# CityEngine SDK dir. If left empty, cmake will try to download it from Github.
[String]$CESDK_DIR = '',
# Build 'number'.
[String]$BUILD_NO = 'DEV',
# Default download locations for required tools. May be overwritten if desired.
[String]$BUILD_NINJA_URL = 'https://github.com/ninja-build/ninja/releases/download/v1.9.0/ninja-win.zip',
[String]$CMAKE_URL = 'https://github.com/Kitware/CMake/releases/download/v3.15.0/cmake-3.15.0-win64-x64.zip',
[String]$WIX_TOOLSET_URL = 'https://github.com/wixtoolset/wix3/releases/download/wix3111rtm/wix311-binaries.zip'
)

Set-StrictMode -Version Latest

# Folders for the build.
$BUILD_FOLDER = "$PSScriptRoot\build"
$TOOLS_FOLDER = "$BUILD_FOLDER\tools"
$ARCHIVE_BUILD_FOLDER = "$BUILD_FOLDER\build_zip"
$INSTALLER_BUILD_FOLDER = "$BUILD_FOLDER\build_msi"
$ARTIFACTS_FOLDER = "$BUILD_FOLDER\out"

# Folders for the build tools
$NINJA_FOLDER = "$TOOLS_FOLDER\ninja"
$CMAKE_FOLDER = "$TOOLS_FOLDER\cmake"
$WIX_FOLDER = "$TOOLS_FOLDER\wix"

# Relevant executables
$NINJA_EXE = 'ninja.exe'
$CMAKE_EXE = 'cmake.exe'
$WIX_EXE = 'candle.exe'

function NukeAll {
    NukeFolder($BUILD_FOLDER)
    Write-Host ">>> Folder structure cleaned." -ForegroundColor Green
}

function NukeTools {
    NukeFolder($TOOLS_FOLDER)
    Write-Host ">>> Folder structure for tools destroyed." -ForegroundColor Green
}

function NukeBuilds {
    NukeFolder($ARCHIVE_BUILD_FOLDER)
    NukeFolder($INSTALLER_BUILD_FOLDER)
    NukeFolder($ARTIFACTS_FOLDER)
    Write-Host ">>> Folder structure for builds destroyed." -ForegroundColor Green
}

function MakeDirs {
    # Create the desired folder structure.
    $folders = $BUILD_FOLDER, $TOOLS_FOLDER, $NINJA_FOLDER, $CMAKE_FOLDER, $WIX_FOLDER, $ARCHIVE_BUILD_FOLDER, $INSTALLER_BUILD_FOLDER, $ARTIFACTS_FOLDER
    foreach ($f in $folders) {
        if (!(Test-Path $f)) {
            New-Item -ItemType Directory -Path $f | Out-Null
        }
    }
    Write-Host ">>> Folder structure created. Root: $PSScriptRoot" -ForegroundColor Green
}

function FetchTools {
    $ninja = Triple $BUILD_NINJA_URL $NINJA_FOLDER $NINJA_EXE
    $cmake = Triple $CMAKE_URL $CMAKE_FOLDER $CMAKE_EXE
    $wix = Triple $WIX_TOOLSET_URL $WIX_FOLDER $WIX_EXE
    $tool_list = $ninja, $cmake, $wix
    Write-Host ">>> Fetching tools..." -ForegroundColor Green
    foreach ($t in $tool_list) {
        $u = $t.Item1
        $archive = $u.Split('/') | Select-Object -Last 1
        $download = "$TOOLS_FOLDER\$archive"
        if (Test-Path $download) {
            Write-Host ">>>>> $archive already present. Good!" -ForegroundColor Green
        } else {
            Write-Host ">>>>> $archive not found. Downloading from $u to $download." -ForegroundColor Green
            DownloadFile $u $download
        }

        if (FindFile $t.Item3 $t.Item2) {
            Write-Host ">>>>> $archive already unpacked. Good!" -ForegroundColor Green
        } else {
            $dest = $t.Item2
            Write-Host ">>>>> Extracting $archive to $dest." -ForegroundColor Green
            UnzipArchive $download $dest
        }
    }
}

function SetupEnv {
    Write-Host ">>> Preparing PATH..." -ForegroundColor Green
    $ninjaPath = FindFile 'ninja.exe' $NINJA_FOLDER
    Write-Host ">>>>> Found ninja.exe in $ninjaPath. Good!" -ForegroundColor Green
    $cmakePath = FindFile 'cmake.exe' $CMAKE_FOLDER
    Write-Host ">>>>> Found cmake.exe in $cmakePath. Good!" -ForegroundColor Green
    $wixPath = FindFile 'candle.exe' $WIX_FOLDER
    Write-Host ">>>>> Found candle.exe in $wixPath. Good!" -ForegroundColor Green

    $env:Path = "$ninjaPath;$cmakePath;$wixPath;$env:Path"
}

function BuildArchive {
     Write-Host ">>> Building zip archive..." -ForegroundColor Green
    $cwd = Get-Location
    Set-Location -Path $ARCHIVE_BUILD_FOLDER
    # Call cmake
    Write-Host ">>>>> Calling 'cmake' now." -ForegroundColor Green
    $mdir = If ([System.String]::IsNullOrWhiteSpace($MAYA_DIR)) { "" } else { "-Dmaya_DIR='$MAYA_DIR'" }
    $cesdkdir = If ([System.String]::IsNullOrWhiteSpace($CESDK_DIR)) { "" } else { "-Dprt_DIR='$CESDK_DIR'" }
    $bt = If ($DEBUGINFO) { "-DCMAKE_BUILD_TYPE=RelWithDebInfo" } else { "-DCMAKE_BUILD_TYPE=Release" }
    cmake -G Ninja $mdir $cesdkdir -DSRL_VERSION_BUILD="$BUILD_NO" $bt ../../src
    # ...and ninja
    Write-Host ">>>>> Calling 'ninja' now." -ForegroundColor Green
    ninja package
    # Move the built installer to the out folder
    $archive = FindFile '*.zip' $ARCHIVE_BUILD_FOLDER
    if($archive) {
        Move-Item -Path "$ARCHIVE_BUILD_FOLDER\*.zip" -Destination $ARTIFACTS_FOLDER -Force
        Write-Host ">>>>> Archive build done. Zip can be found in $ARTIFACTS_FOLDER\." -ForegroundColor Green
    } else {
        Write-Host ">>>>> Strange, no .zip found..." -ForegroundColor Red
        throw 'Archive build failed.'
    }
    Set-Location -Path $cwd
}

function BuildInstaller {
    Write-Host ">>> Building MSI installer...!" -ForegroundColor Green
    $cwd = Get-Location
    Set-Location -Path $INSTALLER_BUILD_FOLDER
    # Call cmake
    Write-Host ">>>>> Calling 'cmake' now." -ForegroundColor Green
    $mdir = If ([System.String]::IsNullOrWhiteSpace($MAYA_DIR)) { "" } else { "-Dmaya_DIR='$MAYA_DIR'" }
    $cesdkdir = If ([System.String]::IsNullOrWhiteSpace($CESDK_DIR)) { "" } else { "-Dprt_DIR='$CESDK_DIR'" }
    cmake -G Ninja -DWIN_INSTALLER=True $mdir $cesdkdir -DSRL_VERSION_BUILD="$BUILD_NO" ../../src
    # ...and ninja
    Write-Host ">>>>> Calling 'ninja' now." -ForegroundColor Green
    ninja package
    # Move the built installer to the out folder
    $installer = FindFile '*.msi' $INSTALLER_BUILD_FOLDER
    if($installer) {
        Move-Item -Path "$INSTALLER_BUILD_FOLDER\*.msi" -Destination $ARTIFACTS_FOLDER -Force
        Write-Host ">>>>> Installer build done. MSI can be found in $ARTIFACTS_FOLDER\." -ForegroundColor Green
    } else {
        Write-Host ">>>>> Strange, no .msi found..." -ForegroundColor Red
        throw 'Installer build failed.'
    }
    Set-Location -Path $cwd
}

## Do it!
function DoIt {
    if ($TARGET.EndsWith('FromScratch')) {
        NukeAll
    }

    if (ExeOnPath 'cl.exe') { # <-- minimal test, cmake will be more thorough
        Write-Host ">>> Looks like cl.exe is around. Good!" -ForegroundColor Green
    } else {
        Write-Host ">>> Oops, looks like cl.exe cannot be found..." -ForegroundColor Red
        throw 'I cannot go on like this. Run me from a VS command prompt.'
    }

    MakeDirs
    FetchTools
    SetupEnv

    if($TARGET.StartsWith('All') -or $TARGET.StartsWith('Installer')) {
        BuildInstaller
    }
    if($TARGET.StartsWith('All') -or $TARGET.StartsWith('Archive')) {
        BuildArchive
    }
}

## Helpers
function NukeFolder($Folder) {
    if (Test-Path $Folder) {
        # Recursively delete the whole bunch.
        Remove-Item -Path $Folder -Recurse -Force
    }
}

function DownloadFile([String]$Url, [String]$Destination) {
    $wc = New-Object System.Net.WebClient
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        $wc.DownloadFile($Url, $Destination)
    } finally {
        $wc.Dispose()
    }
}

function UnzipArchive([String]$Archive, [String]$Destination) {
    $shell = New-Object -com shell.application
    $zip = $shell.NameSpace($Archive)
    foreach($item in $zip.items()) {
        $shell.NameSpace($Destination).copyhere($item)
    }
}

function FindFile([String]$Name, [String]$SearchRoot) {
    $fi = Get-ChildItem -Path $SearchRoot -Include $Name -File -Recurse `
            -ErrorAction SilentlyContinue
    if ($fi) {
        return $fi.Directory.FullName
    } else {
        return $null 
    }
}

function ExeOnPath([String]$Exe) {
    if(Get-Command $Exe -ErrorAction SilentlyContinue) {
        return $True
    }
    return $False
}
function Triple($Item1, $Item2, $Item3) {
    return [System.Tuple]::Create($Item1, $Item2, $Item3)
}

$ErrorActionPreference = "Stop"
DoIt


