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
#ifndef __page_h__
#define __page_h__ __page_h__

#include "private.h"
#include "thread.h"
#include <sys/user.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

enum {
	WBHT_LENGTH = PAGE_SIZE * 64,
	WBHT_ALIGN = WBHT_LENGTH, 
	WBHT_LIMIT = WBHT_LENGTH - PAGE_SIZE
};

#define WBHT_LOW ((uint64_t)(WBHT_ALIGN - 1))
#define WBHT_HIGH (UINT64_MAX - WBHT_LOW)

struct page
{
	struct thread* thread;
	int64_t fillter;
	int64_t front[1];
	uint8_t payload[WBHT_LENGTH 
		- sizeof(struct thread*) 
		- sizeof(int64_t) 
		- sizeof(int64_t) 
		- sizeof(int64_t) 
		- sizeof(struct page*) 
		- sizeof(void**)];
	int64_t back[1];
	struct page* next;
	void** free;
};

#define WBHT_PRINTF(stream, format...) \
do \
{ \
	char* str = (char*)MAP_FAILED; \
	ssize_t size = 0, count = 0; \
	int n; \
	while(count == size) \
	{ \
		size += PAGE_SIZE; \
		if(MAP_FAILED != (void*)str) \
			munmap(str, size); \
		count = 0; \
		if(MAP_FAILED == (void*)(str = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0))) \
			break; \
		count = snprintf(str, size, format); \
	} \
	for(n = 0; 0 < count && 0 <= (size = write(fileno(stream), str + n, count)); n += size, count -= size); \
	if(MAP_FAILED != (void*)str) \
		munmap(str, size); \
} while(0)

#ifndef __OPTIMIZE__
#define WBHT_ASSERT(expression) \
do \
{ \
	if(unlikely(!(expression))) \
		{ \
			char* str = (char*)MAP_FAILED; \
			ssize_t size = 0; \
			int n = 0; \
			ssize_t count; \
			while(0 == n) \
			{ \
				size += PAGE_SIZE; \
				if(MAP_FAILED != (void*)str) \
					munmap(str, size); \
				count = 0; \
				if(MAP_FAILED == (void*)(str = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0))) \
					break; \
				count = snprintf(str, size, "%s%s%s:%u: %s%sAssertion `%s' failed.\n%n", "librtbf.so:", " ", __FILE__, __LINE__, __FUNCTION__, ": ", #expression, &n); \
			} \
			for(n = 0; 0 < count && 0 <= (size = write(fileno(stderr), str + n, count)); n += size, count -= size); \
			if(MAP_FAILED != (void*)str) \
				munmap(str, size); \
			abort(); \
		} \
} while(0)
#else
#define WBHT_ASSERT(expression) \
do \
{ \
	if(!(expression)) \
		{ \
		} \
} while(0)
#endif

static inline struct page* WBHT_PAGE(void* ptr)
{
	struct page* page;
	if(WBHT_LIMIT <= (WBHT_LOW & (uint64_t)ptr))
		page = (struct page*)(PAGE_MASK & (uint64_t)ptr);
	else
		page = (struct page*)((WBHT_HIGH & (uint64_t)ptr) - PAGE_SIZE);
#ifndef __OPTIMIZE__
	assert(page->front < (int64_t*)ptr && (int64_t*)ptr < page->back);
#endif
	return page;
}

#endif/*__page_h__*/
