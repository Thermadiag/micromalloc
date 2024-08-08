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

#ifndef MICRO_PAGE_MAP_HPP
#define MICRO_PAGE_MAP_HPP

#include "headers.hpp"
#include <algorithm>

#if defined(MICRO_DEBUG) && !defined(MICRO_OVERRIDE)
#define DEBUG_PAGE_MAP
#endif

#ifdef DEBUG_PAGE_MAP
#include <set>
#endif

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

namespace micro
{
	namespace detail
	{

		/// @brief Sorted map of PageRunHeader handling consecutive runs to reduce its size.
		///
		/// PageMap is a sorted array of Key objects, where Key represents one or more PageRunHeader.
		/// PageMap class provides an easy way to tell if a given PageRunHeader belongs to its parent BaseMemoryManager.
		///
		/// Each PageRunHeader have a size of at least MICRO_BLOCK_SIZE bytes. Consecutive PageRunHeader of
		/// size MICRO_BLOCK_SIZE (medium allocations) are merged to reduce the number of entries.
		///
		/// A sorted array is preffered over a hash table as its number of entries is very low compared to
		/// a hash table thanks to the merge mechanism.
		///
		/// Finding a PageRunHeader is (almost) as simple as a straight std::lower_bound.
		///
		class PageMap
		{
			// Key representing 1 or more consecutive PageRunHeader,
			// contains the page address, the number of concsecutive runs (upto page_size -2) of size MICRO_BLOCK_SIZE,
			// and tells if the run corresponds to a big allocation (size above MICRO_BLOCK_SIZE)
			struct Key
			{
				uintptr_t value;
				MICRO_ALWAYS_INLINE PageRunHeader* page(uintptr_t psize) const noexcept { return PageRunHeader::from(reinterpret_cast<void*>(value & ~(psize - 1u))); }
				MICRO_ALWAYS_INLINE uintptr_t count(uintptr_t psize) const noexcept { return value & (psize - 1u); }
				MICRO_ALWAYS_INLINE bool is_big(uintptr_t psize) const noexcept { return count(psize) == (psize - 1u); }

				static MICRO_ALWAYS_INLINE Key from_page(void* p, uintptr_t count) noexcept { return Key{ reinterpret_cast<uintptr_t>(p) | count }; }
				static MICRO_ALWAYS_INLINE Key from_big_page(void* p, uintptr_t psize) noexcept { return Key{ reinterpret_cast<uintptr_t>(p) | (psize - 1u) }; }
				MICRO_ADD_CASTS(Key)
			};
			struct Less
			{
				PageMap* map;
				MICRO_ALWAYS_INLINE bool operator()(const Key& left, const Key& right) const noexcept { return left.page(map->page_size) < right.page(map->page_size); }
			};

			// Find the key that contains given PageRunHeader
			Key* find_internal(PageRunHeader* p) noexcept
			{
				if (!count)
					return nullptr;

				Key* key = nullptr;
				Key k = Key::from_page(p, 1);
				auto* it = std::lower_bound(begin(), end(), k, Less{ this });
				if (it == begin()) {
					// check if start of it is equal to p
					key = it->page(page_size) == p ? it : nullptr;
				}
				else if (it == end()) {
					// check if p is in the range [start,start + count),
					// and that p is located at a multiple of MICRO_BLOCK_SIZE
					--it;
					PageRunHeader* start = it->page(page_size);
					if (!it->is_big(page_size)) {
						auto pcount = it->count(page_size);
						char* pend = start->as_char() + MICRO_BLOCK_SIZE * pcount;
						if (p->as_char() < pend)
							if (static_cast<uintptr_t>(p->as_char() - start->as_char()) % MICRO_BLOCK_SIZE == 0)
								key = it;
					}
				}
				else {
					if (it->page(page_size) == p)
						// Exact match
						key = it;
					else {
						// Check if p belongs to previous entry
						--it;
						PageRunHeader* start = it->page(page_size);
						auto pcount = it->count(page_size);
						if (!it->is_big(page_size) && p->as_char() < (start->as_char() + MICRO_BLOCK_SIZE * pcount) &&
						    static_cast<uintptr_t>(p->as_char() - start->as_char()) % MICRO_BLOCK_SIZE == 0)
							key = it;
					}
				}

				return key;
			}

			bool grow() noexcept
			{
				// move to new area
				uintptr_t _new_cap = (capacity ? capacity * 2u : 64u);
				Key* _new = Key::from(mgr->allocate_and_forget(static_cast<unsigned>(_new_cap * sizeof(Key))));
				if (!_new)
					return false;
				if (block) {
					memcpy(_new, block, count * sizeof(Key));
				}
				block = _new;
				capacity = _new_cap;
				return true;
			}

			Key* begin() noexcept { return Key::from(block); }
			Key* end() noexcept { return begin() + count; }

			using lock_type = shared_spinlock;

			lock_type lock;		  // shared lock
			uintptr_t page_size{ 0 }; // page size (usually os_allocation_granularity())
			uintptr_t count{ 0 };	  // number of entries
			uintptr_t capacity{ 0 };
			Key* block{ nullptr };		   // current block where the map is stored
			BaseMemoryManager* mgr{ nullptr }; // parent BaseMemoryManager

#ifdef DEBUG_PAGE_MAP
			std::set<PageRunHeader*> set;
#endif

		public:
			PageMap(BaseMemoryManager* m) noexcept
			  : page_size(m->page_provider()->allocation_granularity())
			  , mgr(m)
			{
			}

			void reset() noexcept
			{
				// Reset the page map.
				// No need to deallocate pages as this is done by the parent BaseMemoryManager
				lock.lock();
				count = 0;
				capacity = 0;
				block = nullptr;

#ifdef DEBUG_PAGE_MAP
				set.clear();
#endif

				lock.unlock();
			}

			PageRunHeader* first() noexcept
			{
				if (!count)
					return nullptr;
				return block[0].page(page_size);
			}

			// Maximum number of consecutive PageRunHeader of size MICRO_BLOCK_SIZE.
			// The value (page_size-1) is used to tell if an entry corresponds to a big PageRunHeader or not.
			uintptr_t max_page_count() const noexcept { return page_size - 2; }

			bool insert(PageRunHeader* p, bool big) noexcept
			{

				std::lock_guard<lock_type> ll(lock);

				bool found = find_internal(p) != nullptr;
#ifdef DEBUG_PAGE_MAP
				MICRO_ASSERT_DEBUG((set.find(p) != set.end()) == found, "");
#endif
				if (found)
					return true;

#ifdef DEBUG_PAGE_MAP
				set.insert(p);
#endif

				if (capacity == 0 && !grow())
					return false;

				// Create key
				Key k = big ? Key::from_big_page(p, page_size) : Key::from_page(p, 1);
				auto* it = std::lower_bound(begin(), end(), k, Less{ this });

				if (!big && count) {
					// try to merge
					if (it == end()) {
						auto* prev = it - 1;
						if (!prev->is_big(page_size) && prev->page(page_size)->as_char() + prev->count(page_size) * MICRO_BLOCK_SIZE == p->as_char()) {
							if (prev->count(page_size) != max_page_count()) {
								*prev = Key::from_page(prev->page(page_size), prev->count(page_size) + 1);
								return true;
							}
						}
					}
					else {
						MICRO_ASSERT_DEBUG(p < it->page(page_size), "");
						if (!it->is_big(page_size) && p->as_char() + p->run_size() == it->page(page_size)->as_char() && it->count(page_size) != max_page_count()) {
							*it = Key::from_page(p, it->count(page_size) + 1);
							return true;
						}
						else if (it != begin()) {
							auto* prev = it - 1;
							if (!prev->is_big(page_size) && prev->page(page_size)->as_char() + prev->count(page_size) * MICRO_BLOCK_SIZE == p->as_char()) {
								if (prev->count(page_size) != max_page_count()) {
									*prev = Key::from_page(prev->page(page_size), prev->count(page_size) + 1);
									return true;
								}
							}
						}
					}
				}

				uintptr_t diff = static_cast<uintptr_t>(end() - it);

				if (count == capacity) {
					if (!grow())
						return false;
					it = end() - diff;
				}

				// insert
				if (diff)
					memmove(it + 1, it, diff * sizeof(Key));

				if (big)
					*it = Key::from_big_page(p, page_size);
				else
					*it = Key::from_page(p, 1);
				++count;

				return true;
			}

			void erase(PageRunHeader* p) noexcept
			{
				MICRO_ASSERT_DEBUG(p->address() % page_size == 0, "");

				std::lock_guard<lock_type> ll(lock);

				Key* k = find_internal(p);
				if (!k) {
					// This page run does not belong to the map
#ifdef DEBUG_PAGE_MAP
					MICRO_ASSERT_DEBUG(set.find(p) == set.end(), "");
#endif
					return;
				}

#ifdef DEBUG_PAGE_MAP
				MICRO_ASSERT_DEBUG(set.erase(p) == 1, "");
#endif

				if (k->is_big(page_size) || k->count(page_size) == 1) {
					// erase entry
					size_t diff = static_cast<size_t>(end() - k - 1);
					if (diff)
						memmove(k, k + 1, diff * sizeof(Key));
					--count;
				}
				else {
					PageRunHeader* start = k->page(page_size);
					auto pcount = k->count(page_size);
					if (p == start) {
						start = PageRunHeader::from(p->as_char() + MICRO_BLOCK_SIZE);
						*k = Key::from_page(start, pcount - 1);
					}
					else if (p->as_char() == start->as_char() + (pcount - 1) * MICRO_BLOCK_SIZE)
						*k = Key::from_page(start, pcount - 1);
					else {
						// split entry
						uintptr_t diff_bytes = static_cast<uintptr_t>(p->as_char() - start->as_char());
						MICRO_ASSERT_DEBUG(diff_bytes % MICRO_BLOCK_SIZE == 0, "");
						auto left_count = diff_bytes / MICRO_BLOCK_SIZE;
						auto right_count = pcount - left_count - 1;

						if (count == capacity) {
							size_t idx = static_cast<size_t>(k - begin());
							// move to new page run
							Key* _new = Key::from(mgr->allocate_and_forget(static_cast<unsigned>(capacity * 2u * sizeof(Key))));
							if (!_new)
								return; // TODO: error handling
							memcpy(_new, block, static_cast<size_t>(count * sizeof(uintptr_t)));
							block = _new;
							capacity *= 2u;
							k = begin() + idx;
						}

						// move toward the right
						size_t diff = static_cast<size_t>(end() - k - 1);
						if (diff)
							memmove(k + 2, k + 1, diff * sizeof(Key));

						*k = Key::from_page(start, left_count);
						*(k + 1) = Key::from_page(p->as_char() + MICRO_BLOCK_SIZE, right_count);
						++count;
					}
				}

#ifdef DEBUG_PAGE_MAP
				MICRO_ASSERT_DEBUG(set.find(p) == set.end(), "");
#endif
				MICRO_ASSERT_DEBUG(find_internal(p) == nullptr, "");
			}

			Key* find(PageRunHeader* p) noexcept
			{
				lock.lock_shared();
				Key* res = find_internal(p);
#ifdef DEBUG_PAGE_MAP
				auto it = set.find(p);
				bool found_in_map = res != nullptr;
				bool found_in_hash_table = it != set.end();
				MICRO_ASSERT_DEBUG(found_in_map == found_in_hash_table, "");
#endif
				lock.unlock_shared();
				return res;
			}

			bool own(void* p) noexcept 
			{ 
				lock.lock_shared();
				for (auto* k = begin(); k != end(); ++k) {
					bool is_big = k->is_big(page_size);
					PageRunHeader* r;
					size_t bytes;
					if (is_big) {
						r = k->page(page_size);
						bytes = r->size_bytes;
					}
					else {
						r = k->page(page_size);
						bytes = k->count(page_size) * MICRO_BLOCK_SIZE;
					}
					if (p >= r && p < (r->as_char() + bytes)) {
						lock.unlock_shared();
						return true;
					}
				}
				lock.unlock_shared();
				return false;
			}
		};

	}
}

MICRO_POP_DISABLE_OLD_STYLE_CAST

#endif
