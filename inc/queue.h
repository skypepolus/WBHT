#ifndef __queue_h__
#define __queue_h__ __queue_h__

#include <stdint.h>
#include <sys/user.h>

#define QUEUE_NMEMB (PAGE_SIZE / (uint64_t)sizeof(uint64_t))
#define QUEUE_TAIL (QUEUE_NMEMB - 1UL)
#define QUEUE_HEAD 0UL
#define QUEUE_MASK ((1UL << (6UL + 3UL)) - 1UL) 
#define QUEUE_LOW ((1UL << 3UL) - 1UL)
#define QUEUE_HIGH (QUEUE_MASK - QUEUE_LOW)
#define QUEUE_INDEX(index) (((uint64_t)(index) & QUEUE_LOW) << 6UL | ((uint64_t)(index) & QUEUE_HIGH) >> 3UL)

#endif/*__queue_h__*/
