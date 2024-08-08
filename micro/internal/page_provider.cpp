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

#include "page_provider.hpp"
#include "../enums.h"
#include "../logger.hpp"
#include "defines.hpp"

#include <cstdio>
#include <ctime>
#include <cstring>

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

namespace micro
{

	MICRO_EXPORT_CLASS_MEMBER MemoryPageProvider::MemoryPageProvider(const parameters& params, unsigned psize, bool allow_grow) noexcept
	  : BasePageProvider(params)
	  , grow(allow_grow)
	  , p_size(psize)
	  , p_size_bits(bit_scan_reverse_64(psize ? psize : 1))
	{
		if (!(psize > 0 && (psize & (psize - 1)) == 0 && psize <= MICRO_MAXIMUM_PAGE_SIZE && psize >= MICRO_MINIMUM_PAGE_SIZE))
			if (log_enabled(MicroCritical))
				print_safe(stderr, "CRITICAL page size must be a power of 2 in between 2048 and 65536, provided value is ", psize ,"\n");
		MICRO_ASSERT(psize > 0 && (psize & (psize - 1)) == 0 && psize <= MICRO_MAXIMUM_PAGE_SIZE && psize >= MICRO_MINIMUM_PAGE_SIZE,
			     "page size must be a power of 2 in between 2048 and 65536");
	}

	// Insert a PageEntry into by_size and by_addr
	MICRO_EXPORT_CLASS_MEMBER void MemoryPageProvider::insert_entry(const PageEntry& e)
	{
		by_addr.set()->insert(e);
		by_size.set()->insert(e);
	}

	MICRO_EXPORT_CLASS_MEMBER void MemoryPageProvider::init(char* b, size_type size) noexcept
	{
		std::unique_lock<lock_type> ll(lock);

		// align end buffer on page size
		if (b) {
			uintptr_t end = reinterpret_cast<uintptr_t>(b) + size;
			end = end & ~(static_cast<uintptr_t>(p_size) - 1u);
			char* pend = reinterpret_cast<char*>(end);
			size = 0;
			if (pend > b && (pend - b) > p_size) {
				size = static_cast<uintptr_t>(pend - b);
			}
			else
				b = nullptr;
		}

		buffer = b;
		buffer_size = size;
		page_head = buffer + buffer_size;
		set_tail = buffer;
		first_free = nullptr;
		page_count = 0;
		if (buffer) {
			// no need to cleanup the set, just create a new one
			new (by_size.set()) set_size_type(allocator_type{ this });
			new (by_addr.set()) set_addr_type(allocator_type{ this });
		}
	}

	MICRO_EXPORT_CLASS_MEMBER bool MemoryPageProvider::own(void* ptr) const noexcept
	{
		char* p = static_cast<char*>(ptr);
		return p >= buffer && p < (buffer + buffer_size);
	}

	MICRO_EXPORT_CLASS_MEMBER bool MemoryPageProvider::empty() const noexcept { return page_count == 0; }

	MICRO_EXPORT_CLASS_MEMBER size_t MemoryPageProvider::max_pages() const noexcept
	{
		size_t from_head = static_cast<size_t>(page_head - set_tail) >> page_size_bits();
		size_t from_free = 0;
		if (!by_size.set()->empty()) {
			from_free = (--by_size.set()->end())->size >> page_size_bits();
		}
		return std::max(from_head, from_free);
	}

	/// @brief Allocate and return pcount pages of size PageSize
	MICRO_EXPORT_CLASS_MEMBER void* MemoryPageProvider::allocate_pages(size_t pcount) noexcept
	{
		std::lock_guard<lock_type> ll(lock);

		if (!buffer) {
			if (grow)
				return os_allocate_pages(pcount);
			return nullptr;
		}

		try {
			size_type bytes = pcount * p_size;

			// Find in free pages
			auto it = by_size.set()->lower_bound(PageEntry{ nullptr, bytes });
			if (it == by_size.set()->end()) {
				// allocate from page head
				char* new_head = page_head - bytes;
				if (new_head < set_tail) {
					// no room left
					if (grow)
						return os_allocate_pages(pcount);
					if (log_enabled(MicroWarning))
						print_stderr(MicroWarning, this->params().log_date_format.data(), "MemoryPageProvider: cannot allocate %u pages\n", static_cast<unsigned>(pcount));
					return nullptr;
				}
				page_head = new_head;
				page_count += pcount;
				return page_head;
			}

			PageEntry e = *it;

			// remove entry
			erase_entry(e, by_addr.set()->end(), it);

			if (e.size != bytes) {
				// split
				char* new_pages = e.page + bytes;
				size_type new_size = e.size - bytes;
				// add new pages
				insert_entry(PageEntry{ new_pages, new_size });
			}

			page_count += pcount;
			return e.page;
		}
		catch (...) {
			// potential bad_alloc from std::set/multiset
			if (!grow)
				if (log_enabled(MicroWarning))
					print_stderr(MicroWarning, this->params().log_date_format.data(), "MemoryPageProvider: cannot allocate %u pages\n", static_cast<unsigned>(pcount));
		}

		if (grow)
			return os_allocate_pages(pcount);
		return nullptr;
	}

	/// @brief Deallocate pcount pages starting at address p
	MICRO_EXPORT_CLASS_MEMBER bool MemoryPageProvider::deallocate_pages(void* p, size_t pcount) noexcept
	{
		std::lock_guard<lock_type> ll(lock);

		if (!own(p)) {
			if (grow) {
				if (!os_free_pages(p, pcount)) {
					if (log_enabled(MicroWarning))
						print_stderr(MicroWarning, params().log_date_format.data(), "unable to free pages");
					return false;
				}
				return true;
			}
			return false;
		}

		MICRO_ASSERT_DEBUG(by_addr.set()->find(PageEntry{ static_cast<char*>(p), pcount * p_size }) == by_addr.set()->end(), "");

		try {
			PageEntry e{ static_cast<char*>(p), pcount * p_size };

			if (by_addr.set()->size()) {
				// find free page just after
				auto next = by_addr.set()->lower_bound(e);
				auto prev = next;
				--prev; // we are certain that prev != next since the map is not empty
				if (next != by_addr.set()->end() && e.page + e.size == next->page) {
					// free pages just after: merge
					e.size += next->size;
					erase_entry(*next, next, by_size.set()->end());
				}
				if (prev != by_addr.set()->end() && (prev->page + prev->size) == e.page) {
					// free pages before: merge
					e.size += prev->size;
					e.page = prev->page;
					erase_entry(*prev, prev, by_size.set()->end());
				}
			}
			insert_entry(e);
			page_count -= pcount;
			return true;
		}
		catch (...) {
			// potential bad_alloc from std::set/multiset
			if (log_enabled(MicroWarning))
				print_stderr(MicroWarning, this->params().log_date_format.data(), "MemoryPageProvider: cannot deallocate %u pages\n", static_cast<unsigned>(pcount));
			return false;
		}
	}

	MICRO_EXPORT_CLASS_MEMBER size_t MemoryPageProvider::page_size() const noexcept { return p_size; }

	MICRO_EXPORT_CLASS_MEMBER size_t MemoryPageProvider::page_size_bits() const noexcept { return p_size_bits; }
	MICRO_EXPORT_CLASS_MEMBER bool MemoryPageProvider::own_pages() const noexcept
	{
		// If grow is true, the MemoryManager MUST deallocate pages
		return grow;
	}

	MICRO_EXPORT_CLASS_MEMBER void MemoryPageProvider::reset() noexcept { init(buffer, buffer_size); }

#ifndef MICRO_NO_FILE_MAPPING

	MICRO_EXPORT_CLASS_MEMBER FilePageProvider::FilePageProvider(const parameters& params, unsigned psize, double grow_factor) noexcept
	  : BasePageProvider(params)
	  , p_size(psize)
	  , p_size_bits(bit_scan_reverse_64(psize ? psize : 1))
	  , d_grow_factor(grow_factor)
	  , d_first(nullptr)
	  , d_size(0)
	  , d_file_size(0)
	  , d_flags(0)
	{
		d_filename[0] = 0;
		d_basename[0] = 0;
		strcpy(d_basename, params.page_file_provider.data());
		if (!(psize > 0 && (psize & (psize - 1)) == 0 && psize <= MICRO_MAXIMUM_PAGE_SIZE && psize >= MICRO_MINIMUM_PAGE_SIZE) && (log_enabled(MicroCritical)))
			print_safe(stderr, "CRITICAL page size must be a power of 2 in between 2048 and 65536, provided value is ", psize, "\n");
		if (!(grow_factor > 0 && grow_factor <= 8) && (log_enabled(MicroCritical)))
			print_safe(stderr, "CRITICAL grow factor must be in the range (0,8]\n");
		MICRO_ASSERT(psize > 0 && (psize & (psize - 1)) == 0 && psize <= MICRO_MAXIMUM_PAGE_SIZE && psize >= MICRO_MINIMUM_PAGE_SIZE,
			     "page size must be a power of 2 in between 2048 and 65536");
		MICRO_ASSERT(grow_factor > 0 && grow_factor <= 8, "grow factor must be in the range (0,8]");
	}

	MICRO_EXPORT_CLASS_MEMBER FilePageProvider::~FilePageProvider() noexcept
	{
		if (d_filename[0]) {
			// Close all views
			auto* p = d_first;
			while (p) {
				auto* next = p->next;
				p->view = memory_map_file_view{};
				p = next;
			}
			d_file.init(nullptr, 0);
		}
		//if (d_filename[0] && (d_flags & MicroRemoveOnClose)) {
			// Remove file
		//	remove(d_filename);
		//}
	}

	inline const char* string_memrchr(const char* s, char c, size_t n) noexcept
	{
		const char* p = s;
		for (p += n; n > 0; n--) {
			if (*--p == c) {
				return (p);
			}
		}

		return nullptr;
	}

	static inline bool create_filename(BasePageProvider * p, char* fname, const char * dir, const char * prefix, unsigned try_count)
	{
		// Build directory name

		const char* tmp = dir;
		if (!dir || dir[0] == 0) {
			tmp = std::getenv("TEMP");
			if (!tmp)
				tmp = std::getenv("TMP");
			if (!tmp)
				tmp = std::getenv("TMPDIR");
			if (!tmp) {
#ifdef P_tmpdir
				tmp = (char*)P_tmpdir;
#endif
			}
		}

		if (!tmp || tmp[0] == 0)
			return false;
		size_t len = strlen(tmp);

		// '~' is allowed and should be interpreted as the user's HOME
		if (tmp[0] != '~')
			strcpy(fname, tmp);
		else {
			const char* home = nullptr;
#ifdef WIN32
			home = std::getenv("USERPROFILE");
#endif
			if (!home)
				home = std::getenv("HOME");
			if (!home) {
				if (p->log_enabled(MicroCritical))
					print_safe(stderr, "CRITICAL unable to find the HOME variable\n");
				MICRO_ASSERT(false, "CRITICAL unable to find the HOME variable");
			}
			strcpy(fname, home);
			strcpy(fname + strlen(home), tmp + 1);
			len = strlen(fname);
		}

		// Make sure the path ends with a '/'
		if (fname[len - 1] == '\\')
			fname[len - 1] = '/';
		if (fname[len - 1] != '/') {
			fname[len] = '/';
			len++;
		}

		// add prefix
		if (prefix && prefix[0] != 0) {
			size_t plen = strlen(prefix);
			if (prefix[plen - 1] != '/') {
				// does not ends with '/'
				const char* slash_pos = string_memrchr(prefix, '/', plen);
				size_t start = 0;
				if (slash_pos)
					start = static_cast<size_t>((slash_pos - prefix) + 1);
				size_t length = plen - start;
				memcpy(fname + len, prefix + start, length);
				len += length;
			}
		}

		if (try_count != 0 || fname[len - 1] == '/') {
			unsigned cl = static_cast<unsigned>(std::clock());
			while (cl) {
				fname[len++] = '0' + static_cast<char>(cl % 10);
				cl /= 10;
			}
			while (try_count) {
				fname[len++] = '0' + static_cast<char>(try_count % 10);
				try_count /= 10;
			}
		}
		return true;
	}


	MICRO_EXPORT_CLASS_MEMBER bool FilePageProvider::init(const char* filename, std::uint64_t size, unsigned flags) noexcept
	{
		
		std::lock_guard<spinlock> ll(d_lock);

		if (d_filename[0]) {
			// Close all views
			auto* p = d_first;
			while (p) {
				auto* next = p->next;
				p->view = memory_map_file_view{};
				p = next;
			}
			d_file.init(nullptr, 0);
			d_first = nullptr;
			d_filename[0] = 0;
			d_size = d_file_size = 0;
			d_flags = 0;
		}

		// Initial size must be at least equal to 2 pages (one to store the memory provider, and one to provide)
		if (size < p_size * 2)
			size = p_size * 2;

		memory_map_file_view view;

		char tmp[MICRO_MAX_PATH];

		if (!d_basename[0] || params().page_file_provider_dir[0]) {
			unsigned try_count = 0;
			while (try_count < 1000) {
				memset(tmp, 0, sizeof(tmp));
				if (!create_filename(this, tmp, params().page_file_provider_dir.data(), d_basename, try_count))
					return false;
				filename = tmp;
				view = d_file.init(filename, size);
				if (view.valid()) {
					strcpy(const_cast<char*>(params().page_file_provider.data()), filename);
					break;
				}
				++try_count;
			}
		}
		else {
			view = d_file.init(d_basename, size);
		}

		if (!view.valid()) {
			d_file.init(nullptr, 0);
			if (log_enabled(MicroWarning))
				print_safe(stderr, "WARNING cannot create FilePageProvider on ", filename ,"\n");
			return false;
		}

		// Initialize first memory provider
		auto* view_ptr = view.view_ptr();
		auto view_size = view.view_size();

		d_first = new (view_ptr) MemPageProvider{ std::move(view), { this->params(), p_size, false }, nullptr };
		d_first->provider.init(static_cast<char*>(view_ptr) + sizeof(MemPageProvider), static_cast<std::uintptr_t>(view_size - sizeof(MemPageProvider)));
		d_file_size = view_size;
		d_size = size;
		d_flags = flags;
		strcpy(d_filename, filename);

		// If we used a temporary file, remove it at exit
		//if (filename == tmp) {
		//	d_flags |= MicroRemoveOnClose;
		//}
		return true;
	}

	MICRO_EXPORT_CLASS_MEMBER void* FilePageProvider::allocate_pages(size_t pcount) noexcept
	{
		
		std::lock_guard<spinlock> ll(d_lock);

		// Try to allocate pages from existing providers
		MemPageProvider* p = d_first;
		while (p) {
			void* pages = p->provider.allocate_pages(pcount);
			if (pages) {
				MICRO_ASSERT_DEBUG(!pages || (reinterpret_cast<uintptr_t>(pages) & (p_size - 1)) == 0, "");
				memset(pages, 0, p_size * pcount);
				return pages;
			}
			p = p->next;
		}

		if (!(d_flags & MicroGrowing) || !d_first) {
			if (log_enabled(MicroWarning))
				print_stderr(MicroWarning, this->params().log_date_format.data(), "FilePageProvider: cannot allocate %u pages\n", static_cast<unsigned>(pcount));
			return nullptr;
		}

		// Create a new provider by growing the file
		std::uint64_t bytes = pcount * p_size;
		std::uint64_t bytes_from_grow_factor = static_cast<std::uint64_t>(static_cast<double>(d_file_size) * (d_grow_factor - 1.));
		if (bytes < bytes_from_grow_factor)
			bytes = bytes_from_grow_factor;
		bytes += p_size; // add a page for MemoryPageProvider bookkeeping

		auto view = d_file.extend(bytes);
		if (!view.valid()) {
			if (log_enabled(MicroWarning))
				print_stderr(
				  MicroWarning, this->params().log_date_format.data(), "FilePageProvider: cannot allocate %u pages: unable to extend file %s\n", static_cast<unsigned>(pcount), d_filename);
			return nullptr;
		}

		auto* view_ptr = view.view_ptr();
		auto view_size = view.view_size();

		d_file_size += view_size;
		MemPageProvider* provider = new (view_ptr) MemPageProvider{ std::move(view), { this->params(), p_size, false }, d_first };
		provider->provider.init(static_cast<char*>(view_ptr) + sizeof(MemPageProvider), static_cast<std::uintptr_t>(view_size - sizeof(MemPageProvider)));
		d_first = provider;

		void * pages= provider->provider.allocate_pages(pcount);
		MICRO_ASSERT_DEBUG(!pages || (reinterpret_cast<uintptr_t>(pages) & (p_size - 1)) == 0, "");
		if(pages)
			memset(pages, 0, p_size * pcount);
		return pages;
	}

	MICRO_EXPORT_CLASS_MEMBER bool FilePageProvider::deallocate_pages(void* p, size_t pcount) noexcept
	{
		std::lock_guard<spinlock> ll(d_lock);

		// Find the owning provider
		MemPageProvider* provider = d_first;
		while (provider) {
			if (provider->provider.own(p))
				return provider->provider.deallocate_pages(p, pcount);
			provider = provider->next;
		}
		if (log_enabled(MicroWarning))
			print_stderr(MicroWarning, this->params().log_date_format.data(), "FilePageProvider: cannot deallocate %u pages\n", static_cast<unsigned>(pcount));
		return false;
	}

	MICRO_EXPORT_CLASS_MEMBER const char* FilePageProvider::current_filename() const noexcept
	{
		std::lock_guard<spinlock> ll(const_cast<spinlock&>(d_lock));
		return params().page_file_provider.data();
	}
	MICRO_EXPORT_CLASS_MEMBER std::uint64_t FilePageProvider::current_size() const noexcept
	{
		std::lock_guard<spinlock> ll(const_cast<spinlock&>(d_lock));
		return d_size;
	}
	MICRO_EXPORT_CLASS_MEMBER unsigned FilePageProvider::current_flags() const noexcept
	{
		std::lock_guard<spinlock> ll(const_cast<spinlock&>(d_lock));
		return d_flags;
	}

#endif

	MICRO_EXPORT_CLASS_MEMBER PreallocatePageProvider::PreallocatePageProvider(const parameters& params, size_t bytes, bool allow_grow) noexcept
	  : BasePageProvider(params)
	  , d_pages(nullptr)
	  , d_pcount(0)
	  , d_provider(params, static_cast<unsigned>(os_page_size()), allow_grow)
	{
		size_t pcount = bytes / os_page_size() + (bytes % os_page_size() ? 1 : 0);
		d_pages = os_allocate_pages(pcount);
		if (d_pages) {
			d_pcount = pcount;
			d_provider.init(static_cast<char*>(d_pages), pcount * os_page_size());
		}
	}
	MICRO_EXPORT_CLASS_MEMBER PreallocatePageProvider::~PreallocatePageProvider() noexcept
	{
		if (d_pages) {
			if (!os_free_pages(d_pages, d_pcount))
				if (log_enabled(MicroWarning))
					print_stderr(MicroWarning, params().log_date_format.data(), "unable to free pages");
		}
	}

	MICRO_EXPORT_CLASS_MEMBER void* PreallocatePageProvider::allocate_pages(size_t pcount) noexcept { return d_provider.allocate_pages(pcount); }

	MICRO_EXPORT_CLASS_MEMBER bool PreallocatePageProvider::deallocate_pages(void* p, size_t pcount) noexcept { return d_provider.deallocate_pages(p, pcount); }
}

MICRO_POP_DISABLE_OLD_STYLE_CAST
