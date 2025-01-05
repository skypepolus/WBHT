#ifndef __splay_h__
#define __splay_h__ __splay_h__

#include "btree.h"

#if 0
An optimized implementation of top-down splay tree algorithm.[1]

Reference
1. DANIEL DOMINIC SLEATOR A N D ROBERT ASSEMBLERE TARJAN (July 1985) "Self-Adjusting Binary Search Trees". Journal of the Association for Computing Machinery. Vol. 32, No. 3, July 1985, pp. 652-686.
#endif

static inline struct node* splay_first(register struct node* root)
{
	struct node null;
	register struct node* left = &null;
	register struct node* right = &null;
	register struct node* child;
	null.left[0] = NULL;
	null.right[0] = NULL;
ZIG:
	if(!(child = root->left[0]))
		goto ASSEMBLE;
	root->left[0] = child->right[0];
	child->right[0] = root;
	root = child;
	if(!(child = root->left[0]))
		goto ASSEMBLE;
	right->left[0] = root;
	right = root;
	root = child;
	goto ZIG;
ASSEMBLE:
	left->right[0] = root->left[0];
	right->left[0] = root->right[0];
	root->left[0] = null.right[0];
	root->right[0] = null.left[0];
	return root;
}

static inline struct node* splay_last(register struct node* root)
{
	struct node null;
	register struct node* left = &null;
	register struct node* right = &null;
	register struct node* child;
	null.left[0] = NULL;
	null.right[0] = NULL;
ZAG:
	if(!(child = root->right[0]))
		goto ASSEMBLE;
	root->right[0] = child->left[0];
	child->left[0] = root;
	root = child;
	if(!(child = root->right[0]))
		goto ASSEMBLE;
	left->right[0] = root;
	left = root;
	root = child;
	goto ZAG;
ASSEMBLE:
	left->right[0] = root->left[0];
	right->left[0] = root->right[0];
	root->left[0] = null.right[0];
	root->right[0] = null.left[0];
	return root;
}

static inline struct node* splay(register struct node* root, register struct node* key)
{
	struct node null;
	register struct node* left = &null;
	register struct node* right = &null;
	register struct node* child;
	null.left[0] = NULL;
	null.right[0] = NULL;
COMPARE:
	if(key < root)
	{
	ZIG:
		if(!(child = root->left[0]))
			goto ASSEMBLE;
		if(key < child)
		{
			root->left[0] = child->right[0];
			child->right[0] = root;
			root = child;
			if(!(child = root->left[0]))
				goto ASSEMBLE;
			right->left[0] = root;
			right = root;
			root = child;
			goto COMPARE;
		}
		else
		if(key > child)
		{
			right->left[0] = root;
			right = root;
			root = child;
			goto ZAG;
		}
		else
		{
			right->left[0] = root;
			right = root;
			root = child;
		}
	}
	else
	if(key > root)
	{
	ZAG:
		if(!(child = root->right[0]))
			goto ASSEMBLE;
		if(key > child)
		{
			root->right[0] = child->left[0];
			child->left[0] = root;
			root = child;
			if(!(child = root->right[0]))
				goto ASSEMBLE;
			left->right[0] = root;
			left = root;
			root = child;
			goto COMPARE;
		}
		else
		if(key < child)
		{
			left->right[0] = root;
			left = root;
			root = child;
			goto ZIG;
		}
		else
		{
			left->right[0] = root;
			left = root;
			root = child;
		}
	}
ASSEMBLE:
	left->right[0] = root->left[0];
	right->left[0] = root->right[0];
	root->left[0] = null.right[0];
	root->right[0] = null.left[0];
	return root;
}

static inline struct node* splay_insert(register struct node* root, register struct node* key)
{
	if((root))
	{
		root = splay(root, key);
		if(key < root)
		{	
			key->left[0] = root->left[0];
			root->left[0] = NULL;
			key->right[0] = root;
			root = key;
		}
		else
		if(root < key)
		{
			key->right[0] = root->right[0];
			root->right[0] = NULL;
			key->left[0] = root;
			root = key;
		}
	}
	else
	{
		key->left[0] = NULL;
		key->right[0] = NULL;
		root = key;
	}
	return root;
}

#endif/*__splay_h__*/
