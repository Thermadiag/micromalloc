/**
 * MIT License
 *
 * Copyright (c) 2024 Victor Moncada <vtr.moncada@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define MICRO_NO_HEADER_ONLY

//#include "micro/micro.h"
//#include "micro/testing.hpp"
#include <cstdlib>
#include <cstdio>
#include <stdexcept>

#include "micro_proxy/micro_proxy.h"

int main(int argc, char** argv)
{
	//TEST
	printf("Start!\n"); fflush(stdout);
	int* v1 = new int[2000000];
	void * v2 = malloc(2000000);
	void* v3 = malloc(29);
	//void* v4 = micro_malloc(10);
	delete [] v1;
	free(v2);
	free(v3);
	//micro_free(v4);
	printf("%i\n", (int)MICRO_peak_bytes()); fflush(stdout);
	if(MICRO_peak_bytes() < 10000000)
		throw std::runtime_error("");

	//micro_process_infos infos;
	//micro_get_process_infos(&infos);
	printf("Override test: SUCCESS!\n"); fflush(stdout);
	return 0;
} 
