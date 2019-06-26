cmake_minimum_required(VERSION 3.13)

add_executable(${TEST_TARGET} tests.cpp)

set_target_properties(${TEST_TARGET} PROPERTIES CXX_STANDARD 14)

target_compile_definitions(${TEST_TARGET} PRIVATE -DSRL_TEST_EXPORTS)

if(WIN32)

else()
	target_compile_options(${TEST_TARGET} PRIVATE
			-D_GLIBCXX_USE_CXX11_ABI=0 -Wl,--exclude-libs,ALL
			-fvisibility=hidden -fvisibility-inlines-hidden)
endif()

target_include_directories(${TEST_TARGET} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${PRT_INCLUDE_PATH})

target_link_libraries(${TEST_TARGET} PRIVATE
        serlio
		serlio_codec
		${PRT_LINK_LIBRARIES}
		${maya_LINK_LIBRARIES})

# copy libraries next to test excutable so they can be found
add_custom_command(TARGET ${TEST_TARGET} POST_BUILD
	COMMAND ${CMAKE_COMMAND} ARGS -E make_directory ${CMAKE_BINARY_DIR}/bin
	COMMAND ${CMAKE_COMMAND} ARGS -E copy ${PRT_LIBRARIES} ${CMAKE_BINARY_DIR}/bin
	COMMAND ${CMAKE_COMMAND} ARGS -E make_directory ${CMAKE_BINARY_DIR}/lib
	COMMAND ${CMAKE_COMMAND} ARGS -E copy ${PRT_EXT_LIBRARIES} ${CMAKE_BINARY_DIR}/lib)
