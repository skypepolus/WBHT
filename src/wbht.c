#if 0
BSD 3-Clause License

Copyright (c) 2024, Young Ho Song

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#endif
#include "thread.h"
#include "heap.h"
#include "page.h"
#include "wbht.h"
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include "static.h"

#ifndef __OPTIMIZE__
static unsigned sanity_check(void* ptr)
{
	unsigned error = 1;
	struct page* page = WBHT_PAGE(ptr);
	int64_t* front, * back;
	unsigned i;
	WBHT_PRINTF(stderr, "%p\n", front + 1);
	for(front = page->front + 1, back = front + (0 < *front ? *front : -1 * *front) - 1;
		*front != 0;
		front = back + 1, back = front + (0 < *front ? *front : -1 * *front) - 1)
	{
		WBHT_PRINTF(stderr, "|%ld %p %ld", *front, front + 1, *back);
		if(ptr == (void*)(front + 1))
		{
			error = 0;
			WBHT_PRINTF(stderr, "****<<<<****\n");
		}
		if(*front != *back)
		{
			WBHT_PRINTF(stderr, "****%ld != %ld****\n", *front, *back);
			return 1;
		}
		if(0 < *front && 0 < front[-1])
			return 1;
	}
	WBHT_PRINTF(stderr, "|\n");
	if(front < page->back)
		return 1;
	return error;
}
#endif
static inline void destructor_free(struct thread* thread)
{
	void** free;
	while((free = thread->free))
	{
		int64_t* front; 
		int64_t* back;
		thread->free = (void**)*free;
		front = ((int64_t*)free) - 1;
		back = front - *front - 1;
		local_free(thread, front, back);
	}
}

static void destructor(register void* data)
{
	if((data))
	{
	#define local ((struct thread**)data)
		struct thread* thread = *local;
		if((thread))
		{
			*local = NULL;
			if((thread->free))
				destructor_free(thread);
			for(thread->polling = 0; thread->polling < nprocs; thread->polling++)
			{
				void** free;
				if((free = (void**)thread->channel[thread->polling].free) 
				&& __sync_bool_compare_and_swap(&thread->channel[thread->polling].free, free, NULL))
				{
					__atomic_thread_fence(__ATOMIC_ACQUIRE);
					thread->free = free;
					destructor_free(thread);
				}
			}
			if(1 == thread->reference)
			{
				struct heap* root;
				if((root = thread->root) && 0 < *root->size)
				{
					int64_t* front = root->size;
					int64_t* back = front + *front - 1;
					if(0 == front[-1] && 0 == back[1])
					{
						thread->root = heap_remove(thread->root, root);
						WBHT_ASSERT(0 == munmap(WBHT_PAGE(front), WBHT_LENGTH));
						thread->reference--;
					}
				}
			}
			if(0 == thread->reference)
				WBHT_ASSERT(0 == munmap((void*)thread, thread_length));
			else
			{
				do
				{
					register unsigned i, j;
					for(i = 0, j = dial, j %= nprocs; i < nprocs; i++, j++, j %= nprocs)
					{
						thread->next = (struct thread*)channel[j].thread;
						__atomic_thread_fence(__ATOMIC_RELEASE);
						if(__sync_bool_compare_and_swap(&channel[j].thread, thread->next, thread))
						{
							dial++;
							return;
						}
					}
				} while(0 == sched_yield());
				dial++;
				WBHT_ASSERT(NULL == thread);
			}
		}
	#undef local
	}
}

#define HEAP_LIMIT (int64_t)(sizeof(struct heap) / sizeof(int64_t))

static void local_free(struct thread* thread, int64_t* front, int64_t* back)
{
	struct heap* dst;
	struct heap* src;
	WBHT_ASSERT(0 > *front && 0 > *back && *front == *back);
	if(HEAP_LIMIT < front[-1])
	{
		if(HEAP_LIMIT < back[1])
		{
			int64_t delta = back[1];
			thread->root = heap_remove(thread->root, (struct heap*)(back + 1));
			back += delta;
			*back += front[-1] + -1 * *front;
			front -= front[-1];
		}
		else
		if(0 < back[1])
		{
			back += back[1];
			*back += -1 * *front + front[-1];
			front -= front[-1];
		}
		else
		{
			*back *= -1;
			*back += front[-1]; 
			front -= front[-1];
		}
		heap_increase((struct heap*)front, *back);
		goto RETURN;
	}
	else
	if(0 < front[-1])
	{
		if(HEAP_LIMIT < back[1])
		{
			dst = (struct heap*)(front - front[-1]);
			src = (struct heap*)(back + 1);
			back += back[1];
			*back += -1 * *front + front[-1];
			thread->root = heap_move(thread->root, dst, src);
			heap_increase(dst, *back);
			front = dst->size;
			goto RETURN;
		}
		else
		if(0 < back[1])
		{
			back += back[1];
			*back += -1 * *front + front[-1];
			front -= front[-1];
		}
		else
		{
			*back *= -1;
			*back += front[-1]; 
			front -= front[-1];
		}
	}
	else
	if(HEAP_LIMIT < back[1])
	{
		dst = (struct heap*)front;
		src = (struct heap*)(back + 1);
		back += back[1];
		*back += -1 * *front;
		thread->root = heap_move(thread->root, dst, src);
		heap_increase(dst, *back);
		goto RETURN;
	}
	else
	if(0 < back[1])
	{
		back += back[1];
		*back += -1 * *front;
	}
	else
		*back *= -1;
	if(HEAP_LIMIT < *back)
		thread->root = heap_insert(thread->root, (struct heap*)front, *back);
	else
		*front = *back;
RETURN:
	if((sizeof(struct heap) + PAGE_SIZE * 2 >> 3) <= *front)
	{	
		if(0 == front[-1] && 0 == back[1])
		{
			struct heap* root = thread->root;
			if((root->edge[HEAP_LEFT]) || (root->edge[HEAP_RIGHT]))
			{
				thread->root = heap_remove(root, (struct heap*)front);
				WBHT_ASSERT(0 == munmap((void*)(front - 2), WBHT_LENGTH));
				thread->reference--;
			}
			return;
		}
		else
		{
			void* addr = (void*)((size_t)front + sizeof(struct heap) + PAGE_SIZE - 1 & PAGE_MASK);
			if((*(uint64_t*)addr))
			{
				size_t length = (*front - 2) * sizeof(void*) - ((size_t)addr - (size_t)&front[1]);
				WBHT_ASSERT(front < (int64_t*)addr);
				WBHT_ASSERT((size_t)addr + (length & PAGE_MASK) <= (size_t)(front + *front - 1));
				if(PAGE_SIZE <= length)
					WBHT_ASSERT(MAP_FAILED != mmap(addr, length & PAGE_MASK, PROT_READ|PROT_WRITE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)); 	
			}
		}
	}
#ifndef __OPTIMIZE__
	WBHT_ASSERT(NULL == thread->root || 0 < *thread->root->size);
	WBHT_ASSERT(NULL == thread->root || INT64_MIN == thread->root->size[HEAP_LEFT] || 0 < thread->root->size[HEAP_LEFT]);
	WBHT_ASSERT(NULL == thread->root || INT64_MIN == thread->root->size[HEAP_RIGHT] || 0 < thread->root->size[HEAP_RIGHT]);
	WBHT_ASSERT(0 < *front && 0 < *back && *front == *back);
	WBHT_ASSERT(0 == back[1] ? back + 1 == WBHT_PAGE(front + 1)->back : back + 1 < WBHT_PAGE(front + 1)->back);
	WBHT_ASSERT(0 == front[-1] ? front - 1 == WBHT_PAGE(front + 1)->front : front - 1 > WBHT_PAGE(front + 1)->front);
	if(sanity_check(front + 1)) { WBHT_PRINTF(stderr, "%s %s %d %ld %p %ld\n\n", __FUNCTION__, __FILE__, __LINE__, *front, front + 1, *back); pause(); } else WBHT_PRINTF(stderr, "%s\n", __FUNCTION__);
#endif
}

static inline void* local_shrink(struct thread* thread, int64_t* front, int64_t* back, int64_t size)
{
	int64_t delta = *front + size;
	WBHT_ASSERT(0 > *front && 0 > *back && *front == *back);
	WBHT_ASSERT(0 > delta);
	delta *= -1;
	if(HEAP_LIMIT < front[-1])
	{
		int64_t* adjacent = front - front[-1];
		*front += delta;
		front[-1] = *adjacent + delta;
		heap_increase((struct heap*)adjacent, front[-1]);
		*back += delta;
		*front = *back;
	}
	else
	if(0 < front[-1])
	{
		int64_t* adjacent = front - front[-1];
		front += delta;
		front[-1] = *adjacent + delta;
		if(HEAP_LIMIT < *adjacent)
			thread->root = heap_insert(thread->root, (struct heap*)adjacent, front[-1]);
		else
			*adjacent = front[-1];
		*back += delta;
		*front = *back;
	}
	else
	{
		int64_t* adjacent = front;
		front += delta;
		front[-1] = delta;
		if(HEAP_LIMIT < delta)
			thread->root = heap_insert(thread->root, (struct heap*)adjacent, delta);
		else
			*adjacent = delta;
		*back += delta;
		*front = *back;
	}
#ifndef __OPTIMIZE__
	if(sanity_check(front + 1)) { WBHT_PRINTF(stderr, "%s %s %d %ld %p %ld\n\n", __FUNCTION__, __FILE__, __LINE__, *front, front + 1, *back); pause(); } else WBHT_PRINTF(stderr, "%s\n", __FUNCTION__);
#endif
	return (void*)(front + 1);
}

static inline void* local_realloc(struct thread* thread, int64_t* front, int64_t* back, int64_t size)
{
	int64_t delta = *front + size;
	WBHT_ASSERT(0 > *front && 0 > *back && *front == *back);
	if(0 < delta)
	{
		if(delta < back[1])
		{
			int64_t* adjacent = back + back[1];
			*adjacent -= delta;
			if(HEAP_LIMIT < back[1])
			{
				if(HEAP_LIMIT < *adjacent)
				{
					struct heap* dst;
					struct heap* src;
					src = (struct heap*)(back + 1);
					back += delta;
					dst = (struct heap*)(back + 1);
					thread->root = heap_move(thread->root, dst, src);
					heap_decrease(dst, *adjacent);
				}
				else
				{
					thread->root = heap_remove(thread->root, (struct heap*)(back + 1));
					back += delta;
					back[1] = *adjacent;
				}
			}
			else
			{
				back += delta;
				back[1] = *adjacent;
			}
		}
		else
		if(delta == back[1])
		{
			if(HEAP_LIMIT < delta)
				thread->root = heap_remove(thread->root, (struct heap*)(back + 1));
			back += delta;
		}
		else
			return NULL;
		*front -= delta;
		*back = *front;
	}
	else
	if(0 > delta)
	{
		delta *= -1;
		if(HEAP_LIMIT < back[1])
		{
			int64_t* adjacent = back + back[1];
			struct heap* dst;
			struct heap* src;
			*adjacent += delta;
			src = (struct heap*)(back + 1);
			back -= delta;
			dst = (struct heap*)(back + 1);
			thread->root = heap_move(thread->root, dst, src);
			heap_increase(dst, *adjacent);
			*front += delta;
			*back = *front;
		}
		else
		if(0 < back[1])
		{
			int64_t* adjacent = back + back[1];
			*adjacent += delta;
			back -= delta;
			if(HEAP_LIMIT < *adjacent)
				thread->root = heap_insert(thread->root, (struct heap*)(back + 1), *adjacent);
			else
				back[1] = *adjacent;
			*front += delta;
			*back = *front;
		}
		else
		{
			int64_t* adjacent = back;
			back -= delta;
			*front += delta;
			*back = *front;
			*adjacent = delta;
			if(HEAP_LIMIT < delta)
				thread->root = heap_insert(thread->root, (struct heap*)(back + 1), delta);
			else
				back[1] = delta;
		}
	}
#ifndef __OPTIMIZE__
	if(sanity_check(front + 1)) { WBHT_PRINTF(stderr, "%s %s %d %ld %p %ld\n\n", __FUNCTION__, __FILE__, __LINE__, *front, front + 1, *back); pause(); } else WBHT_PRINTF(stderr, "%s\n", __FUNCTION__);
#endif
	return (void*)(front + 1);
}

static inline struct page* local_page(struct thread* thread, size_t align)
{
	uint64_t addr;
	uint64_t length;
	uint64_t page;

	if(align < WBHT_ALIGN)
		align = WBHT_ALIGN;

	length = WBHT_LENGTH + align;
	if(MAP_FAILED == (void*)(addr = (size_t)mmap(NULL, length, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)))
	{
		errno = ENOMEM;
		return NULL;
	}
	page = addr + PAGE_SIZE;
	page += align - 1;
	page &= UINT64_MAX - (align - 1);
	page -= PAGE_SIZE;
	if(addr < page)
	{
		uint64_t delta = page - addr;
		WBHT_ASSERT(0 == munmap((void*)addr, delta));
		length -= delta;
	}
	addr = page + WBHT_LENGTH;
	if(addr < page + length)
	{
		uint64_t delta = page + length - addr;
		WBHT_ASSERT(0 == munmap((void*)addr, delta));
		length -= delta;
	}
	WBHT_ASSERT(0 == mprotect((void*)page, length, PROT_READ|PROT_WRITE));
	((struct page*)page)->thread = thread;
	thread->reference++;
	return (struct page*)page;
}

static inline void* local_alloc(struct thread* thread, int64_t size)
{
	struct heap* heap = heap_first_fit(thread->root, size);
	int64_t* front;
	int64_t* back;
#ifndef __OPTIMIZE__
	WBHT_ASSERT(NULL == thread->root || 0 == sanity_check(thread->root->size + 1));
#endif
	if(NULL == heap)
	{
		struct page* page = local_page(thread, sizeof(void*)); 
		if(NULL == page)
		{
			errno = ENOMEM;
			return NULL;
		}
		heap = (struct heap*)(page->front + 1);
		back = page->back - 1;
		*back = sizeof(page->payload) / sizeof(int64_t);
		thread->root = heap_insert(thread->root, heap, *back);
	}
	else
		back = heap->size + *heap->size - 1;
	if(size == *heap->size)
	{
		thread->root = heap_remove(thread->root, heap);
		front = heap->size;
		*back *= -1;
		*front = *back;
	}
	else
	{
		int64_t delta = *heap->size - size;
		if(HEAP_LIMIT < delta)
			heap_decrease(heap, delta);
		else
		{
			thread->root = heap_remove(thread->root, heap);
			*heap->size = delta;
		}
		front = heap->size + delta;
		front[-1] = delta;
		*front = -1 * size;
		*back = *front;
	}
#ifndef __OPTIMIZE__
	WBHT_ASSERT(NULL == thread->root || 0 < *thread->root->size);
	WBHT_ASSERT(NULL == thread->root || INT64_MIN == thread->root->size[HEAP_LEFT] || 0 < thread->root->size[HEAP_LEFT]);
	WBHT_ASSERT(NULL == thread->root || INT64_MIN == thread->root->size[HEAP_RIGHT] || 0 < thread->root->size[HEAP_RIGHT]);
	WBHT_ASSERT(0 > *front && 0 > *back && *front == *back);
	WBHT_ASSERT(0 == back[1] ? back + 1 == WBHT_PAGE(front + 1)->back : back + 1 < WBHT_PAGE(front + 1)->back);
	WBHT_ASSERT(0 == front[-1] ? front - 1 == WBHT_PAGE(front + 1)->front : front - 1 > WBHT_PAGE(front + 1)->front);
	if(sanity_check(front + 1)) { WBHT_PRINTF(stderr, "%s %s %d %ld %p %ld\n\n", __FUNCTION__, __FILE__, __LINE__, *heap->size, heap->size + 1, *back); pause(); } else WBHT_PRINTF(stderr, "%s\n", __FUNCTION__);
#endif
	return (void*)(front + 1);
}

static void* wbht_map(size_t alignment, size_t size)
{
	uint64_t addr;
	uint64_t length;
	uint64_t align = WBHT_ALIGN < alignment ? alignment : WBHT_ALIGN;
	uint64_t page;
	int64_t* front; 

	length = PAGE_SIZE + size + align - 1;
	length += PAGE_SIZE - 1;
	length &= PAGE_MASK;
	WBHT_ASSERT(0 == length % PAGE_SIZE);
	if(MAP_FAILED == (void*)(addr = (size_t)mmap(NULL, length, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)))
	{
		errno = ENOMEM;
		return NULL;
	}
	page = addr + PAGE_SIZE;
	page += align - 1;
	page &= UINT64_MAX - (align - 1);
	page -= PAGE_SIZE;
	if(addr < page)
	{
		uint64_t delta = page - addr;
		WBHT_ASSERT(0 == delta % PAGE_SIZE);
		WBHT_ASSERT(0 == munmap((void*)addr, delta));
		length -= delta;
	}
	addr = (size_t)(((struct page*)page)->front + 1);
	addr += alignment - 1;
	addr &= ~(alignment - 1);
	front = (void*)addr;
	front--;

	addr += size;
	addr += PAGE_SIZE - 1;
	addr &= PAGE_MASK;
	
	if(addr < page + length)
	{
		uint64_t delta = page + length - addr;
		WBHT_ASSERT(0 == delta % PAGE_SIZE);
		WBHT_ASSERT(0 == munmap((void*)addr, delta));
		length -= delta;
	}
	if(mprotect((void*)page, length, PROT_READ|PROT_WRITE))
	{
		WBHT_ASSERT(0 == length % PAGE_SIZE);
		WBHT_ASSERT(0 == munmap((void*)page, length));
		errno = ENOMEM;
		return NULL;
	}
	*front = (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(int64_t);
	*front *= -1;
	*((struct page*)page)->front = (addr - (size_t)((struct page*)page)->front) / sizeof(int64_t);
	*((struct page*)page)->front *= -1;
	return front + 1;
}

WEAK void* wbht_malloc(size_t size)
{
	struct thread* thread = (local) ? local : thread_initial(&local);
	if(unlikely(thread->free))
	{
		void** free;
		if((free = thread->free))
		{
			int64_t* front; 
			int64_t* back;
			thread->free = (void**)*free;
			front = ((int64_t*)free) - 1;
			back = front - *front - 1;
			local_free(thread, front, back);
		}
		if((free = thread->free))
		{
			int64_t* front; 
			int64_t* back;
			thread->free = (void**)*free;
			front = ((int64_t*)free) - 1;
			back = front - *front - 1;
			if(0 < size && local_realloc(thread, front, back, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*)))
				return (void*)free;
			else
				local_free(thread, front, back);
		}
	}
	else
	if(unlikely(thread->channel[thread->polling].free))
	{
		void** free;
		if((free = (void**)thread->channel[thread->polling].free) 
		&& __sync_bool_compare_and_swap(&thread->channel[thread->polling].free, free, NULL))
		{
			__atomic_thread_fence(__ATOMIC_ACQUIRE);
			thread->free = free;
		}
	}
	else
	{
		thread->polling++;
		thread->polling %= nprocs;
	}
	if(WBHT_LIMIT < size)
		return wbht_map(sizeof(void*), size);
	else
	if(0 < size)
		return local_alloc(thread, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*));
	return NULL;
}

WEAK void* wbht_calloc(size_t nmemb, size_t size)
{
	if(0 < (size *= nmemb))
	{
		void* ptr;
		if((ptr = wbht_malloc(size)))
			return memset(ptr, 0, size);
	}
	return NULL;
}

static inline int64_t* boundary(int64_t* front, const char* file, int line)
{
	int64_t* back;
#ifdef __OPTIMIZE__
	if(-1 * ((WBHT_LENGTH / sizeof(void*)) - 3) > *front)
	{
		WBHT_PRINTF(stderr, "%p corruption %ld %s %d\n", front + 1, *front, file, line);
		return NULL;
	}
#endif
	back = front - *front - 1;
#ifdef __OPTIMIZE__
	if(0 <= *back || *front != *back)
	{
		WBHT_PRINTF(stderr, "%p corruption %ld %ld %s %d\n", front + 1, *front, *back, file, line);
		return NULL;
	}
#endif
	return back;
}

static void remote_free(struct thread* thread, void* ptr)
{
	do
	{
		register unsigned i, j;
		for(i = 0, j = polling, j %= nprocs; i < nprocs; i++, j++, j %= nprocs)
		{
			void** free = (void**)ptr;
			*free = thread->channel[j].free;
			__atomic_thread_fence(__ATOMIC_RELEASE);
			if(__sync_bool_compare_and_swap(&thread->channel[j].free, *free, free))
			{
				polling++;
				return;
			}
		}
	} while(0 == sched_yield());
	polling++;
}

WEAK void wbht_free(void* ptr)
{
	struct thread* thread;
	int64_t* front; 
	int64_t* back;
	void** free;
	if(NULL == ptr)
		return;
	else
	if((thread = local))
	{
		if(unlikely(thread->free))
		{
			struct page* page = WBHT_PAGE(ptr);
			struct thread* remote = page->thread;
			if((free = thread->free))
			{
				thread->free = (void**)*free;
				front = ((int64_t*)free) - 1;
				back = front - *front - 1;
				local_free(thread, front, back);
			}
			front = ((int64_t*)ptr) - 1;
			if(remote == thread)
			{
				if((back = boundary(front, __FILE__, __LINE__)))
					local_free(thread, front, back);
			}
			else
			if((remote))
			{
				if((back = boundary(front, __FILE__, __LINE__)))
					remote_free(remote, ptr);
				if((free = thread->free))
				{
					thread->free = (void**)*free;
					front = ((int64_t*)free) - 1;
					back = front - *front - 1;
					local_free(thread, front, back);
				}
			}
			else
			{
				size_t length = -1 * *page->front * sizeof(int64_t) + sizeof(struct thread*);
				WBHT_ASSERT(0 == length % PAGE_SIZE);
				WBHT_ASSERT(0 == munmap(page, length));
			}
		}
		else
		if(unlikely(thread->channel[thread->polling].free))
		{
			struct page* page = WBHT_PAGE(ptr);
			struct thread* remote = page->thread;
			if((free = (void**)thread->channel[thread->polling].free) 
			&& __sync_bool_compare_and_swap(&thread->channel[thread->polling].free, free, NULL))
			{
				__atomic_thread_fence(__ATOMIC_ACQUIRE);
				thread->free = free;
			}
			front = ((int64_t*)ptr) - 1;
			if(remote == thread)
			{
				if((back = boundary(front, __FILE__, __LINE__)))
					local_free(thread, front, back);
			}
			else
			if((remote))
			{
				if((back = boundary(front, __FILE__, __LINE__)))
					remote_free(remote, ptr);
			}
			else
			{
				size_t length = -1 * *page->front * sizeof(int64_t) + sizeof(struct thread*);
				WBHT_ASSERT(0 == length % PAGE_SIZE);
				WBHT_ASSERT(0 == munmap(page, length));
			}
		}
		else
		{
			struct page* page = WBHT_PAGE(ptr);
			struct thread* remote = page->thread;
			front = ((int64_t*)ptr) - 1;
			if(remote == thread)
			{
				if((back = boundary(front, __FILE__, __LINE__)))
					local_free(thread, front, back);
			}
			else
			if((remote))
			{
				register unsigned i, j;
				if((back = boundary(front, __FILE__, __LINE__)))
					remote_free(remote, ptr);
				i = scan;
				i %= nprocs;
				j = thread->polling;
				if((remote = channel[i].thread) && remote->channel[j].free)
				{
					__atomic_thread_fence(__ATOMIC_ACQUIRE);
					if(__sync_bool_compare_and_swap(&channel[i].thread, remote, remote->next))
					{
						destructor(&local);
						remote->polling = j;
						local = remote;
						scan++;
						return;
					}
				}
			}
			else
			{
				size_t length = -1 * *page->front * sizeof(int64_t) + sizeof(struct thread*);
				WBHT_ASSERT(0 == length % PAGE_SIZE);
				WBHT_ASSERT(0 == munmap(page, length));
			}
			thread->polling++;
			if(nprocs <= thread->polling)
			{
				thread->polling = 0;
				scan++;
			}
		}
	}
	else
	{
		struct page* page = WBHT_PAGE(ptr);
		struct thread* remote = page->thread;
		front = ((int64_t*)ptr) - 1;
		if((remote))
		{
			register unsigned i, j;
			if((back = boundary(front, __FILE__, __LINE__)))
				remote_free(remote, ptr);
			i = scan;
			i %= nprocs;
			j = polling;
			j %= nprocs;
			if((remote = channel[i].thread) && (remote->channel[j].free))
			{
				__atomic_thread_fence(__ATOMIC_ACQUIRE);
				if(__sync_bool_compare_and_swap(&channel[i].thread, remote, remote->next))
				{
					if(NULL == pthread_getspecific(key))
						pthread_setspecific(key, (const void*)&local);
					remote->polling = j;
					local = remote;
					scan++;
					return; 
				}
			}
			if(nprocs <= polling)
			{
				polling = 0;
				scan++;
			}
		}
		else
		{
			size_t length = -1 * *page->front * sizeof(int64_t) + sizeof(struct thread*);
			WBHT_ASSERT(0 == length % PAGE_SIZE);
			WBHT_ASSERT(0 == munmap(page, length));
		}
	}
}

WEAK void* wbht_realloc(void *ptr, size_t size)
{
	if(ptr)
	{
		if(0 < size)
		{
			struct thread* thread = (local) ? local : thread_initial(&local);
			struct page* page = WBHT_PAGE(ptr);
			void* dst;
			size_t n;
			int64_t* front; 
			int64_t* back;
			if(unlikely(thread->free))
			{
				void** free;
				if((free = thread->free))
				{
					thread->free = (void**)*free;
					front = ((int64_t*)free) - 1;
					back = front - *front - 1;
					local_free(thread, front, back);
				}
			}
			else
			if(unlikely(thread->channel[thread->polling].free))
			{
				void** free;
				if((free = (void**)thread->channel[thread->polling].free) 
				&& __sync_bool_compare_and_swap(&thread->channel[thread->polling].free, free, NULL))
				{
					__atomic_thread_fence(__ATOMIC_ACQUIRE);
					thread->free = free;
				}
			}
			else
			{
				thread->polling++;
				thread->polling %= nprocs;
			}			
			front = ((int64_t*)ptr) - 1;
			if(page->thread == thread)
			{
				back = front - *front - 1;
				if(WBHT_LIMIT < size)
				{
					if(dst = wbht_map(sizeof(void*), size))
					{
						n = (-1 * *front - 2) * sizeof(void*);
						if(n > size)
							n = size;
						memcpy(dst, ptr, n);
						local_free(thread, front, back);
						return dst;
					}
					else
						return NULL;
				}
				else
				{
					if(local_realloc(thread, front, back, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*)))
						return ptr;
					else
					{
						if(dst = local_alloc(thread, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*)))
						{
							n = (-1 * *front - 2) * sizeof(void*);
							if(n > size)
								n = size;
							memcpy(dst, ptr, n);
							local_free(thread, front, back);
							return dst;
						}
						else
							return NULL;
					}
				}
			}
			else
			if((page->thread))
			{
				if(dst = WBHT_LIMIT < size ? wbht_map(sizeof(void*), size) : local_alloc(thread, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*)))
				{
					n = (-1 * *front - 2) * sizeof(void*);
					if(n > size)
						n = size;
					memcpy(dst, ptr, n);
					remote_free(page->thread, ptr);
					return dst;
				}
				else
					return NULL;
			}
			else
			{
				if(dst = WBHT_LIMIT < size ? wbht_map(sizeof(void*), size) : local_alloc(thread, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*)))
				{
					n = (-1 * *front - 2) * sizeof(void*);
					if(n > size)
						n = size;
					memcpy(dst, ptr, n);
					n = -1 * *page->front * sizeof(int64_t) + sizeof(struct thread*);
					WBHT_ASSERT(0 == n % PAGE_SIZE);
					WBHT_ASSERT(0 == munmap(page, n));
					return dst;
				}
				else
					return NULL;
			}
		}
		else
			wbht_free(ptr);
	}
	else
	if(0 < size)
		return wbht_malloc(size);
	return NULL;
}

WEAK void* wbht_reallocf(void *ptr, size_t size)
{
	if(ptr)
	{
		if(0 < size)
		{
			struct thread* thread = (local) ? local : thread_initial(&local);
			struct page* page = WBHT_PAGE(ptr);
			void* dst;
			size_t n;
			int64_t* front; 
			int64_t* back;
			if(unlikely(thread->free))
			{
				void** free;
				if((free = thread->free))
				{
					thread->free = (void**)*free;
					front = ((int64_t*)free) - 1;
					back = front - *front - 1;
					local_free(thread, front, back);
				}
			}
			else
			if(unlikely(thread->channel[thread->polling].free))
			{
				void** free;
				if((free = (void**)thread->channel[thread->polling].free) 
				&& __sync_bool_compare_and_swap(&thread->channel[thread->polling].free, free, NULL))
				{
					__atomic_thread_fence(__ATOMIC_ACQUIRE);
					thread->free = free;
				}
			}
			else
			{
				thread->polling++;
				thread->polling %= nprocs;
			}			
			front = ((int64_t*)ptr) - 1;
			if(page->thread == thread)
			{
				back = front - *front - 1;
				if(WBHT_LIMIT < size)
				{
					if(dst = wbht_map(sizeof(void*), size))
					{
						n = (-1 * *front - 2) * sizeof(void*);
						if(n > size)
							n = size;
						memcpy(dst, ptr, n);
						local_free(thread, front, back);
						return dst;
					}
					else
					{
						local_free(thread, front, back);
						return NULL;
					}
				}
				else
				{
					if(local_realloc(thread, front, back, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*)))
						return ptr;
					else
					{
						if(dst = local_alloc(thread, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*)))
						{
							n = (-1 * *front - 2) * sizeof(void*);
							if(n > size)
								n = size;
							memcpy(dst, ptr, n);
							local_free(thread, front, back);
							return dst;
						}
						else
						{
							local_free(thread, front, back);
							return NULL;
						}
					}
				}
			}
			else
			if((page->thread))
			{
				if(dst = WBHT_LIMIT < size ? wbht_map(sizeof(void*), size) : local_alloc(thread, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*)))
				{
					n = (-1 * *front - 2) * sizeof(void*);
					if(n > size)
						n = size;
					memcpy(dst, ptr, n);
					remote_free(page->thread, ptr);
					return dst;
				}
				else
				{
					remote_free(page->thread, ptr);
					return NULL;
				}
			}
			else
			{
				if(dst = WBHT_LIMIT < size ? wbht_map(sizeof(void*), size) : local_alloc(thread, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*)))
				{
					n = (-1 * *front - 2) * sizeof(void*);
					if(n > size)
						n = size;
					memcpy(dst, ptr, n);
					n = -1 * *page->front * sizeof(int64_t) + sizeof(struct thread*);
					WBHT_ASSERT(0 == n % PAGE_SIZE);
					WBHT_ASSERT(0 == munmap(page, n));
					return dst;
				}
				else
				{
					size_t length = -1 * *page->front * sizeof(int64_t) + sizeof(struct thread*);
					WBHT_ASSERT(0 == length % PAGE_SIZE);
					WBHT_ASSERT(0 == munmap(page, length));
					return NULL;
				}
			}
		}
		else
			wbht_free(ptr);
	}
	else
	if(0 < size)
		return wbht_malloc(size);
	return NULL;

}

WEAK void *wbht_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	if(64 == sizeof(void*))
	{
		if(size >= (1UL << 48) / nmemb)
		{
			errno = EINVAL;
			return NULL;
		}
	}
	else
	if(32 == sizeof(void*))
	{
		if(size >= (1UL << 31) / nmemb)
		{
			errno = EINVAL;
			return NULL;
		}
	}
	return wbht_realloc(ptr, nmemb * size);
}

WEAK void* wbht_aligned_alloc(size_t alignment, size_t size)
{
	void* memptr;
	int ret;
	switch(ret == wbht_posix_memalign(&memptr, alignment, size))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return NULL;
}

WEAK int wbht_posix_memalign(void **memptr, size_t alignment, size_t size)
{
	int ret = 0;
	if(0 == size)
		*memptr = NULL;
	else
	if(sizeof(void*) > alignment)
		ret = EINVAL;
	else
	if((alignment & (alignment - 1)))
		ret = EINVAL;
	else
	{
		struct thread* thread =	 (local) ? local : thread_initial(&local);
		if(WBHT_LIMIT < size)
		{
			void* ptr;
			if((ptr = wbht_map(alignment, size)))
				*memptr = ptr;
			else
				ret = ENOMEM;
		}
		else
		if(WBHT_LIMIT < alignment + size)
		{
			struct page* page = local_page(thread, alignment); 
			if((page))
			{
				int64_t* front = page->front + 1;
				int64_t* back = page->back - 1;
				size_t addr = (size_t)(front + 1);
				void* ptr = (void*)(addr + alignment - 1 & ~(alignment - 1));
				*front = (-1) * (page->back - front);
				*back = *front;
				if((size_t)ptr < addr)
				{
					front = ((int64_t*)addr) - 1;
					back = front - *front - 1;
					ptr = local_shrink(thread, front, back, (sizeof(int64_t) + alignment + size - ((size_t)ptr - addr) + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*));
				}
				front = ((int64_t*)ptr) - 1;
				back = front - *front - 1;
				*memptr = local_realloc(thread, front, back, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*));
			}
			else
				ret = ENOMEM;
		}
		else
		{
			size_t addr;
			if((addr = (size_t)local_alloc(thread, (sizeof(int64_t) + alignment + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*))))	
			{	
				void* ptr = (void*)(addr + alignment - 1 & ~(alignment - 1));
				int64_t* front; 
				int64_t* back;
				if((size_t)ptr < addr)
				{
					front = ((int64_t*)addr) - 1;
					back = front - *front - 1;
					ptr = local_shrink(thread, front, back, (sizeof(int64_t) + alignment + size - ((size_t)ptr - addr) + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*));
				}
				front = ((int64_t*)ptr) - 1;
				back = front - *front - 1;
				*memptr = local_realloc(thread, front, back, (sizeof(int64_t) + size + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*));
			}
			else
				ret = ENOMEM;
		}
	}
	return ret;
}

WEAK void* wbht_valloc(size_t size)
{
	void* memptr;
	int ret;
	switch(ret == wbht_posix_memalign(&memptr, PAGE_SIZE, size))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return NULL;
}

WEAK void* wbht_memalign(size_t alignment, size_t size)
{
	void* memptr;
	int ret;
	switch(ret == wbht_posix_memalign(&memptr, alignment, size))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return NULL;
}

WEAK void* wbht_pvalloc(size_t alignment, size_t size)
{
	void* memptr;
	int ret;
	switch(ret == wbht_posix_memalign(&memptr, alignment, size + PAGE_SIZE - 1 & PAGE_MASK))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return NULL;
}

