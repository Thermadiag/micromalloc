#include <micro/micro.h>
#include <micro/testing.hpp>

#include <vector>
#include <thread>
#include <iostream>

#define MAX_THREADS 20

static size_t thcount{ 0 };
static std::atomic<int> finish_count{ 0 };
static std::atomic<bool> start_compute{ false };



template<class T, size_t MT>
void test_alloc_dealloc_thread(unsigned count, std::atomic<void*>* ptr, const size_t* order, unsigned* sizes, unsigned thread_idx, micro::op_counter<MT>* c)
{
	while (!start_compute.load(std::memory_order_relaxed))
		std::this_thread::yield();

	// do 10 passes
	for (int pass = 0; pass < 10; ++pass)
	for (size_t i = 0; i < count; i++) {
		size_t idx = (order)[i];
		void* p = (ptr)[idx].load(std::memory_order_relaxed);
		if (!p) {
			unsigned s = (sizes)[idx] + 4;
			void* m = T::alloc_mem(s);
			memcpy(m, &s, 4);
			c->allocate( s);

			if (!(ptr)[idx].compare_exchange_strong(p, m)) {
				T::free_mem(m);
				c->deallocate( s);
			}

		}
		else {
			void* m = (ptr)[idx].exchange(nullptr);
			if (m) {
				unsigned s = 0;
				memcpy(&s, m, 4);
				T::free_mem(m);
				c->deallocate( s);
			}
		}
	}

	// dealloc all
	for (size_t i = 0; i <count; i++) {
		size_t idx = (order)[i];
		void* m = (ptr)[idx].exchange(nullptr);
		if (m) {
			unsigned s = 0;
			memcpy(&s, m, 4);
			T::free_mem(m);
			c->deallocate(s);
		}
	}
}

template<class T>
void test_allocator_simultaneous_alloc_dealloc(const char* allocator, size_t max_size, size_t max_mem)
{
	using Count = micro::op_counter<MAX_THREADS>;
	using TUnsignedAlloc = micro::testing_allocator<unsigned, T, Count>;
	using TSizetAlloc = micro::testing_allocator<size_t, T, Count>;

	size_t additional = 0, total_ops = 0;
	Count counter;
	{

		//std::random_device dev;
		std::mt19937 rng(0);
		std::uniform_int_distribution<std::mt19937::result_type> dist(0, max_size); // distribution in range [0, max_size]
		size_t alloc_count = max_mem / (max_size / 2);

		std::vector<unsigned, TUnsignedAlloc> ss(alloc_count,TUnsignedAlloc{ &counter });
		for (size_t i = 0; i < ss.size(); ++i) {
			ss[i] = dist(rng) % (unsigned)max_size;
		}

		using size_t_vector = std::vector<size_t,TSizetAlloc>;
		size_t_vector order(ss.size(), TSizetAlloc{&counter});
		for (size_t i = 0; i < order.size(); ++i)
			order[i] = i;

		std::vector<size_t_vector, micro::testing_allocator<size_t_vector, T, Count>> orders(thcount, micro::testing_allocator<size_t_vector, T, Count>{ &counter });
		for (unsigned i = 0; i < thcount; ++i) {
			orders[i] = order;
			micro::random_shuffle(orders[i].begin(), orders[i].end(), i);
		}

		std::atomic<void*>* ptr =
		  micro::testing_allocator<std::atomic<void*>, T, Count>{ &counter }.allocate(ss.size()); //(std::atomic<void*>*) T::alloc_mem(sizeof(std::atomic<void*>) * ss.size());
		for (size_t i = 0; i < ss.size(); ++i)
			new (ptr + i) std::atomic<void*>{ nullptr };

		/* additional = ss.size() * sizeof(unsigned);
		additional += ss.size() * sizeof(std::atomic<void*>);
		additional += order.size() * sizeof(size_t);
		for (size_t i = 0; i < orders.size(); ++i)
			additional += orders[i].size() * sizeof(size_t);
		*/
		
		
		std::thread threads[MAX_THREADS] ;
		for (size_t i = 0; i < thcount; ++i)
		{

			threads[i] = std::thread([&, i]() { test_alloc_dealloc_thread<T, MAX_THREADS>(ss.size(), ptr, orders[i].data(), ss.data(), i, &counter); });
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		micro::tick();
		start_compute = true;
		for (size_t i = 0; i < thcount; ++i)
			threads[i].join();


		ss.clear();
		ss.shrink_to_fit();

		order.clear();
		order.shrink_to_fit();

		orders.clear();
		orders.shrink_to_fit();

		T::free_mem(ptr);

		

		// std::cout << "Interleaved allocation/deallocation in different threads" << std::endl;
		// std::cout << el_alloc << " ms" << std::endl;
		

		total_ops = counter.total_ops();
	}

	micro::allocator_trim(allocator);
	size_t el_alloc = micro::tock_ms();


	micro_process_infos infos;
	micro_get_process_infos(&infos);
	//std::cout << "Threads\tOps/s\tMemoryOverhead" << std::endl;
	double overhead = (infos.peak_rss - additional);
	overhead /= counter.memory_peak();

	std::cout << thcount << "\t" << size_t(total_ops / ((double)el_alloc / 1000.)) << "\t" << overhead << std::endl;

	//std::cout << "Peak allocation: " << counter.memory_peak() << std::endl;
}

int alloc_dealloc_separate_thread(int, char** const)
{
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
	if(thcount > MAX_THREADS)
		thcount = MAX_THREADS;

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

	size_t max_mem = 100000000ull;
	
	

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
	test_allocator_simultaneous_alloc_dealloc<Alloc>("micro", max_size,max_mem);
#endif

#ifdef MICRO_BENCH_MALLOC
	start_compute = false;
	test_allocator_simultaneous_alloc_dealloc<Malloc>("malloc", max_size, max_mem);
#endif

#ifdef MICRO_BENCH_JEMALLOC
	// const char* je_malloc_conf = "dirty_decay_ms:0";
	start_compute = false;
	test_allocator_simultaneous_alloc_dealloc<Jemalloc>("jemalloc", max_size, max_mem);

#endif

#ifdef MICRO_BENCH_SNMALLOC
	start_compute = false;
	test_allocator_simultaneous_alloc_dealloc<SnMalloc>("snmalloc", max_size, max_mem);
#endif

#ifdef MICRO_BENCH_MIMALLOC
	start_compute = false;
	test_allocator_simultaneous_alloc_dealloc<MiMalloc>("mimalloc", max_size, max_mem);
#endif

#ifdef USE_TBB
	start_compute = false;
	test_allocator_simultaneous_alloc_dealloc<TBBMalloc>("onetbb", max_size, max_mem);
#endif

	
	
	return 0;
}
