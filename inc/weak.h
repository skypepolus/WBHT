#ifndef __weak_h__
#define __weak_h__ __weak_h__

WEAK void* weak_reallocf(void *ptr, size_t size)
{
	return NULL;
}

WEAK void *weak_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	return NULL;
}

WEAK void* weak_aligned_alloc(size_t alignment, size_t size)
{
	return NULL;
}

WEAK int weak_posix_memalign(void **memptr, size_t alignment, size_t size)
{
	return -1;
}

WEAK void* weak_valloc(size_t size)
{
	return NULL;
}

WEAK void* weak_memalign(size_t alignment, size_t size)
{
	return NULL;
}

WEAK void* weak_pvalloc(size_t alignment, size_t size)
{
	return NULL;
}

#endif/*__weak_h__*/
