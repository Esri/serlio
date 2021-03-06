### setup packaging

include(${CMAKE_CURRENT_LIST_DIR}/cpack_common.cmake)

# Put everything into the archive root (cf. cpack_installer.cmake)
set(INSTALL_FOLDER_PREFIX ".")

if(SRL_WINDOWS)
    set(CPACK_GENERATOR             ZIP)
    set(CPACK_SOURCE_GENERATOR      ZIP)
    set(CPACK_ZIP_COMPONENT_INSTALL 1)
elseif(SRL_MACOS)
    set(CPACK_GENERATOR             TGZ)
    set(CPACK_SOURCE_GENERATOR      TGZ)
    set(CPACK_TGZ_COMPONENT_INSTALL 1)
elseif(SRL_LINUX)
    set(CPACK_GENERATOR             TGZ)
    set(CPACK_SOURCE_GENERATOR      TGZ)
    set(CPACK_TGZ_COMPONENT_INSTALL 1)
endif()