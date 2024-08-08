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
 /*
     Copyright (c) 2005-2022 Intel Corporation

     Licensed under the Apache License, Version 2.0 (the "License");
     you may not use this file except in compliance with the License.
     You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

     Unless required by applicable law or agreed to in writing, software
     distributed under the License is distributed on an "AS IS" BASIS,
     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     See the License for the specific language governing permissions and
     limitations under the License.
 */

#ifndef _MICRO_malloc_proxy_H_
#define _MICRO_malloc_proxy_H_

#define MALLOC_UNIXLIKE_OVERLOAD_ENABLED __linux__
#define MALLOC_ZONE_OVERLOAD_ENABLED __APPLE__


#include "proxy_export.h"

// MALLOC_UNIXLIKE_OVERLOAD_ENABLED depends on MALLOC_CHECK_RECURSION stuff
// TODO: limit MALLOC_CHECK_RECURSION to *_OVERLOAD_ENABLED only
#if defined(__unix__) || defined(__APPLE__) || MALLOC_UNIXLIKE_OVERLOAD_ENABLED
#define MALLOC_CHECK_RECURSION 1
#endif

#include <stddef.h>

extern "C" {
    OVERRIDE_EXPORT void   __MICRO_malloc_safer_free( void *ptr, void (*original_free)(void*));
    OVERRIDE_EXPORT void * __MICRO_malloc_safer_realloc( void *ptr, size_t, void* );
    OVERRIDE_EXPORT void * __MICRO_malloc_safer_aligned_realloc( void *ptr, size_t, size_t, void* );
    OVERRIDE_EXPORT size_t __MICRO_malloc_safer_msize( void *ptr, size_t (*orig_msize_crt80d)(void*));
    OVERRIDE_EXPORT size_t __MICRO_malloc_safer_aligned_msize( void *ptr, size_t, size_t, size_t (*orig_msize_crt80d)(void*,size_t,size_t));

#if MALLOC_ZONE_OVERLOAD_ENABLED
    void   __MICRO_malloc_free_definite_size(void *object, size_t size);
#endif
} // extern "C"

// Struct with original free() and _msize() pointers
struct orig_ptrs {
    void   (*free) (void*);
    size_t (*msize)(void*);
};

struct orig_aligned_ptrs {
    void   (*aligned_free) (void*);
    size_t (*aligned_msize)(void*,size_t,size_t);
};

#endif /* _MICRO_malloc_proxy_H_ */
