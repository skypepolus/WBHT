#ifndef __btree_h__
#define __btree_h__ __btree_h__

#include "btff.h"
#include "thread.h"
#include "heap.h"
#include <sys/user.h>
#include <string.h>
#include <stdint.h>

#define LEAF_NMEMB 32
#define LEAF_HALF (LEAF_NMEMB / 2)
#define NODE_SPLIT (LEAF_HALF - 1)
#define NODE_HALF 7

typedef struct node
{
	int16_t leaf[0];
	struct node* left[0];
	int16_t child[LEAF_HALF];
	struct node* right[0];
	int16_t space[LEAF_HALF]; 
} node_t;

typedef union
{
	uint64_t uint64[1];
	int64_t int64[1];
	uint32_t uint32[2];
	int32_t int32[2];
	uint16_t uint16[4];
	int16_t int16[4];
	uint8_t uint8[8];
	int8_t int8[8];
} header_t;

enum { MAX, NODE, BRANCH };

enum
{
	HEADER_LENGTH,
	HEADER_LEFT,
	HEADER_PROTECT,
	HEADER_SPACE, 
	HEADER_KEY,
	HEADER_COUNT,
	HEADER_ROOT,
	HEADER_STACK,
	HEADER_TOP = HEADER_STACK + 7,
	HEADER_LOCAL,
	HEADER_NEXT = HEADER_LOCAL + 7,
	HEADER_REMOTE = HEADER_NEXT + 7,
	HEADER = HEADER_REMOTE + 7
};

#define BTREE_ALIGNMENT ((1UL << (16UL - 1UL)) * sizeof(void*))
#define BTREE_BARRIER_SIZE PAGE_SIZE
#define BTREE_PAYLOAD_SIZE (BTREE_ALIGNMENT - sizeof(struct thread*) - HEADER * sizeof(header_t)  - sizeof(node_t*) - sizeof(struct heap) - LEAF_NMEMB * sizeof(uint16_t))
#define BTREE_NODE_SIZE (502UL * BTREE_PAYLOAD_SIZE / 800UL + PAGE_SIZE - 1UL & PAGE_MASK)
#define BTREE_LENGTH (BTREE_ALIGNMENT + BTREE_BARRIER_SIZE + BTREE_NODE_SIZE)
#define BTREE_LIMIT (BTREE_PAYLOAD_SIZE & PAGE_MASK)

struct thread;

struct btree
{
	struct thread* thread;
	header_t header[HEADER];
	node_t* free;
	struct heap heap;
	uint16_t leaf[LEAF_NMEMB];
	uint64_t payload[BTREE_PAYLOAD_SIZE / sizeof(uint64_t)];
	uint8_t barrier[BTREE_BARRIER_SIZE];
	node_t node[BTREE_NODE_SIZE / sizeof(node_t)];
};

#define HEADER_STACK_HEADER_COUNT (sizeof(((struct btree*)NULL)->header.stack) / sizeof(*((struct btree*)NULL)->header.stack))

#define BTREE_LOW ((uint64_t)(BTREE_ALIGNMENT - 1UL))
#define BTREE_HIGH (UINT64_MAX - BTREE_LOW)
#define BTREE(ptr) ((struct btree*)(BTREE_LIMIT <= (BTREE_LOW & (uint64_t)(ptr)) ? PAGE_MASK & (uint64_t)(ptr) : (BTREE_HIGH & (uint64_t)(ptr)) - PAGE_SIZE))

#define btree_length btree->header[HEADER_LENGTH].uint64[0]
#define btree_thread btree->thread
#define btree_node_top btree->header[HEADER_TOP].int16[0]
#define btree_node_free btree->free
#define btree_node_count btree->header[HEADER_COUNT].int16[0]
#define btree_root btree->header[HEADER_ROOT].int16[0]
#define btree_left btree->header[HEADER_LEFT].int16[0]
#define btree_protect btree->header[HEADER_PROTECT].int16[0]
#define btree_key btree->header[HEADER_KEY].int64[0]
#define btree_space btree->header[HEADER_SPACE].int16[0]
#define btree_stack(height, name) btree->header[HEADER_STACK + height].int16[name] 
#define btree_right (int16_t)(sizeof(btree->payload) / sizeof(*btree->payload))
#define btree_local ((void***)btree->header[HEADER_LOCAL].int64)[0]
#define btree_next ((struct btree**)btree->header[HEADER_NEXT].int64)[0]
#define btree_remote ((void***)btree->header[HEADER_REMOTE].int64)[0]

WEAK void btree_free(struct btree* btree, int16_t disp) PRIVATE(btree_free);
WEAK size_t btree_realloc(struct btree* btree, int16_t disp, int16_t delta) PRIVATE(btree_realloc);
WEAK size_t btree_shrink(struct btree* btree, int16_t disp, int16_t delta) PRIVATE(btree_shrink);
WEAK void* btree_page(struct thread* thread, size_t alignment, size_t size) PRIVATE(btree_page);
WEAK void* first_fit_alloc(struct btree* btree, int16_t dela) PRIVATE(first_fit_alloc);
WEAK struct thread* last_free(struct btree* btree) PRIVATE(last_free);

#endif/*__btree_h__*/
