#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

void fatal(const char *message) {
  fprintf(stderr, "%s\n", message);
  abort();
}

void *check(void *ptr) {
  if (!ptr) {
    fatal("Failed to allocate memory");
  }
  return ptr;
}

