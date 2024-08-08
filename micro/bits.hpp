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

#ifndef MICRO_BITS_HPP
#define MICRO_BITS_HPP

#ifdef __clang__
// Get rid of VERY annoying and useless warnings (clang does not recognize some doxygen commands)
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

/** @file */

/**\defgroup bits Bits: collection of functions for low level bits manipulation.

The bits module provides several portable low-level functions for bits manipulation:
	-	micro::bit_scan_forward_32: index of the lowest set bit in a 32 bits word
	-	micro::bit_scan_forward_64: index of the lowest set bit in a 64 bits word
	-	micro::bit_scan_reverse_32: index of the highest set bit in a 32 bits word
	-	micro::bit_scan_reverse_64: index of the highest set bit in a 32 bits word

See functions documentation for more details.
*/

/** \addtogroup bits
 *  @{
 */

/*#ifdef _MSC_VER
 // Silence msvc warning message about alignment
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#endif*/

#include <atomic>
#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "internal/micro_assert.hpp"
#include "micro_config.hpp"
#include "micro_export.hpp"

#if defined(__APPLE__)
// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#endif

#if defined(__sun) || defined(sun)
#include <sys/byteorder.h>
#endif

#if defined(__FreeBSD__)
#include <sys/endian.h>
#endif

#if defined(__OpenBSD__)
#include <sys/types.h>
#endif

#if defined(__NetBSD__)
#include <machine/bswap.h>
#include <sys/types.h>
#endif

// Fore header only (if MICRO_HEADER_ONLY is defined)
#ifdef MICRO_HEADER_ONLY
#define MICRO_HEADER_ONLY_EXPORT_FUNCTION inline
#define MICRO_HEADER_ONLY_ARG(...) __VA_ARGS__
#define MICRO_EXPORT_CLASS
#define MICRO_EXPORT_CLASS_MEMBER inline
#else
#define MICRO_HEADER_ONLY_EXPORT_FUNCTION
#define MICRO_HEADER_ONLY_ARG(...)
#define MICRO_EXPORT_CLASS MICRO_EXPORT
#define MICRO_EXPORT_CLASS_MEMBER
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#define MICRO_U64F "%I64d"
#else
#define MICRO_U64F "%" PRIu64
#endif

// From rapsody library
// https://stackoverflow.com/questions/4239993/determining-endianness-at-compile-time

#define MICRO_BYTEORDER_LITTLE_ENDIAN 0 // Little endian machine.
#define MICRO_BYTEORDER_BIG_ENDIAN 1	// Big endian machine.

// Find byte order
#ifndef MICRO_BYTEORDER_ENDIAN
// Detect with GCC 4.6's macro.
#if defined(__BYTE_ORDER__)
#if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define MICRO_BYTEORDER_ENDIAN MICRO_BYTEORDER_LITTLE_ENDIAN
#elif (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define MICRO_BYTEORDER_ENDIAN MICRO_BYTEORDER_BIG_ENDIAN
#else
#error "Unknown machine byteorder endianness detected. User needs to define MICRO_BYTEORDER_ENDIAN."
#endif
// Detect with GLIBC's endian.h.
#elif defined(__GLIBC__)
#include <endian.h>
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
#define MICRO_BYTEORDER_ENDIAN MICRO_BYTEORDER_LITTLE_ENDIAN
#elif (__BYTE_ORDER == __BIG_ENDIAN)
#define MICRO_BYTEORDER_ENDIAN MICRO_BYTEORDER_BIG_ENDIAN
#else
#error "Unknown machine byteorder endianness detected. User needs to define MICRO_BYTEORDER_ENDIAN."
#endif
// Detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro.
#elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#define MICRO_BYTEORDER_ENDIAN MICRO_BYTEORDER_LITTLE_ENDIAN
#elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
#define MICRO_BYTEORDER_ENDIAN MICRO_BYTEORDER_BIG_ENDIAN
// Detect with architecture macros.
#elif defined(__sparc) || defined(__sparc__) || defined(_POWER) || defined(__powerpc__) || defined(__ppc__) || defined(__hpux) || defined(__hppa) || defined(_MIPSEB) || defined(_POWER) ||            \
  defined(__s390__)
#define MICRO_BYTEORDER_ENDIAN MICRO_BYTEORDER_BIG_ENDIAN
#elif defined(__i386__) || defined(__alpha__) || defined(__ia64) || defined(__ia64__) || defined(_M_IX86) || defined(_M_IA64) || defined(_M_ALPHA) || defined(__amd64) || defined(__amd64__) ||        \
  defined(_M_AMD64) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || defined(__bfin__)
#define MICRO_BYTEORDER_ENDIAN MICRO_BYTEORDER_LITTLE_ENDIAN
#elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
#define MICRO_BYTEORDER_ENDIAN MICRO_BYTEORDER_LITTLE_ENDIAN
#else
#error "Unknown machine byteorder endianness detected. User needs to define MICRO_BYTEORDER_ENDIAN."
#endif
#endif

// Find 32/64 bits
#if defined(__x86_64__) || defined(__ppc64__) || defined(_WIN64)
#define MICRO_ARCH_64
#else
#define MICRO_ARCH_32
#endif

// Remove clang warning with non literal printing
#ifdef __clang__
#define MICRO_WARN_FORMAT(...)                                                                                                                                                                         \
	_Pragma("clang diagnostic push") _Pragma("clang diagnostic warning \"-Wformat-nonliteral\"") _Pragma("clang diagnostic warning \"-Wformat-security\"") __VA_ARGS__;                            \
	_Pragma("clang diagnostic pop")

#else
#define MICRO_WARN_FORMAT(...) __VA_ARGS__;
#endif

// Remove clang warning about old style cast
#ifdef __clang__
#define MICRO_PUSH_DISABLE_OLD_STYLE_CAST _Pragma("clang diagnostic push") _Pragma("clang diagnostic warning \"-Wold-style-cast\"") _Pragma("clang diagnostic warning \"-Wcast-align\"")
#define MICRO_POP_DISABLE_OLD_STYLE_CAST _Pragma("clang diagnostic pop")
#else
#define MICRO_PUSH_DISABLE_OLD_STYLE_CAST
#define MICRO_POP_DISABLE_OLD_STYLE_CAST
#endif

// Remove clang warning about exit time destructor
#ifdef __clang__
#define MICRO_PUSH_DISABLE_EXIT_TIME_DESTRUCTOR _Pragma("clang diagnostic push") _Pragma("clang diagnostic warning \"-Wexit-time-destructors\"")
#define MICRO_POP_DISABLE_EXIT_TIME_DESTRUCTOR _Pragma("clang diagnostic pop")
#else
#define MICRO_PUSH_DISABLE_EXIT_TIME_DESTRUCTOR
#define MICRO_POP_DISABLE_EXIT_TIME_DESTRUCTOR
#endif

// Disable copy for given class
#define MICRO_DELETE_COPY(classname)                                                                                                                                                                   \
	classname(const classname&) = delete;                                                                                                                                                          \
	classname& operator=(const classname&) = delete;

// Add specific cast members for given class
#define MICRO_ADD_CASTS(classname)                                                                                                                                                                     \
	static MICRO_ALWAYS_INLINE classname* from(void* p) noexcept { return static_cast<classname*>(p); }                                                                                            \
	static MICRO_ALWAYS_INLINE classname* from(std::uintptr_t p) noexcept { return reinterpret_cast<classname*>(p); }                                                                              \
	MICRO_ALWAYS_INLINE char* as_char() noexcept { return reinterpret_cast<char*>(this); }                                                                                                         \
	MICRO_ALWAYS_INLINE std::uintptr_t address() noexcept { return reinterpret_cast<std::uintptr_t>(this); }

// BIM2 instruction set is not properly defined on msvc
#if defined(_MSC_VER) && defined(__AVX2__)
#ifndef __BMI2__
#define __BMI2__
#endif
#endif

#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32)
#define WIN32 // Make sure WIN32 is defined
#endif

// __MINGW32__ doesn't seem to be properly defined, so define it.
#ifndef __MINGW32__
#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && defined(__GNUC__) && !defined(__CYGWIN__)
#define __MINGW32__
#endif
#endif

// Abort program with a last message
#define MICRO_ABORT(...)                                                                                                                                                                               \
	{                                                                                                                                                                                              \
		printf(__VA_ARGS__);                                                                                                                                                                   \
		fflush(stdout);                                                                                                                                                                        \
		abort();                                                                                                                                                                               \
	}

#ifdef __clang__
// clang produces " unused function template" warning with this one (?)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"
#endif
// going through a variable to avoid cppcheck error with MICRO_OFFSETOF
static constexpr void* __dummy_ptr_with_long_name = nullptr;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
// Redefine offsetof to get rid of warning "'offsetof' within non-standard-layout type ...."
#define MICRO_OFFSETOF(s, m) (reinterpret_cast<::size_t>(&reinterpret_cast<char const volatile&>(((static_cast<const s*>(__dummy_ptr_with_long_name))->m))))

// Check for C++17
#if defined(_MSC_VER) && !defined(__clang__)
#if _MSVC_LANG >= 201703L
#define MICRO_HAS_CPP_17
#endif
#if _MSVC_LANG >= 202002L
#define MICRO_HAS_CPP_20
#endif
#else
#if __cplusplus >= 201703L
#define MICRO_HAS_CPP_17
#endif
#if __cplusplus >= 202002L
#define MICRO_HAS_CPP_20
#endif
#endif

// If constexpr
#ifdef MICRO_HAS_CPP_17
#define MICRO_CONSTEXPR constexpr
#else
#define MICRO_CONSTEXPR
#endif

// Unreachable code
#ifdef _MSC_VER
#define MICRO_UNREACHABLE() __assume(0)
#else
#define MICRO_UNREACHABLE() __builtin_unreachable()
#endif

// pragma directive might be different between compilers, so define a generic MICRO_PRAGMA macro.
// Use MICRO_PRAGMA with no quotes around argument (ex: MICRO_PRAGMA(omp parallel) and not MICRO_PRAGMA("omp parallel") ).
#if defined(_MSC_VER) && !defined(__clang__)
#define MICRO_INTERNAL_PRAGMA(text) __pragma(text)
#else
#define MICRO_INTERNAL_PRAGMA(text) _Pragma(#text)
#endif
#define MICRO_PRAGMA(text) MICRO_INTERNAL_PRAGMA(text)

// no inline
#if defined(_MSC_VER) && !defined(__clang__)
#define MICRO_NOINLINE(...) __declspec(noinline) __VA_ARGS__
#else
#define MICRO_NOINLINE(...) __VA_ARGS__ __attribute__((noinline))
#endif

// For msvc, define __SSE__ and __SSE2__ manually
#if !defined(__SSE2__) && defined(_MSC_VER) && (defined(_M_X64) || _M_IX86_FP >= 2)
#define __SSE__ 1
#define __SSE2__ 1
#endif

// prefetching
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
#define MICRO_PREFETCH(p) __builtin_prefetch(reinterpret_cast<const char*>(p))
#elif defined(__SSE2__)
#define MICRO_PREFETCH(p) _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_T1)
#else
#define MICRO_PREFETCH(p)
#endif

// SSE intrinsics
#if defined(__SSE2__)
#if defined(__unix) || defined(__linux) || defined(__posix)
#include <emmintrin.h>
#include <xmmintrin.h>
#else
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

#endif

// fallthrough
#ifndef __has_cpp_attribute
#define __has_cpp_attribute(x) 0
#endif
#if __has_cpp_attribute(clang::fallthrough)
#define MICRO_FALLTHROUGH() [[clang::fallthrough]]
#elif __has_cpp_attribute(gnu::fallthrough)
#define MICRO_FALLTHROUGH() [[gnu::fallthrough]]
#else
#define MICRO_FALLTHROUGH()
#endif

// likely/unlikely definition
#if !defined(_MSC_VER) || defined(__clang__)
#define MICRO_LIKELY(x) __builtin_expect(!!(x), 1)
#define MICRO_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define MICRO_HAS_EXPECT
#else
#define MICRO_LIKELY(x) x
#define MICRO_UNLIKELY(x) x
#endif

// Simple function inlining
#define MICRO_INLINE inline

// Strongest available function inlining
#if (defined(__GNUC__) && (__GNUC__ >= 4)) || defined(__clang__)
#define MICRO_ALWAYS_INLINE __attribute__((always_inline)) inline
#define MICRO_EXTENSION __extension__
#define MICRO_HAS_ALWAYS_INLINE
#elif defined(__GNUC__)
#define MICRO_ALWAYS_INLINE inline
#define MICRO_EXTENSION __extension__
#elif (defined _MSC_VER) || (defined __INTEL_COMPILER)
#define MICRO_HAS_ALWAYS_INLINE
#define MICRO_ALWAYS_INLINE __forceinline
#else
#define MICRO_ALWAYS_INLINE inline
#endif

#ifndef MICRO_EXTENSION
#define MICRO_EXTENSION
#endif

// assume data are aligned
#if defined(__GNUC__) && (__GNUC__ >= 4 && __GNUC_MINOR__ >= 7)
#define MICRO_RESTRICT __restrict
#define MICRO_ASSUME_ALIGNED(type, ptr, out, alignment) type* MICRO_RESTRICT out = (type*)__builtin_assume_aligned((ptr), alignment);
#elif defined(__GNUC__)
#define MICRO_RESTRICT __restrict
#define MICRO_ASSUME_ALIGNED(type, ptr, out, alignment) type* MICRO_RESTRICT out = (ptr);
// on intel compiler, another way is to use #pragma vector aligned before the loop.
#elif defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)
#define MICRO_RESTRICT restrict
#define MICRO_ASSUME_ALIGNED(type, ptr, out, alignment)                                                                                                                                                \
	type* MICRO_RESTRICT out = ptr;                                                                                                                                                                \
	__assume_aligned(out, alignment);
#elif defined(__IBMCPP__)
#define MICRO_RESTRICT restrict
#define MICRO_ASSUME_ALIGNED(type, ptr, out, alignment) type __attribute__((aligned(alignment)))* MICRO_RESTRICT out = (type __attribute__((aligned(alignment)))*)(ptr);
#elif defined(_MSC_VER)
#define MICRO_RESTRICT __restrict
#define MICRO_ASSUME_ALIGNED(type, ptr, out, alignment) type* MICRO_RESTRICT out = ptr;
#endif

// Forces data to be n-byte aligned (this might be used to satisfy SIMD requirements).
#if (defined __GNUC__) || (defined __PGI) || (defined __IBMCPP__) || (defined __ARMCC_VERSION) || (defined __clang__)
#define MICRO_ALIGN_TO_BOUNDARY(n) __attribute__((aligned(n)))
#elif (defined _MSC_VER)
#define MICRO_ALIGN_TO_BOUNDARY(n) __declspec(align(n))
#elif (defined __SUNPRO_CC)
// FIXME not sure about this one:
#define MICRO_ALIGN_TO_BOUNDARY(n) __attribute__((aligned(n)))
#else
#define MICRO_ALIGN_TO_BOUNDARY(n) MICRO_USER_ALIGN_TO_BOUNDARY(n)
#endif

#ifndef MICRO_DEBUG
#ifndef NDEBUG
#define MICRO_DEBUG
#endif
#endif

// Debug assertion
#ifndef MICRO_DEBUG
#define MICRO_ASSERT_DEBUG(condition, msg)
#else
#define MICRO_ASSERT_DEBUG(condition, ...) assert((condition) && (__VA_ARGS__))
#endif

#ifdef MICRO_DEBUG
#define MICRO_DEBUG_ONLY(...) __VA_ARGS__
#else
#define MICRO_DEBUG_ONLY(...)
#endif

#if defined(MICRO_DEBUG) || defined(MICRO_ENABLE_ASSERT)
#define MICRO_ASSERT(condition, ...)                                                                                                                                                                   \
	do {                                                                                                                                                                                           \
		if (!(condition)) {                                                                                                                                                                    \
			micro::detail::assert_always(__FILE__, __LINE__, __VA_ARGS__);                                                                                                                 \
		}                                                                                                                                                                                      \
	} while (0)

#else
#define MICRO_ASSERT(condition, ...)
#endif

// Exit time destructor, sadly does not remove the warning...
#ifdef __clang__
#define MICRO_EXIT_TIME_DESTRUCTOR [[clang::always_destroy]]
#else
#define MICRO_EXIT_TIME_DESTRUCTOR
#endif

// Support for __has_builtin
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

// Support for __has_attribute
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if defined(__SIZEOF_INT128__)

namespace micro
{
	MICRO_ALWAYS_INLINE void umul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh)
	{
		const unsigned __int128 r = static_cast<unsigned __int128>(m1) * m2;

		*rh = static_cast<uint64_t>(r >> 64);
		*rl = static_cast<uint64_t>(r);
	}
}
#define MICRO_HAS_FAST_UMUL128 1

#elif (defined(__IBMC__) || defined(__IBMCPP__)) && defined(__LP64__)

namespace micro
{
	MICRO_ALWAYS_INLINE void umul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh)
	{
		*rh = __mulhdu(m1, m2);
		*rl = m1 * m2;
	}
}
#define MICRO_HAS_FAST_UMUL128 1

#elif defined(_MSC_VER) && (defined(_M_ARM64) || (defined(_M_X64) && defined(__INTEL_COMPILER)))

#include <intrin.h>

namespace micro
{
	MICRO_ALWAYS_INLINE void umul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh)
	{
		*rh = __umulh(m1, m2);
		*rl = m1 * m2;
	}
}
#define MICRO_HAS_FAST_UMUL128 1

#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IA64))

#include <intrin.h>
#pragma intrinsic(_umul128)
#pragma intrinsic(__shiftright128)

namespace micro
{
	// 128 bits multiplication for hash function
	static MICRO_ALWAYS_INLINE void umul128(const uint64_t m1, const uint64_t m2, uint64_t* const rl, uint64_t* const rh) { *rl = _umul128(m1, m2, rh); }
}
#define MICRO_HAS_FAST_UMUL128 1

#else // defined( _MSC_VER )

// _umul128() code for 32-bit systems, adapted from Hacker's Delight,
// Henry S. Warren, Jr.

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)

#include <intrin.h>
#pragma intrinsic(__emulu)
#define MICRO_EMULU(x, y) __emulu(x, y)

#else // defined( _MSC_VER ) && !defined( __INTEL_COMPILER )

#define MICRO_EMULU(x, y) ((uint64_t)(x) * (y))

#endif // defined( _MSC_VER ) && !defined( __INTEL_COMPILER )

namespace micro
{
	static inline void umul128(const uint64_t u, const uint64_t v, uint64_t* const rl, uint64_t* const rh)
	{
		*rl = u * v;

		const uint32_t u0 = static_cast<uint32_t>(u);
		const uint32_t v0 = static_cast<uint32_t>(v);
		const uint64_t w0 = MICRO_EMULU(u0, v0);
		const uint32_t u1 = static_cast<uint32_t>(u >> 32);
		const uint32_t v1 = static_cast<uint32_t>(v >> 32);
		const uint64_t t = MICRO_EMULU(u1, v0) + static_cast<uint32_t>(w0 >> 32);
		const uint64_t w1 = MICRO_EMULU(u0, v1) + static_cast<uint32_t>(t);

		*rh = MICRO_EMULU(u1, v1) + static_cast<uint32_t>(w1 >> 32) + static_cast<uint32_t>(t >> 32);
	}
}

#endif

#ifdef __GNUC__
#define GNUC_PREREQ(x, y) (__GNUC__ > x || (__GNUC__ == x && __GNUC_MINOR__ >= y))
#else
#define GNUC_PREREQ(x, y) 0
#endif

#ifdef __clang__
#define CLANG_PREREQ(x, y) (__clang_major__ > (x) || (__clang_major__ == (x) && __clang_minor__ >= (y)))
#else
#define CLANG_PREREQ(x, y) 0
#endif

#if (_MSC_VER < 1900) && !defined(__cplusplus)
#define inline __inline
#endif

#if (defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64))
#define X86_OR_X64
#endif

#if GNUC_PREREQ(4, 2) || __has_builtin(__builtin_popcount)
#define HAVE_BUILTIN_POPCOUNT
#endif

#if GNUC_PREREQ(4, 2) || CLANG_PREREQ(3, 0)
#define HAVE_ASM_POPCNT
#endif

#if defined(X86_OR_X64) && (defined(HAVE_ASM_POPCNT) || defined(_MSC_VER))
#define HAVE_POPCNT
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <immintrin.h>
#include <intrin.h>
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <nmmintrin.h>
#endif

namespace micro
{

	/// @brief Returns the lowest set bit index in \a val
	/// Undefined if val==0.
	MICRO_ALWAYS_INLINE auto bit_scan_forward_32(std::uint32_t val) -> unsigned int
	{
#if defined(_MSC_VER) /* Visual */
		unsigned long r = 0;
		_BitScanForward(&r, val);
		return static_cast<unsigned>(r);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))) /* Use GCC Intrinsic */
		return __builtin_ctz(val);
#else								     /* Software version */
		static const int MultiplyDeBruijnBitPosition[32] = { 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
		return MultiplyDeBruijnBitPosition[((uint32_t)((val & -val) * 0x077CB531U)) >> 27];
#endif
	}

	/// @brief Returns the highest set bit index in \a val
	/// Undefined if val==0.
	MICRO_ALWAYS_INLINE auto bit_scan_reverse_32(std::uint32_t val) -> unsigned int
	{
#if defined(_MSC_VER) /* Visual */
		unsigned long r = 0;
		_BitScanReverse(&r, val);
		return static_cast<unsigned>(r);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))) /* Use GCC Intrinsic */
		return 31 - __builtin_clz(val);
#else								     /* Software version */
		static const unsigned int pos[32] = { 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
		// does not work for 0
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v = (v >> 1) + 1;
		return pos[(v * 0x077CB531UL) >> 27];
#endif
	}

	/// @brief Returns the lowest set bit index in \a bb.
	/// Developed by Kim Walisch (2012).
	/// Undefined if bb==0.
	MICRO_ALWAYS_INLINE auto bit_scan_forward_64(std::uint64_t bb) noexcept -> unsigned
	{
#if defined(_MSC_VER) && defined(_WIN64)
		unsigned long r = 0;
		_BitScanForward64(&r, bb);
		return static_cast<unsigned>(r);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3)))
		return __builtin_ctzll(bb);
#else
		static const unsigned forward_index64[64] = { 0,  47, 1,  56, 48, 27, 2,  60, 57, 49, 41, 37, 28, 16, 3,  61, 54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4, 62,
							      46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45, 25, 39, 14, 33, 19, 30, 9,  24, 13, 18, 8,  12, 7,  6,  5, 63 };
		const std::uint64_t debruijn64 = std::int64_t(0x03f79d71b4cb0a89);
		return forward_index64[((bb ^ (bb - 1)) * debruijn64) >> 58];
#endif
	}

	/// @brief Returns the highest set bit index in \a bb.
	/// Developed by Kim Walisch, Mark Dickinson.
	/// Undefined if bb==0.
	MICRO_ALWAYS_INLINE auto bit_scan_reverse_64(std::uint64_t bb) noexcept -> unsigned
	{
#if (defined(_MSC_VER) && defined(_WIN64)) //|| defined(__MINGW64_VERSION_MAJOR)
		unsigned long r = 0;
		_BitScanReverse64(&r, bb);
		return static_cast<unsigned>(r);
#elif (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3)))
		return 63 - __builtin_clzll(bb);
#else
		static const unsigned backward_index64[64] = { 0,  47, 1,  56, 48, 27, 2,  60, 57, 49, 41, 37, 28, 16, 3,  61, 54, 58, 35, 52, 50, 42, 21, 44, 38, 32, 29, 23, 17, 11, 4, 62,
							       46, 55, 26, 59, 40, 36, 15, 53, 34, 51, 20, 43, 31, 22, 10, 45, 25, 39, 14, 33, 19, 30, 9,  24, 13, 18, 8,  12, 7,  6,  5, 63 };
		const std::uint64_t debruijn64 = std::int64_t(0x03f79d71b4cb0a89);
		// assert(bb != 0);
		bb |= bb >> 1;
		bb |= bb >> 2;
		bb |= bb >> 4;
		bb |= bb >> 8;
		bb |= bb >> 16;
		bb |= bb >> 32;
		return backward_index64[(bb * debruijn64) >> 58];
#endif
	}

#if defined(_MSC_VER) || ((defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 3))))
#define MICRO_HAS_BUILTIN_BITSCAN
#endif

	/// @brief Fast psudo random number generator.
	/// Generates 32 bits random integers.
	class fast_rand
	{
#if MICRO_HAS_FAST_UMUL128
		// komihash implementation: https://github.com/avaneev/komihash
		struct Seeds
		{
			uint64_t Seed1;
			uint64_t Seed2;
		};
		Seeds seeds;

	public:
		fast_rand(size_t seed) noexcept
		  : seeds{ seed, seed }
		{
		}

		unsigned operator()() noexcept
		{
			uint64_t s1 = seeds.Seed1;
			uint64_t s2 = seeds.Seed2;

			umul128(s1, s2, &s1, &s2);

			s2 += 0xAAAAAAAAAAAAAAAA;
			s1 ^= s2;

			seeds.Seed1 = s2;
			seeds.Seed2 = s1;

			return static_cast<unsigned>(s1 ^ (s1 << 32u));
		}

#else
		// Use Marsaglia's xorshf generator: https://github.com/raylee/xorshf96/blob/master/xorshf96.c
		unsigned x;
		unsigned y{ 362436069 };
		unsigned z{ 521288629 };

	public:
		fast_rand(size_t seed) noexcept
		  : x(static_cast<unsigned>(seed))
		{
			if (x == 0)
				x = 123456789;
		}

		unsigned operator()() noexcept
		{ // period 2^96-1
			unsigned t;
			x ^= x << 16;
			x ^= x >> 5;
			x ^= x << 1;

			t = x;
			x = y;
			y = z;
			z = t ^ x ^ y;

			return z;
		}
#endif
	};

	static MICRO_ALWAYS_INLINE unsigned random_uint32()
	{
		thread_local int seed = 0;
		thread_local fast_rand rng(static_cast<size_t>(reinterpret_cast<std::uintptr_t>(&seed)));
		return rng();
	}

} // end namespace micro

#ifdef __GNUC__
//#pragma GCC diagnostic pop
#endif

#undef max
#undef min

/** @}*/
// end bits

#endif
