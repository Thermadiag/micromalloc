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

#include "micro_assert.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace micro
{
	namespace detail
	{
#ifdef __clang__
		_Pragma("clang diagnostic push") _Pragma("clang diagnostic warning \"-Wformat-nonliteral\"") _Pragma("clang diagnostic warning \"-Wformat-security\"")
		  _Pragma("clang diagnostic warning \"-Wmissing-noreturn\"")
#endif

#ifdef MICRO_HEADER_ONLY
		inline void assert_always(const char* file, int line, const char* format, ...)
#else
		void assert_always(const char* file, int line, const char* format, ...)
#endif
		{
			char buffer[1024];
			int s = snprintf(buffer, 1024, "error in file %s at line %i: ", file, line);
			if (s < 1024) {
				char* st = buffer + s;
				va_list args;
				va_start(args, format);
				snprintf(st, static_cast<size_t>(1024 - s), format, args);
				va_end(args);
			}
			buffer[sizeof(buffer) - 1] = 0;
			printf(buffer);
			std::abort();
		}

#ifdef __clang__
		_Pragma("clang diagnostic pop")
#endif
	}
}
