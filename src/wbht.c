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
#ifdef AVLHT
#include "avl.h"
#else
#include "heap.h"
#endif
#include "page.h"
#include "wbht.h"
#include "list.h"
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include "static.h"

#define LINK_LIMIT (int64_t)(1 + 2 + 1)
#define INDEX(size) ((((size) - 2) * sizeof(int64_t)) / sizeof(__int128) - 1)

#ifndef __OPTIMIZE__
void heap_print(struct heap* node, int tab)
{
	assert(64 >= tab);
	if((node))
	{
		int i;
		char tmp[1];
		size_t count;
		switch(node->balance)
		{
		case -1:
			assert(node->edge[HEAP_LEFT]);
			break;
		case 0:
			assert((node->edge[HEAP_LEFT] && node->edge[HEAP_RIGHT]) || (!node->edge[HEAP_LEFT] && !node->edge[HEAP_RIGHT]));
			break;
		case 1:
			assert(node->edge[HEAP_RIGHT]);
			break;
		default:
			assert(0);
			break;
		}
		heap_print(node->edge[HEAP_RIGHT], tab + 1);
		for(i = 0; i < tab; i++) 
			write(fileno(stderr), "  ", 2);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
		if(0 < (count = snprintf(tmp, sizeof(tmp), "%ld-%ld/%ld/%ld\n", node->size[HEAP_SIZE], node->balance, node->size[HEAP_LEFT], node->size[HEAP_RIGHT])))
		{
			char buf[count + 1];
			if(count == snprintf(buf, sizeof(buf), "%ld-%ld/%ld/%ld\n", node->size[HEAP_SIZE], node->balance, node->size[HEAP_LEFT], node->size[HEAP_RIGHT]))
				write(fileno(stderr), buf, count);
		}
#pragma GCC diagnostic pop
		heap_print(node->edge[HEAP_LEFT], tab + 1);
	}
}

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
static inline int64_t* boundary(register int64_t* front, const char* file, int line)
{
	register int64_t* back;
#ifdef __OPTIMIZE__
	register struct page* page = WBHT_PAGE(front);
	if(front <= page->front || 0 <= *front || -1 * (int64_t)WBHT_LIMIT > *front)
	{
		WBHT_PRINTF(stderr, "%p corruption %ld %s %d\n", front + 1, *front, file, line);
		return NULL;
	}
#endif
	back = front - *front - 1;
#ifdef __OPTIMIZE__
	if(page->back <= back || 0 <= *back || *front != *back)
	{
		WBHT_PRINTF(stderr, "%p corruption %ld %ld %s %d\n", front + 1, *front, *back, file, line);
		return NULL;
	}
#endif
	return back;
}

static void destructor(register void* data)
{
	if((data))
	{
		struct thread** local = (struct thread**)data;
		struct thread* thread = *local;
		if((thread))
		{
			struct page* page;
			int64_t* front; 
			int64_t* back;
			void** free;
			*local = NULL;
			for(;;)
			{
				if(unlikely(page = thread->local))
				{
					if((free = thread->free))
					{
						int64_t* front; 
						int64_t* back;
						void* ptr;
						thread->free = (void**)*free;
						front = ((int64_t*)free) - 1;
						if((back = boundary(front, __FILE__, __LINE__)))
							local_free(thread, front, back);
					}
					else
					if(free = page->free)
					{
						if(__sync_bool_compare_and_swap(&page->free, free, NULL))
						{
							__atomic_thread_fence(__ATOMIC_ACQUIRE);
							thread->free = free;
						}
						else
							break;
					}
					else
					{
						page->next = MAP_FAILED;
						thread->local = NULL;
					}
				}
				else
				if(unlikely(page = thread->remote))
				{
					__atomic_thread_fence(__ATOMIC_ACQUIRE);
					if(__sync_bool_compare_and_swap(&thread->remote, page, page->next))
						thread->local = page;
					else
						break;
				}
				else
					break;
			}
			if(0 == thread->reference)
			{
				if(0 < thread->length)
					WBHT_ASSERT(0 == munmap((void*)thread->addr, thread->length));
				WBHT_ASSERT(0 == munmap((void*)thread, thread_length));
			}
			else
			{
				do
				{
					int i;
					for(i = 0, thread->polling = (thread->polling + 1) % nprocs; 
						i < nprocs; 
						i++, thread->polling = (thread->polling + 1) % nprocs)
					{
						thread->next = (struct thread*)channel[thread->polling].thread;
						__atomic_thread_fence(__ATOMIC_RELEASE);
						if(__sync_bool_compare_and_swap(&channel[thread->polling].thread, thread->next, thread))
							return;
					}
				} while(0 == sched_yield());
				WBHT_ASSERT(NULL == thread);
			}
		}
	}
}

static inline void local_free(register struct thread* thread, register int64_t* front, register int64_t* back)
{
	register struct heap* dst;
	register struct heap* src;
	if(HEAP_LIMIT < front[-1])
	{
		if(HEAP_LIMIT < back[1])
		{
			register int64_t delta = back[1];
			thread->root = heap_remove(thread->root, (struct heap*)(back + 1));
			back += delta;
			*back += front[-1] + (-1) * *front;
			front -= front[-1];
		}
		else
		if(0 < back[1])
		{
			if(LINK_LIMIT <= back[1]) link_remove((struct link*)(back + 1 + 1));
			back += back[1];
			*back += front[-1] + (-1) * *front;
			front -= front[-1];
		}
		else
		{
			*back *= (-1);
			*back += front[-1]; 
			front -= front[-1];
		}
		heap_increase((struct heap*)front, *back);
		goto RETURN;
	}
	else
	if(0 < front[-1])
	{
		register int64_t* adjacent = front - front[-1];
		if(LINK_LIMIT <= *adjacent) link_remove((struct link*)(adjacent + 1));
		if(HEAP_LIMIT < back[1])
		{
			dst = (struct heap*)(front - front[-1]);
			src = (struct heap*)(back + 1);
			back += back[1];
			*back += (-1) * *front + front[-1];
			thread->root = heap_move(thread->root, dst, src);
			heap_increase(dst, *back);
			front = dst->size;
			goto RETURN;
		}
		else
		if(0 < back[1])
		{
			if(LINK_LIMIT <= back[1]) link_remove((struct link*)(back + 1 + 1));
			back += back[1];
			*back += (-1) * *front + front[-1];
			front -= front[-1];
		}
		else
		{
			*back *= (-1);
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
		*back += (-1) * *front;
		thread->root = heap_move(thread->root, dst, src);
		heap_increase(dst, *back);
		goto RETURN;
	}
	else
	if(0 < back[1])
	{
		if(LINK_LIMIT <= back[1]) link_remove((struct link*)(back + 1 + 1));
		back += back[1];
		*back += (-1) * *front;
	}
	else
		*back *= (-1);
	if(HEAP_LIMIT < *back)
		thread->root = heap_insert(thread->root, (struct heap*)front, *back);
	else
	{
		*front = *back;
		if(LINK_LIMIT <= *front) link_insert((struct link*)(front + 1), &thread->list[INDEX(*front)].tail);
	}
RETURN:
	if(0 == front[-1] && 0 == back[1])
	{
		register struct page* page = (struct page*)(PAGE_MASK & (size_t)front);
		thread->root = heap_remove(thread->root, (struct heap*)front);
		if(thread->local == page)
			thread->local = NULL;
		WBHT_ASSERT(0 == munmap((void*)page, WBHT_LENGTH));
		thread->reference--;
		return;
	}
#ifndef __OPTIMIZE__
	WBHT_ASSERT(NULL == thread->root || NULL == thread->root->edge[HEAP_PARENT]);
	WBHT_ASSERT(NULL == thread->root || 0 < *thread->root->size);
	WBHT_ASSERT(NULL == thread->root || INT64_MIN == thread->root->size[HEAP_LEFT] || 0 < thread->root->size[HEAP_LEFT]);
	WBHT_ASSERT(NULL == thread->root || INT64_MIN == thread->root->size[HEAP_RIGHT] || 0 < thread->root->size[HEAP_RIGHT]);
	WBHT_ASSERT(0 < *front && 0 < *back && *front == *back);
	WBHT_ASSERT(0 == back[1] ? back + 1 == WBHT_PAGE(front + 1)->back : back + 1 < WBHT_PAGE(front + 1)->back);
	WBHT_ASSERT(0 == front[-1] ? front - 1 == WBHT_PAGE(front + 1)->front : front - 1 > WBHT_PAGE(front + 1)->front);
	heap_print(thread->root, 0);
	if(sanity_check(front + 1)) { WBHT_PRINTF(stderr, "%s %s %d %ld %p %ld\n\n", __FUNCTION__, __FILE__, __LINE__, *front, front + 1, *back); pause(); } else WBHT_PRINTF(stderr, "%s\n", __FUNCTION__);
#endif
}

static inline void* local_shrink(register struct thread* thread, register int64_t* front, register int64_t* back, register int64_t size)
{
	register int64_t delta = *front + size;
	WBHT_ASSERT(0 > *front && 0 > *back && *front == *back);
	WBHT_ASSERT(0 > delta);
	delta *= (-1);
	if(HEAP_LIMIT < front[-1])
	{
		register int64_t* adjacent = front - front[-1];
		front += delta;
		front[-1] = *adjacent + delta;
		heap_increase((struct heap*)adjacent, front[-1]);
	}
	else
	if(0 < front[-1])
	{
		register int64_t* adjacent = front - front[-1];
		if(LINK_LIMIT <= *adjacent) link_remove((struct link*)(adjacent + 1));
		front += delta;
		front[-1] = *adjacent + delta;
		if(HEAP_LIMIT < front[-1])
			thread->root = heap_insert(thread->root, (struct heap*)adjacent, front[-1]);
		else
		{
			*adjacent = front[-1];
			if(LINK_LIMIT <= *adjacent) link_insert((struct link*)(adjacent + 1), &thread->list[INDEX(*adjacent)].tail);
		}
	}
	else
	{
		register int64_t* adjacent = front;
		front += delta;
		front[-1] = delta;
		if(HEAP_LIMIT < front[-1])
			thread->root = heap_insert(thread->root, (struct heap*)adjacent, delta);
		else
		{
			*adjacent = delta;
			if(LINK_LIMIT <= *adjacent) link_insert((struct link*)(adjacent + 1), &thread->list[INDEX(*adjacent)].tail);
		}
	}
	*back += delta;
	*front = *back;
#ifndef __OPTIMIZE__
	if(sanity_check(front + 1)) { WBHT_PRINTF(stderr, "%s %s %d %ld %p %ld\n\n", __FUNCTION__, __FILE__, __LINE__, *front, front + 1, *back); pause(); } else WBHT_PRINTF(stderr, "%s\n", __FUNCTION__);
#endif
	return (void*)(front + 1);
}

static inline void* local_realloc(register struct thread* thread, register int64_t* front, register int64_t* back, register int64_t size)
{
	register int64_t delta = *front + size;
	WBHT_ASSERT(0 > *front && 0 > *back && *front == *back);
	if(0 < delta)
	{
		if(delta < back[1])
		{
			register int64_t* adjacent = back + back[1];
			*adjacent -= delta;
			if(HEAP_LIMIT < back[1])
			{
				if(HEAP_LIMIT < *adjacent)
				{
					register struct heap* dst;
					register struct heap* src;
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
					if(LINK_LIMIT <= back[1]) link_insert((struct link*)(back + 1 + 1), &thread->list[INDEX(back[1])].tail);
				}
			}
			else
			{
				if(LINK_LIMIT <= back[1]) link_remove((struct link*)(back + 1 + 1));
				back += delta;
				back[1] = *adjacent;
				if(LINK_LIMIT <= back[1]) link_insert((struct link*)(back + 1 + 1), &thread->list[INDEX(back[1])].tail);
			}
		}
		else
		if(delta == back[1])
		{
			if(HEAP_LIMIT < delta)
				thread->root = heap_remove(thread->root, (struct heap*)(back + 1));
			else
				if(LINK_LIMIT <= back[1]) link_remove((struct link*)(back + 1 + 1));
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
		delta *= (-1);
		if(HEAP_LIMIT < back[1])
		{
			register int64_t* adjacent = back + back[1];
			register struct heap* dst;
			register struct heap* src;
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
			register int64_t* adjacent = back + back[1];
			if(LINK_LIMIT <= back[1]) link_remove((struct link*)(back + 1 + 1));
			*adjacent += delta;
			back -= delta;
			if(HEAP_LIMIT < *adjacent)
				thread->root = heap_insert(thread->root, (struct heap*)(back + 1), *adjacent);
			else
			{
				back[1] = *adjacent;
				if(LINK_LIMIT <= back[1]) link_insert((struct link*)(back + 1 + 1), &thread->list[INDEX(back[1])].tail);
			}
			*front += delta;
			*back = *front;
		}
		else
		{
			register int64_t* adjacent = back;
			back -= delta;
			*front += delta;
			*back = *front;
			*adjacent = delta;
			if(HEAP_LIMIT < delta)
				thread->root = heap_insert(thread->root, (struct heap*)(back + 1), delta);
			else
			{
				back[1] = delta;
				if(LINK_LIMIT <= back[1]) link_insert((struct link*)(back + 1 + 1), &thread->list[INDEX(back[1])].tail);
			}	
		}
	}
#ifndef __OPTIMIZE__
	heap_print(thread->root, 0);
	WBHT_ASSERT(NULL == thread->root || NULL == thread->root->edge[HEAP_PARENT]);
	if(sanity_check(front + 1)) { WBHT_PRINTF(stderr, "%s %s %d %ld %p %ld\n\n", __FUNCTION__, __FILE__, __LINE__, *front, front + 1, *back); pause(); } else WBHT_PRINTF(stderr, "%s\n", __FUNCTION__);
#endif
	return (void*)(front + 1);
}

static inline struct page* local_page(struct thread* thread, size_t align)
{
	for(;;)
	{
		struct page* page;
		size_t length;
		size_t addr = thread->addr + thread->length & ~(WBHT_ALIGN - 1);
		void* ptr;
		if(thread->addr + thread->length < addr - PAGE_SIZE + WBHT_LENGTH)
			addr -= WBHT_LENGTH;
		if(thread->addr <= addr - PAGE_SIZE && addr - PAGE_SIZE + WBHT_LENGTH <= thread->addr + thread->length)
		{
			page = (struct page*)(addr - PAGE_SIZE);
			addr = (size_t)page + WBHT_LENGTH;
			if(0 < (length = thread->addr + thread->length - addr))
			{
				WBHT_ASSERT(0 == munmap((void*)addr, length));
				thread->length -= length;
			}
			WBHT_ASSERT(0 == mprotect((void*)page, WBHT_LENGTH, PROT_READ|PROT_WRITE));
			thread->length -= WBHT_LENGTH;
			((struct page*)page)->thread = thread;
			((struct page*)page)->next = MAP_FAILED;
			thread->reference++;
			return (struct page*)page;
		}
		if(0 < thread->length)
			WBHT_ASSERT(0 == munmap((void*)thread->addr, thread->length));
		length = 1024 * 1024 * 1024;
		if(MAP_FAILED == (ptr = mmap(NULL, length, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)))
		{
			thread->length = 0;
			errno = ENOMEM;
			WBHT_PRINTF(stderr, "ENOMEM %lu %s %d\n", size, __FILE__, __LINE__);
			return NULL;
		}
		else
		{
			thread->addr = (size_t)ptr;
			thread->length = length;
		}
	}
	errno = ENOMEM;
	WBHT_PRINTF(stderr, "ENOMEM %lu %s %d\n", size, __FILE__, __LINE__);
	return NULL;
}

static inline void* local_alloc(register struct thread* thread, register int64_t size)
{
	register struct heap* heap;
	register int64_t* front;
	register int64_t* back;
#ifndef __OPTIMIZE__
	WBHT_ASSERT(NULL == thread->root || NULL == thread->root->edge[HEAP_PARENT]);
#endif
	if(unlikely(HEAP_LIMIT >= size))
	{
		register int index;
		register struct list* list;
		if(LINK_LIMIT <= size)
		{
			index = INDEX(size);
			list = thread->list + index;
			if(list->head.next != &list->tail)
			{
				register struct link* link = list->head.next;
				link_remove(link);
				front = ((int64_t*)link) - 1;
				back = front + *front - 1;
				*front *= (-1);
				*back = *front;
				return (void*)(front + 1);
			}
		}
		else
			index = -1;
		for(index++; index < sizeof(thread->list) / sizeof(*thread->list); index++)
		{
			list = thread->list + index;
			if(list->head.next != &list->tail)
			{
				register struct link* link = list->head.next;
				link_remove(link);
				front = ((int64_t*)link) - 1;
				back = front + *front - 1;
				*front -= size;
				if(LINK_LIMIT <= *front) link_insert((struct link*)(front + 1), &thread->list[INDEX(*front)].tail);
				front = back + 1 - size;
				front[-1] = *back - size;
				*front = (-1) * size;
				*back = *front;
				return (void*)(front + 1);
			}
		}
	}
	heap = heap_first_fit(thread->root, size);
	if(NULL == heap)
	{
		register struct page* page = local_page(thread, sizeof(void*)); 
		if(NULL == page)
		{
			errno = ENOMEM;
			WBHT_PRINTF(stderr, "%s %d %s\n", __FILE__, __LINE__, __FUNCTION__);
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
		*back *= (-1);
		*front = *back;
	}
	else
	{
		register int64_t delta = *heap->size - size;
		if(HEAP_LIMIT < delta)
			heap_decrease(heap, delta);
		else
		{
			thread->root = heap_remove(thread->root, heap);
			*heap->size = delta;
			if(LINK_LIMIT <= *heap->size) link_insert((struct link*)(heap->size + 1), &thread->list[INDEX(*heap->size)].tail);
		}
		front = heap->size + delta;
		front[-1] = delta;
		*front = (-1) * size;
		*back = *front;
	}
#ifndef __OPTIMIZE__
	heap_print(thread->root, 0);
	WBHT_ASSERT(NULL == thread->root || 0 < *thread->root->size);
	WBHT_ASSERT(NULL == thread->root || INT64_MIN == thread->root->size[HEAP_LEFT] || 0 < thread->root->size[HEAP_LEFT]);
	WBHT_ASSERT(NULL == thread->root || INT64_MIN == thread->root->size[HEAP_RIGHT] || 0 < thread->root->size[HEAP_RIGHT]);
	WBHT_ASSERT(0 > *front && 0 > *back && *front == *back);
	WBHT_ASSERT(0 == back[1] ? back + 1 == WBHT_PAGE(front + 1)->back : back + 1 < WBHT_PAGE(front + 1)->back);
	WBHT_ASSERT(0 == front[-1] ? front - 1 == WBHT_PAGE(front + 1)->front : front - 1 > WBHT_PAGE(front + 1)->front);
	if(sanity_check(front + 1)) { WBHT_PRINTF(stderr, "%s %s %d %ld %p %ld\n\n", __FUNCTION__, __FILE__, __LINE__, *heap->size, heap->size + 1, *back); pause(); } else WBHT_PRINTF(stderr, "%s\n", __FUNCTION__);
	memset((void*)(front + 1), 0, sizeof(int64_t) * ((-1 * *front) - 2));
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
		WBHT_PRINTF(stderr, "%s %d %s\n", __FILE__, __LINE__, __FUNCTION__);
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
		WBHT_PRINTF(stderr, "%s %d %s\n", __FILE__, __LINE__, __FUNCTION__);
		return NULL;
	}
	*front = (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(int64_t);
	*front *= (-1);
	*((struct page*)page)->front = (addr - (size_t)((struct page*)page)->front) / sizeof(int64_t);
	*((struct page*)page)->front *= (-1);
	return front + 1;
}

static inline void* local_coalesce(register struct thread* thread, register int64_t* front, register int64_t* back, register int64_t size)
{
	register size_t delta = (-1) * *front;
	register int64_t* adjacent;

	if(size < delta)
		return local_shrink(thread, front, back, size);
	if(0 < back[1])
		delta += back[1];
	if(size <= delta)
		return local_realloc(thread, front, back, size);
	if(0 < front[-1])
		delta += front[-1];
	if(delta < size)
	{
		local_free(thread, front, back);
		return NULL;
	}
	else
	if(delta > size)
	{
		delta -= size;
		adjacent = front - front[-1];
		if(HEAP_LIMIT < front[-1])
		{
			if(HEAP_LIMIT < delta)
				heap_decrease((struct heap*)adjacent, delta);
			else
			{
				thread->root = heap_remove(thread->root, (struct heap*)adjacent);
				*adjacent = delta;
				if(LINK_LIMIT <= *adjacent) link_insert((struct link*)(adjacent + 1), &thread->list[INDEX(*adjacent)].tail);
			}
		}
		else
		{
			if(LINK_LIMIT <= *adjacent) link_remove((struct link*)(adjacent + 1));
			*adjacent = delta;
			if(LINK_LIMIT <= *adjacent) link_insert((struct link*)(adjacent + 1), &thread->list[INDEX(*adjacent)].tail);
		}
		front = adjacent + delta;
		front[-1] = delta;
		if(HEAP_LIMIT < back[1])
		{
			adjacent = back + back[1];
			thread->root = heap_remove(thread->root, (struct heap*)(back + 1));
			back = adjacent;
		}
		else
		if(0 < back[1])
		{
			adjacent = back + back[1];
			if(LINK_LIMIT <= back[1]) link_remove((struct link*)(back + 1 + 1));
			back = adjacent;
		}
	}
	else
	{
		adjacent = front - front[-1];
		if(HEAP_LIMIT < *adjacent)
			thread->root = heap_remove(thread->root, (struct heap*)adjacent);
		else
			if(LINK_LIMIT <= *adjacent) link_remove((struct link*)(adjacent + 1));
		front = adjacent;
		if(HEAP_LIMIT < back[1])
		{
			adjacent = back + back[1];
			thread->root = heap_remove(thread->root, (struct heap*)(back + 1));
			back = adjacent;
		}
		else
		if(0 < back[1])
		{
			adjacent = back + back[1];
			if(LINK_LIMIT <= back[1]) link_remove((struct link*)(back + 1 + 1));
			back = adjacent;
		}
	}
	*front = (-1) * size;
	*back = *front;
	return (void*)(front + 1);
}

WEAK void* wbht_malloc(size_t size)
{
	size += sizeof(__int128) - 1;
	size &= ~(sizeof(__int128) - 1);

	if(WBHT_LIMIT < size)
		return wbht_map(sizeof(__int128), size);
	else
	{
		register struct thread* thread = local ? local : thread_initial(&local);
		register struct thread* remote;
		register struct page* page;

		if(unlikely(page = thread->local))
		{
			register void** free;
			if((free = thread->free))
			{
				register int64_t* front; 
				register int64_t* back;
				register void* ptr;
				thread->free = (void**)*free;
				front = ((int64_t*)free) - 1;
				if((back = boundary(front, __FILE__, __LINE__)))
				{
					if(ptr = local_coalesce(thread, front, back, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
					{
						if((free = thread->free))
						{	
							thread->free = (void**)*free;
							front = ((int64_t*)free) - 1;
							if((back = boundary(front, __FILE__, __LINE__)))
							{
								local_free(thread, front, back);
								if(0 == thread->reference)
									destructor(&local);
							}
						}
						return ptr;
					}
					else			
					if((free = thread->free))
					{
						thread->free = (void**)*free;
						front = ((int64_t*)free) - 1;
						if((back = boundary(front, __FILE__, __LINE__)))
						{
							if(ptr = local_coalesce(thread, front, back, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
								return ptr;
							else
							if(0 == thread->reference)
							{
								if((remote = channel[thread->polling].thread))
								{
									__atomic_thread_fence(__ATOMIC_ACQUIRE);
									if(__sync_bool_compare_and_swap(&channel[thread->polling].thread, remote, remote->next))
									{
										destructor(&local);
										local = thread = remote;
									}
								}
								else
									thread->polling = (thread->polling + 1) % nprocs;
							}
						}
					}
					else
					if(0 == thread->reference)
					{
						if((remote = channel[thread->polling].thread))
						{
							__atomic_thread_fence(__ATOMIC_ACQUIRE);
							if(__sync_bool_compare_and_swap(&channel[thread->polling].thread, remote, remote->next))
							{
								destructor(&local);
								local = thread = remote;
							}
						}
						else
							thread->polling = (thread->polling + 1) % nprocs;
					}
				}
				else
				if((free = thread->free))
				{
					thread->free = (void**)*free;
					front = ((int64_t*)free) - 1;
					if((back = boundary(front, __FILE__, __LINE__)))
					{
						if(ptr = local_coalesce(thread, front, back, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
							return ptr;
						else
						if(0 == thread->reference)
						{
							if((remote = channel[thread->polling].thread))
							{
								__atomic_thread_fence(__ATOMIC_ACQUIRE);
								if(__sync_bool_compare_and_swap(&channel[thread->polling].thread, remote, remote->next))
								{
									destructor(&local);
									local = thread = remote;
								}
							}
							else
								thread->polling = (thread->polling + 1) % nprocs;
						}
					}
				}
			}
			else
			if(free = page->free)
			{
				if(__sync_bool_compare_and_swap(&page->free, free, NULL))
				{
					__atomic_thread_fence(__ATOMIC_ACQUIRE);
					thread->free = free;
				}
			}
			else
			{
				page->next = MAP_FAILED;
				thread->local = NULL;
			}
		}
		else
		if(unlikely(page = thread->remote))
		{
			__atomic_thread_fence(__ATOMIC_ACQUIRE);
			if(__sync_bool_compare_and_swap(&thread->remote, page, page->next))
				thread->local = page;
		}
		else
		if((remote = channel[thread->polling].thread))
		{					
			if((remote->remote))
			{
				__atomic_thread_fence(__ATOMIC_ACQUIRE);
				if(__sync_bool_compare_and_swap(&channel[thread->polling].thread, remote, remote->next))
				{
					destructor(&local);
					local = thread = remote;
				}
			}
			else
				thread->polling = (thread->polling + 1) % nprocs;
		}
		else
			thread->polling = (thread->polling + 1) % nprocs;
		return local_alloc(thread, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*));
	}
	return NULL;
}

WEAK void* wbht_calloc(size_t nmemb, size_t size)
{
	size_t n = nmemb * size;
	if(0 < n)
	{
		if(n / nmemb == size)
		{
			void* ptr;
			if((ptr = wbht_malloc(n)))
				return memset(ptr, 0, n);
		}
		else
			errno = EINVAL;
	}
	return NULL;
}

static inline void remote_free(register struct thread* thread, register struct page* page, register void* ptr)
{
	do
	{
		register void** free = (void**)ptr;
		*free = page->free;
		__atomic_thread_fence(__ATOMIC_RELEASE);
		if(__sync_bool_compare_and_swap(&page->free, *free, free))
		{
			if(MAP_FAILED == page->next
			&& __sync_bool_compare_and_swap(&page->next, MAP_FAILED, NULL))
				do
				{
					page->next = thread->remote;
					__atomic_thread_fence(__ATOMIC_RELEASE);
					if(__sync_bool_compare_and_swap(&thread->remote, page->next, page))
						return;
				} while(0 == sched_yield());
			else
				return;
		}
	} while(0 == sched_yield());
}

WEAK void wbht_free(void* ptr)
{
	if((ptr))
	{
		register struct thread* thread;
		register struct page* page;
		register struct thread* remote;
		register int64_t* front; 
		register int64_t* back;
		register void** free;

		page = WBHT_PAGE(ptr);
		remote = page->thread;	
		front = ((int64_t*)ptr) - 1;

		if((thread = local))
		{
			if(remote == thread)
			{
				if((back = boundary(front, __FILE__, __LINE__)))
				{
					local_free(thread, front, back);
					if(0 == thread->reference)
						destructor(&local);
					else
					if(unlikely(page = thread->local))
					{
						if((free = thread->free))
						{
							thread->free = (void**)*free;
							front = ((int64_t*)free) - 1;
							back = front - *front - 1;
							local_free(thread, front, back);
							if(0 == thread->reference)
								destructor(&local);
						}
						else
						if(free = page->free)
						{
							if(__sync_bool_compare_and_swap(&page->free, free, NULL))
							{
								__atomic_thread_fence(__ATOMIC_ACQUIRE);
								thread->free = free;
							}
						}
						else
						{
							page->next = MAP_FAILED;
							thread->local = NULL;
						}
					}
					else
					if(unlikely(page = thread->remote))
					{
						__atomic_thread_fence(__ATOMIC_ACQUIRE);
						if(__sync_bool_compare_and_swap(&thread->remote, page, page->next))
							thread->local = page;
					}
				}
			}
			else
			if((remote))
			{
				if((back = boundary(front, __FILE__, __LINE__)))
					remote_free(remote, page, ptr);

				if(unlikely(page = thread->local))
				{
					if((free = thread->free))
					{
						thread->free = (void**)*free;
						front = ((int64_t*)free) - 1;
						back = front - *front - 1;
						local_free(thread, front, back);
						if(0 == thread->reference)
							destructor(&local);
					}
					else
					if(free = page->free)
					{
						if(__sync_bool_compare_and_swap(&page->free, free, NULL))
						{
							__atomic_thread_fence(__ATOMIC_ACQUIRE);
							thread->free = free;
						}
					}
					else
					{
						page->next = MAP_FAILED;
						thread->local = NULL;
					}
				}
				else
				if(unlikely(page = thread->remote))
				{
					__atomic_thread_fence(__ATOMIC_ACQUIRE);
					if(__sync_bool_compare_and_swap(&thread->remote, page, page->next))
						thread->local = page;
				}
				else
				if((remote = channel[thread->polling].thread))
				{
					if((remote->remote))
					{
						__atomic_thread_fence(__ATOMIC_ACQUIRE);
						if(__sync_bool_compare_and_swap(&channel[thread->polling].thread, remote, remote->next))
						{
							destructor(&local);
							local = thread = remote;
						}
					}
					else
						thread->polling = (thread->polling + 1) % nprocs;
				}
				else
					thread->polling = (thread->polling + 1) % nprocs;
			}
			else
			{
				WBHT_ASSERT(0 == ((size_t)&page->front[-1 * *page->front] - (size_t)page) % PAGE_SIZE);
				WBHT_ASSERT(0 == munmap(page, (size_t)&page->front[-1 * *page->front] - (size_t)page));
			}
		}
		else
		{
			if((remote))
			{
				register int index = polling;
				if((back = boundary(front, __FILE__, __LINE__)))
					remote_free(remote, page, ptr);

				if((remote = channel[index].thread))
				{
					__atomic_thread_fence(__ATOMIC_ACQUIRE);
					if(__sync_bool_compare_and_swap(&channel[index].thread, remote, remote->next))
					{
						if(NULL == pthread_getspecific(key))
							pthread_setspecific(key, (const void*)local);
						local = remote;
					}
				}
				else
					polling = (index + 1) % nprocs;
			}
			else
			{
				WBHT_ASSERT(0 == ((size_t)&page->front[-1 * *page->front] - (size_t)page) % PAGE_SIZE);
				WBHT_ASSERT(0 == munmap(page, (size_t)&page->front[-1 * *page->front] - (size_t)page));
			}
		}
	}
}

WEAK void* wbht_realloc(void *ptr, size_t size)
{
	if(ptr)
	{
		if(0 < size)
		{
			struct thread* thread = local ? local : thread_initial(&local);
			struct thread* remote;
			struct page* page;
			void* dst;
			size_t n;
			int64_t* front; 
			int64_t* back;
			void** free;

			if(unlikely(page = thread->local))
			{
				if((free = thread->free))
				{
					int64_t* front; 
					int64_t* back;
					void* ptr;
					thread->free = (void**)*free;
					front = ((int64_t*)free) - 1;
					if((back = boundary(front, __FILE__, __LINE__)))
					{
						local_free(thread, front, back);
						if(0 == thread->reference)
						{
							if((remote = channel[thread->polling].thread))
							{
								__atomic_thread_fence(__ATOMIC_ACQUIRE);
								if(__sync_bool_compare_and_swap(&channel[thread->polling].thread, remote, remote->next))
								{
									destructor(&local);
									local = thread = remote;
								}
							}
							else
								thread->polling = (thread->polling + 1) % nprocs;
						}
					}
				}
				else
				if(free = page->free)
				{
					if(__sync_bool_compare_and_swap(&page->free, free, NULL))
					{
						__atomic_thread_fence(__ATOMIC_ACQUIRE);
						thread->free = free;
					}
				}
				else
				{
					page->next = MAP_FAILED;
					thread->local = NULL;
				}
			}
			else
			if(unlikely(page = thread->remote))
			{
				__atomic_thread_fence(__ATOMIC_ACQUIRE);
				if(__sync_bool_compare_and_swap(&thread->remote, page, page->next))
					thread->local = page;
			}

			page = WBHT_PAGE(ptr);
			size += sizeof(__int128) - 1;
			size &= ~(sizeof(__int128) - 1);
			front = ((int64_t*)ptr) - 1;
			if(page->thread == thread)
			{
				if((back = boundary(front, __FILE__, __LINE__)))
				{
					if(WBHT_LIMIT < size)
					{
						if(dst = wbht_map(sizeof(__int128), size))
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
						if(local_realloc(thread, front, back, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
							return ptr;
						else
						{
							if(dst = local_alloc(thread, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
							{
								n = ((-1) * *front - 2) * sizeof(void*);
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
				{
					errno = EINVAL;
					return NULL;
				}
			}
			else
			if((page->thread))
			{
				if(dst = WBHT_LIMIT < size ? wbht_map(sizeof(__int128), size) : local_alloc(thread, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
				{
					n = (-1 * *front - 2) * sizeof(void*);
					if(n > size)
						n = size;
					memcpy(dst, ptr, n);
					if((back = boundary(front, __FILE__, __LINE__)))
						remote_free(page->thread, page, ptr);
					return dst;
				}
				else
					return NULL;
			}
			else
			{
				if(dst = WBHT_LIMIT < size ? wbht_map(sizeof(__int128), size) : local_alloc(thread, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
				{
					n = (-1 * *front - 2) * sizeof(void*);
					if(n > size)
						n = size;
					memcpy(dst, ptr, n);
					WBHT_ASSERT(0 == ((size_t)&page->front[-1 * *page->front] - (size_t)page) % PAGE_SIZE);
					WBHT_ASSERT(0 == munmap(page, (size_t)&page->front[-1 * *page->front] - (size_t)page));
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
		return wbht_malloc(size);
	return NULL;
}

WEAK void* wbht_reallocf(void *ptr, size_t size)
{
	if(ptr)
	{
		if(0 < size)
		{
			struct thread* thread = local ? local : thread_initial(&local);
			struct thread* remote;
			struct page* page;
			void* dst;
			size_t n;
			int64_t* front; 
			int64_t* back;
			void** free;

			if(unlikely(page = thread->local))
			{
				if((free = thread->free))
				{
					int64_t* front; 
					int64_t* back;
					void* ptr;
					thread->free = (void**)*free;
					front = ((int64_t*)free) - 1;
					if((back = boundary(front, __FILE__, __LINE__)))
					{
						local_free(thread, front, back);
						if(0 == thread->reference)
						{
							if((remote = channel[thread->polling].thread))
							{
								__atomic_thread_fence(__ATOMIC_ACQUIRE);
								if(__sync_bool_compare_and_swap(&channel[thread->polling].thread, remote, remote->next))
								{
									destructor(&local);
									local = thread = remote;
								}
							}
							else
								thread->polling = (thread->polling + 1) % nprocs;
						}
					}	
				}
				else
				if(free = page->free)
				{
					if(__sync_bool_compare_and_swap(&page->free, free, NULL))
					{
						__atomic_thread_fence(__ATOMIC_ACQUIRE);
						thread->free = free;
					}
				}
				else
				{
					page->next = MAP_FAILED;
					thread->local = NULL;
				}
			}
			else
			if(unlikely(page = thread->remote))
			{
				__atomic_thread_fence(__ATOMIC_ACQUIRE);
				if(__sync_bool_compare_and_swap(&thread->remote, page, page->next))
					thread->local = page;
			}

			page = WBHT_PAGE(ptr);
			size += sizeof(__int128) - 1;
			size &= ~(sizeof(__int128) - 1);
			front = ((int64_t*)ptr) - 1;
			if(page->thread == thread)
			{
				if((back = boundary(front, __FILE__, __LINE__)))
				{
					if(WBHT_LIMIT < size)
					{
						if(dst = wbht_map(sizeof(__int128), size))
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
						if(local_realloc(thread, front, back, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
							return ptr;
						else
						{
							if(dst = local_alloc(thread, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
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
				{
					errno = EINVAL;
					return NULL;
				}
			}
			else
			if((page->thread))
			{
				if(dst = WBHT_LIMIT < size ? wbht_map(sizeof(__int128), size) : local_alloc(thread, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
				{
					n = (-1 * *front - 2) * sizeof(void*);
					if(n > size)
						n = size;
					memcpy(dst, ptr, n);
					if((back = boundary(front, __FILE__, __LINE__)))
						remote_free(page->thread, page, ptr);
					return dst;
				}
				else
				{
					if((back = boundary(front, __FILE__, __LINE__)))
						remote_free(page->thread, page, ptr);
					return NULL;
				}
			}
			else
			{
				if(dst = WBHT_LIMIT < size ? wbht_map(sizeof(__int128), size) : local_alloc(thread, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*)))
				{
					n = (-1 * *front - 2) * sizeof(void*);
					if(n > size)
						n = size;
					memcpy(dst, ptr, n);
					WBHT_ASSERT(0 == ((size_t)&page->front[-1 * *page->front] - (size_t)page) % PAGE_SIZE);
					WBHT_ASSERT(0 == munmap(page, (size_t)&page->front[-1 * *page->front] - (size_t)page));
					return dst;
				}
				else
				{
					WBHT_ASSERT(0 == ((size_t)&page->front[-1 * *page->front] - (size_t)page) % PAGE_SIZE);
					WBHT_ASSERT(0 == munmap(page, (size_t)&page->front[-1 * *page->front] - (size_t)page));
					return NULL;
				}
			}
		}
		else
			wbht_free(ptr);
	}
	else
		return wbht_malloc(size);
	return NULL;
}

WEAK void *wbht_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	if(0 < nmemb)
	{
		if(8 == sizeof(void*))
		{
			if(size >= (1UL << 48) / nmemb)
			{
				errno = EINVAL;
				return NULL;
			}
		}
		else
		if(4 == sizeof(void*))
		{
			if(size >= (1UL << 31) / nmemb)
			{
				errno = EINVAL;
				return NULL;
			}
		}
	}
	return wbht_realloc(ptr, nmemb * size);
}

WEAK void* wbht_aligned_alloc(size_t alignment, size_t size)
{
	void* memptr = NULL;
	int ret;
	switch(ret = wbht_posix_memalign(&memptr, alignment, size))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return memptr;
}

WEAK int wbht_posix_memalign(void **memptr, size_t alignment, size_t size)
{
	int ret = 0;
	errno = 0;
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
		struct thread* thread = local ? local : thread_initial(&local);
		size += sizeof(__int128) - 1;
		size &= ~(sizeof(__int128) - 1);
		if(WBHT_LIMIT < size)
		{
			void* ptr;
			if((ptr = wbht_map(alignment, size)))
				*memptr = ptr;
			else
			{
				ret = ENOMEM;
				WBHT_PRINTF(stderr, "%s %d %s\n", __FILE__, __LINE__, __FUNCTION__);
			}
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
				if(addr < (size_t)ptr)
				{
					front = ((int64_t*)addr) - 1;
					back = front - *front - 1;
					ptr = local_shrink(thread, front, back, (sizeof(int64_t) + alignment + size - ((size_t)ptr - addr) + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*));
				}
				front = ((int64_t*)ptr) - 1;
				back = front - *front - 1;
				*memptr = local_realloc(thread, front, back, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*));
			}
			else
			{
				ret = ENOMEM;
				WBHT_PRINTF(stderr, "%s %d %s\n", __FILE__, __LINE__, __FUNCTION__);
			}
		}
		else
		{
			size_t addr;
			if((addr = (size_t)local_alloc(thread, (sizeof(int64_t) + alignment + size + sizeof(int64_t)) / sizeof(void*))))	
			{	
				void* ptr = (void*)(addr + alignment - 1 & ~(alignment - 1));
				int64_t* front; 
				int64_t* back;
				if(addr < (size_t)ptr)
				{
					front = ((int64_t*)addr) - 1;
					back = front - *front - 1;
					ptr = local_shrink(thread, front, back, (sizeof(int64_t) + alignment + size - ((size_t)ptr - addr) + sizeof(void*) - 1 + sizeof(int64_t)) / sizeof(void*));
				}
				front = ((int64_t*)ptr) - 1;
				back = boundary(front, __FILE__, __LINE__);
				*memptr = local_realloc(thread, front, back, (sizeof(int64_t) + size + sizeof(int64_t)) / sizeof(void*));
			}
			else
			{
				ret = ENOMEM;
				WBHT_PRINTF(stderr, "%s %d %s\n", __FILE__, __LINE__, __FUNCTION__);
			}
		}
	}
	return ret;
}

WEAK void* wbht_valloc(size_t size)
{
	void* memptr = NULL;
	int ret;
	switch(ret = wbht_posix_memalign(&memptr, PAGE_SIZE, size))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return memptr;
}

WEAK void* wbht_memalign(size_t alignment, size_t size)
{
	void* memptr = NULL;
	int ret;
	switch(ret = wbht_posix_memalign(&memptr, alignment, size))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return memptr;
}

WEAK void* wbht_pvalloc(size_t alignment, size_t size)
{
	void* memptr = NULL;
	int ret;
	switch(ret = wbht_posix_memalign(&memptr, alignment, size + PAGE_SIZE - 1 & PAGE_MASK))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return memptr;
}

