// allocators benchmark, MIT licensed.
// - rlyeh 2013
// Taken from https://github.com/r-lyeh-archived/malloc-survey/blob/master/test.cpp
// @todo
// speed: ms
// memory leak: yes/no
// fragmentation
// memory occupancy
// dynamic growth vs fixed pool
// initial heap size
// small allocs tests
// big alloc tests
// alignment: no/4/8/16
// containers: bench most sequential and associative containers (vector, set, deque, map, unordered_map, unordered_set)
//
// portability: win, posix, unix, mac, ios, android
// flexibility: malloc/free, realloc, std::allocator, new/delete
//   bonus-point: sizealloc()
// integrability: header-only = 10, pair = 6, many = 3, 3rdparty = 0
// has-test-suites
// last-activity-on-repo <3mo, <6mo, <1yo, <2yo
// documentation: yes/no
// compactness: LOC
// open-source: yes/no
//  bonus-point: github +1, gitorius +1, bitbucket 0, sourceforge -1 :D, googlecode -1 :D
// license:
//  bonus-point: permissive-licenses: pd/zlib/mit/bsd3


#include <cassert>
#include <list>
#include <vector>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <map>
#include <set>
#include <tuple>
#include <thread>
#include <chrono>

#include "micro/testing.hpp"

#if 0
// comment out before making an .exe just to be sure these are not defined elsewhere,
// then comment it back and recompile.
void* operator new(size_t size) { return std::malloc(size); }
void* operator new[](size_t size) { return std::malloc(size); }
void operator delete(void* ptr) { return std::free(ptr); }
void operator delete[](void* ptr) { return std::free(ptr); }
#endif


std::map< double, std::set< std::string > > ranking;


enum { THREAD_SAFE, SAFE_DELETE, RESET_MEMORY, AVG_SPEED };
std::map< std::string, std::tuple<bool, bool, bool, double> > feat;
double default_allocator_time = 1;


/* #if defined (NDEBUG) || defined(_NDEBUG)
#define $release(...) __VA_ARGS__
#define $debug(...)
#else
#define $release(...)
#define $debug(...)   __VA_ARGS__
#endif

#define $stringize(...) #__VA_ARGS__
#define $string(...) std::to_string(__VA_ARGS__)
*/
/*
bool &feature( int what )  {
    if( what == THREAD_SAFE ) return

    static bool dummy;
    return dummy = false;
}
*/

// timing - rlyeh, public domain [ref] https://gist.github.com/r-lyeh/07cc318dbeee9b616d5e {


struct timing {
    static double now() {
        static auto const epoch = std::chrono::steady_clock::now(); // milli ms > micro us > nano ns
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - epoch).count() / 1000000.0;
    }
    template<typename FN>
    static double bench(const FN& fn) {
        auto took = -now();
        return (fn(), took + now());
    }
    static void sleep(double secs) {
        std::chrono::microseconds duration((int)(secs * 1000000));
        std::this_thread::sleep_for(duration);
    }
};
// } timing


template<typename TEST>
void benchmark_suite(int mode, const std::string& name) {

    double took = 0;

    {
        TEST container;

            auto single = [&]() {
            for (int j = 0; j < 30000; ++j)
            {
                TEST s;
                for (int i = 0; i < 100; ++i)
                    s.push_back(i);
                TEST s2 = s;
            }
        };

        auto multi = [&]() {
            std::thread th1([=] { single(); });
            std::thread th2([=] { single(); });
            std::thread th3([=] { single(); });
            std::thread th4([=] { single(); });
            th1.join();
            th2.join();
            th3.join();
            th4.join();
        };

        auto creator = [](TEST& s) {
            s = TEST();
            for (int j = 0; j < 30000; ++j)
                for (int i = 0; i < 100; ++i)
                    s.push_back(i);
        };

        auto deleter = [](TEST& s) {
            s = TEST();
        };

        auto deleter2 = [](typename TEST::value_type*& s) {
            typename TEST::allocator_type alloc;
            alloc.deallocate(s, 1);
        };

        auto creator2 = [](typename TEST::value_type*& s) {
            typename TEST::allocator_type alloc;
            s = alloc.allocate(1);
        };

        std::get<THREAD_SAFE>(feat[name]) = false;
        std::get<SAFE_DELETE>(feat[name]) = false;
        std::get<RESET_MEMORY>(feat[name]) = false;
        std::get<AVG_SPEED>(feat[name]) = 0;

        if (mode == 0)
            return;



        if (mode & 1) {
            std::string id = std::string() + "single: " + name;
            std::cout << id;
            double span = timing::now();
            single();
            took += (timing::now() - span) * 1000000;
            std::cout << " " << int(took) << "us" << std::endl;
#if 1
            int* x = 0;
            creator2(x);
            if (x == 0) throw std::runtime_error("bad test");
            if (*x == 0) std::get<RESET_MEMORY>(feat[name]) = true;
            deleter2(x);
            if (x == 0) std::get<SAFE_DELETE>(feat[name]) = true;
#endif
        }
        if (mode & 2) {
            {
                std::string id = std::string() + "multi: " + name;
                std::cout << id;
                double span = timing::now();
                multi();
                took += (timing::now() - span) * 1000000;
                std::cout << " " << int(took) << "us" << std::endl;
            }
            if (1)
            {
                std::string id = std::string() + "owner: " + name;
                std::cout << id;
                double span = timing::now();
                TEST s;
                std::thread th1([&]() { creator(s); }); th1.join();
                std::thread th2([&]() { deleter(s); }); th2.join();
#if 1
                int* x = 0;
                std::thread th3([&]() { creator2(x); }); th3.join();
                if (x == 0) throw std::runtime_error("bad test");
                if (*x == 0) std::get<RESET_MEMORY>(feat[name]) = true;
                std::thread th4([&]() { deleter2(x); }); th4.join();
                if (x == 0) std::get<SAFE_DELETE>(feat[name]) = true;
#endif
                took += (timing::now() - span) * 1000000;
                std::cout << " " << int(took) << "us" << std::endl;
            }

            std::get<THREAD_SAFE>(feat[name]) = true;
        }

    }

    // trim
    double span = timing::now();
    micro::allocator_trim(name.c_str());
    took += (timing::now() - span) * 1000000;

    /**/ if (mode & 3) took /= 3;
    else if (mode & 2) took /= 2;
    else               took /= 1;

    std::get<AVG_SPEED>(feat[name]) = took;
    ranking[took].insert(name);

    if (name == "malloc")
        default_allocator_time = took;

    micro::print_process_infos();
}

int malloc_survey(int, char** const)
{
	//micro_set_parameter(MicroBackendMemory, 100000000ull);
    try {

        auto header = [](const std::string& title) {
            std::cout << std::endl;
            std::cout << "+-" << std::string(title.size(), '-') << "-+" << std::endl;
            std::cout << "| " << title << " |X" << std::endl;
            std::cout << "+-" << std::string(title.size(), '-') << "-+X" << std::endl;
            std::cout << " " << std::string(title.size() + 4, 'X') << std::endl;
            std::cout << std::endl;
        };

        header("running tests");

        enum { none = 0, single = 1, thread = 2, all = ~0 };
        // some suites got single only because... { - they crashed, or; - they deadlocked, or; - they took more than 30 secs to finish }

        benchmark_suite< std::list<int, micro::heap_allocator<int> >>(all, "micro");
        benchmark_suite< std::list<int, std::allocator<int> >>(all, "malloc");
#ifdef MICRO_BENCH_JEMALLOC
        benchmark_suite< std::list<int, micro::testing_allocator<int, micro::Jemalloc> >>(all, "jemalloc");
#endif
#ifdef MICRO_BENCH_MIMALLOC
        benchmark_suite<std::list<int, micro::testing_allocator<int, micro::MiMalloc> >>(all, "mimalloc");
#endif
#ifdef MICRO_BENCH_SNMALLOC
	benchmark_suite<std::list<int, micro::testing_allocator<int, micro::SnMalloc>>>(all, "snmalloc");
#endif
#ifdef USE_TBB
        benchmark_suite<std::list<int, micro::testing_allocator<int, micro::TBBMalloc> >>(all, "onetbb");
#endif
       
        header(std::string() + "comparison table " );

        std::cout << "       " << std::string(40, ' ') + "THS RSM SFD AVG" << std::endl;
        int pos = 1;
        for (const auto& result : ranking) {
            const auto& mark = result.first;
            for (const auto& name : result.second) {
                const auto& values = feat[name];
                if (pos < 10) std::cout << " ";
                /**/ if (pos % 10 == 1) std::cout << pos++ << "st)";
                else if (pos % 10 == 2) std::cout << pos++ << "nd)";
                else if (pos % 10 == 3) std::cout << pos++ << "rd)";
                else                     std::cout << pos++ << "th)";
                std::cout << " " << (name)+std::string(40 - name.size(), ' ');
                std::cout << " " << (std::get<THREAD_SAFE >(values) ? "[x]" : "[ ]");
                std::cout << " " << (std::get<RESET_MEMORY>(values) ? "[x]" : "[ ]");
                std::cout << " " << (std::get<SAFE_DELETE >(values) ? "[x]" : "[ ]");
                std::cout << " " << int(std::get<AVG_SPEED>(values)) << " us";
                double factor = std::get<AVG_SPEED>(values) / default_allocator_time;
                /**/ if (factor > 1.05) std::cout << " (x" << std::setprecision(3) << (factor) << " times slower)";
                else if (factor < 0.95) std::cout << " (x" << std::setprecision(3) << (1.0 / factor) << " times faster)";
                else                     std::cout << " (performs similar to standard allocator)";
                std::cout << std::endl;
            }
        }
        std::cout << std::endl;
        std::cout << "THS: THREAD_SAFE: safe to use in multithreaded scenarios (on is better)" << std::endl;
        std::cout << "RSM: RESET_MEMORY: allocated contents are reset to zero (on is better)" << std::endl;
        std::cout << "SFD: SAFE_DELETE: deallocated pointers are reset to zero (on is better)" << std::endl;
        std::cout << "AVG: AVG_SPEED: average time for each benchmark (lower is better)" << std::endl;

        return 0;

    }
    catch (std::exception& e) {
        std::cout << "exception thrown :( " << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "exception thrown :( no idea" << std::endl;
    }

    std::cout << "trying to invoke debugger..." << std::endl;
    assert(!"trying to invoke debugger...");

    return -1;
}
