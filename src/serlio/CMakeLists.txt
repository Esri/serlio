cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0015 NEW)


### setup build target

add_library(${SERLIO_TARGET} SHARED
		serlioPlugin.cpp
		prtModifier/MayaCallbacks.cpp
        prtModifier/PRTModifierAction.cpp
        prtModifier/PRTModifierCommand.cpp
        prtModifier/PRTModifierNode.cpp
		prtModifier/polyModifier/polyModifierCmd.cpp
		prtModifier/polyModifier/polyModifierFty.cpp
		prtModifier/polyModifier/polyModifierNode.cpp
		prtMaterial/PRTMaterialNode.cpp
		util/Utilities.cpp
		util/ResolveMapCache.cpp)

set_target_properties(${SERLIO_TARGET} PROPERTIES
	INSTALL_RPATH_USE_LINK_PATH FALSE
	SKIP_RPATH FALSE
	INSTALL_RPATH "\$ORIGIN/")

if(WIN32)
    # On Windows, Maya requires plugins to have the extension "mll"
    set_target_properties(${SERLIO_TARGET} PROPERTIES SUFFIX ".mll")
endif()


### setup compiler flags

target_compile_definitions(${SERLIO_TARGET} PRIVATE
	-DSRL_VERSION=\"${SRL_VERSION}\") # quoted to use it as string literal

target_include_directories(${SERLIO_TARGET} PRIVATE
		$<TARGET_PROPERTY:${CODEC_TARGET},INTERFACE_INCLUDE_DIRECTORIES> # for IMayaCallbacks.h
		${CMAKE_CURRENT_SOURCE_DIR}
		${PRT_INCLUDE_PATH}
		${maya_INCLUDE_PATH})

target_link_libraries(${SERLIO_TARGET} PRIVATE
		${maya_LINK_LIBRARIES}
		${PRT_LINK_LIBRARIES})

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
			-std=c++11 -D_GLIBCXX_USE_CXX11_ABI=0 -Wl,--exclude-libs,ALL
			-fvisibility=hidden -fvisibility-inlines-hidden
			-m64 -fPIC -fno-strict-aliasing -funsigned-char -Wno-reorder -fno-gnu-keywords -ftemplate-depth=50 -pthread
			-Wall -Wextra -Wno-deprecated -Wno-multichar -Wno-comment -Wno-sign-compare
			$<$<CONFIG:DEBUG>:-ggdb>
			$<$<CONFIG:RELEASE>:-O2 -s>)

	target_compile_definitions(${SERLIO_TARGET} PRIVATE
			-DBits64_ -DUNIX -D_BOOL -DLINUX -DFUNCPROTO -D_GNU_SOURCE -DLINUX_64 -DREQUIRE_IOSTREAM)
endif()


### setup install target

install(TARGETS ${SERLIO_TARGET} DESTINATION plug-ins)
install(FILES ${PRT_LIBRARIES} DESTINATION plug-ins)
install(FILES ${PRT_EXT_LIBRARIES} DESTINATION plug-ins/ext)
install(FILES scripts/AEserlioTemplate.mel DESTINATION scripts)
install(FILES scripts/serlioCreateUI.mel DESTINATION scripts)
install(FILES scripts/serlioDeleteUI.mel DESTINATION scripts)
install(FILES shaders/serlioShaderStingray.sfx DESTINATION shaders)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/../doc DESTINATION .)
install(FILES ${PROJECT_SOURCE_DIR}/../README.md ${PROJECT_SOURCE_DIR}/../LICENSE DESTINATION .)
