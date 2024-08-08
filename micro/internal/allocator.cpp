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

#include <atomic>
#include <cinttypes>
#include <cstdlib>
#include <functional>
#include <map>

#include "../logger.hpp"
#include "../os_page.hpp"
#include "../os_timer.hpp"
#include "../parameters.hpp"
#include "allocator.hpp"
#include "recursive.hpp"

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

#if defined(_MSC_VER) || defined(__MINGW32__)
#include "Windows.h"
#endif

#ifdef min
#undef min
#undef max
#undef small
#endif

#ifdef MICRO_ZERO_MEMORY
#define MICRO_RESET_MEM(p, size) reset_mem_no_tiny(p, size)
#else
#define MICRO_RESET_MEM(p, size)
#endif

namespace micro
{

	namespace detail
	{
#ifdef MICRO_ZERO_MEMORY
		static MICRO_ALWAYS_INLINE void reset_mem_no_tiny(void* p, size_t len) noexcept
		{
			// Initialize memory on allocation from the raix tree
			memset(p, 0, len);
		}
#endif

		MICRO_EXPORT_CLASS_MEMBER RadixLeaf* RadixTree::alloc(unsigned pos) noexcept
		{
			// Allocate a RadixLeaf using the internal memory pool
			RadixLeaf* l = nullptr;
			RadixLeaf* tmp = static_cast<RadixLeaf*>(arena->manager()->allocate_and_forget(sizeof(RadixLeaf)));
			if (MICRO_UNLIKELY(!tmp))
				return nullptr;
			new (tmp) RadixLeaf;
			tmp->parent_index = pos;
			if (MICRO_LIKELY(data[pos].compare_exchange_strong(l, tmp)))
				return tmp;
			return l;
		}

		MICRO_EXPORT_CLASS_MEMBER void RadixTree::invalidate_masks(RadixLeaf* ch) noexcept
		{
			// Invalidate bit mask for given RadixLeaf
			do {
				this->mask.unset(ch->parent_index);
				if (MICRO_UNLIKELY(!ch->mask.null())) {
					// leaf mask just changed: cancel invalidation!
					this->mask.set(ch->parent_index);
					continue;
				}
				else
					break;
			} while (ch->mask.null());
		}

		MICRO_EXPORT_CLASS_MEMBER bool RadixTree::add_new() noexcept
		{
			// Add a new MediumChunkHeader to the radix tree from
			// a newly allocated PageRunHeader

			PageRunHeader* block = this->arena->manager()->allocate_medium_block();
			if (MICRO_UNLIKELY(!block))
				return false;

			block->arena = this->arena;

			MediumChunkHeader* h = MediumChunkHeader::from(block + 1);
			new (h) MediumChunkHeader();
			h->set_elems((block->size_bytes - sizeof(PageRunHeader) - sizeof(MediumChunkHeader)) >> MICRO_ELEM_SHIFT);
			h->th.offset_bytes = sizeof(PageRunHeader) >> MICRO_ELEM_SHIFT;
			h->th.status = MICRO_ALLOC_FREE;
			h->offset_prev = 0;

			// Set other_arenas_count to 0 (non empty) if it was to UINT_MAX (empty and unused)
			Arena* a = static_cast<Arena*>(arena);
			unsigned uintmax = UINT_MAX;
			a->other_arenas_count.compare_exchange_strong(uintmax, 0);

			Match m;
			insert_free(h, m);

			return true;
		}

		MICRO_EXPORT_CLASS_MEMBER MediumChunkHeader* RadixTree::align_header(MediumChunkHeader* h, unsigned align, PageRunHeader* parent) noexcept
		{
			uintptr_t addr = (h + 1)->address();
			if ((addr & (align - 1)) != 0) {
				// unaligned address
				addr = addr & ~(static_cast<uintptr_t>(align - 1));
				addr += align;

				// For aligned allocation only, allow the creation of 0 size chunks
				// if (addr == (uintptr_t)(h + 2)) {
				// 16 bytes after h data start: that would leave a header without data
				// addr += align;
				//}

				MediumChunkHeader* new_h = MediumChunkHeader::from(addr) - 1;

				// create left chunk
				unsigned h_elems = h->elems;
				unsigned new_free_elems = static_cast<unsigned>((new_h - h) - 1);

				// Ensure the radix leaf is available
				RadixLeaf* ch = nullptr;
				Match m;
				if (MICRO_LIKELY(new_free_elems)) {
					ch = get_free(new_free_elems, m);
					if (MICRO_UNLIKELY(!ch))
						return nullptr;
				}

				MediumChunkHeader* new_free = h;
				new_free->elems = new_free_elems;
				new_free->th.status = MICRO_ALLOC_FREE;

				// update h
				h = new (new_h) MediumChunkHeader(
				  new_free->elems + 1, h_elems - (new_free->elems + 1), MICRO_ALLOC_FREE, static_cast<unsigned>((new_h->as_char() - parent->as_char()) >> MICRO_ELEM_SHIFT));

#if MICRO_USE_NODE_LOCK
				// lock h before updating next offset
				h->get_lock()->lock_shared();
#endif

				// update next chunk prev offset
				MediumChunkHeader* next = h + h->elems + 1;
				if (next->as_char() < parent->end()) {
					next->offset_prev = static_cast<unsigned>(next - h);
#if MICRO_USE_NODE_LOCK == 0
					MICRO_ASSERT_DEBUG(check_prev_next(next), "");
#endif
				}

#if MICRO_USE_NODE_LOCK == 0
				MICRO_ASSERT_DEBUG(check_prev_next(new_free), "");
				MICRO_ASSERT_DEBUG(check_prev_next(h), "");
#endif
				MICRO_ASSERT_DEBUG(new_free_elems == new_free->elems, "");

				// Insert
				if (MICRO_LIKELY(new_free->elems))
					insert_free(new_free, ch, m);
			}
			MICRO_ASSERT_DEBUG((h + 1)->address() % align == 0, "");
			return h;
		}

		/// @brief Split chunk
		MICRO_EXPORT_CLASS_MEMBER MediumChunkHeader* RadixTree::split_chunk(MediumChunkHeader*& h, PageRunHeader* parent, unsigned elems_1, Match& m, RadixLeaf*& ch) noexcept
		{
			// Split a free chunk and returns the new free chunk.
			// Can split from the left (default) or from the right (unused currently)
			MediumChunkHeader* new_free;
			// if (keep_left) {
			// Keep the left and return the new right free chunk
			unsigned free_elems = h->elems - elems_1;

			// ensure the radix leaf is available
			ch = get_free(free_elems, m);
			if (MICRO_UNLIKELY(!ch))
				return nullptr;

			new_free = h + elems_1;
			new (new_free) MediumChunkHeader(elems_1, h->elems - elems_1, MICRO_ALLOC_FREE, static_cast<unsigned>((new_free->as_char() - parent->as_char()) >> MICRO_ELEM_SHIFT));

			// update h size
			h->set_elems(elems_1 - 1u);

			// update next chunk prev offset
			MediumChunkHeader* next = new_free + new_free->elems + 1u;
			if (next->as_char() < parent->end())
				next->offset_prev = static_cast<unsigned>(next - new_free);
			/*}
			else {
				// TODO: make it work with node lock (use MediumChunkHeader to initialize lock)
				// Also, ensure the radix leaf for the new free size is available or can be allocated
				// This might be used to randomly allocate from the left or the right of a page run.

				// New free chunk is at the start of previous chunk
				new_free = h;
				h = new_free + new_free->elems + 1 - elems_1;
				h->th.status = MICRO_ALLOC_FREE;
				h->set_elems(elems_1 - 1u);
				h->offset_prev = static_cast<unsigned>(h - new_free); // new_free->elems - elems_1;
				h->th.offset_bytes = static_cast<unsigned>((h->as_char() - parent->as_char()) >> MICRO_ELEM_SHIFT);
				h->th.guard = MICRO_BLOCK_GUARD;

				new_free->elems -= elems_1;

				// update next chunk prev offset
				MediumChunkHeader* next = h + elems_1;
				if (next->as_char() < parent->end())
					next->offset_prev = static_cast<unsigned>(next - h);
			}*/
			return new_free;
		}

		MICRO_EXPORT_CLASS_MEMBER void* RadixTree::allocate_elems_from_match(unsigned elems, Match& m, unsigned align, PageRunHeader* parent, MediumChunkHeader* h, RadixLeaf* ch) noexcept
		{
			// We found a free chunk big enough, allocate from it

#if MICRO_USE_NODE_LOCK == 0
			MICRO_ASSERT_DEBUG(check_prev_next(ch->data[m.index1]), "");
#endif

			// Remove chunk
			MediumChunkHeader* next = remove_free_link(m, h, ch);

			MICRO_ASSERT(h->th.status == MICRO_ALLOC_FREE, "");
			MICRO_ASSERT(h->th.guard == MICRO_BLOCK_GUARD, "");

			// Invalidate mask if necessary
			if (!next)
				ch->mask.unset(m.index1);

			// No need to hold the leaf lock anymore
			ch->locks[m.index1].unlock();

			// Keep around the found free chunk
			MediumChunkHeader* h_saved = h;

			if (align > 16) {
				// Align chunk
#if MICRO_USE_FIRST_ALIGNED_CHUNK
				if (!(align == MICRO_ALIGNED_POOL && h->offset_prev == 0 && h->elems == (MICRO_ALIGNED_POOL - sizeof(PageRunHeader) - 32) / 16))
#endif
					if (MICRO_UNLIKELY(!(h = align_header(h, align, parent)))) {
						// Fail to create radix leaf: reinsert h and return
						insert_free(h_saved, m);
						return nullptr;
					}
			}

			if (MICRO_LIKELY(h->elems > elems + 1)) {
				// We can carve a free block out of this one
				RadixLeaf* ch;
				MediumChunkHeader* new_free = split_chunk(h, parent, elems + 1, m, ch);
				if (MICRO_UNLIKELY(!new_free)) {
					// Fail to create radix leaf: reinsert h and return
					insert_free(h, m);
					return nullptr;
				}

				// Insert the new free chunk
				MICRO_ASSERT_DEBUG(new_free->parent() == h->parent(), "");
				MICRO_ASSERT_DEBUG(check_prev_next(new_free), "");
				insert_free(new_free, ch, m);
			}
			else // Reset m for potential last fit
				m = Match{};

			if (!next && ch->mask.null())
				// invalidate mask
				invalidate_masks(ch);

			MICRO_ASSERT_DEBUG(parent->left != nullptr, "");
			MICRO_ASSERT_DEBUG(parent->right != nullptr, "");
			// Set new status
			h->th.status = MICRO_ALLOC_MEDIUM;
			MICRO_RESET_MEM(h + 1, h->elems << MICRO_ELEM_SHIFT);

#if MICRO_USE_NODE_LOCK
			// Unlock chunk in case of aligned allocation
			// (previously locked by align_header())
			if (h_saved != h)
				h->get_lock()->unlock_shared();
#endif
			return h + 1;
		}

		MICRO_EXPORT_CLASS_MEMBER RadixLeaf* RadixTree::find_aligned_small_block(Match& m) noexcept
		{
			// Find aligned free chunk.
			// Used to take the last aligned chunk of a PageRunHeader that might be forgotten
			// By previous strategy (over allocation).
			// If MICRO_USE_FIRST_ALIGNED_CHUNK is 1, also try to use the very first chunk of a PageRunHeader
			// (which is trickier as we have to take into account the PageRunHeader header).

			unsigned reduced = static_cast<unsigned>((MICRO_ALIGNED_POOL - sizeof(PageRunHeader) - (MICRO_USE_FIRST_ALIGNED_CHUNK ? 32 : 0)) / 16);
			if (RadixLeaf* ch = lower_bound_lock(reduced, m)) {

				// tail block
				uintptr_t data_p = (ch->data[m.index1] + 1)->address();
				uintptr_t aligned = data_p & ~(static_cast<uintptr_t>(MICRO_ALIGNED_POOL - 1));
				if (data_p != aligned)
					aligned += MICRO_ALIGNED_POOL;
				if (aligned + reduced * 16 > data_p + ch->data[m.index1]->elems * 16) {
					ch->locks[m.index1].unlock();
					return nullptr;
				}
				return ch;
			}
			return nullptr;
		}

#if MICRO_USE_NODE_LOCK
		static MICRO_ALWAYS_INLINE bool lockForAlloc(PageRunHeader*, MediumChunkHeader* h, MediumChunkHeader* n, bool valid_end) noexcept
		{
			// Attempt to lock found chunk and next ones
			if (!h->get_lock()->try_lock_shared())
				return false;
			if (valid_end && !n->get_lock()->try_lock_shared()) {
				h->get_lock()->unlock_shared();
				return false;
			}
			return true;
		}
		static MICRO_ALWAYS_INLINE void unlockForAlloc(PageRunHeader*, MediumChunkHeader* h, MediumChunkHeader* n, bool valid_end) noexcept
		{
			// Unlock found free chunk and next one
			h->get_lock()->unlock_shared();
			if (valid_end)
				n->get_lock()->unlock_shared();
		}
#endif

		MICRO_EXPORT_CLASS_MEMBER void* RadixTree::allocate_small_fast(unsigned elems) noexcept
		{
			// Allocate a small chunk at first try.
			// Returns null if a lock acquire attempt fails.

			PageRunHeader* parent = nullptr;

			// Initialize match
			Match m{ 0, static_cast<uint16_t>(first->mask.scan_forward_small(RadixAccess::radix_1(elems))) };

			// Check if found
			if (m.index1 == RadixAccess::l1_size)
				return nullptr;
			// Try to lock the leaf spinlock
			if (MICRO_UNLIKELY(!first->locks[m.index1].try_lock()))
				return nullptr;
			// Ensure the found chunk is still valid
			if (MICRO_UNLIKELY(!first->data[m.index1])) {
				first->locks[m.index1].unlock();
				return nullptr;
			}
			parent = first->data[m.index1]->parent();

#if MICRO_USE_NODE_LOCK
			MediumChunkHeader* h = first->data[m.index1];
			MediumChunkHeader* n = h + h->elems + 1;
			const bool valid_end = n->as_char() < parent->end();
			// Try to lock the chunks
			if (MICRO_UNLIKELY(!lockForAlloc(parent, h, n, valid_end))) {
				first->locks[m.index1].unlock();
				return nullptr;
			}
#else
			// Try to lock the page run header
			if (MICRO_UNLIKELY(!parent->lock.try_lock_shared())) {
				first->locks[m.index1].unlock();
				return nullptr;
			}
#endif

			MICRO_ASSERT_DEBUG(check_prev_next(first->data[m.index1]), "");
			MICRO_ASSERT_DEBUG(parent->left != nullptr, "");
			MICRO_ASSERT_DEBUG(parent->right != nullptr, "");
			// Allocate from found chunk
			void* r = this->allocate_elems_from_match(elems, m, 0, parent, first->data[m.index1], first);

			// Unlock
#if MICRO_USE_NODE_LOCK
			unlockForAlloc(parent, h, n, valid_end);
#else
			parent->lock.unlock_shared();
#endif
			return r;
		}

		MICRO_EXPORT_CLASS_MEMBER void* RadixTree::allocate_elems(unsigned elems, unsigned align, bool force) noexcept
		{
			// Allocate elems*16 bytes with given alignment
			// If force is true, allocate fresh pages if necessary

			PageRunHeader* parent = nullptr;
			unsigned search_for = elems;
			if (align > 16) {
				// Overalignment: increase chunk size
				MICRO_ASSERT_DEBUG((align & (align - 1)) == 0, "");
				search_for += align / 16u + 1u;
			}

			Match m;
			RadixLeaf* ch;

			// If MICRO_ALLOC_FROM_LAST is 1, reuse the last free chunk.
			// Faster in some scenarios, but consume more memory (last-fit
			// in addition to best-fit).

			if MICRO_CONSTEXPR (MICRO_ALLOC_FROM_LAST) {
				if (uint32_t l = last) {
					m.from_uint(l);
					if (RadixAccess::elems(m) >= search_for) {
						ch = data[m.index0].load(std::memory_order_relaxed);
						ch->locks[m.index1].lock();
						if (ch->data[m.index1])
							// We found (and locked) a free chunk: directly go to the allocation step
							goto found;
						ch->locks[m.index1].unlock();
					}
				}
			}

			for (;;) {
				// Find a valid chunk and lock it
				ch = lower_bound_lock(search_for, m);

				if (!ch) {
					if (align == MICRO_ALIGNED_POOL)
						// Try from page run tails
						ch = find_aligned_small_block(m);
					if (!ch) {
						// Allocate fresh pages if possible
						if (!force || !add_new())
							return nullptr;
						continue;
					}
				}

			found:
				parent = ch->data[m.index1]->parent();

				// Now, lock the page run (or the chunks themselves)
				// and carve a suitable chunk from found one.
				// If the lock cannot be acquired, go back to
				// the beginning of best-fit strategy.

#if MICRO_USE_NODE_LOCK
				MediumChunkHeader* h = ch->data[m.index1];
				MediumChunkHeader* n = h + h->elems + 1;
				const bool valid_end = n->as_char() < parent->end();
				if (MICRO_LIKELY(lockForAlloc(parent, h, n, valid_end))) {
					MICRO_ASSERT_DEBUG(parent->left != nullptr, "");
					MICRO_ASSERT_DEBUG(parent->right != nullptr, "");
					void* r = this->allocate_elems_from_match(elems, m, align, parent, h, ch);
					unlockForAlloc(parent, h, n, valid_end);
					if MICRO_CONSTEXPR (MICRO_ALLOC_FROM_LAST)
						last = m.to_uint();
					MICRO_ASSERT_DEBUG(!r || align == 0 || (reinterpret_cast<uintptr_t>(r) % align) == 0, "");
					return r;
				}
#else
				if (MICRO_LIKELY(parent->lock.try_lock_shared())) {
					MICRO_ASSERT_DEBUG(check_prev_next(ch->data[m.index1]), "");
					MICRO_ASSERT_DEBUG(parent->left != nullptr, "");
					MICRO_ASSERT_DEBUG(parent->right != nullptr, "");
					void* r = this->allocate_elems_from_match(elems, m, align, parent, ch->data[m.index1], ch);
					parent->lock.unlock_shared();
					if MICRO_CONSTEXPR (MICRO_ALLOC_FROM_LAST)
						last = m.to_uint();
					MICRO_ASSERT_DEBUG(!r || align == 0 || (reinterpret_cast<uintptr_t>(r) % align) == 0, "");
					return r;
				}
#endif

				ch->locks[m.index1].unlock();
			}
			MICRO_UNREACHABLE();
		}

		// Shrink block.
		// We should use it at some point.

		/*MICRO_EXPORT_CLASS_MEMBER void RadixTree::shrink(MediumChunkHeader* h, unsigned new_elems) noexcept
		{
			MICRO_ASSERT_DEBUG(h->th.guard == MICRO_BLOCK_GUARD, "");
			MICRO_ASSERT_DEBUG(h->th.status == MICRO_ALLOC_MEDIUM, "");

			if (h->elems < new_elems || new_elems == h->elems)
				return;
			if (new_elems == 0)
				return;

			PageRunHeader* parent = h->parent();
			MediumChunkHeader* end = (MediumChunkHeader*)parent->end();

			using lock_type = PageRunHeader::shared_lock_type;
			std::lock_guard<lock_type> ll(parent->lock);

			MediumChunkHeader* _new = h + 1 + new_elems;
			unsigned remaining = h->elems - new_elems;
			unsigned offset_run = (unsigned)(_new - (MediumChunkHeader*)parent);

			MediumChunkHeader* next = h + h->elems + 1;

			if (next >= end || next->th.status == MICRO_ALLOC_MEDIUM) {
				// This is the last block, or next block is used: create a new free block after, if possible

				if (remaining < 2)
					return; // No room to create a new free block

				new (_new) MediumChunkHeader(1 + new_elems, remaining - 1, MICRO_ALLOC_FREE, offset_run);
				h->elems = new_elems;

				if (next < end) {
					// Update offset of next block
					next->offset_prev = remaining;
					MICRO_ASSERT_DEBUG(check_prev_next(next), "");
				}
				MICRO_ASSERT_DEBUG(check_prev_next(h), "");
				MICRO_ASSERT_DEBUG(check_prev_next(_new), "");
			}
			else
			{

				MICRO_ASSERT_DEBUG(next->th.guard == MICRO_BLOCK_GUARD, "");
				MICRO_ASSERT_DEBUG(next->th.status == MICRO_ALLOC_FREE, "");
				// There is a free block after: resize it

				// First, remove next from free list
				remove_from_list(next);

				MediumChunkHeader *next_next = next + next->elems + 1;
				new (_new) MediumChunkHeader(1 + new_elems, next->elems + remaining, MICRO_ALLOC_FREE, offset_run);
				h->elems = new_elems;
				if (next_next < end) {
					// Update offset of next block
					next_next->offset_prev = (unsigned)(next_next - _new);
					MICRO_ASSERT_DEBUG(check_prev_next(next_next), "");
				}
				MICRO_ASSERT_DEBUG(check_prev_next(h), "");
				MICRO_ASSERT_DEBUG(check_prev_next(_new), "");

				// Reset next header to reduce false positives in MemoryManager::type_of();
				memset((void*)next, 0, sizeof(MediumChunkHeader));
			}

			// Insert new free chunk
			Match m;
			insert_free(_new, m);
		}*/

#define MICRO_CONTINUE_YIELD                                                                                                                                                                           \
	{                                                                                                                                                                                              \
		std::this_thread::yield();                                                                                                                                                             \
		continue;                                                                                                                                                                              \
	}

		MICRO_EXPORT_CLASS_MEMBER unsigned RadixTree::deallocate(void* ptr) noexcept
		{
			// Deallocate a chunk, and try to merge
			// it with its neighbors (immediate coalescing)

			// Get the chunk header
			MediumChunkHeader* f = MediumChunkHeader::from(ptr) - 1;

			// Ensure it is valid
			MICRO_ASSERT_DEBUG(f->th.guard == MICRO_BLOCK_GUARD, "");
			MICRO_ASSERT_DEBUG(f->th.status == MICRO_ALLOC_MEDIUM, "");

			// Get parent page run, next chunk, and end of page run
			PageRunHeader* parent = f->parent();
			MediumChunkHeader* n = f + f->elems + 1;
			MediumChunkHeader* end = MediumChunkHeader::from(parent->end());
			MediumChunkHeader* p;
			unsigned bytes = static_cast<unsigned>(f->elems) << MICRO_ELEM_SHIFT;

			// Invalidate next chunk if past-the-end
			bool lock_next = true;
			if (n == end)
				n = nullptr;

#if MICRO_USE_NODE_LOCK
			// Finer grained locking
			for (;;) {
				// Lock chunk
				if (!f->get_lock()->try_lock_fast())
					MICRO_CONTINUE_YIELD

				// Compute previous chunk and lock it
				p = nullptr;
				if (f->offset_prev) {
					p = f - f->offset_prev;
					if (MICRO_UNLIKELY(!p->get_lock()->try_lock_fast())) {
						f->get_lock()->unlock();
						MICRO_CONTINUE_YIELD
					}
				}

				// Only lock next node if free.
				// This works as freeing the next node
				// must lock this node first (through
				// previous node locking).
				if ((lock_next = n && n->th.status == MICRO_ALLOC_FREE)) {
					if (MICRO_UNLIKELY(!n->get_lock()->try_lock_fast())) {
						f->get_lock()->unlock();
						if (p)
							p->get_lock()->unlock();
						MICRO_CONTINUE_YIELD
					}
					// If not free after locking, unlock
					if (n->th.status != MICRO_ALLOC_FREE) {
						n->get_lock()->unlock();
						lock_next = false;
					}
					// We don't need to lock the last node (after n) as we only modify its offset to previous
				}

				break;
			}

#else
			// Just lock the parent page run
			parent->lock.lock();
			p = f->offset_prev ? f - f->offset_prev : nullptr;
#endif

			if (p && p->th.status == MICRO_ALLOC_FREE) {
				// Merge with previous
				MICRO_ASSERT_DEBUG(p != f, "");
				f = merge_previous(p, f, n, end);
#if MICRO_USE_NODE_LOCK
				// Remove p from the list of chunks to unlock
				p = nullptr;
#endif
			}
			if (lock_next && n->th.status == MICRO_ALLOC_FREE) {
				// Merge with next
				merge_next(p, f, n, end);
#if MICRO_USE_NODE_LOCK
				// Remove n from the list of chunks to unlock
				// n = nullptr;
				lock_next = false;
#endif
			}

			// Deallocate memory block if needed
			if (MICRO_UNLIKELY(((f->block_bytes() + sizeof(PageRunHeader) == parent->size_bytes)))) {
				MICRO_ASSERT_DEBUG(f->as_char() == parent->as_char() + sizeof(PageRunHeader), "");

				// No need to release locks with MICRO_USE_NODE_LOCK,
				// the content of the page run will be reseted anyway

#if MICRO_USE_NODE_LOCK == 0
				parent->lock.unlock();
#endif
				this->arena->manager()->deallocate_pages(parent);
				return bytes;
			}
			else {
				f->th.status = MICRO_ALLOC_FREE;

#if MICRO_USE_NODE_LOCK
				// With MICRO_USE_NODE_LOCK, we can at least release locks of left/right chunks
				// before inserting the new free chunk
				if (p)
					p->get_lock()->unlock();
				if (lock_next)
					n->get_lock()->unlock();
#endif
				// Add new free chunk to the radix tree
				Match m;
				insert_free(f, m);

#if MICRO_ALLOC_FROM_LAST
				// For last-fit strategy
				last = m.to_uint();
#endif

#if MICRO_USE_NODE_LOCK == 0
				MICRO_ASSERT_DEBUG(check_prev_next(f), "");
#endif
				MICRO_ASSERT_DEBUG(parent->left != nullptr, "");
				MICRO_ASSERT_DEBUG(parent->right != nullptr, "");
			}

			// Unlock all
#if MICRO_USE_NODE_LOCK
			f->get_lock()->unlock();
#else
			parent->lock.unlock();
#endif
			return bytes;
		}

#undef MICRO_CONTINUE_YIELD

		/// @brief Incrememnt counter on construction, decrement on destruction
		struct Counter
		{
			std::atomic<unsigned>& cnt;
			Counter(std::atomic<unsigned>& c) noexcept
			  : cnt(c)
			{
				c.fetch_add(1);
			}
			~Counter() noexcept { cnt.fetch_sub(1); }
		};

		MICRO_EXPORT_CLASS_MEMBER bool MemoryManager::initialize_arenas() noexcept
		{
			std::lock_guard<lock_type> ll(lock);
			if (!arenas) {
				// Initialize global memory pool that will be used to perform following allocations
				new (&radix_pool) MemPool(this);

				// Allocate arenas
				size_t arenas_bytes = (sizeof(ArenaProxy) * params().max_arenas + sizeof(PageRunHeader));
				void* a = allocate_and_forget(static_cast<unsigned>(arenas_bytes));
				if (!a)
					return false;
				// Initialize arenas
				ArenaProxy* _arenas = static_cast<ArenaProxy*>(a);
				for (unsigned i = 0; i < params().max_arenas; ++i)
					new (_arenas[i].arena()) Arena(this);

				// Initialize arenas at the end to avoid other threads to go further
				arenas = _arenas;
			}
			return true;
		}

		MICRO_EXPORT_CLASS_MEMBER unsigned MemoryManager::compute_max_medium_pages() const noexcept
		{
			// Maximum number of pages for the radix tree
			return static_cast<unsigned>(MICRO_BLOCK_SIZE / page_provider()->page_size());
		}
		MICRO_EXPORT_CLASS_MEMBER unsigned MemoryManager::compute_max_medium_size() const noexcept
		{
			unsigned max_pages_ = compute_max_medium_pages();
			return (max_pages_ << page_size_bits()) - (sizeof(PageRunHeader) + sizeof(MediumChunkHeader));
		}

		MICRO_EXPORT_CLASS_MEMBER bool MemoryManager::has_mem_pool(TinyMemPool* pool) noexcept
		{
			// Ensure given TinyMemPool is a valid one and belongs to this MemoryManager
			for (unsigned i = 0; i < params().max_arenas; ++i) {
				if (get_arenas()[i].tiny_pool() == pool)
					return true;
			}
			return false;
		}

		MICRO_EXPORT_CLASS_MEMBER void* MemoryManager::allocate_big(size_t bytes, unsigned align) noexcept
		{
			// Allocate big object

			size_t requested = bytes + sizeof(PageRunHeader) + sizeof(BigChunkHeader) + (align > 16 ? align : 0);
			auto* block = allocate_pages_for_bytes(requested);
			if (MICRO_UNLIKELY(!block))
				return nullptr;

			// Insert new pages into the page map
			if (!page_map.insert(block, true)) {
				deallocate_pages(block);
				return nullptr;
			}

			void* res = (block + 1)->as_char() + sizeof(BigChunkHeader);

			if (align > 16) {
				// Check power of 2
				MICRO_ASSERT_DEBUG(((align - 1) & align) == 0, "");

				// Compute new aligned address
				uintptr_t aligned = reinterpret_cast<uintptr_t>(res) & ~(static_cast<uintptr_t>(align) - 1);
				if (aligned != reinterpret_cast<uintptr_t>(res))
					aligned += align;

				res = reinterpret_cast<void*>(aligned);
			}

			// Create BigChunkHeader
			BigChunkHeader* h = new (BigChunkHeader::from(res) - 1) BigChunkHeader();
			h->size = bytes;
			h->th.offset_bytes = static_cast<unsigned>(h->as_char() - block->as_char());
			h->th.status = MICRO_ALLOC_BIG;

			MICRO_ASSERT_DEBUG(res > block && (!align || (reinterpret_cast<uintptr_t>(res) % align == 0)), "");
			return res;
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::print_stats_if_necessary(bool force) noexcept
		{
			// Print stats based on trigger strategy
			bool print = force;
			if (!print) {
				if (params().print_stats_trigger & MicroOnBytes)
					if (stats().max_alloc_bytes.load() - last_bytes.load() >= params().print_stats_bytes) {
						last_bytes.store(stats().max_alloc_bytes.load());
						print = true;
					}
				if (!print && params().print_stats_trigger & MicroOnTime) {
					uint64_t current = el_timer.tock();
					double el_ms = static_cast<double>(current - last_time.load()) * 1e-6;
					if (el_ms >= params().print_stats_ms) {
						last_time.store(current);
						print = true;
					}
				}
			}
			if (print && stats_output) {

				if (params().print_stats_csv) {
					if (!header_printed.exchange(true))
						print_stats_header(default_print_callback, stats_output);
					print_stats_row(default_print_callback, stats_output);
				}
				else {
					print_stats(default_print_callback, stats_output);
				}
			}
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::print_os_infos(print_callback_type callback, void* opaque) const noexcept
		{
			print_generic(callback, opaque, MicroNoLog, nullptr, "os_page_size\t%u\n", static_cast<unsigned>(page_size()));
			print_generic(callback, opaque, MicroNoLog, nullptr, "os_allocation_granularity\t%u\n", static_cast<unsigned>(page_provider()->allocation_granularity()));
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::print_exit_infos(print_callback_type callback, void* opaque) const noexcept
		{
			double elapsed = static_cast<double>(el_timer.tock()) * 1e-9;
			micro_process_infos infos;
			os_process_infos(infos);

			print_generic(callback, opaque, MicroNoLog, nullptr, "Peak_RSS\t" MICRO_U64F "\n", static_cast<std::uint64_t>(infos.peak_rss));
			print_generic(callback, opaque, MicroNoLog, nullptr, "Peak_Commit\t" MICRO_U64F "\n", static_cast<std::uint64_t>(infos.peak_commit));
			print_generic(callback, opaque, MicroNoLog, nullptr, "Page_Faults\t" MICRO_U64F "\n", static_cast<std::uint64_t>(infos.page_faults));
			print_generic(callback, opaque, MicroNoLog, nullptr, "Elapsed_Seconds\t%f\n", elapsed);
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::init_internal() noexcept
		{
			// Initialize printing variables
			const char* f = params().print_stats.data();
			if (f[0]) {

				if (strcmp(f, "stdout") == 0) {
					stats_output = stdout;
				}
				else if (strcmp(f, "stderr") == 0) {
					stats_output = stderr;
				}
				else {
					continuous = fopen(f, "w");
					if (continuous)
						stats_output = continuous;
					else if (params().log_level >= MicroWarning)
						print_stderr(MicroWarning, "unable to open log file %s\n", f);
				}
				if (stats_output) {
					setvbuf(stats_output, nullptr, _IONBF, 0);
					if (params().print_stats_csv)
						fprintf(stats_output, "sep=\t\n");
					print_os_infos(default_print_callback, stats_output);
					params().print(default_print_callback, stats_output);
					fprintf(stats_output, "\n");
				}
			}
		}

		MICRO_EXPORT_CLASS_MEMBER MemoryManager::MemoryManager(const parameters& p) noexcept
		  : MemoryManager(p, false)
		{
			init();
		}
		MICRO_EXPORT_CLASS_MEMBER MemoryManager::MemoryManager(const parameters& p, bool) noexcept
		  : BaseMemoryManager(p)
		  , os_psize(static_cast<unsigned>(page_provider()->page_size()))
		  , os_psize_bits(static_cast<unsigned>(page_provider()->page_size_bits()))
		  , os_alloc_granularity(static_cast<unsigned>(page_provider()->allocation_granularity()))
		  , os_max_medium_pages(compute_max_medium_pages())
		  , os_max_medium_size(compute_max_medium_size())
		  , page_map(this)
		{
			end.left = end.right = &end;
			end_free.left_free = end_free.right_free = &end_free;
			get_main_manager() = this;
			el_timer.tick();
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::perform_exit_operations() noexcept
		{
			if (on_exit_done.exchange(true))
				return;
			init();
			// print statistics on exit
			if (stats_output) {
				if (params().print_stats_trigger)
					print_stats_if_necessary(true);
				print_exit_infos(default_print_callback, stats_output);
				if (continuous)
					fclose(continuous);
			}
		}

		MICRO_EXPORT_CLASS_MEMBER MemoryManager::~MemoryManager() noexcept
		{
			// print statistics on exit
			perform_exit_operations();

			// Do NOT free pages if this is the main manager
#ifdef MICRO_OVERRIDE
			if (get_main_manager() != this)
#endif
			{

				// clear pages
				if (page_provider()->own_pages())
					clear();
			}
#ifdef MICRO_OVERRIDE
			if (get_main_manager() == this) {
				get_main_manager() = nullptr;
			}
#endif
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::clear() noexcept
		{
			std::unique_lock<lock_type> ll(lock);

			if (arenas) {
				used_pages = 0;
				used_spans = 0;
				free_page_count = 0;
				side_pages = 0;

				// No-op, keep it in case we add a destructor to Arena, RadixTree or TinyMemPool
				for (unsigned i = 0; i < params().max_arenas; ++i)
					get_arenas()[i].~Arena();

				// deallocate pages
				PageRunHeader* next = end.right;
				while (next != &end) {
					PageRunHeader* p = next;
					next = next->right;
					p->~PageRunHeader();
					page_provider()->deallocate_pages(p, static_cast<size_t>((p->run_size() >> os_psize_bits)));
				}

				page_provider()->reset();
				page_map.reset();
				// Since all pages were deallocated, the radix tree and memory pools are fully invalidated
				// They will be recreated in the next call to allocate()

				end.left = end.right = &end;
				end_free.left_free = end_free.right_free = &end_free;
				arenas = nullptr;
			}
		}

		MICRO_EXPORT_CLASS_MEMBER PageRunHeader* MemoryManager::allocate_pages(size_t page_count) noexcept
		{
			size_t size_bytes = page_count << os_psize_bits;

			// Align to os_alloc_granularity
			if (MICRO_UNLIKELY(size_bytes & (os_alloc_granularity - 1))) {
				size_bytes = (size_bytes / os_alloc_granularity + 1u) * os_alloc_granularity;
				page_count = size_bytes >> os_psize_bits;
			}

			PageRunHeader* res = nullptr;
			bool allocated = false;
			{
				std::unique_lock<lock_type> ll(lock);
				// Try to reuse free page run
				if (page_count == max_medium_pages() && end_free.right_free != &end_free) {
					res = end_free.right_free;
					res->remove_free();
					free_page_count -= max_medium_pages();
				}
				else {
					// We are going to allocate pages, make sure it won't go over the limit
					size_t current_pages = used_pages.load(std::memory_order_relaxed) + free_page_count;
					if (MICRO_UNLIKELY(params().memory_limit && params().memory_limit < (current_pages + page_count) * os_psize))
						return nullptr;
				}
			}
			if (!res) {
				// Allocate pages
				res = PageRunHeader::from(page_provider()->allocate_pages(page_count));
				if (MICRO_UNLIKELY(!res))
					return nullptr;
				new (res) PageRunHeader();
				res->size_bytes = size_bytes;
				allocated = true;
			}

			if (page_count < max_medium_pages())
				side_pages.fetch_add(page_count);

			res->arena = this;
			used_pages += page_count;
			++used_spans;

			std::unique_lock<lock_type> ll(lock);
			if (allocated)
				res->insert(&end);
			if (used_pages.load(std::memory_order_relaxed) + free_page_count > max_pages.load(std::memory_order_relaxed))
				max_pages.store(used_pages.load(std::memory_order_relaxed) + free_page_count);
			return res;
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::deallocate_pages(PageRunHeader* p) noexcept
		{
			size_t page_count = static_cast<size_t>(p->size_bytes >> os_psize_bits);

			std::uint64_t limit = 0;
			if (params().backend_memory) {
				if (params().backend_memory <= 100)
					limit = (used_pages.load() * params().backend_memory / 100) << os_psize_bits;
				else
					limit = params().backend_memory;
			}

			PageRunHeader* to_free = nullptr;
			{
				std::unique_lock<lock_type> ll(lock);

				// Deallocate free pages until we reach the target backend memory (if any)
				PageRunHeader* r = end_free.right_free;
				while (r != &end_free && (free_page_count << os_psize_bits) > limit) {
					PageRunHeader* next = r->right_free;
					r->remove();
					r->remove_free();
					r->right_free = to_free;
					to_free = r;
					r = next;
					free_page_count -= max_medium_pages();
				}

				if (p->run_size() == (max_medium_pages() << os_psize_bits)) {
					// Insert pages to deallocate to the free list (we always have at least one free page run)
					p->insert_free(&end_free);
					free_page_count += max_medium_pages();

#ifdef MICRO_DEBUG
					// Ensure the page is free of chunks
					MediumChunkHeader* first = MediumChunkHeader::from(p + 1);
					MICRO_ASSERT_DEBUG(first->elems * 16u == (MICRO_BLOCK_SIZE - sizeof(PageRunHeader) - sizeof(MediumChunkHeader)), "");
#endif
				}
				else {
					// Remove from list of page runs and add to the deallocation list
					p->remove();
					p->right_free = to_free;
					to_free = p;
				}

				used_pages -= page_count;
				--used_spans;

				// Remove page run from the page map
				page_map.erase(p);
			}

			// Actual page deallocation.
			// No need to hold the manager lock.
			while (to_free) {
				auto* next = to_free->right_free;
				page_provider()->deallocate_pages(to_free, to_free->run_size() >> os_psize_bits);
				to_free = next;
			}
		}

		MICRO_EXPORT_CLASS_MEMBER PageRunHeader* MemoryManager::allocate_medium_block() noexcept
		{
			// Allocate page run suitable for the radix tree
			PageRunHeader* run = allocate_pages(max_medium_pages());
			if (run && !page_map.insert(run, false)) {
				deallocate_pages(run);
				return nullptr;
			}
			return run;
		}

		MICRO_EXPORT_CLASS_MEMBER PageRunHeader* MemoryManager::allocate_pages_for_bytes(size_t bytes) noexcept
		{
			size_t pages = bytes >> os_psize_bits;
			if ((pages << os_psize_bits) < bytes)
				++pages;
			if (pages == 0)
				pages = 1;
			return allocate_pages(pages);
		}

#ifdef MICRO_ENABLE_TIME_STATISTICS
		// Thread local precise timer fo statistics
		static MICRO_ALWAYS_INLINE timer& get_local_timer() noexcept
		{
			thread_local timer t;
			return t;
		}
#endif

		MICRO_EXPORT_CLASS_MEMBER void* MemoryManager::allocate_big_path(size_t bytes, unsigned align, bool stats) noexcept
		{
			// Big allocation
#ifdef MICRO_ENABLE_TIME_STATISTICS
			if (stats)
				get_local_timer().tick();
#endif
			void* res = allocate_big(bytes, align);
			if (MICRO_UNLIKELY(res && stats))
				record_stats(res, MICRO_ALLOC_BIG);
			return res;
		}

		MICRO_EXPORT_CLASS_MEMBER void* MemoryManager::allocate_in_other_arenas(size_t bytes, unsigned elems, unsigned align, Arena* first, bool) noexcept
		{
			// Try to allocate from other manager arenas.
			// By default, inspect ALL arenas starting from*
			// a random position.
			//
			// For high memory levels, we might inspect fewer arenas.
			// The andom start position ensures that all arenas
			// will be inspected eventually.
			//

			if (!params().deplete_arenas || params().max_arenas == 1)
				return nullptr;

			unsigned count = std::min(get_max_thread_count(), params().max_arenas);
			unsigned inspect_count = count / MICRO_DEPLETE_ARENA_FACTOR;
			if (inspect_count == 0)
				inspect_count = 1;
			unsigned start = random_uint32() % count;
			bool is_small = bytes <= params().small_alloc_threshold && align <= MICRO_MINIMUM_ALIGNMENT;
			for (unsigned i = 0; i < inspect_count; ++i, ++start) {
				if (start >= count)
					start = 0;
				auto* a = arenas[start].arena();
				if (a == first)
					continue;
				if (is_small) {
					if (void* r = a->tiny_pool()->allocate(static_cast<unsigned>(bytes), false)) {
						return r;
					}
				}
				else {
					if (a->other_arenas_count.load(std::memory_order_relaxed))
						continue;
					if (void* r = a->tree()->allocate_elems(elems, align, false)) {
						MICRO_ASSERT_DEBUG(!r || align == 0 || (reinterpret_cast<uintptr_t>(r) % align) == 0, "");
						return r;
					}
				}
			}

			if (is_small) {

				// Small objects:
				// If other arena tiny pool were empty,
				// try to allocate from other arena radix
				// tree. To avoid too much lock contention,
				// use allocate_small_fast().

				for (unsigned i = 0; i < inspect_count; ++i, ++start) {
					if (start >= count)
						start = 0;
					auto* a = arenas[start].arena();
					if (a != first)
						if (void* r = a->tree()->allocate_small_fast(elems))
							return r;
				}
			}
			return nullptr;
		}

		MICRO_EXPORT_CLASS_MEMBER void* MemoryManager::allocate_no_tiny_pool(size_t bytes, unsigned obj_size, unsigned align, bool* is_small) noexcept
		{
			// Allocate for the tiny memory pool.
			// Try first to allocate requested bytes and alignment.
			// If it fails, try to allocate a single object instead of a TinyBlockPool.

			MICRO_ASSERT_DEBUG(bytes < max_medium_size(), "");

			if (MICRO_UNLIKELY(!arenas))
				if (!initialize_arenas())
					return nullptr;

			// Note: bytes CANNOT be 0
			unsigned elems = RadixTree::bytes_to_elems(static_cast<unsigned>(bytes));
			unsigned obj_elems = RadixTree::bytes_to_elems(obj_size);
			Arena* a = select_arena();

			// First, try to allocate and aligned block to hold a TinyBlockPool
			void* r = a->tree()->allocate_elems(elems, align, false);

			// Then, directly try to allocate an object of given size.
			// This might work as aligned allocation of MICRO_ALIGNED_POOL
			// will leave lots of holes
			if (!r && obj_size && a->tree()->has_small_free_chunks()) {
				r = a->tree()->allocate_elems(obj_elems, 0, false);
				if (r)
					*is_small = true;
			}
#ifndef MICRO_NO_LOCK
			if (!r && params().deplete_arenas) {
				// Increment counter for this arena, telling that it is already depleted
				Counter cnt(a->other_arenas_count);

				// Try to allocate an aligned block to hold a TinyBlockPool
				r = allocate_in_other_arenas(bytes, elems, align, a, true);

				if (!r && obj_size) {
					// Directly try to allocate an object of given size
					r = allocate_in_other_arenas(obj_size, obj_elems, 0, a, true);
					if (r)
						*is_small = true;
				}
			}
#endif
			if (!r)
				// Allocate a block to hold a TinyBlockPool
				r = a->tree()->allocate_elems(elems, align, true);

			return r;
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::deallocate_no_tiny_pool(void* p) noexcept
		{
			// Deallocate from the radix tree
			MediumChunkHeader* h = MediumChunkHeader::from(p) - 1;
			MICRO_ASSERT_DEBUG(h->th.status == MICRO_ALLOC_MEDIUM, "");
			MICRO_ASSERT_DEBUG(h->th.guard == MICRO_BLOCK_GUARD, "");
			auto* parent = h->parent();
			auto* arena = static_cast<Arena*>(parent->arena);
			arena->tree()->deallocate(p);
		}

		MICRO_EXPORT_CLASS_MEMBER void* MemoryManager::allocate(size_t bytes, unsigned align) noexcept
		{
			if (MICRO_UNLIKELY(!arenas)) {
				// Initialize arenas if necessary
				if (!initialize_arenas())
					return nullptr;
			}

			// Check alignment value
			MICRO_ASSERT_DEBUG(align == 0 || (align & (align - 1)) == 0, "");

			if (MICRO_UNLIKELY(bytes > max_medium_size() - align || align >= MICRO_ALIGNED_POOL))
				// Big allocation or big alignment
				return allocate_big_path(bytes, align, params().print_stats_trigger);

			//if (MICRO_UNLIKELY(bytes == 0))
			//	bytes = 1;
			bytes += (bytes == 0);

			void* res;

#if MICRO_THREAD_LOCAL_NO_ALLOC == 0
			// Detect recursion due, for instance, to thread_local storage or fprintf function
			DetectRecursion::KeyHolder holder;
			if (get_main_manager() == this) {
				holder.k = get_detect_recursion().insert((std::uint32_t)this_thread_id_hash());
				if (MICRO_UNLIKELY(!holder)) {
					unsigned elems = RadixTree::bytes_to_elems(static_cast<unsigned>(bytes));
					// Do NOT record stats as it might trigger yet another allocation
					return arenas[0].arena()->tree()->allocate_elems(elems, align, true);
				}
			}
#endif

#ifdef MICRO_OVERRIDE
			// If malloc override is on, initialize printing stuff here.
			// Any triggered allocation will go through the recursion detection.
			init();
#endif

			// Select arena based on thread id
			Arena* arena = select_arena();

#if defined(MICRO_ENABLE_STATISTICS_PARAMETERS) && defined(MICRO_ENABLE_TIME_STATISTICS)
			// Time statistics
			if (params().print_stats_trigger)
				get_local_timer().tick();
#endif

			if (bytes <= params().small_alloc_threshold && align <= MICRO_MINIMUM_ALIGNMENT) {
				// Allocate from the tiny memory pool for small objects
				res = arena->tiny_pool()->allocate(static_cast<unsigned>(bytes), true);
			}
			else {
				unsigned elems = RadixTree::bytes_to_elems(static_cast<unsigned>(bytes));
				// Allocate from the radix tree, but do not force calls to page allocation
				res = arena->tree()->allocate_elems(elems, align, params().max_arenas == 1);
				if (!res) {
#ifndef MICRO_NO_LOCK
					// Try from other arenas
					if (params().deplete_arenas) {
						// Increment counter for this arena, telling that it is already depleted
						Counter cnt(arena->other_arenas_count);
						res = allocate_in_other_arenas(bytes, elems, align, arena);
					}
					if (!res)
#endif
						// Retry with potential page allocation
						res = arena->tree()->allocate_elems(elems, align, true);
				}
			}

#ifdef MICRO_ENABLE_STATISTICS_PARAMETERS
			// Record statistics (TODO: optimize)
			if (MICRO_UNLIKELY(params().print_stats_trigger && res))
				record_stats(res);
#endif

			// Check alignment
			MICRO_ASSERT_DEBUG(!res || align == 0 || (reinterpret_cast<uintptr_t>(res) % align) == 0, "");
			return res;
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::record_stats(void* p, int status) noexcept
		{
			// Record allocation statistics
			if (!p)
				return;

			// Time statistics
			MICRO_TIME_STATS(this->mem_stats.update_alloc_time(get_local_timer().tock()));

			// Memory statistics
			if (status == MICRO_ALLOC_SMALL_BLOCK) {
				this->mem_stats.allocate_small(usable_size(p, MICRO_ALLOC_SMALL_BLOCK));
			}
			else {
				size_t s = usable_size(p, status);
				if (status == MICRO_ALLOC_BIG)
					this->mem_stats.allocate_big(s);
				else if (status == MICRO_ALLOC_MEDIUM)
					this->mem_stats.allocate_medium(s);
			}

			// Statistics dump
			if (params().print_stats_trigger > 1)
				print_stats_if_necessary();
		}

		MICRO_EXPORT_CLASS_MEMBER MemoryManager* MemoryManager::find_from_page_run(PageRunHeader* run) noexcept
		{
			// Find the MemoryManager that manages given page run.
			// Returns null if the parent MemoryManager cannot be found (meaning that the page run wasn't created by the micro library)
			end_lock().lock_shared();
			BaseMemoryManagerIter* m = end_mgr()->right;
			MemoryManager* found = nullptr;
			while (m != end_mgr()) {
				if (static_cast<MemoryManager*>(m)->page_map.find(run)) {
					found = static_cast<MemoryManager*>(m);
					break;
				}
				m = m->right;
			}
			end_lock().unlock_shared();
			return found;
		}
		MICRO_EXPORT_CLASS_MEMBER MemoryManager* MemoryManager::find_from_ptr(void* p) noexcept
		{
			// Find the MemoryManager that manages given address.
			// Returns null if the parent MemoryManager cannot be found (meaning that the allocated chunk wasn't created by the micro library)
			end_lock().lock_shared();
			BaseMemoryManagerIter* m = end_mgr()->right;
			MemoryManager* found = nullptr;
			while (m != end_mgr()) {
				if (static_cast<MemoryManager*>(m)->page_map.own(p)) {
					found = static_cast<MemoryManager*>(m);
					break;
				}
				m = m->right;
			}
			end_lock().unlock_shared();
			return found;
		}

		MICRO_EXPORT_CLASS_MEMBER int MemoryManager::type_of_maybe_small(SmallChunkHeader* tiny, block_pool_type* pool, void* p) noexcept
		{
			// we have a conflict here: this might be a very small object (without header) and we are unlucky to have the
			// tiny header set with valid values. Or, this really is a small/medium/big object with a header.
			// To check that, we must get the corresponding PageRunHeader and find it into a map of all
			// allocated PageRunHeader. This must be done in last resort as it is relatively expensive.
			// The first obvious cases are first checked.

			PageRunHeader* run_from_tiny = pool->header.parent();

			if (tiny->status == MICRO_ALLOC_MEDIUM) {
				// The most likely: a valid medium chunk located just at the end of a small block
				MediumChunkHeader* mediumh = MediumChunkHeader::from(p) - 1;
				PageRunHeader* run_from_medium = mediumh->parent(); //(PageRunHeader*)(mediumh - mediumh->th.offset_bytes);
				if (run_from_tiny == run_from_medium) {
					// Ok, now we are sure the PageRunHeader is valid and can be accessed
					// We sill need to check if the pool is accessible
					if (pool->as_char() > run_from_tiny->as_char() && pool->as_char() < run_from_tiny->as_char() + run_from_tiny->run_size()) {
						if (!pool->is_inside(p) || pool->header.guard != MICRO_BLOCK_GUARD)
							return tiny->status;
					}
				}
			}

			// Ok, now we must find the corresponding page run and MemoryManager to go further
			MemoryManager* m = find_from_page_run(run_from_tiny);
			if (!m)
				// Invalid block header, this is NOT a MICRO_ALLOC_SMALL_BLOCK
				return tiny->status;

			// At this point we know the block pool is accessible, but the pointer to deallocate might still be outside:
			// we deallocate a medium chunk that starts right before the end of a block pool aligned on page size.
			if (!pool->is_inside(p) || /*pool->mgr != m*/ !m->has_mem_pool(pool->get_parent()))
				return tiny->status;

			// If this is not a small block, the pool run page should be invalid
			if (!m->page_map.find(pool->get_parent_run()))
				return tiny->status;

			// Final check : we know the parent PageRunHeader is valid, but does it contains a valid pool at this address ?
			if (!pool->get_parent_run()->test_pool(pool))
				return tiny->status;

			MICRO_ASSERT_DEBUG(pool->header.tail <= pool->get_chunk_size(), "");
			return MICRO_ALLOC_SMALL_BLOCK;
		}

		MICRO_EXPORT_CLASS_MEMBER MemoryManager*& MemoryManager::get_main_manager() noexcept
		{
			static MemoryManager* main = nullptr;
			return main;
		}

		MICRO_EXPORT_CLASS_MEMBER bool MemoryManager::verify_block(int status, void* p) noexcept
		{
#ifdef MICRO_DEBUG
			// Check block status before deallocation.
			// Note: this is VERY slow
			if (status == MICRO_ALLOC_MEDIUM || status == MICRO_ALLOC_SMALL_BLOCK) {
				MediumChunkHeader* h;
				PageRunHeader* from_small = nullptr;
				if (status == MICRO_ALLOC_MEDIUM)
					h = MediumChunkHeader::from(p) - 1;
				else {
					uintptr_t aligned = reinterpret_cast<uintptr_t>(p) & ~(MICRO_ALIGNED_POOL - 1ull);
					using pool_type = MemoryManager::block_pool_type;
					pool_type* pool = pool_type::from(aligned);
					if (pool->header.offset_bytes == 0) {
						pool = pool_type::from(reinterpret_cast<char*>(aligned) + sizeof(PageRunHeader) + sizeof(MediumChunkHeader));
					}
					h = MediumChunkHeader::from(pool) - 1;
					from_small = pool->get_parent_run();
					MICRO_ASSERT_DEBUG(from_small, "");
				}
				h->parent()->lock.lock();
#if MICRO_USE_NODE_LOCK == 0
				check_prev_next(h);
#endif
				PageRunHeader* mem = h->parent();
				MemoryManager* m = static_cast<MemoryManager*>(static_cast<Arena*>(mem->arena)->manager());
				h->parent()->lock.unlock();

				MICRO_ASSERT_DEBUG(m->pmap().find(mem), "");
				MICRO_ASSERT_DEBUG(from_small == nullptr || from_small == mem, "");
			}
			if (status == MICRO_ALLOC_BIG) {
				BigChunkHeader* h = BigChunkHeader::from(p) - 1;
				MICRO_ASSERT_DEBUG(h->th.guard == MICRO_BLOCK_GUARD, "");

				PageRunHeader* mem = PageRunHeader::from(h->as_char() - h->th.offset_bytes);
				MemoryManager* m = static_cast<MemoryManager*>(mem->arena);

				MICRO_ASSERT_DEBUG(m->pmap().find(mem), "");
			}
#else
			(void)status;
			(void)p;
#endif
			return true;
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::deallocate(void* p, int status, block_pool_type* pool, BaseMemoryManager* mgr, bool stats) noexcept
		{
			MemoryManager* m = nullptr;
			if (status == MICRO_ALLOC_SMALL_BLOCK) {

				// Usually, MICRO_ALLOC_SMALL_BLOCK are handle before reaching this point.
				// But we might still end there with MICRO_OVERRIDE.

				deallocate_small(p, pool, static_cast<MemoryManager*>(mgr), stats);
				return;
			}

			// Get chunk header, verify integrity
			auto* tiny = SmallChunkHeader::from(p) - 1;
			MICRO_ASSERT(tiny->guard == MICRO_BLOCK_GUARD, "");

			if (MICRO_UNLIKELY(tiny->status == MICRO_ALLOC_MEDIUM)) {
				// Medium chunk that belongs to the radix tree
				auto* parent = (MediumChunkHeader::from(p) - 1)->parent();
				auto* arena = static_cast<Arena*>(parent->arena);
				m = static_cast<MemoryManager*>(arena->manager());
#if defined(MICRO_ENABLE_STATISTICS_PARAMETERS) && defined(MICRO_ENABLE_TIME_STATISTICS)
				if (MICRO_UNLIKELY(m->params().print_stats_trigger && stats))
					get_local_timer().tick();
#endif
				size_t bytes = static_cast<Arena*>(arena)->tree()->deallocate(p);
#ifdef MICRO_ENABLE_STATISTICS_PARAMETERS
				if (MICRO_UNLIKELY(m->params().print_stats_trigger && stats)) {
					MICRO_TIME_STATS(m->mem_stats.update_dealloc_time(get_local_timer().tock()));
					m->mem_stats.deallocate_medium(bytes);
				}
#endif
			}
			else {
				// Big chunk
				MICRO_ASSERT_DEBUG(tiny->status == MICRO_ALLOC_BIG, "Invalid block header");
				BigChunkHeader* h = BigChunkHeader::from(p) - 1;
				MICRO_ASSERT_DEBUG(h->th.guard == MICRO_BLOCK_GUARD, "");

				PageRunHeader* mem = PageRunHeader::from(h->as_char() - h->th.offset_bytes);
				m = static_cast<MemoryManager*>(mem->arena);
#if defined(MICRO_ENABLE_STATISTICS_PARAMETERS) && defined(MICRO_ENABLE_TIME_STATISTICS)
				if (MICRO_UNLIKELY(m->params().print_stats_trigger && stats))
					get_local_timer().tick();
#endif
				size_t bytes = usable_size(p, MICRO_ALLOC_BIG);
				m->deallocate_pages(mem);
#ifdef MICRO_ENABLE_STATISTICS_PARAMETERS
				if (MICRO_UNLIKELY(m->params().print_stats_trigger && stats)) {
					MICRO_TIME_STATS(m->mem_stats.update_dealloc_time(get_local_timer().tock()));
					m->mem_stats.deallocate_big(bytes);
				}
#endif
			}
		}

		MICRO_EXPORT_CLASS_MEMBER size_t MemoryManager::usable_size(void* p, int status) noexcept
		{
			// Get chunk full size in bytes

			if (status == MICRO_ALLOC_SMALL_BLOCK) {
				uintptr_t aligned = reinterpret_cast<uintptr_t>(p) & ~(MICRO_ALIGNED_POOL - 1ull);
				block_pool_type* pool = block_pool_type::from(aligned);
				return SmallAllocation::idx_to_size(static_cast<unsigned>(pool->header.pool_idx_plus_one) - 1);
			}

			auto* tiny = SmallChunkHeader::from(p) - 1;
			MICRO_ASSERT_DEBUG(tiny->guard == MICRO_BLOCK_GUARD, "");

			if (tiny->status == MICRO_ALLOC_BIG) {
				BigChunkHeader* h = BigChunkHeader::from(p) - 1;
				MICRO_ASSERT_DEBUG(h->th.guard == MICRO_BLOCK_GUARD, "");
				MICRO_ASSERT_DEBUG(h->th.status == MICRO_ALLOC_BIG, "");

				PageRunHeader* mem = PageRunHeader::from(h->as_char() - h->th.offset_bytes);
				return static_cast<size_t>(mem->run_size() - static_cast<unsigned>(static_cast<char*>(p) - mem->as_char()));
			}
			else if (tiny->status == MICRO_ALLOC_MEDIUM) {
				MediumChunkHeader* f = MediumChunkHeader::from(p) - 1;
				return (static_cast<unsigned>(f->elems) << MICRO_ELEM_SHIFT);
			}

			MICRO_ASSERT(false, "Invalid block header");
			MICRO_UNREACHABLE();
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::reset_statistics() noexcept
		{
			// Reset stats
			this->mem_stats.reset();
			this->last_bytes.store(0);
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::set_start_time() 
		{ 
			// Reset start time
			this->el_timer.tick();
		}

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::dump_statistics(micro_statistics& st) noexcept
		{
			// Dump stats

			st.max_used_memory = max_pages.load(std::memory_order_relaxed) << os_psize_bits;
			st.current_used_memory = (used_pages.load(std::memory_order_relaxed) + free_page_count) << os_psize_bits;

			st.max_alloc_bytes = stats().max_alloc_bytes;
			st.total_alloc_bytes = stats().total_alloc_bytes;

			st.small.alloc_count = stats().small.alloc_count;
			st.small.freed_count = stats().small.freed_count;
			st.small.alloc_bytes = stats().small.alloc_bytes;
			st.small.freed_bytes = stats().small.freed_bytes;
			st.small.current_alloc_count = stats().small.current_alloc_count;
			st.small.current_alloc_bytes = stats().small.current_alloc_bytes;

			st.medium.alloc_count = stats().medium.alloc_count;
			st.medium.freed_count = stats().medium.freed_count;
			st.medium.alloc_bytes = stats().medium.alloc_bytes;
			st.medium.freed_bytes = stats().medium.freed_bytes;
			st.medium.current_alloc_count = stats().medium.current_alloc_count;
			st.medium.current_alloc_bytes = stats().medium.current_alloc_bytes;

			st.big.alloc_count = stats().big.alloc_count;
			st.big.freed_count = stats().big.freed_count;
			st.big.alloc_bytes = stats().big.alloc_bytes;
			st.big.freed_bytes = stats().big.freed_bytes;
			st.big.current_alloc_count = stats().big.current_alloc_count;
			st.big.current_alloc_bytes = stats().big.current_alloc_bytes;

			st.total_alloc_time_ns = stats().total_alloc_time_ns;
			st.total_dealloc_time_ns = stats().total_dealloc_time_ns;
		}

		static inline std::uint64_t div_bytes(std::uint64_t a, std::uint64_t b) noexcept { return b == 0 ? 0ull : static_cast<std::uint64_t>(static_cast<double>(a) / static_cast<double>(b)); }

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::print_stats_header(print_callback_type callback, void* opaque) noexcept
		{
			// Print stats header for CSV output (still experimental)
			print_generic(callback,
				      opaque,
				      MicroNoLog,
				      "DATE",
				      "PEAK_PAGES\tCURRENT_PAGES\tCURRENT_SPANS\tPEAK_REQ_MEM\tPEAK_MEM\tCURRENT_MEM\tALLOCS\tALLOCS_B\tALLOCS_AVG\tFREE\tFREE_B\tCURRENT\tCURRENT_B\tCURRENT_AVG\t"
				      "S_ALLOCS\tS_ALLOCS_B\tS_ALLOCS_AVG\tS_FREE\tS_FREE_B\tS_CURRENT\tS_CURRENT_B\tS_CURRENT_AVG\t"
				      "M_ALLOCS\tM_ALLOCS_B\tM_ALLOCS_AVG\tM_FREE\tM_FREE_B\tM_CURRENT\tM_CURRENT_B\tM_CURRENT_AVG\t"
				      "B_ALLOCS\tB_ALLOCS_B\tB_ALLOCS_AVG\tB_FREE\tB_FREE_B\tB_CURRENT\tB_CURRENT_B\tB_CURRENT_AVG\n");
		}
		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::print_stats_header_stdout() noexcept { print_stats_header(default_print_callback, stdout); }

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::print_stats_row(print_callback_type callback, void* opaque) noexcept
		{
			// Print stats for CSV output (still experimental) 
			auto tot_alloc = this->mem_stats.small.alloc_count.load() + this->mem_stats.medium.alloc_count.load() + this->mem_stats.big.alloc_count.load();
			auto tot_alloc_bytes = this->mem_stats.small.alloc_bytes.load() + this->mem_stats.medium.alloc_bytes.load() + this->mem_stats.big.alloc_bytes.load();
			auto tot_freed = this->mem_stats.small.freed_count.load() + this->mem_stats.medium.freed_count.load() + this->mem_stats.big.freed_count.load();
			auto tot_freed_bytes = this->mem_stats.small.freed_bytes.load() + this->mem_stats.medium.freed_bytes.load() + this->mem_stats.big.freed_bytes.load();

			auto cur_alloc = this->mem_stats.small.current_alloc_count.load() + this->mem_stats.medium.current_alloc_count.load() + this->mem_stats.big.current_alloc_count.load();
			auto cur_alloc_bytes = this->mem_stats.total_alloc_bytes.load();

			print_generic(callback,
				      opaque,
				      MicroNoLog,
				      nullptr,
				      MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F
						 "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F
						 "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F
						 "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F "\t" MICRO_U64F
						 "\t" MICRO_U64F "\n",
				      max_pages.load(),
				      used_pages.load(),
				      used_spans.load(),
				      this->mem_stats.max_alloc_bytes.load(),
				      max_pages.load() * page_size(),
				      used_pages.load() * page_size(),
				      tot_alloc,
				      tot_alloc_bytes,
				      div_bytes(tot_alloc_bytes, tot_alloc),
				      tot_freed,
				      tot_freed_bytes,
				      cur_alloc,
				      cur_alloc_bytes,
				      div_bytes(cur_alloc_bytes, cur_alloc),
				      this->mem_stats.small.alloc_count.load(),
				      this->mem_stats.small.alloc_bytes.load(),
				      div_bytes(this->mem_stats.small.alloc_bytes, this->mem_stats.small.alloc_count),
				      this->mem_stats.small.freed_count.load(),
				      this->mem_stats.small.freed_bytes.load(),
				      this->mem_stats.small.current_alloc_count.load(),
				      this->mem_stats.small.current_alloc_bytes.load(),
				      div_bytes(this->mem_stats.small.current_alloc_bytes, this->mem_stats.small.current_alloc_count),
				      this->mem_stats.medium.alloc_count.load(),
				      this->mem_stats.medium.alloc_bytes.load(),
				      div_bytes(this->mem_stats.medium.alloc_bytes, this->mem_stats.medium.alloc_count),
				      this->mem_stats.medium.freed_count.load(),
				      this->mem_stats.medium.freed_bytes.load(),
				      this->mem_stats.medium.current_alloc_count.load(),
				      this->mem_stats.medium.current_alloc_bytes.load(),
				      div_bytes(this->mem_stats.medium.current_alloc_bytes, this->mem_stats.medium.current_alloc_count),
				      this->mem_stats.big.alloc_count.load(),
				      this->mem_stats.big.alloc_bytes.load(),
				      div_bytes(this->mem_stats.big.alloc_bytes, this->mem_stats.big.alloc_count),
				      this->mem_stats.big.freed_count.load(),
				      this->mem_stats.big.freed_bytes.load(),
				      this->mem_stats.big.current_alloc_count.load(),
				      this->mem_stats.big.current_alloc_bytes.load(),
				      div_bytes(this->mem_stats.big.current_alloc_bytes, this->mem_stats.big.current_alloc_count));
		}
		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::print_stats_row_stdout() noexcept { print_stats_row(default_print_callback, stdout); }

		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::print_stats(print_callback_type callback, void* opaque) noexcept
		{
			// Print stats
			auto tot_alloc = this->mem_stats.small.alloc_count.load() + this->mem_stats.medium.alloc_count.load() + this->mem_stats.big.alloc_count.load();
			auto tot_alloc_bytes = this->mem_stats.small.alloc_bytes.load() + this->mem_stats.medium.alloc_bytes.load() + this->mem_stats.big.alloc_bytes.load();
			auto tot_freed = this->mem_stats.small.freed_count.load() + this->mem_stats.medium.freed_count.load() + this->mem_stats.big.freed_count.load();
			auto tot_freed_bytes = this->mem_stats.small.freed_bytes.load() + this->mem_stats.medium.freed_bytes.load() + this->mem_stats.big.freed_bytes.load();

			auto cur_alloc = this->mem_stats.small.current_alloc_count.load() + this->mem_stats.medium.current_alloc_count.load() + this->mem_stats.big.current_alloc_count.load();
			auto cur_alloc_bytes = this->mem_stats.total_alloc_bytes.load();

			print_generic(callback,
				      opaque,
				      MicroNoLog,
				      nullptr,
				      "\nPages: max pages " MICRO_U64F ", current pages " MICRO_U64F ", current spans " MICRO_U64F "\n"
				      "Global: max requested memory " MICRO_U64F " bytes, max used memory: " MICRO_U64F ", current used memory: " MICRO_U64F "\n"
				      "Total allocations:\t alloc " MICRO_U64F " (" MICRO_U64F " bytes, avg. " MICRO_U64F "/alloc),\t free " MICRO_U64F " (" MICRO_U64F " bytes),\t current " MICRO_U64F
				      " (" MICRO_U64F " bytes, avg. " MICRO_U64F "/alloc)\n"
				      "Small allocations:\t alloc " MICRO_U64F " (" MICRO_U64F " bytes, avg. " MICRO_U64F "/alloc),\t free " MICRO_U64F " (" MICRO_U64F " bytes),\t current " MICRO_U64F
				      " (" MICRO_U64F " bytes, avg. " MICRO_U64F "/alloc)\n"
				      "Medium allocations:\t alloc " MICRO_U64F " (" MICRO_U64F " bytes, avg. " MICRO_U64F "/alloc),\t free " MICRO_U64F " (" MICRO_U64F
				      " bytes),\t current " MICRO_U64F " (" MICRO_U64F " bytes, avg. " MICRO_U64F "/alloc)\n"
				      "Big allocations:\t alloc " MICRO_U64F " (" MICRO_U64F " bytes, avg. " MICRO_U64F "/alloc),\t free " MICRO_U64F " (" MICRO_U64F " bytes),\t current " MICRO_U64F
				      " (" MICRO_U64F " bytes, avg. " MICRO_U64F "/alloc)\n"
#ifdef MICRO_ENABLE_TIME_STATISTICS
				      "Timer allocation (ns):\t total " MICRO_U64F ", average " MICRO_U64F ", max " MICRO_U64F "\n"
				      "Timer deallocation (ns):\t total " MICRO_U64F ", average " MICRO_U64F ", max " MICRO_U64F "\n\n",
#else
				      "\n",
#endif

				      max_pages.load(),
				      used_pages.load(),
				      used_spans.load(),
				      this->mem_stats.max_alloc_bytes.load(),
				      max_pages.load() * page_size(),
				      used_pages.load() * page_size(),
				      tot_alloc,
				      tot_alloc_bytes,
				      div_bytes(tot_alloc_bytes, tot_alloc),
				      tot_freed,
				      tot_freed_bytes,
				      cur_alloc,
				      cur_alloc_bytes,
				      div_bytes(cur_alloc_bytes, cur_alloc),
				      this->mem_stats.small.alloc_count.load(),
				      this->mem_stats.small.alloc_bytes.load(),
				      div_bytes(this->mem_stats.small.alloc_bytes, this->mem_stats.small.alloc_count),
				      this->mem_stats.small.freed_count.load(),
				      this->mem_stats.small.freed_bytes.load(),
				      this->mem_stats.small.current_alloc_count.load(),
				      this->mem_stats.small.current_alloc_bytes.load(),
				      div_bytes(this->mem_stats.small.current_alloc_bytes, this->mem_stats.small.current_alloc_count),
				      this->mem_stats.medium.alloc_count.load(),
				      this->mem_stats.medium.alloc_bytes.load(),
				      div_bytes(this->mem_stats.medium.alloc_bytes, this->mem_stats.medium.alloc_count),
				      this->mem_stats.medium.freed_count.load(),
				      this->mem_stats.medium.freed_bytes.load(),
				      this->mem_stats.medium.current_alloc_count.load(),
				      this->mem_stats.medium.current_alloc_bytes.load(),
				      div_bytes(this->mem_stats.medium.current_alloc_bytes, this->mem_stats.medium.current_alloc_count),
				      this->mem_stats.big.alloc_count.load(),
				      this->mem_stats.big.alloc_bytes.load(),
				      div_bytes(this->mem_stats.big.alloc_bytes, this->mem_stats.big.alloc_count),
				      this->mem_stats.big.freed_count.load(),
				      this->mem_stats.big.freed_bytes.load(),
				      this->mem_stats.big.current_alloc_count.load(),
				      this->mem_stats.big.current_alloc_bytes.load(),
				      div_bytes(this->mem_stats.big.current_alloc_bytes, this->mem_stats.big.current_alloc_count)
#ifdef MICRO_ENABLE_TIME_STATISTICS
					,
				      this->mem_stats.total_alloc_time_ns.load(),
				      div_bytes(this->mem_stats.total_alloc_time_ns.load(),
						(this->mem_stats.small.alloc_count.load() + this->mem_stats.medium.alloc_count.load() + this->mem_stats.big.alloc_count.load())),
				      this->mem_stats.max_alloc_time_ns.load(),
				      this->mem_stats.total_dealloc_time_ns.load(),
				      div_bytes(this->mem_stats.total_dealloc_time_ns.load(),
						(this->mem_stats.small.freed_count.load() + this->mem_stats.medium.freed_count.load() + this->mem_stats.big.freed_count.load())),
				      this->mem_stats.max_dealloc_time_ns.load()
#endif
			);
		}
		MICRO_EXPORT_CLASS_MEMBER void MemoryManager::print_stats_stdout() noexcept { print_stats(default_print_callback, stdout); }

	} // end namespace detail

} // end namespace micro

MICRO_POP_DISABLE_OLD_STYLE_CAST
