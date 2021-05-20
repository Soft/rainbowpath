#ifndef INDEXER_H
#define INDEXER_H

#include <stddef.h>

void init_random(void);

typedef size_t (*indexer_t)(
  size_t palette_size,
  size_t ind,
  const char *start,
  const char *end
);

size_t index_sequential(size_t palette_size, size_t ind, const char *start, const char *end);
size_t index_hash(size_t palette_size, size_t ind, const char *start, const char *end);
size_t index_random(size_t palette_size, size_t ind, const char *start, const char *end);

indexer_t get_indexer(const char *name);

#endif
