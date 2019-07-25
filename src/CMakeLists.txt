cmake_minimum_required(VERSION 3.13)

project(serlio CXX)

include(common.cmake)


### version

set(SRL_VERSION_MAJOR 1)
set(SRL_VERSION_MINOR 0)
set(SRL_VERSION_PATCH 0-beta.1)
if(NOT SRL_VERSION_BUILD)
    set(SRL_VERSION_BUILD DEV)
endif()

# we use semver.org, Semantic Versioning 2.0.0
# i.e. <major>.<minor>.<patch>+b<buildnr>.maya<maya major ver>.prt<prt ver>
set(SRL_VERSION_BASE "${SRL_VERSION_MAJOR}.${SRL_VERSION_MINOR}.${SRL_VERSION_PATCH}+b${SRL_VERSION_BUILD}")
set(SRL_VERSION "${SRL_VERSION_BASE}.maya${maya_VERSION_MAJOR}.prt${PRT_VERSION_MAJOR}-${PRT_VERSION_MINOR}-${PRT_VERSION_MICRO}")
message(STATUS "Using SRL_VERSION = ${SRL_VERSION}")


### setup targets

set(CODEC_TARGET serlio_codec)
set(SERLIO_TARGET serlio)
set(TEST_TARGET serlio_test)

add_subdirectory(codec)
add_subdirectory(serlio)
add_dependencies(${SERLIO_TARGET} ${CODEC_TARGET})

enable_testing()
add_subdirectory(test EXCLUDE_FROM_ALL)


### setup packaging

if(SRL_WINDOWS)
    set(CPACK_GENERATOR             ZIP)
    set(CPACK_SOURCE_GENERATOR      ZIP)
    set(CPACK_ZIP_COMPONENT_INSTALL 1)
    set(SRL_PKG_OS "windows")
elseif(SRL_MACOS)
    set(CPACK_GENERATOR             TGZ)
    set(CPACK_SOURCE_GENERATOR      TGZ)
    set(CPACK_TGZ_COMPONENT_INSTALL 1)
    set(SRL_PKG_OS "macos")
elseif(SRL_LINUX)
    set(CPACK_GENERATOR             TGZ)
    set(CPACK_SOURCE_GENERATOR      TGZ)
    set(CPACK_TGZ_COMPONENT_INSTALL 1)
    set(SRL_PKG_OS "linux")
endif()

set(CPACK_PACKAGE_NAME                "serlio")
set(CPACK_PACKAGE_VENDOR              "Esri R&D Center Zurich")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Esri CityEngine Plugin for Autodesk Maya")
set(CPACK_PACKAGE_VERSION_MAJOR       ${SRL_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR       ${SRL_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH       ${SRL_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "serlio-${SRL_VERSION}")
set(CPACK_PACKAGE_FILE_NAME           "${CPACK_PACKAGE_NAME}-${SRL_VERSION}-${SRL_PKG_OS}")

configure_file(deployment.properties.in deployment.properties @ONLY)

include(CPack)
