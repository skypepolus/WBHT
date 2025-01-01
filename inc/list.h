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

static inline void list_initial(struct list* list)
{
	list->head.previous = NULL;
	list->head.next = &list->tail;
	list->tail.previous = &list->head;
	list->tail.next = NULL;
}

static inline void link_insert(struct link* link, struct link* next)
{
	struct link* previous = next->previous;
	previous->next = link;
	link->previous = previous;
	next->previous = link;
	link->next = next;
}

static inline void link_add(struct link* previous, struct link* link)
{
	struct link* next = previous->next;
	previous->next = link;
	link->previous = previous;
	next->previous = link;
	link->next = next;
}

static inline void link_remove(struct link* link)
{
	struct link* previous = link->previous;
	struct link* next = link->next;
	previous->next = next;
	next->previous = previous;
}

#endif/*__list_h__*/
