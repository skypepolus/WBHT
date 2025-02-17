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
#ifndef __thread_h__
#define __thread_h__ __thread_h__

#include "private.h"
#include "heap.h"
#include "list.h"
#ifdef __btff_h__
struct btree;
#else
#define HEAP_LIMIT (int64_t)(sizeof(struct heap) / sizeof(int64_t))
#endif
struct thread
{
	struct thread* next;
	struct heap* root;
	size_t addr;
	size_t length;
	int polling;
#ifdef __btff_h__
	struct btree* btree;
	struct btree* local;
	uint64_t barrier[sizeof(void*) * 15];
	struct btree* remote;
#else
	void** free;
	int reference;
	struct page* local;
	struct list list[HEAP_LIMIT / 2];
	uint64_t barrier[sizeof(void*) * 7];
	struct page* volatile remote;
	struct
	{
		uint8_t barrier[sizeof(void*) * 7];
		void** free;
	} channel[1];
#endif
};

#endif/*__thread_h__*/
