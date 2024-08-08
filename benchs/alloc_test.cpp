/* -------------------------------------------------------------------------------
 * Copyright (c) 2018, OLogN Technologies AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * -------------------------------------------------------------------------------
 *
 * Memory allocator tester -- new-delete allocator
 *
 * v.1.00    Jun-22-2018    Initial release
 *
 * From https://github.com/node-dot-cpp/alloc-test/tree/master
 * -------------------------------------------------------------------------------*/

#include <micro/testing.hpp>
#include <micro/micro.hpp>

#include <memory>
#include <cstdint>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <stdint.h>
#define NOMINMAX

#include <memory>
#include <stdio.h>
#include <time.h>
#include <thread>
#include <assert.h>
#include <chrono>
#include <random>
#include <limits.h>

#ifndef __GNUC__
#include <intrin.h>
#else
#endif

#if _MSC_VER
#include <intrin.h>
#define ALIGN(n)      __declspec(align(n))
#define NOINLINE      __declspec(noinline)
#define FORCE_INLINE	__forceinline
#elif __GNUC__
#include <x86intrin.h>
#define ALIGN(n)      __attribute__ ((aligned(n))) 
#define NOINLINE      __attribute__ ((noinline))
#define	FORCE_INLINE inline __attribute__((always_inline))
#else
#define	FORCE_INLINE inline
#define NOINLINE
 //#define ALIGN(n)
#warning ALIGN, FORCE_INLINEand NOINLINE may not be properly defined
#endif

#define VERBOSE 0

int64_t GetMicrosecondCount();
size_t GetMillisecondCount();
size_t getRss();

constexpr size_t max_threads = 32;

enum MEM_ACCESS_TYPE { none, single, full, check };

#define COLLECT_USER_MAX_ALLOCATED

struct ThreadTestRes
{
	size_t threadID;

	size_t innerDur;

	uint64_t rdtscBegin;
	uint64_t rdtscSetup;
	uint64_t rdtscMainLoop;
	uint64_t rdtscExit;

	size_t rssMax;
	size_t allocatedAfterSetupSz;
#ifdef COLLECT_USER_MAX_ALLOCATED
	size_t allocatedMax;
#endif
};

inline
void printThreadStats(const char* prefix, ThreadTestRes& res)
{
	uint64_t rdtscTotal = res.rdtscExit - res.rdtscBegin;
	if(VERBOSE)printf("%s%zd: %zdms; %zd (%.2f | %.2f | %.2f);\n", prefix, res.threadID, res.innerDur, rdtscTotal, (res.rdtscSetup - res.rdtscBegin) * 100. / rdtscTotal, (res.rdtscMainLoop - res.rdtscSetup) * 100. / rdtscTotal, (res.rdtscExit - res.rdtscMainLoop) * 100. / rdtscTotal);
}

struct TestRes
{
	size_t duration;
	size_t cumulativeDuration;
	size_t rssMax;
	size_t allocatedAfterSetupSz;
	size_t rssAfterExitingAllThreads;
#ifdef COLLECT_USER_MAX_ALLOCATED
	size_t allocatedMax;
#endif
	ThreadTestRes threadRes[max_threads];
};

struct TestStartupParams
{
	size_t threadCount;
	size_t maxItems;
	size_t maxItemSize;
	size_t iterCount;
	MEM_ACCESS_TYPE mat;
	size_t  rndSeed;
};

struct TestStartupParamsAndResults
{
	TestStartupParams startupParams;
	TestRes* testRes;
};

struct ThreadStartupParamsAndResults
{
	TestStartupParams startupParams;
	size_t threadID;
	ThreadTestRes* threadRes;
};


#include <stdint.h>
#include <assert.h>

#ifdef _MSC_VER
#include <Windows.h>
#else
#include <time.h>
#endif


int64_t GetMicrosecondCount()
{
	int64_t now = 0;
#ifdef _MSC_VER
	static int64_t frec = 0;
	if (frec == 0)
	{
		LARGE_INTEGER val;
		BOOL ok = QueryPerformanceFrequency(&val);
		assert(ok);
		frec = val.QuadPart;
	}
	LARGE_INTEGER val;
	BOOL ok = QueryPerformanceCounter(&val);
	assert(ok);
	now = (val.QuadPart * 1000000) / frec;
#endif
	return now;
}



NOINLINE
size_t GetMillisecondCount()
{
	size_t now;
#if defined(_MSC_VER) || defined(__MINGW32__)
	static uint64_t frec = 0;
	if (frec == 0)
	{
		LARGE_INTEGER val;
		BOOL ok = QueryPerformanceFrequency(&val);
		assert(ok);
		frec = val.QuadPart / 1000;
	}
	LARGE_INTEGER val;
	BOOL ok = QueryPerformanceCounter(&val);
	assert(ok);
	now = val.QuadPart / frec;

#else
#if 1
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);//clock get time monotonic
	now = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; // mks
#else
	struct timeval now_;
	gettimeofday(&now_, NULL);
	now = now_.tv_sec;
	now *= 1000;
	now += now_.tv_usec / 1000000;
#endif
#endif
	return now;
}

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <psapi.h>
size_t getRss()
{
	HANDLE hProcess;
	PROCESS_MEMORY_COUNTERS pmc;
	hProcess = GetCurrentProcess();
	BOOL ok = GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
	CloseHandle(hProcess);
	if (ok)
		return pmc.PagefileUsage >> 12; // note: we may also be interested in 'PeakPagefileUsage'
	else
		return 0;
}
#else
size_t getRss()
{
	// see http://man7.org/linux/man-pages/man5/proc.5.html for details
	FILE* fstats = fopen("/proc/self/statm", "rb");
	constexpr size_t buffsz = 0x1000;
	char buff[buffsz];
	buff[buffsz - 1] = 0;
	fread(buff, 1, buffsz - 1, fstats);
	fclose(fstats);
	const char* pos = buff;
	while (*pos && *pos == ' ') ++pos;
	while (*pos && *pos != ' ') ++pos;
	return atol(pos);
}
#endif



template<class ActualAllocator>
class VoidAllocatorForTest
{
	ThreadTestRes* testRes;
	ThreadTestRes discardedTestRes;
	ActualAllocator alloc;
	uint8_t* fakeBuffer = nullptr;
	static constexpr size_t fakeBufferSize = 0x1000000;

public:
	VoidAllocatorForTest(ThreadTestRes* testRes_) : alloc(&discardedTestRes) { testRes = testRes_; }
	static constexpr bool isFake() { return true; } // thus indicating that certain checks over allocated memory should be ommited

	static constexpr const char* name() { return "void allocator"; }

	void init()
	{
		alloc.init();
		fakeBuffer = reinterpret_cast<uint8_t*>(alloc.allocate(fakeBufferSize));
	}
	void* allocateSlots(size_t sz) {  assert(sz <= fakeBufferSize); return alloc.allocate(sz); }
	void* allocate(size_t sz) { assert(sz <= fakeBufferSize); return fakeBuffer; }
	void deallocate(void* ptr) {}
	void deallocateSlots(void* ptr) { alloc.deallocate(ptr); }
	void deinit() { if (fakeBuffer) alloc.deallocate(fakeBuffer); fakeBuffer = nullptr; }

	// next calls are to get additional stats of the allocator, etc, if desired
	void doWhateverAfterSetupPhase() {}
	void doWhateverAfterMainLoopPhase() {}
	void doWhateverAfterCleanupPhase() {}

	ThreadTestRes* getTestRes() { return testRes; }
};




class PRNG
{
	uint64_t seedVal;
public:
	PRNG() { seedVal = 0; }
	PRNG(size_t seed_) { seedVal = seed_; }
	void seed(size_t seed_) { seedVal = seed_; }

	/*FORCE_INLINE uint32_t rng32( uint32_t x )
	{
		// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
		x ^= x << 13;
		x ^= x >> 17;
		x ^= x << 5;
		return x;
	}*/
	/*	FORCE_INLINE uint32_t rng32()
		{
			unsigned long long x = (seedVal += 7319936632422683443ULL);
			x ^= x >> 32;
			x *= c;
			x ^= x >> 32;
			x *= c;
			x ^= x >> 32;
			return uint32_t(x);
		}*/
	FORCE_INLINE uint32_t rng32()
	{
		// based on implementation of xorshift by Arvid Gerstmann
		// see, for instance, https://arvid.io/2018/07/02/better-cxx-prng/
		uint64_t ret = seedVal * 0xd989bcacc137dcd5ull;
		seedVal ^= seedVal >> 11;
		seedVal ^= seedVal << 31;
		seedVal ^= seedVal >> 18;
		return uint32_t(ret >> 32ull);
	}

	FORCE_INLINE uint64_t rng64()
	{
		uint64_t ret = rng32();
		ret <<= 32;
		return ret + rng32();
	}
};

FORCE_INLINE size_t calcSizeWithStatsAdjustment(uint64_t randNum, size_t maxSizeExp)
{
	assert(maxSizeExp >= 3);
	maxSizeExp -= 3;
	uint32_t statClassBase = (randNum & ((1 << maxSizeExp) - 1)) + 1; // adding 1 to avoid dealing with 0
	randNum >>= maxSizeExp;
	unsigned long idx;
#if _MSC_VER
	uint8_t r = _BitScanForward(&idx, statClassBase);
	assert(r);
#elif __GNUC__
	idx = __builtin_ctzll(statClassBase);
#else
	static_assert(false, "Unknown compiler");
#endif
	//	assert( idx <= maxSizeExp - 3 );
	assert(idx <= maxSizeExp);
	idx += 2;
	size_t szMask = (1 << idx) - 1;
	return (randNum & szMask) + 1 + (((size_t)1) << idx);
}

inline void testDistribution()
{
	constexpr size_t exp = 16;
	constexpr size_t testCnt = 0x100000;
	size_t bins[exp + 1];
	memset(bins, 0, sizeof(bins));
	size_t total = 0;

	PRNG rng;

	for (size_t i = 0; i < testCnt; ++i)
	{
		size_t val = calcSizeWithStatsAdjustment(rng.rng64(), exp);
		//		assert( val <= (((size_t)1)<<exp) );
		assert(val);
		if (val <= 8)
			bins[3] += 1;
		else
			for (size_t j = 4; j <= exp; ++j)
				if (val <= (((size_t)1) << j) && val > (((size_t)1) << (j - 1)))
					bins[j] += 1;
	}
	if (VERBOSE)printf("<=3: %zd\n", bins[0] + bins[1] + bins[2] + bins[3]);
	total = 0;
	for (size_t j = 0; j <= exp; ++j)
	{
		total += bins[j];
		if (VERBOSE)printf("%zd: %zd\n", j, bins[j]);
	}
	assert(total == testCnt);
}


constexpr double Pareto_80_20_6[7] = {
	0.262144000000,
	0.393216000000,
	0.245760000000,
	0.081920000000,
	0.015360000000,
	0.001536000000,
	0.000064000000 };

struct Pareto_80_20_6_Data
{
	uint32_t probabilityRanges[6];
	uint32_t offsets[8];
};

FORCE_INLINE
void Pareto_80_20_6_Init(Pareto_80_20_6_Data& data, uint32_t itemCount)
{
	data.probabilityRanges[0] = (uint32_t)(UINT32_MAX * Pareto_80_20_6[0]);
	data.probabilityRanges[5] = (uint32_t)(UINT32_MAX * (1. - Pareto_80_20_6[6]));
	for (size_t i = 1; i < 5; ++i)
		data.probabilityRanges[i] = data.probabilityRanges[i - 1] + (uint32_t)(UINT32_MAX * Pareto_80_20_6[i]);
	data.offsets[0] = 0;
	data.offsets[7] = itemCount;
	for (size_t i = 0; i < 6; ++i)
		data.offsets[i + 1] = data.offsets[i] + (uint32_t)(itemCount * Pareto_80_20_6[6 - i]);
}

FORCE_INLINE
size_t Pareto_80_20_6_Rand(const Pareto_80_20_6_Data& data, uint32_t rnum1, uint32_t rnum2)
{
	size_t idx = 6;
	if (rnum1 < data.probabilityRanges[0])
		idx = 0;
	else if (rnum1 < data.probabilityRanges[1])
		idx = 1;
	else if (rnum1 < data.probabilityRanges[2])
		idx = 2;
	else if (rnum1 < data.probabilityRanges[3])
		idx = 3;
	else if (rnum1 < data.probabilityRanges[4])
		idx = 4;
	else if (rnum1 < data.probabilityRanges[5])
		idx = 5;
	uint32_t rangeSize = data.offsets[idx + 1] - data.offsets[idx];
	uint32_t offsetInRange = rnum2 % rangeSize;
	return data.offsets[idx] + offsetInRange;
}

void fillSegmentWithRandomData(uint8_t* ptr, size_t sz, size_t reincarnation)
{
	PRNG rng(((uintptr_t)ptr) ^ ((uintptr_t)sz << 32) ^ reincarnation);
	for (size_t i = 0; i < (sz >> 2); ++i)
		(reinterpret_cast<uint32_t*>(ptr))[i] = rng.rng32();
	ptr += (sz >> 2) << 2;
	if (sz & 3)
	{
		uint32_t last = rng.rng32();
		for (size_t i = 0; i < (sz & 3); ++i)
		{
			(ptr)[i] = (uint8_t)last;
			last >>= 8;
		}
	}
}
void checkSegment(uint8_t* ptr, size_t sz, size_t reincarnation)
{
	PRNG rng(((uintptr_t)ptr) ^ ((uintptr_t)sz << 32) ^ reincarnation);
	for (size_t i = 0; i < (sz >> 2); ++i)
		if ((reinterpret_cast<uint32_t*>(ptr))[i] != rng.rng32())
		{
			printf("memcheck failed for ptr=%zd, size=%zd, reincarnation=%zd, from %zd\n", (size_t)(ptr), sz, reincarnation, i * 4);
			throw std::bad_alloc();
		}
	ptr += (sz >> 2) << 2;
	if (sz & 3)
	{
		uint32_t last = rng.rng32();
		for (size_t i = 0; i < (sz & 3); ++i)
		{
			if ((ptr)[i] != (uint8_t)last)
			{
				printf("memcheck failed for ptr=%zd, size=%zd, reincarnation=%zd, from %zd\n", (size_t)(ptr), sz, reincarnation, ((sz >> 2) << 2) + i);
				throw std::bad_alloc();
			}
			last >>= 8;
		}
	}
}

template< class AllocatorUnderTest, MEM_ACCESS_TYPE mat>
void randomPos_RandomSize(AllocatorUnderTest& allocatorUnderTest, size_t iterCount, size_t maxItems, size_t maxItemSizeExp, size_t threadID, size_t rnd_seed)
{
	if (maxItemSizeExp >= 32)
	{
		printf("allocation sizes greater than 2^31 are not yet supported; revise implementation, if desired\n");
		throw std::bad_exception();
	}

	static constexpr const char* memAccessTypeStr = mat == MEM_ACCESS_TYPE::none ? "none" : (mat == MEM_ACCESS_TYPE::single ? "single" : (mat == MEM_ACCESS_TYPE::full ? "full" : (mat == MEM_ACCESS_TYPE::check ? "check" : "unknown")));
	if (VERBOSE)printf("    running thread %zd with \'%s\' and maxItemSizeExp = %zd, maxItems = %zd, iterCount = %zd, allocated memory access mode: %s,  [rnd_seed = %llu] ...\n", threadID, allocatorUnderTest.name(), maxItemSizeExp, maxItems, iterCount, memAccessTypeStr, rnd_seed);
	constexpr bool doMemAccess = mat != MEM_ACCESS_TYPE::none;
	allocatorUnderTest.init();
	allocatorUnderTest.getTestRes()->threadID = threadID; // just as received
	allocatorUnderTest.getTestRes()->rdtscBegin = __rdtsc();

	size_t start = GetMillisecondCount();

	size_t dummyCtr = 0;
	size_t rssMax = 0;
	size_t rss;
	size_t allocatedSz = 0;
	size_t allocatedSzMax = 0;

	uint32_t reincarnation = 0;

	Pareto_80_20_6_Data paretoData;
	assert(maxItems <= UINT32_MAX);
	Pareto_80_20_6_Init(paretoData, (uint32_t)maxItems);

	struct TestBin
	{
		uint8_t* ptr;
		uint32_t sz;
		uint32_t reincarnation;
	};

	TestBin* baseBuff = nullptr;
	if (!allocatorUnderTest.isFake())
		baseBuff = reinterpret_cast<TestBin*>(allocatorUnderTest.allocate(maxItems * sizeof(TestBin)));
	else
		baseBuff = reinterpret_cast<TestBin*>(allocatorUnderTest.allocateSlots(maxItems * sizeof(TestBin)));
	assert(baseBuff);
	allocatedSz += maxItems * sizeof(TestBin);
	memset(baseBuff, 0, maxItems * sizeof(TestBin));

	PRNG rng;

	// setup (saturation)
	for (size_t i = 0; i < maxItems / 32; ++i)
	{
		uint32_t randNum = rng.rng32();
		for (size_t j = 0; j < 32; ++j)
			if ((randNum >> j) & 1)
			{
				size_t randNumSz = rng.rng64();
				size_t sz = calcSizeWithStatsAdjustment(randNumSz, maxItemSizeExp);
				baseBuff[i * 32 + j].sz = (uint32_t)sz;
				baseBuff[i * 32 + j].ptr = reinterpret_cast<uint8_t*>(allocatorUnderTest.allocate(sz));
				if (doMemAccess)
				{
					if (mat == MEM_ACCESS_TYPE::full)
						memset(baseBuff[i * 32 + j].ptr, (uint8_t)sz, sz);
					else
					{
						if (mat == MEM_ACCESS_TYPE::single)
							baseBuff[i * 32 + j].ptr[sz / 2] = (uint8_t)sz;
						else
						{
							//static_assert(mat == MEM_ACCESS_TYPE::check, "");
							baseBuff[i * 32 + j].reincarnation = reincarnation;
							fillSegmentWithRandomData(baseBuff[i * 32 + j].ptr, sz, reincarnation++);
						}
					}
				}
				allocatedSz += sz;
			}
	}
	allocatorUnderTest.doWhateverAfterSetupPhase();
	allocatorUnderTest.getTestRes()->rdtscSetup = __rdtsc();
	allocatorUnderTest.getTestRes()->allocatedAfterSetupSz = allocatedSz;

	rss = getRss();
	if (rssMax < rss) rssMax = rss;

	// main loop
	for (size_t k = 0; k < 32; ++k)
	{
		for (size_t j = 0; j < iterCount >> 5; ++j)
		{
			uint32_t rnum1 = rng.rng32();
			uint32_t rnum2 = rng.rng32();
			size_t idx = Pareto_80_20_6_Rand(paretoData, rnum1, rnum2);
			if (baseBuff[idx].ptr)
			{
				if (doMemAccess)
				{
					if (mat == MEM_ACCESS_TYPE::full)
					{
						size_t i = 0;
						for (; i < baseBuff[idx].sz / sizeof(size_t); ++i)
							dummyCtr += (reinterpret_cast<size_t*>(baseBuff[idx].ptr))[i];
						uint8_t* tail = baseBuff[idx].ptr + i * sizeof(size_t);
						for (i = 0; i < baseBuff[idx].sz % sizeof(size_t); ++i)
							dummyCtr += tail[i];
					}
					else
					{
						if (mat == MEM_ACCESS_TYPE::single)
							dummyCtr += baseBuff[idx].ptr[baseBuff[idx].sz / 2];
						else
						{
							//static_assert(mat == MEM_ACCESS_TYPE::check, "");
							checkSegment(baseBuff[idx].ptr, baseBuff[idx].sz, baseBuff[idx].reincarnation);
						}
					}
				}
#ifdef COLLECT_USER_MAX_ALLOCATED
				allocatedSz -= baseBuff[idx].sz;
#endif
				allocatorUnderTest.deallocate(baseBuff[idx].ptr);
				baseBuff[idx].ptr = 0;
			}
			else
			{
				size_t sz = calcSizeWithStatsAdjustment(rng.rng64(), maxItemSizeExp);
				baseBuff[idx].sz = (uint32_t)sz;
				baseBuff[idx].ptr = reinterpret_cast<uint8_t*>(allocatorUnderTest.allocate(sz));
				if  (doMemAccess)
				{
					if  (mat == MEM_ACCESS_TYPE::full)
						memset(baseBuff[idx].ptr, (uint8_t)sz, sz);
					else
					{
						if  (mat == MEM_ACCESS_TYPE::single)
							baseBuff[idx].ptr[sz / 2] = (uint8_t)sz;
						else
						{
							//static_assert(mat == MEM_ACCESS_TYPE::check, "");
							baseBuff[idx].reincarnation = reincarnation;
							fillSegmentWithRandomData(baseBuff[idx].ptr, sz, reincarnation++);
						}
					}
				}
#ifdef COLLECT_USER_MAX_ALLOCATED
				allocatedSz += sz;
				if (allocatedSzMax < allocatedSz)
					allocatedSzMax = allocatedSz;
#endif
			}
		}
		rss = getRss();
		if (rssMax < rss) rssMax = rss;
	}
	allocatorUnderTest.doWhateverAfterMainLoopPhase();
	allocatorUnderTest.getTestRes()->rdtscMainLoop = __rdtsc();
	allocatorUnderTest.getTestRes()->allocatedMax = allocatedSzMax;

	// exit
	for (size_t idx = 0; idx < maxItems; ++idx)
		if (baseBuff[idx].ptr)
		{
			if MICRO_CONSTEXPR (doMemAccess)
			{
				if MICRO_CONSTEXPR(mat == MEM_ACCESS_TYPE::full)
				{
					size_t i = 0;
					for (; i < baseBuff[idx].sz / sizeof(size_t); ++i)
						dummyCtr += (reinterpret_cast<size_t*>(baseBuff[idx].ptr))[i];
					uint8_t* tail = baseBuff[idx].ptr + i * sizeof(size_t);
					for (i = 0; i < baseBuff[idx].sz % sizeof(size_t); ++i)
						dummyCtr += tail[i];
				}
				else
				{
					if MICRO_CONSTEXPR(mat == MEM_ACCESS_TYPE::single)
						dummyCtr += baseBuff[idx].ptr[baseBuff[idx].sz / 2];
					else
					{
						//static_assert(mat == MEM_ACCESS_TYPE::check, "");
						checkSegment(baseBuff[idx].ptr, baseBuff[idx].sz, baseBuff[idx].reincarnation);
					}
				}
			}
			allocatorUnderTest.deallocate(baseBuff[idx].ptr);
		}

	if MICRO_CONSTEXPR(!allocatorUnderTest.isFake())
		allocatorUnderTest.deallocate(baseBuff);
	else
		allocatorUnderTest.deallocateSlots(baseBuff);
	allocatorUnderTest.deinit();
	allocatorUnderTest.getTestRes()->rdtscExit = __rdtsc();
	allocatorUnderTest.getTestRes()->innerDur = GetMillisecondCount() - start;
	allocatorUnderTest.doWhateverAfterCleanupPhase();

	rss = getRss();
	if (rssMax < rss) rssMax = rss;
	allocatorUnderTest.getTestRes()->rssMax = rssMax;

	if (VERBOSE)printf("about to exit thread %zd (%zd operations performed) [ctr = %zd]...\n", threadID, iterCount, dummyCtr);
};




template<class Allocator>
void* runRandomTest(void* params)
{
	assert(params != nullptr);
	ThreadStartupParamsAndResults* testParams = reinterpret_cast<ThreadStartupParamsAndResults*>(params);
	Allocator allocator(testParams->threadRes);
	switch (testParams->startupParams.mat)
	{
	case MEM_ACCESS_TYPE::none:
		randomPos_RandomSize<Allocator, MEM_ACCESS_TYPE::none>(allocator, testParams->startupParams.iterCount, testParams->startupParams.maxItems, testParams->startupParams.maxItemSize, testParams->threadID, testParams->startupParams.rndSeed);
		break;
	case MEM_ACCESS_TYPE::full:
		randomPos_RandomSize<Allocator, MEM_ACCESS_TYPE::full>(allocator, testParams->startupParams.iterCount, testParams->startupParams.maxItems, testParams->startupParams.maxItemSize, testParams->threadID, testParams->startupParams.rndSeed);
		break;
	case MEM_ACCESS_TYPE::single:
		randomPos_RandomSize<Allocator, MEM_ACCESS_TYPE::single>(allocator, testParams->startupParams.iterCount, testParams->startupParams.maxItems, testParams->startupParams.maxItemSize, testParams->threadID, testParams->startupParams.rndSeed);
		break;
	case MEM_ACCESS_TYPE::check:
		randomPos_RandomSize<Allocator, MEM_ACCESS_TYPE::check>(allocator, testParams->startupParams.iterCount, testParams->startupParams.maxItems, testParams->startupParams.maxItemSize, testParams->threadID, testParams->startupParams.rndSeed);
		break;
	}

	return nullptr;
}

template<class Allocator>
void runTest(TestStartupParamsAndResults* startupParams)
{
	size_t threadCount = startupParams->startupParams.threadCount;

	size_t start = GetMillisecondCount();

	ThreadStartupParamsAndResults testParams[max_threads];
	std::thread threads[max_threads];

	for (size_t i = 0; i < threadCount; ++i)
	{
		memcpy(testParams + i, startupParams, sizeof(TestStartupParams));
		testParams[i].threadID = i;
		testParams[i].threadRes = startupParams->testRes->threadRes + i;
	}

	// run threads
	for (size_t i = 0; i < threadCount; ++i)
	{
		if (VERBOSE)printf("about to run thread %zd...\n", i);
		std::thread t1(runRandomTest<Allocator>, (void*)(testParams + i));
		threads[i] = std::move(t1);
		if (VERBOSE)printf("    ...done\n");
	}
	// join threads
	for (size_t i = 0; i < threadCount; ++i)
	{
		if (VERBOSE)printf("joining thread %zd...\n", i);
		threads[i].join();
		if (VERBOSE)printf("    ...done\n");
	}

	size_t end = GetMillisecondCount();
	startupParams->testRes->duration = end - start;
	if (VERBOSE)printf("%zd threads made %zd alloc/dealloc operations in %zd ms (%zd ms per 1 million)\n", threadCount, startupParams->startupParams.iterCount * threadCount, end - start, (end - start) * 1000000 / (startupParams->startupParams.iterCount * threadCount));
	startupParams->testRes->cumulativeDuration = 0;
	startupParams->testRes->rssMax = 0;
	startupParams->testRes->allocatedAfterSetupSz = 0;
	startupParams->testRes->allocatedMax = 0;
	for (size_t i = 0; i < threadCount; ++i)
	{
		startupParams->testRes->cumulativeDuration += startupParams->testRes->threadRes[i].innerDur;
		startupParams->testRes->allocatedAfterSetupSz += startupParams->testRes->threadRes[i].allocatedAfterSetupSz;
		startupParams->testRes->allocatedMax += startupParams->testRes->threadRes[i].allocatedMax;
		if (startupParams->testRes->rssMax < startupParams->testRes->threadRes[i].rssMax)
			startupParams->testRes->rssMax = startupParams->testRes->threadRes[i].rssMax;
	}
	startupParams->testRes->cumulativeDuration /= threadCount;
	startupParams->testRes->rssAfterExitingAllThreads = getRss();
}

template<class MyAllocatorT>
int runTest()
{
	TestRes testResMyAlloc[max_threads];
	TestRes testResVoidAlloc[max_threads];
	memset(testResMyAlloc, 0, sizeof(testResMyAlloc));
	memset(testResVoidAlloc, 0, sizeof(testResVoidAlloc));

	size_t maxItems = 1 << 25;
	TestStartupParamsAndResults params;
	params.startupParams.iterCount = 20000000;
	params.startupParams.maxItemSize = 16;
	//		params.startupParams.maxItems = 23 << 20;
	params.startupParams.mat = MEM_ACCESS_TYPE::full;

	size_t threadMin = 1;
	size_t threadMax = 8;

	for (params.startupParams.threadCount = threadMin; params.startupParams.threadCount <= threadMax; ++(params.startupParams.threadCount))
	{
		params.startupParams.maxItems = maxItems / params.startupParams.threadCount;
		params.testRes = testResMyAlloc + params.startupParams.threadCount;
		runTest<MyAllocatorT>(&params);

		if (params.startupParams.mat != MEM_ACCESS_TYPE::check)
		{
			params.startupParams.maxItems = maxItems / params.startupParams.threadCount;
			params.testRes = testResVoidAlloc + params.startupParams.threadCount;
			runTest<VoidAllocatorForTest<MyAllocatorT>>(&params);
		}
	}

	if (params.startupParams.mat == MEM_ACCESS_TYPE::check)
	{
		printf("Correctness test has been passed successfully\n");
		return 0;
	}

	if (VERBOSE)printf("Test summary:\n");
	for (size_t threadCount = threadMin; threadCount <= threadMax; ++threadCount)
	{
		TestRes& trVoid = testResVoidAlloc[threadCount];
		TestRes& trMy = testResMyAlloc[threadCount];
		if (VERBOSE) {
			printf("%zd,%zd,%zd,%zd\n", threadCount, trMy.duration, trVoid.duration, trMy.duration - trVoid.duration);
			printf("Per-thread stats:\n");
			for (size_t i = 0; i < threadCount; ++i)
			{
				printf("   %zd:\n", i);
				printThreadStats("\t", trMy.threadRes[i]);
			}
		}
	}
	printf("\n");
	const char* memAccessTypeStr = params.startupParams.mat == MEM_ACCESS_TYPE::none ? "none" : (params.startupParams.mat == MEM_ACCESS_TYPE::single ? "single" : (params.startupParams.mat == MEM_ACCESS_TYPE::full ? "full" : "unknown"));
	printf("Short test summary for \'%s\' and maxItemSizeExp = %zd, maxItems = %zd, iterCount = %zd, allocated memory access mode: %s:\n", MyAllocatorT::name(), params.startupParams.maxItemSize, maxItems, params.startupParams.iterCount, memAccessTypeStr);
	/*printf("columns:\n");
	printf("thread,duration(ms),duration of void(ms),diff(ms),RSS max(pages),rssAfterExitingAllThreads(pages),RSS max for void(pages),rssAfterExitingAllThreads for void(pages),allocatedAfterSetup(app level,bytes),allocatedMax(app level,bytes),(RSS max<<12)/allocatedMax\n");
	for (size_t threadCount = threadMin; threadCount <= threadMax; ++threadCount)
	{
		TestRes& trVoid = testResVoidAlloc[threadCount];
		TestRes& trMy = testResMyAlloc[threadCount];
		printf("%zd,%zd,%zd,%zd,%zd,%zd,%zd,%zd,%zd,%zd,%f\n", threadCount, trMy.duration, trVoid.duration, trMy.duration - trVoid.duration, trMy.rssMax, trMy.rssAfterExitingAllThreads, trVoid.rssMax, trVoid.rssAfterExitingAllThreads, trMy.allocatedAfterSetupSz, trMy.allocatedMax, (trMy.rssMax << 12) * 1. / trMy.allocatedMax);
	}*/

	printf("Threads\t\tDuration(ms)\tMemoryOverhead\n");
	for (size_t threadCount = threadMin; threadCount <= threadMax; ++threadCount)
	{
		TestRes& trVoid = testResVoidAlloc[threadCount];
		TestRes& trMy = testResMyAlloc[threadCount];
		printf("%i\t\t%i\t\t%f\n", (int)threadCount, (int)(trMy.duration - trVoid.duration), (trMy.rssMax << 12) * 1. / trMy.allocatedMax);
	}

	/*	printf( "Short test summary for USE_RANDOMPOS_RANDOMSIZE (alt computations):\n" );
		for ( size_t threadCount=threadMin; threadCount<=threadMax; ++threadCount )
		{
			TestRes& trVoid = testResVoidAlloc[threadCount];
			TestRes& trMy = testResMyAlloc[threadCount];
			printf( "%zd,%zd,%zd,%zd,%zd,%zd,%zd,%zd,%zd,%zd,%f\n", threadCount, trMy.cumulativeDuration, trVoid.cumulativeDuration, trMy.cumulativeDuration - trVoid.cumulativeDuration, trMy.rssMax, trMy.rssAfterExitingAllThreads, trVoid.rssMax, trVoid.rssAfterExitingAllThreads, trMy.allocatedAfterSetupSz, trMy.allocatedMax, (trMy.rssMax << 12) * 1. / trMy.allocatedMax );
		}*/

	return 0;
}


static const char* CURRENT_NAME;

template<class Alloc>
class Allocator
{
	ThreadTestRes* testRes;
public:
	Allocator(ThreadTestRes* testRes_) { testRes = testRes_; }
	static constexpr bool isFake() { return false; }

	static constexpr const char* name() { return CURRENT_NAME; }

	void init() {}
	void* allocate(size_t sz) { return new uint8_t[sz]; }
	void* allocateSlots(size_t) { return nullptr; }
	void deallocate(void* ptr) { delete[] reinterpret_cast<uint8_t*>(ptr); }
	void deallocateSlots(void*) {}
	void deinit() { micro::allocator_trim(CURRENT_NAME); }

	// next calls are to get additional stats of the allocator, etc, if desired
	void doWhateverAfterSetupPhase() {}
	void doWhateverAfterMainLoopPhase() {}
	void doWhateverAfterCleanupPhase() {}

	ThreadTestRes* getTestRes() { return testRes; }
};


int alloc_test(int, char** const)
{
	CURRENT_NAME = "micro";
	runTest<Allocator<micro::Alloc>>();

	CURRENT_NAME = "malloc";
	runTest<Allocator<micro::Malloc>>();
#ifdef MICRO_BENCH_JEMALLOC
	CURRENT_NAME = "jemalloc";
	runTest<Allocator<micro::Jemalloc>>();
#endif
#ifdef MICRO_BENCH_MIMALLOC
	CURRENT_NAME = "mimalloc";
	runTest<Allocator<micro::MiMalloc>>();
#endif
#ifdef MICRO_BENCH_SNMALLOC
	CURRENT_NAME = "snmalloc";
	runTest<Allocator<micro::SnMalloc>>();
#endif
#ifdef USE_TBB
	CURRENT_NAME = "onetbb";
	runTest<Allocator<micro::TBBMalloc>>();
#endif
	return 0;
}