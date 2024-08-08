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

#include "../logger.hpp"
#include "../micro.h"
#include "../micro.hpp"

#include <cstring>

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

MICRO_HEADER_ONLY_EXPORT_FUNCTION const char* micro_version() MICRO_THROW
{
	return MICRO_VERSION;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION size_t micro_max_static_cost_per_arena() MICRO_THROW
{
	return sizeof(micro::detail::RadixLeaf) * (1u << (MICRO_MAX_RADIX_SIZE / 2u));
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION size_t micro_usable_size(void* ptr) MICRO_THROW
{
	return micro::heap::usable_size(ptr);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION size_t micro_aligned_usable_size(void* memblock, size_t alignment, size_t offset) MICRO_THROW
{
	(void)offset;
	if (!memblock || alignment == 0 || ((alignment - 1) & alignment) != 0) {
		errno = EINVAL;
		return static_cast<size_t>(-1);
	}
	MICRO_ASSERT_DEBUG((static_cast<uintptr_t>(alignment - 1) & reinterpret_cast<uintptr_t> (memblock)) == 0, "");
	return micro_usable_size(memblock);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_malloc(size_t bytes) MICRO_THROW
{
	return micro::get_process_heap().allocate(bytes);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_free(void* p) MICRO_THROW
{
	micro::detail::MemoryManager::deallocate(p);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_memalign(size_t alignment, size_t size) MICRO_THROW
{
	return micro::get_process_heap().aligned_allocate(alignment, size);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_realloc(void* ptr, size_t size) MICRO_THROW
{
	if (!ptr)
		return micro_malloc(size);

	size_t usable = micro_usable_size(ptr);
	if (size <= usable)
		return ptr;

	void* _new = micro_malloc(size);
	if (!_new)
		return nullptr;

	memcpy(_new, ptr, usable);
	micro_free(ptr);
	return _new;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_aligned_realloc(void* ptr, size_t size, size_t alignment) MICRO_THROW
{
	if (!ptr)
		return micro_malloc(size);
	if (!size)
		return nullptr;
	MICRO_ASSERT_DEBUG((static_cast<uintptr_t>(alignment - 1) & reinterpret_cast<uintptr_t> (ptr)) == 0, "");
	size_t usable = micro_usable_size(ptr);
	if (size <= usable)
		return ptr;

	void* _new = micro_memalign(alignment, size);
	if (!_new)
		return nullptr;

	memcpy(_new, ptr, usable);
	micro_free(ptr);
	return _new;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_reallocf(void* ptr, size_t size) MICRO_THROW
{
	if (!ptr)
		return micro_malloc(size);

	size_t usable = micro_usable_size(ptr);
	if (size <= usable)
		return ptr;

	void* _new = micro_malloc(size);
	if (!_new) {
		micro_free(ptr);
		return nullptr;
	}

	memcpy(_new, ptr, usable);
	micro_free(ptr);
	return _new;
}

// mimalloc style
static inline bool micro_mul_overflow(size_t count, size_t size, size_t* total)
{
	// This part comes from mimalloc
#define MICRO_MUL_COULD_OVERFLOW (static_cast<size_t>(1) << (4u * sizeof(size_t))) // sqrt(SIZE_MAX)
	*total = count * size;
	// note: gcc/clang optimize this to directly check the overflow flag
	return ((size >= MICRO_MUL_COULD_OVERFLOW || count >= MICRO_MUL_COULD_OVERFLOW) && size > 0 && (SIZE_MAX / size) < count);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_reallocarray(void* ptr, size_t num, size_t size) MICRO_THROW
{
	size_t total;
	if (micro_mul_overflow(num, size, &total))
		return nullptr;
	return micro_realloc(ptr, total);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION int micro_reallocarr(void* p, size_t count, size_t size) MICRO_THROW
{ // NetBSD

	if (p == nullptr) {
		errno = EINVAL;
		return EINVAL;
	}
	void** op = reinterpret_cast<void**>(p);
	void* newp = micro_reallocarray(*op, count, size);
	if (MICRO_UNLIKELY(newp == nullptr)) {
		return errno;
	}
	*op = newp;
	return 0;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_expand(void* ptr, size_t size) MICRO_THROW
{
	if (!ptr)
		return nullptr;

	size_t usable = micro_usable_size(ptr);
	if (size <= usable)
		return ptr;
	return nullptr;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_recalloc(void* p, size_t num, size_t _size) MICRO_THROW
{
	// if p == NULL then behave as malloc.
	// else if size == 0 then reallocate to a zero-sized block (and don't return NULL, just as mi_malloc(0)).
	// (this means that returning NULL always indicates an error, and `p` will not have been freed in that case.)
	size_t newsize = _size * num;
	const size_t size = p ? micro_usable_size(p) : 0;     // also works if p == NULL (with size 0)
	if (MICRO_UNLIKELY(newsize <= size && newsize > 0)) { // note: newsize must be > 0 or otherwise we return NULL for realloc(NULL,0)
		MICRO_ASSERT_DEBUG(p != nullptr, "");
		return p; // reallocation still fits
	}
	void* newp = micro_malloc(newsize);
	if (MICRO_LIKELY(newp != nullptr)) {
		if (newsize > size) {
			// also set last word in the previous allocation to zero to ensure any padding is zero-initialized
			const size_t start = (size >= sizeof(intptr_t) ? size - sizeof(intptr_t) : 0);
			memset(static_cast<uint8_t*>(newp) + start, 0, newsize - start);
		}
		else if (newsize == 0) {
			(static_cast<uint8_t*>(newp))[0] = 0; // work around for applications that expect zero-reallocation to be zero initialized (issue #725)
		}
		if (MICRO_LIKELY(p != nullptr)) {
			const size_t copysize = (newsize > size ? size : newsize);
			memcpy(newp, p, copysize);
			micro_free(p); // only free the original pointer if successful
		}
	}
	return newp;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION size_t micro_malloc_good_size(size_t size) MICRO_THROW
{
	if (size % MICRO_MINIMUM_ALIGNMENT)
		size = (size / MICRO_MINIMUM_ALIGNMENT + 1) * MICRO_MINIMUM_ALIGNMENT;
	return size;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_calloc(size_t num, size_t size) MICRO_THROW
{
	void* p = micro_malloc(num * size);
	if (p)
		memset(p, 0, num * size);
	return p;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_valloc(size_t size) MICRO_THROW
{
	return micro_memalign(micro::os_page_size(), size);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_pvalloc(size_t size) MICRO_THROW
{
	size_t psize = micro::os_page_size();
	if (size & (psize - 1)) {
		size &= ~(psize - 1);
		size += psize;
	}
	return micro_memalign(psize, size);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_aligned_alloc(size_t alignment, size_t size) MICRO_THROW
{
	if (alignment == 0 || (alignment & (alignment - 1)) != 0)
		return nullptr;
	if (size & (alignment - 1))
		return nullptr;
	return micro_memalign(alignment, size);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION int micro_posix_memalign(void** memptr, size_t alignment, size_t size) MICRO_THROW
{
	void* p = micro_memalign(alignment, size);
	if (!p)
		return ENOMEM;

	*memptr = p;
	return 0;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_clear() MICRO_THROW
{
	micro::get_process_heap().clear();
}

namespace micro
{
	namespace detail
	{

		template<size_t N>
		static inline void assign_char_array(std::array<char, N>& ar, const char* ptr)
		{
			const char* val = ptr;
			size_t len = std::min(N - 1, strlen(val));
			memcpy(ar.data(), val, len);
			ar[len] = 0;
		}

		static inline void set_parameter(parameters& h, micro_parameter p, uint64_t value)
		{
			switch (p) {
				case MicroSmallAllocThreshold:
					h.small_alloc_threshold = unsigned(value);
					break;
				case MicroAllowSmallAlloxFromRadixTree:
					h.allow_small_alloc_from_radix_tree = bool(value);
					break;
				case MicroDepleteArenas:
					h.deplete_arenas = bool(value);
					break;
				case MicroMaxArenas:
					h.max_arenas = unsigned(value);
					break;
				case MicroMemoryLimit:
					h.memory_limit = (value);
					break;
				case MicroBackendMemory:
					h.backend_memory = (value);
					break;
				case MicroLogLevel:
					h.log_level = unsigned(value);
					break;
				case MicroPageSize:
					h.page_size = unsigned(value);
					break;
				case MicroPageMemorySize:
					h.page_memory_size = (value);
					break;
				case MicroGrowFactor:
					h.grow_factor = 1. + (static_cast<double>(value) / 10.);
					break;
				case MicroProviderType:
					h.provider_type = unsigned(value);
					break;
				case MicroAllowOsPageAlloc:
					h.allow_os_page_alloc = bool(value);
					break;
				case MicroPageFileFlags:
					h.page_file_flags = unsigned(value);
					break;
				case MicroPrintStatsTrigger:
					h.print_stats_trigger = unsigned(value);
					break;
				case MicroPrintStatsMs:
					h.print_stats_ms = unsigned(value);
					break;
				case MicroPrintStatsBytes:
					h.print_stats_bytes = unsigned(value);
					break;

				case MicroDateFormat:
				case MicroPageFileProvider:
				case MicroPageFileDirProvider:
				case MicroPrintStats:
				case MicroPageMemoryProvider:
					MICRO_ASSERT(false, "wrong parameter type");
					break;
			}
		}

		static inline uint64_t get_parameter(parameters& h, micro_parameter p)
		{
			switch (p) {
				case MicroSmallAllocThreshold:
					return h.small_alloc_threshold;
				case MicroAllowSmallAlloxFromRadixTree:
					return h.allow_small_alloc_from_radix_tree;
				case MicroDepleteArenas:
					return h.deplete_arenas;
				case MicroMaxArenas:
					return h.max_arenas;
				case MicroMemoryLimit:
					return h.memory_limit;
				case MicroBackendMemory:
					return h.backend_memory;
				case MicroLogLevel:
					return h.log_level;
				case MicroPageSize:
					return h.page_size;
				case MicroPageMemorySize:
					return h.page_memory_size;
				case MicroGrowFactor:
					return static_cast<uint64_t>((h.grow_factor - 1) * 10);
				case MicroProviderType:
					return h.provider_type;
				case MicroAllowOsPageAlloc:
					return h.allow_os_page_alloc;
				case MicroPageFileFlags:
					return h.page_file_flags;
				case MicroPrintStatsTrigger:
					return h.print_stats_trigger;
				case MicroPrintStatsMs:
					return h.print_stats_ms;
				case MicroPrintStatsBytes:
					return h.print_stats_bytes;

				case MicroDateFormat:
				case MicroPageFileProvider:
				case MicroPageFileDirProvider:
				case MicroPrintStats:
				case MicroPageMemoryProvider:
					MICRO_ASSERT(false, "wrong parameter type");
					return 0;
			}
			return 0;
		}

		static inline void set_string_parameter(parameters& h, micro_parameter p, const char* value)
		{
			switch (p) {

				case MicroDateFormat:
					micro::detail::assign_char_array(h.log_date_format, value);
					break;
				case MicroPageFileProvider:
					micro::detail::assign_char_array(h.page_file_provider, value);
					break;
				case MicroPageFileDirProvider:
					micro::detail::assign_char_array(h.page_file_provider_dir, value);
					break;
				case MicroPrintStats:
					micro::detail::assign_char_array(h.print_stats, value);
					break;
				case MicroPageMemoryProvider:
					h.page_memory_provider = const_cast<char*>(value);
					break;

				case MicroSmallAllocThreshold:
				case MicroAllowSmallAlloxFromRadixTree:
				case MicroMaxArenas:
				case MicroMemoryLimit:
				case MicroBackendMemory:
				case MicroLogLevel:
				case MicroPageSize:
				case MicroPageMemorySize:
				case MicroGrowFactor:
				case MicroProviderType:
				case MicroAllowOsPageAlloc:
				case MicroPageFileFlags:
				case MicroPrintStatsTrigger:
				case MicroPrintStatsMs:
				case MicroPrintStatsBytes:
				case MicroDepleteArenas:
					MICRO_ASSERT(false, "wrong parameter type");
					break;
			}
		}
		static inline const char* get_string_parameter(parameters& h, micro_parameter p)
		{
			switch (p) {
				case MicroDateFormat:
					return h.log_date_format.data();
				case MicroPageFileProvider:
					return h.page_file_provider.data();
				case MicroPageFileDirProvider:
					return h.page_file_provider.data();
				case MicroPrintStats:
					return h.print_stats.data();
				case MicroPageMemoryProvider:
					return h.page_memory_provider;

				case MicroSmallAllocThreshold:
				case MicroAllowSmallAlloxFromRadixTree:
				case MicroMaxArenas:
				case MicroMemoryLimit:
				case MicroBackendMemory:
				case MicroLogLevel:
				case MicroPageSize:
				case MicroPageMemorySize:
				case MicroGrowFactor:
				case MicroProviderType:
				case MicroAllowOsPageAlloc:
				case MicroPageFileFlags:
				case MicroPrintStatsTrigger:
				case MicroPrintStatsMs:
				case MicroPrintStatsBytes:
				case MicroDepleteArenas:
					MICRO_ASSERT(false, "wrong parameter type");
					return nullptr;
			}
			return nullptr;
		}

	}

}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_set_parameter(micro_parameter p, uint64_t value) MICRO_THROW
{
	micro::detail::set_parameter(micro::get_process_parameters(), p, value);
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION uint64_t micro_get_parameter(micro_parameter p) MICRO_THROW
{
	return micro::detail::get_parameter(micro::get_process_parameters(), p);
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_set_string_parameter(micro_parameter p, const char* value) MICRO_THROW
{
	micro::detail::set_string_parameter(micro::get_process_parameters(), p, value);
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION const char* micro_get_string_parameter(micro_parameter p) MICRO_THROW
{
	return micro::detail::get_string_parameter(micro::get_process_parameters(), p);
}

namespace micro
{

}
MICRO_HEADER_ONLY_EXPORT_FUNCTION micro_heap* micro_heap_create() MICRO_THROW
{
	using namespace micro;
	unsigned s = sizeof(detail::heap_t);
	unsigned pcount = s / static_cast<unsigned>(os_page_size());
	if (pcount == 0)
		pcount = 1;
	detail::heap_t* h = detail::heap_t::from(os_allocate_pages(pcount));
	if (!h)
		return nullptr;
	new (&h->init) std::atomic<bool>{ false };
	new (&h->p) parameters(parameters::from_env());
	return reinterpret_cast<micro_heap*>(h);
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_heap_destroy(micro_heap* h) MICRO_THROW
{
	using namespace micro;
	auto* heap = detail::heap_t::from(h);
	if (heap->init.load()) {
		heap->h.~heap();
	}
	unsigned pcount = sizeof(detail::heap_t) / static_cast<unsigned>(micro::os_page_size());
	if (pcount == 0)
		pcount = 1;
	os_free_pages(h, pcount);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_heap_clear(micro_heap* h) MICRO_THROW
{
	using namespace micro;
	auto* heap = detail::heap_t::from(h);
	if (heap->init.load())
		heap->h.clear();
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_heap_set_parameter(micro_heap* h, micro_parameter p, uint64_t value) MICRO_THROW
{
	using namespace micro;
	detail::set_parameter((detail::heap_t::from(h))->p, p, value);
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_heap_set_string_parameter(micro_heap* h, micro_parameter p, const char* value) MICRO_THROW
{
	using namespace micro;
	detail::set_string_parameter((detail::heap_t::from(h))->p, p, value);
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION uint64_t micro_heap_get_parameter(micro_heap* h, micro_parameter p) MICRO_THROW
{
	using namespace micro;
	return detail::get_parameter((detail::heap_t::from(h))->p, p);
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION const char* micro_heap_get_string_parameter(micro_heap* h, micro_parameter p) MICRO_THROW
{
	using namespace micro;
	return detail::get_string_parameter((detail::heap_t::from(h))->p, p);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_heap_malloc(micro_heap* h, size_t size) MICRO_THROW
{
	using namespace micro;
	auto* heap = detail::init_heap(h);
	return heap->h.allocate(size);
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_heap_memalign(micro_heap* h, size_t alignment, size_t size) MICRO_THROW
{
	using namespace micro;
	auto* heap = detail::init_heap(h);
	return heap->h.aligned_allocate(alignment, size);
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_heap_realloc(micro_heap* h, void* ptr, size_t size) MICRO_THROW
{
	using namespace micro;
	auto* heap = detail::init_heap(h);
	if (!ptr)
		return heap->h.allocate(size);

	size_t usable = heap->h.usable_size(ptr);
	if (size < usable)
		return ptr;

	void* _new = heap->h.allocate(size);
	if (!_new)
		return nullptr;

	memcpy(_new, ptr, usable);
	micro_free(ptr);
	return _new;
}
MICRO_HEADER_ONLY_EXPORT_FUNCTION void* micro_heap_calloc(micro_heap* h, size_t num, size_t size) MICRO_THROW
{
	using namespace micro;
	auto* heap = detail::init_heap(h);
	void* p = heap->h.allocate(num * size);
	if (p)
		memset(p, 0, num * size);
	return p;
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_heap_dump_stats(micro_heap* h, micro_statistics* stats) MICRO_THROW
{
	using namespace micro;
	auto* heap = detail::init_heap(h);
	heap->h.dump_stats(*stats);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_set_process_heap(micro_heap* h) MICRO_THROW
{
	using namespace micro;
	auto* heap = detail::init_heap(h);
	set_process_heap(heap->h);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION void micro_dump_stats(micro_statistics* st) MICRO_THROW
{
	if (!st)
		return;
	micro::get_process_heap().dump_stats(*st);
}

MICRO_HEADER_ONLY_EXPORT_FUNCTION int micro_get_process_infos(micro_process_infos* infos) MICRO_THROW
{
	if (!infos)
		return -1;
	if (micro::get_process_infos(*infos))
		return 0;
	return -1;
}

MICRO_POP_DISABLE_OLD_STYLE_CAST
