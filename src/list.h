#ifndef LIST_H
#define LIST_H

struct list;
struct list_elem;
typedef void (*elem_free_t)(void *);

struct list *list_create(void);
void list_append(struct list *list, void *value);
struct list_elem *list_first(const struct list *list);
struct list_elem *list_elem_next(const struct list_elem *elem);
void *list_elem_value(const struct list_elem *elem);
void *list_pop(struct list *list);
void list_free(struct list *list, elem_free_t elem_free);

#endif
