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

#include "../micro.hpp"
#include "../os_page.hpp"

namespace micro
{
	namespace detail
	{
		
#ifdef MICRO_OVERRIDE
		static inline size_t sizeof_of_heap() noexcept
		{
			// Sizeof heap in page count
			return (sizeof(heap) / os_page_size()) + (sizeof(heap) % os_page_size() ? 1 : 0);
		}
	
		static inline void* get_buffer_for_default_heap() noexcept {

			// Allocate the heap object on the heap to avoid static variable uninitialization issues
			static void* buff = os_allocate_pages(sizeof_of_heap());
			return buff;
		}
#endif

		/* static void exit_process()
		{ 
			get_process_heap().perform_exit_operations();
			// Make sure to destroy the default heap that was allocated... on the heap
			if (void * p = get_buffer_for_default_heap()) {
				heap* h = static_cast<heap*>(p);
				// call the heap destructor as it might trigger things like file removal in case of file page provider
				h->~heap();

				// but we still need a heap as malloc calls can keep going, even if we are exiting the program
				memset(p, 0, sizeof(heap));
				// create a new heap with minimal parameters
				parameters params;
				params.max_arenas = 1;
				params.small_alloc_threshold = 0;
				h = new (p) heap(params,false);
				h->set_main();
				// we purposefully leak pages used for the default heap as they will be collected back
				// by the OS, and malloc calls might be triggered until the very end of the program.
			}
		}*/
		struct UninitHeap
		{
			heap* h;
			UninitHeap(heap* _h)
			  : h(_h)
			{
				//atexit(exit_process);
			}

			~UninitHeap() noexcept {
				h->perform_exit_operations();
				// call the heap destructor as it might trigger things like file removal in case of file page provider
				/* h->~heap();

				// but we still need a heap as malloc calls can keep going, even if we are exiting the program
				// create a new heap with minimal parameters
				parameters params;
				params.max_arenas = 1;
				params.small_alloc_threshold = 0;
				h = new (get_buffer_for_default_heap()) heap(params, false);
				MemoryManager::get_main_manager() = nullptr;*/
				//h->set_main();
				// we purposefully leak pages used for the default heap as they will be collected back
				// by the OS, and malloc calls might be triggered until the very end of the program.
			}
		};

		
		

		MICRO_HEADER_ONLY_EXPORT_FUNCTION heap* get_default_process_heap() noexcept
		{
			MICRO_PUSH_DISABLE_EXIT_TIME_DESTRUCTOR

#ifdef MICRO_OVERRIDE
			// Allocate the heap object on the heap to avoid static variable uninitialization issues
			heap* h = new (get_buffer_for_default_heap()) heap{ get_process_parameters(), false };
			static UninitHeap uninit{ h }; // Make sure perform_exit_operations is called before heap destruction
			
#else
			static heap _h{ get_process_parameters() };
			heap* h = &_h;
#endif
			MICRO_POP_DISABLE_EXIT_TIME_DESTRUCTOR
			return h;

		}

		MICRO_HEADER_ONLY_EXPORT_FUNCTION heap*& get_heap_pointer() noexcept
		{
			static heap* inst = get_default_process_heap();
			return inst;
		}
	}

	MICRO_HEADER_ONLY_EXPORT_FUNCTION heap& get_process_heap() noexcept { return *detail::get_heap_pointer(); }

	MICRO_HEADER_ONLY_EXPORT_FUNCTION void set_process_heap(heap& h) noexcept
	{
		detail::get_heap_pointer() = &h;
#ifdef MICRO_OVERRIDE
		h.set_main();
#endif
	}

	MICRO_HEADER_ONLY_EXPORT_FUNCTION bool get_process_infos(micro_process_infos& infos) noexcept
	{
		memset(&infos, 0, sizeof(infos));
		return os_process_infos(infos);
	}

}
