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

#ifndef MICRO_OS_TIMER_HPP
#define MICRO_OS_TIMER_HPP

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_time.h>
#elif defined(_MSC_VER) || defined(__MINGW32__)
#include "Windows.h"
#else
#include <time.h>
#endif
#include <cstdint>

#include "bits.hpp"

namespace micro
{
	/// @class timer
	///
	/// @brief Precise timer class returning elapsed time in nanoseconds
	///
	///

#ifdef __APPLE__

	class MICRO_EXPORT_CLASS timer
	{
		std::uint64_t start{ 0 };
		mach_timebase_info_data_t rate;

	public:
		timer()
		{
			mach_timebase_info_data_t rate;
			mach_timebase_info(&rate);
		}
		void tick() noexcept { start = mach_absolute_time(); }
		std::uint64_t tock() const noexcept
		{
			std::uint64_t elapsed = mach_absolute_time() - start;
			return (elapsed * rate.numer) / rate.denom;
		}
	};

#elif defined(_MSC_VER) || defined(__MINGW32__)

	class MICRO_EXPORT_CLASS timer
	{
		LARGE_INTEGER start;
		static LARGE_INTEGER compute_frequency() noexcept
		{
			LARGE_INTEGER frequency;
			frequency.QuadPart = 1;
			::QueryPerformanceFrequency(&frequency);
			return frequency;
		}
		static LARGE_INTEGER get_frequency() noexcept
		{
			static LARGE_INTEGER frequency = compute_frequency();
			return frequency;
		}

	public:
		void tick() noexcept { ::QueryPerformanceCounter(&start); }
		std::uint64_t tock() const noexcept
		{
			LARGE_INTEGER end;
			if (::QueryPerformanceCounter(&end) == FALSE)
				return 0;

			return (static_cast<std::uint64_t>(end.QuadPart - start.QuadPart) * 1000000000ull) / static_cast<std::uint64_t>(get_frequency().QuadPart);
		}
	};

#else

	// Unix systems

	class MICRO_EXPORT_CLASS timer
	{
		timespec start;
		timespec diff(timespec start, timespec end) const noexcept
		{
			timespec temp;
			if ((end.tv_nsec - start.tv_nsec) < 0) {
				temp.tv_sec = end.tv_sec - start.tv_sec - 1;
				temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
			}
			else {
				temp.tv_sec = end.tv_sec - start.tv_sec;
				temp.tv_nsec = end.tv_nsec - start.tv_nsec;
			}
			return temp;
		}

	public:
		void tick() noexcept { clock_gettime(CLOCK_MONOTONIC, &start); }
		std::uint64_t tock() const noexcept
		{
			timespec end;
			clock_gettime(CLOCK_MONOTONIC, &end);

			timespec d = diff(start, end);
			return d.tv_sec * 1000000000ull + d.tv_nsec;
		}
	};
#endif

}

#endif
