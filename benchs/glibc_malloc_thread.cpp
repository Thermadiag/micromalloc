/* Benchmark malloc and free functions.
   Copyright (C) 2013-2021 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <vector>

#include <micro/micro.h>
#include <micro/testing.hpp>
#include <micro/os_timer.hpp>
#include <iostream>

typedef void* (*CUSTOM_MALLOC_P)(size_t);
typedef void (*CUSTOM_FREE_P)(void*);
typedef void* (*CUSTOM_REALLOC_P)(void*, size_t);
static CUSTOM_MALLOC_P CUSTOM_MALLOC;
static CUSTOM_FREE_P CUSTOM_FREE;
static CUSTOM_REALLOC_P CUSTOM_REALLOC;


   // #include "bench-timing.h"
   // #include "json-lib.h"

   /* Benchmark duration in seconds.  */
#define BENCHMARK_DURATION	2
#define RAND_SEED		88


/* Maximum memory that can be allocated at any one time is:

   NUM_THREADS * WORKING_SET_SIZE * MAX_ALLOCATION_SIZE

   However due to the distribution of the random block sizes
   the typical amount allocated will be much smaller.  */
#define WORKING_SET_SIZE    1024

#define MIN_ALLOCATION_SIZE	4
#define MAX_ALLOCATION_SIZE	32768
#define NUM_THREADS 8

   /* Get a random block size with an inverse square distribution.  */
static unsigned int
get_block_size(unsigned int rand_data)
{
    /* Inverse square.  */
    const float exponent = -2;
    /* Minimum value of distribution.  */
    const float dist_min = MIN_ALLOCATION_SIZE;
    /* Maximum value of distribution.  */
    const float dist_max = MAX_ALLOCATION_SIZE;

    float min_pow = powf(dist_min, exponent + 1);
    float max_pow = powf(dist_max, exponent + 1);

    float r = (float)rand_data / RAND_MAX;

    return (unsigned int)powf((max_pow - min_pow) * r + min_pow,
        1 / (exponent + 1));
}

#define NUM_BLOCK_SIZES	8000
#define NUM_OFFSETS	((WORKING_SET_SIZE) * 4)

static unsigned int random_block_sizes[NUM_BLOCK_SIZES];
static unsigned int random_offsets[NUM_OFFSETS];

static void
init_random_values(void)
{
    for (size_t i = 0; i < NUM_BLOCK_SIZES; i++)
        random_block_sizes[i] = get_block_size(rand());

    for (size_t i = 0; i < NUM_OFFSETS; i++)
        random_offsets[i] = rand() % WORKING_SET_SIZE;
}

static unsigned int
get_random_block_size(unsigned int* state)
{
    unsigned int idx = *state;

    if (idx >= NUM_BLOCK_SIZES - 1)
        idx = 0;
    else
        idx++;

    *state = idx;

    return random_block_sizes[idx];
}

static unsigned int
get_random_offset(unsigned int* state)
{
    unsigned int idx = *state;

    if (idx >= NUM_OFFSETS - 1)
        idx = 0;
    else
        idx++;

    *state = idx;

    return random_offsets[idx];
}

static bool stop = false;

/* Allocate and free blocks in a random order.  */
static size_t
malloc_benchmark_loop(void** ptr_arr)
{
    unsigned int offset_state = 0, block_state = 0;
    size_t iters = 0;

    while (!stop)
    {
        unsigned int next_idx = get_random_offset(&offset_state);
        unsigned int next_block = get_random_block_size(&block_state);

        CUSTOM_FREE(ptr_arr[next_idx]);

        void* p = ptr_arr[next_idx] = CUSTOM_MALLOC(next_block);

        // touch the allocated memory
        memset(p, 0, next_block);
        iters++;
    }

    return iters;
}

struct thread_args
{
    size_t iters;
    void** working_set;
};

static void*
benchmark_thread(void* arg)
{
    struct thread_args* args = (struct thread_args*)arg;
    size_t iters;
    void** thread_set = args->working_set;
    
    iters = malloc_benchmark_loop(thread_set);
    
    args->iters = iters;

    return NULL;
}

static int
do_benchmark( size_t* iters)
{

    
    {
        struct thread_args args[NUM_THREADS];
        void* working_set[NUM_THREADS][WORKING_SET_SIZE];
        std::thread threads[NUM_THREADS];

        memset(working_set, 0, sizeof(working_set));

        *iters = 0;

        for (size_t i = 0; i < NUM_THREADS; i++)
        {
            args[i].working_set = working_set[i];
            threads[i] = std::thread([&args, i]() {benchmark_thread(args + i); });
        }

        for (size_t i = 0; i < NUM_THREADS; i++)
        {
            threads[i].join();
            *iters += args[i].iters;
        }
    }
    return 0;
}



static int bench(const char * name)
{
    size_t iters = 0;
    double d_total_s, d_total_i;
    stop = false;

    static bool init = false;
    if (!init) {
        init_random_values();
        init = true;
    }
    
    micro::tick();

    
    struct thread_args args[NUM_THREADS];
    void* working_set[NUM_THREADS][WORKING_SET_SIZE];
    std::thread threads[NUM_THREADS];

    memset(working_set, 0, sizeof(working_set));


    for (size_t i = 0; i < NUM_THREADS; i++)
    {
        args[i].working_set = working_set[i];
        threads[i] = std::thread([&args, i]() {benchmark_thread(args + i); });
    }

    

    while (true) {
        size_t el = micro::tock_ms();
        if (el / 1000. >= BENCHMARK_DURATION)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds( 10));
    }

    stop = true;
    for (size_t i = 0; i < NUM_THREADS; i++)
    {
        threads[i].join();
        iters += args[i].iters;
    }
    micro::timer t;
    t.tick();
    micro::allocator_trim(name);
    auto el = t.tock();
    double el_s = el / 1000000000.;
    iters = (size_t)(iters * ((double)BENCHMARK_DURATION / ((double)BENCHMARK_DURATION + el_s)));


    std::cout << name << ": " << iters << " iterations" << std::endl;
    micro::print_process_infos();
    std::cout <<  std::endl;
    return 0;
}


int glibc_malloc_thread(int, char** const)
{
#ifdef MICRO_BENCH_MICROMALLOC
    {
        //micro_set_parameter(MicroProviderType, MicroOSPreallocProvider);
        //micro_set_parameter(MicroAllowOsPageAlloc, 1);
        //micro_set_parameter(MicroPageMemorySize, 600000000ull);
        //micro_set_string_parameter(MicroPrintStats, "stdout");
        //micro_set_parameter(MicroPrintStatsTrigger, MicroOnExit);
        CUSTOM_MALLOC = micro_malloc;
        CUSTOM_FREE = micro_free;
        CUSTOM_REALLOC = micro_realloc;
        bench("micro");
    }
#endif

#ifdef MICRO_BENCH_MALLOC

    CUSTOM_MALLOC = malloc;
    CUSTOM_FREE = free;
    CUSTOM_REALLOC = realloc;
    bench("malloc");
#endif

#ifdef MICRO_BENCH_JEMALLOC
    //const char* je_malloc_conf = "dirty_decay_ms:0";
    CUSTOM_MALLOC = je_malloc;
    CUSTOM_FREE = je_free;
    CUSTOM_REALLOC = je_realloc;
    bench("jemalloc");

#endif

#ifdef MICRO_BENCH_MIMALLOC
    CUSTOM_MALLOC = mi_malloc;
    CUSTOM_FREE = mi_free;
    CUSTOM_REALLOC = mi_realloc;
    bench("mimalloc");
   
#endif

#ifdef MICRO_BENCH_SNMALLOC
    CUSTOM_MALLOC = snmalloc::libc::malloc;
    CUSTOM_FREE = snmalloc::libc::free;
    bench("snmalloc");
#endif

#ifdef USE_TBB
    CUSTOM_MALLOC = scalable_malloc;
    CUSTOM_FREE = scalable_free;
    CUSTOM_REALLOC = scalable_realloc;
    bench("onetbb");
#endif

    return 0;
}
