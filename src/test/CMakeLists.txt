cmake_minimum_required(VERSION 3.13)

add_executable(${TEST_TARGET}
	tests.cpp
	../serlio/PRTContext.cpp
	../serlio/utils/Utilities.cpp
	../serlio/utils/ResolveMapCache.cpp
	../serlio/modifiers/RuleAttributes.cpp)

set_target_properties(${TEST_TARGET} PROPERTIES CXX_STANDARD 14)

target_compile_definitions(${TEST_TARGET} PRIVATE
	-DSRL_TEST_EXPORTS
	-DSERLIO_CODEC_PATH="$<TARGET_FILE:${CODEC_TARGET}>"
	-DTEST_DATA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data")

if (WIN32)

else ()
	target_compile_options(${TEST_TARGET} PRIVATE
		-D_GLIBCXX_USE_CXX11_ABI=0 -Wl,--exclude-libs,ALL
		-fvisibility=hidden -fvisibility-inlines-hidden)

	target_link_libraries(${TEST_TARGET} PRIVATE dl)
endif ()

target_include_directories(${TEST_TARGET} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	$<TARGET_PROPERTY:${SERLIO_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)

srl_add_dependency_prt(${TEST_TARGET})

# copy libraries next to test excutable so they can be found
# TODO: do a proper install of the test executable alongside its dependencies
add_custom_command(TARGET ${TEST_TARGET} POST_BUILD
	COMMAND ${CMAKE_COMMAND} ARGS -E copy ${PRT_LIBRARIES} ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND ${CMAKE_COMMAND} ARGS -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/ext
	COMMAND ${CMAKE_COMMAND} ARGS -E copy ${PRT_EXT_LIBRARIES} ${CMAKE_CURRENT_BINARY_DIR}/ext)

# integrate with ctest so we can use "make test" to run the tests
add_test(NAME ${TEST_TARGET} COMMAND ${TEST_TARGET} -r junit -o ${TEST_TARGET}_report.xml)

# workaround to have a single target to build and run the tests for CI
# Note: in CMake 3.15, the build mode supposedly supports multiple targets

add_custom_target(build_and_run_tests
	# first we build our test target
	COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${TEST_TARGET}
	# then we run the ctest syntetic 'test' target which runs the tests
	COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	COMMENT "Build and run the tests"
	VERBATIM)
