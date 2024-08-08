/* ----------------------------------------------------------------------------
Copyright (c) 2018,2019 Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license.
-----------------------------------------------------------------------------*/

/* This is a stress test for the allocator, using multiple threads and
   transferring objects between threads. This is not a typical workload
   but uses a random linear size distribution. Do not use this test as a benchmark!
*/
#include <cstddef>

typedef void* (*CUSTOM_MALLOC_P)(size_t);
typedef void (*CUSTOM_FREE_P)(void*);
typedef void* (*CUSTOM_REALLOC_P)(void*,size_t);
static CUSTOM_MALLOC_P CUSTOM_MALLOC;
static CUSTOM_FREE_P CUSTOM_FREE;
static CUSTOM_REALLOC_P CUSTOM_REALLOC;

static void* test_malloc(size_t size)
{
    return CUSTOM_MALLOC(size);
}

static void* test_realloc(void * p, size_t size)
{
    return CUSTOM_REALLOC(p,size);
}

static void bench_start_program() {}
static void bench_end_program() {}
static void bench_start_thread() {}
static void bench_end_thread() {}

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdbool>
#include <cstring>


#include <micro/micro.h>
#include <micro/testing.hpp>
#include <iostream>

// > mstress [THREADS] [SCALE] [ITER]
//
// argument defaults
static int THREADS = 8;      // more repeatable if THREADS <= #processors
static int SCALE = 50;      // scaling factor
static int ITER = 50;      // N full iterations re-creating all threads

// static int THREADS = 8;    // more repeatable if THREADS <= #processors
// static int SCALE   = 100;  // scaling factor

static bool   allow_large_objects = true;    // allow very large objects?
static size_t use_one_size = 0;              // use single object size of N uintptr_t?

// transfer pointer between threads
#define TRANSFERS     (1000)
static volatile void* transfer[TRANSFERS];

#if (INTPTR_MAX != UINT32_MAX)
const uintptr_t cookie = 0xbf58476d1ce4e5b9UL;
#else
const uintptr_t cookie = 0x1ce4e5b9UL;
#endif

static void* atomic_exchange_ptr(volatile void** p, void* newval);

typedef uintptr_t* random_t;

static uintptr_t pick(random_t r) {
    uintptr_t x = *r;
#if (INTPTR_MAX > UINT32_MAX)
    // by Sebastiano Vigna, see: <http://xoshiro.di.unimi.it/splitmix64.c>
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9UL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebUL;
    x ^= x >> 31;
#else
    // by Chris Wellons, see: <https://nullprogram.com/blog/2018/07/31/>
    x ^= x >> 16;
    x *= 0x7feb352dUL;
    x ^= x >> 15;
    x *= 0x846ca68bUL;
    x ^= x >> 16;
#endif
    * r = x;
    return x;
}

static bool chance(int perc, random_t r) {
    return (pick(r) % 100 <= perc);
}

static void* alloc_items(size_t items, random_t r) {
    if (chance(1, r)) {
        if (chance(1, r) && allow_large_objects) items *= 10000;       // 0.01% giant
        else if (chance(10, r) && allow_large_objects) items *= 1000;  // 0.1% huge
        else items *= 100;                                             // 1% large objects;
    }
    if (items == 40) items++;              // pthreads uses that size for stack increases
    if (use_one_size > 0) items = (use_one_size / sizeof(uintptr_t));
    uintptr_t* p = (uintptr_t*)CUSTOM_MALLOC(items * sizeof(uintptr_t));
    if (p != NULL) {
        for (uintptr_t i = 0; i < items; i++) p[i] = (items - i) ^ cookie;
    }
    return p;
}

static void free_items(void* p) {
    if (p != NULL) {
        uintptr_t* q = (uintptr_t*)p;
        uintptr_t items = (q[0] ^ cookie);
        for (uintptr_t i = 0; i < items; i++) {
            if ((q[i] ^ cookie) != items - i) {
                fprintf(stderr, "memory corruption at block %p at %zu\n", p, i);
                abort();
            }
        }
    }
    CUSTOM_FREE(p);
}


static void stress(intptr_t tid) {
    bench_start_thread();
    uintptr_t r = ((tid + 1) * 43);
    const size_t max_item_shift = 5; // 128
    const size_t max_item_retained_shift = max_item_shift + 2;
    size_t allocs = 100 * ((size_t)SCALE) * (tid % 8 + 1); // some threads do more
    size_t retain = allocs / 2;
    void** data = NULL;
    size_t data_size = 0;
    size_t data_top = 0;
    void** retained = (void**)test_malloc(retain * sizeof(void*));
    size_t retain_top = 0;

    while (allocs > 0 || retain > 0) {
        if (retain == 0 || (chance(50, &r) && allocs > 0)) {
            // 50%+ alloc
            allocs--;
            if (data_top >= data_size) {
                data_size += 100000;
                data = (void**)test_realloc(data, data_size * sizeof(void*));
            }
            data[data_top++] = alloc_items(1ULL << (pick(&r) % max_item_shift), &r);
        }
        else {
            // 25% retain
            retained[retain_top++] = alloc_items(1ULL << (pick(&r) % max_item_retained_shift), &r);
            retain--;
        }
        if (chance(66, &r) && data_top > 0) {
            // 66% free previous alloc
            size_t idx = pick(&r) % data_top;
            free_items(data[idx]);
            data[idx] = NULL;
        }
        if (chance(25, &r) && data_top > 0) {
            // 25% exchange a local pointer with the (shared) transfer buffer.
            size_t data_idx = pick(&r) % data_top;
            size_t transfer_idx = pick(&r) % TRANSFERS;
            void* p = data[data_idx];
            void* q = atomic_exchange_ptr(&transfer[transfer_idx], p);
            data[data_idx] = q;
        }
    }
    // free everything that is left
    for (size_t i = 0; i < retain_top; i++) {
        free_items(retained[i]);
    }
    for (size_t i = 0; i < data_top; i++) {
        free_items(data[i]);
    }
    CUSTOM_FREE(retained);
    CUSTOM_FREE(data);
    bench_end_thread();
}

static void run_os_threads(size_t nthreads);



void launch_test(const char* allocator)
{
    micro::tick();
    // Run ITER full iterations where half the objects in the transfer buffer survive to the next round.
    uintptr_t r = 43 * 43;
    for (int n = 0; n < ITER; n++) {
        run_os_threads(THREADS);
        for (int i = 0; i < TRANSFERS; i++) {
            if (chance(50, &r) || n + 1 == ITER) { // free all on last run, otherwise free half of the transfers
                void* p = atomic_exchange_ptr(&transfer[i], NULL);
                free_items(p);
            }
        }
        //mi_collect(false);
#ifndef NDEBUG
        //if ((n + 1) % 10 == 0) { printf("- iterations: %3d\n", n + 1); }
#endif
    }

    micro::allocator_trim(allocator);

    size_t el = micro::tock_ms();
    std::cout << allocator <<" " << el << " ms" << std::endl;
    micro::print_process_infos();
}

int mstress(int argc, char** const argv) {
    // > mstress [THREADS] [SCALE] [ITER]
    if (argc >= 2) {
        char* end;
        long n = strtol(argv[1], &end, 10);
        if (n > 0) THREADS = n;
    }
    if (argc >= 3) {
        char* end;
        long n = (strtol(argv[2], &end, 10));
        if (n > 0) SCALE = n;
    }
    if (argc >= 4) {
        char* end;
        long n = (strtol(argv[3], &end, 10));
        if (n > 0) ITER = n;
    }
    printf("start with %d threads with a %d%% load-per-thread and %d iterations\n", THREADS, SCALE, ITER);
   
    {
        //micro_set_parameter(MicroProviderType, MicroOSPreallocProvider);
        //micro_set_parameter(MicroAllowOsPageAlloc, 1);
        //micro_set_string_parameter(MicroPrintStats, "stdout");
        //micro_set_parameter(MicroPrintStatsTrigger, MicroOnExit);
        
        CUSTOM_MALLOC = micro_malloc;
        CUSTOM_FREE = micro_free;
        CUSTOM_REALLOC = micro_realloc;
        launch_test("micro");
        //return 0;
    }



#ifdef MICRO_BENCH_MALLOC

    CUSTOM_MALLOC = malloc;
    CUSTOM_FREE = free;
    CUSTOM_REALLOC = realloc;
    launch_test("malloc");
#endif

#ifdef MICRO_BENCH_JEMALLOC
    //const char* je_malloc_conf = "dirty_decay_ms:0";
    CUSTOM_MALLOC = je_malloc;
    CUSTOM_FREE = je_free;
    CUSTOM_REALLOC = je_realloc;
    launch_test("jemalloc");

#endif

#ifdef MICRO_BENCH_SNMALLOC

    CUSTOM_MALLOC = snmalloc::libc::malloc;
    CUSTOM_FREE = snmalloc::libc::free;
    CUSTOM_REALLOC = snmalloc::libc::realloc;
    launch_test("snmalloc");
#endif

#ifdef MICRO_BENCH_MIMALLOC
    CUSTOM_MALLOC = mi_malloc;
    CUSTOM_FREE = mi_free;
    CUSTOM_REALLOC = mi_realloc;
    launch_test("mimalloc");
    
#endif

#ifdef USE_TBB
    CUSTOM_MALLOC = scalable_malloc;
    CUSTOM_FREE = scalable_free;
    CUSTOM_REALLOC = scalable_realloc;
    launch_test("onetbb");
#endif
    

    return 0;
}


#ifdef _WIN32

#include <windows.h>

static DWORD WINAPI thread_entry(LPVOID param) {
    stress(/*(intptr_t)param*/123456789);
    return 0;
}




static void run_os_threads(size_t nthreads) {
    DWORD* tids = (DWORD*)CUSTOM_MALLOC(nthreads * sizeof(DWORD));
    HANDLE* thandles = (HANDLE*)CUSTOM_MALLOC(nthreads * sizeof(HANDLE));
    for (uintptr_t i = 1; i < nthreads; i++) {
        thandles[i] = CreateThread(0, 4096, &thread_entry, (void*)(i), 0, &tids[i]);
    }
    thread_entry((void*)0); // main runs as well
    for (size_t i = 1; i < nthreads; i++) {
        WaitForSingleObject(thandles[i], INFINITE);
    }
    for (size_t i = 1; i < nthreads; i++) {
        CloseHandle(thandles[i]);
    }
    CUSTOM_FREE(tids);
    CUSTOM_FREE(thandles);
}

static void* atomic_exchange_ptr(volatile void** p, void* newval) {
#if (INTPTR_MAX == UINT32_MAX)
    return (void*)InterlockedExchange((volatile LONG*)p, (LONG)newval);
#else
    return (void*)InterlockedExchange64((volatile LONG64*)p, (LONG64)newval);
#endif
}
#else

#include <pthread.h>
//#include <stdatomic.h>
#include <atomic>

static void* thread_entry(void* param) {
    stress(/*(uintptr_t)param*/123456789);
    return NULL;
}

static void run_os_threads(size_t nthreads) {
    pthread_t* threads = (pthread_t*)CUSTOM_MALLOC(nthreads * sizeof(pthread_t));
    memset(threads, 0, sizeof(pthread_t) * nthreads);
    //pthread_setconcurrency(nthreads);
    for (uintptr_t i = 1; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, &thread_entry, (void*)i);
    }
    thread_entry((void*)0); // main runs as well
    for (size_t i = 1; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    CUSTOM_FREE(threads);
}

static void* atomic_exchange_ptr(volatile void** p, void* newval) {
volatile std::atomic<void*> * a = (std::atomic<void*> *)p;
return a->exchange(newval);
    //return atomic_exchange_explicit((volatile _Atomic(void*)*)p, newval, memory_order_acquire);
}

#endif
