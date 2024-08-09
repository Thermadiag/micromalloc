#include <micro/micro.h>
#include <micro/testing.hpp>

#include <vector>
#include <thread>
#include <iostream>

static size_t thcount{ 0 };
static std::atomic<int> finish_count{ 0 };
static std::atomic<bool> start_compute{ false };
static std::atomic<size_t> allocated{ 0 };
static std::atomic<size_t> max_allocated{ 0 };

template<class T>
void test_alloc_dealloc_thread(std::vector<std::atomic<void*>>* ptr, const std::vector<size_t>& order, std::vector<unsigned>* sizes)
{
	while (!start_compute.load(std::memory_order_relaxed))
		std::this_thread::yield();

	// do 5 passes
	// for (int pass = 0; pass < 5; ++pass)
	for (size_t i = 0; i < ptr->size(); i++) {
		size_t idx = (order)[i];
		void* p = (*ptr)[idx].load(std::memory_order_relaxed);
		if (!p) {
			unsigned s = (*sizes)[idx] + 4;
			void* m = T::alloc_mem(s);
			memcpy(m, &s, 4);
			// size_t peak = allocated.fetch_add(s) + s;

			if (!(*ptr)[idx].compare_exchange_strong(p, m)) {
				T::free_mem(m);
				// allocated.fetch_sub(s);
			}
			// else if (peak > max_allocated.load(std::memory_order_relaxed))
			//	max_allocated.store(peak);
		}
		else {
			void* m = (*ptr)[idx].exchange(nullptr);
			if (m) {
				unsigned s = 0;
				memcpy(&s, m, 4);
				T::free_mem(m);
				// allocated.fetch_sub(s);
			}
		}
	}

	// dealloc all
	for (size_t i = 0; i < ptr->size(); i++) {
		void* m = (*ptr)[i].exchange(nullptr);
		if (m) {
			unsigned s = 0;
			memcpy(&s, m, 4);
			T::free_mem(m);
			// allocated.fetch_sub(s);
		}
	}
}
template<class T>
void test_allocator_simultaneous_alloc_dealloc(const char* allocator, std::vector<unsigned>* sizes, std::vector<std::vector<size_t>>* orders)
{
	std::vector<std::atomic<void*>> ptr(sizes->size());
	{
		std::fill_n(ptr.begin(), ptr.size(), nullptr);

		std::vector<std::thread> threads(thcount);
		for (size_t i = 0; i < thcount; ++i) {

			threads[i] = std::thread([&, i]() { test_alloc_dealloc_thread<T>(&ptr, (*orders)[i], sizes); });
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		micro::tick();
		start_compute = true;
		for (size_t i = 0; i < thcount; ++i)
			threads[i].join();

		micro::allocator_trim(allocator);

		size_t el_alloc = micro::tock_ms();

		std::cout << "Interleaved allocation/deallocation in different threads" << std::endl;
		std::cout << el_alloc << " ms" << std::endl;
	}
}

int alloc_dealloc_separate_thread(int, char** const)
{
	using namespace micro;

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

	size_t max_mem = 1000000000ull;
	size_t alloc_count = max_mem / (max_size / 2);

	std::vector<unsigned> ss(alloc_count);
	for (size_t i = 0; i < ss.size(); ++i) {
		ss[i] = rand() % max_size;
	}

	std::vector<size_t> order(ss.size());
	for (size_t i = 0; i < order.size(); ++i)
		order[i] = i;

	std::vector<std::vector<size_t>> orders(thcount);
	for (unsigned i = 0; i < thcount; ++i) {
		orders[i] = order;
		micro::random_shuffle(orders[i].begin(), orders[i].end(), i);
	}

	/*start_compute = false;
	max_allocated = 0;
	allocated = 0;
	std::cout << "seq:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc< SeqAlloc>(&ss);
	std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	print_process_infos();
	*/

	// micro_set_parameter(MicroSmallAllocThreshold, 512);
	// micro_set_parameter(MicroAllowSmallAlloxFromRadixTree, 0);
	// micro_set_parameter(MicroPrintStatsTrigger, MicroOnExit);
	// micro_set_string_parameter(MicroPrintStats,"stdout");

	// micro_set_parameter(MicroProviderType, MicroOSPreallocProvider);
	// micro_set_parameter( MicroAllowOsPageAlloc, true);
	// micro_set_parameter( MicroPageMemorySize, 3000000000ull);

#ifdef MICRO_BENCH_MICROMALLOC
	start_compute = false;
	max_allocated = 0;
	allocated = 0;
	std::cout << "micro:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<Alloc>("micro", &ss, &orders);
	micro_clear();
	std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	print_process_infos();
#endif

#ifdef MICRO_BENCH_MALLOC
	start_compute = false;
	max_allocated = 0;
	allocated = 0;
	std::cout << "malloc:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<Malloc>("malloc", &ss, &orders);
	malloc_trim(0);
	std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	print_process_infos();
#endif

#ifdef MICRO_BENCH_JEMALLOC
	// const char* je_malloc_conf = "dirty_decay_ms:0";
	start_compute = false;
	max_allocated = 0;
	allocated = 0;
	std::cout << "jemalloc:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<Jemalloc>("jemalloc", &ss, &orders);
	std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	print_process_infos();
#endif

#ifdef MICRO_BENCH_SNMALLOC
	start_compute = false;
	max_allocated = 0;
	allocated = 0;
	std::cout << "snmalloc:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<SnMalloc>("snmalloc", &ss, &orders);
	std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	print_process_infos();

#endif

#ifdef MICRO_BENCH_MIMALLOC
	start_compute = false;
	max_allocated = 0;
	allocated = 0;
	std::cout << "mimalloc:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<MiMalloc>("mimalloc", &ss, &orders);
	std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	mi_heap_collect(mi_heap_get_default(), true);
	print_process_infos();

#endif

#ifdef USE_TBB
	start_compute = false;
	max_allocated = 0;
	allocated = 0;
	std::cout << "onetbb:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<TBBMalloc>("onetbb", &ss, &orders);
	std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	print_process_infos();
#endif

	return 0;
}
