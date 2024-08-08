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

#include "../parameters.hpp"

#include <cstdio>
#include <cstdlib>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include "Windows.h"
#endif

#ifdef min
#undef min
#undef max
#endif

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

namespace micro
{
	namespace detail
	{
		static inline char* mgetenv(const char* name)
		{
#if defined(_MSC_VER) || defined(__MINGW32__)
			static thread_local char buf[4096];
			size_t len = static_cast<size_t>(GetEnvironmentVariable(name, buf, sizeof(buf) - 1));
			if (len == 0) {
				return nullptr;
			}
			else {
				if (len < sizeof(buf) - 1) {
					buf[len] = 0;
					return buf;
				}
				return nullptr;
			}
#else
			return std::getenv(name);
#endif
		}

#if 0
		static inline int mputenv(const char* str)
		{
#if defined(_MSC_VER) || defined(__MINGW32__)
			static thread_local char first[4096];
			static thread_local char second[4096];
			char* eqpos = strchr(const_cast<char*>(str), (int)'=');
			if (eqpos) {
				size_t namelen = (size_t)eqpos - (size_t)str;
				if ((size_t)namelen >= sizeof(first))
					return -1;
				strncpy(first, str, namelen);
				first[namelen] = '\0';
				size_t valuelen = strlen(eqpos + 1);
				if ((size_t)valuelen >= sizeof(second))
					return -1;
				strncpy(second, eqpos + 1, valuelen);
				second[valuelen] = '\0';
				SetEnvironmentVariable(first, second);
				return 0;
			}
			return -1;

#else
			return putenv((char*)str);
#endif
		}

#endif // 0

	}

	MICRO_EXPORT_CLASS_MEMBER parameters parameters::validate(micro_log_level l) const noexcept
	{
		// Check all parameters.
		// Print error messages if a parameter is invalid,
		// and replace it with the default value.

		parameters p = *this;
		if (p.small_alloc_threshold > MICRO_MAX_SMALL_ALLOC_THRESHOLD) {
			if (l != MicroNoLog)
				print_safe(stderr, "WARNING invalid small_alloc_threshold value: ", p.small_alloc_threshold, "\n");
			p.small_alloc_threshold = MICRO_MAX_SMALL_ALLOC_THRESHOLD;
		}
		p.small_alloc_threshold &= ~7u;

		if (p.max_arenas & (p.max_arenas - 1))
			if (p.max_arenas) {
				p.max_arenas = 1u << bit_scan_reverse_32(p.max_arenas);
			}

		if (p.max_arenas > MICRO_MAX_ARENAS) {
			if (l != MicroNoLog)
				print_safe(stderr, "WARNING max_arenas value too high: ", p.max_arenas, "\n");
			p.max_arenas = MICRO_MAX_ARENAS;
		}
		if (p.max_arenas == 0) {
			p.max_arenas = 1;
			if (l != MicroNoLog)
				print_safe(stderr, "WARNING max_arenas value is 0: set to 1\n");
		}

		if ((p.page_size & (p.page_size - 1)) != 0) {
			if (l != MicroNoLog)
				print_safe(stderr, "WARNING invalid page_size value: ", p.page_size, "\n");
			p.page_size = MICRO_DEFAULT_PAGE_SIZE;
		}
		else if (page_size < MICRO_MINIMUM_PAGE_SIZE || p.page_size > MICRO_MAXIMUM_PAGE_SIZE) {
			if (l != MicroNoLog)
				print_safe(stderr, "WARNING invalid page_size value: ", p.page_size, "\n");
			p.page_size = MICRO_DEFAULT_PAGE_SIZE;
		}

		if (p.provider_type > MicroFileProvider) {
			if (l != MicroNoLog)
				print_safe(stderr, "WARNING invalid provider_type value: ", p.provider_type, "\n");
			p.provider_type = MicroOSProvider;
		}

		if (p.page_file_flags > MicroGrowing)
			p.page_file_flags = MicroGrowing;

		if (p.grow_factor <= 0 || p.grow_factor > 8) {
			if (l != MicroNoLog)
				print_safe(stderr, "WARNING invalid grow_factor value: ", p.grow_factor, "\n");
			p.grow_factor = MICRO_DEFAULT_GROW_FACTOR;
		}

		if (p.print_stats_trigger > 7) {
			if (l != MicroNoLog)
				print_safe(stderr, "WARNING invalid print_stats_trigger value: ", p.print_stats_trigger, "\n");
			p.print_stats_trigger = 0;
		}

		if (p.log_level > MicroInfo)
			p.log_level = MicroInfo;

		return p;
	}

	MICRO_EXPORT_CLASS_MEMBER parameters parameters::from_env() noexcept
	{
		// Build parameter object using env. variables

		parameters p;
		if (char* env = detail::mgetenv("MICRO_SMALL_ALLOC_THRESHOLD")) {
			char* end = env + strlen(env);
			p.small_alloc_threshold = (static_cast<unsigned>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_SMALL_ALLOC_FROM_RADIX_TREE")) {
			char* end = env + strlen(env);
			p.allow_small_alloc_from_radix_tree = (static_cast<bool>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_DEPLETE_ARENAS")) {
			char* end = env + strlen(env);
			p.deplete_arenas = (static_cast<unsigned>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_MAX_ARENAS")) {
			char* end = env + strlen(env);
			p.max_arenas = (static_cast<unsigned>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_DISABLE_REPLACEMENT")) {
			char* end = env + strlen(env);
			p.disable_malloc_replacement = (static_cast<bool>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_BACKEND_MEMORY")) {
			char* end = env + strlen(env);
			p.backend_memory = static_cast<uint64_t>(std::strtoll(env, &end, 10));
		}
		if (char* env = detail::mgetenv("MICRO_MEMORY_LIMIT")) {
			char* end = env + strlen(env);
			p.memory_limit = static_cast<uint64_t>(std::strtoll(env, &end, 10));
		}
		if (char* env = detail::mgetenv("MICRO_LOG_LEVEL")) {
			char* end = env + strlen(env);
			p.log_level = (static_cast<unsigned>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_LOG_DATE_FORMAT")) {
			size_t len = std::min(strlen(env), sizeof(p.log_date_format) - 1);
			memcpy(p.log_date_format.data(), env, len);
			p.log_date_format[len] = 0;
		}
		if (char* env = detail::mgetenv("MICRO_PAGE_SIZE")) {
			char* end = env + strlen(env);
			p.page_size = (static_cast<unsigned>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_GROW_FACTOR")) {
			char* end = env + strlen(env);
			p.grow_factor = std::strtod(env, &end);
		}
		if (char* env = detail::mgetenv("MICRO_PROVIDER_TYPE")) {
			char* end = env + strlen(env);
			p.provider_type = (static_cast<unsigned>(std::strtoll(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_PAGE_FILE_PROVIDER")) {
			size_t len = std::min(strlen(env), sizeof(p.page_file_provider) - 1);
			memcpy(p.page_file_provider.data(), env, len);
			p.page_file_provider[len] = 0;
		}
		if (char* env = detail::mgetenv("MICRO_PAGE_FILE_PROVIDER_DIR")) {
			size_t len = std::min(strlen(env), sizeof(p.page_file_provider_dir) - 1);
			memcpy(p.page_file_provider_dir.data(), env, len);
			p.page_file_provider_dir[len] = 0;
		}
		if (char* env = detail::mgetenv("MICRO_PAGE_MEMORY_SIZE")) {
			char* end = env + strlen(env);
			p.page_memory_size = (static_cast<uint64_t>(std::strtoll(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_PAGE_FILE_FLAGS")) {
			char* end = env + strlen(env);
			p.page_file_flags = (static_cast<unsigned>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_ALLOW_OS_PAGE_ALLOC")) {
			char* end = env + strlen(env);
			p.allow_os_page_alloc = (static_cast<bool>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_PRINT_STATS")) {
			size_t len = std::min(strlen(env), sizeof(p.print_stats) - 1);
			memcpy(p.print_stats.data(), env, len);
			p.print_stats[len] = 0;
		}
		if (char* env = detail::mgetenv("MICRO_PRINT_STATS_TRIGGER")) {
			char* end = env + strlen(env);
			p.print_stats_trigger = (static_cast<unsigned>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_PRINT_STATS_MS")) {
			char* end = env + strlen(env);
			p.print_stats_ms = (static_cast<unsigned>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_PRINT_STATS_BYTES")) {
			char* end = env + strlen(env);
			p.print_stats_bytes = (static_cast<unsigned>(std::strtol(env, &end, 10)));
		}
		if (char* env = detail::mgetenv("MICRO_PRINT_STATS_CSV")) {
			char* end = env + strlen(env);
			p.print_stats_csv = (static_cast<bool>(std::strtol(env, &end, 10)));
		}

		return p;
	}

	struct RemoveStdBuffers
	{
		RemoveStdBuffers()
		{
			// Disable stdout/stderr buffering (and potential allocations)
			setvbuf(stdout, nullptr, _IONBF, 0);
			setvbuf(stderr, nullptr, _IONBF, 0);
		}
	};

	MICRO_HEADER_ONLY_EXPORT_FUNCTION parameters& get_process_parameters() noexcept
	{
		static RemoveStdBuffers rm_buffers;
		static parameters inst = parameters::from_env();
		return inst;
	}

	MICRO_HEADER_ONLY_EXPORT_FUNCTION void parameters::print(print_callback_type callback, void* opaque) const noexcept
	{
		// Dump parameters

		print_generic(callback, opaque, MicroNoLog, nullptr, "small_alloc_threshold\t%u\n", small_alloc_threshold);
		print_generic(callback, opaque, MicroNoLog, nullptr, "allow_small_alloc_from_radix_tree\t%u\n", static_cast<unsigned>(allow_small_alloc_from_radix_tree));
		print_generic(callback, opaque, MicroNoLog, nullptr, "deplete_arenas\t%u\n", deplete_arenas);
		print_generic(callback, opaque, MicroNoLog, nullptr, "max_arenas\t%u\n", max_arenas);
		print_generic(callback, opaque, MicroNoLog, nullptr, "backend_memory\t" MICRO_U64F "\n", backend_memory);
		print_generic(callback, opaque, MicroNoLog, nullptr, "memory_limit\t" MICRO_U64F "\n", memory_limit);
		print_generic(callback, opaque, MicroNoLog, nullptr, "log_level\t%u\n", log_level);
		print_generic(callback, opaque, MicroNoLog, nullptr, "page_size\t%u\n", page_size);
		print_generic(callback, opaque, MicroNoLog, nullptr, "grow_factor\t%f\n", grow_factor);
		print_generic(callback, opaque, MicroNoLog, nullptr, "disable_malloc_replacement\t%u\n", static_cast<unsigned>(disable_malloc_replacement));

		print_generic(callback, opaque, MicroNoLog, nullptr, "provider_type\t%u\n", provider_type);
		print_generic(callback, opaque, MicroNoLog, nullptr, "page_memory_provider\t%p\n", static_cast<void*>(page_memory_provider));
		print_generic(callback, opaque, MicroNoLog, nullptr, "page_memory_size\t" MICRO_U64F "\n",static_cast<uint64_t>(page_memory_size));

		print_generic(callback, opaque, MicroNoLog, nullptr, "page_file_provider\t%s\n", page_file_provider.data()[0] ? page_file_provider.data() : "");
		print_generic(callback, opaque, MicroNoLog, nullptr, "page_file_provider_dir\t%s\n", page_file_provider_dir.data()[0] ? page_file_provider_dir.data() : "");
		print_generic(callback, opaque, MicroNoLog, nullptr, "page_file_flags\t%u\n", static_cast<unsigned>(page_file_flags));
		print_generic(callback, opaque, MicroNoLog, nullptr, "allow_os_page_alloc\t%u\n", static_cast<unsigned>(allow_os_page_alloc));

		print_generic(callback, opaque, MicroNoLog, nullptr, "print_stats\t%s\n", print_stats.data());
		print_generic(callback, opaque, MicroNoLog, nullptr, "print_stats_trigger\t%u\n", static_cast<unsigned>(print_stats_trigger));
		print_generic(callback, opaque, MicroNoLog, nullptr, "print_stats_ms\t%u\n", static_cast<unsigned>(print_stats_ms));
		print_generic(callback, opaque, MicroNoLog, nullptr, "print_stats_bytes\t%u\n", static_cast<unsigned>(print_stats_bytes));
		print_generic(callback, opaque, MicroNoLog, nullptr, "print_stats_csv\t%u\n", static_cast<unsigned>(print_stats_csv));
	}
}

MICRO_POP_DISABLE_OLD_STYLE_CAST
