cmake_minimum_required(VERSION 3.8)


# add the executable
add_executable (override main.cpp)

set_property(TARGET override PROPERTY CXX_STANDARD 14)


if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(override PRIVATE -lKernel32 -lpsapi -lBcrypt )
endif()

if (NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set_target_properties(override PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
if (NOT WIN32) #link pthread
	target_link_options(override PRIVATE -lpthread)
endif()

target_include_directories(override PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(override PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../../..)

target_link_libraries(override PRIVATE micro_proxy)


install (TARGETS override RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )
