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

#ifndef MICRO_PAGE_PROVIDER_HPP
#define MICRO_PAGE_PROVIDER_HPP

#ifdef _MSC_VER
// Remove useless warnings ...needs to have dll-interface to be used by clients of class...
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#include "../os_page.hpp"
#ifndef MICRO_NO_FILE_MAPPING
#include "../os_map_file.hpp"
#endif
#include "../lock.hpp"
extern "C" {
#include "../enums.h"
}
#include "../parameters.hpp"
#include <set>

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

namespace micro
{
	/// @brief Base class for thread safe page allocation/deallocation
	class MICRO_EXPORT_CLASS BasePageProvider
	{
		const parameters* d_params;

	public:
		MICRO_DELETE_COPY(BasePageProvider)

		BasePageProvider(const parameters& params) noexcept
		  : d_params(&params)
		{
		}
		virtual ~BasePageProvider() {}
		virtual void* allocate_pages(size_t pcount) noexcept = 0;
		virtual bool deallocate_pages(void* p, size_t pcount) noexcept = 0;
		virtual size_t page_size() const noexcept = 0;
		virtual size_t page_size_bits() const noexcept = 0;
		virtual size_t allocation_granularity() const noexcept { return this->page_size(); }

		/// @brief Tells if this providers owns the pages, i.e. pages need
		/// to be deallocated when parent BaseMemoryManager is destroyed.
		virtual bool own_pages() const noexcept = 0;

		virtual bool is_valid() const noexcept = 0;

		/// @brief Reset page provider in an empty valid state, ready to provide new pages.
		/// Called in BaseMemoryManager::clear()
		virtual void reset() noexcept {}

		const parameters& params() const noexcept { return *d_params; }
		bool log_enabled(micro_log_level l) const noexcept { return d_params->log_level >= static_cast<unsigned>(l); }
	};

	/// @brief Page provider using OS page allocation/deallocation mechanisms
	class MICRO_EXPORT_CLASS OsPageProvider : public BasePageProvider
	{
	public:
		OsPageProvider(const parameters& params) noexcept
		  : BasePageProvider(params)
		{
		}
		virtual void* allocate_pages(size_t pcount) noexcept override { return os_allocate_pages(pcount); }
		virtual bool deallocate_pages(void* p, size_t pcount) noexcept override { return os_free_pages(p, pcount); }
		virtual size_t page_size() const noexcept override { return os_page_size(); }
		virtual size_t allocation_granularity() const noexcept override { return os_allocation_granularity(); }
		virtual size_t page_size_bits() const noexcept override
		{
			static size_t size_bits = bit_scan_reverse_64(os_page_size());
			return size_bits;
		}
		virtual bool own_pages() const noexcept override { return true; }
		virtual bool is_valid() const noexcept override { return true; }
	};

	/// @brief Page provider using a user provided buffer to allocate/deallocate pages.
	/// The size of one page is given in the constructor.
	///
	/// MemoryPageProvider internally uses 2 sorted sets of free pages: one sorted by page address
	/// and another sorted by page count.
	///
	/// When allocating pages, MemoryPageProvider first look in the set of free pages for the lowest possible
	/// a page run that fits required page count. If found, this page run is removed from the set(s), and
	/// possible remaining pages are added back to the set(s).
	///
	/// If no big enough free page run is found, the page head pointer (pointing to the very end of provided buffer)
	/// is shifted left if possible by the required amount of pages. Thus, pages are carved from the right side
	/// of provided buffer.
	///
	/// The internal sets are standard std::set and std::multiset objects. They both use a custom allocator that
	/// provides memory chunks from the beginning of the buffer. Therefore, page allocation becomes impossible
	/// when the tail pointer of sets allocators meet the head pointer of carved pages.
	///
	class MICRO_EXPORT_CLASS MemoryPageProvider : public BasePageProvider
	{
		template<class T>
		friend struct SetAllocator;

		// Let's use std::uintptr_t to make sure that PageEntry has the size of 2 pointers
		using size_type = std::uintptr_t;

		// Page description (address and size in bytes)
		struct PageEntry
		{
			char* page{ nullptr };
			size_type size{ 0 }; // size of the page span in bytes

			// forward list of PageEntry, used by custom std::set allocator
			PageEntry* next() noexcept { return reinterpret_cast<PageEntry*>(page); }
			void set_next(PageEntry* n) noexcept { page = reinterpret_cast<char*>(n); }
		};

		// Sort PageEntry by size
		struct LessSize
		{
			bool operator()(const PageEntry& l, const PageEntry& r) const noexcept { return l.size < r.size; }
		};

		// Sort PageEntry by address
		struct LessAddr
		{
			bool operator()(const PageEntry& l, const PageEntry& r) const noexcept { return l.page < r.page; }
		};

		// Custom allocator for std::set and std::multiset
		template<class T>
		struct SetAllocator : std::allocator<T>
		{
			using is_always_equal = std::false_type;
			using propagate_on_container_swap = std::true_type;
			using propagate_on_container_copy_assignment = std::true_type;
			using propagate_on_container_move_assignment = std::true_type;
			template<class Other>
			struct rebind
			{
				using other = SetAllocator<Other>;
			};

			MemoryPageProvider* mgr{ nullptr };
			SetAllocator(MemoryPageProvider* d) noexcept
			  : mgr(d)
			{
			}
			SetAllocator(const SetAllocator& other) noexcept
			  : mgr(other.mgr)
			{
			}
			template<class Other>
			SetAllocator(const SetAllocator<Other>& other) noexcept
			  : mgr(other.mgr)
			{
			}
			SetAllocator& operator=(const SetAllocator& other) noexcept
			{
				mgr = other.mgr;
				return *this;
			}
			bool operator==(const SetAllocator& other) const noexcept { return mgr == other.mgr; }
			bool operator!=(const SetAllocator& other) const noexcept { return mgr != other.mgr; }

			void deallocate(T* p, const size_t count) noexcept
			{
				// add to free list
				PageEntry* entry = reinterpret_cast<PageEntry*>(p);
				entry->size = sizeof(T) * count;
				entry->set_next(mgr->first_free);
				mgr->first_free = entry;
			}

			T* allocate(const size_t count)
			{
				size_t bytes = count * sizeof(T);
				// first, look in free entries
				PageEntry* prev = nullptr;
				PageEntry* cur = mgr->first_free;
				while (cur) {
					if (cur->size >= bytes) {
						// remove and return
						if (!prev)
							mgr->first_free = cur->next();
						else
							prev->set_next(cur->next());
						return reinterpret_cast<T*>(cur);
					}
					prev = cur;
					cur = cur->next();
				}
				// use tail
				if (mgr->set_tail + bytes > mgr->page_head)
					throw std::bad_alloc();
				T* p = reinterpret_cast<T*>(mgr->set_tail);
				mgr->set_tail += bytes;
				return p;
			}
			T* allocate(const size_t count, const void*) { return allocate(count); }
			size_t max_size() const noexcept { return static_cast<size_t>(-1) / sizeof(T); }
		};

		// Set proxy, used to avoid calling default constructor and destructor of std::set/std::multiset
		template<class Set>
		struct alignas(Set) SetProxy
		{
			char data[sizeof(Set)];
			Set* set() noexcept { return reinterpret_cast<Set*>(data); }
			const Set* set() const noexcept { return reinterpret_cast<const Set*>(data); }
		};

		using lock_type = spinlock;
		using allocator_type = SetAllocator<PageEntry>;
		using set_size_type = std::multiset<PageEntry, LessSize, allocator_type>;
		using set_addr_type = std::set<PageEntry, LessAddr, allocator_type>;

		// global lock
		lock_type lock;
		// allow using os_allocate_pages
		bool grow;
		// user provided buffer
		char* buffer{ nullptr };
		// user provided buffer size
		size_type buffer_size{ 0 };
		// page head position, used to carve new pages. Points to the end of the buffer.
		char* page_head{ nullptr };
		// sets allocator tail position, points to the beginning of the buffer.
		char* set_tail{ nullptr };

		// First free memory block in order to recycle memory blocks deallocated by std::set/multiset
		PageEntry* first_free{ nullptr };

		// Set of PageEntry sorted by size
		SetProxy<set_size_type> by_size;
		// Set of PageEntry sorted by address
		SetProxy<set_addr_type> by_addr;

		unsigned p_size{ 4096 };
		unsigned p_size_bits{ 12 };
		std::atomic<size_type> page_count{ 0 };

		// Erase a PageEntry from by_size and by_addr
		template<class It1, class It2>
		void erase_entry(PageEntry e, It1 addr_it, It2 size_it)
		{
			// Take PageEntry by value as it will be invalidated

			// Erase entry from address map
			if (addr_it != by_addr.set()->end())
				by_addr.set()->erase(addr_it);
			else
				by_addr.set()->erase(e);

			// Erase entry from size map
			if (size_it != by_size.set()->end())
				by_size.set()->erase(size_it);
			else {
				auto range = by_size.set()->equal_range(e);
				for (auto it = range.first; it != range.second; ++it) {
					if (it->page == e.page) {
						by_size.set()->erase(it);
						return;
					}
				}
			}
		}
		// Insert a PageEntry into by_size and by_addr
		void insert_entry(const PageEntry& e);

	public:
		MemoryPageProvider(const parameters& params, unsigned psize, bool allow_grow) noexcept;
		MemoryPageProvider(const parameters& params, unsigned psize, bool allow_grow, char* b, size_type size) noexcept
		  : MemoryPageProvider(params, psize, allow_grow)
		{
			init(b, size);
		}
		/// @brief Init the MemoryPageProvider from an address and a size in bytes.
		/// This function can be called multiple times if needed, but this will invalidate previously allocated pages.
		///
		void init(char* b, size_type size) noexcept;

		bool own(void* ptr) const noexcept;
		bool empty() const noexcept;
		size_t max_pages() const noexcept;
		size_t allocated_pages() const noexcept { return page_count; }

		/// @brief Allocate and return pcount pages of size page_size()
		virtual void* allocate_pages(size_t pcount) noexcept override;

		/// @brief Deallocate pcount pages starting at address p
		virtual bool deallocate_pages(void* p, size_t pcount) noexcept override;

		virtual size_t page_size() const noexcept override;
		virtual size_t page_size_bits() const noexcept override;
		virtual bool own_pages() const noexcept override;
		virtual void reset() noexcept override;
		virtual bool is_valid() const noexcept override { return buffer != nullptr; }
	};

#ifndef MICRO_NO_FILE_MAPPING
	/// @brief BasePageProvider allocating pages from a memory mapped file
	class MICRO_EXPORT_CLASS FilePageProvider : public BasePageProvider
	{
		// Combination of MemoryPageProvider and forward linked list
		struct MemPageProvider
		{
			memory_map_file_view view;
			MemoryPageProvider provider;
			MemPageProvider* next;
		};

		unsigned p_size;		 // page size
		unsigned p_size_bits;		 // page size bits
		double d_grow_factor;		 // growth factor, usually 2
		MemPageProvider* d_first;	 // first MemPageProvider in the forward linked list
		memory_map_file d_file;		 // memory file mapper
		std::uint64_t d_size;		 // initial file size
		std::uint64_t d_file_size;	 // current file size
		unsigned d_flags;		 // initial flags
		char d_filename[MICRO_MAX_PATH]; // filename
		char d_basename[MICRO_MAX_PATH]; // copy of params().file_page_provider
		spinlock d_lock;		 // global lock

	public:
		FilePageProvider(const parameters& params, unsigned psize, double grow_factor) noexcept;
		FilePageProvider(const parameters& params, unsigned psize, double grow_factor, const char* filename, std::uint64_t size, unsigned flags = MicroStaticSize) noexcept
		  : FilePageProvider(params, psize, grow_factor)
		{
			init(filename, size, flags);
		}
		virtual ~FilePageProvider() noexcept override;
		bool init(const char* filename, std::uint64_t size, unsigned flags = MicroStaticSize) noexcept;
		const char* current_filename() const noexcept;
		std::uint64_t current_size() const noexcept;
		unsigned current_flags() const noexcept;

		virtual void* allocate_pages(size_t pcount) noexcept override;
		virtual bool deallocate_pages(void* p, size_t pcount) noexcept override;
		virtual size_t page_size() const noexcept override { return p_size; }
		virtual size_t page_size_bits() const noexcept override { return p_size_bits; }

		/// @brief Tells if this providers owns the pages, i.e. pages need
		/// to be deallocated when parent BaseMemoryManager is destroyed.
		virtual bool own_pages() const noexcept override { return false; }

		/// @brief Reset page provider in an empty valid state, ready to provide new pages.
		/// Called in BaseMemoryManager::clear()
		virtual void reset() noexcept override { init(current_filename(), current_size(), current_flags()); }
		virtual bool is_valid() const noexcept override { return d_first != nullptr; }
	};
#endif

	/// @brief BasePageProvider that preallocates a certain amount of memory
	class MICRO_EXPORT_CLASS PreallocatePageProvider : public BasePageProvider
	{
	private:
		void* d_pages;
		size_t d_pcount;
		MemoryPageProvider d_provider;
		spinlock d_lock;

	public:
		PreallocatePageProvider(const parameters& params, size_t bytes, bool allow_grow) noexcept;
		virtual ~PreallocatePageProvider() noexcept override;

		/// @brief Allocate and return pcount pages of size page_size()
		virtual void* allocate_pages(size_t pcount) noexcept override;

		/// @brief Deallocate pcount pages starting at address p
		virtual bool deallocate_pages(void* p, size_t pcount) noexcept override;

		virtual size_t page_size() const noexcept override { return d_provider.page_size(); }
		virtual size_t page_size_bits() const noexcept override { return d_provider.page_size_bits(); }
		virtual bool own_pages() const noexcept override { return true; }
		virtual void reset() noexcept override { d_provider.reset(); }
		virtual bool is_valid() const noexcept override { return d_provider.is_valid(); }
	};

	/// @brief Generic page provider as stored in MemoryManager class
	class MICRO_EXPORT_CLASS GenericPageProvider : public BasePageProvider
	{
		static constexpr size_t sizeof_mem_provider = sizeof(PreallocatePageProvider);
		static constexpr size_t sizeof_file_provider = sizeof(FilePageProvider);
		static constexpr size_t sizeof_data = sizeof_mem_provider > sizeof_file_provider ? sizeof_mem_provider : sizeof_file_provider;

		alignas(16) char d_data[sizeof_data];
		BasePageProvider* d_provider;

	public:
		GenericPageProvider(const parameters& params) noexcept
		  : BasePageProvider(params)
		{
			d_provider = new (d_data) OsPageProvider(params);
		}

		void setOSProvider() noexcept
		{
			d_provider->~BasePageProvider();
			d_provider = new (d_data) OsPageProvider(params());
		}
		void setMemoryProvider(unsigned psize, bool grow, void* p, std::uintptr_t size) noexcept
		{
			d_provider->~BasePageProvider();
			d_provider = new (d_data) MemoryPageProvider(params(), psize, grow, static_cast<char*>(p), size);
		}
#ifndef MICRO_NO_FILE_MAPPING
		void setFileProvider(unsigned psize, double grow_factor, const char* filename, std::uint64_t size, unsigned flags = 0) noexcept
		{
			d_provider->~BasePageProvider();
			d_provider = new (d_data) FilePageProvider(params(), psize, grow_factor, filename, size, flags);
		}
#endif
		void setPreallocatedPageProvider(size_t bytes, bool grow) noexcept
		{
			d_provider->~BasePageProvider();
			d_provider = new (d_data) PreallocatePageProvider(params(), bytes, grow);
		}

		virtual ~GenericPageProvider() override { d_provider->~BasePageProvider(); }
		virtual void* allocate_pages(size_t pcount) noexcept override { return d_provider->allocate_pages(pcount); }
		virtual bool deallocate_pages(void* p, size_t pcount) noexcept override { return d_provider->deallocate_pages(p, pcount); }
		virtual size_t page_size() const noexcept override { return d_provider->page_size(); }
		virtual size_t page_size_bits() const noexcept override { return d_provider->page_size_bits(); }
		virtual size_t allocation_granularity() const noexcept override { return d_provider->allocation_granularity(); }
		virtual bool own_pages() const noexcept override { return d_provider->own_pages(); }
		virtual void reset() noexcept override { d_provider->reset(); }
		virtual bool is_valid() const noexcept override { return d_provider->is_valid(); }
	};
}

#ifdef MICRO_HEADER_ONLY
#include "page_provider.cpp"
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

MICRO_POP_DISABLE_OLD_STYLE_CAST

#endif
