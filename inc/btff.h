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
#ifndef __btff_h__
#define __btff_h__ __btff_h__

#include "private.h"
#include "thread.h"
#include <sys/user.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

#define BTFF_PRINTF(stream, format...) \
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
#define BTFF_ASSERT(expression) \
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
#define BTFF_ASSERT(expression) \
do \
{ \
	if(!(expression)) \
		{ \
		} \
} while(0)
#endif

void *btff_malloc(size_t size)
	PRIVATE(btff_malloc);
void btff_free(void *ptr)
	PRIVATE(btff_free);
void* btff_calloc(size_t count, size_t size)
	PRIVATE(btff_calloc);
void* btff_realloc(void *ptr, size_t size)
	PRIVATE(btff_realloc);

#endif/*__btff_h__*/
