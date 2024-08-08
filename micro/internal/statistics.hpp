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

#ifndef MICRO_STATISTICS_HPP
#define MICRO_STATISTICS_HPP

#ifdef _MSC_VER
// Remove useless warnings ...needs to have dll-interface to be used by clients of class...
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include <atomic>
#include <cstddef>

#include "../bits.hpp"

#ifdef small
// defined by Windows.h
#undef small
#endif

namespace micro
{
	namespace detail
	{
		// Forward declaration
		class MemoryManager;
	}

	/// @brief Gather statistics for tiny, medium or big allocations
	class MICRO_EXPORT_CLASS type_statistics
	{
		friend class statistics;
		void allocate(size_t bytes) noexcept;
		void deallocate(size_t bytes) noexcept;
		void reset() noexcept;

	public:
		// statistics
		std::atomic<std::uint64_t> alloc_count{ 0 };	     // total number of allocations
		std::atomic<std::uint64_t> freed_count{ 0 };	     // total number of deallocations
		std::atomic<std::uint64_t> alloc_bytes{ 0 };	     // total allocation bytes
		std::atomic<std::uint64_t> freed_bytes{ 0 };	     // total freed bytes
		std::atomic<std::uint64_t> current_alloc_count{ 0 }; // current number of allocations
		std::atomic<std::uint64_t> current_alloc_bytes{ 0 }; // current allocation bytes

		std::atomic<std::uint64_t>& max_alloc_bytes;   // inter-class memory peak
		std::atomic<std::uint64_t>& total_alloc_bytes; // inter-class current allocation bytes

		type_statistics(std::atomic<std::uint64_t>& max_alloc_bytes, std::atomic<std::uint64_t>& total_alloc_bytes) noexcept;
	};

	/// @brief Gather all statistics
	class MICRO_EXPORT_CLASS statistics
	{
		friend class detail::MemoryManager;

		void allocate_small(size_t bytes) noexcept;
		void deallocate_small(size_t bytes) noexcept;

		void allocate_medium(size_t bytes) noexcept;
		void deallocate_medium(size_t bytes) noexcept;

		void allocate_big(size_t bytes) noexcept;
		void deallocate_big(size_t bytes) noexcept;

	public:
		std::atomic<std::uint64_t> max_alloc_bytes{ 0 };   // inter-class memory peak
		std::atomic<std::uint64_t> total_alloc_bytes{ 0 }; // inter-class current allocation bytes

		std::atomic<std::uint64_t> total_alloc_time_ns{ 0 };   // total time spent in allocation
		std::atomic<std::uint64_t> total_dealloc_time_ns{ 0 }; // total time spent in deallocation
		std::atomic<std::uint64_t> max_alloc_time_ns{ 0 };     // maximum allocation time
		std::atomic<std::uint64_t> max_dealloc_time_ns{ 0 };   // maximum deallocation time

		type_statistics small;
		type_statistics medium;
		type_statistics big;

		statistics() noexcept;
		void reset() noexcept;

		void update_alloc_time(std::uint64_t ns) noexcept
		{
			total_alloc_time_ns.fetch_add(ns);
			auto max_ns = max_alloc_time_ns.load();
			while (ns > max_ns) {
				if (max_alloc_time_ns.compare_exchange_strong(max_ns, ns))
					break;
			}
		}
		void update_dealloc_time(std::uint64_t ns) noexcept
		{
			total_dealloc_time_ns.fetch_add(ns);
			auto max_ns = max_dealloc_time_ns.load();
			while (ns > max_ns) {
				if (max_dealloc_time_ns.compare_exchange_strong(max_ns, ns))
					break;
			}
		}
	};

}

#ifdef MICRO_HEADER_ONLY
#include "statistics.cpp"
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
