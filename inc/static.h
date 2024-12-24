#ifndef __static_h__
#define __static_h__ __static_h__

static struct
{
	uint8_t barrier[sizeof(void*) * 15];
	struct thread* thread;
}* channel;

static unsigned nprocs = 0;
static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static pthread_key_t key;
static size_t thread_length;
static uint8_t barrier[sizeof(void*) * 15];

static void destructor(register void* data);

static void init_routine(void)
{
	int fd;
	char* buf;
	size_t count;
	const char* str;
	assert(0 <= (fd = open("/proc/cpuinfo", O_RDONLY)));
	buf = sbrk(0);
	count = (size_t)buf;
	count += PAGE_SIZE - 1;
	count &= PAGE_MASK;
	count -= (size_t)buf;
	assert(0 == brk(buf + count));
	while(count == read(fd, buf, count))
	{
		count += PAGE_SIZE;
		assert(0 == brk(buf + count));
	}
	assert((str = strstr(buf, "cpu cores\t:")));
	str += sizeof("cpu cores\t:");
	nprocs = atoi(str);
	close(fd);
#ifdef AVX512F
	assert((strstr(buf, "avx512f")));
#endif
	channel = (void*)buf;
	assert(0 == brk((void*)(channel + nprocs)));
	for(count = 0; count < nprocs; count++)
		channel[count].thread = NULL;
	assert(0 == pthread_key_create(&key, destructor));
#ifdef __btff_h__
	thread_length = sizeof(struct thread);
#else
	thread_length = offsetof(struct thread, channel) + sizeof(*((struct thread*)NULL)->channel) * nprocs;
#endif
	thread_length += PAGE_SIZE - 1;
	thread_length &= PAGE_MASK;
}

static inline struct thread* thread_initial(struct thread** local)
{
	int r;
	struct thread* thread;

	if(0 == nprocs)
		assert(0 == pthread_once(&once_control, init_routine));
	do
	{
		if(thread = (struct thread*)channel->thread)
		{
			__atomic_thread_fence(__ATOMIC_ACQUIRE);
			if(__sync_bool_compare_and_swap(&channel->thread, thread, thread->next))
			{
				*local = thread;
				if(NULL == pthread_getspecific(key))
					pthread_setspecific(key, (const void*)local);
				return thread;
			}
		}
	} while((thread) && 0 == sched_yield());

	assert(MAP_FAILED != (void*)(thread = (struct thread*)mmap(NULL, thread_length, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)));
#ifndef __btff_h__
	for(r = 0; r < sizeof(thread->list) / sizeof(*thread->list); r++)
		list_initial(thread->list + r);
	thread->remote = MAP_FAILED;
#endif
	*local = thread;
	pthread_setspecific(key, (const void*)local);
	return thread;
}

static _Thread_local struct thread* local = NULL;

static void local_free(struct thread* thread, int64_t* front, int64_t* back);

#endif/*__static_h__*/
