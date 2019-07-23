# setup wix installer
include(${CMAKE_CURRENT_LIST_DIR}/cpack_common.cmake)

set(CPACK_GENERATOR WIX)

# UPGRADE_GUID. This GUID must remain constant, it helps the installer
# to identify different (previous) installations of the same software.
set(CPACK_WIX_UPGRADE_GUID "9B170749-ECDF-4F66-9E2F-C9045727A6D4")

# TODO: Ideally, the same version of the software should have the same
# GUID. (Maybe hash the version string?)
set(CPACK_WIX_PRODUCT_GUID "*")

# This sets the path to the license file explicitly. If not set, it
# defaults to CPACK_RESOURCE_FILE_LICENSE, which we could use instead.
set(CPACK_WIX_LICENSE_RTF "${CMAKE_CURRENT_LIST_DIR}/resources/license.rtf")

# (1) Installer Icon
set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_LIST_DIR}/resources/serlio.ico")
#
# (2) Banner, visible on all Wizard pages except first and last. Must be 493x58 pixels.
set(CPACK_WIX_UI_BANNER "${CMAKE_CURRENT_LIST_DIR}/resources/banner.png")
#
# (3) Dialog background, appears on first and last page of the Wizard. Must be 493x312 pixels.
set(CPACK_WIX_UI_DIALOG "${CMAKE_CURRENT_LIST_DIR}/resources/dialog.png")

# We need to overwrite this variable from cpack_common because it needs a slightly differnet value.
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "C:/ProgramData/Autodesk/ApplicationPlugins/Serlio")
# XML doesn't like naked '&'.
set(PACKAGE_VENDOR_ESCAPED "Esri R&amp;D Center Zurich")
set(CPACK_WIX_SKIP_PROGRAM_FOLDER true)

# Fill in the PackageContents template
configure_file(${CMAKE_CURRENT_LIST_DIR}/PackageContents.xml.in ${PROJECT_BINARY_DIR}/PackageContents.xml)

# Add it to the WiX installer.
install(FILES ${PROJECT_BINARY_DIR}/PackageContents.xml DESTINATION "/")