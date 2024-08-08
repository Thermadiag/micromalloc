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

#ifndef MICRO_TINY_MEM_POOL_HPP
#define MICRO_TINY_MEM_POOL_HPP

#include "headers.hpp"

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

#undef min
#undef max

#define MEM_POOL_INLINE MICRO_ALWAYS_INLINE

#ifdef MICRO_ZERO_MEMORY
#define MICRO_RESET_MEM_TINY(p, size) reset_mem_tiny(p, size)
#else
#define MICRO_RESET_MEM_TINY(p, size)
#endif

namespace micro
{

	namespace detail
	{
		static MICRO_ALWAYS_INLINE void reset_mem_tiny(void* p, size_t len) noexcept { memset(p, 0, len); }

		/// @brief Small allocation parameters
		class MICRO_EXPORT_CLASS SmallAllocation
		{
		public:
			// Size in bytes to size class index
			static MEM_POOL_INLINE auto size_to_idx(unsigned size) noexcept -> unsigned
			{
				// Note: size CANNOT be 0
				return (size - 1u) >> 4u;
			}
			// Size class index to size in bytes
			static auto idx_to_size(unsigned idx) noexcept -> unsigned { return ((idx + 1u) << 4u); }
			static_assert(MICRO_MAX_SMALL_ALLOC_THRESHOLD % MICRO_MINIMUM_ALIGNMENT == 0, "invalid MICRO_MAX_SMALL_ALLOC_THRESHOLD value");
			static constexpr unsigned class_count = MICRO_MAX_SMALL_ALLOC_THRESHOLD / MICRO_MINIMUM_ALIGNMENT;
		};

		// Forward declaration
		struct TinyMemPool;

		struct alignas(std::uint64_t) SmallBlockHeader
		{
#if MICRO_ALIGNED_POOL == 8192
			// Currently unused, but keep it for future optimizations
			using TailType = std::uint64_t;
			std::uint64_t guard : 16;	     // Block guard, only used to detect buffer overrun as well as address validity
			std::uint64_t pool_idx_plus_one : 6; // Size class index
			std::uint64_t offset_bytes : 8;	     // Offset in MICRO_ALIGNED_POOL bytes to the PageRunHeader start
			std::uint64_t status : 7;	     // Block status, one of MICRO_ALLOC_SMALL, MICRO_ALLOC_SMALL_BLOCK, MICRO_ALLOC_MEDIUM, MICRO_ALLOC_BIG, MICRO_ALLOC_FREE
			std::uint64_t tail : 9;		     // tail position for bump allocation
			std::uint64_t first_free : 9;	     // first free slot
			std::uint64_t objects : 9;	     // number of currently allocated objects
#else
			using TailType = std::uint8_t;
#if MICRO_MEMORY_LEVEL == 4
			// For MICRO_MEMORY_LEVEL == 4, offset_bytes is not big enough, we need one more bit
			std::uint16_t guard;		     // Block guard, only used to detect buffer overrun as well as address validity
			std::uint16_t pool_idx_plus_one : 7; // Size class index
			std::uint16_t offset_bytes : 9;	     // Offset in MICRO_ALIGNED_POOL bytes to the PageRunHeader start
			std::uint8_t status;		     // Block status, one of MICRO_ALLOC_SMALL, MICRO_ALLOC_SMALL_BLOCK, MICRO_ALLOC_MEDIUM, MICRO_ALLOC_BIG, MICRO_ALLOC_FREE
			std::uint8_t tail;		     // tail position for bump allocation
			std::uint8_t first_free;	     // first free slot
			std::uint8_t objects;		     // number of currently allocated objects
#else
			std::uint16_t guard;
			std::uint8_t pool_idx_plus_one;
			std::uint8_t offset_bytes;
			std::uint8_t status;
			std::uint8_t tail;
			std::uint8_t first_free;
			std::uint8_t objects;
#endif

#endif

			MICRO_ALWAYS_INLINE PageRunHeader* parent() noexcept { return PageRunHeader::from(reinterpret_cast<char*>(this) - offset_bytes * MICRO_ALIGNED_POOL); }
		};

		template<class Derived>
		struct MICRO_EXPORT_HEADER TinyBlockPoolIt
		{
			SmallBlockHeader header;
			Derived* left{ nullptr };  // Linked list of TinyBlockPool
			Derived* right{ nullptr }; // Linked list of TinyBlockPool

			Derived* derived() noexcept { return static_cast<Derived*>(this); }

			TinyBlockPoolIt() noexcept
			{
				left = right = derived();
				header.first_free = 0;
			}
			TinyBlockPoolIt(bool) noexcept { header.first_free = 0; }

			MEM_POOL_INLINE unsigned get_chunk_size() noexcept { return (MediumChunkHeader::from(this) - 1)->elems; }
			MEM_POOL_INLINE unsigned get_chunk_size_minus_object() noexcept { return get_chunk_size() - get_pool_size(); }
			MEM_POOL_INLINE unsigned get_chunk_size_minus_2_objects() noexcept { return get_chunk_size() - get_pool_size() * 2u; }
			MEM_POOL_INLINE unsigned get_pool_size() noexcept { return static_cast<unsigned>(header.pool_idx_plus_one); }

			// Support for linked list of TinyBlockPool
			void insert(Derived* l, Derived* r) noexcept
			{
				this->left = l;
				this->right = r;
				l->right = r->left = static_cast<Derived*>(this);
			}
			void remove() noexcept
			{
				MICRO_ASSERT_DEBUG(this->left != nullptr, "");
				this->left->right = this->right;
				this->right->left = this->left;
				this->left = this->right = nullptr;
			}

			bool end() const noexcept { return right == this; }
		};

		/// Contiguous block of memory used to allocate chunks for a specific size class.
		///
		/// TinyBlockPool uses a singly linked list of free slots combined with a bump pointer.
		///
		/// TinyBlockPool is always aligned on MICRO_ALIGNED_POOL (usually 4096) to remove
		/// the need for an allocation header and reduce the memory footprint of small allocations.
		///
		struct MICRO_EXPORT_HEADER alignas(MICRO_MINIMUM_ALIGNMENT) TinyBlockPool : public TinyBlockPoolIt<TinyBlockPool>
		{
			using ParentType = TinyMemPool;
			using Bytes16 = std::pair<uint64_t, uint64_t>;
			using TailType = SmallBlockHeader::TailType;

#if MICRO_ALIGNED_POOL == 8192
			static constexpr unsigned max_objects = 511;
#else
			static constexpr unsigned max_objects = 255;
#endif

			ParentType* parent{ nullptr }; // Parent TinyMemPool

			// Default ctor
			TinyBlockPool() noexcept
			  : TinyBlockPoolIt<TinyBlockPool>()
			{
				static_assert(sizeof(SmallBlockHeader) == 8, "");
			}

			// Construct from parent pool, chunk size in bytes, object size in bytes, class size index, parent PageRunHeader
			TinyBlockPool(ParentType* p, unsigned idx, PageRunHeader* run) noexcept
			  : TinyBlockPoolIt<TinyBlockPool>(false)
			  , parent(p)
			{
				static_assert(sizeof(SmallBlockHeader) == 8, "");

				this->header.tail = sizeof(TinyBlockPool) / 16u;
				MICRO_ASSERT_DEBUG(idx + 1u < 127u, "");
				this->header.pool_idx_plus_one = (static_cast<std::uint8_t>(idx)) + 1;

				header.guard = MICRO_BLOCK_GUARD;
				//MICRO_ASSERT_DEBUG((unsigned)((char*)this - (char*)run) / MICRO_ALIGNED_POOL < 512, "");
				header.offset_bytes = static_cast<decltype(header.offset_bytes)>((as_char() - run->as_char()) / MICRO_ALIGNED_POOL); // 16u;
				header.status = MICRO_ALLOC_SMALL_BLOCK;

				header.first_free = (header.tail);
				header.objects = 0;
			}

			MICRO_DELETE_COPY(TinyBlockPool)
			MICRO_ADD_CASTS(TinyBlockPool)

			// Allocate one object
			MEM_POOL_INLINE auto allocate() noexcept -> void*
			{
				// first_free is 0 when the pool is full
				if (MICRO_UNLIKELY(this->header.first_free == 0))
					return nullptr;

				MICRO_ASSERT_DEBUG(this->header.first_free < get_chunk_size(), "");
				TailType* res = reinterpret_cast<TailType*>(reinterpret_cast<Bytes16*>(this) + this->header.first_free);
				// Tail case: use address bump
				if ( this->header.first_free == this->header.tail) {
					unsigned new_tail = static_cast<unsigned>(this->header.tail + get_pool_size());
					// Set next address to 0 if full, new tail if not
					new_tail *= (new_tail <= get_chunk_size_minus_object());
					MICRO_ASSERT_DEBUG(new_tail <= max_objects, "");
					header.tail = *res = static_cast<TailType>(new_tail);
				}
				this->header.first_free = *res;
				MICRO_ASSERT_DEBUG(this->header.objects < max_objects, "");
				++this->header.objects;

				MICRO_RESET_MEM_TINY(res, (this->get_pool_size() << 4u));
				return res;
			}

			// Deallocate object
			MEM_POOL_INLINE bool deallocate(void* p, spinlock& ll) noexcept
			{
				TailType* b = static_cast<TailType*>(p);
				TailType diff = static_cast<TailType>(reinterpret_cast<Bytes16*>(p) - reinterpret_cast<Bytes16*>(this));

				// Lock the parent spinlock for this size class
				ll.lock();

				MICRO_ASSERT_DEBUG(this->header.first_free < get_chunk_size() && (header.first_free == 0 || header.first_free >= sizeof(TinyBlockPool) / 16), "");
				MICRO_ASSERT_DEBUG(diff >= sizeof(TinyBlockPool) / 16 && diff < get_chunk_size(), "");

				*b = static_cast<TailType>(header.first_free);
				header.first_free = diff;
				return --header.objects == 0;
			}

			MEM_POOL_INLINE bool empty() const noexcept { return header.objects == 0; }
			MEM_POOL_INLINE bool is_inside(void* p) noexcept { return p > this && p < as_char() + (get_chunk_size() << 4u); }
			MEM_POOL_INLINE PageRunHeader* get_parent_run() noexcept { return header.parent(); }
			MEM_POOL_INLINE ParentType* get_parent() noexcept { return parent; }
		};

		/// @brief Parallel small object pool
		///
		/// TinyMemPool provides a (relative) level of concurrency, as it only locks operations per size class.
		/// Therefore, allocating objects of different size class can be done in parallel.
		///
		/// TinyMemPool allocation and deallocation processes have a O(1) complexity, modulo lock contentions.
		///
		struct TinyMemPool
		{
			using this_type = TinyMemPool;
			using block = TinyBlockPool;
			using block_it = TinyBlockPoolIt<block>;

			/// @brief Add a new block for given size class index
			auto add(unsigned size, unsigned idx, void** direct) noexcept -> block*
			{
				unsigned objects = static_cast<unsigned>((MICRO_ALIGNED_POOL - 16 - sizeof(block)) / size);
				unsigned to_alloc = static_cast<unsigned>(sizeof(block) + objects * size);
				unsigned request_obj_size = 0;
				if (d_mgr->params().allow_small_alloc_from_radix_tree)
					request_obj_size = size;

				// Allocate size bytes if possible. If size bytes would require a new page allocation,
				// Try first to retrieve at least min_bytes bytes in order to exhaust the readix tree.
				bool is_small = false;
				block* res = block::from(d_mgr->allocate_no_tiny_pool(to_alloc, request_obj_size, MICRO_ALIGNED_POOL, &is_small));
				if (!res)
					return nullptr;

				// Object was directly allocated from the radix tree
				if (is_small) {
					*direct = res;
					return nullptr;
				}

				MediumChunkHeader* h = (MediumChunkHeader::from(res) - 1);
#if MICRO_USE_FIRST_ALIGNED_CHUNK
				// Set page run header status if this is the first chunk
				if (h->offset_prev == 0)
					h->parent()->header.status = MICRO_ALLOC_SMALL_BLOCK;
#endif
				return new (res) block(this, idx, h->parent());
			}

			/// @brief Allocate from a newly created block
			MICRO_NOINLINE(auto) allocate_from_new_block(unsigned size, unsigned idx) noexcept -> void*
			{

				// Unlock size class spinlock before creating the new block
				// to avoid blocking potential deallocations
				d_data[idx].lock.unlock();

				// Create the block
				void* direct = nullptr;
				block* _bl = add(size, idx, &direct);
				if (!_bl) {
					d_data[idx].lock.lock();
					return direct;
				}
#if MICRO_TINY_POOL_CACHE
				d_pool_count.fetch_add(1, std::memory_order_relaxed);
#endif

				// Lock again, link block
				d_data[idx].lock.lock();

				_bl->insert(static_cast<block*>(&d_data[idx].it), d_data[idx].it.right);

				// Set pool bit
				_bl->get_parent_run()->set_pool(_bl);

				MICRO_ASSERT_DEBUG(d_data[idx].it.right != nullptr, "");
				void* r = _bl->allocate();
				MICRO_ASSERT_DEBUG(reinterpret_cast<uintptr_t>(r) % MICRO_MINIMUM_ALIGNMENT == 0, "");
				return r;
			}

			/// @brief Handle complex deallocation
			static MICRO_NOINLINE(void) handle_deallocate(this_type* parent, block* p, unsigned idx) noexcept
			{
				if (p->empty() && parent->d_pool_count.load(std::memory_order_relaxed) >= MICRO_TINY_POOL_CACHE) {

					// Unset pool bit
					p->get_parent_run()->unset_pool(p);

					// Empty block: remove it from the linked list and deallocate from the radix tree
					p->remove();

					MICRO_ASSERT_DEBUG(parent->d_data[idx].it.right != nullptr, "");
					parent->d_data[idx].lock.unlock();

#if MICRO_TINY_POOL_CACHE
					parent->d_pool_count.fetch_sub(1, std::memory_order_relaxed);
#endif

#if MICRO_USE_FIRST_ALIGNED_CHUNK
					// Reset page run header status if this is the first chunk
					MediumChunkHeader* h = (MediumChunkHeader::from(p) - 1);
					if (h->offset_prev == 0)
						h->parent()->header.status = 0;
#endif

					// Invalidate the header.
					// This greatly helps deallocations and false positive in tiny blocks detection.
					memset(static_cast<void*>(p), 0, sizeof(block));

					// Deallocate block
					parent->d_mgr->deallocate_no_tiny_pool(p);

					return;
				}

				// If the block was removed from the linked list, add it back as it now provides free slot(s)
				if (!p->left) {
					p->insert(static_cast<block*>(&p->get_parent()->d_data[idx].it), p->get_parent()->d_data[idx].it.right);
				}
				parent->d_data[idx].lock.unlock();
			}

			MICRO_NOINLINE(void*) allocate_from_pool_list(unsigned idx) noexcept
			{
				block* bl = d_data[idx].it.right;
				if (bl != &d_data[idx].it) {
					bl->remove();
					bl = d_data[idx].it.right;
				}
				MICRO_ASSERT_DEBUG(bl != nullptr, "");
				while ((bl != &d_data[idx].it)) {
					void* res = bl->allocate();
					if (MICRO_LIKELY(res)) {
						MICRO_ASSERT_DEBUG(reinterpret_cast<uintptr_t>(res) % MICRO_MINIMUM_ALIGNMENT == 0, "");
						return res;
					}
					else {
						auto* next = bl->right;
						bl->remove();
						bl = next;
						MICRO_ASSERT_DEBUG(bl != nullptr, "");
					}
				}
				return nullptr;
			}
			

			BaseMemoryManager* d_mgr;

			struct It
			{
				block_it it;
				spinlock lock;
			};
			It d_data[SmallAllocation::class_count];
			std::atomic<size_t> d_pool_count{ 0 };

		public:
			using block_type = block;

			/// @brief Default constructor
			TinyMemPool(BaseMemoryManager* mgr) noexcept
			  : d_mgr(mgr)
			{
			}

			MICRO_DELETE_COPY(TinyMemPool)

			/// @brief Allocate object of given size
			/// @param size size in bytes
			/// @param force if true and no free slot available, allocate from a new block
			MEM_POOL_INLINE void* allocate(unsigned size, bool force) noexcept
			{
				// Note: size CANNOT be 0

				// Compute class index
				unsigned idx = SmallAllocation::size_to_idx(size);
				MICRO_ASSERT_DEBUG(idx < SmallAllocation::class_count, "");

				std::lock_guard<spinlock> ll(d_data[idx].lock);

				void* res = d_data[idx].it.right->allocate();
				if (MICRO_LIKELY(res))
					return res;
				if((res = allocate_from_pool_list(idx)))
					return res;
				if (force)
					return allocate_from_new_block(SmallAllocation::idx_to_size(idx), idx);
				return nullptr;
			}

			/// @brief Deallocate object from given block
			static MEM_POOL_INLINE void deallocate(void* ptr, block* p) noexcept
			{
				const auto idx = p->header.pool_idx_plus_one - 1u;
				const auto* left = p->left;
				auto* parent = p->get_parent();
				MICRO_ASSERT_DEBUG(idx < SmallAllocation::class_count, "");
				if (MICRO_UNLIKELY(p->deallocate(ptr, parent->d_data[idx].lock) || !left))
					return handle_deallocate(parent, p, static_cast<unsigned>(idx));
				parent->d_data[idx].lock.unlock();
			}
		};
	} // end namespace detail

} // end namespace micro

MICRO_POP_DISABLE_OLD_STYLE_CAST

#endif
