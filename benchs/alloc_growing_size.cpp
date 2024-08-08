#include <micro/micro.h>
#include <micro/testing.hpp>

#include <vector>
#include <thread>
#include <iostream>

static size_t thcount{ 0 };
static std::atomic<int> finish_count{ 0 };
static std::atomic<bool> start_compute{ false };

template<class T>
void test_growing_thread(std::vector<unsigned>* sizes)
{
	std::vector<void*> ptr(sizes->size(), nullptr);
	while (!start_compute.load(std::memory_order_relaxed))
		std::this_thread::yield();

	for (size_t i = 0; i < sizes->size(); ++i) {
		(ptr)[i] = T::alloc_mem((*sizes)[i]);
		if (i & 1) {
			T::free_mem((ptr)[i - 1]);
			(ptr)[i - 1] = nullptr;
		}
	}
	// free in reverse order
	for (int i = (int)sizes->size() - 1; i >= 0; --i) {
		if ((ptr)[i])
			T::free_mem((ptr)[i]);
	}
}

template<class T>
void test_growing(const char* allocator, std::vector<unsigned>* sizes)
{
	std::vector<std::thread> threads(thcount);
	for (size_t i = 0; i < thcount; ++i) {
		threads[i] = std::thread( // std::bind(test_pathological_thread<T>, &start, sizes));
		  [&sizes]() { test_growing_thread<T>(sizes); });
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	micro::tick();
	start_compute = true;
	for (size_t i = 0; i < thcount; ++i)
		threads[i].join();

	micro::allocator_trim(allocator);
	size_t el_alloc = micro::tock_ms();

	std::cout << "Interleaved allocation/deallocation in gorwing order" << std::endl;
	std::cout << el_alloc << " ms" << std::endl;
}

int alloc_growing_size(int, char** const)
{
	using namespace micro;

#ifndef MICRO_TEST_THREAD
	std::cout << "Thread count:";
	std::cin >> thcount;
#else
	thcount = MICRO_TEST_THREAD;
#endif

	srand(0);

	size_t peak = 0;
	std::vector<unsigned> ss(15000);
	for (size_t i = 0; i < ss.size(); i += 2) {
		ss[i] = i * 16; // rand() % 5000;
		ss[i + 1] = i * 16;
		peak += i * 16;
	}

	std::cout << "Allocation peak: " << peak << std::endl;
	// std::sort(ss.begin(), ss.end());

	// micro_set_string_parameter(MicroPrintStats, "stdout");
	// micro_set_parameter(MicroPrintStatsTrigger, MicroOnExit);

	std::cout << "micro:" << std::endl;
	test_growing<Alloc>("micro", &ss);
	micro_clear();
	print_process_infos();

	// return 0;

#ifdef MICRO_BENCH_MALLOC
	start_compute = false;
	std::cout << "malloc:" << std::endl;
	test_growing<Malloc>("malloc", &ss);
	malloc_trim(0);
	print_process_infos();
#endif

#ifdef MICRO_BENCH_JEMALLOC
	// const char* je_malloc_conf = "dirty_decay_ms:0";
	start_compute = false;
	std::cout << "jemalloc:" << std::endl;
	test_growing<Jemalloc>("jemalloc", &ss);
	print_process_infos();
#endif

#ifdef MICRO_BENCH_MIMALLOC
	start_compute = false;
	std::cout << "mimalloc:" << std::endl;
	test_growing<MiMalloc>("mimalloc", &ss);
	print_process_infos();
	mi_heap_collect(mi_heap_get_default(), true);
#endif

#ifdef MICRO_BENCH_SNMALLOC
	start_compute = false;
	std::cout << "snmalloc:" << std::endl;
	test_growing<SnMalloc>("snmalloc", &ss);
	print_process_infos();
#endif

#ifdef USE_TBB
	start_compute = false;
	std::cout << "onetbb:" << std::endl;
	test_growing<TBBMalloc>("onetbb", &ss);
	print_process_infos();
#endif
	return 0;
}
