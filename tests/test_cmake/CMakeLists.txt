cmake_minimum_required(VERSION 3.8)

# This test assume that micro is installed in build/install folder
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../build/install/lib/cmake")

# Create project
project(cmake_test)

# Find micro package
find_package(micro REQUIRED micro micro_static micro_proxy )

# Print folders
message(STATUS "micro library :${MICRO_STATIC_LIBRARY}")
message(STATUS "micro include: ${MICRO_INCLUDE_DIR}")
message(STATUS "micro binary: ${MICRO_BIN_DIR}")



# Create target using micro_static
add_executable (test_import_static main.cpp)
# Various unrelated stuff
set_property(TARGET test_import_static PROPERTY CXX_STANDARD 14)
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(test_import_static PRIVATE -lKernel32 -lpsapi -lBcrypt )
endif()
if (NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set_target_properties(test_import_static PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# Link with micro_static
target_link_libraries(test_import_static PRIVATE micro_static)
# MANDATORY: add MICRO_STATIC definition to preprocessor
target_compile_definitions(test_import_static PRIVATE -DMICRO_STATIC)
# Install
install (TARGETS test_import_static RUNTIME DESTINATION ${MICRO_BIN_DIR} )



# create target using micro
add_executable (test_import_shared main.cpp)
# Various unrelated stuff
set_property(TARGET test_import_shared PROPERTY CXX_STANDARD 14)
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(test_import_shared PRIVATE -lKernel32 -lpsapi -lBcrypt )
endif()
if (NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set_target_properties(test_import_shared PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
# Link with shared version of micro
target_link_libraries(test_import_shared PRIVATE micro)
# Install
install (TARGETS test_import_shared RUNTIME DESTINATION ${MICRO_BIN_DIR} )


# create target using micro_proxy
add_executable (test_import_proxy main_proxy.cpp)
set_property(TARGET test_import_proxy PROPERTY CXX_STANDARD 14)
# Various unrelated stuff
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(test_import_proxy PRIVATE -lKernel32 -lpsapi -lBcrypt )
endif()
if (NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set_target_properties(test_import_proxy PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
# Link with micro_proxy
target_link_libraries(test_import_proxy PRIVATE micro_proxy)
# Install
install (TARGETS test_import_proxy RUNTIME DESTINATION ${MICRO_BIN_DIR} )
