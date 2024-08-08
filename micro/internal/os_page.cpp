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

#if defined(_WIN32)

extern "C" {
#include "Windows.h"
#include "Psapi.h"
}
#endif

#include "../os_page.hpp"
#include "defines.hpp"

#include <atomic>

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

#if defined(_WIN32)

namespace micro
{
	namespace detail
	{
		static inline SYSTEM_INFO build_sys_infos()
		{
			SYSTEM_INFO si;
			GetSystemInfo(&si);
			return si;
		}
		static inline const SYSTEM_INFO& sys_infos()
		{
			static SYSTEM_INFO inst = build_sys_infos();
			return inst;
		}

	}
	MICRO_HEADER_ONLY_EXPORT_FUNCTION size_t os_allocation_granularity() noexcept { return detail::sys_infos().dwAllocationGranularity; }

	MICRO_HEADER_ONLY_EXPORT_FUNCTION size_t os_page_size() noexcept { return detail::sys_infos().dwPageSize; }

	MICRO_HEADER_ONLY_EXPORT_FUNCTION void* os_allocate_pages(size_t pages) noexcept { return VirtualAlloc(nullptr, pages * os_page_size(), MEM_COMMIT, PAGE_READWRITE); }

	MICRO_HEADER_ONLY_EXPORT_FUNCTION bool os_free_pages(void* p, size_t pages) noexcept
	{
#ifndef MICRO_STRONG_PAGE_FREE
		// For small/medium page runs, decommit instead of releasing
		if (pages * os_page_size() <= MICRO_BLOCK_SIZE)
			return VirtualFree(p, pages * os_page_size(), MEM_DECOMMIT) != 0;
#else
		(void)pages;
#endif
		int r = VirtualFree(p, 0, MEM_RELEASE);
		return r != 0;
	}

	MICRO_HEADER_ONLY_EXPORT_FUNCTION bool os_process_infos(micro_process_infos& infos) noexcept
	{
		struct Init
		{
			typedef BOOL(WINAPI* PGetProcessMemoryInfo)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
			PGetProcessMemoryInfo pGetProcessMemoryInfo = nullptr;
			Init()
			{
				HINSTANCE hDll = LoadLibrary(TEXT("psapi.dll"));
				if (hDll != nullptr) {
					pGetProcessMemoryInfo = reinterpret_cast<PGetProcessMemoryInfo>(reinterpret_cast<void (*)(void)>(GetProcAddress(hDll, "GetProcessMemoryInfo")));
				}
			}
		};
		// mimalloc based approach

		static Init init;

		// get process info
		PROCESS_MEMORY_COUNTERS info;
		memset(&info, 0, sizeof(info));
		if (init.pGetProcessMemoryInfo)
			init.pGetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
		else
			return false;
		infos.current_rss = static_cast<size_t>(info.WorkingSetSize);
		infos.peak_rss = static_cast<size_t>(info.PeakWorkingSetSize);
		infos.current_commit = static_cast<size_t>(info.PagefileUsage);
		infos.peak_commit = static_cast<size_t>(info.PeakPagefileUsage);
		infos.page_faults = static_cast<size_t>(info.PageFaultCount);
		return true;
	}

}

#else // (Linux, macOSX, BSD, Illumnos, Haiku, DragonFly, etc.)

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE // ensure mmap flags and syscall are defined
#endif

#if defined(__sun)
// illumos provides new mman.h api when any of these are defined
// otherwise the old api based on caddr_t which predates the void pointers one.
// stock solaris provides only the former, chose to atomically to discard those
// flags only here rather than project wide tough.
#undef _XOPEN_SOURCE
#undef _POSIX_C_SOURCE
#endif

extern "C" {

#include <sys/mman.h>	  // mmap
#include <sys/resource.h> //getrusage
#include <unistd.h>	  // sysconf
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
}

namespace micro
{

	static inline int unix_madvise(void* addr, size_t size, int advice)
	{
#if defined(__sun)
		return madvise((caddr_t)addr, size, advice); // Solaris needs cast (issue #520)
#else
		return madvise(addr, size, advice);
#endif
	}

	MICRO_HEADER_ONLY_EXPORT_FUNCTION size_t os_page_size() noexcept
	{
		static size_t psize = (size_t)sysconf(_SC_PAGESIZE);
		return psize;
	}

	MICRO_HEADER_ONLY_EXPORT_FUNCTION void* os_allocate_pages(size_t pages) noexcept
	{
		size_t len = pages * os_page_size();
		void* p;
		if (MICRO_DEFAULT_PAGE_SIZE > os_page_size()) {
			void* m = mmap(0, len + (MICRO_DEFAULT_PAGE_SIZE - os_page_size()), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			p = m;
			if ((uintptr_t)m & (MICRO_DEFAULT_PAGE_SIZE - 1)) {
				p = (void*)(((uintptr_t)m & ~(MICRO_DEFAULT_PAGE_SIZE - 1)) + MICRO_DEFAULT_PAGE_SIZE);
				munmap(m, (size_t)((char*)p - (char*)m));
			}
		}
		else
			p = mmap(0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#if defined(__linux__) && defined(MICRO_ENABLE_THP)
		if (p && len == 2097152)
			unix_madvise(p, len, MADV_HUGEPAGE);
#endif
		return p;
	}

	MICRO_HEADER_ONLY_EXPORT_FUNCTION bool os_free_pages(void* p, size_t pages) noexcept
	{
#ifndef MICRO_STRONG_PAGE_FREE
		if (pages * os_page_size() <= MICRO_BLOCK_SIZE)
			return unix_madvise(p, pages * os_page_size(), MADV_DONTNEED) == 0;
#endif
		return (munmap(p, pages * os_page_size()) != -1);
	}

	MICRO_HEADER_ONLY_EXPORT_FUNCTION size_t os_allocation_granularity() noexcept { return os_page_size(); }

	MICRO_HEADER_ONLY_EXPORT_FUNCTION bool os_process_infos(micro_process_infos& infos) noexcept
	{
		struct rusage rusage;
		getrusage(RUSAGE_SELF, &rusage);
#if !defined(__HAIKU__)
		infos.page_faults = rusage.ru_majflt;
#endif
#if defined(__HAIKU__)
		// Haiku does not have (yet?) a way to
		// get these stats per process
		thread_info tid;
		area_info mem;
		ssize_t c;
		get_thread_info(find_thread(0), &tid);
		while (get_next_area_info(tid.team, &c, &mem) == B_OK) {
			infos.peak_rss += mem.ram_size;
		}
		infos.page_faults = 0;
#elif defined(__APPLE__)
		infos.peak_rss = rusage.ru_maxrss; // macos reports in bytes
#ifdef MACH_TASK_BASIC_INFO
		struct mach_task_basic_info info;
		mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
		if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
			infos.current_rss = (size_t)info.resident_size;
		}
#else
		struct task_basic_info info;
		mach_msg_type_number_t infoCount = TASK_BASIC_INFO_COUNT;
		if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
			infos.current_rss = (size_t)info.resident_size;
		}
#endif
#else

		infos.peak_rss = rusage.ru_maxrss * 1024; // Linux/BSD report in KiB

#endif
		// use defaults for commit

		// system cmd:
		// char cmd[200];
		// snprintf(cmd,sizeof(cmd),"grep ^VmPeak /proc/%d/status",(int)getpid ());
		// std::system(cmd);

		return true;
	}

}

#endif

MICRO_POP_DISABLE_OLD_STYLE_CAST
