#ifndef __list_h__
#define __list_h__ __list_h__

struct link
{
	struct link* previous;
	struct link* next;
};

struct list
{
	struct link head;
	struct link tail;
};

static inline void list_initial(register struct list* list)
{
	list->head.previous = NULL;
	list->head.next = &list->tail;
	list->tail.previous = &list->head;
	list->tail.next = NULL;
}

static inline void link_insert(register struct link* link, register struct link* next)
{
	register struct link* previous = next->previous;
	previous->next = link;
	link->previous = previous;
	next->previous = link;
	link->next = next;
}

static inline void link_add(register struct link* previous, register struct link* link)
{
	register struct link* next = previous->next;
	previous->next = link;
	link->previous = previous;
	next->previous = link;
	link->next = next;
}

static inline void link_remove(register struct link* link)
{
	register struct link* previous = link->previous;
	register struct link* next = link->next;
	previous->next = next;
	next->previous = previous;
}

#endif/*__list_h__*/
