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

#ifndef MICRO_ALLOCATOR_HPP
#define MICRO_ALLOCATOR_HPP

#ifdef _MSC_VER
// Remove useless warnings ...needs to have dll-interface to be used by clients of class...
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include <climits>

#include "../logger.hpp"
#include "../parameters.hpp"
#include "../os_timer.hpp"
#include "headers.hpp"
#include "page_map.hpp"
#include "page_provider.hpp"
#include "statistics.hpp"
#include "tiny_mem_pool.hpp"
#include "uint_large.hpp"

extern "C" {
#include "../enums.h"
}

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

// Undefine min/max due to Windows.h inclusion without NOMINMAX defined
#ifdef min
#undef min
#undef max
#endif

#ifdef small
#undef small
#endif

#define ALLOCATOR_INLINE MICRO_ALWAYS_INLINE

// Disable old cast warnings
MICRO_PUSH_DISABLE_OLD_STYLE_CAST

namespace micro
{
	namespace detail
	{

		/// @brief Match in a radix tree
		struct Match
		{
			std::uint16_t index0{ 0 }; // First level position
			std::uint16_t index1{ 0 }; // Second level position

			// Convert from/to unsigned

			MICRO_ALWAYS_INLINE std::uint32_t to_uint() const noexcept
			{
				std::uint32_t res;
				memcpy(&res, this, 4);
				return res;
			}
			MICRO_ALWAYS_INLINE void from_uint(std::uint32_t m) noexcept { memcpy(this, &m, 4); }
		};

		// Common functions for the radix tree.
		// Depends on the maximum radix tree size (based on MICRO_MEMORY_LEVEL)

#if MICRO_MAX_RADIX_SIZE == 17

		class MICRO_EXPORT_CLASS RadixAccess
		{
		public:
			static constexpr unsigned max_bits = 17;
			static constexpr unsigned l0_size = 256;
			static constexpr unsigned l1_size = 512;

			using l0_type = UInt256;
			using l1_type = UInt512;

			static ALLOCATOR_INLINE std::uint16_t radix_0(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems >> 9u); }
			static ALLOCATOR_INLINE std::uint16_t radix_1(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems & 511u); }
			static ALLOCATOR_INLINE std::uint32_t elems(Match m) noexcept { return (static_cast<unsigned>(m.index0) << 9u) | m.index1; }
		};

#elif MICRO_MAX_RADIX_SIZE == 16

		class MICRO_EXPORT_CLASS RadixAccess
		{
		public:
			static constexpr unsigned max_bits = 16;
			static constexpr unsigned l0_size = 256;
			static constexpr unsigned l1_size = 256;

			using l0_type = UInt256;
			using l1_type = UInt256;

			static ALLOCATOR_INLINE std::uint16_t radix_0(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems >> 8u); }
			static ALLOCATOR_INLINE std::uint16_t radix_1(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems & 255u); }
			static ALLOCATOR_INLINE std::uint32_t elems(Match m) noexcept { return (static_cast<unsigned>(m.index0) << 8u) | m.index1; }
		};

#elif MICRO_MAX_RADIX_SIZE == 15

		class MICRO_EXPORT_CLASS RadixAccess
		{
		public:
			static constexpr unsigned max_bits = 15;
			static constexpr unsigned l0_size = 128;
			static constexpr unsigned l1_size = 256;

			using l0_type = UInt128;
			using l1_type = UInt256;

			static ALLOCATOR_INLINE std::uint16_t radix_0(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems >> 8u); }
			static ALLOCATOR_INLINE std::uint16_t radix_1(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems & 255u); }
			static ALLOCATOR_INLINE std::uint32_t elems(Match m) noexcept { return (static_cast<unsigned>(m.index0) << 8u) | m.index1; }
		};

#elif MICRO_MAX_RADIX_SIZE == 14

		class MICRO_EXPORT_CLASS RadixAccess
		{
		public:
			static constexpr unsigned max_bits = 14;
			static constexpr unsigned l0_size = 128;
			static constexpr unsigned l1_size = 128;

			using l0_type = UInt128;
			using l1_type = UInt128;

			static ALLOCATOR_INLINE std::uint16_t radix_0(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems >> 7u); }
			static ALLOCATOR_INLINE std::uint16_t radix_1(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems & 127u); }
			static ALLOCATOR_INLINE std::uint32_t elems(Match m) noexcept { return (static_cast<unsigned>(m.index0) << 7u) | m.index1; }
		};

#elif MICRO_MAX_RADIX_SIZE == 12

		class MICRO_EXPORT_CLASS RadixAccess
		{
		public:
			static constexpr unsigned max_bits = 12;
			static constexpr unsigned l0_size = 64;
			static constexpr unsigned l1_size = 64;

			using l0_type = UInt64;
			using l1_type = UInt64;

			static ALLOCATOR_INLINE std::uint16_t radix_0(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems >> 6u); }
			static ALLOCATOR_INLINE std::uint16_t radix_1(unsigned elems) noexcept { return static_cast<std::uint16_t>(elems & 63u); }
			static ALLOCATOR_INLINE std::uint32_t elems(Match m) noexcept { return (static_cast<unsigned>(m.index0) << 6u) | m.index1; }
		};

#endif

		/// @brief Leaf of a radix tree, holds up to RadixAccess::l1_size free chunks
		struct MICRO_EXPORT_CLASS RadixLeaf
		{
			using lock_type = spinlock;
			using mask_type = typename RadixAccess::l1_type;

			RadixLeaf() noexcept { memset(data, 0, sizeof(data)); }

			mask_type mask;									// Mask of non null positions
			uint32_t parent_index{ 0 };						// Index in parent tree
			lock_type locks[RadixAccess::l1_size];			// One spinlock per position
			MediumChunkHeader* data[RadixAccess::l1_size];	// Array of free MediumChunkHeader. Each entry is a linked list of MediumChunkHeader.
		};


#ifdef MICRO_DEBUG
		/// @brief Check previous and next MediumChunkHeader for given header
		/// For debug build only
		static inline bool check_prev_next(MediumChunkHeader* f)
		{
			static const char* msg = "corrupted heap block information";
			MICRO_ASSERT_DEBUG(f->th.guard == MICRO_BLOCK_GUARD, msg);
			MICRO_ASSERT_DEBUG(f->th.status == MICRO_ALLOC_FREE || f->th.status == MICRO_ALLOC_MEDIUM, msg);
			MediumChunkHeader* end = MediumChunkHeader::from(f->parent()->end());
			MediumChunkHeader* p = f - f->offset_prev;
			MediumChunkHeader* n = f + f->elems + 1u;
			MICRO_ASSERT_DEBUG(p == f || p == (f - f->offset_prev), msg);
			MICRO_ASSERT_DEBUG(p == f || (p + p->elems + 1) == f, msg);
			MICRO_ASSERT_DEBUG(n >= end || f == (n - n->offset_prev), msg);
			MICRO_ASSERT_DEBUG(p->parent() == f->parent(), msg);
			MICRO_ASSERT_DEBUG(n >= end || n->parent() == f->parent(), msg);
			return true;
		}
#endif


		class Arena;

		/// @brief Radix tree
		///
		/// RadixTree is a 2 level radix tree that can hold at most
		/// 65536*2 entries (depending on MICRO_MEMORY_LEVEL).
		///
		/// The radix tree is used to store free chunks of memory that can
		/// be splitted and merged depending on requested allocation sizes.
		///
		/// The allocation granulariy of the radix tree is 16 bytes, and
		/// a radix tree can store entries of at most 2MB (based on MICRO_MEMORY_LEVEL).
		///
		/// The radix tree provides a fast and parallel best fit algorithm
		/// implementation with a O(1) complexity.
		///
		class MICRO_EXPORT_CLASS RadixTree
		{
			static constexpr unsigned l0_size = RadixAccess::l0_size;
			using lock_type = typename RadixLeaf::lock_type;
			using mask_type = typename RadixAccess::l0_type;

			mask_type mask;							// mask of empty slots
			std::atomic<RadixLeaf*> data[l0_size];	// array of level1 entries
			Arena* arena{ nullptr };				// parent arena
			volatile uint32_t last{ 0 };			// last allocated/deallocated chunk for last fit (if MICRO_ALLOC_FROM_LAST is 1)

			RadixLeaf* alloc(unsigned pos) noexcept;

			/// @brief Returns (and potentially allocate) the RadixLeaf object at given position
			ALLOCATOR_INLINE RadixLeaf* get(unsigned pos) noexcept
			{
				RadixLeaf* l = data[pos].load(std::memory_order_relaxed);
				if (MICRO_UNLIKELY(!l))
					l = alloc(pos);
				return l;
			}

			/// @brief Lower bound algorithm.
			/// Find an empty slot of size >= elems, where elems is given in 16 bytes granularity.
			/// If found, the output Match object will contain the position of found entry.
			/// Returns the match leaf on success, null otherwise.
			ALLOCATOR_INLINE RadixLeaf* lower_bound(unsigned elems, Match& m) noexcept
			{
				m.index0 = RadixAccess::radix_0(elems);
				m.index1 = RadixAccess::radix_1(elems);

				unsigned index0;
			L0:
				// Search in level 0
				index0 = this->mask.scan_forward(m.index0);
				if (index0 == l0_size)
					return nullptr;
				RadixLeaf* ch = this->get(index0);
				if (MICRO_UNLIKELY(!ch))
					return nullptr;

				// We had to go to a greater L0 size: not need to use the exact indexes anymore
				if (index0 != m.index0)
					m.index1 = 0;
				m.index0 = static_cast<std::uint16_t>(index0);

				// Search in leaf
				m.index1 = static_cast<std::uint16_t>(ch->mask.scan_forward(m.index1));
				if (m.index1 == RadixAccess::l1_size) {
					// Perfect fit so far: increment index1 and retry from there
					if (++m.index0 == l0_size)
						return nullptr;
					m.index1 = 0;
					goto L0;
				}
				return ch;
			}

			/// @brief Lower bound search in the radix tree.
			/// If a valid entry is found, the corresponding position is locked.
			ALLOCATOR_INLINE RadixLeaf* lower_bound_lock(unsigned elems, Match& m) noexcept
			{
				while (RadixLeaf* ch = lower_bound(elems, m)) {
					ch->locks[m.index1].lock();
					if (MICRO_LIKELY(ch->data[m.index1])) {
						MICRO_ASSERT_DEBUG(ch->data[m.index1]->elems >= elems, "");
						MICRO_ASSERT_DEBUG(ch->data[m.index1]->th.guard == MICRO_BLOCK_GUARD, "");
						MICRO_ASSERT_DEBUG(ch->data[m.index1]->th.status == MICRO_ALLOC_FREE, "");
						return ch;
					}
					ch->locks[m.index1].unlock();
				}
				return nullptr;
			}

			/// @brief Remove entry at given position.
			/// The entry lock must be held.
			ALLOCATOR_INLINE MediumChunkHeader* remove_free_link(const Match& m, MediumChunkHeader* n, RadixLeaf* ch) noexcept
			{
				// MediumChunkHeader* n = m.ch->data[m.index1];
				MediumChunkHeader* next = n->next();
#if MICRO_USE_NODE_LOCK == 0
				MICRO_ASSERT_DEBUG(check_prev_next(n), "");
#endif
				ch->data[m.index1] = next;
				if (next) {
					next->set_prev(nullptr);
					MICRO_ASSERT_DEBUG(next->th.status == MICRO_ALLOC_FREE, "");
				}
				return next;
			}

			/// @brief Fills Match object using free slot size (in 16 bytes granularity)
			ALLOCATOR_INLINE RadixLeaf* get_free(unsigned elems, Match& m) noexcept
			{
				m.index0 = RadixAccess::radix_0(elems);
				m.index1 = RadixAccess::radix_1(elems);

				// No need to check results of get() as we know the leaf is already allocated
				// TODO: in fact we need to check

				return this->get(m.index0);
			}

			/// @brief Fills Match object using free slot size (in 16 bytes granularity)
			ALLOCATOR_INLINE RadixLeaf* get_free_no_check(unsigned elems, Match& m) noexcept
			{
				m.index0 = RadixAccess::radix_0(elems);
				m.index1 = RadixAccess::radix_1(elems);

				// No need to check results of get() as we know the leaf is already allocated
				RadixLeaf* ch = this->data[m.index0].load(std::memory_order_relaxed);
				MICRO_ASSERT_DEBUG(ch, "");
				return ch;
			}

			/// @brief Insert new entry in the tree, and fill its position.
			/// Thread safe function.
			ALLOCATOR_INLINE void insert_free(MediumChunkHeader* h, Match& m) noexcept 
			{
				RadixLeaf* ch = get_free(h->elems, m);
				return insert_free(h, ch, m);
			}

			/// @brief Insert new entry in the tree, and fill its position.
			/// Thread safe function.
			ALLOCATOR_INLINE void insert_free(MediumChunkHeader* h, RadixLeaf* ch, Match& m) noexcept
			{
				h->set_prev(nullptr);
				std::lock_guard<lock_type> ll(ch->locks[m.index1]);

#if MICRO_USE_NODE_LOCK == 0
				MICRO_ASSERT_DEBUG(check_prev_next(h), "");
#endif
				MICRO_ASSERT_DEBUG(h->th.status == MICRO_ALLOC_FREE, "");

				MediumChunkHeader* f = ch->data[m.index1];
				ch->data[m.index1] = h;

				h->set_next(f);
				if (f) {
					MICRO_ASSERT_DEBUG(f->th.status == MICRO_ALLOC_FREE, "");
					f->set_prev(h);
				}
				else {
					// Set the mask bit if the leaf was empty
					ch->mask.set(m.index1);
					this->mask.set(m.index0);
				}
			}

			/// @brief Update tree masks base on leaf one
			void invalidate_masks(RadixLeaf* ch) noexcept;

			/// @brief Add a new free chunk of size MICRO_BLOCK_SIZE bytes to the tree
			bool add_new() noexcept;

			/// @brief Split chunk
			MediumChunkHeader* split_chunk(MediumChunkHeader*& h, PageRunHeader* parent, unsigned elems_1, Match& m, RadixLeaf*& ch) noexcept;

			/// @brief Remove entry from the tree.
			/// Thread safe function.
			ALLOCATOR_INLINE void remove_from_list(MediumChunkHeader* c) noexcept
			{
				Match m;
				RadixLeaf* ch = get_free_no_check(c->elems, m);

				std::lock_guard<lock_type> ll(ch->locks[m.index1]);

				// remove free chunk from tree

				MediumChunkHeader* p = c->prev();
				MediumChunkHeader* n = c->next();
				if (n)
					n->set_prev(p);
				if (p)
					p->set_next(n);
				else {
					MICRO_ASSERT_DEBUG(ch->data[m.index1] == c, "");
					ch->data[m.index1] = n;
					if (MICRO_UNLIKELY(!n)) {
						ch->mask.unset(m.index1);
						if (ch->mask.null())
							// invalidate mask
							invalidate_masks(ch);
					}
				}
			}

			/// @brief Merge blocks
			ALLOCATOR_INLINE MediumChunkHeader* merge_previous(MediumChunkHeader* p, MediumChunkHeader* f, MediumChunkHeader* n, MediumChunkHeader*) noexcept
			{
				if (n) {
					MICRO_ASSERT_DEBUG(n - n->offset_prev == f, "");
					n->offset_prev = static_cast<unsigned>(n - p);
				}
				if (MICRO_LIKELY(p->elems != 0)) // Note that chunks of size 0 are allowed and created by aligned allocation of MICRO_ALIGNED_POOL alignment
					remove_from_list(p);

				// merge
				p->elems += 1u + f->elems;
				return p;
			}

			/// @brief Merge blocks
			ALLOCATOR_INLINE void merge_next(MediumChunkHeader*, MediumChunkHeader* f, MediumChunkHeader* n, MediumChunkHeader* end) noexcept
			{
				unsigned elems_1 = n->elems + 1;
				auto* nn = n + elems_1;

				if (MICRO_LIKELY(n->elems != 0)) // Note that chunks of size 0 are allowed and created by aligned allocation of MICRO_ALIGNED_POOL alignment
					remove_from_list(n);

				// merge
				f->elems += elems_1;
				// update next chunk
				if (nn < end)
					nn->offset_prev = static_cast<unsigned>(nn - f);
			}

			/// @brief Returns an aligned MediumChunkHeader based on provided one and alignment value.
			MediumChunkHeader* align_header(MediumChunkHeader* h, unsigned align, PageRunHeader* parent) noexcept;

			/// @brief Allocate memory from given match
			void* allocate_elems_from_match(unsigned elems, Match& m, unsigned align, PageRunHeader* parent, MediumChunkHeader* h, RadixLeaf* ch) noexcept;

			RadixLeaf* find_aligned_small_block(Match& m) noexcept;

		public:
			static_assert(sizeof(MediumChunkHeader) == MICRO_HEADER_SIZE, "");

			RadixLeaf* first{ nullptr };

			/// @brief Construct the radix tree from its parent arena
			ALLOCATOR_INLINE RadixTree(Arena* a) noexcept
			  : arena(a)
			{
				memset(static_cast<void*>(data), 0, sizeof(data));
				first = get(0);
			}
			MICRO_DELETE_COPY(RadixTree)

			/// @brief Convert bytes to number of 16 bytes chunks
			static ALLOCATOR_INLINE unsigned bytes_to_elems(unsigned bytes) noexcept { return (bytes + 15u) >> MICRO_ELEM_SHIFT; }

			/// @brief Allocate elems*16 bytes with given alignment (up to page size).
			/// If force is true, allocate new pages to fullfill the request.
			/// Returns null on failure.
			void* allocate_elems(unsigned elems, unsigned align, bool force) noexcept;

			/// @brief Allocate elems*16 bytes with default alignment.
			/// Only works for small objects (the maximum size depends on the radix tree size).
			/// Do NOT allocate pages on failure.
			void* allocate_small_fast(unsigned elems) noexcept;

			/// @brief Deallocate memory previously allocated with allocate_elems().
			/// Returns the deallocated block size in bytes.
			unsigned deallocate(void* ptr) noexcept;

			ALLOCATOR_INLINE bool has_small_free_chunks() const noexcept { return mask.has_first_bit(); }
		};

		/// @brief Arena class.
		///
		/// A memory manager can contain at most MICRO_MAX_ARENAS arenas.
		/// The maximum number of arenas is given as a parameter of the
		/// parent memory manager constructor.
		///
		/// The arena contains a radix tree to fullfill medium allocations
		/// (up to 1MB for 64 bits plateforms), and a TinyMemPool
		/// potentially used for small allocations (depending on the
		/// memory manager parameters).
		///
		/// Each thread is attached to a specific arena.
		///
		class MICRO_EXPORT_CLASS Arena
		{
			BaseMemoryManager* pmanager;
			RadixTree radix_tree; // radix tree
			TinyMemPool pool;     // small object pool
		public:
			Arena(BaseMemoryManager* p) noexcept
			  : pmanager(p)
			  , radix_tree(this)
			  , pool(p)
			{
			}
			MICRO_DELETE_COPY(Arena)
			ALLOCATOR_INLINE BaseMemoryManager* manager() noexcept { return pmanager; }
			ALLOCATOR_INLINE RadixTree* tree() noexcept { return &radix_tree; }
			ALLOCATOR_INLINE TinyMemPool* tiny_pool() noexcept { return &pool; }

			// Number of current allocate_in_other_arenas() calls for this arena
			std::atomic<unsigned> other_arenas_count{ UINT_MAX };
		};

		/// @brief Memory block used by MemPool, uses bump allocation
		class MICRO_EXPORT_CLASS MemBlock
		{
		public:
			const unsigned block_size;  // full size of the block
			std::atomic<unsigned> tail; // tail position for new allcoations

			MemBlock(unsigned bsize) noexcept
			  : block_size(bsize)
			  , tail(sizeof(MemBlock))
			{
			}

			ALLOCATOR_INLINE void* allocate(unsigned size) noexcept
			{
				unsigned pos = tail.fetch_add(size);
				if (MICRO_UNLIKELY(pos + size > block_size)) {
					tail.fetch_sub(size);
					return nullptr;
				}
				return reinterpret_cast<char*>(this) + pos;
			}
		};

		/// @brief Thread safe memory pool for allocation only.
		/// Used to allocate radix tree leaves, allocate recursion detection hash table and page map.
		class MICRO_EXPORT_CLASS MemPool //: public BaseMemPool
		{
			shared_spinlock lock;	     // lock
			MemBlock* last{ nullptr };   // last MemBlock
			BaseMemoryManager* pmanager; // Parent manager

			MemBlock* allocate_block(unsigned size) noexcept
			{
				// The maximum requested size is in between 8192 (page map + radix tree leaves) and required size to store arenas
				size_t max_size = MICRO_MINIMUM_PAGE_SIZE * 2 - sizeof(PageRunHeader);
				if (sizeof(Arena) * manager()->params().max_arenas > max_size)
					max_size = sizeof(Arena) * manager()->params().max_arenas;
				if (size > max_size)
					max_size = size;
				max_size += sizeof(PageRunHeader);
				if (max_size % manager()->allocation_granularity())
					max_size = (max_size / manager()->allocation_granularity() + 1) * manager()->allocation_granularity();
				size_t pages = (max_size / manager()->page_size());

				PageRunHeader* p = manager()->allocate_pages(pages);
				if (MICRO_UNLIKELY(!p)) {
					return nullptr;
				}
				p->arena = manager();

				// Create MemBlock
				MemBlock* block = reinterpret_cast<MemBlock*>(p + 1);
				new (block) MemBlock(static_cast<unsigned>(p->run_size() - sizeof(PageRunHeader)));
				return block;
			}

		public:
			/// @brief Construct from a parent arena and element size in bytes
			MemPool(BaseMemoryManager* m) noexcept
			  : pmanager(m)
			{
			}

			BaseMemoryManager* manager() noexcept { return pmanager; }

			/// @brief Allocate size bytes
			void* allocate(unsigned size) noexcept
			{
				lock.lock_shared();
				void* r = last ? last->allocate(size) : nullptr;
				if (MICRO_LIKELY(r)) {
					lock.unlock_shared();
					return r;
				}
				lock.unlock_shared();

				std::lock_guard<shared_spinlock> ll(lock);
				r = last ? last->allocate(size) : nullptr;
				if (r)
					return r;

				if (MICRO_UNLIKELY(!(last = allocate_block(size))))
					return nullptr;
				r = last->allocate(size);
				return r;
			}
		};

		/// @brief Memory manager class
		///
		/// MemoryManager is the main class for memory allocation/deallocation.
		/// A MemoryManager object manages a collection of pages (retrieved from
		/// OS calls, from a memory chunk or from a file depending on the page
		/// provider) and distribute them among arenas.
		///
		/// Each arena handles allocations for specific threads in order to avoid
		/// contentions. An arena can serve allocations up to 1MB.
		///
		/// Big allocations (above 1MB) are directly served using the underlying page provider.
		///
		/// On destruction, a MemoryManager object will deallocate all remaining
		/// allocated memory.
		///
		/// You should use the public class micro::heap to manage allocations
		/// instead of MemoryManager.
		///
		class MICRO_EXPORT_CLASS MemoryManager : public BaseMemoryManager
		{
		public:
			using block_pool_type = typename TinyMemPool::block;

		private:
			friend struct TinyMemPool;
			friend class MemPool;
			friend class RadixTree;

			/// @brief MemPool proxy class, just to call the constructor on demand
			struct alignas(MemPool) MemPoolProxy
			{
				char data[sizeof(MemPool)];
				ALLOCATOR_INLINE MemPool* as_mem_pool() noexcept { return reinterpret_cast<MemPool*>(data); }
			};
			/// @brief Arena proxy class, just to call the constructor on demand
			struct alignas(Arena) ArenaProxy
			{
				char data[sizeof(Arena)];
				ALLOCATOR_INLINE Arena* arena() noexcept { return reinterpret_cast<Arena*>(data); }
			};

			using lock_type = recursive_spinlock;
			lock_type lock;		// Recursive lock used to protect pages manipulations
			PageRunHeader end;	// Linked list of ALL page runs
			PageRunHeader end_free; // Buffer of pages

			const unsigned os_psize;	     // used page size (from page provider)
			const unsigned os_psize_bits;	     // page size bits
			const unsigned os_alloc_granularity; // allocation granularity (from page provider)
			const unsigned os_max_medium_pages;  // maximum page count for the radix tree
			const unsigned os_max_medium_size;   // maximum size before big allocations (direct calls to the page provider)
			timer el_timer;			     // object start time (for statistics)

			std::atomic<size_t> free_page_count{ 0 };
			std::atomic<size_t> used_pages{ 0 }; // Total pages currently in use
			std::atomic<size_t> used_spans{ 0 }; // Total page runs (or PageRunHeader) currently in use
			std::atomic<size_t> max_pages{ 0 };  // Maximum used pages (or memory peak)
			std::atomic<size_t> side_pages{ 0 }; // Pages used by the internal memory pool (radix leaf, page map...)

			PageMap page_map; // page map, sorted array of ALL PageRunHeader currently in use

			// memory pools for radix tree
			MemPoolProxy radix_pool; // memory pool mainly used for radix tree allocation

			FILE* continuous{ nullptr };		   // continuous statistics output
			FILE* stats_output{ nullptr };		   // on exit statistics output
			std::atomic<bool> on_exit_done{ false };   // exit operations performed (or not)
			std::atomic<bool> init_done{ false };	   // initialization operations performed (or not)
			std::atomic<bool> header_printed{ false }; // CSV statistics header printed (or not)

			std::atomic<std::uint64_t> last_bytes{ 0 }; // last allocated bytes, used to trigger stats print
			std::atomic<std::uint64_t> last_time{ 0 };  // last allocation time, used to trigger stats print

			ArenaProxy* arenas{ nullptr }; // array of arenas

			/// @brief Initialize the arenas
			bool initialize_arenas() noexcept;
			/// @brief Compute the maximum number of pages for the radix tree
			unsigned compute_max_medium_pages() const noexcept;
			/// @brief Compute the allocation size limit before big allocations
			unsigned compute_max_medium_size() const noexcept;

			MICRO_ALWAYS_INLINE unsigned max_medium_pages() const noexcept { return os_max_medium_pages; }
			MICRO_ALWAYS_INLINE unsigned max_medium_size() const noexcept { return os_max_medium_size; }
			MICRO_ALWAYS_INLINE Arena* get_arenas() noexcept { return arenas[0].arena(); }

			MICRO_ALWAYS_INLINE unsigned get_mask() const noexcept
			{
				// return std::min(get_thread_mask(), this->params().max_arenas - 1u); }
				return get_thread_mask() & (this->params().max_arenas - 1u);
			}
			MICRO_ALWAYS_INLINE unsigned get_max_mask() const noexcept
			{
				// return std::min(get_thread_max_mask(), this->params().max_arenas - 1u);
				return get_thread_max_mask() & (this->params().max_arenas - 1u);
			}
			MICRO_ALWAYS_INLINE unsigned select_arena_id() const noexcept { return this_thread_id_for_arena() & get_mask(); }
			/// @brief Returns the arena used to allocate memory in current thread
			MICRO_ALWAYS_INLINE Arena* select_arena() noexcept
			{
				// All MemoryManager share the same arena idx for a specific thread
				return arenas[select_arena_id()].arena();
			}

			bool has_mem_pool(TinyMemPool* pool) noexcept;

			void print_os_infos(print_callback_type callback, void* opaque) const noexcept;
			void print_exit_infos(print_callback_type callback, void* opaque) const noexcept;
			MICRO_ALWAYS_INLINE void record_stats(void* p) noexcept { record_stats(p, type_of(p)); }
			void record_stats(void* p, int status) noexcept;
			void print_stats_if_necessary(bool force = false) noexcept;
			void init_internal() noexcept;
			void* allocate_big(size_t bytes, unsigned align) noexcept;
			void* allocate_big_path(size_t bytes, unsigned align, bool stats) noexcept;
			void* allocate_in_other_arenas(size_t bytes, unsigned elems, unsigned align, Arena* first, bool request_for_page = false) noexcept;

			static MICRO_ALWAYS_INLINE void deallocate_small(void* p, block_pool_type* pool, MemoryManager* m, bool stats) noexcept
			{
				// Small block, pool and mgr must be valid
				MICRO_ASSERT_DEBUG(pool->is_inside(p), "");

				size_t bytes = 0;
#ifdef MICRO_ENABLE_STATISTICS_PARAMETERS
				if (MICRO_UNLIKELY(stats && m->params().print_stats_trigger)) {
					MICRO_TIME_STATS(get_local_timer().tick());
					bytes = usable_size(p, MICRO_ALLOC_SMALL_BLOCK);
				}
#endif
				TinyMemPool::deallocate(p, pool);
#ifdef MICRO_ENABLE_STATISTICS_PARAMETERS
				if (MICRO_UNLIKELY(stats && m->params().print_stats_trigger)) {
					MICRO_TIME_STATS(m->mem_stats.update_dealloc_time(get_local_timer().tock()));
					m->mem_stats.deallocate_small(bytes);
				}
#endif
			}

			static bool verify_block(int status, void* p) noexcept;
			static MICRO_ALWAYS_INLINE void deallocate(void* p, bool stats) noexcept
			{
				// As much as possible, we want the fast path to be inlined
				if (MICRO_UNLIKELY(!p))
					return;

				block_pool_type* pool = nullptr;
				BaseMemoryManager* mgr = nullptr;
				int status = type_of(p, &pool, &mgr);
				MICRO_ASSERT_DEBUG(status != MICRO_ALLOC_SMALL_BLOCK || mgr, "");
				MICRO_ASSERT_DEBUG(verify_block(status, p), "");

				if (status == MICRO_ALLOC_SMALL_BLOCK) {
					// For small chunks, we want to go fast an inlined
					deallocate_small(p, pool, static_cast<MemoryManager*>(mgr), stats);
					return;
				}

				deallocate(p, status, pool, mgr, stats);
			}

			static int type_of_maybe_small(SmallChunkHeader* tiny, block_pool_type* pool, void* p) noexcept;
			// static int type_of(void* p, block_pool_type** block_pool = nullptr, BaseMemoryManager** memory_mgr = nullptr) noexcept;
			static MemoryManager* find_from_page_run(PageRunHeader*) noexcept;
			static MemoryManager* find_from_ptr(void* p) noexcept;

			static MICRO_ALWAYS_INLINE int type_of(void* p, block_pool_type** block_pool = nullptr, BaseMemoryManager** memory_mgr = nullptr) noexcept
			{
				// Returns the pointer type, considering it is already a valid block allocated by a MemoryManager object.
				// For MICRO_ALLOC_SMALL_BLOCK only, fill block_pool and memory_mgr if not null.

				SmallChunkHeader* tiny = SmallChunkHeader::from(p) - 1;
				block_pool_type* h = block_pool_type::from(reinterpret_cast<uintptr_t>(p) & ~(MICRO_ALIGNED_POOL - 1ull));

				if (h != p && h->header.guard == MICRO_BLOCK_GUARD && h->header.status == MICRO_ALLOC_SMALL_BLOCK) {

					int ret = MICRO_ALLOC_SMALL_BLOCK;
					const bool maybe_micro_block = tiny->guard == MICRO_BLOCK_GUARD && (tiny->status == MICRO_ALLOC_MEDIUM || tiny->status == MICRO_ALLOC_BIG);
					if (maybe_micro_block)
						ret = type_of_maybe_small(tiny, h, p);

					if (MICRO_LIKELY(ret == MICRO_ALLOC_SMALL_BLOCK && block_pool)) {

#if MICRO_USE_FIRST_ALIGNED_CHUNK
						// This the first chunk: the pool itself is not aligned on MICRO_ALIGNED_POOL
						if (h->header.offset_bytes == 0) {
							*block_pool = block_pool_type::from(h->as_char() + sizeof(PageRunHeader) + sizeof(MediumChunkHeader));
							*memory_mgr = (*block_pool)->get_parent()->d_mgr; //(*block_pool)->mgr;
						}
						else
#endif
						{
							*block_pool = h;
							*memory_mgr = h->get_parent()->d_mgr; // h->mgr;
						}

						// check foreign pointer
						if (!*memory_mgr || !*block_pool) {
							return maybe_micro_block ? tiny->status : 0;
						}
						// MICRO_ASSERT_DEBUG(find_from_page_run(((MediumChunkHeader*)*block_pool - 1)->parent()), "");
					}
					return ret;
				}

				return tiny->status == MICRO_ALLOC_SMALL_BLOCK ? 0 : tiny->status;
			}

		public:
			/// @brief Construct from parameters, but do not initialize the manager (to avoid triggering a potential allocation)
			MemoryManager(const parameters& p, bool) noexcept;
			/// @brief Construct from parameters and initialize
			MemoryManager(const parameters& p) noexcept;
			/// @brief destructor, deallocate all remainign memory
			~MemoryManager() noexcept override;

			/// @brief Initialize the memory manager
			MICRO_ALWAYS_INLINE void init() noexcept
			{
				if (MICRO_UNLIKELY(!init_done.load(std::memory_order_relaxed)))
					if (MICRO_UNLIKELY(!init_done.exchange(true)))
						init_internal();
			}

			MICRO_ALWAYS_INLINE PageMap& pmap() noexcept { return page_map; }

			/// @brief Clear the memory manager
			virtual void clear() noexcept override;

			virtual PageRunHeader* allocate_pages(size_t page_count) noexcept override;
			virtual PageRunHeader* allocate_medium_block() noexcept override;
			virtual PageRunHeader* allocate_pages_for_bytes(size_t bytes) noexcept override;
			virtual void deallocate_pages(PageRunHeader* p) noexcept override;

			virtual void* allocate_no_tiny_pool(size_t bytes, unsigned obj_size, unsigned align, bool* is_small) noexcept override;
			virtual void deallocate_no_tiny_pool(void*) noexcept override;
			virtual void* allocate_and_forget(unsigned size) noexcept override
			{
				// Allocate using the memory pool
				return radix_pool.as_mem_pool()->allocate(size);
			}

			unsigned maximum_medium_size() const noexcept { return max_medium_size(); }
			void* allocate(size_t bytes, unsigned align = 0) noexcept;
			MICRO_ALWAYS_INLINE void* aligned_allocate(size_t alignment, size_t bytes) noexcept { return allocate(bytes, static_cast<unsigned>(alignment)); }
			static void deallocate(void* p, int status, block_pool_type* pool, BaseMemoryManager* mgr, bool stats) noexcept;
			static MICRO_ALWAYS_INLINE void deallocate(void* p) noexcept { deallocate(p, true); }
			static MICRO_ALWAYS_INLINE size_t usable_size(void* p) noexcept
			{
				if (MICRO_UNLIKELY(!p))
					return 0;
				return usable_size(p, type_of(p));
			}
			static size_t usable_size(void* p, int status) noexcept;
			static MICRO_ALWAYS_INLINE int type_of_safe(void* p, block_pool_type** block_pool = nullptr, BaseMemoryManager** memory_mgr = nullptr) noexcept
			{
				// Returns the type of the memory block, or 0 if i wasn't allocated with the micro library
				auto* tiny = SmallChunkHeader::from(p) - 1;
				int status = type_of(p, block_pool, memory_mgr);
				if (status == MICRO_ALLOC_SMALL_BLOCK)
					return MICRO_ALLOC_SMALL_BLOCK;

				if (tiny->guard == MICRO_BLOCK_GUARD && (tiny->status == MICRO_ALLOC_BIG || tiny->status == MICRO_ALLOC_MEDIUM))
					return tiny->status;

				// In case of foreign pointer: test that this REALLY is a foreign pointer
				MICRO_ASSERT_DEBUG(!find_from_ptr(p), "");
				return 0;
			}
			static MICRO_ALWAYS_INLINE int type_of_safe_for_proxy(void* p, block_pool_type** block_pool = nullptr, BaseMemoryManager** memory_mgr = nullptr) noexcept
			{
				// MemoryManager* main = get_main_manager();
				// if (main && !main->page_map.is_inside(p))
				//	return 0;
				return type_of_safe(p, block_pool, memory_mgr);
			}

			void reset_statistics() noexcept;
			void set_start_time();

			void dump_statistics(micro_statistics& stats) noexcept;

			std::uint64_t peak_allocated_memory() const noexcept { return max_pages.load() * this->provider.page_size(); }

			void print_stats_header(print_callback_type callback, void* opaque) noexcept;
			void print_stats_header_stdout() noexcept;

			void print_stats_row(print_callback_type callback, void* opaque) noexcept;
			void print_stats_row_stdout() noexcept;

			void print_stats(print_callback_type callback, void* opaque) noexcept;
			void print_stats_stdout() noexcept;

			void perform_exit_operations() noexcept;

			static MemoryManager*& get_main_manager() noexcept;
		};
	}
}

#ifdef MICRO_HEADER_ONLY
#include "allocator.cpp"
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

MICRO_POP_DISABLE_OLD_STYLE_CAST

#endif
