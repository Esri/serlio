cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0015 NEW)


### setup build target

add_library(${CODEC_TARGET} SHARED
	CodecMain.cpp
	encoder/MayaEncoder.cpp)

if (CMAKE_GENERATOR MATCHES "Visual Studio.+")
	target_sources(${CODEC_TARGET}
		PRIVATE
		CodecMain.h
		encoder/IMayaCallbacks.h)
endif ()


### setup compiler flags

target_include_directories(${CODEC_TARGET}
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE ${PRT_INCLUDE_PATH})

target_compile_definitions(${CODEC_TARGET} PRIVATE
	-DSRL_VERSION=\"${SRL_VERSION}\" # quoted to use it as string literal
	-DPRT_VERSION_MAJOR=${PRT_VERSION_MAJOR}
	-DPRT_VERSION_MINOR=${PRT_VERSION_MINOR})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_compile_options(${CODEC_TARGET} PRIVATE -bigobj -GR -EHsc)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	target_compile_options(${CODEC_TARGET} PRIVATE -std=c++14 -stdlib=libc++ -fvisibility=hidden -fvisibility-inlines-hidden -Wl,--exclude-libs,ALL)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	target_compile_options(${CODEC_TARGET} PRIVATE -std=c++14 -D_GLIBCXX_USE_CXX11_ABI=0 -march=nocona -fvisibility=hidden -fvisibility-inlines-hidden -Wl,--exclude-libs,ALL)
endif ()

target_link_libraries(${CODEC_TARGET} ${PRT_LINK_LIBRARIES})

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_link_libraries(${CODEC_TARGET} IPHlpApi Psapi DbgHelp)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	# none
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	target_link_libraries(${CODEC_TARGET} z pthread dl rt)
endif ()

# Setup solution folder (for Visual Studio Solution)
set_property(TARGET ${CODEC_TARGET} PROPERTY FOLDER "Serlio Maya Plugin")

### setup install target

# TODO: below line is sufficient with CMake >= 3.14
#install(TARGETS ${CODEC_TARGET} RUNTIME DESTINATION ${INSTALL_FOLDER_PREFIX}/plug-ins/ext)
if (SRL_WINDOWS)
	install(TARGETS ${CODEC_TARGET} RUNTIME DESTINATION ${INSTALL_FOLDER_PREFIX}/plug-ins/ext)
else ()
	install(TARGETS ${CODEC_TARGET} LIBRARY DESTINATION ${INSTALL_FOLDER_PREFIX}/plug-ins/ext)
endif ()