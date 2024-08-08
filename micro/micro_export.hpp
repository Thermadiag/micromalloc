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

#ifndef MICRO_EXPORT_HPP
#define MICRO_EXPORT_HPP

#include "micro_config.hpp"

// Export symbols
#if defined _WIN32 || defined __CYGWIN__ || defined __MINGW32__
#ifdef MICRO_BUILD_SHARED_LIBRARY
#ifdef __GNUC__
#define MICRO_EXPORT __attribute__((dllexport))
#else
#define MICRO_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#if !defined(MICRO_BUILD_STATIC_LIBRARY) && !defined(MICRO_STATIC) // For static build, the user must define MICRO_STATIC in its project
#ifdef __GNUC__
#define MICRO_EXPORT __attribute__((dllimport))
#else
#define MICRO_EXPORT __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#define MICRO_EXPORT
#endif
#endif
#else
#if __GNUC__ >= 4
#define MICRO_EXPORT __attribute__((visibility("default")))
#else
#define MICRO_EXPORT
#endif
#endif

#endif
