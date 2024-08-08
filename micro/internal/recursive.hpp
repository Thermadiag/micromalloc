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

#ifndef MICRO_RECURSIVE_HPP
#define MICRO_RECURSIVE_HPP

#include "../bits.hpp"
#include "../lock.hpp"

#if defined(__linux__) || defined(__CYGWIN__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#elif defined(_MSC_VER)
#include "Windows.h"
#endif

#include <atomic>
#include <thread>

// Undefine min/max due to Windows.h inclusion without NOMINMAX defined
#ifdef min
#undef min
#undef max
#endif

#ifdef small
#undef small
#endif

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

namespace micro
{
	namespace detail
	{

		/// @brief Recusrion detection class.
		///
		/// When overriding the malloc/free functions, the
		/// malloc function might enter in infinite recursion
		/// if it triggers another malloc call.
		///
		/// This might be the case when printing information or
		/// logging statistics to a file.
		///
		/// DetectRecursion class is used to detect such recursions
		/// in order to satify recursive allocations through a
		/// different path.
		///
		/// DetectRecursion is basically a hash map of thread id
		/// with no probing strategy. Indeed, we must ensure that
		/// all recursions are detected, but we don't really care
		/// if a non recursive scenario is detected as such, as long
		/// as this rarely happen.
		///
		class MICRO_EXPORT_CLASS DetectRecursion
		{
		public:
			using Key = std::atomic<bool>;

		private:
			Key* keys{ nullptr };
			unsigned capacity{ 0 };

		public:
			struct KeyHolder
			{
				Key* k{ nullptr };
				MICRO_ALWAYS_INLINE operator const void*() const noexcept { return k; }
				MICRO_ALWAYS_INLINE ~KeyHolder() noexcept
				{
					if (k)
						k->store(false, std::memory_order_relaxed);
				}
			};

			DetectRecursion(void* p, unsigned c) noexcept
			  : keys(static_cast<Key*>(p))
			  , capacity(c)
			{
			}

			/// @brief Returns a Key * if the value was successfully inserted,
			/// null if the value already exists.
			MICRO_ALWAYS_INLINE Key* insert(std::uint32_t hash) noexcept
			{
				auto* k = keys + (hash & (capacity - 1u));
				if (!k->load(std::memory_order_relaxed) && !k->exchange(true, std::memory_order_acquire))
					return k;
				return nullptr;
			}
			/// @brief Erase entry
			MICRO_ALWAYS_INLINE void erase(Key* k) noexcept { k->store(false, std::memory_order_relaxed); }
		};

		/// @brief Global DetectRecursion object, only used by the heap overriding malloc
		inline DetectRecursion& get_detect_recursion()
		{
			static std::atomic<bool> buffer[256 << MICRO_MEMORY_LEVEL];
			static DetectRecursion detect{ buffer, sizeof(buffer) };
			return detect;
		}

	}
}

MICRO_POP_DISABLE_OLD_STYLE_CAST

#endif
