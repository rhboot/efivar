// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * list.h - simple list primitives
 */

#ifndef LIST_H_
#define LIST_H_

#include <stdlib.h>

#define container_of(ptr, type, member)                      \
	({                                                   \
		void *__mptr = (void *)(ptr);                \
		((type *)(__mptr - offsetof(type, member))); \
	})

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

typedef struct list_head list_t;

#define LIST_HEAD_INIT(name)                     \
	{                                        \
		.next = &(name), .prev = &(name) \
	}

#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr)          \
	({                           \
		(ptr)->next = (ptr); \
		(ptr)->prev = (ptr); \
	})

static inline int __attribute__((__unused__))
list_empty(const struct list_head *head)
{
	return head->next == head;
}

static inline void
__list_add(struct list_head *new, struct list_head *prev,
           struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void
list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

static inline void __attribute__((__unused__))
list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void
__list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void
__list_del_entry(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

static inline void __attribute__((__unused__))
list_del(struct list_head *entry)
{
	__list_del_entry(entry);
	entry->next = NULL;
	entry->prev = NULL;
}

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

#define list_last_entry(ptr, type, member) list_entry((ptr)->prev, type, member)

#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head)                       \
	for (pos = (head)->next, n = pos->next; pos != (head); \
	     pos = n, n = pos->next)

#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_prev_safe(pos, n, head)                  \
	for (pos = (head)->prev, n = pos->prev; pos != (head); \
	     pos = n, n = pos->prev)

static inline size_t
list_size(struct list_head *entry)
{
	list_t *pos;
	size_t i = 0;
	list_for_each(pos, entry) {
		i++;
	}
	return i;
}

/*
 * Sort a list with cmp()
 * creates a temporary array on the heap
 */
static inline int __attribute__((__unused__))
list_sort(struct list_head *head,
	  int (*cmp)(const void *a, const void *b, void *state),
	  void *state)
{
	struct list_head **array = NULL, *pos;
	size_t nmemb, i = 0;

	nmemb = list_size(head);

	array = calloc(nmemb, sizeof (head));
	if (!array)
		return -1;

	list_for_each(pos, head) {
		/*
		 * clang-analyzer can't quite figure out that this iterator
		 * is limited by the same expression as list_size() is
		 * using, so it complains that this is eventually accessing
		 * uninitialized memory.  This check below accomplishes
		 * nothing, it's effectively i < nmemb, but done with
		 * pointers.
		 */
		char *ptr = (char *)&array[i];
		char *end = (char *)&array[nmemb];
		if (ptr >= end)
			break;
		array[i++] = pos;
	}

	qsort_r(array, nmemb, sizeof(*array), cmp, state);

	INIT_LIST_HEAD(head);
	for (i = 0; i < nmemb; i++) {
		INIT_LIST_HEAD(array[i]);
		list_add(array[i], head);
		head = head->next;
	}
	free(array);

	return 0;
}


#endif /* !LIST_H_ */
// vim:fenc=utf-8:tw=75:noet
