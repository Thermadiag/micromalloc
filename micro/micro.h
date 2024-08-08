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

#ifndef MICRO_MICRO_H
#define MICRO_MICRO_H

#include "enums.h"
#include "micro_config.hpp"
#include "micro_export.hpp"

#include <stdint.h>
#include <stdlib.h>

#ifdef __THROW 
#define MICRO_THROW __THROW
#else

#ifdef __cplusplus
#define MICRO_THROW noexcept
#else
#define MICRO_THROW
#endif

#endif

/// @brief micro_heap opaque structure, used to create local heaps.
typedef struct micro_heap
{
} micro_heap;

#ifndef MICRO_HEADER_ONLY

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Version & Miscellaneous
///////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Returns the library version number
MICRO_EXPORT const char* micro_version() MICRO_THROW;

/// @brief Returns the maximum static cost in bytes per arena
MICRO_EXPORT size_t micro_max_static_cost_per_arena() MICRO_THROW;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic parameters interface
///////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Set a parameter for the global heap object.
/// This must be called prior to any allocation.
/// This function is NOT thread safe.
MICRO_EXPORT void micro_set_parameter(micro_parameter p, uint64_t value) MICRO_THROW;

/// @brief Set a string parameter for the global heap object.
/// This must be called prior to any allocation.
/// This function is NOT thread safe.
MICRO_EXPORT void micro_set_string_parameter(micro_parameter p, const char* value) MICRO_THROW;

/// @brief Returns a parameter from the global heap object.
MICRO_EXPORT uint64_t micro_get_parameter(micro_parameter p) MICRO_THROW;
/// @brief Returns a string parameter from the global heap object.
MICRO_EXPORT const char* micro_get_string_parameter(micro_parameter p) MICRO_THROW;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic allocation/deallocation interface using the global heap
///////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Allocate given amount of bytes.
/// Returns a null pointer in case of failure.
MICRO_EXPORT void* micro_malloc(size_t bytes) MICRO_THROW;

/// @brief Allocate given amount of aligned bytes.
/// Returns a null pointer in case of failure.
MICRO_EXPORT void* micro_memalign(size_t alignment, size_t bytes) MICRO_THROW;

/// @brief Allocate given amount of aligned bytes.
/// Similar behavior to standard function aligned_alloc().
MICRO_EXPORT void* micro_aligned_alloc(size_t alignment, size_t size) MICRO_THROW;

/// @brief Similar behavior to _aligned_realloc() win32 function
MICRO_EXPORT void* micro_aligned_realloc(void* ptr, size_t size, size_t alignment) MICRO_THROW;

/// @brief Similar behavior to posix_memalign() POSIX function
MICRO_EXPORT int micro_posix_memalign(void** memptr, size_t alignment, size_t size) MICRO_THROW;

/// @brief Similar behavior to realloc() standard function
MICRO_EXPORT void* micro_realloc(void* ptr, size_t bytes) MICRO_THROW;
/// @brief Similar behavior to reallocarray() POSIX function
MICRO_EXPORT void* micro_reallocarray(void* ptr, size_t num, size_t size) MICRO_THROW;
/// @brief Similar behavior to reallocarr() NetBSD function
MICRO_EXPORT int micro_reallocarr(void* p, size_t count, size_t size) MICRO_THROW;
/// @brief Similar behavior to reallocf() FreeBSD function
MICRO_EXPORT void* micro_reallocf(void* ptr, size_t bytes) MICRO_THROW;
/// @brief Similar behavior to calloc() standard function
MICRO_EXPORT void* micro_calloc(size_t num, size_t size) MICRO_THROW;

/// @brief Similar behavior to calloc() standard function
MICRO_EXPORT void* micro_valloc(size_t size) MICRO_THROW;
/// @brief Similar behavior to calloc() standard function
MICRO_EXPORT void* micro_pvalloc(size_t size) MICRO_THROW;

/// @brief Returns the amount of bytes given chunk (allocated with micro library) can hold.
MICRO_EXPORT size_t micro_usable_size(void*) MICRO_THROW;

/// @brief Similar behavior to _aligned_msize() win32 function
MICRO_EXPORT size_t micro_aligned_usable_size(void* memblock, size_t alignment, size_t offset) MICRO_THROW;

/// @brief Similar to POSIX function malloc_good_size
MICRO_EXPORT size_t micro_malloc_good_size(size_t) MICRO_THROW;

/// @brief Free a chunk of memory allocated with micro_malloc, micro_memalign, micro_realloc,
/// micro_calloc, micro_heap_malloc, micro_heap_memalign, micro_heap_realloc or micro_heap_calloc.
MICRO_EXPORT void micro_free(void*) MICRO_THROW;

/// @brief Clear the global heap: deallocate all previously allocated pages
/// and reset its internal state, except for its parameters.
MICRO_EXPORT void micro_clear() MICRO_THROW;

/// @brief Retrieve global heap statistics
MICRO_EXPORT void micro_dump_stats(micro_statistics* stats) MICRO_THROW;

/// @brief Similar to msvc _expand
MICRO_EXPORT void* micro_expand(void* ptr, size_t size) MICRO_THROW;

/// @brief Similar to msvc _recalloc
MICRO_EXPORT void* micro_recalloc(void* p, size_t num, size_t size) MICRO_THROW;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Heap interface
///////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Create a local heap with default parameters.
/// This heap is similar to the global one and can be used by any thread.
MICRO_EXPORT micro_heap* micro_heap_create() MICRO_THROW;

/// @brief Destroy a local heap and clear all memory allocated by this heap.
MICRO_EXPORT void micro_heap_destroy(micro_heap* h) MICRO_THROW;

/// @brief Clear a local heap: deallocate all previously allocated memory
/// and reset its internal state, except for its parameters.
MICRO_EXPORT void micro_heap_clear(micro_heap* h) MICRO_THROW;

/// @brief Set local heap parameter.
/// This must be called prior to any allocation.
/// This function is NOT trhead safe.
MICRO_EXPORT void micro_heap_set_parameter(micro_heap* h, micro_parameter p, uint64_t value) MICRO_THROW;
/// @brief Set local heap string parameter.
/// This must be called prior to any allocation.
/// This function is NOT trhead safe.
MICRO_EXPORT void micro_heap_set_string_parameter(micro_heap* h, micro_parameter p, const char* value) MICRO_THROW;

/// @brief Retrieve a local heap parameter
MICRO_EXPORT uint64_t micro_heap_get_parameter(micro_heap* h, micro_parameter p) MICRO_THROW;
/// @brief Retrieve a local heap string parameter
MICRO_EXPORT const char* micro_heap_get_string_parameter(micro_heap* h, micro_parameter p) MICRO_THROW;

/// @brief Equivalent to micro_malloc for local heap
MICRO_EXPORT void* micro_heap_malloc(micro_heap* h, size_t) MICRO_THROW;
/// @brief Equivalent to micro_memalign for local heap
MICRO_EXPORT void* micro_heap_memalign(micro_heap* h, size_t, size_t) MICRO_THROW;
/// @brief Equivalent to micro_realloc for local heap
MICRO_EXPORT void* micro_heap_realloc(micro_heap* h, void*, size_t) MICRO_THROW;
/// @brief Equivalent to micro_calloc for local heap
MICRO_EXPORT void* micro_heap_calloc(micro_heap* h, size_t, size_t) MICRO_THROW;

/// @brief Retrieve local heap statistics
MICRO_EXPORT void micro_heap_dump_stats(micro_heap* h, micro_statistics* stats) MICRO_THROW;

/// @brief Set the global process heap.
/// The previous global heap is NOT destroyed.
/// This function is NOT thread safe.
MICRO_EXPORT void micro_set_process_heap(micro_heap* h) MICRO_THROW;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// Process infos
///////////////////////////////////////////////////////////////////////////////////////////////////////

/// @brief Retrieve process infos on memory usage
MICRO_EXPORT int micro_get_process_infos(micro_process_infos* infos) MICRO_THROW;

#ifdef __cplusplus
}
#endif

#else
#ifndef __cplusplus
#error "header-only mode is only supported in C++"
#endif
#include "internal/micro_c.cpp"
#endif

#endif
