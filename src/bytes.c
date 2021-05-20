#include "bytes.h"
#include "utils.h"

#include <stdlib.h>

enum {
  INITIAL_BYTES_SIZE = 32
};

struct bytes {
  char *data;
  size_t size;
  size_t cap;
};

struct bytes *bytes_create(void) {
  struct bytes *bytes = check(malloc(sizeof(*bytes)));
  char *data = check(malloc(INITIAL_BYTES_SIZE));
  bytes->size = 0;
  bytes->cap = INITIAL_BYTES_SIZE;
  bytes->data = data;
  return bytes;
}

void bytes_append_char(struct bytes *bytes, char c) {
  if (bytes->size >= bytes->cap) {
    size_t new_cap = bytes->cap * 2;
    bytes->data = check(realloc(bytes->data, new_cap));
    bytes->cap = new_cap;
  }
  *(bytes->data + bytes->size) = c;
  bytes->size++;
}

size_t bytes_size(const struct bytes *bytes) {
  return bytes->size;
}

const char *bytes_data(const struct bytes *bytes) {
  return bytes->data;
}

char *bytes_take(struct bytes *bytes) {
  char *ret = bytes->data;
  free(bytes);
  return ret;
}

void bytes_free(struct bytes *bytes) {
  free(bytes->data);
  free(bytes);
}
