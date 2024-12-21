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
#include "btree.h"
#include "thread.h"
#include "heap.h"
#include "btff.h"
#include "queue.h"
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "static.h"

static inline unsigned _thread_local(struct thread* thread)
{
	struct btree* btree = thread->local;
	void** free;
	if((free = btree_local))
	{
		int16_t disp;
		if(NULL == (btree_local = (void**)*free))
		{
			thread->local = NULL;
			btree_next = MAP_FAILED;
		}
		disp = (uint64_t*)free - btree->payload;
		if(btree_left == disp)
			return !last_free(btree);
		else
			btree_free(btree, disp);
	}
	else
	if((free = btree_remote))
	{
		if(__sync_bool_compare_and_swap(&btree_remote, free, NULL))
		{
			__atomic_thread_fence(__ATOMIC_ACQUIRE);
			btree_local = free;
		}
	}
	return 0;
}

static inline void _thread_remote(struct thread* thread)
{
	struct btree* btree = thread->remote;
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	if(__sync_bool_compare_and_swap(&thread->remote, btree, btree_next))
	{
		thread->local = btree;
		if(thread->btree != thread->local)
		{
			if(thread->btree)
			{
				btree = thread->btree;
				thread->root = heap_insert(thread->root, &btree->heap, btree_key);
			}
			btree = thread->local;
			thread->root = heap_remove(thread->root, &btree->heap);
			thread->btree = btree;
		}
	}
}

static void destructor(register void* data)
{
	if((data))
	{
		struct thread* thread = *(struct thread**)data;;
		*(struct thread**)data = NULL;
		if((thread))
		{
			if((thread->local))
				for(;;)
				{
					_thread_local(thread);
					if((thread->local))
					{
						struct btree* btree = thread->local;
						if((btree_remote))
							goto BREAK;
					}
					else
						break;
				}
			if((thread->remote))
				do
				{
					_thread_remote(thread);
					if((thread->local))
						for(;;)
						{
							_thread_local(thread);
							if((thread->local))
							{
								struct btree* btree = thread->local;
								if((btree_remote))
									goto BREAK;
							}
							else
								break;
						}
					else
						break;
				} while(thread->remote);
		BREAK:
			if((thread->btree) || (thread->root))
			{
				do
				{
					register unsigned i, j;
					for(i = 0, j = thread->polling, j %= nprocs; i < nprocs; i++, j++, j %= nprocs)
					{
						thread->next = (struct thread*)channel[j].thread;
						__atomic_thread_fence(__ATOMIC_RELEASE);
						if(__sync_bool_compare_and_swap(&channel[j].thread, thread->next, thread))
							return;
					}
				} while(0 == sched_yield());
				BTFF_ASSERT(NULL == thread);
			}
			else
			{
				BTFF_ASSERT(NULL == thread->local);
				BTFF_ASSERT(NULL == thread->remote);
				if(0 < thread->length)
					BTFF_ASSERT(0 == munmap((void*)thread->addr, thread->length));
				BTFF_ASSERT(0 == munmap((void*)thread, thread_length));
			}
		}
	}
}

static inline void _remote_free(struct thread* thread, struct btree* btree, void* ptr)
{
	do
	{
		void** free = (void**)ptr;
		*free = btree_remote;
		__atomic_thread_fence(__ATOMIC_RELEASE);
		if(__sync_bool_compare_and_swap(&btree_remote, *free, free))
		{
			if(MAP_FAILED == btree_next
			&& __sync_bool_compare_and_swap(&btree_next, MAP_FAILED, NULL))
				do
				{
					btree_next = thread->remote;
					__atomic_thread_fence(__ATOMIC_RELEASE);
					if(__sync_bool_compare_and_swap(&thread->remote, btree_next, btree))
						return;
				} while(0 == sched_yield());
			else
				return;
		}
	} while(0 == sched_yield());
}

static inline void* btff_mmap(struct thread* thread, size_t alignment, size_t size)
{
	size_t btree_alignment = alignment;

	if(BTREE_ALIGNMENT > btree_alignment)
		btree_alignment = BTREE_ALIGNMENT;

	for(;;)
	{
		struct btree* btree;
		size_t length;
		size_t addr = thread->addr + PAGE_SIZE + btree_alignment - 1 & ~(btree_alignment - 1);
		void* ptr;

		if(PAGE_SIZE <= addr - thread->addr && addr <= thread->addr + thread->length)
		{
			btree = (struct btree*)(addr - PAGE_SIZE);
			addr = ((size_t)&btree->header[HEADER_LEFT]) + alignment - 1 & ~(alignment - 1);
			ptr = (void*)addr;
			addr = addr + size + PAGE_SIZE - 1 & ~(PAGE_SIZE - 1);
			if(addr <= thread->addr + thread->length)
			{
				if(thread->addr < (size_t)btree)
				{
					length = (size_t)btree - thread->addr;
					BTFF_ASSERT(0 == munmap((void*)thread->addr, length));
				}
				length = addr - (size_t)btree;
				BTFF_ASSERT(0 == mprotect((void*)btree, length, PROT_READ|PROT_WRITE));
				btree_length = length;
				length = addr - thread->addr;
				thread->addr += length;
				thread->length -= length;
			#ifndef __OPTIMIZE__
				return memset(ptr, 0, size);
			#else
				return ptr;
			#endif
			}
		}
		if(0 < thread->length)
			BTFF_ASSERT(0 == munmap((void*)thread->addr, thread->length));
		if(PAGE_SIZE * 1024 > (length = PAGE_SIZE + (size + PAGE_SIZE - 1 & PAGE_MASK) + btree_alignment))
			length = PAGE_SIZE * 1024;
		if(MAP_FAILED == (ptr = mmap(NULL, length, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)))
		{
			thread->length = 0;
			errno = ENOMEM;
			BTFF_PRINTF(stderr, "ENOMEM %lu %s %d\n", size, __FILE__, __LINE__);
			return NULL;
		}
		else
		{
			thread->addr = (size_t)ptr;
			thread->length = length;
		}
	}
	errno = ENOMEM;
	BTFF_PRINTF(stderr, "ENOMEM %lu %s %d\n", size, __FILE__, __LINE__);
	return NULL;
}

static inline void* btff_alloc(struct thread* thread, int64_t size, int16_t delta)
{
	struct btree* btree;
	if(unlikely(NULL == (btree = thread->btree)))
	{
		if(NULL == thread->root)
			return btree_page(thread, sizeof(void*), size);
		else
		{
			struct heap* heap;
			if(NULL == (heap = heap_first_fit(thread->root, size)))
				return btree_page(thread, sizeof(void*), size);
			else
			{
				thread->btree = btree = BTREE(heap);
				thread->root = heap_remove(thread->root, heap);
			}
		}
	}
	else
	if(unlikely(btree_key < size))
	{
		if(NULL == thread->root)
		{
			thread->root = heap_insert(thread->root, &btree->heap, btree_key);
			thread->btree = NULL;
			return btree_page(thread, sizeof(void*), size);
		}
		else
		{
			struct heap* heap;
			if(NULL == (heap = heap_first_fit(thread->root, size)))
			{
				thread->root = heap_insert(thread->root, &btree->heap, btree_key);
				thread->btree = NULL;
				return btree_page(thread, sizeof(void*), size);
			}
			else
			{
				thread->root = heap_insert(thread->root, &btree->heap, btree_key);
				thread->btree = btree = BTREE(heap);
				thread->root = heap_remove(thread->root, heap);
			}
		}
	}
	return first_fit_alloc(btree, delta);
}

WEAK void* btff_malloc(size_t size)
{
	struct thread* thread = (local) ? local : thread_initial(&local);
	int16_t delta = (size + sizeof(void*) - 1) >> 3;
	uint64_t* queue;

	if(unlikely(thread->local))
	{
		_thread_local(thread);
		if((thread->local))
		{
			if(BTREE_LIMIT < size)
				_thread_local(thread);
			else
			{	
				struct btree* btree = thread->local;
				void** free;
				if((free = btree_local))
				{
					int16_t disp;
					if(NULL == (btree_local = (void**)*free))
					{
						thread->local = NULL;
						btree_next = MAP_FAILED;
					}
					disp = (uint64_t*)free - btree->payload;
					if(0 == btree_realloc(btree, disp, delta))
					#ifndef __OPTIMIZE__
						return memset(free, 0, size);
					#else
						return (void*)free;
					#endif
					if(btree_left == disp)
						last_free(btree);
					else
						btree_free(btree, disp);
				}
				else
				if((free = btree_remote))
				{
					if(__sync_bool_compare_and_swap(&btree_remote, free, NULL))
					{
						__atomic_thread_fence(__ATOMIC_ACQUIRE);
						btree_local = free;
					}
				}
			} 
		}
	}
	else
	if(unlikely(thread->remote))
		_thread_remote(thread);

	if(BTREE_LIMIT < size)
		return btff_mmap(thread, sizeof(void*), size);
	else
	if(0 < size)
		return btff_alloc(thread, size, delta);
	return NULL;
}

WEAK void* btff_calloc(size_t nmemb, size_t size)
{
	if(0 < (size *= nmemb))
	{
		void* ptr;
		if((ptr = btff_malloc(size)))
			return memset(ptr, 0, size);
	}
	return NULL;
}

WEAK void btff_free(void* ptr)
{
	struct thread* thread;
	struct btree* btree;
	int16_t disp;
	void** free;
	if(unlikely(NULL == ptr))
		return;
	else
	if(unlikely((uint64_t)(sizeof(void*) - 1) & (uint64_t)ptr))
	{
		BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
		return;
	}
	else
	if((thread = local))
	{
		unsigned destruct = 0;
		if(unlikely(thread->local))
		{
			destruct = _thread_local(thread);
			if((ptr))
			{
				struct thread* remote;
				btree = BTREE(ptr);
				remote = btree->thread;
				if(remote == thread)
				{
				#ifndef __OPTIMIZE__
					if((size_t)ptr < (size_t)&btree->payload[btree_left]) 
					{
						BTFF_PRINTF(stderr, "%p %p %p %lu %p %hu %lu corruption %s %d\n", ptr, btree, &btree_left, (size_t)&btree->payload[btree_left] - (size_t)btree->payload, &btree->payload[btree_left], btree_left, (size_t)&btree->payload[btree_left] - (size_t)ptr, __FILE__, __LINE__);
						return;
					}
				#endif
					if(btree_left == (disp = (uint64_t*)ptr - btree->payload))
						destruct = !last_free(btree);
					else
						btree_free(btree, disp);
				}
				else
				if((remote))
				{
					if((size_t)ptr < (size_t)&btree->payload[btree_left]) 
					{
						BTFF_PRINTF(stderr, "%p %p %p %lu %p %hu %lu corruption %s %d\n", ptr, btree, &btree_left, (size_t)&btree->payload[btree_left] - (size_t)btree->payload, &btree->payload[btree_left], btree_left, (size_t)&btree->payload[btree_left] - (size_t)ptr, __FILE__, __LINE__);
						return;
					}
					if(unlikely(thread->local))
						destruct = _thread_local(thread);
					_remote_free(remote, btree, ptr);
				}
				else
				{
					size_t length = btree_length;
					BTFF_ASSERT(0 == length % PAGE_SIZE);
					BTFF_ASSERT(0 == munmap(btree, length));
				}
			}
		}
		else
		if(unlikely(thread->remote))
		{
			_thread_remote(thread);
			if((ptr))
			{
				struct thread* remote;
				btree = BTREE(ptr);
				remote = btree->thread;
				if(remote == thread)
				{
					if((uint64_t*)ptr < &btree->payload[btree_left]) 
					{
						BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
						return;
					}
					if(btree_left == (disp = (uint64_t*)ptr - btree->payload))
						destruct = !last_free(btree);
					else
						btree_free(btree, disp);
				}
				else
				if((remote))
				{
					if(likely(thread->local))
						destruct = _thread_local(thread);
					if((size_t)ptr < (size_t)&btree->payload[btree_left]) 
						BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
					else
						_remote_free(remote, btree, ptr);
				}
				else
				{
					size_t length = btree_length;
					BTFF_ASSERT(0 == length % PAGE_SIZE);
					BTFF_ASSERT(0 == munmap(btree, length));
				}
			}
		}
		else
		if((ptr))
		{
			struct thread* remote;
			btree = BTREE(ptr);
			remote = btree->thread;
			if(remote == thread)
			{
				if((uint64_t*)ptr < &btree->payload[btree_left]) 
				{
					BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
					return;
				}
				if(btree_left == (disp = (uint64_t*)ptr - btree->payload))
					destruct = !last_free(btree);
				else
					btree_free(btree, disp);
			}
			else
			if((remote))
			{
				register unsigned i, j;
				if((uint64_t*)ptr < &btree->payload[btree_left]) 
				{
					BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
					return;
				}
				_remote_free(remote, btree, ptr);
				i = thread->polling % nprocs;
				if((remote = channel[i].thread) && ((remote->local) || (remote->remote)))
				{
					__atomic_thread_fence(__ATOMIC_ACQUIRE);
					if(__sync_bool_compare_and_swap(&channel[i].thread, remote, remote->next))
					{
						destructor(&local);
						local = remote;
						return;
					}
				}
				else
					thread->polling++;
			}
			else
			{
				size_t length = btree_length;
				BTFF_ASSERT(0 == length % PAGE_SIZE);
				BTFF_ASSERT(0 == munmap(btree, length));
			}
		}
		if(unlikely(destruct))
			destructor(&local);
	}
	else
	{
		struct thread* remote;
		btree = BTREE(ptr);
		remote = btree->thread;
		if((remote))
		{
			register unsigned i;
			if((uint64_t*)ptr < &btree->payload[btree_left]) 
			{
				BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
				return;
			}
			_remote_free(remote, btree, ptr);
			i = polling % nprocs;
			if((remote = channel[i].thread) && ((remote->local) || (remote->remote)))
			{
				__atomic_thread_fence(__ATOMIC_ACQUIRE);
				if(__sync_bool_compare_and_swap(&channel[i].thread, remote, remote->next))
				{
					if(NULL == pthread_getspecific(key))
						pthread_setspecific(key, (const void*)&local);
					local = remote;
					return; 
				}
			}
			else
				polling++;
		}
		else
		{
			size_t length = btree_length;
			BTFF_ASSERT(0 == length % PAGE_SIZE);
			BTFF_ASSERT(0 == munmap(btree, length));
		}
	}
}

WEAK void* btff_realloc(void *ptr, size_t size)
{
	if((ptr))
	{
		if((uint64_t)(sizeof(void*) - 1) & (uint64_t)ptr)
		{
			BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
			errno = ENOMEM;
			return NULL;
		}
		else
		if(0 < size)
		{
			struct thread* thread = (local) ? local : thread_initial(&local);
			struct btree* btree;
			int16_t disp;
			void* dst;
			size_t n;
			int16_t delta;
			uint64_t* queue;

			if(unlikely(thread->local))
				_thread_local(thread);
			else
			if(unlikely(thread->remote))
				_thread_remote(thread);

			btree = BTREE(ptr);
			if(btree->thread == thread)
			{
				if((uint64_t*)ptr < &btree->payload[btree_left]) 
				{
					BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
					return NULL;
				}
				if(BTREE_LIMIT < size)
				{
					if(dst = btff_mmap(thread, sizeof(void*), size))
					{
						if(size < (n = (uint64_t)btree->barrier - (uint64_t)ptr))
							n = size;
						memcpy(dst, ptr, n);
						if(btree_left == (disp = (uint64_t*)ptr - btree->payload))
							last_free(btree);
						else
							btree_free(btree, disp);
						return dst;
					}
					else
						return NULL;
				}
				else
				{
					disp = (uint64_t*)ptr - btree->payload;
					delta = (size + sizeof(void*) - 1) >> 3;
					BTFF_ASSERT(size <= ((size_t)delta) << 3);
					if(0 == (n = btree_realloc(btree, disp, delta)))
					#ifndef __OPTIMIZE__
						return memset(ptr, 0, size);
					#else
						return ptr;
					#endif
					else
					{
						if((dst = btff_alloc(thread, size, delta)))
						{
							if(size < n)
								n = size;
							memcpy(dst, ptr, n);
							if(btree_left == (disp = (uint64_t*)ptr - btree->payload))
								last_free(btree);
							else
								btree_free(btree, disp);
							return dst;
						}
						else
							return NULL;
					}
				}
			}
			else
			if((btree->thread))
			{
				if(size < (n = (uint64_t)btree->barrier - (uint64_t)ptr))
					n = size;
				if(BTREE_LIMIT < size)
				{
					if((dst = btff_mmap(thread, sizeof(void*), size)))
					{
						memcpy(dst, ptr, n);
						if((size_t)ptr < (size_t)&btree->payload[btree_left]) 
							BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
						else
							_remote_free(btree->thread, btree, ptr);
						return dst;
					}
				}
				else
				{
					delta = (size + sizeof(void*) - 1) >> 3;
					if((dst = btff_alloc(thread, size, delta)))
					{
						memcpy(dst, ptr, size);
						if((size_t)ptr < (size_t)&btree->payload[btree_left]) 
							BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
						else
							_remote_free(btree->thread, btree, ptr);
						return dst;
					}
				}
			}
			else
			{
				if(size < (n = (uint64_t)btree + (uint64_t)btree_length - (uint64_t)ptr))
					n = size;
				if(BTREE_LIMIT < size)
				{
					if((dst = btff_mmap(thread, sizeof(void*), size)))
					{
						memcpy(dst, ptr, n);
						BTFF_ASSERT(0 == munmap(btree, btree_length));
						return dst;
					}
				}
				else
				{
					delta = (size + sizeof(void*) - 1) >> 3;
					if((dst = btff_alloc(thread, size, delta)))
					{	
						memcpy(dst, ptr, n);
						BTFF_ASSERT(0 == munmap(btree, btree_length));
						return dst;
					}
				}
			}
		}
		else	
			btff_free(ptr);
	}
	else
	if(0 < size)
		return btff_malloc(size);
	return NULL;
}

WEAK void* btff_reallocf(void *ptr, size_t size)
{
	if((ptr))
	{
		if((uint64_t)(sizeof(void*) - 1) & (uint64_t)ptr)
		{
			BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
			errno = ENOMEM;
			return NULL;
		}
		else
		if(0 < size)
		{
			struct thread* thread = (local) ? local : thread_initial(&local);
			struct btree* btree;
			int16_t disp;
			void* dst;
			size_t n;
			int16_t delta;
			uint64_t* queue;

			if(unlikely(thread->local))
				_thread_local(thread);
			else
			if(unlikely(thread->remote))
				_thread_remote(thread);

			btree = BTREE(ptr);
			if(btree->thread == thread)
			{
				if((uint64_t*)ptr < &btree->payload[btree_left]) 
				{
					BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
					return NULL;
				}
				if(BTREE_LIMIT < size)
				{
					if(dst = btff_mmap(thread, sizeof(void*), size))
					{
						if(size < (n = (uint64_t)btree->barrier - (uint64_t)ptr))
							n = size;
						memcpy(dst, ptr, n);
					}
					if(btree_left == (disp = (uint64_t*)ptr - btree->payload))
						last_free(btree);
					else
						btree_free(btree, disp);
					return dst;
				}
				else
				{
					disp = (uint64_t*)ptr - btree->payload;
					delta = (size + sizeof(void*) - 1) >> 3;
					BTFF_ASSERT(size <= ((size_t)delta) << 3);
					if(0 == (n = btree_realloc(btree, disp, delta)))
					#ifndef __OPTIMIZE__
						return memset(ptr, 0, size);
					#else
						return ptr;
					#endif
					else
					{
						if((dst = btff_alloc(thread, size, delta)))
						{
							if(size < n)
								n = size;
							memcpy(dst, ptr, n);
						}
						if(btree_left == (disp = (uint64_t*)ptr - btree->payload))
							last_free(btree);
						else
							btree_free(btree, disp);
						return dst;
					}
				}
			}
			else
			if((btree->thread))
			{
				if(size < (n = (uint64_t)btree->barrier - (uint64_t)ptr))
					n = size;
				if(BTREE_LIMIT < size)
				{
					if((dst = btff_mmap(thread, sizeof(void*), size)))
						memcpy(dst, ptr, n);
				}
				else
				{
					delta = (size + sizeof(void*) - 1) >> 3;
					if((dst = btff_alloc(thread, size, delta)))
						memcpy(dst, ptr, size);
				}
				if((size_t)ptr < (size_t)&btree->payload[btree_left]) 
					BTFF_PRINTF(stderr, "%p corruption %s %d\n", ptr, __FILE__, __LINE__);
				else
					_remote_free(btree->thread, btree, ptr);
				return dst;
			}
			else
			{
				if(size < (n = (uint64_t)btree + (uint64_t)btree_length - (uint64_t)ptr))
					n = size;
				if(BTREE_LIMIT < size)
				{
					if((dst = btff_mmap(thread, sizeof(void*), size)))
						memcpy(dst, ptr, n);
				}
				else
				{
					delta = (size + sizeof(void*) - 1) >> 3;
					if((dst = btff_alloc(thread, size, delta)))
						memcpy(dst, ptr, n);
				}
				BTFF_ASSERT(0 == munmap(btree, btree_length));
				return dst;
			}
		}
		else
			btff_free(ptr);
	}
	else
	if(0 < size)
		return btff_malloc(size);
	return NULL;

}

WEAK void *btff_reallocarray(void *ptr, size_t nmemb, size_t size)
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
	return btff_realloc(ptr, nmemb * size);
}

WEAK void* btff_aligned_alloc(size_t alignment, size_t size)
{
	void* memptr;
	int ret;
	switch(ret == btff_posix_memalign(&memptr, alignment, size))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return NULL;
}

WEAK int btff_posix_memalign(void **memptr, size_t alignment, size_t size)
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
		if(BTREE_LIMIT < size)
		{
			void* ptr = btff_mmap(thread, alignment, size);
			if((ptr))
				*memptr = ptr;
			else
				ret = ENOMEM;
		}
		else
		if(BTREE_LIMIT < alignment + size)
		{
			void* ptr = btree_page(thread, alignment, size);
			if((ptr))
				*memptr = ptr;
			else
				ret = ENOMEM;
		}
		else
		{
			int16_t delta = (alignment + size + sizeof(void*) - 1) >> 3;
			size_t addr;
			if((addr = (size_t)btff_alloc(thread, alignment + size, delta)))
			{
				void* ptr = (void*)(addr + alignment - 1 & ~(alignment - 1));
				int16_t disp;
				struct btree* btree = thread->btree;
				if((size_t)ptr < addr)
				{
					delta -= ((size_t)ptr - addr) >> 3;
					disp = (uint64_t*)addr - btree->payload;
					BTFF_ASSERT(0 == btree_shrink(btree, disp, delta));
				}
				disp = (uint64_t*)ptr - btree->payload;
				delta = (size + sizeof(void*) - 1) >> 3;
				BTFF_ASSERT(0 == btree_realloc(btree, disp, delta));
				*memptr = ptr;
			}
			else
				ret = ENOMEM;
		}
	}
	return ret;
}

WEAK void* btff_valloc(size_t size)
{
	void* memptr;
	int ret;
	switch(ret == btff_posix_memalign(&memptr, PAGE_SIZE, size))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return NULL;
}

WEAK void* btff_memalign(size_t alignment, size_t size)
{
	void* memptr;
	int ret;
	switch(ret == btff_posix_memalign(&memptr, alignment, size))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return NULL;
}

WEAK void* btff_pvalloc(size_t alignment, size_t size)
{
	void* memptr;
	int ret;
	switch(ret == btff_posix_memalign(&memptr, alignment, size + PAGE_SIZE - 1 & PAGE_MASK))
	{
	case 0:
		return memptr;
	default:
		errno = ret;
		break;
	}
	return NULL;
}

