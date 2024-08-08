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

#ifndef MICRO_LOGGER_HPP
#define MICRO_LOGGER_HPP

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <ctime>

#include "lock.hpp"
extern "C" {
#include "enums.h"
}

MICRO_PUSH_DISABLE_OLD_STYLE_CAST

namespace micro
{
	namespace detail
	{
		template<class T>
		void print_float(T fVal, FILE* out) noexcept
		{
			// Print floating point value.
			// Does not work for all values: nan and infinity are not supported,
			// Very big values are not supported as well.

			static const int64_t precision = 1000;
			if (fVal < 0) {
				putc('-', out);
				fVal = -fVal;
			}
			if (fVal == 0) {
				putc('0', out);
				return;
			}

			// Basic float print without sprintf
			// https://stackoverflow.com/questions/23191203/convert-float-to-string-without-sprintf
			char result[100];
			int64_t dVal, dec, i;

			fVal += static_cast<T>(5. / (precision * 10)); // round up

			dVal = static_cast<int64_t>(fVal);
			dec = static_cast<int64_t>(fVal * precision) % precision;

			memset(result, 0, 100);
			result[0] = static_cast<char>((dec % 10) + '0');
			result[1] = static_cast<char>((dec / 10) + '0');
			result[2] = '.';

			i = 3;
			while (dVal > 0) {
				result[i] = static_cast<char>((dVal % 10) + '0');
				dVal /= 10;
				i++;
			}

			for (i = static_cast<int64_t>( strlen(result))- 1; i >= 0; i--)
				putc(result[i], out);
		}

		template<class T>
		typename std::enable_if<std::is_signed<T>::value, T>::type abs_integer(T val) noexcept
		{
			return -val;
		}
		template<class T>
		T abs_integer(T val) noexcept
		{
			return val;
		}

		template<class T>
		void print_integer(T iVal, FILE* out) noexcept
		{
			// Basic integer printing
			// Works for all types of integer

			char buffer[100];
			char* start = buffer;

			if (iVal < 0) {
				iVal = abs_integer(iVal);
				putc('-', out);
			}
			if (iVal == 0) {
				putc('0', out);
				return;
			}

			while (iVal) {
				*start++ = '0' + iVal % 10;
				iVal /= 10;
			}
			while (start-- > buffer) {
				putc(*start, out);
			}
		}

		inline void print_string(const char* str, FILE* out) noexcept
		{
			// Basic string printing
			if (str) {
				while (*str) {
					putc(*str, out);
					++str;
				}
			}
		}

		inline void print_safe_value(const char* s, FILE* out) noexcept { print_string(s, out); }
		inline void print_safe_value(float v, FILE* out) noexcept { print_float(v, out); }
		inline void print_safe_value(double v, FILE* out) noexcept { print_float(v, out); }
		inline void print_safe_value(long double v, FILE* out) noexcept { print_float(static_cast<double>(v), out); }
		template<class T>
		inline void print_safe_value(T v, FILE* out) noexcept
		{
			print_integer(v, out);
		}

		template<size_t Count>
		struct Writer
		{
			template<class A, class... Args>
			static void write(FILE* out, A a, Args&&... args) noexcept
			{
				print_safe_value(a, out);
				Writer<Count - 1>::write(out, std::forward<Args>(args)...);
			}
		};
		template<>
		struct Writer<0>
		{
			template<class... Args>
			static void write(FILE* out, Args&&... args) noexcept
			{
			}
		};
	}

	/// @brief Basic but safe printing that never triggers allocation
	/// @param out output stream
	template<class... Args>
	inline void print_safe(FILE* out, Args&&... args) noexcept
	{
		detail::Writer<sizeof...(Args)>::write(out, std::forward<Args>(args)...);
	}

	/// @brief Callback function passed to micro::print_generic()
	typedef void (*print_callback_type)(void* /*opaque*/, const char* /*str*/);

	/// @brief Default callback function, cast opaque parameter as a FILE*
	inline void default_print_callback(void* opaque, const char* str) noexcept
	{
		// write to output stream
		MICRO_WARN_FORMAT(fprintf(static_cast<FILE*>(opaque), str))
	}

	namespace detail
	{
		static inline size_t format_current_date_time(char* out, size_t out_len, const char* format)
		{
			// Format current date time, might trigger an allocation
			time_t t = std::time(nullptr);
			struct tm* tm = std::localtime(&t);
			return std::strftime(out, out_len, format, tm);
		}

		/// @brief Generic thread safe (but not allocation free) print function
		inline void print_generic_internal(print_callback_type callback, void* opaque, micro_log_level l, const char* date_format, const char* format, va_list args) noexcept
		{
			static std::atomic<bool> recurse{ false };

			// Drop recursive calls
			if (recurse.exchange(true))
				return;

			char buf[1024];
			char* start = buf;

			// Add date
			if (date_format && date_format[0]) {
				size_t len = detail::format_current_date_time(start, sizeof(buf), date_format);
				if (len < sizeof(buf)) {
					buf[len] = '\t';
					start = buf + len + 1;
				}
			}

			// Add level
			if (l != MicroNoLog) {
				if (l == MicroCritical)
					start += snprintf(start, sizeof(buf) - 1 - static_cast<size_t>(start - buf), "Critical\t");
				else if (l == MicroWarning)
					start += snprintf(start, sizeof(buf) - 1 - static_cast<size_t>(start - buf), "Warning\t");
				else
					start += snprintf(start, sizeof(buf) - 1 - static_cast<size_t>(start - buf), "Info\t");
			}

			// Add message
			if (start < buf + sizeof(buf)) {
				MICRO_WARN_FORMAT(vsnprintf(start, sizeof(buf) - 1 - static_cast<size_t>(start - buf), format, args))

				buf[sizeof(buf) - 1] = 0;
				callback(opaque, buf);
			}

			recurse.store(false);
		}
	}

	inline void print_generic(print_callback_type callback, void* opaque, micro_log_level l, const char* date_format, const char* format, ...) noexcept
	{
		va_list args;
		va_start(args, format);
		detail::print_generic_internal(callback, opaque, l, date_format, format, args);
		va_end(args);
	}

	inline void print_stdout(micro_log_level l, const char* date_format, const char* format, ...) noexcept
	{
		va_list args;
		va_start(args, format);
		detail::print_generic_internal(default_print_callback, stdout, l, date_format, format, args);
		va_end(args);
	}
	inline void print_stderr(micro_log_level l, const char* date_format, const char* format, ...) noexcept
	{
		va_list args;
		va_start(args, format);
		detail::print_generic_internal(default_print_callback, stderr, l, date_format, format, args);
		va_end(args);
	}

} // end namespace micro

MICRO_POP_DISABLE_OLD_STYLE_CAST

#endif
