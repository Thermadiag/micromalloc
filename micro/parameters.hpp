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

#ifndef MICRO_PARAMETERS_HPP
#define MICRO_PARAMETERS_HPP

#include "enums.h"
#include "internal/defines.hpp"
#include "logger.hpp"

#include <array>
#include <thread>

namespace micro
{
	namespace detail
	{

#ifndef MICRO_NO_LOCK
		static inline unsigned compute_default_arenas() noexcept
		{
			unsigned cores = std::thread::hardware_concurrency();
			if (cores <= 1)
				return 1;
			if ((cores & (cores - 1)) != 0)
				cores = (1u << bit_scan_reverse_32(cores));
			if (cores > MICRO_MAX_ARENAS)
				cores = MICRO_MAX_ARENAS;
			return cores;
		}
		static inline unsigned default_arenas()
		{
			static unsigned arenas = compute_default_arenas();
			return arenas;
		}

#else
		static inline unsigned default_arenas() noexcept { return 1; }
#endif
	}

	/// @brief Memory manager parameters, used by the micro::heap class constructor
	class MICRO_EXPORT_CLASS parameters
	{

	public:
		/// @brief Use dedicated memory pools for small allocations
		unsigned small_alloc_threshold{ MICRO_MAX_SMALL_ALLOC_THRESHOLD };

		/// @brief allow using the medium allocation radix tree for small allocations if possible.
		bool allow_small_alloc_from_radix_tree{ MICRO_ALLOW_SMALL_ALLOC_FROM_RADIX_TREE };

		/// @brief Allow allocating from other arenas if current one cannot allocate requested size
		bool deplete_arenas{ true };

		/// @brief Number of arenas
		unsigned max_arenas{ detail::default_arenas() };

		/// @brief Global memory limit, calls to micro_malloc() or heap::allocate() will return null if we go beyong this limit.
		/// Default to 0 (disabled).
		std::uint64_t memory_limit{ 0 };

		/// @brief Backend pages to be kept on deallocation.
		/// If the value is <= 100, it is considered as a percent of currently used memory.
		/// If >= 100, it is considered as a raw maximum number of pages.
		std::uint64_t backend_memory{ MICRO_DEFAULT_BACKEND_MEMORY };

		/// @brief Disable malloc replacement in micro_proxy.
		/// Only used by micro_proxy shared library based on MICRO_DISABLE_REPLACEMENT env. variable.
		bool disable_malloc_replacement{ false };

		/// @brief Log level, default to no log
		unsigned log_level{ 0 };

		/// @brief Log date format, as used by strftime()
		std::array<char, 64> log_date_format = { "%Y-%m-%d %H:%M:%S\0" };

		/// @brief Type of page provider, see micro_provider_type enum
		unsigned provider_type{ MicroOSProvider };

		/// @brief Default page size for non-OS page providers
		unsigned page_size{ MICRO_DEFAULT_PAGE_SIZE };

		/// @brief Memory block used for memory page provider
		char* page_memory_provider{ nullptr };

		/// @brief Memory provider size, or file provider start size, or preallocated provider size, default to 0
		std::uint64_t page_memory_size{ 0 };

		/// @brief For MicroOSPreallocProvider, MicroMemProvider and MicroFileProvider,
		/// Allow the use of OS page alloc/dealloc API when the page provider cannot allocate pages anymore.
		/// Default to true.
		bool allow_os_page_alloc{ true };

		/// @brief Growth factor for file page provider
		double grow_factor{ MICRO_DEFAULT_GROW_FACTOR };

		/// @brief If not null, filename for the file page provider
		std::array<char, MICRO_MAX_PATH> page_file_provider = { 0 };

		/// @brief If not null, directory used for the file page provider.
		/// In this case, the page_file_provider is used as a file prefix withing this directory.
		std::array<char, MICRO_MAX_PATH> page_file_provider_dir = { 0 };

		/// @brief Flags to be passed to the file page provider
		unsigned page_file_flags{ 0 };

		/// @brief Periodically print statistics in given location.
		/// The value could be a valid file path, "stdout" or "stderr".
		std::array<char, MICRO_MAX_PATH> print_stats = { 0 };

		/// @brief Tells what triggers a stats print.
		/// Possible values: 0 (no print), 1 (print every N ms), 2 (print every M allocated bytes), 3 (both)
		unsigned print_stats_trigger{ 0 };

		/// @brief If print_stats_trigger is 1 or 3, minimum elapsed time between 2 stats prints
		unsigned print_stats_ms{ 0 };

		/// @brief If print_stats_trigger is 2 or 3, minimum allocated bytes between 2 stats prints
		unsigned print_stats_bytes{ 0 };

		bool print_stats_csv{ false };

		/// @brief Validate parameters, possibly by modifying them
		parameters validate(micro_log_level l = MicroWarning) const noexcept;

		/// @brief Build from env. variables (not validated)
		static parameters from_env() noexcept;

		void print(print_callback_type callback, void* opaque) const noexcept;
		void print_stdout() const noexcept { this->print(default_print_callback, stdout); }
	};

}

#ifndef MICRO_HEADER_ONLY
namespace micro
{
	/// @brief Returns the global process parameters
	MICRO_EXPORT parameters& get_process_parameters() noexcept;
}
#else
#include "internal/parameters.cpp"
#endif

#endif
