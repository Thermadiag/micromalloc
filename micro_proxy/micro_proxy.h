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

/*
Replacing the standard memory allocation routines in Microsoft* C/C++ RTL
(malloc/free, global new/delete, etc.) with the MICRO memory allocator.

Include the following header to a source of any binary which is loaded during
application startup

#include "micro/micro_proxy.h"

or add following parameters to the linker options for the binary which is
loaded during application startup. It can be either exe-file or dll.


micro_proxy.lib /INCLUDE:"__MICRO_malloc_proxy"
*/

#ifndef __MICRO_PROXY_H
#define __MICRO_PROXY_H

#include "proxy_export.h"


/* Public Windows API */
MICRO_EXTERN_C int MICRO_malloc_replacement_log(char*** function_replacement_log_ptr);
// For debug purpose only
MICRO_EXTERN_C unsigned long long MICRO_peak_bytes();

#if _MSC_VER

#ifdef _DEBUG
#pragma comment(lib, "micro_proxy.lib")
#else
#pragma comment(lib, "micro_proxy.lib")
#endif

//#if defined(_WIN64)
#pragma comment(linker, "/include:__MICRO_malloc_proxy")
//#else
//#pragma comment(linker, "/include:___MICRO_malloc_proxy")
//#endif

#else
/* Primarily to support MinGW */
MICRO_EXTERN_C void __MICRO_malloc_proxy();
struct __MICRO_malloc_proxy_caller {
    __MICRO_malloc_proxy_caller() { __MICRO_malloc_proxy(); }
} volatile __MICRO_malloc_proxy_helper_object;

#endif // _MSC_VER


// Convenient defines, based on mimalloc

#ifndef MICRO_NO_MALLOC_DEFINE

#include "micro.h"

// Standard C allocation
#define malloc(n)               micro_malloc(n)
#define calloc(n,c)             micro_calloc(n,c)
#define realloc(p,n)            micro_realloc(p,n)
#define free(p)                 micro_free(p)

// Microsoft extensions
#define _expand(p,n)            micro_expand(p,n)
#define _msize(p)               micro_usable_size(p)
#define _recalloc(p,n,c)        micro_recalloc(p,n,c)

// Various Posix and Unix variants
#define reallocf(p,n)           micro_reallocf(p,n)
#define malloc_size(p)          micro_usable_size(p)
#define malloc_usable_size(p)   micro_usable_size(p)
#define malloc_good_size(sz)    micro_malloc_good_size(sz)
#define cfree(p)                micro_free(p)

#define valloc(n)               micro_valloc(n)
#define pvalloc(n)              micro_pvalloc(n)
#define reallocarray(p,s,n)     micro_reallocarray(p,s,n)
#define reallocarr(p,s,n)       micro_reallocarr(p,s,n)
#define memalign(a,n)           micro_memalign(a,n)
#define aligned_alloc(a,n)      micro_aligned_alloc(a,n)
#define posix_memalign(p,a,n)   micro_posix_memalign(p,a,n)
#define _posix_memalign(p,a,n)  micro_posix_memalign(p,a,n)

// Microsoft aligned variants
#define _aligned_malloc(n,a)                  micro_memalign(a,n)
#define _aligned_realloc(p,n,a)               micro_aligned_realloc(p,n,a)
#define _aligned_msize(p,a,o)                 micro_aligned_usable_size(p,a,o)
#define _aligned_free(p)                      micro_free(p)
//#define _aligned_offset_malloc(n,a,o)         mi_malloc_aligned_at(n,a,o)
//#define _aligned_offset_realloc(p,n,a,o)      mi_realloc_aligned_at(p,n,a,o)
//#define _aligned_offset_recalloc(p,s,n,a,o)   mi_recalloc_aligned_at(p,s,n,a,o)

#endif


#endif 

