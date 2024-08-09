#include <micro/micro.h>
#include <micro/testing.hpp>

#include <vector>
#include <thread>
#include <iostream>

#define MAX_THREADS 16

static size_t el_alloc=0;
static size_t thcount{ 0 };
static std::atomic<int> finish_count{ 0 };
static std::atomic<bool> start_compute{ false };
static std::atomic<size_t> max_allocated{ 0 };


class Counter
{
	unsigned sizes[MAX_THREADS];
	size_t ops[MAX_THREADS];
	size_t thread_count;
	size_t thread_count_up;

public:
	Counter(size_t threads)
	  : thread_count(threads)
	{
		thread_count_up = threads;
		if(thread_count_up & 1) ++thread_count_up;
		for (size_t i = 0; i < MAX_THREADS; ++i) {
			ops[i] = 0;
			sizes[i] = 0;
		}
	}

	MICRO_ALWAYS_INLINE size_t add(size_t thread_idx, size_t size)
	{
		++ops[thread_idx];
		sizes[thread_idx]+= size;
		unsigned tot = 0;
		unsigned count = (unsigned)thread_count_up;
		unsigned * ss = sizes;
		
		while(count >= 4){
			tot += ss[0];
			tot += ss[1];
			tot += ss[2];
			tot += ss[3];
			ss+=4;count-=4;
		}
		if(count >= 2){
			tot += ss[0];
			tot += ss[1];
			ss+=2;count-=2;
		}
		if(count)
			tot+=ss[0];
		/*for (size_t i = 0; i < thread_count_up; i+=2) {
			tot += sizes[i];
			tot += sizes[i +1];
		}*/
		return tot;
	}
	MICRO_ALWAYS_INLINE void sub(size_t thread_idx, size_t size) { 
		sizes[thread_idx] -= size;
		++ops[thread_idx];
	}
	size_t total_ops() const {
		size_t tot = 0;
		for (size_t i = 0; i < thread_count; ++i)
			tot += ops[i];
		return tot;
	}
};


template<class T>
void test_alloc_dealloc_thread(unsigned count, std::atomic<void*>* ptr, const size_t* order, unsigned* sizes, unsigned thread_idx, Counter* c)
{
	while (!start_compute.load(std::memory_order_relaxed))
		std::this_thread::yield();

	// do 5 passes
	for (int pass = 0; pass < 5; ++pass)
	for (size_t i = 0; i < count; i++) {
		size_t idx = (order)[i];
		void* p = (ptr)[idx].load(std::memory_order_relaxed);
		if (!p) {
			unsigned s = (sizes)[idx] + 4;
			void* m = T::alloc_mem(s);
			memcpy(m, &s, 4);
			size_t peak = c->add(thread_idx, s);

			if (!(ptr)[idx].compare_exchange_strong(p, m)) {
				T::free_mem(m);
				c->sub(thread_idx, s);
			}
			 else { 
				size_t m = max_allocated.load(std::memory_order_relaxed);
				if(peak > m) max_allocated.store(peak);
				/*{
					if(max_allocated.compare_exchange_strong(m,peak) ) 
						break;
				}*/
			}
		}
		else {
			void* m = (ptr)[idx].exchange(nullptr);
			if (m) {
				unsigned s = 0;
				memcpy(&s, m, 4);
				T::free_mem(m);
				c->sub(thread_idx, s);
			}
		}
	}

	// dealloc all
	for (size_t i = 0; i <count; i++) {
		void* m = (ptr)[i].exchange(nullptr);
		if (m) {
			unsigned s = 0;
			memcpy(&s, m, 4);
			T::free_mem(m);
			c->sub(thread_idx, s);
		}
	}
}
template<class T>
void test_allocator_simultaneous_alloc_dealloc(const char* allocator, size_t max_size, size_t max_mem)
{
	size_t additional, total_ops;

	{

		std::random_device dev;
		std::mt19937 rng(dev());
		std::uniform_int_distribution<std::mt19937::result_type> dist(0, max_size); // distribution in range [0, max_size]
		size_t alloc_count = max_mem / (max_size / 2);

		std::vector<unsigned, micro::testing_allocator<unsigned, T>> ss(alloc_count);
		for (size_t i = 0; i < ss.size(); ++i) {
			ss[i] = dist(rng) % (unsigned)max_size;
		}

		using size_t_vector = std::vector<size_t, micro::testing_allocator<size_t, T>>;
		size_t_vector order(ss.size());
		for (size_t i = 0; i < order.size(); ++i)
			order[i] = i;

		std::vector<size_t_vector, micro::testing_allocator<size_t_vector, T>> orders(thcount);
		for (unsigned i = 0; i < thcount; ++i) {
			orders[i] = order;
			micro::random_shuffle(orders[i].begin(), orders[i].end(), i);
		}
		std::atomic<void*>* ptr = (std::atomic<void*>*) T::alloc_mem(sizeof(std::atomic<void*>) * ss.size());
		for (size_t i = 0; i < ss.size(); ++i)
			new (ptr + i) std::atomic<void*>{ nullptr };

		additional = ss.size() * sizeof(unsigned);
		additional += ss.size() * sizeof(std::atomic<void*>);
		additional += order.size() * sizeof(size_t);
		for (size_t i = 0; i < orders.size(); ++i)
			additional += orders[i].size() * sizeof(size_t);

		Counter counter(thcount);
		
		std::thread threads[MAX_THREADS] ;
		for (size_t i = 0; i < thcount; ++i)
		{

			threads[i] = std::thread([&, i]() { test_alloc_dealloc_thread<T>(ss.size(), ptr, orders[i].data(), ss.data(), i, &counter); });
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
	el_alloc = micro::tock_ms();

	

	micro_process_infos infos;
	micro_get_process_infos(&infos);
	//std::cout << "Threads\tOps/s\tMemoryOverhead" << std::endl;
	double overhead = (infos.peak_rss - additional);
	overhead /= max_allocated.load();

	std::cout << thcount << "\t" << size_t(total_ops / ((double)el_alloc / 1000.)) << "\t" << overhead << std::endl;
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
	max_allocated = 0;
	//std::cout << "micro:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<Alloc>("micro", max_size,max_mem);
	//micro_clear();
	//std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	//print_process_infos();
#endif

#ifdef MICRO_BENCH_MALLOC
	start_compute = false;
	max_allocated = 0;
	//std::cout << "malloc:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<Malloc>("malloc", max_size, max_mem);
	//malloc_trim(0);
	//std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	//print_process_infos();
#endif

#ifdef MICRO_BENCH_JEMALLOC
	// const char* je_malloc_conf = "dirty_decay_ms:0";
	start_compute = false;
	max_allocated = 0;
	//std::cout << "jemalloc:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<Jemalloc>("jemalloc", max_size, max_mem);
	//std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	//print_process_infos();
#endif

#ifdef MICRO_BENCH_SNMALLOC
	start_compute = false;
	max_allocated = 0;
	//std::cout << "snmalloc:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<SnMalloc>("snmalloc", max_size, max_mem);
	//std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	//print_process_infos();

#endif

#ifdef MICRO_BENCH_MIMALLOC
	start_compute = false;
	max_allocated = 0;
	//std::cout << "mimalloc:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<MiMalloc>("mimalloc", max_size, max_mem);
	//std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	//mi_heap_collect(mi_heap_get_default(), true);
	//print_process_infos();

#endif

#ifdef USE_TBB
	start_compute = false;
	max_allocated = 0;
	//std::cout << "onetbb:" << std::endl;
	test_allocator_simultaneous_alloc_dealloc<TBBMalloc>("onetbb", max_size, max_mem);
	//std::cout << "Peak allocation: " << max_allocated.load() << std::endl;
	//print_process_infos();
#endif

	
	
	return 0;
}
