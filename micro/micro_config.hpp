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

#ifndef MICRO_CONFIG_HPP
#define MICRO_CONFIG_HPP

/** @file */

/** \mainpage Micro-alloc library: generic allocator with tiny memory footprint


Purpose
-------

*/

/// This is the default micro.hpp file, just in case people will use the micro folder directly without installing the library

#define MICRO_VERSION_MAJOR 0
#define MICRO_VERSION_MINOR 0
#define MICRO_VERSION "0.0"

#if !defined(MICRO_BUILD_SHARED_LIBRARY) && !defined(MICRO_BUILD_STATIC_LIBRARY) && !defined(MICRO_PROXY_BUILD_LIBRARY)
#define MICRO_DETECT_IS_HEADER_ONLY 1
#else
#define MICRO_DETECT_IS_HEADER_ONLY 0
#endif

#if MICRO_DETECT_IS_HEADER_ONLY == 1
#if !defined(MICRO_HEADER_ONLY) && !defined(MICRO_NO_HEADER_ONLY)
#define MICRO_HEADER_ONLY
#elif defined(MICRO_NO_HEADER_ONLY) && defined(MICRO_HEADER_ONLY)
#undef MICRO_HEADER_ONLY
#endif
#endif

#endif
