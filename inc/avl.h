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
#ifndef __avl_h__
#define __avl_h__ __avl_h__

#include "heap.h"
#include <assert.h>

static inline void rotate_left(struct heap* node, struct heap* right)
{
	struct heap* tmp;
	int64_t size;

	if((tmp = node->edge[HEAP_PARENT]))
	{
		if(tmp->edge[HEAP_RIGHT] == node)
			tmp->edge[HEAP_RIGHT] = right;
		else
			tmp->edge[HEAP_LEFT] = right;
	}
	right->edge[HEAP_PARENT] = tmp;

	if((node->edge[HEAP_RIGHT] = (tmp = right->edge[HEAP_LEFT])))
		tmp->edge[HEAP_PARENT] = node;

	right->edge[HEAP_LEFT] = node;
	node->edge[HEAP_PARENT] = right;

	node->size[HEAP_RIGHT] = right->size[HEAP_LEFT];

	if(node->size[HEAP_LEFT] < node->size[HEAP_SIZE])
		size = node->size[HEAP_SIZE];
	else
		size = node->size[HEAP_LEFT];

	if(right->size[HEAP_LEFT] < size)
		right->size[HEAP_LEFT] = size;
}

static inline void rotate_right(struct heap* node, struct heap* left)
{
	struct heap* tmp;
	int64_t size;

	if((tmp = node->edge[HEAP_PARENT]))
	{
		if(tmp->edge[HEAP_LEFT] == node)
			tmp->edge[HEAP_LEFT] = left;
		else
			tmp->edge[HEAP_RIGHT] = left;
	}
	left->edge[HEAP_PARENT] = tmp;

	if((node->edge[HEAP_LEFT] = (tmp = left->edge[HEAP_RIGHT])))
		tmp->edge[HEAP_PARENT] = node;

	left->edge[HEAP_RIGHT] = node;
	node->edge[HEAP_PARENT] = left;

	node->size[HEAP_LEFT] = left->size[HEAP_RIGHT];

	if(node->size[HEAP_RIGHT] < node->size[HEAP_SIZE])
		size = node->size[HEAP_SIZE];
	else
		size = node->size[HEAP_RIGHT];

	if(left->size[HEAP_RIGHT] < size)
		left->size[HEAP_RIGHT] = size;
}

static inline void rebalance_left_right(struct heap* x, struct heap* y, struct heap* z)
{
	switch(y->balance)
	{
	case -1:
		z->balance = 0; x->balance = 1;
		break;
	case 0:
		z->balance = 0; x->balance = 0;
		break;
	case 1:
		z->balance = -1; x->balance = 0;
		break;
	default:
		assert(0);
		break;
	}
	y->balance = 0;
}

static inline void rebalance_right_left(struct heap* x, struct heap* y, struct heap* z)
{
	switch(y->balance)
	{
	case -1:
		x->balance = 0; z->balance = 1;
		break;
	case 0:
		x->balance = 0; z->balance = 0;
		break;
	case 1:
		x->balance = -1; z->balance = 0;
		break;
	default:
		assert(0);
		break;
	}
	y->balance = 0;
}

static inline struct heap* avl_insert(struct heap* root, struct heap* insert, int64_t size)
{
	struct heap* parent = NULL;
	struct heap** node = &root;
	struct heap* tmp;
	struct heap* child;
	while((*node))
	{
		parent = *node;
		if(insert < parent)
			node = parent->edge + HEAP_LEFT;
		else
		if(parent < insert)
			node = parent->edge + HEAP_RIGHT;
		else
			assert(parent != insert);
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
	for(tmp = insert, parent = tmp->edge[HEAP_PARENT]; parent; tmp = parent, parent = tmp->edge[HEAP_PARENT])
		if(parent->edge[HEAP_LEFT] == tmp)
		{
			parent->balance--;
			switch(parent->balance)
			{
			case 0:
			#ifndef __OPTIMIZE__
				assert((!parent->edge[HEAP_LEFT] && !parent->edge[HEAP_RIGHT]) 
					|| (parent->edge[HEAP_LEFT] && parent->edge[HEAP_RIGHT]));
			#endif
				return root;
			case -1:
				continue;
			case -2:
				switch(tmp->balance)
				{
				case -1:
					rotate_right(parent, tmp);
					parent->balance = tmp->balance = 0;
					if(root == parent)
						return tmp;
					break;
				case 1:
					child = tmp->edge[HEAP_RIGHT];
					rotate_left(tmp, child);
					rotate_right(parent, child);
					rebalance_left_right(parent, child, tmp);
					if(root == parent)
						return child;
					break;
				default:
					assert(0);
					break;
				}
				return root;
			default:
				assert(0);
				break;
			}
		}
		else
		{
			parent->balance++;
			switch(parent->balance)
			{
			case 0:
			#ifndef __OPTIMIZE__
				assert((!parent->edge[HEAP_LEFT] && !parent->edge[HEAP_RIGHT]) 
					|| (parent->edge[HEAP_LEFT] && parent->edge[HEAP_RIGHT]));
			#endif
				return root;
			case 1:
				continue;
			case 2: 
				switch(tmp->balance)
				{
				case 1:
					rotate_left(parent, tmp);
					parent->balance = tmp->balance = 0;
					if(root == parent)
						return tmp;
					break;
				case -1:
					child = tmp->edge[HEAP_LEFT];
					rotate_right(tmp, child);
					rotate_left(parent, child);
					rebalance_right_left(parent, child, tmp);
					if(root == parent)
						return child;
					break;
				default:
					assert(0);
					break;
				}
				return root;
			default:
				assert(0);
				break;
			}
		}
	return root;
}

static inline struct heap* avl_remove(struct heap* root, struct heap* remove)
{
	struct heap* parent;
	struct heap* tmp;
	struct heap* child;
	struct heap** node;

	if(remove->edge[HEAP_LEFT] && remove->edge[HEAP_RIGHT])
	{
		int64_t size;
		struct heap* next;
		for(next = remove->edge[HEAP_RIGHT]; next->edge[HEAP_LEFT]; next = next->edge[HEAP_LEFT]);
		heap_swap(remove, next);
		if(remove == root)
			root = next;
		size = next->size[HEAP_SIZE];
		if(remove->size[HEAP_SIZE] < size)
		{
			next->size[HEAP_SIZE] = remove->size[HEAP_SIZE];
			heap_increase(next, size);
		}
		else
		if(remove->size[HEAP_SIZE] > size)
		{
			next->size[HEAP_SIZE] = remove->size[HEAP_SIZE];
			heap_decrease(next, size);
		}
		remove->size[HEAP_SIZE] = size;
	}
	if((parent = remove->edge[HEAP_PARENT]))
	{
		if(remove == parent->edge[HEAP_LEFT])
			node = parent->edge + HEAP_LEFT;
		else
			node = parent->edge + HEAP_RIGHT;
	}
	else
		node = &root;
	if(remove->edge[HEAP_LEFT])
	{
		if(remove->size[HEAP_LEFT] < remove->size[HEAP_SIZE])
			heap_decrease(remove, remove->size[HEAP_LEFT]);
		(*node = remove->edge[HEAP_LEFT])->edge[HEAP_PARENT] = parent;
	}
	else
	if(remove->edge[HEAP_RIGHT])
	{
		if(remove->size[HEAP_RIGHT] < remove->size[HEAP_SIZE])
			heap_decrease(remove, remove->size[HEAP_RIGHT]);
		(*node = remove->edge[HEAP_RIGHT])->edge[HEAP_PARENT] = parent;
	}
	else
	{
		heap_decrease(remove, INT64_MIN);
		*node = NULL;
	}
	if(parent)
	{
		if((node == &parent->edge[HEAP_LEFT]))
			goto REMOVE_LEFT;
	REMOVE_RIGHT:
		parent->balance--;
		switch(parent->balance)
		{
		case 0:
		#ifndef __OPTIMIZE__
			assert((!parent->edge[HEAP_LEFT] && !parent->edge[HEAP_RIGHT]) 
				|| (parent->edge[HEAP_LEFT] && parent->edge[HEAP_RIGHT]));
		#endif
			break;
		case -1:
		#ifndef __OPTIMIZE__
			assert(parent->edge[HEAP_LEFT] || parent->edge[HEAP_RIGHT]);
		#endif
			return root;
		case -2:
			tmp = parent->edge[HEAP_LEFT];
		#ifndef __OPTIMIZE__
			assert((tmp));
		#endif
			switch(tmp->balance)
			{
			case -1:
				rotate_right(parent, tmp);
				parent->balance = tmp->balance = 0;
				if(root == parent)
					return tmp;
				else
					parent = tmp;
				break;
			case 0:
				rotate_right(parent, tmp);
				tmp->balance = 1;
				parent->balance = -1;
				if(root == parent)
					return tmp;
				else
					return root;
				break;
			case 1:
				child = tmp->edge[HEAP_RIGHT];
			#ifndef __OPTIMIZE__
				assert((child));
			#endif
				rotate_left(tmp, child);
				rotate_right(parent, child);
				rebalance_left_right(parent, child, tmp);
				if(root == parent)
					return child;
				else
					parent = child;
				break;
			default:
				assert(0);
				break;
			}
			break;
		default:
			assert(0);
			break;
		}
		goto CONTINUE;
	REMOVE_LEFT:
		parent->balance++;
		switch(parent->balance)
		{
		case 0:
		#ifndef __OPTIMIZE__
			assert((!parent->edge[HEAP_LEFT] && !parent->edge[HEAP_RIGHT]) 
				|| (parent->edge[HEAP_LEFT] && parent->edge[HEAP_RIGHT]));
		#endif
			break;
		case 1:
		#ifndef __OPTIMIZE__
			assert(parent->edge[HEAP_LEFT] || parent->edge[HEAP_RIGHT]);
		#endif
			return root;
		case 2:
			tmp = parent->edge[HEAP_RIGHT];
		#ifndef __OPTIMIZE__
			assert((tmp));
		#endif
			switch(tmp->balance)
			{
			case 1:
				rotate_left(parent, tmp);
				parent->balance = tmp->balance = 0;
				if(root == parent)
					return tmp;
				else
					parent = tmp;
				break;
			case 0:
				rotate_left(parent, tmp);
				tmp->balance = -1;
				parent->balance = 1;
				if(root == parent)
					return tmp;
				else
					return root;
				break;
			case -1:
				child = tmp->edge[HEAP_LEFT];
			#ifndef __OPTIMIZE__
				assert((child));
			#endif
				rotate_right(tmp, child);
				rotate_left(parent, child);
				rebalance_right_left(parent, child, tmp);
				if(root == parent)
					return child;
				else
					parent = child;
				break;
			default:
				assert(0);
				break;
			}
			break;
		default:
			assert(0);
			break;
		}
	CONTINUE:
		tmp = parent;
		if(parent = tmp->edge[HEAP_PARENT])
		{
			if(parent->edge[HEAP_LEFT] == tmp)
				goto REMOVE_LEFT;
			else
				goto REMOVE_RIGHT;
		}
	}
	return root;
}

static inline struct heap* avl_first_fit(struct heap* node, int64_t size)
{
	while((node))
		if(size <= node->size[HEAP_RIGHT])
			node = node->edge[HEAP_RIGHT];
		else
		if(size <= node->size[HEAP_SIZE])
			return node;
		else
		if(size <= node->size[HEAP_LEFT])
			node = node->edge[HEAP_LEFT];
		else
			break;
	return NULL;
}

#define heap_first_fit(node, size) avl_first_fit(node, size)
#define heap_insert(root, insert, size) avl_insert(root, insert, size)
#define heap_remove(root, remove) avl_remove(root, remove)

#endif/*__avl_h__*/
