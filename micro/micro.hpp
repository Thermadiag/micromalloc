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

#ifndef MICRO_MICRO_HPP
#define MICRO_MICRO_HPP

#include <cstddef>
#include <new>
#include <type_traits>

#include "internal/allocator.hpp"
#include "micro.h"
#include "parameters.hpp"

namespace micro
{
	
	// Forward declarations
	class heap;
	namespace detail
	{
#ifndef MICRO_HEADER_ONLY
		MICRO_EXPORT heap* get_default_process_heap() noexcept;
		MICRO_EXPORT heap*& get_heap_pointer() noexcept;
#else
		heap* get_default_process_heap() noexcept;
		heap*& get_heap_pointer() noexcept;
#endif
	}

#ifndef MICRO_HEADER_ONLY

	/// @brief Returns the global process heap
	MICRO_EXPORT heap& get_process_heap() noexcept;

	/// @brief Set the global process heap.
	/// The previous global heap is NOT destroyed.
	/// This function is NOT thread safe.
	MICRO_EXPORT void set_process_heap(heap&) noexcept;

#else
	heap& get_process_heap() noexcept;
	void set_process_heap(heap&) noexcept;
#endif

	/// @brief Heap class used to allocate/deallocate memory.
	///
	/// The heap class is the micro library class dedicated to
	/// allocation/deallocation of memory blocks of any size. A
	/// heap object can be created in any thread, and all member
	/// functions are thread safe.
	///
	/// A heap object can be created using the default constructor,
	/// in which case the default process parameters are used (
	/// see micro::get_process_parameters() function).
	///
	/// A heap object can also be created from custom parameters.
	/// Once constructed the heap parameters cannot be modified.
	///
	/// On destruction, a heap object will deallocate all remaining
	/// allocated memory.
	///
	class MICRO_EXPORT_CLASS heap
	{
		friend heap* detail::get_default_process_heap() noexcept;
		
		MICRO_ALWAYS_INLINE void init() noexcept { d_mgr.init(); }
		heap(const parameters& p, bool) noexcept
		  : d_mgr(p, false)
		{
		}
	public:
		MICRO_DELETE_COPY(heap)
		
		/// @brief Default constructor, uses global process parameters
		heap() noexcept
		  : d_mgr(get_process_parameters())
		{
		}

		/// @brief Construct from custom parameters.
		/// Note that the parameters will be validated (and potentially modified)
		/// by the heap object.
		heap(const parameters& p) noexcept
		  : d_mgr(p)
		{
		}

		/// @brief Returns parameters
		MICRO_ALWAYS_INLINE const parameters& params() const noexcept { return d_mgr.params(); }

		/// @brief Allocates size bytes.
		/// Returns null on error.
		MICRO_ALWAYS_INLINE void* allocate(size_t size) noexcept { return d_mgr.allocate(size); }

		/// @brief Allocates size aligned bytes.
		/// Returns null on error.
		MICRO_ALWAYS_INLINE void* aligned_allocate(size_t alignment, size_t size) noexcept { return d_mgr.aligned_allocate(alignment, size); }

		/// @brief Deallocate a memory chunk previously allocated with
		/// heap::allocate, heap::aligned_allocate, micro_malloc,
		/// micro_memalign, micro_realloc, micro_calloc, micro_heap_malloc,
		/// micro_heap_memalign, micro_heap_realloc or micro_heap_calloc.
		static MICRO_ALWAYS_INLINE void deallocate(void* p) noexcept { detail::MemoryManager::deallocate(p); }

		/// @brief Returns the amount of bytes given chunk (allocated with micro library) can hold.
		static MICRO_ALWAYS_INLINE size_t usable_size(void* p) noexcept { return detail::MemoryManager::usable_size(p); }

		/// @brief Clear the heap: deallocated all remaining memory and reset internal state
		/// (except for the parameters)
		MICRO_ALWAYS_INLINE void clear() noexcept { d_mgr.clear(); }

		/// @brief Reset the heap statistics
		MICRO_ALWAYS_INLINE void reset_stats() noexcept { d_mgr.reset_statistics(); }

		/// @brief Reset the heap creation time
		MICRO_ALWAYS_INLINE void set_start_time() noexcept { d_mgr.set_start_time(); }

		/// @brief Retrieve the heap statistics
		MICRO_ALWAYS_INLINE void dump_stats(micro_statistics& st) noexcept { d_mgr.dump_statistics(st); }

		/// @brief Returns the heap peak allocated memory
		MICRO_ALWAYS_INLINE std::uint64_t peak_allocated_memory() const noexcept { return d_mgr.peak_allocated_memory(); }

		/// @brief Prints the statistics header in CSV format
		MICRO_ALWAYS_INLINE void print_stats_header(print_callback_type callback, void* opaque) noexcept { d_mgr.print_stats_header(callback, opaque); }
		/// @brief Prints the statistics header in CSV format
		MICRO_ALWAYS_INLINE void print_stats_header_stdout() noexcept { d_mgr.print_stats_header_stdout(); }

		/// @brief Prints current statistics in CSV format
		MICRO_ALWAYS_INLINE void print_stats_row(print_callback_type callback, void* opaque) noexcept { d_mgr.print_stats_row(callback, opaque); }
		/// @brief Prints current statistics in CSV format
		MICRO_ALWAYS_INLINE void print_stats_row_stdout() noexcept { d_mgr.print_stats_row_stdout(); }

		/// @brief Prints current statistics
		MICRO_ALWAYS_INLINE void print_stats(print_callback_type callback, void* opaque) noexcept { d_mgr.print_stats(callback, opaque); }
		/// @brief Prints current statistics
		MICRO_ALWAYS_INLINE void print_stats_stdout() noexcept { d_mgr.print_stats_stdout(); }

		/// @brief Performs all exit operations.
		/// Called by the heap destructor and should not be called manually.
		MICRO_ALWAYS_INLINE void perform_exit_operations() noexcept { d_mgr.perform_exit_operations(); }

		MICRO_ALWAYS_INLINE void set_main() noexcept { detail::MemoryManager::get_main_manager() = &d_mgr; }

	private:
		detail::MemoryManager d_mgr;
	};

	namespace detail
	{
		struct heap_t
		{
			std::atomic<bool> init;
			heap h;
			parameters p;

			MICRO_ADD_CASTS(heap_t)
		};

		static inline heap_t* init_heap(void* mem) noexcept
		{
			detail::heap_t* heap = detail::heap_t::from(mem);
			if (MICRO_UNLIKELY(!heap->init.load(std::memory_order_relaxed))) {
				if (!heap->init.exchange(true))
					new (&heap->h) micro::heap(heap->p);
			}
			return heap;
		}
	}

	/// @brief Retrieve a heap object from a micro_heap opaque pointer
	/// (if mixing C and C++ interfaces)
	inline heap& from_micro_heap(void* h) noexcept { return detail::init_heap(h)->h; }

	/// @brief Stl conforming allocator based on the heap class.
	///
	/// micro::heap_allocator is a stl compliant allocator based on micro::heap class.
	/// It can be created from the global process heap or from a local heap.
	///
	/// It usually provides faster allocation/deallocation time as well as reduced memory
	/// footprint and reduced memory fragmentation compared to the default allocator.
	///
	/// Note that heap_allocator does not work with std::list::sort() on some gcc versions.
	/// (https://stackoverflow.com/questions/63716394/list-sort-fails-with-abort-when-list-is-created-with-stateful-allocator-when-com)
	///
	template<class T>
	class heap_allocator
	{
		template<class U>
		friend class heap_allocator;

		heap* d_heap;

	public:
		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_swap = std::true_type;
		using propagate_on_container_copy_assignment = std::true_type;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::false_type;
		template<class U>
		struct rebind
		{
			using other = heap_allocator<U>;
		};

		auto select_on_container_copy_construction() const noexcept -> heap_allocator<T> { return *this; }

		heap_allocator() noexcept
		  : d_heap(&get_process_heap())
		{
		}
		heap_allocator(heap& h) noexcept
		  : d_heap(&h)
		{
		}
		heap_allocator(const heap_allocator& other) noexcept
		  : d_heap(other.d_heap)
		{
		}
		template<class U>
		heap_allocator(const heap_allocator<U>& other) noexcept
		  : d_heap(other.d_heap)
		{
		}
		~heap_allocator() noexcept {}

		auto operator==(const heap_allocator& other) const noexcept -> bool { return d_heap == other.d_heap; }
		auto operator!=(const heap_allocator& other) const noexcept -> bool { return !operator==(other); }
		auto address(reference x) const noexcept -> pointer { return std::addressof(x); }
		auto address(const_reference x) const noexcept -> const_pointer { return std::addressof(x); }
		auto allocate(size_t n, const void* /*unused*/) -> value_type* { return allocate(n); }
		auto allocate(size_t n) -> value_type*
		{
			value_type* p = static_cast<value_type*>(d_heap->aligned_allocate(alignof(T), n * sizeof(T)));
			if (MICRO_UNLIKELY(!p))
				throw std::bad_alloc();
			return p;
		}
		void deallocate(value_type* p, size_t n) noexcept
		{
#ifdef MICRO_DEBUG
			MICRO_ASSERT_DEBUG(d_heap->usable_size(p) >= n * sizeof(T), "");
#else
			(void)n;
#endif
			d_heap->deallocate(p);
		}
	};
}

#ifndef MICRO_HEADER_ONLY
namespace micro
{
	MICRO_EXPORT bool get_process_infos(micro_process_infos& infos) noexcept;
}
#else
#include "internal/micro.cpp"
#endif

#endif
