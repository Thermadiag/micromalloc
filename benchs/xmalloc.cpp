/**
 * \file   test-malloc_test.c
 * \author C. Lever and D. Boreham, Christian Eder ( ederc@mathematik.uni-kl.de )
 * \date   2000
 * \brief  Test file for CUSTOM_MALLOC. This is a multi-threaded test system by
 *         Lever and Boreham. It is first noted in their paper "malloc()
 *         Performance in a Multithreaded Linux Environment", appeared at the
 *         USENIX 2000 Annual Technical Conference: FREENIX Track.
 *         This file is part of XMALLOC, licensed under the GNU General
 *         Public License version 3. See COPYING for more information.
 */

#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <chrono>


#include <micro/micro.h>
#include <micro/testing.hpp>
#include <iostream>
#include <cstdint>
#include <vector>

typedef void* (*CUSTOM_MALLOC_P)(size_t);
typedef void (*CUSTOM_FREE_P)(void*);
static CUSTOM_MALLOC_P CUSTOM_MALLOC;
static CUSTOM_FREE_P CUSTOM_FREE;
static const char* CUSTOM_NAME;

#define LRAN2_MAX 714025l /* constants for portable */
#define IA	  1366l	  /* random number generator */
#define IC	  150889l /* (see e.g. `Numerical Recipes') */

struct lran2_st {
	long x, y, v[97];
};

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

#define CACHE_ALIGNED 1

#define DEFAULT_OBJECT_SIZE 1024

int debug_flag = 0;
int verbose_flag = 0;
#define num_workers_default 4
int num_workers = num_workers_default;
double run_time = 5.0;
int object_size = DEFAULT_OBJECT_SIZE;
/* array for thread ids */
std::vector<std::thread> thread_ids;

/* array for saving result of each thread */
#if CACHE_ALIGNED
struct alignas(64) counter {
	long c;
};
#else
struct counter {
	long c;
};
#endif

struct counter* counters;

static std::atomic<int> done_flag{ 0 };
static double begin;

static double secs_since_epoch()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() * 1e-3;
}

double elapsed_time( double* time0)
{
	double td = secs_since_epoch() - *time0;
	return td ;
}

static const long possible_sizes[] = { 8,12,16,24,32,48,64,96,128,192,256,(256 * 3) / 2,512, (512 * 3) / 2, 1024, (1024 * 3) / 2, 2048 };
static const int n_sizes = sizeof(possible_sizes) / sizeof(long);

#define OBJECTS_PER_BATCH 4096
struct batch {
	struct batch* next_batch;
	void* objects[OBJECTS_PER_BATCH];
};

volatile struct batch* batches = NULL;
volatile int batch_count = 0;
const int batch_count_limit = 100;
std::condition_variable empty_cv ;
std::condition_variable full_cv ;
std::mutex lock ;

void enqueue_batch( batch* b) {
	std::unique_lock<std::mutex> ll(lock);
	while (batch_count >= batch_count_limit && !done_flag.load()) {
		full_cv.wait(ll);
	}
	b->next_batch = (batch*)batches;
	batches = b;
	batch_count++;
	empty_cv.notify_one();
}

struct batch* dequeue_batch() {
	std::unique_lock<std::mutex> ll(lock);
	while (batches == NULL && !atomic_load(&done_flag)) {
		empty_cv.wait(ll);
	}
	struct batch* result = (batch*)batches;
	if (result) {
		batches = result->next_batch;
		batch_count--;
		full_cv.notify_one();
	}
	return result;
}

void* mem_allocator(void* arg) {
	int thread_id = *(int*)arg;
	struct lran2_st lr;
	lran2_init(&lr, thread_id);

	while (!done_flag.load()) {
		struct batch* b = (batch*) CUSTOM_MALLOC(sizeof(batch));
		for (int i = 0; i < OBJECTS_PER_BATCH; i++) {
			size_t siz = object_size > 0 ? object_size : possible_sizes[lran2(&lr) % n_sizes];
			b->objects[i] = CUSTOM_MALLOC(siz);
			memset(b->objects[i], i % 256, (siz > 128 ? 128 : siz));
		}
		enqueue_batch(b);
	}
	return NULL;
}

void* mem_releaser(void* arg) {
	int thread_id = *(int*)arg;

	while (!atomic_load(&done_flag)) {
		struct batch* b = dequeue_batch();
		if (b) {
			for (int i = 0; i < OBJECTS_PER_BATCH; i++) {
				CUSTOM_FREE(b->objects[i]);
			}
			CUSTOM_FREE(b);
		}
		counters[thread_id].c += OBJECTS_PER_BATCH;
	}
	return NULL;
}

int run_memory_free_test(const char * allocator)
{
	done_flag.store(0);
	batches = NULL;
	batch_count = 0;
	void* ptr = NULL;
	int i;
	double elapse_time = 0.0;
	long total = 0;
	int* ids = (int*)CUSTOM_MALLOC(sizeof(int) * num_workers);

	/* Initialize counter */
	for (i = 0; i < num_workers; ++i)
		counters[i].c = 0;

	if (thread_ids.size() == 0)
		thread_ids.resize(num_workers*2);

	begin = secs_since_epoch();// gettimeofday(&begin, (struct timezone*)0);

	/* Start up the mem_allocator and mem_releaser threads  */
	for (i = 0; i < num_workers; ++i) {
		ids[i] = i;
		thread_ids[i * 2] = std::thread([i, &ids]() {mem_releaser((void*)&ids[i]); });
		thread_ids[i * 2 + 1] = std::thread([i, &ids]() {mem_allocator((void*)&ids[i]); });
	}


	while (1) {
		std::this_thread::sleep_for(std::chrono::microseconds(1000));
		if (elapsed_time(&begin) > run_time) {
			done_flag.store(1);
			empty_cv.notify_all();
			full_cv.notify_all();
			break;
		}
	}

	for (i = 0; i < num_workers * 2; ++i)
		thread_ids[i].join();

	while (batches) {
		struct batch* b = (batch*)batches;
		batches = b->next_batch;
		for (int i = 0; i < OBJECTS_PER_BATCH; i++) {
			CUSTOM_FREE(b->objects[i]);
		}
		CUSTOM_FREE(b);
	}

	for (i = 0; i < num_workers; ++i)
		total += counters[i].c;
	if (ids != NULL) CUSTOM_FREE(ids);
	CUSTOM_FREE(counters);

	micro::allocator_trim(allocator);

	elapse_time = elapsed_time(&begin);

	double mfree_per_sec = ((double)total / elapse_time) * 1e-6;
	double rtime = 100.0 / mfree_per_sec;
	printf("rtime: %.3f, free/sec: %.3f M\n", rtime, mfree_per_sec);
	
	thread_ids.clear();

	return(0);
}

void usage(char* prog)
{
	printf("%s [-w workers] [-t run_time] [-d] [-v]\n", prog);
	printf("\t -w number of producer threads (and number of consumer threads), default %d\n", num_workers_default);
	printf("\t -t run time in seconds, default 20.0 seconds.\n");
	printf("\t -s size of object to allocate (default %d bytes) (specify -1 to get many different object sizes)\n", DEFAULT_OBJECT_SIZE);
	printf("\t -d debug mode\n");
	printf("\t -v verbose mode (-v -v produces more verbose)\n");
	exit(1);
}


int test_xmallox(const char * allocator)
{
	std::cout << allocator << std::endl;
	num_workers = 8;
	run_time = 5;
	debug_flag = 1;
	object_size = -1;
	verbose_flag = 0;

	/*int c;
	while ((c = getopt(argc, argv, "w:t:ds:v")) != -1) {

		switch (c) {

		case 'w':
			num_workers = atoi(optarg);
			break;
		case 't':
			run_time = atof(optarg);
			break;
		case 'd':
			debug_flag = 1;
			break;
		case 's':
			object_size = atoi(optarg);
			break;
		case 'v':
			verbose_flag++;
			break;
		default:
			usage(argv[0]);
		}
	}*/

	/* allocate memory for working arrays */
	counters = (struct counter*)CUSTOM_MALLOC(sizeof(*counters) * num_workers);

	run_memory_free_test(allocator);

	
	std::cout << std::endl;

	return 0;
}



int xmalloc(int, char** const)
{

	//micro_set_parameter(MicroPrintStatsTrigger, 1);
	//micro_set_string_parameter(MicroPrintStats, "stdout");
#ifdef MICRO_BENCH_MICROMALLOC
	CUSTOM_MALLOC = micro_malloc;
	CUSTOM_FREE = micro_free;
	test_xmallox("micro");
#endif

#ifdef MICRO_BENCH_MALLOC
	CUSTOM_MALLOC = malloc;
	CUSTOM_FREE = free;
	test_xmallox("malloc");
#endif

#ifdef MICRO_BENCH_MIMALLOC
	CUSTOM_MALLOC = malloc;
	CUSTOM_FREE = free;
	test_xmallox("mimalloc");
#endif

#ifdef MICRO_BENCH_SNMALLOC
	CUSTOM_MALLOC = snmalloc::libc::malloc;
	CUSTOM_FREE = snmalloc::libc::free;
	test_xmallox("snmalloc");
#endif

#ifdef MICRO_BENCH_JEMALLOC
	//const char* je_malloc_conf = "dirty_decay_ms:0";
	CUSTOM_MALLOC = je_malloc;
	CUSTOM_FREE = je_free;
	test_xmallox("jemalloc");
#endif


#ifdef USE_TBB
	CUSTOM_MALLOC = scalable_malloc;
	CUSTOM_FREE = scalable_free;
	test_xmallox("tbb");
#endif

	return 0;
}
