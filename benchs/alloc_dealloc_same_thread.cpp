#include <micro/micro.h>
#include <micro/testing.hpp>

#include <vector>
#include <thread>
#include <iostream>
#include <random>
#include <list>

#define MAX_THREADS 20

#if defined(MICRO_BENCH_TBB) && defined(NDEBUG)
#define USE_TBB
#endif

#ifdef MICRO_BENCH_MIMALLOC
#include <mimalloc.h>
#endif

#ifdef USE_TBB
#include <tbb/scalable_allocator.h>
#endif

#ifdef MICRO_BENCH_SNMALLOC
#include <snmalloc/snmalloc.h>
#endif

//#define BENCH_JEMALLOC
#ifdef MICRO_BENCH_JEMALLOC
extern "C" {
#include "jemalloc.h"
}
#endif
static size_t el_alloc{0};
static size_t thcount{ 0 };
static std::atomic<int> finish_count{ 0 };
static std::atomic<bool> start_compute{ false };

template<class T>
void alloc_dealloc_thread(size_t count, void** ptr, unsigned* sizes, size_t start)
{
	while (!start_compute.load(std::memory_order_relaxed))
		std::this_thread::yield();

	for (size_t i = start; i < count; i += thcount) {
		if ((ptr)[i])
			throw std::runtime_error("");
		(ptr)[i] = T::alloc_mem((sizes)[i]);
	}

	finish_count.fetch_add(1);
	// wait for all threads to finish allocations
	while (finish_count.load(std::memory_order_relaxed) != thcount)
		std::this_thread::yield();

	for (size_t i = start; i < count; i += thcount) {
		if (!(ptr)[i])
			throw std::runtime_error("");
		T::free_mem((ptr)[i]);
	}
}

template<class T>
void test_allocator(const char* allocator, size_t max_size, size_t max_mem)
{
	size_t alloc_count = max_mem / (max_size / 2);
	size_t total = 0;
	size_t additional = 0;
	{
		std::random_device dev;
		std::mt19937 rng(dev());
		std::uniform_int_distribution<std::mt19937::result_type> dist(0, max_size); // distribution in range [0, max_size]

		std::vector<void*, micro::testing_allocator<void*,T>> ptr(alloc_count);
		std::vector<unsigned, micro::testing_allocator<unsigned*, T>> ss(ptr.size());

		for (size_t i = 0; i < ss.size(); ++i) {
			ss[i] = (unsigned)dist(rng);
			total += ss[i];
		}

		additional = ptr.size() * sizeof(void*);
		additional += ss.size() * sizeof(unsigned);

		std::fill_n(ptr.begin(), ptr.size(), nullptr);
		std::thread threads[MAX_THREADS];
		for (size_t i = 0; i < thcount; ++i) {
			threads[i] = std::thread([&ptr, &ss, i]() { alloc_dealloc_thread<T>(ss.size(), ptr.data(), ss.data(), i); });
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		micro::tick();
		start_compute = true;
		for (size_t i = 0; i < thcount; ++i)
			threads[i].join();

		
	}
	micro::allocator_trim(allocator);
	el_alloc = micro::tock_ms();
	size_t total_ops = alloc_count * 2;
	micro_process_infos infos;
	micro_get_process_infos(&infos);
	//std::cout << "Threads\tOps/s\tMemoryOverhead" << std::endl;
	double overhead = (infos.peak_rss - additional);
	overhead /= total;

	std::cout << thcount << "\t" << size_t(total_ops / ((double)el_alloc / 1000.)) << "\t" << overhead << std::endl;

}

int alloc_dealloc_same_thread(int, char** const)
{
	union Node
	{
		std::atomic<int> access; // one byte
		struct
		{
			uint32_t reserved : 8;
			uint32_t size : 23;
			uint32_t isnull : 1;
		};
		Node() {}
	};
	Node n;
	n.reserved;

	using namespace micro;

	unsigned max_size = 0;


#ifndef MICRO_TEST_THREAD
	const char * MICRO_TEST_THREAD = std::getenv("MICRO_TEST_THREAD");
	if(MICRO_TEST_THREAD)
		thcount = micro::detail::from_string<size_t>(MICRO_TEST_THREAD);
	else {
		std::cout << "Thread count:";
		std::cin >> thcount;
	}
#else
	thcount = MICRO_TEST_THREAD;
#endif

#ifndef MICRO_TEST_SIZE
	const char * MICRO_TEST_SIZE = std::getenv("MICRO_TEST_SIZE");
	if(MICRO_TEST_SIZE)
		max_size = micro::detail::from_string<unsigned>(MICRO_TEST_SIZE);
	else {
		std::cout << "Max alloc size:";
		std::cin >> max_size;
	}
#else
	max_size = MICRO_TEST_SIZE;
#endif

	if (max_size == 0)
		max_size = 5000;

	srand(0);

	size_t max_mem = 2000000000ull;
	size_t alloc_count = max_mem / (max_size / 2);

	

#ifdef MICRO_BENCH_MICROMALLOC
	start_compute = false;
	finish_count = 0;
	//std::cout << "micro:" << std::endl;
	test_allocator<Alloc>("micro", max_size, max_mem);
	//print_process_infos();
#endif

#ifdef MICRO_BENCH_MALLOC
	start_compute = false;
	finish_count = 0;
	//std::cout << "malloc:" << std::endl;
	test_allocator<Malloc>("malloc", max_size, max_mem);
	//print_process_infos();
#endif

#ifdef MICRO_BENCH_SNMALLOC
	start_compute = false;
	finish_count = 0;
	//std::cout << "snmalloc:" << std::endl;
	test_allocator<SnMalloc>("snmalloc", max_size, max_mem);
	//print_process_infos();
#endif

#ifdef MICRO_BENCH_JEMALLOC
	// const char* je_malloc_conf = "dirty_decay_ms:0,muzzy_decay_ms=0";

	start_compute = false;
	finish_count = 0;
	//std::cout << "jemalloc:" << std::endl;
	test_allocator<Jemalloc>("jemalloc", max_size, max_mem);
	//print_process_infos();
#endif

#ifdef MICRO_BENCH_MIMALLOC
	start_compute = false;
	finish_count = 0;
	//std::cout << "mimalloc:" << std::endl;
	test_allocator<MiMalloc>("mimalloc", max_size, max_mem);
	//print_process_infos();

#endif

#ifdef USE_TBB
	start_compute = false;
	finish_count = 0;
	//std::cout << "onetbb:" << std::endl;
	test_allocator<TBBMalloc>("onetbb", max_size, max_mem);
	//print_process_infos();
#endif

	return 0;
}
