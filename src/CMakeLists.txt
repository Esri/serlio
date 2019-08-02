cmake_minimum_required(VERSION 3.13)

project(serlio CXX)

set(SRL_PROJECT_NAME "serlio")
set(SRL_VENDOR "Esri R&D Center Zurich")
set(SRL_DESCRIPTION "Esri CityEngine Plugin for Autodesk Maya")

include(common.cmake)

### version

set(SRL_VERSION_MAJOR 1)
set(SRL_VERSION_MINOR 0)
set(SRL_VERSION_PATCH 1)

if(NOT SRL_VERSION_BUILD)
    set(SRL_VERSION_BUILD DEV)
endif()

# we use semver.org, Semantic Versioning 2.0.0
# i.e. <major>.<minor>.<patch>(-<prerelease>)+b<buildnr>.maya<maya major ver>.prt<prt ver>
# Note: There are some limitations regarding Windows installers. See cpack_installer.cmake for details.
set(SRL_VERSION_MMP "${SRL_VERSION_MAJOR}.${SRL_VERSION_MINOR}.${SRL_VERSION_PATCH}")
if(NOT SRL_VERSION_PRERELEASE)
    set(SRL_VERSION_MMP_PRE "${SRL_VERSION_MMP}")
else()
    set(SRL_VERSION_MMP_PRE "${SRL_VERSION_MMP}-${SRL_VERSION_PRERELEASE}")
endif()
set(SRL_VERSION_BASE "${SRL_VERSION_MMP_PRE}+b${SRL_VERSION_BUILD}")
set(SRL_VERSION "${SRL_VERSION_BASE}.maya${maya_VERSION_MAJOR}.prt${PRT_VERSION_MAJOR}-${PRT_VERSION_MINOR}-${PRT_VERSION_MICRO}")
message(STATUS "Using SRL_VERSION = ${SRL_VERSION}")


### setup targets

set(CODEC_TARGET serlio_codec)
set(SERLIO_TARGET serlio)
set(TEST_TARGET serlio_test)

### configure packaging
if(WIN_INSTALLER) # <-- To be set on the command line
    include(${CMAKE_CURRENT_SOURCE_DIR}/../deploy/cpack_installer.cmake)
else()
    include(${CMAKE_CURRENT_SOURCE_DIR}/../deploy/cpack_archives.cmake)
endif()

add_subdirectory(codec)
add_subdirectory(serlio)
add_dependencies(${SERLIO_TARGET} ${CODEC_TARGET})

enable_testing()
add_subdirectory(test EXCLUDE_FROM_ALL)

include(CPack)
