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

#ifndef MICRO_HEADERS_HPP
#define MICRO_HEADERS_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <thread>

#include "../logger.hpp"
#include "../parameters.hpp"
#include "defines.hpp"
#include "page_provider.hpp"
#include "statistics.hpp"

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

// Undefine min/max due to Windows.h inclusion without NOMINMAX defined
#ifdef min
#undef min
#undef max
#endif

#ifdef _MSC_VER
#define MICRO_EXPORT_HEADER MICRO_EXPORT_CLASS
#else
#define MICRO_EXPORT_HEADER
#endif

namespace micro
{
	namespace detail
	{
		/// @brief 3 bytes unsigned integer.
		/// Only used when MICRO_USE_NODE_LOCK is 1 and MICRO_MEMORY_LEVEL is 4,
		/// in order to represent free chunks of up to 2MB.
		struct UInt24
		{
			UInt24() = default;
			UInt24(uint32_t value) { from_uint(value); }
			void from_uint(uint32_t value) noexcept
			{
				lsb = static_cast<std::uint8_t>(value & 0xffu);
				value >>= 8u;
				midb = static_cast<std::uint8_t>(value & 0xffu);
				msb = static_cast<std::uint8_t>(value >> 8u);
			}
			template<class T>
			void from_integer(T value) noexcept
			{
				from_uint(static_cast<unsigned>(value));
			}
			uint32_t to_uint() const noexcept { return (static_cast<unsigned>(msb) << (8u * 2u)) + (static_cast<unsigned>(midb) << 8u) + static_cast<unsigned>(lsb); }
			operator uint32_t() const noexcept { return to_uint(); }

			UInt24& operator=(const UInt24& o) noexcept
			{
				lsb = o.lsb;
				midb = o.midb;
				msb = o.msb;
				return *this;
			}
			UInt24& operator=(uint32_t value) noexcept
			{
				from_uint(value);
				return *this;
			}
			UInt24& operator+=(UInt24 o) noexcept
			{
				from_uint(to_uint() + o.to_uint());
				return *this;
			}
			UInt24& operator+=(uint32_t o) noexcept
			{
				from_uint(to_uint() + o);
				return *this;
			}
			UInt24& operator-=(UInt24 o) noexcept
			{
				from_uint(to_uint() - o.to_uint());
				return *this;
			}
			UInt24& operator-=(uint32_t o) noexcept
			{
				from_uint(to_uint() - o);
				return *this;
			}

			std::uint8_t msb = 0;
			std::uint8_t midb = 0;
			std::uint8_t lsb = 0;
		};

		struct PageRunHeader;

		/// @brief Header structure shared by all types of allocation (small, medium, big, blocks of tiny allocations)
		class MICRO_EXPORT_HEADER alignas(std::uint64_t) SmallChunkHeader
		{
			using Data16 = std::pair<std::uint64_t, std::uint64_t>;

		public:
			// Block guard, only used to detect buffer overrun as well as address validity.
			// Could be replaced by a hash value like scudo.
			std::uint16_t guard{ MICRO_BLOCK_GUARD };

			// Block status, one of MICRO_ALLOC_SMALL, MICRO_ALLOC_SMALL_BLOCK, MICRO_ALLOC_MEDIUM, MICRO_ALLOC_BIG, MICRO_ALLOC_FREE
			std::uint16_t status{ 0 };

			// Offset in 16 bytes granularity to the parent PageRunHeader
			std::uint32_t offset_bytes{ 0 };

			SmallChunkHeader() {}
			SmallChunkHeader(std::uint16_t _status, std::uint32_t _offset_bytes)
			  : status(_status)
			  , offset_bytes(_offset_bytes)
			{
			}

			MICRO_ALWAYS_INLINE void* data() noexcept { return this + 1; }
			MICRO_ALWAYS_INLINE PageRunHeader* parent() noexcept { return reinterpret_cast<PageRunHeader*>(reinterpret_cast<Data16*>(this) - offset_bytes); }
			MICRO_ADD_CASTS(SmallChunkHeader)
		};

		/// @brief Header structure for big allocations
		class MICRO_EXPORT_HEADER alignas(std::uint64_t) BigChunkHeader
		{
		public:
			std::uint64_t size{ 0 }; // size in bytes of the chunk
			SmallChunkHeader th;

			MICRO_ADD_CASTS(BigChunkHeader)
		};

		/// @brief Header structure for medium allocations
		class MICRO_EXPORT_HEADER alignas(16) MediumChunkHeader
		{
		public:
			// Support for linked list of MediumChunkHeader.
			// The Link structure is stored AFTER the MediumChunkHeader itself.
			struct Links
			{
				MediumChunkHeader* prev;
				MediumChunkHeader* next;
			};

			// Offset in 16 bytes granularity to the previous chunk (or 0 if first chunk)
			std::uint32_t offset_prev;

			// Now, in 32 bits, store the chunk size 16 bytes granularity,
			// as well as a spinlock if MICRO_USE_NODE_LOCK is 1.

#if MICRO_USE_NODE_LOCK
			using lock_type = spinlock;
#if MICRO_MEMORY_LEVEL == 4
			// We need 3 bytes to represent up to 2MB with a granularity of 16 bytes
			using elems_type = UInt24;
#else
			// For up to 1MB free chunks, 16 bits is enough with a granularity of 16 bytes
			using elems_type = std::uint16_t;
#endif
			lock_type lock;
			elems_type elems;

			MICRO_ALWAYS_INLINE lock_type* get_lock() noexcept { return &this->lock; }
#else
			using elems_type = std::uint32_t;
			elems_type elems; // chunk extent in 16 bytes granularity
#endif

			// Small chunk header defined above
			SmallChunkHeader th;

			MediumChunkHeader() noexcept
			  : offset_prev(0)
			  , elems(0)
			{
			}
			MediumChunkHeader(std::uint32_t _offset_prev, std::uint32_t _elems, std::uint16_t _status, std::uint32_t _offset_bytes) noexcept
			  : offset_prev(_offset_prev)
			  , elems(static_cast<elems_type>(_elems))
			  , th(_status, _offset_bytes)
			{
				static_assert(sizeof(MediumChunkHeader) == 16, "");
			}
			template<class T>
			MICRO_ALWAYS_INLINE void set_elems(T val) noexcept
			{
				elems = static_cast<elems_type>(val);
			}
			MICRO_ADD_CASTS(MediumChunkHeader)
			MICRO_ALWAYS_INLINE PageRunHeader* parent() noexcept { return reinterpret_cast<PageRunHeader*>(this - th.offset_bytes); }
			MICRO_ALWAYS_INLINE unsigned block_bytes() const noexcept { return (elems + 1u) << MICRO_ELEM_SHIFT; }
			MICRO_ALWAYS_INLINE Links* links() noexcept { return reinterpret_cast<Links*>(this + 1); }
			MICRO_ALWAYS_INLINE MediumChunkHeader* prev() noexcept { return links()->prev; }
			MICRO_ALWAYS_INLINE MediumChunkHeader* next() noexcept { return links()->next; }
			MICRO_ALWAYS_INLINE void set_prev(MediumChunkHeader* p) noexcept { links()->prev = p; }
			MICRO_ALWAYS_INLINE void set_next(MediumChunkHeader* n) noexcept { links()->next = n; }
		};

		/// @brief Header structure for page runs (multiple contiguous pages)
		struct MICRO_EXPORT_HEADER alignas(16) PageRunHeader
		{

#if MICRO_USE_FIRST_ALIGNED_CHUNK
			SmallChunkHeader header;
#endif
			// Owner arena (if any) or parent MemoryManager for big allocations
			void* arena;

			PageRunHeader* left_free;
			PageRunHeader* right_free;

			// Full size in bytes of the run
			std::uint64_t size_bytes;

			// Linked list of page run
			PageRunHeader* left;
			PageRunHeader* right;

			shared_spinlock lock;

			// Location of tiny pools,
			// Use to remove ambiguities on deallocation

			static constexpr unsigned pool_bits_count = static_cast<unsigned>(MICRO_BLOCK_SIZE / MICRO_ALIGNED_POOL);

			std::atomic<std::uint64_t> pool_bits[pool_bits_count >= 64u ? pool_bits_count / 64u : 1];

			unsigned pool_idx(void* pool_addr) noexcept { return static_cast<unsigned>((static_cast<char*>(pool_addr) - as_char()) / MICRO_ALIGNED_POOL); }
			void set_pool(void* pool_addr) noexcept
			{
				unsigned idx = pool_idx(pool_addr);
				pool_bits[idx / 64].fetch_or(1ull << (idx & 63));
			}
			void unset_pool(void* pool_addr) noexcept
			{
				unsigned idx = pool_idx(pool_addr);
				pool_bits[idx / 64].fetch_and(~(1ull << (idx & 63)));
			}
			bool test_pool(void* pool_addr) noexcept
			{
				unsigned idx = pool_idx(pool_addr);
				return pool_bits[idx / 64].load() & (1ull << (idx & 63));
			}

			// Linked list of free page runs

			void insert_free(PageRunHeader* after) noexcept
			{
				right_free = after;
				left_free = after->left_free;
				left_free->right_free = right_free->left_free = this;
			}
			void remove_free() noexcept
			{
				right_free->left_free = left_free;
				left_free->right_free = right_free;
				right_free = left_free = this;
			}

			// Linked list of all page runs

			void insert(PageRunHeader* after) noexcept
			{
				right = after;
				left = after->left;
				left->right = right->left = this;
			}
			void remove() noexcept
			{
				right->left = left;
				left->right = right;
				right = left = this;
			}

			MICRO_ADD_CASTS(PageRunHeader)

			// full page run size
			MICRO_ALWAYS_INLINE std::uint64_t run_size() const noexcept { return size_bytes; }

			// start of usable memory block
			MICRO_ALWAYS_INLINE char* start() noexcept { return (this + 1)->as_char(); }
			// Past-the-end pointer
			MICRO_ALWAYS_INLINE char* end() noexcept { return as_char() + size_bytes; }
		};

		class BaseMemoryManager;
		/// @brief Base class for BaseMemoryManager, provides linked list support
		class MICRO_EXPORT_HEADER BaseMemoryManagerIter
		{
			friend class BaseMemoryManager;
			friend class MemoryManager;

		protected:
			BaseMemoryManagerIter* left;
			BaseMemoryManagerIter* right;

		public:
			BaseMemoryManagerIter() noexcept { left = right = this; }
			MICRO_DELETE_COPY(BaseMemoryManagerIter)
		};

		/// @brief Base memory manager class, inherited by MemoryManager.
		/// Used to pass BaseMemoryManager pointers to classes that do
		/// not need to know the full MemoryManager implementation.
		class MICRO_EXPORT_HEADER BaseMemoryManager : public BaseMemoryManagerIter
		{
		protected:
			// Store parameters
			parameters parms;

			// Statistics
			statistics mem_stats;

			// Store all types of page providers
			GenericPageProvider provider;

			// Support for linked list of all BaseMemoryManager instances
			static BaseMemoryManagerIter* end_mgr() noexcept
			{
				static BaseMemoryManagerIter end;
				return (&end);
			}
			// Support for linked list of all BaseMemoryManager instances
			static shared_spinlock& end_lock() noexcept
			{
				static shared_spinlock lock;
				return lock;
			}
			// Support for linked list of all BaseMemoryManager instances
			static void insert_manager(BaseMemoryManager* mgr) noexcept
			{
				std::lock_guard<shared_spinlock> ll(end_lock());
				mgr->right = end_mgr();
				mgr->left = end_mgr()->left;
				mgr->left->right = mgr->right->left = mgr;
			}
			// Support for linked list of all BaseMemoryManager instances
			static void remove_manager(BaseMemoryManager* mgr) noexcept
			{
				std::lock_guard<shared_spinlock> ll(end_lock());
				mgr->right->left = mgr->left;
				mgr->left->right = mgr->right;
				mgr->right = mgr->left = mgr;
			}

			/// @brief Given a potentially invalid BaseMemoryManager address,
			/// returns mgr if the address is valid, null otherwise.
			static BaseMemoryManager* internal_find(BaseMemoryManager* mgr) noexcept
			{
				BaseMemoryManagerIter* m = end_mgr()->right;
				BaseMemoryManager* found = nullptr;
				while (m != end_mgr()) {
					if (static_cast<BaseMemoryManager*>(m) == mgr) {
						found = mgr;
						break;
					}
					m = m->right;
				}
				return found;
			}

		public:
			/// @brief Given a potentially invalid BaseMemoryManager address,
			/// returns mgr if the address is valid, null otherwise.
			static BaseMemoryManager* find(BaseMemoryManager* mgr) noexcept
			{
				end_lock().lock_shared();
				BaseMemoryManager* found = internal_find(mgr);
				end_lock().unlock_shared();
				return found;
			}

			/// @brief Construct from parameters
			BaseMemoryManager(const parameters& p) noexcept
			  : parms(p.validate(p.log_level != MicroNoLog ? MicroWarning : MicroNoLog))
			  , provider(parms)
			{
#ifndef MICRO_NO_FILE_MAPPING
				// Select page provider based on parameters
				if (parms.provider_type == MicroFileProvider)
					provider.setFileProvider(parms.page_size, parms.grow_factor, parms.page_file_provider.data(), parms.page_memory_size, parms.page_file_flags);
				else
#endif
				  if (parms.provider_type == MicroMemProvider)
					provider.setMemoryProvider(parms.page_size, parms.allow_os_page_alloc, parms.page_memory_provider, static_cast<std::uintptr_t>(parms.page_memory_size));
				else if (parms.provider_type == MicroOSPreallocProvider)
					provider.setPreallocatedPageProvider(static_cast<size_t>(parms.page_memory_size), parms.allow_os_page_alloc);

				insert_manager(this);
			}
			virtual ~BaseMemoryManager() noexcept { remove_manager(this); }

			MICRO_DELETE_COPY(BaseMemoryManager)

			/// @brief Returns read-only parameters used by this manager
			MICRO_ALWAYS_INLINE const parameters& params() const noexcept { return parms; }

			/// @brief Returns the internal page provider
			MICRO_ALWAYS_INLINE BasePageProvider* page_provider() noexcept { return &provider; }
			MICRO_ALWAYS_INLINE const BasePageProvider* page_provider() const noexcept { return &provider; }

			/// @brief Returns provider page size
			MICRO_ALWAYS_INLINE size_t page_size() const noexcept { return provider.page_size(); }
			/// @brief Returns provider page size bits
			MICRO_ALWAYS_INLINE size_t page_size_bits() const noexcept { return provider.page_size_bits(); }
			/// @brief Returns provider allocation granularity (usually page size, except on Windows)
			MICRO_ALWAYS_INLINE size_t allocation_granularity() const noexcept { return provider.allocation_granularity(); }

			/// @brief Returns statistics
			MICRO_ALWAYS_INLINE const statistics& stats() const noexcept { return mem_stats; }

			/// @brief Clear the manager.
			/// Deallocate all pages and reset the manager state.
			virtual void clear() noexcept = 0;

			/// @brief Allocate page_count pages.
			/// The amount of allocated page might be greater than page_count depending on the allocation granularity.
			virtual PageRunHeader* allocate_pages(size_t page_count) noexcept = 0;
			/// @brief Allocate enough pages to hold given amount of bytes
			virtual PageRunHeader* allocate_pages_for_bytes(size_t bytes) noexcept = 0;
			/// @brief Allocate a page run suitable for the radix tree (size MICRO_BLOCK_SIZE)
			virtual PageRunHeader* allocate_medium_block() noexcept = 0;
			/// @brief Deallocate page run
			virtual void deallocate_pages(PageRunHeader* p) noexcept = 0;

			/// @brief Allocate size bytes using internal memory pool.
			/// This allocation will never be deallocated except in clear() and destructor.
			virtual void* allocate_and_forget(unsigned size) noexcept = 0;

			/// @brief Allocate given amount of bytes with provided alignment.
			/// Must use the radix tree ONLY. This function is used by TinyMemPool to allocate
			/// blocks dedicated to small objects.
			virtual void* allocate_no_tiny_pool(size_t bytes, unsigned obj_size, unsigned align, bool* is_small) noexcept = 0;
			virtual void deallocate_no_tiny_pool(void*) noexcept = 0;
		};

	}

}

MICRO_POP_DISABLE_OLD_STYLE_CAST

#endif
