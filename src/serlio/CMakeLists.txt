cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0015 NEW)


### setup build target

add_library(${SERLIO_TARGET} SHARED
		serlioPlugin.cpp
		PRTContext.cpp
		prtModifier/MayaCallbacks.cpp
		prtModifier/RuleAttributes.cpp
		prtModifier/PRTModifierAction.cpp
		prtModifier/PRTModifierCommand.cpp
		prtModifier/PRTModifierNode.cpp
		prtModifier/polyModifier/polyModifierCmd.cpp
		prtModifier/polyModifier/polyModifierFty.cpp
		prtModifier/polyModifier/polyModifierNode.cpp
		prtMaterial/PRTMaterialNode.cpp
		util/Utilities.cpp
		util/ResolveMapCache.cpp
		util/MayaUtilities.cpp)

if(CMAKE_GENERATOR MATCHES "Visual Studio.+")
	target_sources(${SERLIO_TARGET}
		PRIVATE
			serlioPlugin.h
			PRTContext.h
			prtModifier/MayaCallbacks.h
			prtModifier/RuleAttributes.h
			prtModifier/PRTModifierAction.h
			prtModifier/PRTModifierCommand.h
			prtModifier/PRTModifierNode.h
			prtModifier/polyModifier/polyModifierCmd.h
			prtModifier/polyModifier/polyModifierFty.h
			prtModifier/polyModifier/polyModifierNode.h
			prtMaterial/PRTMaterialNode.h
			util/Utilities.h
			util/ResolveMapCache.h
			util/MayaUtilities.h)
endif()

set_target_properties(${SERLIO_TARGET} PROPERTIES
	INSTALL_RPATH_USE_LINK_PATH FALSE
	SKIP_RPATH FALSE
	INSTALL_RPATH "\$ORIGIN/")

if(WIN32)
    # On Windows, Maya requires plugins to have the extension "mll"
    set_target_properties(${SERLIO_TARGET} PROPERTIES SUFFIX ".mll")
endif()


### setup compiler flags

set_target_properties(${CLIENT_TARGET} PROPERTIES
		CXX_STANDARD 14)

target_compile_definitions(${SERLIO_TARGET} PRIVATE
	-DSRL_VERSION=\"${SRL_VERSION}\") # quoted to use it as string literal

target_include_directories(${SERLIO_TARGET}
		PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
		PRIVATE $<TARGET_PROPERTY:${CODEC_TARGET},INTERFACE_INCLUDE_DIRECTORIES>) # for IMayaCallbacks.h

if(WIN32)
	target_compile_options(${SERLIO_TARGET} PRIVATE
			-GR -GS -EHsc -Zi -c -permissive-
			$<$<CONFIG:DEBUG>:-Gm -RTC1>)

	target_compile_definitions(${SERLIO_TARGET} PRIVATE
			-DWIN32 -D_WINDOWS -D_AFXDLL -D_MBCS -DNT_PLUGIN -DREQUIRE_IOSTREAM -DBits64_
			$<$<CONFIG:DEBUG>:-D_DEBUG>
			$<$<CONFIG:RELEASE>:-DNDEBUG>)

	target_link_libraries(${SERLIO_TARGET} PRIVATE
			-export:initializePlugin
			-export:uninitializePlugin
			$<$<CONFIG:DEBUG>:-NODEFAULTLIB:LIBCMT>
			$<$<CONFIG:RELEASE>:-NODEFAULTLIB:CPMTD>)

else()
	target_compile_options(${SERLIO_TARGET} PRIVATE
			-D_GLIBCXX_USE_CXX11_ABI=0 -Wl,--exclude-libs,ALL
			-fvisibility=hidden -fvisibility-inlines-hidden
			-m64 -fPIC -fno-strict-aliasing -funsigned-char -Wno-reorder -fno-gnu-keywords -ftemplate-depth=50 -pthread
			-Wall -Wextra -Wno-deprecated -Wno-multichar -Wno-comment -Wno-sign-compare
			$<$<CONFIG:DEBUG>:-ggdb>
			$<$<CONFIG:RELEASE>:-O2 -s>)

	target_compile_definitions(${SERLIO_TARGET} PRIVATE
			-DBits64_ -DUNIX -D_BOOL -DLINUX -DFUNCPROTO -D_GNU_SOURCE -DLINUX_64 -DREQUIRE_IOSTREAM)
endif()


### setup dependencies

srl_add_dependency_prt(${SERLIO_TARGET})
srl_add_dependency_maya(${SERLIO_TARGET})

# Add to solution folder (Visual Studio)
set_property(TARGET ${SERLIO_TARGET} PROPERTY FOLDER "Serlio Maya Plugin")


### setup install target

# serlio

# TODO: below line is sufficient with CMake >= 3.14
#install(TARGETS ${SERLIO_TARGET} RUNTIME DESTINATION ${INSTALL_FOLDER_PREFIX}/plug-ins)
if(SRL_WINDOWS)
	install(TARGETS ${SERLIO_TARGET} RUNTIME DESTINATION ${INSTALL_FOLDER_PREFIX}/plug-ins)
else()
	install(TARGETS ${SERLIO_TARGET} LIBRARY DESTINATION ${INSTALL_FOLDER_PREFIX}/plug-ins)
endif()

install(FILES scripts/AEserlioTemplate.mel DESTINATION ${INSTALL_FOLDER_PREFIX}/scripts)
install(FILES scripts/serlioCreateUI.mel DESTINATION ${INSTALL_FOLDER_PREFIX}/scripts)
install(FILES scripts/serlioDeleteUI.mel DESTINATION ${INSTALL_FOLDER_PREFIX}/scripts)
install(FILES shaders/serlioShaderStingray.sfx DESTINATION ${INSTALL_FOLDER_PREFIX}/shaders)

# PRT: filter import libraries (Windows)
set(SRL_PRT_LIBRARIES ${PRT_LIBRARIES})
list(FILTER SRL_PRT_LIBRARIES EXCLUDE REGEX "\\.lib")
install(FILES ${SRL_PRT_LIBRARIES} DESTINATION ${INSTALL_FOLDER_PREFIX}/plug-ins)

# PRT: whitelist required extension libraries
set(SRL_PRT_EXT_LIBRARIES ${PRT_EXT_LIBRARIES})
list(FILTER SRL_PRT_EXT_LIBRARIES INCLUDE REGEX "com\\.esri\\.prt\\.codecs|com\\.esri\\.prt\\.adaptors|VueExport")
install(FILES ${SRL_PRT_EXT_LIBRARIES} DESTINATION ${INSTALL_FOLDER_PREFIX}/plug-ins/ext)

# doc
install(DIRECTORY ${PROJECT_SOURCE_DIR}/../doc DESTINATION ${INSTALL_FOLDER_PREFIX})
install(FILES ${PROJECT_SOURCE_DIR}/../README.md ${PROJECT_SOURCE_DIR}/../LICENSE DESTINATION ${INSTALL_FOLDER_PREFIX})

