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

#ifndef MICRO_OS_MAP_FILE_HPP
#define MICRO_OS_MAP_FILE_HPP

#include "bits.hpp"
#include <limits>

namespace micro
{
	/// @class memory_map_file_view
	///
	/// @brief A memory mapped view on a sub part of file.
	///

	/// @class memory_map_file
	///
	/// @brief Memory mapped file that can be extended
	///
}

#if defined(_MSC_VER) || defined(__MINGW32__)
#include "Windows.h"

#ifdef min
#undef min
#undef max
#endif

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

namespace micro
{
	class MICRO_EXPORT_CLASS memory_map_file_view
	{
		friend class memory_map_file;

		HANDLE hMapFile{ nullptr };
		void* hPtr{ nullptr };
		std::uint64_t offset{ 0 };
		std::uint64_t size{ 0 };

		memory_map_file_view(HANDLE MapFile, void* p, std::uint64_t o, std::uint64_t s) noexcept
		  : hMapFile(MapFile)
		  , hPtr(p)
		  , offset(o)
		  , size(s)
		{
		}

	public:
		MICRO_DELETE_COPY(memory_map_file_view)

		memory_map_file_view() noexcept {}
		memory_map_file_view(memory_map_file_view&& other) noexcept
		  : hMapFile(other.hMapFile)
		  , hPtr(other.hPtr)
		  , offset(other.offset)
		  , size(other.size)
		{
			other.hMapFile = nullptr;
			other.hPtr = nullptr;
			other.offset = 0;
			other.size = 0;
		}
		~memory_map_file_view()
		{
			if (hPtr)
				UnmapViewOfFile(hPtr);
			if (hMapFile)
				CloseHandle(hMapFile);
		}
		memory_map_file_view& operator=(memory_map_file_view&& other) noexcept
		{
			std::swap(hMapFile, other.hMapFile);
			std::swap(hPtr, other.hPtr);
			std::swap(offset, other.offset);
			std::swap(size, other.size);
			return *this;
		}

		bool null() const noexcept { return hPtr == nullptr; }
		bool valid() const noexcept { return hPtr != nullptr; }
		std::uint64_t file_offset() const noexcept { return offset; }
		std::uint64_t view_size() const noexcept { return size; }
		void* view_ptr() const noexcept { return hPtr; }
	};

	class MICRO_EXPORT_CLASS memory_map_file
	{
		HANDLE hFile{ nullptr };
		std::uint64_t hSize{ 0 };
		bool hUseFileSize{ false };

	public:
		MICRO_DELETE_COPY(memory_map_file)
		memory_map_file() noexcept = default;
		~memory_map_file() noexcept { init(nullptr, 0); }
		std::uint64_t file_size() const noexcept { return hSize; }

		/// @brief Initialize from filename and file size.
		/// If provided size is 0, use the file size and prevent from growing. In this case, the file must already exist.
		memory_map_file_view init(const char* filename, std::uint64_t size) noexcept
		{
			// Calling init() will invalidate all previously created memory_map_file_view!
			if (hFile) {
				CloseHandle(hFile);
				hFile = nullptr;
				hSize = 0;
			}
			if (!filename)
				return memory_map_file_view{};

			hUseFileSize = size == 0;
			DWORD open_flag = hUseFileSize ? OPEN_EXISTING : OPEN_ALWAYS;

			// Create file
			hFile = CreateFileA(filename,			  // file name
					    GENERIC_WRITE | GENERIC_READ, // access type
					    0,				  // other processes can't share
					    nullptr,			  // security
					    open_flag,
					    FILE_ATTRIBUTE_NORMAL,
					    nullptr);
			if (hFile == INVALID_HANDLE_VALUE) {
				hFile = nullptr;
				return memory_map_file_view{};
			}

			if (hUseFileSize) {
				// use file size
				LARGE_INTEGER fsize;
				if (!GetFileSizeEx(hFile, &fsize) || fsize.QuadPart < static_cast<LONGLONG>(os_allocation_granularity())) {
					CloseHandle(hFile);
					hFile = nullptr;
					hSize = 0;
					return memory_map_file_view{};
				}

				return extend((static_cast<ULONGLONG>(fsize.QuadPart) / os_allocation_granularity()) * os_allocation_granularity());
			}

			return extend(size);
		}

		memory_map_file_view extend(std::uint64_t bytes) noexcept
		{
			if (!bytes || !hFile)
				return memory_map_file_view{};

			if (hUseFileSize && hSize) {
				// If using file size and a memory_map_file_view was already created, cannot extend file
				return memory_map_file_view{};
			}

			std::uint64_t new_size = hSize + bytes;

			if (!hUseFileSize) {
				// Resize file
				bytes = (bytes / os_allocation_granularity() + (bytes % os_allocation_granularity() ? 1 : 0)) * os_allocation_granularity();
				new_size = hSize + bytes;
				// Set size
				LARGE_INTEGER l;
				l.QuadPart = static_cast<LONGLONG>(new_size);
				if (!SetFilePointerEx(hFile, l, nullptr, FILE_BEGIN)) {
					return memory_map_file_view{};
				}
				if (!SetEndOfFile(hFile)) {
					return memory_map_file_view{};
				}
			}

			HANDLE hMapFile = CreateFileMapping(hFile,	    // file handle
							    nullptr,	    // default security
							    PAGE_READWRITE, // read/write access
							    0,		    // maximum object size (high-order DWORD)
							    0,		    // maximum object size (low-order DWORD)
							    // 0 means map the whole file
							    nullptr);

			if (hMapFile == nullptr) {
				if (!hUseFileSize) {
					LARGE_INTEGER l;
					l.QuadPart = static_cast<LONGLONG>(hSize);
					if (SetFilePointerEx(hFile, l, nullptr, FILE_BEGIN))
						SetEndOfFile(hFile);
				}
				return memory_map_file_view{};
			}

			LARGE_INTEGER l;
			l.QuadPart = static_cast<LONGLONG>(hSize);
			void* p = MapViewOfFile(hMapFile, FILE_MAP_READ | FILE_MAP_WRITE, static_cast<DWORD>(l.HighPart), l.LowPart, 0);
			if (!p) {
				if (!hUseFileSize) {
					// shrink file to previous size
					l.QuadPart = static_cast<LONGLONG>(hSize);
					if (SetFilePointerEx(hFile, l, nullptr, FILE_BEGIN))
						SetEndOfFile(hFile);
				}
				return memory_map_file_view{};
			}
			hSize = new_size;
			return memory_map_file_view{ hMapFile, p, new_size - bytes, bytes };
		}
	};
}

#else

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h> // sysconf
#if defined(__linux__)
#include <fcntl.h>
#include <features.h>
#if defined(__GLIBC__)
#include <linux/mman.h> // linux mmap flags
#else
#include <sys/mman.h>
#endif
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if !TARGET_IOS_IPHONE && !TARGET_IOS_SIMULATOR
#include <mach/vm_statistics.h>
#endif
#elif defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/param.h>
#if __FreeBSD_version >= 1200000
#include <sys/cpuset.h>
#include <sys/domainset.h>
#endif
#include <sys/sysctl.h>
#endif

#if !defined(__HAIKU__) && !defined(__APPLE__) && !defined(__CYGWIN__)
#define MICRO_HAS_SYSCALL_H
#include <sys/syscall.h>
#endif

namespace micro
{
	class MICRO_EXPORT_CLASS memory_map_file_view
	{
		friend class memory_map_file;

		void* hPtr{ nullptr };
		std::uint64_t offset{ 0 };
		std::uint64_t size{ 0 };

		memory_map_file_view(void* p, std::uint64_t o, std::uint64_t s) noexcept
		  : hPtr(p)
		  , offset(o)
		  , size(s)
		{
		}

	public:
		MICRO_DELETE_COPY(memory_map_file_view)

		memory_map_file_view() noexcept {}
		memory_map_file_view(memory_map_file_view&& other) noexcept
		  : hPtr(other.hPtr)
		  , offset(other.offset)
		  , size(other.size)
		{
			other.hPtr = nullptr;
			other.offset = 0;
			other.size = 0;
		}
		~memory_map_file_view()
		{
			if (hPtr)
				munmap(hPtr, size);
		}
		memory_map_file_view& operator=(memory_map_file_view&& other) noexcept
		{
			std::swap(hPtr, other.hPtr);
			std::swap(offset, other.offset);
			std::swap(size, other.size);
			return *this;
		}

		bool null() const noexcept { return hPtr == nullptr; }
		bool valid() const noexcept { return hPtr != nullptr; }
		std::uint64_t file_offset() const noexcept { return offset; }
		std::uint64_t view_size() const noexcept { return size; }
		void* view_ptr() const noexcept { return hPtr; }
	};

	class MICRO_EXPORT_CLASS memory_map_file
	{
		int hFile{ 0 };
		std::uint64_t hSize{ 0 };

	public:
		MICRO_DELETE_COPY(memory_map_file)
		memory_map_file() noexcept = default;
		~memory_map_file() noexcept { init(nullptr, 0); }
		std::uint64_t file_size() const noexcept { return hSize; }

		/// @brief Initialize from filename and file size.
		/// If provided size is 0, use the file size and prevent from growing. In this case, the file must already exist.
		memory_map_file_view init(const char* filename, std::uint64_t size) noexcept
		{
			// Calling init() will invalidate all previously created memory_map_file_view!
			if (hFile) {
				close(hFile);
				hFile = 0;
				hSize = 0;
			}
			if (!filename)
				return memory_map_file_view{};

			hFile = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

			if (hFile == -1) {
				hFile = 0;
				return memory_map_file_view{};
			}

			return extend(size);
		}

		memory_map_file_view extend(std::uint64_t bytes) noexcept
		{
			if (!bytes || !hFile)
				return memory_map_file_view{};

			std::uint64_t new_size = hSize + bytes;

			// Resize file
			bytes = (bytes / os_allocation_granularity() + (bytes % os_allocation_granularity() ? 1 : 0)) * os_allocation_granularity();
			new_size = hSize + bytes;

			if (ftruncate(hFile, new_size) < 0)
				return memory_map_file_view{};

			void* ptr = mmap(0, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, hFile, hSize);

			if (ptr == 0) {
				ftruncate(hFile, hSize);
				return memory_map_file_view{};
			}

			hSize = new_size;
			return memory_map_file_view{ ptr, new_size - bytes, bytes };
		}
	};

}
#endif

MICRO_POP_DISABLE_OLD_STYLE_CAST

#endif
