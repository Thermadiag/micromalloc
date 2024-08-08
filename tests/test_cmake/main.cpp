#include "micro/micro.h"
#include "micro/micro.hpp"

int main(int, char**)
{
	void* p = micro_malloc(1);
	micro_free(p);

	micro::heap h;
	p = h.allocate(1);
	h.deallocate(p);

	printf("SUCCESS!\n");
	return 0;
}