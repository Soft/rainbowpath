#include <stdlib.h>

#include "list.h"
#include "utils.h"

struct list {
  struct list_elem *head;
  struct list_elem *tail;
};

struct list_elem {
  void *value;
  struct list_elem *next;
};

struct list *list_create(void) {
  struct list *list = check(malloc(sizeof(*list)));
  list->head =  NULL;
  list->tail = NULL;
  return list;
}

void list_append(struct list *list, void *value) {
  struct list_elem *elem = check(malloc(sizeof(*elem)));
  elem->value = value;
  elem->next = NULL;
  if (list->tail) {
    list->tail->next = elem;
    list->tail = elem;
  } else {
    list->head = elem;
    list->tail = elem;
  }
}

struct list_elem *list_first(const struct list *list) {
  return list->head;
}

struct list_elem *list_elem_next(const struct list_elem *elem) {
  return elem->next;
}

void *list_elem_value(const struct list_elem *elem) {
  return elem->value;
}

void *list_pop(struct list *list) {
  struct list_elem *first = list_first(list);
  if (first) {
    struct list_elem *second = list_elem_next(first);
    if (second) {
      list->head = second;
    } else {
      list->head = NULL;
      list->tail = NULL;
    }
    void *value = first->value;
    free(first);
    return value;
  }
  return NULL;
}

void list_free(struct list *list, void (*elem_free)(void *)) {
  struct list_elem *elem;
  while ((elem = list_pop(list))) {
    elem_free(elem);
  }
  free(list);
}
