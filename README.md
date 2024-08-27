
Purpose
-------

*Micro* is a general purpose allocator that focuses on low memory overhead.

It's key features are: 
-	Low memory overhead and reduced fragmentation based on a combination of best-fit and segregated-fit,
-	O(1) allocation and deallocation,
-	Possibility to create independant heap objects and free all allocated memory in one call,
-	Can allocate memory from preallocated buffers or files using file mapping,
-	Scale well with the number of cores,
-	C++ and C interfaces are provided,
-	Works on Windows and most Unix systems,
-	Can work on systems providing a few MBs only.

The micro library behaves quite well compared to default allocators on Windows (Low-fragmentation Heap) and Linux (glibc implementation) as it often outperforms them in both space and time.
Micro is usually (but not always) slower than well known allocators like *mimalloc*, *snmalloc*, *tbbmalloc* or *jemalloc*, but consumes far less memory in all tested use cases.

The library is written in C++14 and should compile on most platforms with gcc, clang or msvc. A header only mode is provided for quick benchmarking. 

A few benchmarks are available [here](md/benchmarks.md).

C and C++ interfaces
--------------------

Micro library is written in C++ but the compiled library (static or shared) can be used from C only codes.
For pure C code, include `micro/micro.h` header. For C/C++ code, `micro/micro.hpp` should be used as it provides both C and C++ interfaces.
See the [examples](md/examples.md) page for more details.

Furthermore, the `micro` folder can be directly included in a foreign C++ application without compilation (header-only mode). In this case, the macro `MICRO_HEADER_ONLY` must be defined.

Replacing system allocator
--------------------------

Micro provides the usual hugly tricks to replace the system allocator on Windows and Unix systems. Not that it has not been tested on MacOS and Android yet.
You should link with the *micro_proxy* library for that instead of the default *micro* or *micro_static* ones. See this [cmake file](tests/test_cmake/CMakeLists.txt) example for more details.

In addition, the [mp](md/mp.md) command line utility is provided for quick benchmarking of the allocator on foreign applications. It basically uses LD_PRELOAD on Linux, DYLD_INSERT_LIBRARIES on MacOS and DLL injection on Windows to force the application to link with *micro_proxy*.


(Brief) design overview
-----------------------

The micro library divides allocations in 3 types:
-	Big: above 512k
-	Medium: under 512k
-	Small: under 656 bytes.

For big allocations, pages are simply mapped/unmap on malloc/free calls, like most other allocators.

For medium allocation, the library uses a 2 level radix tree with fine grained locking that stores free chunks sorted by size. Each chunk (allocated or free) have a 16 bytes header storing various information like its size and the position of previous block.
Free chunks are immediately splitted/merged based on requested allocations/deallocations. Therefore, medium allocations use a **best-fit with immediate coalescing policy**. Allocation and deallocation have a O(1) complexity, and support a first level of parallelism thanks to the radix tree design.
This approach is quite similar to the *Two-Level Segregated Fit* (TLSF) but provides a true best-fit policy instead of an approximation.

Small allocations use a standard segregated-fit strategy with 16 bytes spaced size classes. A memory block for a specific size class has a size of at most 4096 bytes and is aligned on 4096 bytes. This removed the need of the chunk header as the block header is retrieved with address masking.
One original detail compared to other allocators is that size class blocks are allocated from the radix tree using the medium allocation strategy. Therefore, they benefit from the radix tree parallelism, and small leftover memory chunks due to aligned allocations can be recycled for other small allocations.

The micro library uses (almost) independant arenas to increase allocation scalability based on the number of CPU cores. Each thread is attached to an arena for its lifetime in a round-robin way, and will only use other arenas when its own arena is depleted (before refuelling it with fresh pages).
Thread local caches are avoided by design as they tend to greatly increase the memory overhead with heavily multithreaded applications. By default, the number of arenas is equal to the number of CPU cores, rounded down to the previous power of 2.

By default, the micro library allocates pages by block of 512k, does not rely on memory overcommitment, and does not over align allocated pages. This allows to use it on preallocated buffers or even on files using OS file mapping utilities (see [examples](md/examples.md)).

All allocations are 16 bytes aligned.

See the [examples](md/examples.md) for more information on the library usage.

Not that the micro library does **NOT** embed security features against heap exploitation (maybe in a future version).

Memory usage
------------

The micro library provides 5 possible profiles (selected at compile time) corresponding to different use cases:
-	**Level 0**: suitable for low memory systems. The library will allocate pages by runs of 64k only. The maximum radix tree size is bounded to 64k free chunks as well. All allocations above 64k directly call map/unmap. The maximum number of arenas is bounded to 4. Small allocations start at 256 bytes.
-	**Level 1**: suitable for low capability computers. The library will allocate pages by runs of 262k. The maximum number of arenas is bounded to 16. Small allocations start at 512 bytes. This level has the lowest memory overhead.
-	**Level 2** (default): for any kind of usage on modern platform. The library will allocate pages by runs of 524k. The maximum number of arenas is bounded to 32. Small allocations start at 656 bytes.
-	**Level 3 and 4**: the library will allocate pages by runs of 1MB and 2MB. Mechanims used to reduced the memory footprint (like arena depletion) are reduced. These levels are overall faster but will consume more memory. The level 4 is provided for future integration of huge page support.

The memory level is passed to cmake as a compilation option and default to 2.
Note that the radix tree has a static memory cost per arena that will increase with the memory level. See function `micro_max_static_cost_per_arena()` to get an estimation of this cost per arena.

Configuration
-------------

The library offers several ways to configure the global heap or independant ones.
The C functions `micro_set_parameter()` and `micro_set_string_parameter()` configure the global heap programmatically. The functions `micro_heap_set_parameter()` and `micro_heap_set_string_parameter()` configure the local heaps. See the [examples](md/examples.md) for more details.

By default, the global heap is configured based on the following environment variables (with default values):
-	**MICRO_SMALL_ALLOC_THRESHOLD**(656): max size in bytes for small allocations. Setting to 0 disable the segregated-fit policy (only medium and big allocations are used).
-	**MICRO_SMALL_ALLOC_FROM_RADIX_TREE**(1): enable small allocations to use the radix tree if no free chunk is found for the corresponding size class.
-	**MICRO_DEPLETE_ARENAS**(1): allow arenas to deplete other arenas before allocating fresh pages. This greatly reduces the memory usage at the expense of parallelism.
-	**MICRO_MAX_ARENAS** (CPU based): maximum number of arenas per heap. the default value depends on the memory level.
-	**MICRO_DISABLE_REPLACEMENT(0)**: disable malloc replacement, Windows only.
-	**MICRO_BACKEND_MEMORY**(0): backend pages to be kept on deallocation. If the value is <= 100, it is considered as a percent of currently used memory. If >= 100, it is considered as a raw maximum number of bytes.
-	**MICRO_MEMORY_LIMIT**(0): memory usage limit in bytes that cannot bypass the heap (0 to disable).
-	**MICRO_LOG_LEVEL**(0): library logging level (0 to disable).
-	**MICRO_LOG_DATE_FORMAT**: date format for logging various information as well as statistics. Default to "%Y-%m-%d %H:%M:%S".
-	**MICRO_PAGE_SIZE**(4096): custom page size used for allocations from a buffer or a file.
-	**MICRO_GROW_FACTOR**(1.6): grow factor used when allocating from a file while file growing is enabled (see MICRO_PAGE_FILE_FLAGS).
-	**MICRO_PROVIDER_TYPE**(0): type of memory provider (or page provider):
	-	*MicroOSProvider*(0): Standard OS based page provider.
	-	*MicroOSPreallocProvider*(1): Use OS API to allocate/deallocate pages, and preallocate a certain amount (defined by MICRO_PAGE_MEMORY_SIZE).
	-	*MicroMemProvider*(2): Use a memory buffer to carve pages from. In this case, the heap should be configured programmatically using `micro_set_parameter()` and `micro_set_string_parameter()`.
	-	*MicroFileProvider*(3): Use a memory mapped file to carve pages from. Use MICRO_PAGE_FILE_PROVIDER and MICRO_PAGE_FILE_PROVIDER_DIR for the filename and MICRO_PAGE_FILE_FLAGS for advanced configuration.
-	**MICRO_PAGE_FILE_PROVIDER**(null): filename for the page file provider. If null (and MICRO_PAGE_FILE_PROVIDER_DIR is null), a temporary file is created. You should use this parameter with great care, as any spawn process will use the same filename (certain crash).
-	**MICRO_PAGE_FILE_PROVIDER_DIR**(null): directory name for the page file provider. If not null, the file page provider will create a filename combining the directory name and MICRO_PAGE_FILE_PROVIDER (if not null) as file prefix. If MICRO_PAGE_FILE_PROVIDER is null, a generated file name is used. 
	Note that MICRO_PAGE_FILE_PROVIDER_DIR is the preffered way to use file provider as it should always work regardless of spawn processes.
-	**MICRO_PAGE_MEMORY_SIZE**: start file size for file page provider (MICRO_PROVIDER_TYPE is 3), or preallocated size (MICRO_PROVIDER_TYPE is 1)
-	**MICRO_PAGE_FILE_FLAGS**: configuration flags for the file page provider. Combination of:
	-	*MicroStaticSize*(0, default): The file has a static size defined by MICRO_PAGE_MEMORY_SIZE and cannot grow. 
	-	*MicroGrowing*(1): Allow the file to grow on page demand.
-	**MICRO_ALLOW_OS_PAGE_ALLOC**(1): to be used with MICRO_PROVIDER_TYPE > 0. If set to 1, allow the usage of OS API to allocate pages when the underlying page provider is full.
-	**MICRO_PRINT_STATS**(null): output stream to print statistics. Can be set to "stdout", "stderr", or any filename to output statistics to a file.
-	**MICRO_PRINT_STATS_TRIGGER**(0): defines on which event(s) statistics are printed. Combination of:
	-	*MicroNoStats*(0): statistics are never printed.
	-	*MicroOnExit*(1): statistics are printed at program exit.
	-	*MicroOnTime*(2): statistics are printed every MICRO_PRINT_STATS_MS milliseconds (on allocation only).
	-	*MicroOnBytes*(4): statistics are printed every MICRO_PRINT_STATS_BYTES allocated bytes.
-	**MICRO_PRINT_STATS_MS**: used if (MICRO_PRINT_STATS_TRIGGER & MicroOnTime) != 0
-	**MICRO_PRINT_STATS_BYTES**: used if (MICRO_PRINT_STATS_TRIGGER & MicroOnBytes) != 0
-	**MICRO_PRINT_STATS_CSV**: still experimental, print statistics in CSV format

Build
-----

Micro relies on cmake for building and integration in other projects. List of all options with their default values:
-	**MICRO_MEMORY_LEVEL("2")**: library memory level, as seen above
-	**MICRO_BUILD_SHARED(ON)**: build the shared version of micro
-	**MICRO_BUILD_STATIC(ON)**: build the static version of micro
-	**MICRO_BUILD_PROXY(ON)**: build the proxy version of micro for malloc replacement
-	**MICRO_BUILD_TESTS(ON)**: build the test executables
-	**MICRO_BUILD_BENCHS(ON)**: build the benchmark executable
-	**MICRO_BUILD_TOOLS(ON)**: build the tools executables (currently only the *mp* one)
-	**MICRO_ENABLE_ASSERT(OFF)**: enable release assertions to detect possible memory corruptions even on release builds
-	**MICRO_ZERO_MEMORY(OFF)**: zero memory on allocation
-	**MICRO_NO_FILE_MAPPING(OFF)**: disable the file mapping support for allocation from files
-	**MICRO_BENCH_MALLOC(ON)**: add standard malloc to benchmarks
-	**MICRO_BENCH_MIMALLOC(ON)**: add mimalloc to benchmarks
-	**MICRO_BENCH_SNMALLOC(OFF)**: add snmalloc to benchmarks
-	**MICRO_BENCH_TBB(ON)**: add tbbmalloc to benchmarks, currently Windows only
-	**MICRO_BENCH_JEMALLOC_PATH("")**: add jemalloc to benchmarks by setting the installation path (jemalloc must be built locally, linux only)
-	**MICRO_NO_WARNINGS(OFF)**: Treat warnings as errors
-	**MICRO_ENABLE_TIME_STATISTICS(OFF)**: Enable time statistics (get average allocation/deallocation time and maximum ones)
-	**MICRO_NO_LOCK(OFF)**: Disable all locking mechanisms for monothreaded systems

See this [cmake file](tests/test_cmake/CMakeLists.txt) file for examples of targets using either *micro*, *micro_static* or *micro_proxy* libraries.

Note that if using *micro_static*, *MICRO_STATIC* must be defined.


Benchmarks
----------

The library embedds several benchmarks in the `benchs` folder.
Some of them are presented in the [benchmarks](md/benchmarks.md) page.

Remaining work
--------------

The micro library is not as complete as other general purpose allocators:
-	Micro library was not tested on many OS, including MacOS and Android.
-	The test suite is not complete and may need extra testing features.
-	The library does not embed security features against heap exploitation.
-	Huge pages are not yet supported.

Some of these points might be addressed in the future. The library was developped for my current work projects, and additional developments will be likely driven by these projects.

In any case, all contributions are more than welcome.

Acknowledgements
----------------

*Micro* library uses a simplified version of the [komihash](https://github.com/avaneev/komihash) hash function. The library was developped with ideas comming from several other generic allocators:
-	<a href="https://github.com/microsoft/mimalloc">mimalloc</a>,
-	<a href="https://github.com/oneapi-src/oneTBB/tree/master">tbbmalloc</a> which proxy method was used.

The benchmarks can include the additional following libraries:
-	<a href="https://github.com/jemalloc/jemalloc">jemalloc</a>,
-	<a href="https://github.com/microsoft/snmalloc">snmalloc</a>.

The benchmarks include code from:
-	<a href="https://github.com/daanx/mimalloc-bench">mimalloc-bench</a>,
-	<a href="https://github.com/mjansson/rpmalloc-benchmark">rpmalloc-benchmark</a>.
-	<a href="https://github.com/r-lyeh-archived/malloc-survey/tree/master">malloc-survey</a>.
-	<a href="https://github.com/node-dot-cpp/alloc-test/tree/master">alloc-test</a>.


micro:: library and this page Copyright (c) 2024, Victor Moncada
