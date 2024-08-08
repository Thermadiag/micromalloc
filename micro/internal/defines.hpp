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

#ifndef MICRO_DEFINES_HPP
#define MICRO_DEFINES_HPP

#include "../bits.hpp"
#include "../micro_config.hpp"

// Memory level, impact the default block size and maximum radix tree size
#ifndef MICRO_MEMORY_LEVEL
#define MICRO_MEMORY_LEVEL 2
#endif

// Header size for memory block in the radix tree
#define MICRO_HEADER_SIZE 16u
// Shift value for multiplication/division by MICRO_HEADER_SIZE
#define MICRO_ELEM_SHIFT 4u

#if MICRO_MEMORY_LEVEL == 4

// Default page size when allocating from preallocated memory or from a file
#define MICRO_DEFAULT_PAGE_SIZE 4096
#define MICRO_MAX_SMALL_SIZE 800
#define MICRO_MAX_RADIX_SIZE 17u
#define MICRO_BLOCK_SIZE (1048576ull * 2)
#define MICRO_MAX_ARENAS 128u
#define MICRO_TINY_POOL_CACHE 2
#define MICRO_DEPLETE_ARENA_FACTOR 4

#elif MICRO_MEMORY_LEVEL == 3

#define MICRO_DEFAULT_PAGE_SIZE 4096
#define MICRO_MAX_SMALL_SIZE 800
#define MICRO_MAX_RADIX_SIZE 16u
#define MICRO_BLOCK_SIZE (1048576ull)
#define MICRO_MAX_ARENAS 64u
#define MICRO_TINY_POOL_CACHE 0
#define MICRO_DEPLETE_ARENA_FACTOR 2

#elif MICRO_MEMORY_LEVEL == 2

#define MICRO_DEFAULT_PAGE_SIZE 4096
#define MICRO_MAX_SMALL_SIZE 656
#define MICRO_MAX_RADIX_SIZE 15u
#define MICRO_BLOCK_SIZE (1048576ull / 2)
#define MICRO_MAX_ARENAS 32u
#define MICRO_TINY_POOL_CACHE 0
#define MICRO_DEPLETE_ARENA_FACTOR 1

#elif MICRO_MEMORY_LEVEL == 1

#define MICRO_DEFAULT_PAGE_SIZE 4096
#define MICRO_MAX_SMALL_SIZE 512
#define MICRO_MAX_RADIX_SIZE 14u
#define MICRO_BLOCK_SIZE (1048576ull / 4)
#define MICRO_MAX_ARENAS 16u
#define MICRO_TINY_POOL_CACHE 0
#define MICRO_DEPLETE_ARENA_FACTOR 1

#elif MICRO_MEMORY_LEVEL == 0

#define MICRO_DEFAULT_PAGE_SIZE 2048
#define MICRO_MAX_SMALL_SIZE 256
#define MICRO_MAX_RADIX_SIZE 12u
// 16 pages of 4096 bytes. On windows, this corresponds to the default allocation granularity.
#define MICRO_BLOCK_SIZE (1048576ull / 16)
#define MICRO_MAX_ARENAS 4u
#define MICRO_TINY_POOL_CACHE 0
#define MICRO_DEPLETE_ARENA_FACTOR 1

#endif

#define MICRO_MAX_SMALL_ALLOC_THRESHOLD MICRO_MAX_SMALL_SIZE

// Minimum/maximum supported page size
#define MICRO_MINIMUM_PAGE_SIZE MICRO_DEFAULT_PAGE_SIZE
#define MICRO_MAXIMUM_PAGE_SIZE 65536

// Alignment for a block of tiny objects
#define MICRO_ALIGNED_POOL (MICRO_MINIMUM_PAGE_SIZE)

// Allow allocations from the radix tree for small objects
#ifndef MICRO_ALLOW_SMALL_ALLOC_FROM_RADIX_TREE
#define MICRO_ALLOW_SMALL_ALLOC_FROM_RADIX_TREE true
#endif

// Grow factor when allocating from a file
#ifndef MICRO_DEFAULT_GROW_FACTOR
#define MICRO_DEFAULT_GROW_FACTOR 1.6
#endif

// Disable usage of last available chunk
#define MICRO_ALLOC_FROM_LAST 0

// Disable usage of the PageRunHeader aligned chunk
#define MICRO_USE_FIRST_ALIGNED_CHUNK 1

// Use fine grained MediumChunkHeader locking
#define MICRO_USE_NODE_LOCK 1

// Minimum alignment for small allocations (and for the micro library in general)
#define MICRO_MINIMUM_ALIGNMENT 16

// Disable lock
#ifdef MICRO_NO_LOCK
#undef MICRO_MAX_ARENAS
#define MICRO_MAX_ARENAS 1u
#undef MICRO_USE_NODE_LOCK
#define MICRO_USE_NODE_LOCK 0
#endif

// Enable/disable statistics support
#ifndef MICRO_DISABLE_STATISTICS
#define MICRO_ENABLE_STATISTICS_PARAMETERS
#endif

// Enable/disable time statistics support
#ifdef MICRO_ENABLE_TIME_STATISTICS
#define MICRO_TIME_STATS(...) __VA_ARGS__
#else
#define MICRO_TIME_STATS(...)
#endif

// Detect if a thread_local variable might trigger a malloc() call.
// This could lead to infinit recursion when malloc/free functions are overridden.
// Such recursions MUST be detected through the use of a dedicated hash map.
#ifndef MICRO_OVERRIDE
#define MICRO_THREAD_LOCAL_NO_ALLOC 1
#else
#ifdef _MSC_VER
#define MICRO_THREAD_LOCAL_NO_ALLOC 1
#else
#define MICRO_THREAD_LOCAL_NO_ALLOC 0
#endif
#endif

// Medium allocation flag
#define MICRO_ALLOC_MEDIUM 62761
// Big allocation flag, directly based on pages allocation/deallocation
#define MICRO_ALLOC_BIG 62897
// Free block flag
#define MICRO_ALLOC_FREE 64063
// Aligned block of small objects
#define MICRO_ALLOC_SMALL_BLOCK 97 // 64067
// Allocation hader guard
#define MICRO_BLOCK_GUARD 64171

// Default backend memory (none)
#define MICRO_DEFAULT_BACKEND_MEMORY 0

#endif
