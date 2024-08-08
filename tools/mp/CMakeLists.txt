cmake_minimum_required(VERSION 3.8)


# add the executable
add_executable (mp mp.cpp)

set_property(TARGET mp PROPERTY CXX_STANDARD 14)


if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(mp PRIVATE -lKernel32 -lpsapi -lBcrypt )
endif()


target_include_directories(mp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(mp PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../../..)



install (TARGETS mp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )