cmake_minimum_required(VERSION 3.8)


# add the executable
add_executable (test_mp main.cpp)

set_property(TARGET test_mp PROPERTY CXX_STANDARD 14)


if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(test_mp PRIVATE -lKernel32 -lpsapi -lBcrypt )
endif()
if (NOT WIN32) #link pthread
	target_link_options(test_mp PRIVATE -lpthread)
endif()

target_include_directories(test_mp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(test_mp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../../..)

# TOREMOVE
target_link_libraries(test_mp PRIVATE micro_proxy)


install (TARGETS test_mp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )
