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
#include "btff.h"
#include "thread.h"
#include "btree.h"
#include "splay.h"
#include <sys/mman.h>
#include <errno.h>
#ifndef __OPTIMIZE__
static void btree_sanity(struct btree* btree);
#endif
static inline void top_free(struct btree* btree)
{
	int i = 0;
	do
	{
		if(btree_root + 2 < btree_node_count 
		&& btree_node_top - 1 == (btree_node_free = splay_last(btree_node_free)) - btree->node)
		{
			node_t* top = btree_node_free;
			btree_node_free = top->left[0];
			btree_node_count--;
			btree_node_top--;
			if(0 == btree_node_top % (PAGE_SIZE / sizeof(node_t)))
				BTFF_ASSERT(MAP_FAILED != mmap((void*)(btree->node + btree_node_top), PAGE_SIZE, PROT_NONE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0));
			i++;
		}
		else
			break;
	} while(i < btree_root + 2);
}

static inline node_t* top_alloc(struct btree* btree)
{
	int16_t i;
	node_t* node;
	if(0 == btree_node_top % (PAGE_SIZE / sizeof(node_t)))
		if(mprotect((void*)(btree->node + btree_node_top), PAGE_SIZE, PROT_READ|PROT_WRITE))
			return NULL;
	node = btree->node + btree_node_top;
	btree_node_top++;
	return node;
}

static inline int16_t leaf_alloc(struct btree* btree)
{
	node_t* node = btree_node_free = splay_first(btree_node_free);
	btree_node_free = node->right[0];
	btree_node_count--;
	memset(node->leaf, 0, LEAF_NMEMB * sizeof(*node->leaf));
	return node - btree->node;
}

static inline int16_t node_alloc(struct btree* btree)
{
	node_t* node = btree_node_free = splay_first(btree_node_free);
	int16_t i;
	btree_node_free = node->right[0];
	btree_node_count--;
	for(i = 0; i < LEAF_HALF; i++)
	{
		node->child[i] = INT16_MAX;
		node->space[i] = 0;
	}
	return node - btree->node;
}

static inline void node_free(struct btree* btree, int16_t index)
{
	node_t* node = btree->node + index;
	btree_node_free = splay_insert(btree_node_free, node);
	btree_node_count++;
}

static inline int16_t node_nmemb(register node_t* node)
{
#define child64 ((int64_t*)(node->child))
#define NO_CHILD 0x7FFF7FFF7FFF7FFF
	if(NO_CHILD != child64[16 / 4 - 1])
	{
		if(INT16_MAX != node->child[14])
			return 15;
		return 13;
	}
	else
	if(NO_CHILD != child64[8 / 4 - 1])
	{
		if(NO_CHILD != child64[12 / 4 - 1])
		{
			if(INT16_MAX != node->child[10]) 
				return 11;
			return 9;
		}
		if(INT16_MAX != node->child[6]) 
			return 7;
		return 5;
	}
	else
	if(NO_CHILD != child64[4 / 4 - 1])
	{
		if(INT16_MAX != node->child[2]) 
			return 3;
		return 1;
	}
	return 0;
#undef NO_CHILD
#undef child64
}

static inline int16_t node_max(register node_t* node)
{
	int16_t max = 0;
	int16_t i;
	for(i = node_nmemb(node) - 1; i >= 0; i--)
		if(max < node->space[i])
			max = node->space[i]; 
	return max;
}

static inline int16_t leaf_nmemb(register node_t* node)
{
#define leaf64 ((int64_t*)node->leaf)
#define leaf16(i) \
do \
{ \
	if(node->leaf[i * 4 + 2]) \
	{ \
		if(node->leaf[i * 4 + 3])  \
			return i * 4 + 4; \
		return i * 4 + 3; \
	} \
	else \
	if(node->leaf[i * 4 + 1])  \
		return i * 4 + 2; \
	return i * 4 + 1; \
} while(0)
	if(leaf64[7])
		leaf16(7);
	else
	if(leaf64[3])
	{
		if(leaf64[5])
		{
			if(leaf64[6])
				leaf16(6);
			leaf16(5);
		}
		else
		if(leaf64[4])
			leaf16(4);
		leaf16(3);
	}
	else
	if(leaf64[1])
	{
		if(leaf64[2])
			leaf16(2);
		leaf16(1);
	}
	else
	if(leaf64[0])
		leaf16(0);
	return 0;
#undef leaf16
#undef leaf64
}
#ifdef AVX512F
int16_t leaf_max(register node_t* node) asm(".btree.leaf.max");
asm(
".btree.leaf.max:							\n\t"
	"MOVDQA XMM0, XMMWORD PTR [RDI]			\n\t"
	"MOVDQA XMM1, XMMWORD PTR [RDI+16]		\n\t"
	"PMAXSW XMM0, XMM1						\n\t"
	"MOVDQA XMM2, XMMWORD PTR [RDI+(2*16)]	\n\t"
	"MOVDQA XMM3, XMMWORD PTR [RDI+(3*16)]	\n\t"
	"PMAXSW XMM2, XMM3						\n\t"
	"PXOR XMM1,XMM1							\n\t"
	"PCMPEQD XMM3,XMM3						\n\t"
	"PMAXSW XMM0, XMM2						\n\t"
	"PMAXSW XMM1, XMM0						\n\t"
	"PSUBUSW XMM3, XMM1						\n\t"
	"PHMINPOSUW XMM5, XMM3					\n\t"
	"MOVD EAX, XMM5							\n\t"	
	"NOT AX									\n\t"
	"RET									\n\t"
);
#else			
static inline int16_t leaf_max(register node_t* node)
{
	int16_t max = 0;
	int16_t i;
	for(i = leaf_nmemb(node) - 1; i >= 0; i--)
		if(max < node->leaf[i])
			max = node->leaf[i]; 
	return max;
}
#endif
static inline void leaf_remove(node_t* node, int16_t i, int16_t c)
{
	memmove(node->leaf + i, node->leaf + i + c, (LEAF_NMEMB - (i + c)) * sizeof(int16_t));
	memset(node->leaf + LEAF_NMEMB - c, 0, c * sizeof(int16_t));
}

static inline int16_t leaf_previous(struct btree* btree)
{
	int16_t h = 0;
	for(h++; h <= btree_root; h++)
		if(0 < btree_stack(h, BRANCH))
			return h;
	return 0;
}

static inline int16_t leaf_left(struct btree* btree)
{
	int16_t h = leaf_previous(btree);
	if(0 < h)
	{
		node_t* node = &btree->node[btree_stack(h, NODE)];
		int16_t i = btree_stack(h, BRANCH) - 1;
		if(0 < node->space[i])
			return node->child[i] + node->space[i];
		else
			return node->child[i] - node->space[i];
	}
	return btree_left;
}
#ifndef __OPTIMIZE__
static inline int16_t leaf_sum(node_t* node)
{
	int16_t sum, i;
	for(sum = 0, i = 0; i < LEAF_NMEMB; i++)
	{
		if(0 < node->leaf[i])
			sum += node->leaf[i];
		else
			sum -= node->leaf[i];
	}
	return sum;
}
#endif
static inline int16_t btree_search(struct btree* btree, int16_t disp)
{
	int16_t h;
	int16_t i, left;
	node_t* node;
	for(h = btree_root; h > 0; h--)
	{
		node = &btree->node[btree_stack(h, NODE)];
		if(node->child[7] < disp)
		{
			if(node->child[11] < disp)
			{
				if(node->child[13] < disp)
				{
					btree_stack(h, BRANCH) = 14;
					btree_stack(h - 1, MAX) = node->space[14];
					btree_stack(h - 1, NODE) = node->child[14];
				}
				else
				if(disp == node->child[13])
				{
					btree_stack(h, BRANCH) = 13;
					return h;
				}
				else
				{
					btree_stack(h, BRANCH) = 12;
					btree_stack(h - 1, MAX) = node->space[12];
					btree_stack(h - 1, NODE) = node->child[12];
				}
			}
			else
			if(disp == node->child[11])
			{
				btree_stack(h, BRANCH) = 11;
				return h;
			}
			else
			{
				if(node->child[9] < disp)
				{
					btree_stack(h, BRANCH) = 10;
					btree_stack(h - 1, MAX) = node->space[10];
					btree_stack(h - 1, NODE) = node->child[10];
				}
				else
				if(disp == node->child[9])
				{
					btree_stack(h, BRANCH) = 9;
					return h;
				}
				else
				{
					btree_stack(h, BRANCH) = 8;
					btree_stack(h - 1, MAX) = node->space[8];
					btree_stack(h - 1, NODE) = node->child[8];
				}
			}
		}
		else
		if(disp == node->child[7])
		{
			btree_stack(h, BRANCH) = 7;
			return h;
		}
		else
		{
			if(node->child[3] < disp)
			{
				if(node->child[5] < disp)
				{
					btree_stack(h, BRANCH) = 6;
					btree_stack(h - 1, MAX) = node->space[6];
					btree_stack(h - 1, NODE) = node->child[6];
				}
				else
				if(disp == node->child[5])
				{
					btree_stack(h, BRANCH) = 5;
					return h;
				}
				else
				{
					btree_stack(h, BRANCH) = 4;
					btree_stack(h - 1, MAX) = node->space[4];
					btree_stack(h - 1, NODE) = node->child[4];
				}
			}
			else
			if(disp == node->child[3])
			{
				btree_stack(h, BRANCH) = 3;
				return h;
			}
			else
			{
				if(node->child[1] < disp)
				{
					btree_stack(h, BRANCH) = 2;
					btree_stack(h - 1, MAX) = node->space[2];
					btree_stack(h - 1, NODE) = node->child[2];
				}
				else
				if(disp == node->child[1])
				{
					btree_stack(h, BRANCH) = 1;
					return h;
				}
				else
				{
					btree_stack(h, BRANCH) = 0;
					btree_stack(h - 1, MAX) = node->space[0];
					btree_stack(h - 1, NODE) = node->child[0];
				}
			}
		}
	}
	node = &btree->node[btree_stack(0, NODE)];
	left = leaf_left(btree);
	for(i = 0; i < LEAF_NMEMB; i++)
	{
		if(left < disp)
		{
			if(0 < node->leaf[i])
				left += node->leaf[i];
			else
			if(0 > node->leaf[i])
				left -= node->leaf[i];
			else
				return -1;
		}
		else
		if(disp == left)
		{
			btree->leaf[i] = left;
			btree_stack(0, BRANCH) = i;
			return 0;
		}
		else
			return -1;
	}
	return -1;
}

static inline unsigned leaf_rebalance(struct btree* btree)
{
	unsigned ret = 0;
	node_t* parent = &btree->node[btree_stack(1, NODE)];
	int16_t i = btree_stack(1, BRANCH);
	node_t* left;
	node_t* right;
	int16_t left_nmemb;
	int16_t right_nmemb;
	if(0 == (i & 1))
	{
		if(0 < i)
			i--;
		else
			i++;
	}
	left = &btree->node[parent->child[i - 1]];
	left_nmemb = leaf_nmemb(left);
	right = &btree->node[parent->child[i + 1]];
	right_nmemb = leaf_nmemb(right);
	if(32 >= left_nmemb + 1 + right_nmemb)
	{
		int16_t nmemb = node_nmemb(parent);
		if(parent->space[i - 1] < parent->space[i])
			parent->space[i - 1] = parent->space[i];
		if(parent->space[i - 1] < parent->space[i + 1])
			parent->space[i - 1] = parent->space[i + 1];
		left->leaf[left_nmemb] = parent->space[i];
		left_nmemb++;
		memcpy(left->leaf + left_nmemb, right->leaf, sizeof(int16_t)* right_nmemb);
		node_free(btree, right - btree->node);
		memmove(parent->child + i, parent->child + i + 2, sizeof(int16_t) * (nmemb - (i + 2)));
		memmove(parent->space + i, parent->space + i + 2, sizeof(int16_t) * (nmemb - (i + 2)));
		for(nmemb -= 2; parent->child[nmemb] < INT16_MAX; nmemb++)
		{
			parent->child[nmemb] = INT16_MAX;
			parent->space[nmemb] = 0;
		}
		return 1;
	}
	else
	if(left_nmemb + 1 < right_nmemb)
	{
		int16_t diff = (right_nmemb - left_nmemb) / 2;
		int16_t l, r;
		unsigned decrease = 0;
		for(l = left_nmemb, r = 0; r < diff; l++, r++)
		{
			if(parent->space[i - 1] < (left->leaf[l] = parent->space[i]))
				parent->space[i - 1] = left->leaf[l];
			if(0 < parent->space[i])
				parent->child[i] += parent->space[i];
			else
				parent->child[i] -= parent->space[i];
			if(parent ->space[i + 1] == (parent->space[i] = right->leaf[r]))
				decrease = 1;
		}
		memmove(right->leaf, right->leaf + diff, sizeof(int16_t) * (LEAF_NMEMB - diff));
		memset(right->leaf + right_nmemb - diff, 0, sizeof(int16_t) * diff);
		if(decrease)
			parent->space[i + 1] = leaf_max(right);
	}
	else
	if(left_nmemb > 1 + right_nmemb)
	{
		int16_t diff = (left_nmemb - right_nmemb) / 2;
		int16_t l, r;
		unsigned decrease = 0;
		memmove(right->leaf + diff, right->leaf, sizeof(int16_t) * right_nmemb);
		for(l = left_nmemb - 1, r = diff - 1; r >= 0; l--, r--)
		{
			if(parent->space[i + 1] < (right->leaf[r] = parent->space[i]))
				parent->space[i + 1] = right->leaf[r];
			if(0 < left->leaf[l])
				parent->child[i] -= left->leaf[l];
			else
				parent->child[i] += left->leaf[l];
			if(parent->space[i - 1] == (parent->space[i] = left->leaf[l]))
				decrease = 1;
			left->leaf[l] = 0;
		}
		if(decrease)
			parent->space[i - 1] = leaf_max(left);
	}
	return 0;
}

static unsigned node_rebalance(struct btree* btree, int16_t h)
{
	node_t* parent = &btree->node[btree_stack(h + 1, NODE)];
	int16_t i = btree_stack(h + 1, BRANCH);
	node_t* left;
	node_t* right;
	int16_t l, r, n;
	if(0 == (i & 1))
	{
		if(0 < i)
			i--;
		else
			i++;
	}
	n = node_nmemb(parent);
	left = &btree->node[parent->child[i - 1]];
	l = node_nmemb(left);
	right = &btree->node[parent->child[i + 1]];
	r = node_nmemb(right);
	if(NODE_HALF + NODE_HALF >= l + 1 + r)
	{
		if(parent->space[i - 1] < parent->space[i])
			parent->space[i - 1] = parent->space[i];
		if(parent->space[i - 1] < parent->space[i + 1])
			parent->space[i - 1] = parent->space[i + 1];

		left->child[l] = parent->child[i];
		left->space[l] = parent->space[i];
		l++;
		memcpy(left->child + l, right->child, sizeof(int16_t) * r);
		memcpy(left->space + l, right->space, sizeof(int16_t) * r);
		node_free(btree, right - btree->node);
		memmove(parent->child + i, parent->child + i + 2, sizeof(int16_t) * (n - (i + 2))); memmove(parent->space + i, parent->space + i + 2, sizeof(int16_t) * (n - (i + 2)));
		for(n -= 2; parent->child[n] < INT16_MAX; n++)
		{
			parent->child[n] = INT16_MAX;
			parent->space[n] = 0;
		}
		return 1;
	}
	else
	if(l + 4 <= r)
	{
		left->child[l] = parent->child[i]; 
		if(parent->space[i - 1] < (left->space[l] = parent->space[i]))
			parent->space[i - 1] = left->space[l];
		l++;
		left->child[l] = right->child[0];
		if(parent->space[i - 1] < (left->space[l] = right->space[0]))
			parent->space[i - 1] = left->space[l];
		l++;
		parent->child[i] = right->child[1];
		parent->space[i] = right->space[1];
		memmove(right->child, right->child + 2, sizeof(int16_t) * (r - 2)); memmove(right->space, right->space + 2, sizeof(int16_t) * (r - 2));
		for(r -= 2; right->child[r] < INT16_MAX; r++)
		{
			right->child[r] = INT16_MAX;
			right->space[r] = 0;
		}
		parent->space[i + 1] = node_max(right);
	}
	else
	if(l >= r + 4)
	{
		memmove(right->child + 2, right->child, sizeof(int16_t) * r);
		memmove(right->space + 2, right->space, sizeof(int16_t) * r);
		right->child[1] = parent->child[i]; 
		if(parent->space[i + 1] < (right->space[1] = parent->space[i]))
			parent->space[i + 1] = right->space[1];
		l--;
		right->child[0] = left->child[l];
		left->child[l] = INT16_MAX;
		if(parent->space[i + 1] < (right->space[0] = left->space[l]))
			parent->space[i + 1] = right->space[0];
		left->space[l] = 0;
		l--;
		parent->child[i] = left->child[l];
		left->child[l] = INT16_MAX;
		parent->space[i] = left->space[l];
		left->space[l] = 0;

		parent->space[i - 1] = node_max(left);
	}
	return 0;
}

static inline int16_t btree_rebalance(struct btree* btree, int16_t h, int16_t root)
{
	if(h < root)
	{
		node_t* node;
		if(0 == h)
		{
			if(!leaf_rebalance(btree))
				return h;
		}
		else
		{
			if(!node_rebalance(btree, h))
				return h;
		}
		for(h++; h < root; h++)
			if(!node_rebalance(btree, h))
				return h;
		node = &btree->node[btree_stack(btree_root, NODE)];
		if(INT16_MAX == node->child[1])
		{
			btree_stack(btree_root - 1, MAX) = node->space[0]; btree_stack(btree_root - 1, NODE) = node->child[0]; 
			node_free(btree, node - btree->node); btree_root--; return h;
		}
	} return h;
}

static inline void thread_btree_swap(struct btree* btree)
{
	struct thread* thread = btree_thread;
	btree_key = *btree->header[HEADER_SPACE].int64 * sizeof(void*);
	if(thread->btree)
	{
		if(thread->btree_stack(thread->btree_root, MAX) < btree_stack(btree_root, MAX))
		{
			struct btree* tmp = thread->btree;
			thread->root = heap_remove(thread->root, &btree->heap);
			thread->btree = btree;
			btree = tmp;
			heap_insert(&thread->root, &btree->heap, btree_key);
		}
		else
		if(btree->heap.size[HEAP_SIZE] < btree_key)
			heap_increase(&btree->heap, btree_key);
		else
		if(btree->heap.size[HEAP_SIZE] > btree_key)
			heap_decrease(&btree->heap, btree_key);
	}
	else
	{
		thread->root = heap_remove(thread->root, &btree->heap);
		thread->btree = btree;
	}
}

static inline int16_t btree_decrease(struct btree* btree, int16_t h, int16_t root)
{
	for(; h <= root; h++)
	{
		node_t* node = &btree->node[btree_stack(h, NODE)];
		int16_t i = btree_stack(h, BRANCH);
		if(btree_stack(h, MAX) > node->space[i])
		{
			node->space[i] = btree_stack(h - 1, MAX);
			return h;
		}
		else
		{
			node->space[i] = btree_stack(h - 1, MAX);
			btree_stack(h, MAX) = node_max(node);
		}
	}
	if(btree_root < h && btree_left < btree_space)
	{
		btree_space = btree_left > btree_stack(btree_root, MAX) ? btree_left : btree_stack(btree_root, MAX);
		if(btree_thread->btree == btree)
			btree_key = *btree->header[HEADER_SPACE].int64 * sizeof(void*);
		else
			thread_btree_swap(btree);
	}
	return h;
}

static inline int16_t node_decrease(struct btree* btree, int16_t h, int16_t delta, int16_t root)
{
	node_t* node = &btree->node[btree_stack(h, NODE)];
	int16_t i = btree_stack(h, BRANCH);
	if(btree_stack(h, MAX) > node->space[i])
	{
		node->space[i] = delta;
		return h;
	}
	else
	{
		node->space[i] = delta;
		btree_stack(h, MAX) = node_max(node);
	}
	return btree_decrease(btree, h + 1, root);
}

static inline int16_t leaf_decrease(struct btree* btree, int16_t delta, int16_t root)
{
	node_t* node = &btree->node[btree_stack(0, NODE)];
	int16_t i = btree_stack(0, BRANCH);
	if(btree_stack(0, MAX) > node->leaf[i])
	{
		node->leaf[i] = delta;
		return 0;
	}
	else
	{
		node->leaf[i] = delta;
		btree_stack(0, MAX) = leaf_max(node);
	}
	return btree_decrease(btree, 1, root);
}

static inline int16_t btree_increase(struct btree* btree, int16_t h, int16_t root)
{
	node_t* node = &btree->node[btree_stack(h, NODE)];
	int16_t i = btree_stack(h, BRANCH);
	if(0 < h)
	{
		if(btree_stack(h, MAX) < node->space[i])
			btree_stack(h, MAX) = node->space[i];
		else
			return h;
	}
	else
	{
		if(btree_stack(h, MAX) < node->leaf[i])
			btree_stack(h, MAX) = node->leaf[i];
		else
			return h;
	}
	for(h++; h <= root; h++)
	{
		node = &btree->node[btree_stack(h, NODE)];
		i = btree_stack(h, BRANCH);
		if(btree_stack(h, MAX) < (node->space[i] = btree_stack(h - 1, MAX)))
			btree_stack(h, MAX) = node->space[i];
		else
			return h;
	}
	if(btree_root < h && btree_space < btree_stack(btree_root, MAX))
	{
		btree_space = btree_stack(btree_root, MAX);

		if(btree_thread->btree == btree)
			btree_key = *btree->header[HEADER_SPACE].int64 * sizeof(void*);
		else
			thread_btree_swap(btree);
	}
	return h;
}

static inline int16_t leaf_next(struct btree* btree)
{
	int16_t h = 0;
	for(h++; h <= btree_root; h++)
	{
		node_t* node = &btree->node[btree_stack(h, NODE)];
		if(node->child[btree_stack(h, BRANCH) + 1] < INT16_MAX)
			return h;
	}
	return 0;
}

static inline int16_t leaf_right(struct btree* btree)
{
	int16_t h = 0;
	for(h++; h <= btree_root; h++)
	{
		node_t* node = &btree->node[btree_stack(h, NODE)];
		if(node->child[btree_stack(h, BRANCH) + 1] < INT16_MAX)
			return node->child[btree_stack(h, BRANCH) + 1];
	}
	return btree_right;
}
#ifndef __OPTIMIZE__
static void node_sanity(struct btree* btree, int16_t h)
{
	if(0 < h)
	{
		node_t* node = &btree->node[btree_stack(h, NODE)];
		int16_t i;
		for(i = 0; i < node_nmemb(node); i += 2)
		{
			btree_stack(h, BRANCH) = i;
			btree_stack(h - 1, MAX) = node->space[i];
			btree_stack(h - 1, NODE) = node->child[i];
			node_sanity(btree, h - 1);
		}
	}
	else
	{
		node_t* node = &btree->node[btree_stack(0, NODE)];
		int16_t left = leaf_left(btree);
		int16_t right = leaf_right(btree);
		int16_t sum = leaf_sum(node);
		BTFF_ASSERT(left + sum == right);
	}
}

static void btree_sanity(struct btree* btree)
{
	node_sanity(btree, btree_root);
}
#endif
static inline int16_t first_fit(struct btree* btree, int16_t key)
{
	int16_t h;
	int16_t i, right;
	node_t* node;

	for(h = btree_root; h > 0; h--)
	{
		node = &btree->node[btree_stack(h, NODE)];
		for(i = node_nmemb(node) - 1; i >= 0; i--)
		{
			if(key <= node->space[i])
			{
				btree_stack(h, BRANCH) = i;
				btree_stack(h - 1, MAX) = node->space[i];
				btree_stack(h - 1, NODE) = node->child[i];
				break;
			}
			i--;	
			if(key <= node->space[i])
			{
				btree_stack(h, BRANCH) = i;
				return h;
			}
		}
	}
	node = &btree->node[btree_stack(0, NODE)];
	right = leaf_right(btree);
	i = leaf_nmemb(node);
	for(i--; i >= 0; i--)
	{
		if(0 < node->leaf[i])
			right -= node->leaf[i];
		else
			right += node->leaf[i];
		if(key <= node->leaf[i])
		{
			btree->leaf[i] = right;
			btree_stack(0, BRANCH) = i;
			return 0;
		}
	}
	return -1;
}

static inline void btree_first(struct btree* btree, int16_t left, int16_t h)
{	
	node_t* node;
	for(; h > 0; h--)
	{
		node_t* node = &btree->node[btree_stack(h, NODE)];
		btree_stack(h, BRANCH) = 0;
		btree_stack(h - 1, MAX) = node->space[0];
		btree_stack(h - 1, NODE) = node->child[0];
	}
	btree->leaf[0] = left;
	btree_stack(0, BRANCH) = 0;
}

static inline void btree_last(struct btree* btree, int16_t h, int16_t right)
{
	int16_t i;
	node_t* node;
	for(; h > 0; h--)
	{
		node = &btree->node[btree_stack(h, NODE)];
		i = node_nmemb(node) - 1;
		btree_stack(h, BRANCH) = i;
		btree_stack(h - 1, MAX) = node->space[i];
		btree_stack(h - 1, NODE) = node->child[i];
	}
	node = &btree->node[btree_stack(0, NODE)];
	i = leaf_nmemb(node) - 1;
	if(0 < node->leaf[i])
		right -= node->leaf[i];
	else
		right += node->leaf[i];
	btree->leaf[i] = right;
	btree_stack(0, BRANCH) = i;
}

static inline void node_next(struct btree* btree, int16_t h)
{
	int16_t i, left;
	node_t* node = &btree->node[btree_stack(h, NODE)];
	i = btree_stack(h, BRANCH);
	left = node->child[i];
	if(0 < node->space[i])
		left += node->space[i];
	else
		left -= node->space[i];
	i++;
	btree_stack(h, BRANCH) = i;
	btree_stack(h - 1, MAX) = node->space[i];
	btree_stack(h - 1, NODE) = node->child[i];
	btree_first(btree, left, h - 1);
}

static inline void node_previous(struct btree* btree, int16_t h)
{
	int16_t i, right;
	node_t* node = &btree->node[btree_stack(h, NODE)];
	i = btree_stack(h, BRANCH);
	right = node->child[i];
	i--;
	btree_stack(h, BRANCH) = i;
	btree_stack(h - 1, MAX) = node->space[i];
	btree_stack(h - 1, NODE) = node->child[i];
	btree_last(btree, h - 1, right);
}

struct split
{
	struct
	{
		int16_t max;
		int16_t node;
	} left;
	int16_t middle;
	int16_t space;
	struct
	{
		int16_t max;
		int16_t node;
	} right;
}; 

static inline void leaf_split(struct btree* btree, int16_t insert, int16_t delta, struct split* split)
{
	node_t* node = btree->node + btree_stack(0, NODE);
	int16_t l, r;
	int16_t lmax = 0;
	int16_t rmax = 0;
	int16_t* left = node->leaf;
	int16_t* right = btree->node[split->right.node = leaf_alloc(btree)].leaf;
	int16_t space = 0;
	int16_t middle = leaf_left(btree);
	split->left.node = node - btree->node;
	
	if(insert <= LEAF_HALF)
	{
		for(l = 0; l < insert; l++)
		{
			if(0 < left[l])
			{
				if(lmax < left[l])
					lmax = left[l];
				middle += left[l];
			}
			else
				middle -= left[l];
		}
		for(space = delta; l < LEAF_HALF; space = delta, l++)
		{
			delta = left[l];
			if(0 < (left[l] = space))
			{
				if(lmax < left[l])
					lmax = left[l];
				middle += left[l];
			}
			else
				middle -= left[l];
		}
		for(r = 0; l < LEAF_NMEMB; l++, r++)
		{
			if(rmax < (right[r] = left[l]))
				rmax = right[r];
			left[l] = 0;
		}
	}
	else
	{
		for(l = 0; l < LEAF_HALF; l++)
		{
			if(0 < left[l])
			{
				if(lmax < left[l])
					lmax = left[l];
				middle += left[l];
			}
			else
				middle -= left[l];
		}
		space = left[l];
		left[l] = 0;
		for(r = 0, l++; l < insert; l++, r++)
		{
			if(rmax < (right[r] = left[l]))
				rmax = right[r];
			left[l] = 0;
		}
		if(rmax < (right[r] = delta))
			rmax = right[r];
		for(r++; l < LEAF_NMEMB; l++, r++)
		{
			if(rmax < (right[r] = left[l]))
				rmax = right[r];
			left[l] = 0;
		}
	}
	split->left.max = lmax;
	split->middle = middle;
	split->space = space;
	split->right.max = rmax;
}

static inline void leaf_insert(register node_t* node, register int16_t i, register int16_t delta)
{
	memmove(node->leaf + i + 1, node->leaf + i, (LEAF_NMEMB - (i + 1)) * sizeof(int16_t));
	node->leaf[i] = delta;
}

static inline void node_split(struct btree* btree, node_t* node, struct split* split)
{
	int16_t l;
	int16_t r;
	int16_t lmax;
	int16_t rmax;

	node_t* left = node;
	node_t* right = btree->node + node_alloc(btree);

	l = NODE_HALF;

	split->middle = left->child[l];
	left->child[l] = INT16_MAX;
	split->space = left->space[l];
	left->space[l] = 0;

	for(r = 0, l++; r < NODE_HALF; l++, r++)
	{
		right->child[r] = left->child[l];
		left->child[l] = INT16_MAX;
		right->space[r] = left->space[l];
		left->space[l] = 0;
	}

	split->left.node = left - btree->node;
	split->right.node = right - btree->node;
	
	split->left.max = node_max(left);
	split->right.max = node_max(right);
}

static inline void node_insert(struct btree* btree, node_t* node, int16_t branch, struct split* split)
{
	int i;
	for(i = node_nmemb(node) - 1; i > branch; i--)
	{
		node->child[i + 2] = node->child[i];
		node->space[i + 2] = node->space[i];
	}
	node->child[i + 2] = split->right.node;
	node->space[i + 2] = split->right.max;
	node->child[i + 1] = split->middle;
	node->space[i + 1] = split->space;
	BTFF_ASSERT(node->child[i] == split->left.node);
	node->space[i] = split->left.max;
}

static inline int16_t split_max(struct split* split)
{
	int16_t max = split->left.max;
	if(max < split->space)
		max = split->space;
	if(max < split->right.max)
		max = split->right.max;
	return max;
}

static inline void btree_split(struct btree* btree, int16_t insert, int16_t delta, int16_t root)
{
	int16_t h;
	struct split split[1];
	leaf_split(btree, insert, delta, split);
	for(h = 1; h <= root; h++)
	{
		node_t* node = &btree->node[btree_stack(h, NODE)];
		node_insert(btree, node, btree_stack(h, BRANCH), split);
		if(node->child[NODE_SPLIT - 1] < INT16_MAX)
			node_split(btree, node, split);
		else
			break;
	}
	if(unlikely(btree_root < h))
	{
		node_t* node = btree->node + node_alloc(btree);
		int16_t i = 0;
		node->child[i] = split->left.node;
		node->space[i] = split->left.max;
		i++;
		node->child[i] = split->middle;
		node->space[i] = split->space;
		i++;
		node->child[i] = split->right.node;
		node->space[i] = split->right.max;

		btree_stack(h, MAX) = split_max(split);
		btree_stack(h, NODE) = ((node_t*)node) - btree->node;
		btree_root = h;
	}
}

static inline void* last_alloc(struct btree* btree, int16_t delta)
{
	node_t* node;
	btree_first(btree, btree_left, btree_root);
	node = &btree->node[btree_stack(0, NODE)];

	btree_left -= delta;
	if(unlikely(btree_protect > btree_left))
	{
		size_t length = 0;
		length += btree_protect - btree_left;
		length *= sizeof(void*);
		length += PAGE_SIZE - 1;
		length &= PAGE_MASK;
		btree_protect -= length / sizeof(void*);
		if(0 > btree_protect)
		{
			btree_protect += PAGE_SIZE / sizeof(void*);
			length -= PAGE_SIZE;
		}
		if(0 < length)
		{
			if(mprotect((void*)&btree->payload[btree_protect], length, PROT_READ|PROT_WRITE))
			{
				errno = ENOMEM;
				btree_left += delta;
				btree_protect += length / sizeof(void*);
				BTFF_PRINTF(stderr, "ENOMEM %lu %s %d\n", ((size_t)delta) << 3, __FILE__, __LINE__);
				return NULL;
			}
		}
	} BTFF_ASSERT(0 >= node->leaf[0]);
	if(likely(0 == node->leaf[LEAF_NMEMB - 1]))
		leaf_insert(node, 0, -1 * delta);
	else
		btree_split(btree, 0, -1 * delta, btree_root);
	btree_space = btree_left > btree_stack(btree_root, MAX) ? btree_left : btree_stack(btree_root, MAX);
	btree_key = *btree->header[HEADER_SPACE].int64 * sizeof(void*);
#ifndef __OPTIMIZE__
	btree_sanity(btree);
	return memset((void*)&btree->payload[btree_left], 0, ((size_t)delta) << 3);
#else
	return (void*)&btree->payload[btree_left];
#endif
}

WEAK struct thread* last_free(struct btree* btree) /* remove from left most */
{
	node_t* node;
	int16_t i;
	struct thread* thread = btree_thread;

	btree_first(btree, btree_left, btree_root);
	node = &btree->node[btree_stack(0, NODE)];
	BTFF_ASSERT(0 > node->leaf[0]);
	btree_left -= node->leaf[0];
	leaf_remove(node, 0, 1);
	if(0 < node->leaf[0])
	{
		btree_left += node->leaf[0];
		if(btree_stack(0, MAX) == node->leaf[0])
			leaf_decrease(btree, INT16_MIN, btree_root);
		leaf_remove(node, 0, 1);
	}

	if(unlikely(btree_protect + PAGE_SIZE / sizeof(void*) <= btree_left))
	{
		size_t length = 0;
		length += btree_left - btree_protect;
		length *= sizeof(void*);
		length &= PAGE_MASK;
		BTFF_ASSERT(MAP_FAILED != mmap((void*)&btree->payload[btree_protect], length, PROT_NONE, MAP_FIXED|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0));
		btree_protect += length / sizeof(void*);
	}

	if(unlikely(0 == node->leaf[LEAF_HALF - 1]))
		btree_rebalance(btree, 0, btree_root);
	if(unlikely(btree_left == btree_right))
	{
		if(thread->local == btree)
			thread->local = NULL;
		if(thread->btree == btree)
		{
			thread->btree = NULL;
			BTFF_ASSERT(0 == munmap((void*)btree, btree_length));
			if((thread->root))
				return thread;
		}
		else
		{
			thread->root = heap_remove(thread->root, &btree->heap);
			BTFF_ASSERT(0 == munmap((void*)btree, btree_length));
			if((thread->root))
				return thread;
			if((thread->btree))
				return thread;
		}
		return NULL;
	}
	else
	if(btree_left > btree_space)
	{
		btree_space = btree_left;

		if(btree_thread->btree == btree)
			btree_key = *btree->header[HEADER_SPACE].int64 * sizeof(void*);
		else
			thread_btree_swap(btree);
	}
	top_free(btree);
#ifndef __OPTIMIZE__
	btree_sanity(btree);
#endif
	return thread;
}

WEAK void* first_fit_alloc(struct btree* btree, int16_t delta)
{
	node_t* node;
	int16_t h;
	int16_t n;
	int16_t disp;
	BTFF_ASSERT(sizeof(struct btree) == BTREE_LENGTH);
	BTFF_ASSERT(BTREE_LENGTH == btree_length);

	if(btree_node_count < btree_root + 2) 
	{ 
		if((btree_node_free)) 
			btree_node_free = splay_last(btree_node_free); 
		do 
		{ 
			if((node = top_alloc(btree))) 
			{ 
				node->left[0] = btree_node_free; 
				node->right[0] = NULL; 
				btree_node_free = node; 
				btree_node_count++; 
			} 
			else 
			{ 
				errno = ENOMEM; 
				BTFF_PRINTF(stderr, "ENOMEM %lu %s %d\n", ((size_t)delta) << 3, __FILE__, __LINE__);
				return NULL;
			} 
		} while(btree_node_count < btree_root + 2); 
	} 

	if(btree_stack(btree_root, MAX) < delta)
		return last_alloc(btree, delta);
	else
	switch((int)(h = first_fit(btree, delta)))
	{
	case -1:
		BTFF_ASSERT(0 <= h);
		break;
	case 0:
		node = &btree->node[btree_stack(0, NODE)];
		n = btree_stack(0, BRANCH);
		disp = btree->leaf[n];
		if(delta < node->leaf[n])
		{
			disp += node->leaf[n];
			disp -= delta;
			leaf_decrease(btree, node->leaf[n] - delta, btree_root);
			if(node->leaf[LEAF_NMEMB - 1])
				btree_split(btree, n + 1, -1 * delta, btree_root);
			else
				leaf_insert(node, n + 1, -1 * delta);
		}
		else
			leaf_decrease(btree, -1 * node->leaf[n], btree_root);
		break;
	default:
		node = &btree->node[btree_stack(h, NODE)];
		n = btree_stack(h, BRANCH);
		disp = node->child[n];
		if(delta < node->space[n])
		{
			node_t* right;
			node_next(btree, h);
			right = &btree->node[btree_stack(0, NODE)];
			disp += node->space[n];
			disp -= delta; 
			btree_stack(h, BRANCH) = n;
			node_decrease(btree, h, node->space[n] - delta, btree_root);
			if(right->leaf[LEAF_NMEMB - 1])
			{
				btree_stack(h, BRANCH) = n + 1;
				btree_split(btree, 0, -1 * delta, btree_root);
			}
			else
				leaf_insert(right, 0, -1 * delta);
		}
		else
			node_decrease(btree, h, -1 * delta, btree_root);
		break;
	}
#ifndef __OPTIMIZE__
	btree_sanity(btree);
	return memset((void*)&btree->payload[disp], 0, ((size_t)delta) << 3);
#else
	return (void*)&btree->payload[disp];
#endif
}

WEAK void btree_free(struct btree* btree, int16_t disp)
{
	int16_t h, l, n;
	node_t* left;
	node_t* node;
	node_t* right;
#ifndef __OPTIMIZE__
	btree_sanity(btree);
#endif
	switch((int)(h = btree_search(btree, disp)))
	{
	case -1:
		BTFF_PRINTF(stderr, "*** btff detected *** : double free or corruption : %p *** %s %d\n", btree->payload + disp, __FILE__, __LINE__);
#ifndef __OPTIMIZE__
		btree_sanity(btree);
#endif
		break;
	case 0:
		node = &btree->node[btree_stack(0, NODE)];
		n = btree_stack(0, BRANCH); BTFF_ASSERT(0 > node->leaf[n]);
		if(unlikely(0 == n))
		{
			h = leaf_previous(btree);
			left = &btree->node[btree_stack(h, NODE)];
			l = btree_stack(h, BRANCH) - 1;
			if(0 < left->space[l])
			{
				if(0 < node->leaf[1])
				{
					left->space[l] -= node->leaf[0];
					left->space[l] += node->leaf[1];
					btree_increase(btree, h, btree_root);
					btree_stack(0, BRANCH) = 1;
					leaf_decrease(btree, INT16_MIN, h);
					leaf_remove(node, 0, 2);
				}
				else
				{
					left->space[l] -= node->leaf[0]; btree_stack(h, BRANCH) = l;
					btree_increase(btree, h, btree_root);
					leaf_remove(node, 0, 1);
				}
			}
			else
			if(0 < node->leaf[1])
			{
				node->leaf[0] *= -1;
				node->leaf[0] += node->leaf[1];
				leaf_remove(node, 1, 1);
				btree_increase(btree, 0, btree_root);
			}
			else
			{
				node->leaf[0] *= -1; BTFF_ASSERT(0 < node->leaf[0]);
				btree_increase(btree, 0, btree_root);
			}
		}
		else
		if(unlikely(LEAF_NMEMB - 1 == n || 0 == node->leaf[n + 1]))
		{
			if(unlikely(disp - node->leaf[n] == btree_right))
			{
				if(0 < node->leaf[n - 1])
				{
					node->leaf[n - 1] -= node->leaf[n];
					node->leaf[n] = 0;
					n--;
					btree_stack(0, BRANCH) = n;
				}
				else
					node->leaf[n] *= -1; BTFF_ASSERT(0 < node->leaf[n]);
				btree_increase(btree, 0, btree_root);
			}
			else
			{
				int16_t r;
				h = leaf_next(btree);
				right = &btree->node[btree_stack(h, NODE)];
				r = btree_stack(h, BRANCH) + 1;
				if(0 < right->space[r])
				{
					if(0 < node->leaf[n - 1])
					{
						right->space[r] -= node->leaf[n];
						right->child[r] += node->leaf[n];
						right->space[r] += node->leaf[n - 1];
						right->child[r] -= node->leaf[n - 1];
						btree_increase(btree, h, btree_root);

						if(node->leaf[n - 1] < node->leaf[n])
						{
							node->leaf[n - 1] = INT16_MIN;
							leaf_decrease(btree, 0, h);
							node->leaf[n - 1] = 0;
						}
						else
						{
							node->leaf[n] = 0;
							btree_stack(0, BRANCH) = n - 1;
							leaf_decrease(btree, 0, h);
						}
					}
					else
					{
						right->child[r] += node->leaf[n];
						right->space[r] -= node->leaf[n];
						node->leaf[n] = 0; btree_stack(h, BRANCH) = r; 
						btree_increase(btree, h, btree_root);
					}
				}
				else
				if(0 < node->leaf[n - 1])
				{
					node->leaf[n - 1] -= node->leaf[n];
					node->leaf[n] = 0; btree_stack(0, BRANCH) = n - 1;
					btree_increase(btree, 0, btree_root);
				}
				else
				{
					node->leaf[n] *= -1; BTFF_ASSERT(0 < node->leaf[n]);
					btree_increase(btree, 0, btree_root);
				}
			}
		}
		else
		{
			if(0 < node->leaf[n - 1])
			{
				if(0 < node->leaf[n + 1])
				{
					node->leaf[n] -= node->leaf[n - 1];
					node->leaf[n] -= node->leaf[n + 1];
					node->leaf[n - 1] = node->leaf[n];
					leaf_remove(node, n, 2);
				}
				else
				{
					node->leaf[n] -= node->leaf[n - 1];
					node->leaf[n - 1] = node->leaf[n];
					leaf_remove(node, n, 1);
				}
				n--;
				btree_stack(0, BRANCH) = n;
			}
			else
			if(0 < node->leaf[n + 1])
			{
				node->leaf[n] -= node->leaf[n + 1];
				leaf_remove(node, n + 1, 1);
			}
			node->leaf[n] *= -1; BTFF_ASSERT(0 < node->leaf[n]);
			btree_increase(btree, 0, btree_root);
		}
		if(unlikely(0 == node->leaf[LEAF_HALF - 1]))
			btree_rebalance(btree, 0, btree_root);
		break;
	default:
		node = &btree->node[btree_stack(h, NODE)];
		n = btree_stack(h, BRANCH);
		node_next(btree, h);	
		right = &btree->node[btree_stack(0, NODE)];
		if(0 < right->leaf[0])
		{
			node->space[n] -= right->leaf[0];
			leaf_decrease(btree, INT16_MIN, h);
			leaf_remove(right, 0, 1);
			if(0 == right->leaf[LEAF_HALF - 1])
				btree_rebalance(btree, 0, h - 1);
		}
		btree_stack(h, BRANCH) = n;
		node_previous(btree, h);
		left = &btree->node[btree_stack(0, NODE)];
		l = leaf_nmemb(left) - 1;
		if(0 < left->leaf[l])
		{
			node->child[n] -= left->leaf[l];
			node->space[n] -= left->leaf[l];
			leaf_decrease(btree, 0, h);
			if(0 == left->leaf[LEAF_HALF - 1])
				btree_rebalance(btree, 0, h - 1);
		}
		node->space[n] *= -1;
		btree_stack(h, BRANCH) = n;
		btree_increase(btree, h, btree_root);

		if(unlikely(left->child[NODE_HALF - 1] == INT16_MAX || right->child[NODE_HALF - 1] == INT16_MAX))
			btree_rebalance(btree, h - 1, btree_root);
		break;
	}
	top_free(btree);
#ifndef __OPTIMIZE__
	btree_sanity(btree);
#endif
}

WEAK void* btree_page(struct thread* thread, size_t alignment, size_t size)
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

		if(PAGE_SIZE <= addr - thread->addr && addr - PAGE_SIZE + BTREE_LENGTH <= thread->addr + thread->length)
		{
			btree = (struct btree*)(addr - PAGE_SIZE);
			addr = (size_t)btree + BTREE_LENGTH;
			if(addr <= thread->addr + thread->length)
			{
				if(thread->addr < (size_t)btree)
				{
					length = (size_t)btree - thread->addr;
					BTFF_ASSERT(0 == munmap((void*)thread->addr, length));
				}
				length = addr - thread->addr;
				thread->addr += length;
				thread->length -= length;
				BTFF_ASSERT(0 == mprotect((void*)btree, PAGE_SIZE, PROT_READ|PROT_WRITE));
				btree_thread = thread;
				btree_length = BTREE_LENGTH;
				btree_left = btree_protect = btree_right;
				btree_key = BTREE_PAYLOAD_SIZE;
				btree_space = BTREE_PAYLOAD_SIZE / sizeof(uint64_t);
				thread->btree = btree;
				BTFF_ASSERT(0 == mprotect((void*)btree->node, PAGE_SIZE, PROT_READ|PROT_WRITE));
				btree_node_top++;
				btree_next = MAP_FAILED;
				if(sizeof(void*) >= alignment)
					return last_alloc(btree, (size + sizeof(void*) - 1) >> 3);
				else
				{
					addr = ((uint64_t)btree->barrier - size) & ~(alignment - 1);
					if(size < (length = (size_t)btree->barrier - addr))
					{
						if((ptr = last_alloc(btree, length >> 3)))
							btree_realloc(btree, (uint64_t*)ptr - btree->payload, (size + sizeof(void*) - 1) >> 3);
						return ptr;
					}
					else
					if(size == length)
						return last_alloc(btree, length >> 3);
					else
					{
						errno = ENOMEM;
						BTFF_PRINTF(stderr, "ENOMEM %lu %s %d\n", size, __FILE__, __LINE__);
						return NULL;
					}
				}
			}
		}
		if(0 < thread->length)
			BTFF_ASSERT(0 == munmap((void*)thread->addr, thread->length));
		if(PAGE_SIZE * 1024 > (length = BTREE_LENGTH + btree_alignment))
			length = PAGE_SIZE * 1024;
		if(MAP_FAILED == (ptr = mmap(NULL, length, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)))
		{
			thread->length = 0;
			errno = ENOMEM;
			return NULL;
		}
		else
		{
			thread->length = length;
			thread->addr = (size_t)ptr;
		}
	}
	return NULL;
}

/* return 0 in case of success, otherwise the old size */
WEAK size_t btree_shrink(struct btree* btree, int16_t disp, int16_t delta)
{
	size_t size = 0;
	int16_t h, n, tmp;
	node_t* node;
	node_t* left;

	if(btree_node_count < btree_root + 2) 
	{ 
		if((btree_node_free)) 
			btree_node_free = splay_last(btree_node_free); 
		do 
		{ 
			if((node = top_alloc(btree))) 
			{ 
				node->left[0] = btree_node_free; 
				node->right[0] = NULL; 
				btree_node_free = node; 
				btree_node_count++; 
			} 
			else 
			{ 
				errno = ENOMEM; 
				return (uint64_t*)btree->barrier - &btree->payload[disp];
			} 
		} while(btree_node_count < btree_root + 2); 
	} 

	switch((int)(h = btree_search(btree, disp)))
	{
	case -1:
		BTFF_PRINTF(stderr, "*** btff detected *** : double free or corruption : %p *** %s %d\n", btree->payload + disp, __FILE__, __LINE__);
#ifndef __OPTIMIZE__
		btree_sanity(btree);
#endif
		break;
	case 0: /* leaf */
		node = &btree->node[btree_stack(0, NODE)];
		n = btree_stack(0, BRANCH);
		if(unlikely(0 <= node->leaf[n]))
		{
			BTFF_PRINTF(stderr, "%p corruption %s %d\n", &btree->payload[disp], __FILE__, __LINE__);
			return btree->barrier - (uint8_t*)&btree->payload[disp];
		}
		delta += node->leaf[n];
		if(0 < delta) /* not allowed to increase the object */
		{
			size = -1 * node->leaf[n];
			size *= sizeof(void*);
		}
		else
		if(0 > delta) /* decrease the object */
		{
			delta *= -1;
			if(unlikely(0 == n))
			{
				if(unlikely(node->leaf[0] == btree_left))
				{
					btree_left += delta;
					leaf_decrease(btree, node->leaf[0] + delta, btree_root);
				}
				else
				{
					int16_t l;
					h = leaf_previous(btree);
					left = &btree->node[btree_stack(h, NODE)];
					l = btree_stack(h, BRANCH) - 1;
					if(0 < left->space[l])
					{
						node->leaf[n] += delta;
						left->space[l] += delta;
						btree_stack(h, BRANCH) = l;
						btree_increase(btree, h, btree_root);
					}
					else
					if(0 > left->space[l])
					{
						tmp = node->leaf[0] + delta;
						node->leaf[0] = delta;
						btree_increase(btree, 0, btree_root);
						if(likely(0 == node->leaf[LEAF_NMEMB - 1]))
							leaf_insert(node, 1, tmp);
						else
							btree_split(btree, 1, tmp, btree_root);
					}
					else
						BTFF_ASSERT((left->space[l]));	
				}
			}
			else
			if(0 < node->leaf[n - 1])
			{
				node->leaf[n] += delta;
				node->leaf[n - 1] += delta;
				btree_stack(0, BRANCH) = n - 1;
				btree_increase(btree, 0, btree_root);
			}
			else
			if(0 > node->leaf[n - 1])
			{
				tmp = node->leaf[n] + delta;
				node->leaf[n] = delta;
				btree_increase(btree, 0, btree_root);
				if(likely(0 == node->leaf[LEAF_NMEMB - 1]))
					leaf_insert(node, n + 1, tmp);
				else
					btree_split(btree, n + 1, tmp, btree_root);
			}
			else
				BTFF_ASSERT((node->leaf[n - 1]));	
		}
		break;
	default: /* node */
		node = &btree->node[btree_stack(h, NODE)];
		n = btree_stack(h, BRANCH);
		if(unlikely(0 <= node->space[n]))
		{
			BTFF_PRINTF(stderr, "%p corruption %s %d\n", &btree->payload[disp], __FILE__, __LINE__);
			return btree->barrier - (uint8_t*)&btree->payload[disp];
		}
		delta += node->space[n];
		if(0 < delta) /* not allowed to increase the object */
		{
			size = -1 * node->leaf[n];
			size *= sizeof(void*);
		}
		else
		if(0 > delta) /* decrease the object */
		{
			int16_t l;
			delta *= -1;
			node_previous(btree, h);
			left = &btree->node[btree_stack(0, NODE)];
			l = leaf_nmemb(left) - 1;
			if(0 < left->leaf[l])
			{
				node->space[n] += delta;
				node->child[n] += delta;
				left->leaf[l] += delta;
				btree_increase(btree, 0, btree_root);
			}
			else
			if(0 > left->leaf[l])
			{
				node->space[n] += delta;
				node->child[n] += delta;
				if(l < LEAF_NMEMB - 1)
				{
					left->leaf[l + 1] = delta;
					btree_stack(0, BRANCH) = l + 1;
					btree_increase(btree, 0, btree_root);
				}
				else
				{
					tmp = left->leaf[l];
					left->leaf[l] = delta;
					btree_increase(btree, 0, btree_root);
					btree_split(btree, l + 1, tmp, btree_root);
				}
			}
			else
				BTFF_ASSERT((left->leaf[l]));
		}
		break;
	}
#ifndef __OPTIMIZE__
	btree_sanity(btree);
#endif
	return size; /* 0 in case of success, otherwise the old size */
}

/* return 0 in case of success, otherwise the old size */
WEAK size_t btree_realloc(struct btree* btree, int16_t disp, int16_t delta)
{
	size_t size = 0;
	int16_t h, n, tmp;
	node_t* node;
	node_t* right;

	if(btree_node_count < btree_root + 2) 
	{ 
		if((btree_node_free)) 
			btree_node_free = splay_last(btree_node_free); 
		do 
		{ 
			if((node = top_alloc(btree))) 
			{ 
				node->left[0] = btree_node_free; 
				node->right[0] = NULL; 
				btree_node_free = node; 
				btree_node_count++; 
			} 
			else 
			{ 
				errno = ENOMEM; 
				return (uint64_t*)btree->barrier - &btree->payload[disp];
			} 
		} while(btree_node_count < btree_root + 2); 
	} 

	switch((int)(h = btree_search(btree, disp)))
	{
	case -1:
		BTFF_PRINTF(stderr, "*** btff detected *** : double free or corruption : %p *** %s %d\n", btree->payload + disp, __FILE__, __LINE__);
#ifndef __OPTIMIZE__
		btree_sanity(btree);
#endif
		break;
	case 0: /* leaf */
		node = &btree->node[btree_stack(0, NODE)];
		n = btree_stack(0, BRANCH);
		if(unlikely(0 <= node->leaf[n]))
		{
			BTFF_PRINTF(stderr, "%p corruption %s %d\n", &btree->payload[disp], __FILE__, __LINE__);
			return btree->barrier - (uint8_t*)&btree->payload[disp];
		}
		delta += node->leaf[n];
		if(0 < delta) /* increase the object */
		{
			if(unlikely(LEAF_NMEMB - 1 == n))
			{
				if(unlikely(disp - node->leaf[n] == btree_right))
				{
					size = -1 * node->leaf[n];
					size *= sizeof(void*);
				}
				else
				{
					int16_t r;
					h = leaf_next(btree);
					right = &btree->node[btree_stack(h, NODE)];
					r = btree_stack(h, BRANCH) + 1;
					if(delta < right->space[r])
					{
						right->child[r] += delta;
						btree_stack(h, BRANCH) = r;
						node_decrease(btree, h, right->space[r] - delta, btree_root);
						node->leaf[n] -= delta;
					}
					else
					if(delta > right->space[r])
					{
						size = -1 * node->leaf[n];
						size *= sizeof(void*);
					}
					else
					{
						right->child[r] += node->leaf[n];
						btree_stack(h, BRANCH) = r;
						node_decrease(btree, h, -1 * right->space[r] + node->leaf[n], btree_root);
						node->leaf[n] = 0;
					}
				}
			}
			else
			if(0 < node->leaf[n + 1])
			{
				if(delta < node->leaf[n + 1])
				{
					node->leaf[n] -= delta;
					btree_stack(0, BRANCH) = n + 1;
					leaf_decrease(btree, node->leaf[n + 1] - delta, btree_root);
				}
				else
				if(delta > node->leaf[n + 1])
				{
					size = -1 * node->leaf[n];
					size *= sizeof(void*);
				}
				else
				{
					node->leaf[n] -= delta;
					btree_stack(0, BRANCH) = n + 1;
					leaf_decrease(btree, INT16_MIN, btree_root);
					leaf_remove(node, n + 1, 1);
					if(unlikely(0 == node->leaf[LEAF_HALF - 1]))
						btree_rebalance(btree, 0, btree_root);
				}
			}
			else
			if(0 > node->leaf[n + 1])
			{
				size = -1 * node->leaf[n];
				size *= sizeof(void*);
			}
			else
			{
				if(unlikely(disp - node->leaf[n] == btree_right))
				{
					size = -1 * node->leaf[n];
					size *= sizeof(void*);
				}
				else
				{
					int16_t r;
					h = leaf_next(btree);
					right = &btree->node[btree_stack(h, NODE)];
					r = btree_stack(h, BRANCH) + 1;
					if(delta < right->space[r])
					{
						right->child[r] += delta;
						btree_stack(h, BRANCH) = r;
						node_decrease(btree, h, right->space[r] - delta, btree_root);
						node->leaf[n] -= delta; 
					}
					else
					if(delta > right->space[r])
					{
						size = -1 * node->leaf[n];
						size *= sizeof(void*);
					}
					else
					{
						right->child[r] += node->leaf[n]; 
						btree_stack(h, BRANCH) = r;
						node_decrease(btree, h, -1 * right->space[r] + node->leaf[n], btree_root);
						node->leaf[n] = 0;
						if(unlikely(0 == node->leaf[LEAF_HALF - 1]))
							btree_rebalance(btree, 0, btree_root);
					}
				}
			}
		}
		else
		if(0 > delta) /* decrease the object */
		{
			delta *= -1;
			if(unlikely(LEAF_NMEMB - 1 == n))
			{
				if(unlikely(disp - node->leaf[n] == btree_right))
				{
				SPLIT:
					tmp = node->leaf[n] + delta;
					node->leaf[n] = delta;
					btree_increase(btree, 0, btree_root);
					btree_split(btree, n, tmp, btree_root);
				}
				else
				{
					int16_t r;
					h = leaf_next(btree);
					right = &btree->node[btree_stack(h, NODE)];
					r = btree_stack(h, BRANCH) + 1;
					if(0 < right->space[r])
					{
						node->leaf[n] += delta;
						right->child[r] -= delta;
						right->space[r] += delta;
						btree_stack(h, BRANCH) = r;
						btree_increase(btree, h, btree_root);
					}
					else
					if(0 > right->space[r])
						goto SPLIT;
					else
						BTFF_ASSERT((right->space[r]));	
				}
			}
			else
			if(0 < node->leaf[n + 1])
			{
				node->leaf[n] += delta;
				node->leaf[n + 1] += delta;
				btree_stack(0, BRANCH) = n + 1;
				btree_increase(btree, 0, btree_root);
			}
			else
			if(0 > node->leaf[n + 1])
			{
				tmp = node->leaf[n] + delta;
				node->leaf[n] = delta;
				btree_increase(btree, 0, btree_root);
				if(likely(0 == node->leaf[LEAF_NMEMB - 1]))
					leaf_insert(node, n, tmp);
				else
					btree_split(btree, n, tmp, btree_root);
			}
			else
			{
				if(unlikely(disp - node->leaf[n] == btree_right))
				{
				APPEND:
					node->leaf[n] += delta;
					node->leaf[n + 1] = delta;
					btree_stack(0, BRANCH) = n + 1;
					btree_increase(btree, 0, btree_root);
				}
				else
				{
					int16_t r;
					h = leaf_next(btree);
					right = &btree->node[btree_stack(h, NODE)];
					r = btree_stack(h, BRANCH) + 1;
					if(0 < right->space[r])
					{
						right->child[r] -= delta;
						right->space[r] += delta;
						node->leaf[n] += delta; btree_stack(h, BRANCH) = r;
						btree_increase(btree, h, btree_root);
					}
					else
					if(0 > right->space[r])
						goto APPEND;
					else
						BTFF_ASSERT((node->space[r]));	
				}
			}
		}
		break;
	default: /* node */
		node = &btree->node[btree_stack(h, NODE)];
		n = btree_stack(h, BRANCH);
		if(unlikely(0 <= node->space[n]))
		{
			BTFF_PRINTF(stderr, "%p corruption %s %d\n", &btree->payload[disp], __FILE__, __LINE__);
			return btree->barrier - (uint8_t*)&btree->payload[disp];
		}
		delta += node->space[n];
		if(0 < delta) /* increase the object */
		{
			node_next(btree, h);
			right = &btree->node[btree_stack(0, NODE)];
			if(delta < right->leaf[0])
			{
				node->space[n] -= delta;
				leaf_decrease(btree, right->leaf[0] - delta, btree_root);
			}
			else
			if(delta > right->leaf[0])
			{
				size = -1 * node->space[n];
				size *= sizeof(void*);
			}
			else
			{
				node->space[n] -= delta;
				leaf_decrease(btree, INT16_MIN, btree_root);
				leaf_remove(right, 0, 1);
				if(unlikely(0 == right->leaf[LEAF_HALF - 1]))
					btree_rebalance(btree, 0, btree_root);
			}
		}
		else
		if(0 > delta) /* decrease the object */
		{
			delta *= -1;
			node_next(btree, h);
			right = &btree->node[btree_stack(0, NODE)];
			if(0 < right->leaf[0])
			{
				node->space[n] += delta;
				right->leaf[0] += delta;
				btree_increase(btree, 0, btree_root);
			}
			else
			if(0 > right->leaf[0])
			{
				node->space[n] += delta;
				if(0 == right->leaf[LEAF_NMEMB - 1])
				{
					leaf_insert(right, 0, delta);
					btree_increase(btree, 0, btree_root);
				}
				else
				{
					tmp = right->leaf[0];
					right->leaf[0] = delta;
					btree_increase(btree, 0, btree_root);
					btree_split(btree, 1, tmp, btree_root);
				}
			}
			else
				BTFF_ASSERT((right->leaf[0]));
		}
		break;
	}
#ifndef __OPTIMIZE__
	btree_sanity(btree);
#endif
	return size; /* 0 in case of success, otherwise the old size */
}
