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

#ifndef MICRO_TESTING_HPP
#define MICRO_TESTING_HPP

#if defined(WIN32) || defined(_WIN32)
#include <Windows.h>
#include <Psapi.h>

#else
#include <time.h>
#endif

#ifdef __linux__
#include <malloc.h>
#else
// Simulate malloc_trim
inline void malloc_trim(size_t) {}
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

#include "micro.h"
#include "os_timer.hpp"

#if defined(MICRO_BENCH_TBB) && defined(NDEBUG)
#define USE_TBB
#endif

#ifdef MICRO_BENCH_MIMALLOC
#include <mimalloc.h>
#endif
#ifdef USE_TBB
#include <tbb/scalable_allocator.h>
#endif

#ifdef MICRO_BENCH_SNMALLOC
#include <snmalloc/snmalloc.h>
#endif

#ifdef MICRO_BENCH_JEMALLOC
extern "C" {
#include "jemalloc.h"
}
#endif

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#ifdef __clang__
// remove these warnings for testing
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

namespace micro
{
	/// @brief Exception thrown for failed tests
	class test_error : public std::runtime_error
	{
	public:
		test_error(const std::string& str)
		  : std::runtime_error(str)
		{
		}
		virtual ~test_error() noexcept override {}
	};

	/// @brief Streambuf that stores the number of outputed characters
	class streambuf_size : public std::streambuf
	{
		std::streambuf* sbuf{ nullptr };
		std::ostream* oss{ nullptr };
		size_t size{ 0 };

		virtual int overflow(int c) override
		{
			size++;
			return sbuf->sputc(static_cast<char>(c));
		}
		virtual int sync() override { return sbuf->pubsync(); }

	public:
		streambuf_size(std::ostream& o)
		  : sbuf(o.rdbuf())
		  , oss(&o)
		{
			oss->rdbuf(this);
		}
		virtual ~streambuf_size() noexcept override { oss->rdbuf(sbuf); }
		size_t get_size() const { return size; }
	};

	namespace detail
	{
		template<class T>
		std::string to_string(const T& value)
		{
			std::ostringstream ss;
			ss << value;
			return ss.str();
		}
	}

}

/// @brief Very basic testing macro that throws micro::test_error if condition is not met.
#define MICRO_TEST(...)                                                                                                                                                                                \
	do {                                                                                                                                                                                           \
		if (!(__VA_ARGS__)) {                                                                                                                                                                  \
			throw micro::test_error("testing error at file " __FILE__ "(" + micro::detail::to_string(__LINE__) + "): " #__VA_ARGS__);                                                      \
		}                                                                                                                                                                                      \
	} while (false)
// if(! (__VA_ARGS__) ) {throw micro::test_error("testing error at file " __FILE__ "(" + micro::detail::to_string(__LINE__) + "): "  #__VA_ARGS__); }

/// @brief Test if writting given argument to a std::ostream produces the string 'result', throws micro::test_error if not.
#define MICRO_TEST_TO_OSTREAM(result, ...)                                                                                                                                                             \
	do {                                                                                                                                                                                           \
		std::ostringstream oss;                                                                                                                                                                \
		oss << (__VA_ARGS__);                                                                                                                                                                  \
		oss.flush();                                                                                                                                                                           \
		if (oss.str() != result) {                                                                                                                                                             \
			std::string v = micro::detail::to_string(__LINE__);                                                                                                                            \
			throw micro::test_error(("testing error at file " __FILE__ "(" + v + "): \"" + std::string(result) + "\" == " #__VA_ARGS__).c_str());                                          \
		}                                                                                                                                                                                      \
	} while (false)

/// @brief Test if given statement throws a 'exception' object. If not, throws micro::test_error.
#define MICRO_TEST_THROW(exception, ...)                                                                                                                                                               \
	do {                                                                                                                                                                                           \
		bool has_thrown = false;                                                                                                                                                               \
		try {                                                                                                                                                                                  \
			__VA_ARGS__;                                                                                                                                                                   \
		}                                                                                                                                                                                      \
		catch (const exception&) {                                                                                                                                                             \
			has_thrown = true;                                                                                                                                                             \
		}                                                                                                                                                                                      \
		catch (...) {                                                                                                                                                                          \
		}                                                                                                                                                                                      \
		if (!has_thrown) {                                                                                                                                                                     \
			std::string v = micro::detail::to_string(__LINE__);                                                                                                                            \
			throw micro::test_error(("testing error at file " __FILE__ "(" + v + "): " #__VA_ARGS__).c_str());                                                                             \
		}                                                                                                                                                                                      \
	} while (false)

/// @brief Test module
#define MICRO_TEST_MODULE(name, ...)                                                                                                                                                                   \
	do {                                                                                                                                                                                           \
		micro::streambuf_size str(std::cout);                                                                                                                                                  \
		size_t size = 0;                                                                                                                                                                       \
		bool ok = true;                                                                                                                                                                        \
		try {                                                                                                                                                                                  \
			std::cout << "TEST MODULE " << #name << "... ";                                                                                                                                \
			std::cout.flush();                                                                                                                                                             \
			size = str.get_size();                                                                                                                                                         \
			__VA_ARGS__;                                                                                                                                                                   \
		}                                                                                                                                                                                      \
		catch (const micro::test_error& e) {                                                                                                                                                   \
			std::cout << std::endl;                                                                                                                                                        \
			ok = false;                                                                                                                                                                    \
			std::cerr << "TEST FAILURE IN MODULE " << #name << ": " << e.what() << std::endl;                                                                                              \
		}                                                                                                                                                                                      \
		catch (const std::exception& e) {                                                                                                                                                      \
			std::cout << std::endl;                                                                                                                                                        \
			ok = false;                                                                                                                                                                    \
			std::cerr << "UNEXPECTED ERROR IN MODULE " << #name << " (std::exception): " << e.what() << std::endl;                                                                         \
		}                                                                                                                                                                                      \
		catch (...) {                                                                                                                                                                          \
			std::cout << std::endl;                                                                                                                                                        \
			ok = false;                                                                                                                                                                    \
			std::cerr << "UNEXPECTED ERROR IN MODULE " << #name << std::endl;                                                                                                              \
		}                                                                                                                                                                                      \
		if (ok) {                                                                                                                                                                              \
			if (str.get_size() != size)                                                                                                                                                    \
				std::cout << std::endl;                                                                                                                                                \
			std::cout << "SUCCESS" << std::endl;                                                                                                                                           \
		}                                                                                                                                                                                      \
	} while (false)

/// @brief Test module
#define MICRO_TEST_MODULE_RETURN(name, ret_value, ...)                                                                                                                                                 \
	do {                                                                                                                                                                                           \
		micro::streambuf_size str(std::cout);                                                                                                                                                  \
		size_t size = 0;                                                                                                                                                                       \
		bool ok = true;                                                                                                                                                                        \
		try {                                                                                                                                                                                  \
			std::cout << "TEST MODULE " << #name << "... ";                                                                                                                                \
			std::cout.flush();                                                                                                                                                             \
			size = str.get_size();                                                                                                                                                         \
			__VA_ARGS__;                                                                                                                                                                   \
		}                                                                                                                                                                                      \
		catch (const micro::test_error& e) {                                                                                                                                                   \
			std::cout << std::endl;                                                                                                                                                        \
			ok = false;                                                                                                                                                                    \
			std::cerr << "TEST FAILURE IN MODULE " << #name << ": " << e.what() << std::endl;                                                                                              \
		}                                                                                                                                                                                      \
		catch (const std::exception& e) {                                                                                                                                                      \
			std::cout << std::endl;                                                                                                                                                        \
			ok = false;                                                                                                                                                                    \
			std::cerr << "UNEXPECTED ERROR IN MODULE " << #name << " (std::exception): " << e.what() << std::endl;                                                                         \
		}                                                                                                                                                                                      \
		catch (...) {                                                                                                                                                                          \
			std::cout << std::endl;                                                                                                                                                        \
			ok = false;                                                                                                                                                                    \
			std::cerr << "UNEXPECTED ERROR IN MODULE " << #name << std::endl;                                                                                                              \
		}                                                                                                                                                                                      \
		if (ok) {                                                                                                                                                                              \
			if (str.get_size() != size)                                                                                                                                                    \
				std::cout << std::endl;                                                                                                                                                \
			std::cout << "SUCCESS" << std::endl;                                                                                                                                           \
		}                                                                                                                                                                                      \
		else                                                                                                                                                                                   \
			return ret_value;                                                                                                                                                              \
	} while (false)

namespace micro
{
	namespace detail
	{
		inline timer& local_timer()
		{
			thread_local timer t;
			return t;
		}
	}
	/// @brief For tests only, reset timer for calling thread
	inline void tick() { detail::local_timer().tick(); }

	/// @brief For tests only, returns elapsed milliseconds since last call to tick()
	inline auto tock_ms() -> std::uint64_t { return detail::local_timer().tock() / 1000000ull; }

	inline auto tock_micro() -> std::uint64_t { return detail::local_timer().tock() / 1000ull; }

	/// @brief Similar to C++11 (and deprecated) std::random_shuffle
	template<class Iter>
	void random_shuffle(Iter begin, Iter end, uint_fast32_t seed = 0)
	{
		std::random_device rd;
		std::mt19937 g(rd());
		if (seed)
			g.seed(seed);
		std::shuffle(begin, end, g);
	}

	struct Malloc
	{
		static void* alloc_mem(size_t i)
		{
			void* p = malloc(i);
			memset(p, 0, i);
			return p;
		}
		static void free_mem(void* p) { free(p); }
	};

#ifdef MICRO_BENCH_MIMALLOC
	struct MiMalloc
	{
		static void* alloc_mem(size_t i)
		{
			void* p = mi_malloc(i);
			memset(p, 0, i);
			return p;
		}
		static void free_mem(void* p) { mi_free(p); }
	};
#endif

#ifdef USE_TBB
	struct TBBMalloc
	{
		static void* alloc_mem(size_t i)
		{
			void* p = scalable_malloc(i);
			memset(p, 0, i);
			return p;
		}
		static void free_mem(void* p) { scalable_free(p); }
	};
#endif

#ifdef MICRO_BENCH_SNMALLOC
	struct SnMalloc
	{
		static void* alloc_mem(size_t i)
		{
			void* p = snmalloc::libc::malloc(i);
			memset(p, 0, i);
			return p;
		}
		static void free_mem(void* p) { snmalloc::libc::free(p); }
	};
#endif

#ifdef MICRO_BENCH_JEMALLOC
	struct Jemalloc
	{
		static void* alloc_mem(size_t i)
		{
			void* p = je_malloc(i);
			memset(p, 0, i);
			return p;
		}
		static void free_mem(void* p) { je_free(p); }
	};
#endif

	struct Alloc
	{
		static void* alloc_mem(size_t i)
		{
			char* p = (char*)micro_malloc(i);
			memset(p, 0, i);
			return p;
		}
		static void free_mem(void* p) { micro_free(p); }
	};

	/// @brief Test allocator, to be used with MiMalloc, TBBMalloc,...
	/// @tparam T
	/// @tparam Alloc
	template<class T, class Alloc>
	class testing_allocator
	{

	public:
		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_swap = std::true_type;
		using propagate_on_container_copy_assignment = std::true_type;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::true_type;
		template<class U>
		struct rebind
		{
			using other = testing_allocator<U, Alloc>;
		};

		auto select_on_container_copy_construction() const noexcept -> testing_allocator<T, Alloc> { return *this; }

		testing_allocator() noexcept = default;
		testing_allocator(const testing_allocator& other) noexcept = default;
		template<class U>
		testing_allocator(const testing_allocator<U, Alloc>&) noexcept
		{
		}
		~testing_allocator() noexcept = default;

		auto operator==(const testing_allocator& other) const noexcept -> bool { return true; }
		auto operator!=(const testing_allocator& other) const noexcept -> bool { return false; }
		auto address(reference x) const noexcept -> pointer { return std::addressof(x); }
		auto address(const_reference x) const noexcept -> const_pointer { return std::addressof(x); }
		auto allocate(size_t n, const void* /*unused*/) -> value_type* { return allocate(n); }
		auto allocate(size_t n) -> value_type*
		{
			value_type* p = static_cast<value_type*>(Alloc::alloc_mem(n * sizeof(T)));
			if (MICRO_UNLIKELY(!p))
				throw std::bad_alloc();
			return p;
		}
		void deallocate(value_type* p, size_t n) noexcept
		{
			(void)n;
			Alloc::free_mem(p);
		}
	};

	inline void print_process_infos()
	{
#ifdef __linux__
		char cmd[200];
		snprintf(cmd, sizeof(cmd), "grep ^VmHWM /proc/%d/status", (int)getpid());
		std::system(cmd);
		snprintf(cmd, sizeof(cmd), "grep ^VmPeak /proc/%d/status", (int)getpid());
		std::system(cmd);

		snprintf(cmd, sizeof(cmd), "echo 1 > /proc/%d/clear_refs", (int)getpid());
		std::system(cmd);
		// snprintf(cmd,sizeof(cmd),"echo 5 > /proc/%d/clear_refs",(int)getpid ());
		// std::system(cmd);
		std::cout << std::endl;
		return; // TEST
#endif
		micro_process_infos infos;
		micro_get_process_infos(&infos);
		std::cout << "Peak RSS: " << infos.peak_rss << std::endl;
		std::cout << "Peak Commit: " << infos.peak_commit << std::endl;
		std::cout << std::endl;
	}

	inline void allocator_trim(const char* allocator)
	{

		if (strcmp(allocator, "micro") == 0)
			micro_clear();
		if (strcmp(allocator, "malloc") == 0)
			malloc_trim(0);
#ifdef MICRO_BENCH_MIMALLOC
		if (strcmp(allocator, "mimalloc") == 0)
			mi_heap_collect(mi_heap_get_default(), true);
#endif
#ifdef USE_TBB
		if (strcmp(allocator, "onetbb") == 0) {
			// scalable_allocation_command(TBBMALLOC_CLEAN_THREAD_BUFFERS, nullptr);
			scalable_allocation_command(TBBMALLOC_CLEAN_ALL_BUFFERS, nullptr);
		}
#endif
	}

#ifdef MICRO_BENCH_JEMALLOC
	namespace detail
	{
		static const char* jemalloc_conf = "dirty_decay_ms:0,muzzy_decay_ms=0";
		struct InitJemalloc
		{
			InitJemalloc()
			{
				const char* je_malloc_conf = "dirty_decay_ms:0,muzzy_decay_ms=0";
				putenv((char*)"JE_MALLOC_CONF=dirty_decay_ms:0,muzzy_decay_ms=0");
			}
		};
		static InitJemalloc init_jemalloc;
	}
#endif
}
#endif
