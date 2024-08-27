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

#define MAX_LOOP_COUNT 50
#define MAX_THREADS 500
#define MAX_SIZE 500

static std::atomic<bool> start_compute{ false };
static std::atomic<unsigned> finish_count{ 0 };

struct ThreadData
{
	micro::fast_rand rng;
	unsigned loop_count;

	ThreadData()
	  : rng((size_t)(uintptr_t)this)
	  , loop_count(0)
	{
	}
};


void loop_over(ThreadData * data, std::vector<std::atomic<void*>>& ptr)
{
	for (size_t i=0; i < ptr.size(); ++i) {
	
		if (!ptr[i].load(std::memory_order_relaxed)) {
			unsigned size = data->rng() % MAX_SIZE;
			void * p = micro_malloc(size);
			void* prev = ptr[i].exchange(p);
			if (prev)
				micro_free(prev);
		}
		else {
			void* prev = ptr[i].exchange(nullptr);
			if (prev)
				micro_free(prev);
		}
	}

	if (data->loop_count++ < MAX_LOOP_COUNT) {
		std::thread([&ptr, data]() { loop_over(data, ptr); }).detach();
	}
	else
		finish_count++;
}

void start_thread(ThreadData* data, std::vector<std::atomic<void*>>& ptr) 
{
	while (!start_compute.load())
		;
	loop_over(data, ptr);
}


int heavy_threads(int, char** const)
{
	std::vector<std::atomic<void*>> ptr(1000);
	std::fill_n(ptr.begin(), ptr.size(), nullptr);

	std::vector<ThreadData> data(MAX_THREADS);

	for (int i =0; i < MAX_THREADS; ++i)
		std::thread([&data,&ptr,i]() { loop_over(&data[i], ptr); }).detach();

	start_compute = true;

	while (finish_count.load(std::memory_order_relaxed) != MAX_THREADS)
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	return 0;
}