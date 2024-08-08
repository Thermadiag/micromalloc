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

#include "statistics.hpp"

namespace micro
{

	MICRO_EXPORT_CLASS_MEMBER type_statistics::type_statistics(std::atomic<std::uint64_t>& _max_alloc_bytes, std::atomic<std::uint64_t>& _total_alloc_bytes) noexcept
	  : max_alloc_bytes(_max_alloc_bytes)
	  , total_alloc_bytes(_total_alloc_bytes)
	{
	}
	MICRO_EXPORT_CLASS_MEMBER void type_statistics::allocate(size_t bytes) noexcept
	{
		alloc_count++;
		alloc_bytes += bytes;
		current_alloc_count++;
		current_alloc_bytes += bytes;
		total_alloc_bytes += bytes;

		for (;;) {
			std::uint64_t total = total_alloc_bytes.load();
			std::uint64_t max = max_alloc_bytes.load();
			if (total > max) {
				if (max_alloc_bytes.compare_exchange_strong(max, total))
					return;
			}
			else
				return;
		}
	}
	MICRO_EXPORT_CLASS_MEMBER void type_statistics::deallocate(size_t bytes) noexcept
	{
		freed_count++;
		freed_bytes += bytes;
		current_alloc_count--;
		current_alloc_bytes -= bytes;
		total_alloc_bytes -= bytes;
	}
	MICRO_EXPORT_CLASS_MEMBER void type_statistics::reset() noexcept
	{
		alloc_count.store(0);	      // total number of allocations
		freed_count.store(0);	      // total number of deallocations
		alloc_bytes.store(0);	      // total allocation bytes
		freed_bytes.store(0);	      // total freed bytes
		current_alloc_count.store(0); // current number of allocations
		current_alloc_bytes.store(0);
	}

	MICRO_EXPORT_CLASS_MEMBER statistics::statistics() noexcept
	  : small(max_alloc_bytes, total_alloc_bytes)
	  , medium(max_alloc_bytes, total_alloc_bytes)
	  , big(max_alloc_bytes, total_alloc_bytes)
	{
	}
	MICRO_EXPORT_CLASS_MEMBER void statistics::allocate_small(size_t bytes) noexcept { small.allocate(bytes); }
	MICRO_EXPORT_CLASS_MEMBER void statistics::deallocate_small(size_t bytes) noexcept { small.deallocate(bytes); }
	MICRO_EXPORT_CLASS_MEMBER void statistics::allocate_medium(size_t bytes) noexcept { medium.allocate(bytes); }
	MICRO_EXPORT_CLASS_MEMBER void statistics::deallocate_medium(size_t bytes) noexcept { medium.deallocate(bytes); }
	MICRO_EXPORT_CLASS_MEMBER void statistics::allocate_big(size_t bytes) noexcept { big.allocate(bytes); }
	MICRO_EXPORT_CLASS_MEMBER void statistics::deallocate_big(size_t bytes) noexcept { big.deallocate(bytes); }
	MICRO_EXPORT_CLASS_MEMBER void statistics::reset() noexcept
	{
		small.reset();
		medium.reset();
		big.reset();
		max_alloc_bytes.store(0);
		total_alloc_bytes.store(0);
	}

}
