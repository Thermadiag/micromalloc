Examples
-----------
This section provides some examples on how to use the micro library and advanced utilities like custom heap objects or allocating from a file.

Simple allocation/deallocation in C/C++:
```c
#include <micro/micro.h>

int main(int, char**)
{
	void * p = micro_malloc(10);
	micro_free(p);
	return 0;
}

```
Usage of independant heaps in C:
```c
#include <micro/micro.h>
#include <stdlib.h>

int main(int, char**)
{
	// Create a heap object
	micro_heap * heap = micro_heap_create();

	// Modify its default parameters
	// Preallocate memory for this heap by using a MicroOSPreallocProvider page provider
	// and allocating 1GB ahead
	micro_heap_set_parameter(heap,MicroProviderType,MicroOSPreallocProvider);
	micro_heap_set_parameter(heap,MicroPageMemorySize, 1000000000ull);
	// Allow OS calls the allocate new pages when 1GB preallocated buffer is reach
	micro_heap_set_parameter(heap,MicroAllowOsPageAlloc, 1);
	
	for(unsigned i=0; i < 1000000; ++i) 
	{
		// Allocate 1M elements of average size ~= 1000 bytes.
		// We might use more than 1GB, that's ok, the heap will
		// allocate new pages if we reach the preallocated limit
		void * unused = micro_heap_malloc(heap, rand() % 2000);
	}

	// Now we could free allocated chunks one by one with micro_free,
	// but we can also free the full heap in on call:
	micro_heap_clear(heap);	

	// ... do whatever we want with the heap

	// destroy the heap (that will free all previously allocated memory)
	micro_heap_destroy(heap);
	
	return 0;
}

```

Usage of independant heaps in C++ (same as above):
```cpp
#include <micro/micro.hpp>
#include <cstdlib>

int main(int, char**)
{
	// Initialize heap parameters from env. variables
	micro::parameters p = micro::parameters::from_env();
	// Customize parameters
	p.provider_type = MicroOSPreallocProvider;
	p.page_memory_size = 1000000000ull;
	p.allow_os_page_alloc = true;


	// create heap object with custom parameters
	micro::heap h(p);

	for(unsigned i=0; i < 1000000; ++i) 
	{
		void * unused = h.allocate( rand() % 2000);
		// we could use deallocate with either h.deallocate()
		// or micro_free()
	}

	// Free previously allocated memory
	h.clear();

	// The heap destructor will also free previously allocated memory
	return 0;
}
```

Below example shows how to allocate memory from a file in C (C++ version is pretty similar):

```c
#include <micro/micro.h>
#include <stdlib.h>

int main(int, char**)
{
	// Create a heap object
	micro_heap * heap = micro_heap_create();

	// Modify heap parameters to allocate from a file
	micro_heap_set_parameter(heap,MicroProviderType,MicroFileProvider);
	// Set the file name. If not provided, a temporary file will be created.
	micro_heap_set_string_parameter(heap,MicroPageFileProvider, "my_file_name");
	// Set file flags: start with 1GB memory, allow file growing and remove it on heap destruction
	micro_heap_set_parameter(heap,MicroPageFileFlags,MicroGrowing);
	micro_heap_set_parameter(heap,MicroPageMemorySize,1000000000ull);
	

	// ... do whatever we want with the heap

	// destroy the heap (that will free all previously allocated memory) and remove the file
	micro_heap_destroy(heap);
	
	return 0;
}

```

Below example shows how to allocate memory from a buffer in C (C++ version is pretty similar).
In this example, we use the default heap instead of a local one (but the basic idea is the same):

```c
#include <micro/micro.h>
#include <stdlib.h>
#include <stdint.h>

extern char * buffer;
extern uint64_t buffer_size;

int main(int, char**)
{
	
	// Modify default heap parameters to allocate from a buffer
	micro_set_parameter(MicroProviderType,MicroMemoryProvider);
	// Set the buffer address and size
	micro_set_string_parameter(MicroPageMemoryProvider, buffer);
	micro_set_parameter(MicroPageMemorySize,buffer_size);
	

	// ... do whatever we want with the heap
	void * p = micro_malloc(10);
	micro_free(p);

	
	
	return 0;
}

```