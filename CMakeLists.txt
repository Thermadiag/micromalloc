cmake_minimum_required(VERSION 3.16)

project(micromalloc
    VERSION 2.1
    DESCRIPTION "Low memory overhead allocator"
    HOMEPAGE_URL "https://github.com/Thermadiag/micro"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(GNUInstallDirs) 

# project options
option(MICRO_BUILD_SHARED "Build shared library" ON)
option(MICRO_BUILD_STATIC "Build static library" ON)
option(MICRO_BUILD_PROXY "Build proxy library" ON)
option(MICRO_BUILD_TESTS "Build tests" ON)
option(MICRO_BUILD_BENCHS "Build benchmarks" ON)
option(MICRO_BUILD_TOOLS "Build tools" ON)
option(MICRO_ENABLE_ASSERT "Enable release asserts" OFF)
option(MICRO_ZERO_MEMORY "Zero memory on allocation" OFF)
option(MICRO_NO_FILE_MAPPING "Disable file mapping support" OFF)
option(MICRO_BENCH_MALLOC "Add standard malloc to benchmarks" ON)
option(MICRO_BENCH_MIMALLOC "Add mimalloc to benchmarks" OFF)
option(MICRO_BENCH_SNMALLOC "Add snmalloc to benchmarks" OFF)
option(MICRO_BENCH_JEMALLOC_PATH "Add jemalloc to benchmarks" "")
option(MICRO_BENCH_TBB "Add OneTBB to benchmarks" OFF)
option(MICRO_NO_WARNINGS "Treat warnings as errors" OFF)
option(MICRO_ENABLE_TIME_STATISTICS "Enable time statistics" OFF) 
option(MICRO_NO_LOCK "Disable multithreading support for monothreaded systems" OFF)
#option(MICRO_MEMORY_LEVEL "Memory level from 0 to 4" "2") 
set(MICRO_MEMORY_LEVEL "2" CACHE STRING "Memory level from 0 to 4")

# create the testing file and list of tests
file(GLOB MicroSources
     "micro/*.h"
	 "micro/*.hpp"
	 "micro/internal/*.h"
	 "micro/internal/*.hpp"
     "micro/internal/*.cpp" 
)

# Disable header only mode
set(PROJECT_HEADER_ONLY 0)

set(MICRO_INSTALL_LIBDIR       "${CMAKE_INSTALL_LIBDIR}")
set(MICRO_INSTALL_BINDIR       "${CMAKE_INSTALL_BINDIR}")
set(MICRO_INSTALL_DATAROOTDIR  "${CMAKE_INSTALL_DATAROOTDIR}")
set(MICRO_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/micro")

message(STATUS "MICRO_MEMORY_LEVEL=${MICRO_MEMORY_LEVEL}")

if(MICRO_BUILD_SHARED)
	# add sources
	add_library(micro SHARED 
		${MicroSources}
	)

	# add preprocessor definitions
	target_compile_definitions(micro PRIVATE -DMICRO_BUILD_SHARED_LIBRARY)
	
	#add linker options
	if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# mingw
		target_link_options(micro PRIVATE -lKernel32 -lpsapi -lBcrypt)
	endif()

	if (NOT WIN32) #link pthread
		target_link_options(micro PRIVATE -lpthread)
	endif()

	if(WIN32)
		target_compile_definitions(micro PRIVATE -D_CRT_SECURE_NO_WARNINGS)
	endif()
	
	if(MICRO_ENABLE_TIME_STATISTICS)
		target_compile_definitions(micro PRIVATE -DMICRO_ENABLE_TIME_STATISTICS)
	endif()
	
	if(MICRO_NO_LOCK)
		target_compile_definitions(micro PRIVATE -DMICRO_NO_LOCK)
	endif()
	
	if(MICRO_ENABLE_ASSERT)
		target_compile_definitions(micro PRIVATE -DMICRO_ENABLE_ASSERT)
	endif()
	
	if(MICRO_ZERO_MEMORY)
		target_compile_definitions(micro PRIVATE -DMICRO_ZERO_MEMORY)
	endif()

	if(MICRO_NO_FILE_MAPPING)
		target_compile_definitions(micro PRIVATE -DMICRO_NO_FILE_MAPPING)
	endif()

	target_compile_definitions(micro PRIVATE MICRO_MEMORY_LEVEL=${MICRO_MEMORY_LEVEL})
	
	if(MICRO_NO_WARNINGS)
		if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
			target_compile_options(micro PRIVATE  /WX /W3 )
		elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			target_compile_options(micro PRIVATE -Werror -Wall -Wno-c++98-compat -Wno-c++98-compat-pedantic)
		else()
			target_compile_options(micro PRIVATE -Werror -Wall)
		endif()
	endif()

	target_include_directories(micro PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/micro>"
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/micro/internal>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
						   
	
	#test
	install(TARGETS micro EXPORT micro ARCHIVE DESTINATION ${MICRO_INSTALL_LIBDIR} RUNTIME DESTINATION ${MICRO_INSTALL_BINDIR} LIBRARY DESTINATION ${MICRO_INSTALL_LIBDIR})
	install(EXPORT micro DESTINATION ${MICRO_CMAKEDIR})
	
endif()


if(MICRO_BUILD_PROXY)
	# add sources
	file(GLOB ProxySources
     "micro_proxy/*.h"
     "micro_proxy/*.cpp"
	)
	add_library(micro_proxy SHARED 
		${MicroSources} ${ProxySources}
	)

	set_property(TARGET micro_proxy PROPERTY CXX_STANDARD 14)

	target_compile_definitions(micro_proxy PRIVATE -DMICRO_PROXY_BUILD_LIBRARY -DMICRO_BUILD_SHARED_LIBRARY -DMICRO_OVERRIDE)
	target_compile_definitions(micro_proxy PUBLIC -DMICRO_OVERRIDE)

	if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# mingw
		target_link_options(micro_proxy PRIVATE -lKernel32 -lpsapi -lBcrypt)
	elseif(UNIX AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
		target_link_options(micro_proxy PRIVATE  -lpthread -ldl )
	endif()

	if(WIN32)
		target_compile_definitions(micro_proxy PRIVATE -D_CRT_SECURE_NO_WARNINGS)
	endif()

	if(MICRO_ENABLE_TIME_STATISTICS)
		target_compile_definitions(micro_proxy PRIVATE -DMICRO_ENABLE_TIME_STATISTICS)
	endif()
	
	if(MICRO_NO_LOCK)
		target_compile_definitions(micro_proxy PRIVATE -DMICRO_NO_LOCK)
	endif()
	
	if(MICRO_ENABLE_ASSERT)
		target_compile_definitions(micro_proxy PRIVATE -DMICRO_ENABLE_ASSERT)
	endif()
	
	if(MICRO_ZERO_MEMORY)
		target_compile_definitions(micro_proxy PRIVATE -DMICRO_ZERO_MEMORY)
	endif()
	
	if(MICRO_NO_FILE_MAPPING)
		target_compile_definitions(micro_proxy PRIVATE -DMICRO_NO_FILE_MAPPING)
	endif()

	target_compile_definitions(micro_proxy PRIVATE MICRO_MEMORY_LEVEL=${MICRO_MEMORY_LEVEL})

	target_include_directories(micro_proxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_include_directories(micro_proxy PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../..)

	if(MICRO_NO_WARNINGS)
		if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
			target_compile_options(micro_proxy PRIVATE  /WX /W3 )
		elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			target_compile_options(micro_proxy PRIVATE -Werror -Wall -Wno-c++98-compat -Wno-c++98-compat-pedantic)
		else()
			target_compile_options(micro_proxy PRIVATE -Werror -Wall)
		endif()
	endif()

	target_include_directories(micro_proxy PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/micro>"
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/micro/internal>"
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/micro_proxy>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
	
	# Install headers 
	install(FILES  "${CMAKE_CURRENT_SOURCE_DIR}/micro_proxy/micro_proxy.h" "${CMAKE_CURRENT_SOURCE_DIR}/micro_proxy/proxy_export.h"
		DESTINATION "${CMAKE_INSTALL_PREFIX}/include/micro/")
	
	#test
	install(TARGETS micro_proxy EXPORT micro ARCHIVE DESTINATION ${MICRO_INSTALL_LIBDIR} RUNTIME DESTINATION ${MICRO_INSTALL_BINDIR} LIBRARY DESTINATION ${MICRO_INSTALL_LIBDIR} )
	install(EXPORT micro DESTINATION ${MICRO_CMAKEDIR})
	install(TARGETS micro_proxy LIBRARY DESTINATION ${MICRO_INSTALL_BINDIR})
endif()


if(MICRO_BUILD_STATIC)
	# add sources
	add_library(micro_static STATIC 
		${MicroSources}
	)
	set_property(TARGET micro_static PROPERTY POSITION_INDEPENDENT_CODE ON)
	# add preprocessor definitions
	target_compile_definitions(micro_static PRIVATE -DMICRO_BUILD_STATIC_LIBRARY)
	
	#add linker options
	if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		# mingw
		target_link_options(micro_static  PRIVATE -lKernel32 -lpsapi -lBcrypt)
	endif()
	if (NOT WIN32) #link pthread
		target_link_options(micro PRIVATE -lpthread)
	endif()

	if(WIN32)
		target_compile_definitions(micro_static PRIVATE -D_CRT_SECURE_NO_WARNINGS)
	endif()

	if(MICRO_ENABLE_TIME_STATISTICS)
		target_compile_definitions(micro_static PRIVATE -DMICRO_ENABLE_TIME_STATISTICS)
	endif()
	
	if(MICRO_NO_LOCK)
		target_compile_definitions(micro_static PRIVATE -DMICRO_NO_LOCK)
	endif()
	
	if(MICRO_ENABLE_ASSERT)
		target_compile_definitions(micro_static PRIVATE -DMICRO_ENABLE_ASSERT)
	endif()
	
	if(MICRO_ZERO_MEMORY)
		target_compile_definitions(micro_static PRIVATE -DMICRO_ZERO_MEMORY)
	endif()
	
	if(MICRO_NO_FILE_MAPPING)
		target_compile_definitions(micro_static PRIVATE -DMICRO_NO_FILE_MAPPING)
	endif()

	target_compile_definitions(micro_static PRIVATE MICRO_MEMORY_LEVEL=${MICRO_MEMORY_LEVEL})
	
	if(MICRO_NO_WARNINGS)
		if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
			target_compile_options(micro_static  PRIVATE  /WX /W3 )
		elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			target_compile_options(micro_static  PRIVATE -Werror -Wall -Wno-c++98-compat -Wno-c++98-compat-pedantic)
		else()
			target_compile_options(micro_static  PRIVATE -Werror -Wall)
		endif()
	endif()

	target_include_directories(micro_static  PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/micro>"
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/micro/internal>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
						   
						   
	#test
	install(TARGETS micro_static EXPORT micro DESTINATION ${MICRO_INSTALL_LIBDIR} LIBRARY)
	install(EXPORT micro DESTINATION ${MICRO_CMAKEDIR})
endif()





# add micro_proxy
# add_subdirectory(micro_proxy)

# add tools
if(MICRO_BUILD_TOOLS)
	add_subdirectory(tools)
endif()

# add tests
enable_testing()

if(MICRO_BUILD_TESTS)
	add_subdirectory(tests)
endif()

# add benchmarks
if(MICRO_BUILD_BENCHS)
	add_subdirectory(benchs)
endif()

# Install headers and sources (for header-only mode)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/micro"
		DESTINATION "${CMAKE_INSTALL_PREFIX}/include")



# Configure and install micro.pc
configure_file(micro.pc.in micro.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/micro.pc
			DESTINATION ${MICRO_INSTALL_DATAROOTDIR}/pkgconfig)



# Configure and install micro_config.hpp
configure_file(micro_config.hpp.in micro_config.hpp @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/micro_config.hpp
			DESTINATION "${CMAKE_INSTALL_PREFIX}/include/micro")

# Configure and install microConfig.cmake and microConfigVersion.cmake
include(CMakePackageConfigHelpers)


configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/microConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/microConfig.cmake"
  INSTALL_DESTINATION ${MICRO_INSTALL_LIBDIR}/cmake/micro
  PATH_VARS )

  
# Generate microConfigVersion.cmake
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/microConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion )
  
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/microConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/microConfigVersion.cmake
        DESTINATION ${MICRO_INSTALL_LIBDIR}/cmake/micro )
		
