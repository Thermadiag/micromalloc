///-*-C++-*-//////////////////////////////////////////////////////////////////
//
// Hoard: A Fast, Scalable, and Memory-Efficient Allocator
//        for Shared-Memory Multiprocessors
// Contact author: Emery Berger, http://www.cs.umass.edu/~emery
//
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as
// published by the Free Software Foundation, http://www.fsf.org.
//
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @file cache-scratch.cpp
 *
 * cache-scratch is a benchmark that exercises a heap's cache locality.
 * An allocator that allows multiple threads to re-use the same small
 * object (possibly all in one cache-line) will scale poorly, while
 * an allocator like Hoard will exhibit near-linear scaling.
 *
 * Try the following (on a P-processor machine):
 *
 *  cache-scratch 1 1000 1 1000000 P
 *  cache-scratch P 1000 1 1000000 P
 *
 *  cache-scratch-hoard 1 1000 1 1000000 P
 *  cache-scratch-hoard P 1000 1 1000000 P
 *
 *  The ideal is a P-fold speedup.
*/

#include <iostream>
#include <stdlib.h>
#include <thread>
#include <vector>

#include <micro/micro.h>
#include <micro/testing.hpp>
#include <iostream>

typedef void* (*CUSTOM_MALLOC_P)(size_t);
typedef void (*CUSTOM_FREE_P)(void*);
static CUSTOM_MALLOC_P CUSTOM_MALLOC;
static CUSTOM_FREE_P CUSTOM_FREE;


// This class just holds arguments to each thread.
class workerArg {
public:

    workerArg() {}

    workerArg(char* obj, int objSize, int repetitions, int iterations)
        : _object(obj),
        _objSize(objSize),
        _iterations(iterations),
        _repetitions(repetitions)
    {}

    char* _object;
    int _objSize;
    int _iterations;
    int _repetitions;
};


#if defined(_WIN32)
extern "C" void worker(void* arg)
#else
extern "C" void* worker(void* arg)
#endif
{
    // free the object we were given.
    // Then, repeatedly do the following:
    //   malloc a given-sized object,
    //   repeatedly write on it,
    //   then free it.
    workerArg* w = (workerArg*)arg;
    CUSTOM_FREE( w->_object);
    workerArg w1 = *w;
    for (int i = 0; i < w1._iterations; i++) {
        // Allocate the object.
        char* obj = (char*)CUSTOM_MALLOC(w1._objSize);
        // Write into it a bunch of times.
        for (int j = 0; j < w1._repetitions; j++) {
            for (int k = 0; k < w1._objSize; k++) {
                obj[k] = (char)k;
                volatile char ch = obj[k];
                ch++;
            }
        }
        // Free the object.
        CUSTOM_FREE(obj);
    }

#if !defined(_WIN32)
    return NULL;
#endif
}




int bench(const char * name)
{
    micro::tick();
    //cache-scratch 1 1000 1 1000000 P
    int nthreads = 10;
    int iterations = 2000;
    int objSize = 1;
    int repetitions = 1000000;

    /*if (argc > 5) {
        nthreads = atoi(argv[1]);
        iterations = atoi(argv[2]);
        objSize = atoi(argv[3]);
        repetitions = atoi(argv[4]);
        concurrency = atoi(argv[5]);
    }
    else {
        cout << "Usage: " << argv[0] << " nthreads iterations objSize repetitions concurrency" << endl;
        exit(1);
    }*/

    std::vector<std::thread> threads(nthreads);
    
    workerArg* w = (workerArg*)CUSTOM_MALLOC( sizeof(workerArg) * nthreads);
    for (int j = 0; j < nthreads; ++j)
        new (w + j) workerArg();

    int i;

    // Allocate nthreads objects and distribute them among the threads.
    char** objs = (char**)CUSTOM_MALLOC(nthreads * sizeof(char*)); 
    for (i = 0; i < nthreads; i++) {
        objs[i] = (char*)CUSTOM_MALLOC(objSize);
    }


    for (i = 0; i < nthreads; i++) {
        w[i] = workerArg(objs[i], objSize, repetitions / nthreads, iterations);
        threads[i] = std::thread([w,i]() {worker((void*)&w[i]); });
        //worker((void*)&w[i]);
    }
    for (i = 0; i < nthreads; i++) {
        threads[i].join();
    }

    CUSTOM_FREE(objs);
    CUSTOM_FREE(w);

    micro::allocator_trim(name);

    size_t el = micro::tock_ms();
    std::cout << name << ": " << el << " ms" << std::endl;
    micro::print_process_infos();
    return 0;
}

int cache_scratch(int, char** const)
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
        bench("micro");
        micro_clear();
    }
#endif

#ifdef MICRO_BENCH_MALLOC
    CUSTOM_MALLOC = malloc;
    CUSTOM_FREE = free;
    bench("malloc");
    malloc_trim(0);
#endif

#ifdef MICRO_BENCH_JEMALLOC
   // const char* je_malloc_conf = "dirty_decay_ms:0";
    CUSTOM_MALLOC = je_malloc;
    CUSTOM_FREE = je_free;
    bench("jemalloc");
#endif

#ifdef MICRO_BENCH_MIMALLOC
    CUSTOM_MALLOC = mi_malloc;
    CUSTOM_FREE = mi_free;
    bench("mimalloc");
    mi_heap_collect(mi_heap_get_default(), true);
#endif

#ifdef MICRO_BENCH_SNMALLOC
    CUSTOM_MALLOC = snmalloc::libc::malloc;
    CUSTOM_FREE = snmalloc::libc::free;
    bench("snmalloc");
#endif

#ifdef USE_TBB
    CUSTOM_MALLOC = scalable_malloc;
    CUSTOM_FREE = scalable_free;
    bench("onetbb");
#endif



    return 0;
}