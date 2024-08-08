
#ifndef LARSON_H
#define LARSON_H

#include <micro/micro.h>
#include <micro/testing.hpp>

#include <vector>
#include <thread> 
#include <iostream>


#ifdef BENCH_MIMALLOC
#include <mimalloc.h>
#endif

#if defined(USE_HOARD) && defined(_WIN32)
#pragma comment(lib, "libhoard.lib")
#endif

#if defined(USE_RX) && defined(_WIN32)
#pragma comment(lib, "libmi.lib")
#endif

#include <assert.h>
#include <stdio.h>

#if defined(_WIN32)
#define __WIN32__
#endif

#ifdef __WIN32__
#include  <windows.h>
#include  <conio.h>
#include  <process.h>

#else
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>

#ifndef __SVR4
//extern "C" int pthread_setconcurrency (int) throw();
#include <pthread.h>
#endif

typedef void* LPVOID;
typedef long long LONGLONG;
typedef long DWORD;
typedef long LONG;
typedef unsigned long ULONG;
typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG  HighPart;
    } foo;
    LONGLONG QuadPart;    // In Visual C++, a typedef to _ _int64} LARGE_INTEGER;
} LARGE_INTEGER;
typedef long long _int64;
#ifndef TRUE
enum { TRUE = 1, FALSE = 0 };
#endif
#include <assert.h>
#define _ASSERTE(x) assert(x)
#define _inline inline
void Sleep(long x)
{
    //  printf ("sleeping for %ld seconds.\n", x/1000);
    sleep(x / 1000);
}

void QueryPerformanceCounter(long* x)
{
    struct timezone tz;
    struct timeval tv;
    gettimeofday(&tv, &tz);
    *x = tv.tv_sec * 1000000L + tv.tv_usec;
}

void QueryPerformanceFrequency(long* x)
{
    *x = 1000000L;
}


#include  <stdio.h>
#include  <stdlib.h>
#include  <stddef.h>
#include  <string.h>
#include  <ctype.h>
#include  <time.h>
#include  <assert.h>

#define _REENTRANT 1
#include <pthread.h>
#ifdef __sun
#include <thread.h>
#endif
typedef void* VoidFunction(void*);
void _beginthread(VoidFunction x, int, void* z)
{
    pthread_t pt;
    pthread_attr_t pa;
    pthread_attr_init(&pa);

#if 1//defined(__SVR4)
    pthread_attr_setscope(&pa, PTHREAD_SCOPE_SYSTEM); /* bound behavior */
#endif

  //  printf ("creating a thread.\n");
    int v = pthread_create(&pt, &pa, x, z);
    //  printf ("v = %d\n", v);
}
#endif


#if 0
static char buf[65536];

#define malloc(v) &buf
#define free(p)
#endif

//#undef CPP
//#define CPP
//#include "arch-specific.h"

#if USE_ROCKALL
//#include "FastHeap.hpp"
//FAST_HEAP theFastHeap (1024 * 1024, true, true, true);

typedef int SBIT32;

#include "SmpHeap.hpp"
SMP_HEAP theFastHeap(1024 * 1024, true, true, true);

void* operator new(unsigned int cb)
{
    void* pRet = theFastHeap.New((size_t)cb);
    return pRet;
}

void operator delete[](void* pUserData)
{
    theFastHeap.Delete(pUserData);
}
#endif

#if 0
extern "C" void* hdmalloc(size_t sz);
extern "C" void hdfree(void* ptr);
extern "C" void hdmalloc_stats(void);
void* operator new(unsigned int cb)
{
    void* pRet = hdmalloc((size_t)cb);
    return pRet;
}

void operator delete[](void* pUserData)
{
    hdfree(pUserData);
}
#endif




/* Test driver for memory allocators           */
/* Author: Paul Larson, palarson@microsoft.com */
#define MAX_THREADS     100
#define MAX_BLOCKS  1000000



struct lran2_st {
    long x, y, v[97];
};


typedef struct thr_data {

    int    threadno;
    int    NumBlocks;
    int    seed;

    int    min_size;
    int    max_size;

    char** array;
    size_t* blksize;
    int     asize;

    long   cAllocs;
    long   cFrees;
    long   cThreads;
    long   cBytesAlloced;

    volatile int finished;
    struct lran2_st rgen;

} thread_data;



/* lran2.h
 * by Wolfram Gloger 1996.
 *
 * A small, portable pseudo-random number generator.
 */

#ifndef _LRAN2_H
#define _LRAN2_H

#define LRAN2_MAX 714025l /* constants for portable */
#define IA	  1366l	  /* random number generator */
#define IC	  150889l /* (see e.g. `Numerical Recipes') */

 //struct lran2_st {
 //    long x, y, v[97];
 //};

static void
lran2_init(struct lran2_st* d, long seed)
{
    long x;
    int j;

    x = (IC - seed) % LRAN2_MAX;
    if (x < 0) x = -x;
    for (j = 0; j < 97; j++) {
        x = (IA * x + IC) % LRAN2_MAX;
        d->v[j] = x;
    }
    d->x = (IA * x + IC) % LRAN2_MAX;
    d->y = d->x;
}

static
long lran2(struct lran2_st* d)
{
    int j = (d->y % 97);

    d->y = d->v[j];
    d->x = (IA * d->x + IC) % LRAN2_MAX;
    d->v[j] = d->x;
    return d->y;
}

#undef IA
#undef IC

#endif

struct Params
{
    typedef void* (*malloc_type)(size_t);
    typedef void (*free_type)(void*);

    malloc_type _malloc;
    free_type _free;

    int volatile  stopflag = FALSE;
    char* blkp[MAX_BLOCKS];
    size_t          blksize[MAX_BLOCKS];
    long            seqlock = 0;
    struct lran2_st rgen;
    int             min_size = 10, max_size = 500;
    int             num_threads;
};

#if defined(_WIN32)
extern "C" {
    extern HANDLE crtheap;
};
#endif




static void runloops(Params * p, long sleep_cnt, int num_chunks)
{
    int     cblks;
    int     victim;
    size_t  blk_size;
#ifdef __WIN32__
    _LARGE_INTEGER ticks_per_sec, start_cnt, end_cnt;
#else
    long ticks_per_sec;
    long start_cnt, end_cnt;
#endif
    _int64        ticks;
    double        duration;
    double        reqd_space;
    ULONG         used_space = 0;
    long          sum_allocs = 0;

    QueryPerformanceFrequency(&ticks_per_sec);
    QueryPerformanceCounter(&start_cnt);

    for (cblks = 0; cblks < num_chunks; cblks++) {
        if (max_size == min_size) {
            blk_size = min_size;
        }
        else {
            blk_size = min_size + lran2(&rgen) % (max_size - min_size);
        }
#ifdef CPP
        blkp[cblks] = new char[blk_size];
#else
        blkp[cblks] = (char*)p->_malloc(blk_size);
#endif
        blksize[cblks] = blk_size;
        assert(blkp[cblks] != NULL);
    }

    while (TRUE) {
        for (cblks = 0; cblks < num_chunks; cblks++) {
            victim = lran2(&rgen) % num_chunks;
#if defined(CPP)
#if defined(SIZED)
            operator delete[](blkp[victim], blksize[victim]);
#else
            delete[] blkp[victim];
#endif
#else
            p->_free(blkp[victim]);
#endif

            if (max_size == min_size) {
                blk_size = min_size;
            }
            else {
                blk_size = min_size + lran2(&rgen) % (max_size - min_size);
            }
#if defined(CPP)
            blkp[victim] = new char[blk_size];
#else
            blkp[victim] = (char*)p->_malloc(blk_size);
#endif
            blksize[victim] = blk_size;
            assert(blkp[victim] != NULL);
        }
        sum_allocs += num_chunks;

        QueryPerformanceCounter(&end_cnt);
#ifdef __WIN32__
        ticks = end_cnt.QuadPart - start_cnt.QuadPart;
        duration = (double)ticks / ticks_per_sec.QuadPart;
#else
        ticks = end_cnt - start_cnt;
        duration = (double)ticks / ticks_per_sec;
#endif

        if (duration >= sleep_cnt) break;
    }
    reqd_space = (0.5 * (min_size + max_size) * num_chunks);

    printf("%6.3f", duration);
    printf("%8.0f", sum_allocs / duration);
    printf(" %6.3f %.3f", (double)used_space / (1024 * 1024), used_space / reqd_space);
    printf("\n");

}


#if defined(_MT) || defined(_REENTRANT)
//#ifdef _MT
static void runthreads(Params * p, long sleep_cnt, int min_threads, int max_threads, int chperthread, int num_rounds)
{
    thread_data  de_area[MAX_THREADS];
    thread_data* pdea;
    long          nperthread;
    long          sum_threads;
    _int64        sum_allocs;
    _int64        sum_frees;
    double        duration;
#ifdef __WIN32__
    _LARGE_INTEGER ticks_per_sec, start_cnt, end_cnt;
#else
    long ticks_per_sec;
    long start_cnt, end_cnt;
#endif
    _int64        ticks;
    double        rate_1 = 0, rate_n;
    double        reqd_space;
    ULONG         used_space;
    int           prevthreads;
    int           i;

    QueryPerformanceFrequency(&ticks_per_sec);

    pdea = &de_area[0];
    memset(&de_area[0], 0, sizeof(thread_data));

    prevthreads = 0;
    for (num_threads = min_threads; num_threads <= max_threads; num_threads++)
    {

        warmup(&blkp[prevthreads * chperthread], (num_threads - prevthreads) * chperthread);

        nperthread = chperthread;
        stopflag = FALSE;

        for (i = 0; i < num_threads; i++) {
            de_area[i].threadno = i + 1;
            de_area[i].NumBlocks = num_rounds * nperthread;
            de_area[i].array = &blkp[i * nperthread];
            de_area[i].blksize = &blksize[i * nperthread];
            de_area[i].asize = nperthread;
            de_area[i].min_size = min_size;
            de_area[i].max_size = max_size;
            de_area[i].seed = lran2(&rgen); ;
            de_area[i].finished = 0;
            de_area[i].cAllocs = 0;
            de_area[i].cFrees = 0;
            de_area[i].cThreads = 0;
            de_area[i].finished = FALSE;
            lran2_init(&de_area[i].rgen, de_area[i].seed);

#ifdef __WIN32__
            _beginthread((void(__cdecl*)(void*)) exercise_heap, 0, &de_area[i]);
#else
            _beginthread(exercise_heap, 0, &de_area[i]);
#endif

        }

        QueryPerformanceCounter(&start_cnt);

        // printf ("Sleeping for %ld seconds.\n", sleep_cnt);
        Sleep(sleep_cnt * 1000L);

        stopflag = TRUE;

        for (i = 0; i < num_threads; i++) {
            while (!de_area[i].finished) {
#ifdef __WIN32__
                Sleep(1);
#elif defined(__SVR4)
                thr_yield();
#else
                sched_yield();
#endif
            }
        }


        QueryPerformanceCounter(&end_cnt);

        sum_frees = sum_allocs = 0;
        sum_threads = 0;
        for (i = 0; i < num_threads; i++) {
            sum_allocs += de_area[i].cAllocs;
            sum_frees += de_area[i].cFrees;
            sum_threads += de_area[i].cThreads;
            de_area[i].cAllocs = de_area[i].cFrees = 0;
        }


#ifdef __WIN32__
        ticks = end_cnt.QuadPart - start_cnt.QuadPart;
        duration = (double)ticks / ticks_per_sec.QuadPart;
#else
        ticks = end_cnt - start_cnt;
        duration = (double)ticks / ticks_per_sec;
#endif

        for (i = 0; i < num_threads; i++) {
            if (!de_area[i].finished)
                printf("Thread at %d not finished\n", i);
        }


        rate_n = sum_allocs / duration;
        if (rate_1 == 0) {
            rate_1 = rate_n;
        }

        reqd_space = (0.5 * (min_size + max_size) * num_threads * chperthread);
        used_space = 0;

        double throughput = (double)sum_allocs / duration;
        double rtime = 1.0e9 / throughput;
        printf("Throughput = %8.0f operations per second, relative time: %.3fs.\n", throughput, rtime);


#if 0
        printf("%2d ", num_threads);
        printf("%6.3f", duration);
        printf("%6.3f", rate_n / rate_1);
        printf("%8.0f", sum_allocs / duration);
        printf(" %6.3f %.3f", (double)used_space / (1024 * 1024), used_space / reqd_space);
        printf("\n");
#endif

        Sleep(2500L); // wait 5 sec for old threads to die

        prevthreads = num_threads;

        printf("Done sleeping...\n");

    }
}


static void* exercise_heap(Params * p, void* pinput)
{
    thread_data* pdea;
    int           cblks = 0;
    int           victim;
    size_t        blk_size;
    int           range;

    if (stopflag) return 0;

    pdea = (thread_data*)pinput;
    pdea->finished = FALSE;
    pdea->cThreads++;
    range = pdea->max_size - pdea->min_size;

    /* allocate NumBlocks chunks of random size */
    for (cblks = 0; cblks < pdea->NumBlocks; cblks++) {
        victim = lran2(&pdea->rgen) % pdea->asize;
#ifdef CPP
#if defined(SIZED)
        operator delete[](pdea->array[victim], pdea->blksize[victim]);
#else
        delete[] pdea->array[victim];
#endif
#else
        p->_free(pdea->array[victim]);
#endif
        pdea->cFrees++;

        if (range == 0) {
            blk_size = pdea->min_size;
        }
        else {
            blk_size = pdea->min_size + lran2(&pdea->rgen) % range;
        }
#ifdef CPP
        pdea->array[victim] = new char[blk_size];
#else
        pdea->array[victim] = (char*)p->_malloc(blk_size);
#endif

        pdea->blksize[victim] = blk_size;
        assert(pdea->array[victim] != NULL);

        pdea->cAllocs++;

        /* Write something! */

        volatile char* chptr = ((char*)pdea->array[victim]);
        *chptr++ = 'a';
        volatile char ch = *((char*)pdea->array[victim]);
        *chptr = 'b';


        if (stopflag) break;
    }

    //  	printf("Thread %u terminating: %d allocs, %d frees\n",
    //		      pdea->threadno, pdea->cAllocs, pdea->cFrees) ;
    pdea->finished = TRUE;


    if (!stopflag) {
#ifdef __WIN32__
        _beginthread((void(__cdecl*)(void*)) exercise_heap, 0, pdea);
#else
        _beginthread(exercise_heap, 0, pdea);
#endif
    }
    else {
        // printf ("thread stopping.\n");
    }

    //end_thread();

#ifndef _WIN32
    pthread_exit(NULL);
#endif
    return 0;
}

static void warmup(Params * p , char** blkp, int num_chunks)
{
    int     cblks;
    int     victim;
    size_t  blk_size;
    LPVOID  tmp;
    size_t  tmp_sz;


    for (cblks = 0; cblks < num_chunks; cblks++) {
        if (min_size == max_size) {
            blk_size = min_size;
        }
        else {
            blk_size = min_size + lran2(&rgen) % (max_size - min_size);
        }
#ifdef CPP
        blkp[cblks] = new char[blk_size];
#else
        blkp[cblks] = (char*)p->_malloc(blk_size);
#endif
        blksize[cblks] = blk_size;
        assert(blkp[cblks] != NULL);
    }

    /* generate a random permutation of the chunks */
    for (cblks = num_chunks; cblks > 0; cblks--) {
        victim = lran2(&rgen) % cblks;
        tmp = blkp[victim];
        tmp_sz = blksize[victim];
        blkp[victim] = blkp[cblks - 1];
        blksize[victim] = blksize[cblks - 1];
        blkp[cblks - 1] = (char*)tmp;
        blksize[cblks - 1] = tmp_sz;
    }

    for (cblks = 0; cblks < 4 * num_chunks; cblks++) {
        victim = lran2(&rgen) % num_chunks;
#ifdef CPP
#if defined(SIZED)
        operator delete[](blkp[victim], blksize[victim]);
#else
        delete[] blkp[victim];
#endif
#else
        p->_free(blkp[victim]);
#endif

        if (max_size == min_size) {
            blk_size = min_size;
        }
        else {
            blk_size = min_size + lran2(&rgen) % (max_size - min_size);
        }
#ifdef CPP
        blkp[victim] = new char[blk_size];
#else
        blkp[victim] = (char*)p->_malloc(blk_size);
#endif
        blksize[victim] = blk_size;
        assert(blkp[victim] != NULL);
    }
}
#endif // _MT

#ifdef __WIN32__
static ULONG CountReservedSpace()
{
    MEMORY_BASIC_INFORMATION info;
    char* addr = NULL;
    ULONG                     size = 0;

    while (true) {
        VirtualQuery(addr, &info, sizeof(info));
        switch (info.State) {
        case MEM_FREE:
        case MEM_RESERVE:
            break;
        case MEM_COMMIT:
            size += info.RegionSize;
            break;
        }
        addr += info.RegionSize;
        if (addr >= (char*)0x80000000UL) break;
    }

    return size;

}
#endif




static void print_process_infos()
{
#ifdef __linux__
    char cmd[200];
    snprintf(cmd, sizeof(cmd), "grep ^VmHWM /proc/%d/status", (int)getpid());
    std::system(cmd);
    snprintf(cmd, sizeof(cmd), "grep ^VmPeak /proc/%d/status", (int)getpid());
    std::system(cmd);

    snprintf(cmd, sizeof(cmd), "echo 1 > /proc/%d/clear_refs", (int)getpid());
    std::system(cmd);
    //snprintf(cmd,sizeof(cmd),"echo 5 > /proc/%d/clear_refs",(int)getpid ());
    //std::system(cmd);
    return; //TEST
#endif
    micro_process_infos infos;
    micro_get_process_infos(&infos);
    std::cout << "Peak RSS: " << infos.peak_rss << std::endl;
    std::cout << "Peak Commit: " << infos.peak_commit << std::endl;

}


int main(int argc, char* argv[])
{


    //  char * dummy = new char[42];
       //ReferenceLibHoard();
#if defined(_MT) || defined(_REENTRANT)
    int          min_threads, max_threads;
    int          num_rounds;
    int          chperthread;
#endif
    unsigned     seed = 12345;
    int          num_chunks = 10000;
    long sleep_cnt;

    if (argc > 7) {
        sleep_cnt = atoi(argv[1]);
        min_size = atoi(argv[2]);
        max_size = atoi(argv[3]);
        chperthread = atoi(argv[4]);
        num_rounds = atoi(argv[5]);
        seed = atoi(argv[6]);
        max_threads = atoi(argv[7]);
        min_threads = max_threads;
        goto DoneWithInput;
    }

#if defined(_MT) || defined(_REENTRANT)
    //#ifdef _MT
    printf("\nMulti-threaded test driver \n");
#else
    printf("\nSingle-threaded test driver \n");
#endif
#ifdef CPP
#if defined(SIZED)
    printf("C++ version (new and sized delete)\n");
#else
    printf("C++ version (new and delete)\n");
#endif
#else
    printf("C version (malloc and free)\n");
#endif
    printf("runtime (sec): ");
    scanf("%ld", &sleep_cnt);

    printf("chunk size (min,max): ");
    scanf("%d %d", &min_size, &max_size);
#if defined(_MT) || defined(_REENTRANT)
    //#ifdef _MT
    printf("threads (min, max):   ");
    scanf("%d %d", &min_threads, &max_threads);
    printf("chunks/thread:  "); scanf("%d", &chperthread);
    printf("no of rounds:   "); scanf("%d", &num_rounds);
    num_chunks = max_threads * chperthread;
#else
    printf("no of chunks:  "); scanf("%d", &num_chunks);
#endif
    printf("random seed:    "); scanf("%d", &seed);

DoneWithInput:

    if (num_chunks > MAX_BLOCKS) {
        printf("Max %d chunks - exiting\n", MAX_BLOCKS);
        return(1);
    }


    {
        std::cout << "Larson micro_malloc:" << std::endl;
        Params p;
        p._malloc = micro_malloc;
        p._free = micro_free;
        lran2_init( &rgen, seed);

#if defined(_MT) || defined(_REENTRANT)
        //#ifdef _MT
        runthreads(&p, sleep_cnt, min_threads, max_threads, chperthread, num_rounds);
#else
        runloops(&p, sleep_cnt, num_chunks);
#endif
        print_process_infos();
    }
    {
        std::cout << "Larson malloc:" << std::endl;
        Params p;
        p._malloc = malloc;
        p._free = free;
        lran2_init(&rgen, seed);

#if defined(_MT) || defined(_REENTRANT)
        //#ifdef _MT
        runthreads(&p, sleep_cnt, min_threads, max_threads, chperthread, num_rounds);
#else
        runloops(&p, sleep_cnt, num_chunks);
#endif
        print_process_infos();
    }

#ifdef BENCH_MIMALLOC
    {
        std::cout << "Larson mimalloc:" << std::endl;
        Params p;
        p._malloc = malloc;
        p._free = free;
        lran2_init(&rgen, seed);

#if defined(_MT) || defined(_REENTRANT)
        //#ifdef _MT
        runthreads(&p, sleep_cnt, min_threads, max_threads, chperthread, num_rounds);
#else
        runloops(&p, sleep_cnt, num_chunks);
#endif
        print_process_infos();
    }
#endif

    return(0);

} /* main */

#endif
// =======================================================
