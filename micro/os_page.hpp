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

#ifndef MICRO_OS_PAGE_HPP
#define MICRO_OS_PAGE_HPP

#include "bits.hpp"
#include "enums.h"

#ifndef MICRO_HEADER_ONLY
namespace micro
{
	/// @brief Returns OS page size
	MICRO_EXPORT size_t os_page_size() noexcept;
	/// @brief Returns OS allocation granularity
	MICRO_EXPORT size_t os_allocation_granularity() noexcept;
	/// @brief Allocate (commit) pages
	MICRO_EXPORT void* os_allocate_pages(size_t pages) noexcept;
	/// @brief Decommit pages
	MICRO_EXPORT bool os_free_pages(void* p, size_t pages) noexcept;
	/// @brief Retrieve process infos
	MICRO_EXPORT bool os_process_infos(micro_process_infos& infos) noexcept;
}
#else
#include "internal/os_page.cpp"
#endif

#endif
