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
#ifndef __wbht_h__
#define __wbht_h__ __wbht_h__

#include "private.h"
#include <stddef.h>

void *wbht_malloc(size_t size)
	PRIVATE(wbht_malloc);
void wbht_free(void *ptr)
	PRIVATE(wbht_free);
void* wbht_calloc(size_t count, size_t size)
	PRIVATE(wbht_calloc);
void* wbht_realloc(void *ptr, size_t size)
	PRIVATE(wbht_realloc);
void* wbht_reallocf(void *ptr, size_t size)
	PRIVATE(wbht_reallocf);
void *wbht_reallocarray(void *ptr, size_t nmemb, size_t size)
	PRIVATE(wbht_reallocarray);
void* wbht_aligned_alloc(size_t alignment, size_t size)
	PRIVATE(wbht_aligned_alloc);
int wbht_posix_memalign(void **memptr, size_t alignment, size_t size)
	PRIVATE(wbht_posix_memalign);
void* wbht_valloc(size_t size)
	PRIVATE(wbht_valloc);
void* wbht_memalign(size_t alignment, size_t size)
	PRIVATE(wbht_memalign); 
void* wbht_pvalloc(size_t alignment, size_t size)
	PRIVATE(wbht_pvalloc);

#endif/*__wbht_h__*/
