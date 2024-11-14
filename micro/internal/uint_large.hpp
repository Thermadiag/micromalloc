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

#ifndef MICRO_UINT_HPP
#define MICRO_UINT_HPP

#include "../bits.hpp"
#include <atomic>

namespace micro
{
	namespace detail
	{

#ifdef MICRO_ARCH_64

		/// @brief 64 bits unsigned integer
		struct UInt64
		{
			std::atomic<std::uint64_t> mask;

			MICRO_ALWAYS_INLINE UInt64() noexcept
			  : mask(0)
			{
			}
			MICRO_ALWAYS_INLINE bool null() const noexcept { return mask.load(std::memory_order_relaxed) == 0; }
			MICRO_ALWAYS_INLINE void set(unsigned pos) noexcept { mask.fetch_or(1ull << pos, std::memory_order_relaxed); }
			MICRO_ALWAYS_INLINE void unset(unsigned pos) noexcept { mask.fetch_and(~(1ull << pos), std::memory_order_relaxed); }
			MICRO_ALWAYS_INLINE bool has_less_than_page_2() const noexcept { return mask.load(std::memory_order_relaxed) & 255u; }
			MICRO_ALWAYS_INLINE unsigned scan_forward(unsigned start) const noexcept
			{
				auto m0 = mask.load(std::memory_order_relaxed) & ~((1ull << start) - 1ull);
				if (m0)
					return bit_scan_forward_64(m0);
				return 64;
			}
			MICRO_ALWAYS_INLINE unsigned scan_forward_small(unsigned start) const noexcept { return scan_forward(start); }
			MICRO_ALWAYS_INLINE bool has_first_bit() const noexcept { return mask.load(std::memory_order_relaxed) & 1u; }
		};

		/// @brief Generic N bits integer for 64 bits platforms
		template<unsigned N>
		struct UIntN_64bits
		{
			std::atomic<std::uint64_t> masks[N];

			MICRO_ALWAYS_INLINE UIntN_64bits() noexcept { memset(static_cast<void*>(masks), 0, sizeof(masks)); }
			MICRO_ALWAYS_INLINE bool null() const noexcept
			{
				for (unsigned i = 0; i < N; ++i)
					if (masks[i].load(std::memory_order_relaxed))
						return false;
				return true;
			}
			MICRO_ALWAYS_INLINE void set(unsigned pos) noexcept
			{
				const unsigned idx = pos / 64;
				const unsigned p = pos & 63;
				masks[idx].fetch_or(1ull << p, std::memory_order_relaxed);
			}
			MICRO_ALWAYS_INLINE void unset(unsigned pos) noexcept
			{
				const unsigned idx = pos / 64;
				const unsigned p = pos & 63;
				masks[idx].fetch_and(~(1ull << p), std::memory_order_relaxed);
			}
			MICRO_ALWAYS_INLINE unsigned scan_forward(unsigned start) const noexcept
			{
				const unsigned idx = start / 64;
				auto m = masks[idx].load(std::memory_order_relaxed) & ~((1ull << (start - idx * 64)) - 1ull);
				if (m)
					return bit_scan_forward_64(m) + idx * 64;

				for (unsigned i = idx + 1; i < N; ++i) {
					m = masks[i].load(std::memory_order_relaxed);
					if (m)
						return bit_scan_forward_64(m) + i * 64;
				}
				return N * 64u;
			}
			MICRO_ALWAYS_INLINE unsigned scan_forward_small(unsigned start) const noexcept
			{
				auto m = masks[0].load(std::memory_order_relaxed) & ~((1ull << start) - 1ull);
				if (m) return bit_scan_forward_64(m) ;
				return N * 64u;
			}
			MICRO_ALWAYS_INLINE bool has_first_bit() const noexcept { return masks[0].load(std::memory_order_relaxed) & 1u; }
		};

		using UInt128 = UIntN_64bits<2>;
		using UInt256 = UIntN_64bits<4>;
		using UInt512 = UIntN_64bits<8>;
#else

		/// @brief Generic N bits integer for 32 bits platforms
		template<unsigned N>
		struct UIntN_32bits
		{
			static constexpr unsigned mask_8192 = N == 1 ? 65535 : (N == 2 ? 255 : 3);
			std::atomic<std::uint32_t> masks[N];

			MICRO_ALWAYS_INLINE UIntN_32bits() noexcept { memset(static_cast<void*>(masks), 0, sizeof(masks)); }
			MICRO_ALWAYS_INLINE bool null() const noexcept
			{
				for (unsigned i = 0; i < N; ++i)
					if (masks[i].load(std::memory_order_relaxed))
						return false;
				return true;
			}
			MICRO_ALWAYS_INLINE void set(unsigned pos) noexcept
			{
				unsigned idx = pos / 32;
				unsigned p = pos & 31;
				masks[idx].fetch_or(1u << p, std::memory_order_relaxed);
			}
			MICRO_ALWAYS_INLINE void unset(unsigned pos) noexcept
			{
				unsigned idx = pos / 32;
				unsigned p = pos & 31;
				masks[idx].fetch_and(~(1u << p), std::memory_order_relaxed);
			}
			MICRO_ALWAYS_INLINE bool has_less_than_page_2() const noexcept { return masks[0].load(std::memory_order_relaxed) & mask_8192; }
			MICRO_ALWAYS_INLINE unsigned scan_forward(unsigned start) const noexcept
			{
				unsigned idx = start / 32;

				auto m = masks[idx].load(std::memory_order_relaxed) & ~((1u << (start - idx * 32)) - 1u);
				if (m)
					return bit_scan_forward_32(m) + idx * 32;

				for (unsigned i = idx + 1; i < N; ++i) {
					m = masks[i].load(std::memory_order_relaxed);
					if (m)
						return bit_scan_forward_32(m) + i * 32;
				}
				return N * 32u;
			}
			MICRO_ALWAYS_INLINE unsigned scan_forward_small(unsigned start) const noexcept
			{
				if (start > 31)
					return N * 32u;
				auto m = masks[0].load(std::memory_order_relaxed) & ~((1u << start) - 1u);
				if (m)
					return bit_scan_forward_32(m);
				return N * 32u;
			}
			MICRO_ALWAYS_INLINE bool has_first_bit() const noexcept { return masks[0].load(std::memory_order_relaxed) & 1u; }
		};

		using UInt64 = UIntN_32bits<2>;
		using UInt128 = UIntN_32bits<4>;
		using UInt256 = UIntN_32bits<8>;
		using UInt512 = UIntN_32bits<16>;
#endif

	}
}

#endif
