

#include <micro/micro.h>
#include <micro/testing.hpp>
#include <iostream>

typedef void* (*CUSTOM_MALLOC_P)(size_t);
typedef void (*CUSTOM_FREE_P)(void*);
static CUSTOM_MALLOC_P CUSTOM_MALLOC;
static CUSTOM_FREE_P CUSTOM_FREE;


#include <chrono>
#include <random>
#include <iostream>
#include <memory>



struct Deallocate
{
    void operator()(void* p) const noexcept
    {
        CUSTOM_FREE(p);
    }
};

static int bench(const char * name) {
    struct Deleter
    {
        const char* name;
        ~Deleter() {
            micro::allocator_trim(name);
        }
    };
    Deleter d{ name };
    static constexpr int kNumBuffers = 40;
    static constexpr size_t kMinBufferSize = 5 * 1024 * 1024;
    static constexpr size_t kMaxBufferSize = 25 * 1024 * 1024;
    using unique_ptr = std::unique_ptr<char, Deallocate>;
    unique_ptr buffers[kNumBuffers];

    //std::random_device rd;
    std::mt19937 gen(42); //rd());
    std::uniform_int_distribution<> size_distribution(kMinBufferSize, kMaxBufferSize);
    std::uniform_int_distribution<> buf_number_distribution(0, kNumBuffers - 1);

    static constexpr int kNumIterations = 20000;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kNumIterations; ++i) {
        int buffer_idx = buf_number_distribution(gen);
        size_t new_size = size_distribution(gen);
        buffers[buffer_idx] = unique_ptr((char*)CUSTOM_MALLOC(new_size));
    }

    

    const auto end = std::chrono::steady_clock::now();
    const auto num_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    const auto us_per_allocation = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / kNumIterations;
    std::cout << name<<": "<<kNumIterations << " allocations Done in " << num_ms << "ms." << std::endl;
    std::cout << "Avg " << us_per_allocation << " us per allocation" << std::endl << std::endl;
    micro::print_process_infos();
    return 0;
}

int malloc_large(int,  char** const)
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
    }
#endif

#ifdef MICRO_BENCH_MALLOC
    CUSTOM_MALLOC = malloc;
    CUSTOM_FREE = free;
    bench("malloc");
#endif

#ifdef MICRO_BENCH_JEMALLOC
    //const char* je_malloc_conf = "dirty_decay_ms:0";
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