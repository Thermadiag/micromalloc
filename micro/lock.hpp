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

#ifndef MICRO_LOCK_HPP
#define MICRO_LOCK_HPP

/** @file */

#ifdef _MSC_VER
// Remove useless warnings ...needs to have dll-interface to be used by clients of class...
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <type_traits>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include "Windows.h"
#else
// Assume pthread is available
#include <pthread.h>
#define MICRO_USE_PTHREAD
#endif

#include "bits.hpp"
#include "internal/defines.hpp"

// Undefine min/max due to Windows.h inclusion without NOMINMAX defined
#ifdef min
#undef min
#undef max
#endif

#ifdef small
#undef small
#endif

namespace micro
{

#ifndef MICRO_NO_LOCK

	/// @brief Lightweight and fast spinlock implementation based on https://rigtorp.se/spinlock/
	///
	class MICRO_EXPORT_CLASS spinlock
	{
		std::atomic<bool> d_lock;

	public:
		constexpr spinlock() noexcept
		  : d_lock(0)
		{
		}

		MICRO_DELETE_COPY(spinlock)

		MICRO_ALWAYS_INLINE void lock() noexcept
		{
			for (;;) {
				// Optimistically assume the lock is free on the first try
				if (MICRO_LIKELY(!d_lock.exchange(true, std::memory_order_acquire)))
					return;

				// Wait for lock to be released without generating cache misses
				while (d_lock.load(std::memory_order_relaxed))
					// Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
					// hyper-threads
					std::this_thread::yield();
			}
		}
		MICRO_ALWAYS_INLINE bool is_locked() const noexcept { return d_lock.load(std::memory_order_relaxed); }
		MICRO_ALWAYS_INLINE bool try_lock() noexcept
		{
			// First do a relaxed load to check if lock is free in order to prevent
			// unnecessary cache misses if someone does while(!try_lock())
			return !d_lock.load(std::memory_order_relaxed) && !d_lock.exchange(true, std::memory_order_acquire);
		}
		MICRO_ALWAYS_INLINE bool try_lock_fast() noexcept { return !d_lock.exchange(true, std::memory_order_acquire); }
		MICRO_ALWAYS_INLINE void unlock() noexcept
		{
			MICRO_ASSERT_DEBUG(d_lock == true, "");
			d_lock.store(false, std::memory_order_release);
		}

		// Shared lock functions
		MICRO_ALWAYS_INLINE void lock_shared() noexcept { lock(); }
		MICRO_ALWAYS_INLINE void unlock_shared() noexcept { unlock(); }
		MICRO_ALWAYS_INLINE bool try_lock_shared() noexcept { return try_lock(); }
	};

	/// @brief An unfaire read-write spinlock class
	///
	template<class LockType = std::uint32_t>
	class MICRO_EXPORT_CLASS shared_spinner
	{
		static_assert(std::is_unsigned<LockType>::value, "shared_spinner only supports unsigned atomic types!");
		using lock_type = LockType;

		std::atomic<lock_type> d_lock;

	public:
		static constexpr lock_type write = 1;
		static constexpr lock_type read = 2;

		constexpr shared_spinner() noexcept
		  : d_lock(0)
		{
		}
		MICRO_DELETE_COPY(shared_spinner)

		MICRO_ALWAYS_INLINE LockType value() const noexcept { return d_lock.load(std::memory_order_relaxed); }

		MICRO_ALWAYS_INLINE void lock() noexcept
		{
			for (;;) {
				// Optimistically assume the lock is free on the first try
				if (MICRO_LIKELY(try_lock()))
					return;
				// Wait for the lock to be free
				while (d_lock.load(std::memory_order_relaxed) != 0)
					std::this_thread::yield();
			}
		}
		MICRO_ALWAYS_INLINE void unlock() noexcept
		{
			MICRO_ASSERT_DEBUG(d_lock & write, "");
			d_lock.store(0, std::memory_order_release);
		}
		MICRO_ALWAYS_INLINE void lock_shared() noexcept
		{
			while (MICRO_UNLIKELY(!try_lock_shared()))
				std::this_thread::yield();
		}
		MICRO_ALWAYS_INLINE void unlock_shared() noexcept
		{
			MICRO_ASSERT_DEBUG(d_lock > 0, "");
			d_lock.fetch_sub(read, std::memory_order_release);
		}
		MICRO_ALWAYS_INLINE bool try_lock() noexcept
		{
			lock_type expect = 0;
			return d_lock.compare_exchange_strong(expect, write, std::memory_order_acq_rel);
		}
		MICRO_ALWAYS_INLINE bool try_lock_fast() noexcept { return try_lock(); }
		MICRO_ALWAYS_INLINE bool try_lock_shared() noexcept
		{
			// Using compare_exchange_strong in high contention scenarios 
			// is faster than successive fetch_add/fetch_sub
			auto l = d_lock.load(std::memory_order_relaxed);
			if MICRO_CONSTEXPR (sizeof(lock_type) == 1)
				// For one byte lock, check for saturation
				return !(l & write || l == (256u - read)) && d_lock.compare_exchange_strong(l, l + read);
			else
				return !(l & write) && d_lock.compare_exchange_strong(l, l + read);
		}
	};

#else

	class MICRO_EXPORT_CLASS spinlock
	{
		bool d_lock{ false };

	public:
		constexpr spinlock() noexcept {}

		MICRO_DELETE_COPY(spinlock)

		void lock() noexcept {}
		bool is_locked() const noexcept { return false; }
		bool try_lock() noexcept { return true; }
		bool try_lock_fast() noexcept { return true; }
		void unlock() noexcept {}
		void lock_shared() noexcept {}
		void unlock_shared() noexcept {}
		bool try_lock_shared() noexcept { return true; }
	};

	/// @brief An unfaire read-write spinlock class
	///
	template<class LockType = std::uint32_t>
	class MICRO_EXPORT_CLASS shared_spinner
	{
		static_assert(std::is_unsigned<LockType>::value, "shared_spinner only supports unsigned atomic types!");
		using lock_type = LockType;

		lock_type d_lock{ 0 };

	public:
		constexpr shared_spinner() noexcept {}
		MICRO_DELETE_COPY(shared_spinner)

		LockType value() const noexcept { return 0; }

		void lock() noexcept {}
		void unlock() noexcept {}
		void lock_shared() noexcept {}
		void unlock_shared() noexcept {}
		bool try_lock() noexcept { return true; }
		bool try_lock_shared() noexcept { return true; }
	};

#endif

	/// @brief Default read-write spinlock type
	using shared_spinlock = shared_spinner<>;

	/// @brief 8 bits read-write spinlock type
	using tiny_shared_spinlock = shared_spinner<std::uint8_t>;

	namespace detail
	{

#ifndef MICRO_NO_LOCK
		/// @brief Small class used to generate unique thread identifiers.
		/// The thread identifier is used within MemoryManager to select
		/// the right arena. ThreadCounter also keep track of the number
		/// of threads currently alive and using at least one arena.
		/// This allows to potentially reduce the subset of active arenas
		/// and decrease the overall memory consumption.
		class ThreadCounter
		{
			struct Data
			{
				static constexpr unsigned slots = 16;
				static constexpr unsigned max_threads = slots * 64; // Maximum number of thread ids that can be recycled

				std::uint64_t threads[slots]; // One bit per thread
				volatile unsigned count;      // Active thread count
				volatile unsigned max_count;  // Max thread count
				volatile unsigned mask;	      // Closest power of 2 for thread count minus one
				volatile unsigned max_mask;   // Closest power of 2 for max thread count minus one
				spinlock lock;		      // Global lock

				Data() noexcept
				  : count(0)
				  , max_count(0)
				  , mask(0)
				  , max_mask(0)
				{
					memset(static_cast<void*>(threads), 0, sizeof(threads));
				}

				/// @brief Compute closest power of 2 minus one for thread count
				static unsigned mask_from_count(unsigned cnt) noexcept
				{
					unsigned mask = cnt ? (1u << bit_scan_reverse_32(cnt)) - 1u : 0u;
					// if (cnt == 0 || mask == cnt - 1) return mask;

					// Possible: round to upper power of 2 instead of closest
					return mask * 2 + 1;

					// unsigned mask2 = (1u << (bit_scan_reverse_32(cnt) + 1u)) - 1u;
					// if (mask2 - cnt <= cnt - mask)
					//	mask = mask2;
					// return mask;
				}

				/// @brief Build thread index
				unsigned build_idx() noexcept
				{
					static std::atomic<unsigned> index{ max_threads };

					std::lock_guard<spinlock> ll(lock);

					// update thread counts and masks
					count = count + 1;
					mask = mask_from_count(count);
					if (count > max_count) {
						max_count = count;
						max_mask = mask_from_count(max_count);
					}

					// Recycle thread id if possible.
					// Strangely, this is faster than incrementing a unique id...
					for (unsigned i = 0; i < slots; ++i) {
						auto val = ~threads[i];
						if (val != 0) {
							unsigned idx = bit_scan_forward_64(val);
							threads[i] |= 1ull << idx;
							return idx;
						}
					}
					// No possible recycling: increment unique id starting from max_threads
					return index.fetch_add(1);
				}
				/// @brief Remove thread index on thread destruction
				void remove_idx(unsigned idx) noexcept
				{
					std::lock_guard<spinlock> ll(lock);
					count = count - 1;
					mask = mask_from_count(count);
					threads[idx / 64] &= ~(1ull << (idx & 63));
				}
			};

			static MICRO_ALWAYS_INLINE Data& data() noexcept
			{
				static Data data;
				return data;
			}

			/// @brief Thread local storage of thread identifier.
			/// Since the thread id is recycled on thread destruction, we need
			/// to provide a non empty class destructor.
			/// On gcc, this produces weird and slow code that
			/// introduces big virtual memory allocation...
			/// That's why we use pthread key mechanism to register
			/// a cleanup function on thread destruction.
			struct THData
			{
				struct Id
				{
					unsigned idx;
#ifdef MICRO_USE_PTHREAD
					pthread_key_t k;
#else
					unsigned k;
#endif
				};
				static void _cleanup(void* arg)
				{

					Id* id = static_cast<Id*>(arg);
					if (id->idx < Data::max_threads)
						data().remove_idx(id->idx);
#ifdef MICRO_USE_PTHREAD
					pthread_key_delete(id->k);
#endif
				}

				Id id;
				THData() noexcept
				  : id{ data().build_idx(), 0 }
				{
#ifdef MICRO_USE_PTHREAD

					pthread_key_create(&id.k, _cleanup);
					pthread_setspecific(id.k, &id);
#endif
				}

#ifndef MICRO_USE_PTHREAD
				~THData() { _cleanup(&id); }
#endif
			};

		public:
			/// @brief Returns current thread id
			static MICRO_ALWAYS_INLINE unsigned get_thread_id() noexcept
			{
				MICRO_PUSH_DISABLE_EXIT_TIME_DESTRUCTOR
				thread_local THData data;
				MICRO_POP_DISABLE_EXIT_TIME_DESTRUCTOR
				return data.id.idx;
			}
			/// @brief Returns current number of threads
			static MICRO_ALWAYS_INLINE unsigned get_thread_count() noexcept { return data().count; }
			/// @brief Returns maximum number of threads reach so far
			static MICRO_ALWAYS_INLINE unsigned get_max_thread_count() noexcept { return data().max_count; }
			static MICRO_ALWAYS_INLINE unsigned get_mask() noexcept { return data().mask; }
			static MICRO_ALWAYS_INLINE unsigned get_max_mask() noexcept { return data().max_mask; }
		};

#endif

		MICRO_ALWAYS_INLINE std::uint64_t Mixin64(std::uint64_t a) noexcept
		{
			a ^= a >> 23;
			a *= 0x2127599bf4325c37ULL;
			a ^= a >> 47;
			return a;
		}
		template<size_t Size>
		struct MICRO_EXPORT_CLASS Mixin
		{
			static MICRO_ALWAYS_INLINE size_t mix(size_t a) noexcept { return static_cast<size_t>(Mixin64(a)); }
		};
		template<>
		struct MICRO_EXPORT_CLASS Mixin<8>
		{
			static MICRO_ALWAYS_INLINE size_t mix(size_t a) noexcept
			{
#ifdef MICRO_HAS_FAST_UMUL128
				// abseil mixin way
				static constexpr uint64_t k = 0xde5fb9d2630458e9ULL;
				uint64_t l, h;
				umul128(a, k, &l, &h);
				return static_cast<size_t>(h + l);
#else
				return static_cast<size_t>(Mixin64(a));
#endif
			}
		};
		/// @brief Mix input hash value for better avalanching
		static MICRO_ALWAYS_INLINE size_t HashFinalize(size_t h) noexcept { return detail::Mixin<sizeof(size_t)>::mix(h); }

	}

#ifndef MICRO_NO_LOCK

	/// @brief Returns the maximum reach number of threads
	MICRO_ALWAYS_INLINE unsigned get_max_thread_count() noexcept { return detail::ThreadCounter::get_max_thread_count(); }

	/// @brief Returns thread mask for arena selection
	MICRO_ALWAYS_INLINE unsigned get_thread_mask() noexcept { return detail::ThreadCounter::get_mask(); }
	MICRO_ALWAYS_INLINE unsigned get_thread_max_mask() noexcept { return detail::ThreadCounter::get_max_mask(); }

	/// @brief Returns current thread id
	MICRO_ALWAYS_INLINE size_t this_thread_id() noexcept { return detail::ThreadCounter::get_thread_id(); }

	/// @brief Returns current thread id suitable to select an arena.
	/// Introduces flickering of the thread id for mono thread cases
	/// as it greatly reduces the memory footprint.
	MICRO_ALWAYS_INLINE size_t this_thread_id_for_arena() noexcept
	{
		static volatile bool id = true;
		unsigned res = detail::ThreadCounter::get_thread_id();
		// Only 1 thread: flicker
		if (detail::ThreadCounter::get_max_thread_count() == 1) {
			id = !id;
			res += static_cast<unsigned>(id);
		}
		return res;
	}

	/// @brief Straightforward recursive spinlock implementation
	///
	class MICRO_EXPORT_CLASS recursive_spinlock
	{
		spinlock d_lock;
		int d_count{ 0 };
		size_t d_id{ 0 };

		bool try_lock(size_t id) noexcept
		{
			std::lock_guard<spinlock> ll(d_lock);
			if (d_count == 0) {
				MICRO_ASSERT_DEBUG(d_id == 0, "");
				d_id = id;
				d_count = 1;
				return true;
			}
			if (id == d_id) {
				++d_count;
				return true;
			}
			return false;
		}

	public:
		constexpr recursive_spinlock() noexcept {}
		MICRO_DELETE_COPY(recursive_spinlock)

		bool try_lock() noexcept { return try_lock(this_thread_id()); }
		void lock() noexcept
		{
			auto id = this_thread_id();
			while (!try_lock(id))
				std::this_thread::yield();
		}
		void unlock() noexcept
		{
			std::lock_guard<spinlock> ll(d_lock);
			if (--d_count == 0)
				d_id = 0;
			MICRO_ASSERT_DEBUG(d_count >= 0, "");
		}
	};

#else

	/// @brief Returns the maximum reach number of threads
	MICRO_ALWAYS_INLINE unsigned get_max_thread_count() noexcept { return 1; }

	MICRO_ALWAYS_INLINE unsigned get_thread_mask() noexcept { return 0; }
	MICRO_ALWAYS_INLINE unsigned get_thread_max_mask() noexcept { return 0; }

	/// @brief Returns current thread id
	MICRO_ALWAYS_INLINE size_t this_thread_id() noexcept { return 0; }

	/// @brief Returns current thread id suitable to select an arena.
	/// Introduces flickering of the thread id for mono thread cases
	/// as it greatly reduces the memory footprint.
	MICRO_ALWAYS_INLINE size_t this_thread_id_for_arena() noexcept { return 0; }

	/// @brief Straightforward recursive spinlock implementation
	///
	class MICRO_EXPORT_CLASS recursive_spinlock
	{
		spinlock d_lock;
		int d_count{ 0 };
		size_t d_id{ 0 };

	public:
		constexpr recursive_spinlock() noexcept {}
		MICRO_DELETE_COPY(recursive_spinlock)

		bool try_lock() noexcept { return true; }
		void lock() noexcept {}
		void unlock() noexcept {}
	};
#endif

	/// @brief Returns hash value for current thread id
	MICRO_ALWAYS_INLINE size_t this_thread_id_hash() { return detail::HashFinalize(this_thread_id()); }

}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
