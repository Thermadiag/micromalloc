/**
 * MIT License
 *
 * Copyright (c) 2024 Victor Moncada <vtr.moncada@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MICRO_ENUMS_H
#define MICRO_ENUMS_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
}
#endif

#ifdef small
#undef small
#endif

#ifndef MICRO_MAX_PATH
#ifdef MAX_PATH
#define MICRO_MAX_PATH MAX_PATH
#else
#define MICRO_MAX_PATH 260
#endif
#endif

/// @brief Parameter type, used within micro_set_parameter() and micro_get_parameter()
typedef enum micro_parameter
{
	// Memory allocator internal parameters

	/// @brief Use dedicated memory pools for small allocations (up to 512 bytes, default value)
	MicroSmallAllocThreshold,
	/// @brief allow using the medium allocation radix tree for small allocations if possible. True by default
	MicroAllowSmallAlloxFromRadixTree,
	/// @brief Deplete all other arenas before going through page allocation (true by default)
	MicroDepleteArenas,
	/// @brief Number of arenas, default to hardware concurrency rounded down to a power of 2.
	MicroMaxArenas,

	/// @brief Global memory limit, calls to micro_malloc() or heap::allocate() will return null if we go beyong this limit.
	/// Default to 0 (disabled).
	MicroMemoryLimit,

	/// @brief Backend pages to be kept on deallocation.
	/// If the value is <= 100, it is considered as a percent of currently used memory.
	/// If >= 100, it is considered as a raw maximum number of bytes.
	/// 0 by default (pages are always deallocated when possible)
	MicroBackendMemory,

	// Logging parameters

	/// @brief Log level, default to no log (0)
	MicroLogLevel,
	/// @brief Date format for logging and printing purposes, default to "%Y-%m-%d %H:%M:%S"
	MicroDateFormat,

	// Page provider parameters

	/// @brief Type of page provider, see micro_provider_type enum
	MicroProviderType,

	/// @brief For memory or file page providers, specify the default page size (which can be different to the OS one).
	/// Default to 4096.
	MicroPageSize,
	/// @brief Memory provider address, default to null
	MicroPageMemoryProvider,
	/// @brief Memory provider size, or file provider start size, or preallocated provider size, default to 0
	MicroPageMemorySize,

	/// @brief For MicroOSPreallocProvider, MicroMemProvider and MicroFileProvider,
	/// Allow the use of OS page alloc/dealloc API when the page provider cannot allocate pages anymore.
	/// Default to true.
	MicroAllowOsPageAlloc,

	/// @brief Grow factor for file page provider with the flag MicroGrowing.
	/// This value is expressed in 1/10 and is added to 1 to get the final grow factor.
	/// For instance, a value of 6 means a grow factor of 1.6
	/// Default to 1.6
	MicroGrowFactor,
	/// @brief File name of the file page provider, null by default
	MicroPageFileProvider,
	/// @brief Directory name of the file page provider, null by default.
	/// If not null, it must point to a valid directory. In such case, the MicroPageFileProvider
	/// is interpreted as a file prefix.
	MicroPageFileDirProvider,
	/// @brief Flags for the file page provider, combination of MicroStaticSize, MicroGrowing and MicroRemoveOnClose.
	/// Default to MicroStaticSize.
	MicroPageFileFlags,

	// Print statistics

	/// @brief Filename where statistics are continuously printed during program execution using a CSV format, null by default.
	/// To be used with MicroPrintStatsTrigger, MicroPrintStatsMs and MicroPrintStatsBytes.
	MicroPrintStats,
	/// @brief Used with MicroPrintStats. Defines the type of event(s) that trigger a stats printing.
	/// Combination of MicroNoStats, MicroOnTime, MicroOnBytes.
	/// Default to MicroNoStats.
	MicroPrintStatsTrigger,
	/// @brief If MicroOnTime is set, print stats every MicroPrintStatsMs value.
	MicroPrintStatsMs,
	/// @brief If MicroOnBytes is set, print stats every MicroPrintStatsBytes allocations.
	MicroPrintStatsBytes

} micro_parameter;

/// @brief Type of page provider.
typedef enum micro_provider_type
{
	/// @brief Use OS api to allocate/deallocate pages
	MicroOSProvider,
	/// @brief Use OS api to allocate/deallocate pages, and preallocate a certain amount
	MicroOSPreallocProvider,
	/// @brief Use a memory block to carve pages from
	MicroMemProvider,
	/// @brief Use a memory mapped file to carve pages from
	MicroFileProvider,

} micro_provider_type;

/// @brief File flags used by the internal file page provider.
/// To be used with micro_set_parameter(MicroPageFileFlags).
typedef enum micro_file_flags
{
	/// @brief The file size is bounded to the value provided in init()
	MicroStaticSize = 0,
	/// @brief Allow the file to grow on page demand
	MicroGrowing = 1,
} micro_file_flags;

/// @brief Statistics printing trigger.
/// To be used with micro_set_parameter(MicroPrintStatsTrigger).
typedef enum micro_print_stats_trigger
{
	/// @brief Stats are never printed
	MicroNoStats = 0,
	/// @brief Print Stats on exit
	MicroOnExit = 1,
	/// @brief Stats are printed every N ms
	MicroOnTime = 2,
	/// @brief Stats are printed every N allocated bytes
	MicroOnBytes = 4
} micro_print_stats_trigger;

/// @brief Allowed Logging level.
/// To be used with micro_set_parameter(MicroLogLevel).
typedef enum micro_log_level
{
	MicroNoLog = 0,
	MicroCritical = 1, //! Critical error, usually called before an abort() call
	MicroWarning = 2,  //! Not critical error
	MicroInfo = 3	   //! General information
} micro_log_level;

/// @brief Statistics for allocation class
typedef struct micro_type_statistics
{
	uint64_t alloc_count;	      // total number of allocations
	uint64_t freed_count;	      // total number of deallocations
	uint64_t alloc_bytes;	      // total allocation bytes
	uint64_t freed_bytes;	      // total freed bytes
	uint64_t current_alloc_count; // current number of allocations
	uint64_t current_alloc_bytes; // current allocation bytes
} micro_type_statistics;

/// @brief Full statistics bounded to a heap object.
/// Use micro_dump_stats() or micro::heap::dump_stats().
typedef struct micro_statistics
{
	uint64_t max_used_memory;     // maximum used memory by the memory manager (based on committed pages)
	uint64_t current_used_memory; // currently used memory by the memory manager (based on committed pages)

	uint64_t max_alloc_bytes;	// maximum number of allcoated (requested) bytes
	uint64_t total_alloc_bytes;	// current total number of allocated (requested) bytes
	uint64_t total_alloc_time_ns;	// total allocation time in ns
	uint64_t total_dealloc_time_ns; // total deallocation time in ns
	micro_type_statistics small;
	micro_type_statistics medium;
	micro_type_statistics big;
} micro_statistics;

/// @brief Process information retrieved with micro_get_process_infos()
typedef struct micro_process_infos
{
	size_t current_rss;
	size_t peak_rss;
	size_t current_commit;
	size_t peak_commit;
	size_t page_faults;
} micro_process_infos;

#endif
