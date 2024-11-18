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
#ifndef __heap_h__
#define __heap_h__ __heap_h__

#include <stdint.h>
#include <stddef.h>

/* balanced heap tree */
struct heap
{
	int64_t size[3];
	struct heap* edge[3];
	int64_t balance;
};

enum { HEAP_SIZE, HEAP_PARENT = HEAP_SIZE, HEAP_LEFT, HEAP_RIGHT };

static inline struct heap* heap_first_fit(struct heap* node, int64_t size)
{
	while((node))
		if(size <= node->size[HEAP_SIZE])
			return node;
		else
		if(size <= node->size[HEAP_LEFT])
			node = node->edge[HEAP_LEFT];
		else
		if(size <= node->size[HEAP_RIGHT])
			node = node->edge[HEAP_RIGHT];
		else
			break;
	return NULL;
}

static inline void heap_increase(struct heap* node, int64_t increase)
{
	struct heap* parent;
	for(node->size[HEAP_SIZE] = increase, parent = node->edge[HEAP_PARENT]; 
		parent; 
		node = parent, parent = node->edge[HEAP_PARENT])
		if(parent->edge[HEAP_LEFT] == node)
		{
			if(parent->size[HEAP_LEFT] < increase)
				parent->size[HEAP_LEFT] = increase;
			else
				break;
		}
		else
		{
			if(parent->size[HEAP_RIGHT] < increase)
				parent->size[HEAP_RIGHT] = increase;
			else
				break;
		}
}

static inline struct heap* heap_insert(struct heap* root, struct heap* insert, int64_t size)
{
	struct heap* parent = NULL;
	struct heap** node = &root;
	while((*node))
	{
		parent = (*node);
		if(0 <= parent->balance)
		{
			parent->balance--;
			node = parent->edge + HEAP_LEFT;
		}
		else
		{
			parent->balance++;
			node = parent->edge + HEAP_RIGHT;
		}
	}
	*node = insert;
	insert->edge[HEAP_PARENT] = parent;
	insert->edge[HEAP_LEFT] = NULL;
	insert->edge[HEAP_RIGHT] = NULL;
	insert->size[HEAP_SIZE] = INT64_MIN;
	insert->size[HEAP_LEFT] = INT64_MIN;
	insert->size[HEAP_RIGHT] = INT64_MIN;
	insert->balance = 0;
	heap_increase(insert, size);
	return root;
}

static inline void heap_decrease(struct heap* node, int64_t decrease)
{
	struct heap* parent;
	int64_t size;
	for(size = node->size[HEAP_SIZE], node->size[HEAP_SIZE] = decrease, parent = node->edge[HEAP_PARENT]; 
		parent; 
		node = parent, parent = node->edge[HEAP_PARENT])
		if(parent->edge[HEAP_LEFT] == node)
		{
			if(parent->size[HEAP_LEFT] == size)
			{
				decrease = node->size[HEAP_SIZE];
				if(decrease < node->size[HEAP_LEFT])
					decrease = node->size[HEAP_LEFT];
				if(decrease < node->size[HEAP_RIGHT])
					decrease = node->size[HEAP_RIGHT];
				if(size == decrease)
					break;
				else
					parent->size[HEAP_LEFT] = decrease;
			}
			else
				break;
		}
		else
		{
			if(parent->size[HEAP_RIGHT] == size)
			{
				decrease = node->size[HEAP_SIZE];
				if(decrease < node->size[HEAP_LEFT])
					decrease = node->size[HEAP_LEFT];
				if(decrease < node->size[HEAP_RIGHT])
					decrease = node->size[HEAP_RIGHT];
				if(size == decrease)
					break;
				else
					parent->size[HEAP_RIGHT] = decrease;
			}
			else
				break;
		}
}

static inline struct heap* heap_leaf(struct heap* node)
{
	struct heap* parent = NULL;
	while((node))
	{
		parent = node;
		if(0 < parent->balance)
		{
			parent->balance--;
			node = parent->edge[HEAP_RIGHT];
		}
		else
		{
			parent->balance++;
			node = parent->edge[HEAP_LEFT];
		}
	}
	return parent;
}

static inline void heap_swap(struct heap* node, struct heap* leaf)
{
	struct heap* edge;
	int64_t size;
	int64_t balance;
	if(node->edge[HEAP_LEFT] == leaf)
	{
		leaf->edge[HEAP_PARENT] = node->edge[HEAP_PARENT];
		node->edge[HEAP_PARENT] = leaf;
		if((edge = leaf->edge[HEAP_PARENT]))
		{
			if(edge->edge[HEAP_LEFT] == node)
				edge->edge[HEAP_LEFT] = leaf;
			else
				edge->edge[HEAP_RIGHT] = leaf;
		}

		edge = leaf->edge[HEAP_LEFT];
		leaf->edge[HEAP_LEFT] = node;
		if((node->edge[HEAP_LEFT] = edge))
			edge->edge[HEAP_PARENT] = node;

		edge = node->edge[HEAP_RIGHT];
		node->edge[HEAP_RIGHT] = leaf->edge[HEAP_RIGHT];
		if((leaf->edge[HEAP_RIGHT] = edge))
			edge->edge[HEAP_PARENT] = leaf;
		if((edge = node->edge[HEAP_RIGHT]))
			edge->edge[HEAP_PARENT] = node;
	}
	else
	if(node->edge[HEAP_RIGHT] == leaf)
	{
		leaf->edge[HEAP_PARENT] = node->edge[HEAP_PARENT];
		node->edge[HEAP_PARENT] = leaf;
		if((edge = leaf->edge[HEAP_PARENT]))
		{
			if(edge->edge[HEAP_LEFT] == node)
				edge->edge[HEAP_LEFT] = leaf;
			else
				edge->edge[HEAP_RIGHT] = leaf;
		}

		edge = node->edge[HEAP_LEFT];
		node->edge[HEAP_LEFT] = leaf->edge[HEAP_LEFT];
		if((leaf->edge[HEAP_LEFT] = edge))
			edge->edge[HEAP_PARENT] = leaf;
		if((edge = node->edge[HEAP_LEFT]))
			edge->edge[HEAP_PARENT] = node;

		edge = leaf->edge[HEAP_RIGHT];
		leaf->edge[HEAP_RIGHT] = node;
		if((node->edge[HEAP_RIGHT] = edge))
			edge->edge[HEAP_PARENT] = node;
	}
	else
	{
		edge = node->edge[HEAP_PARENT];
		node->edge[HEAP_PARENT] = leaf->edge[HEAP_PARENT];
		if((leaf->edge[HEAP_PARENT] = edge))
		{
			if(edge->edge[HEAP_LEFT] == node)
				edge->edge[HEAP_LEFT] = leaf;
			else
				edge->edge[HEAP_RIGHT] = leaf;
		}
		if((edge = node->edge[HEAP_PARENT]))
		{
			if(edge->edge[HEAP_LEFT] == leaf)
				edge->edge[HEAP_LEFT] = node;
			else
				edge->edge[HEAP_RIGHT] = node;
		}

		edge = node->edge[HEAP_LEFT];
		node->edge[HEAP_LEFT] = leaf->edge[HEAP_LEFT];
		if((leaf->edge[HEAP_LEFT] = edge))
			edge->edge[HEAP_PARENT] = leaf;
		if((edge = node->edge[HEAP_LEFT]))
			edge->edge[HEAP_PARENT] = node;

		edge = node->edge[HEAP_RIGHT];
		node->edge[HEAP_RIGHT] = leaf->edge[HEAP_RIGHT];
		if((leaf->edge[HEAP_RIGHT] = edge))
			edge->edge[HEAP_PARENT] = leaf;
		if((edge = node->edge[HEAP_RIGHT]))
			edge->edge[HEAP_PARENT] = node;
	}
	size = node->size[HEAP_LEFT];
	node->size[HEAP_LEFT] = leaf->size[HEAP_LEFT];
	leaf->size[HEAP_LEFT] = size;

	size = node->size[HEAP_RIGHT];
	node->size[HEAP_RIGHT] = leaf->size[HEAP_RIGHT];
	leaf->size[HEAP_RIGHT] = size;

	balance = node->balance;
	node->balance = leaf->balance;
	leaf->balance = balance;
}

static inline struct heap* heap_remove(struct heap* root, struct heap* remove)
{
	struct heap* leaf = heap_leaf(root);
	struct heap* parent;
	if(leaf != remove)
	{
		int64_t size;
		heap_swap(remove, leaf);
		size = leaf->size[HEAP_SIZE];
		if(remove->size[HEAP_SIZE] < size)
		{
			leaf->size[HEAP_SIZE] = remove->size[HEAP_SIZE];
			heap_increase(leaf, size);
		}
		else
		if(remove->size[HEAP_SIZE] > size)
		{
			leaf->size[HEAP_SIZE] = remove->size[HEAP_SIZE];
			heap_decrease(leaf, size);
		}
		remove->size[HEAP_SIZE] = size;
	}
	heap_decrease(remove, INT64_MIN);
	if((parent = remove->edge[HEAP_PARENT]))
	{
		if(parent->edge[HEAP_LEFT] == remove)
			parent->edge[HEAP_LEFT] = NULL;
		else
			parent->edge[HEAP_RIGHT] = NULL;
		if(remove == root)
			return leaf;
		return root;
	}
	return NULL;
}

static inline struct heap* heap_move(struct heap* root, struct heap* dst, struct heap* src)
{
	struct heap* edge;
	if(root == src)
		root = dst;
	if(dst < src)
	{
		dst->size[HEAP_SIZE] = src->size[HEAP_SIZE];
		dst->size[HEAP_LEFT] = src->size[HEAP_LEFT];
		dst->size[HEAP_RIGHT] = src->size[HEAP_RIGHT];
		dst->edge[HEAP_SIZE] = src->edge[HEAP_SIZE];
		dst->edge[HEAP_LEFT] = src->edge[HEAP_LEFT];
		dst->edge[HEAP_RIGHT] = src->edge[HEAP_RIGHT];
		dst->balance = src->balance;
	}
	else
	if(dst > src)
	{
		dst->balance = src->balance;
		dst->edge[HEAP_RIGHT] = src->edge[HEAP_RIGHT];
		dst->edge[HEAP_LEFT] = src->edge[HEAP_LEFT];
		dst->edge[HEAP_SIZE] = src->edge[HEAP_SIZE];
		dst->size[HEAP_RIGHT] = src->size[HEAP_RIGHT];
		dst->size[HEAP_LEFT] = src->size[HEAP_LEFT];
		dst->size[HEAP_SIZE] = src->size[HEAP_SIZE];
	}
	if(edge = dst->edge[HEAP_PARENT])
	{
		if(edge->edge[HEAP_LEFT] == src)
			edge->edge[HEAP_LEFT] = dst;
		else
			edge->edge[HEAP_RIGHT] = dst;
	}
	if(edge = dst->edge[HEAP_LEFT])
		edge->edge[HEAP_PARENT] = dst;
	if(edge = dst->edge[HEAP_RIGHT])
		edge->edge[HEAP_PARENT] = dst;
	return root;
}

#endif/* __heap_h__*/
