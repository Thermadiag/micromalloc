#include <micro/micro.h>
#include <micro/testing.hpp>

#include <vector>
#include <thread>
#include <iostream>
#include <random>
#include <list>

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

static size_t thcount{ 0 };
static std::atomic<int> finish_count{ 0 };
static std::atomic<bool> start_compute{ false };

template<class T>
void alloc_dealloc_thread(std::vector<void*>* ptr, std::vector<unsigned>* sizes, size_t start)
{
	while (!start_compute.load(std::memory_order_relaxed))
		std::this_thread::yield();

	for (size_t i = start; i < ptr->size(); i += thcount) {
		if ((*ptr)[i])
			throw std::runtime_error("");
		(*ptr)[i] = T::alloc_mem((*sizes)[i]);
	}

	finish_count.fetch_add(1);
	// wait for all threads to finish allocations
	while (finish_count.load(std::memory_order_relaxed) != thcount)
		std::this_thread::yield();

	for (size_t i = start; i < ptr->size(); i += thcount) {
		if (!(*ptr)[i])
			throw std::runtime_error("");
		T::free_mem((*ptr)[i]);
	}
}

template<class T>
void test_allocator(const char* allocator, std::vector<void*>* ptr, std::vector<unsigned>* sizes)
{
	{
		std::fill_n(ptr->begin(), ptr->size(), nullptr);
		std::vector<std::thread> threads(thcount);
		for (size_t i = 0; i < thcount; ++i) {
			threads[i] = std::thread([&ptr, &sizes, i]() { alloc_dealloc_thread<T>(ptr, sizes, i); });
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		micro::tick();
		start_compute = true;
		for (size_t i = 0; i < thcount; ++i)
			threads[i].join();

		micro::allocator_trim(allocator);
		size_t el_alloc = micro::tock_ms();

		size_t bytes = 0;
		for (unsigned b : *sizes)
			bytes += b;

		std::cout << "Allocate and deallocate " << bytes << " bytes in the same thread" << std::endl;
		std::cout << el_alloc << " ms" << std::endl;
	}
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

	/* struct Big
	{
		char data[16];
	};
	 {
		micro::tick();
		std::list<Big> lst;
		for (int i = 0; i < 10000000; ++i)
			lst.emplace_back();
		size_t el = micro::tock_ms();
		std::cout << el << std::endl;
	}
	{
		micro::tick();
		std::list<Big, micro::heap_allocator<Big>> lst;
		for (int i = 0; i < 10000000; ++i)
			lst.emplace_back();
		size_t el = micro::tock_ms();
		std::cout << el << std::endl;
	}*/

	// micro_set_parameter(MicroPrintStatsTrigger, 1);
	// micro_set_string_parameter(MicroPrintStats, "stdout");


	// micro_set_parameter( MicroAllowOsPageAlloc, true);
	// micro_set_parameter( MicroPageMemorySize, 3000000000ull);
	// micro_set_parameter(MicroSmallAllocThreshold, 500);
	// micro_set_parameter(MicroSmallAllocThreshold, 0);
	// micro_set_parameter(MicroBackendMemory, 3000000000ull);
	unsigned max_size = 0;

#ifndef MICRO_TEST_THREAD
	std::cout << "Thread count:";
	std::cin >> thcount;
#else
	thcount = MICRO_TEST_THREAD;
#endif

#ifndef MICRO_TEST_SIZE
	std::cout << "Max alloc size:";
	std::cin >> max_size;
#else
	max_size = MICRO_TEST_SIZE;
#endif

	if (max_size == 0)
		max_size = 5000;

	srand(0);

	size_t max_mem = 2000000000ull;
	size_t alloc_count = max_mem / (max_size / 2);

	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> dist(0, max_size); // distribution in range [0, max_size]

	std::vector<void*> ptr(alloc_count);
	std::vector<unsigned> ss(ptr.size());
	size_t total = 0;
	for (size_t i = 0; i < ss.size(); ++i) {
		ss[i] = (unsigned)dist(rng);
		total += ss[i];
	}

#ifdef MICRO_BENCH_MICROMALLOC
	start_compute = false;
	finish_count = 0;
	std::cout << "micro:" << std::endl;
	test_allocator<Alloc>("micro", &ptr, &ss);
	print_process_infos();
#endif

#ifdef MICRO_BENCH_MALLOC
	start_compute = false;
	finish_count = 0;
	std::cout << "malloc:" << std::endl;
	test_allocator<Malloc>("malloc", &ptr, &ss);
	print_process_infos();
#endif

#ifdef MICRO_BENCH_SNMALLOC
	start_compute = false;
	finish_count = 0;
	std::cout << "snmalloc:" << std::endl;
	test_allocator<SnMalloc>("snmalloc", &ptr, &ss);
	print_process_infos();
#endif

#ifdef MICRO_BENCH_JEMALLOC
	// const char* je_malloc_conf = "dirty_decay_ms:0,muzzy_decay_ms=0";

	start_compute = false;
	finish_count = 0;
	std::cout << "jemalloc:" << std::endl;
	test_allocator<Jemalloc>("jemalloc", &ptr, &ss);
	print_process_infos();
#endif

#ifdef MICRO_BENCH_MIMALLOC
	start_compute = false;
	finish_count = 0;
	std::cout << "mimalloc:" << std::endl;
	test_allocator<MiMalloc>("mimalloc", &ptr, &ss);
	print_process_infos();

#endif

#ifdef USE_TBB
	start_compute = false;
	finish_count = 0;
	std::cout << "onetbb:" << std::endl;
	test_allocator<TBBMalloc>("onetbb", &ptr, &ss);
	print_process_infos();
#endif

	int* ptrs[16];
	uintptr_t min_align = 4096;
	for (int i = 0; i < 16; ++i) {
		ptrs[i] = (int*)malloc(sizeof(int));
		uintptr_t addr = (uintptr_t)ptrs[i];
		uintptr_t align = 1u << bit_scan_forward_64(addr);
		min_align = std::min(align, min_align);
	}
	for (int i = 0; i < 16; ++i)
		free(ptrs[i]);

	std::cout << "max_align_t: " << alignof(std::max_align_t) << std::endl;
	std::cout << "measured alignment: " << min_align << std::endl;

	return 0;
}
