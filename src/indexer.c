#include "build.h"

#include "indexer.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_GETRANDOM
#include <sys/random.h>
#endif

#include "utils.h"

void init_random(void) {
#ifdef HAVE_DRAND48
  #define INIT_FN srand48
  #define SEED_TYPE long int
#else
  #define INIT_FN srand
  #define SEED_TYPE unsigned int
#endif
#ifdef HAVE_GETRANDOM
  SEED_TYPE seed = 0;
  if (getrandom(&seed, sizeof(seed), 0) == sizeof(seed)) {
    INIT_FN(seed);
  } else {
    INIT_FN(time(NULL));
  }
#else
  INIT_FN(time(NULL));
#endif
#undef INIT_FN
#undef SEED_TYPE
}

size_t index_sequential(size_t palette_size, size_t ind, UNUSED const char *start, UNUSED const char *end) {
  return ind % palette_size;
}

size_t index_hash(size_t palette_size, UNUSED size_t ind, const char *start, const char *end) {
  size_t hash = 5381;
  for (const char *c = start; c < end; c++) {
    hash = ((hash << 5) + hash) + (size_t)*c;
  }
  return hash % palette_size;
}

size_t index_random(size_t palette_size, UNUSED size_t ind, UNUSED const char *start, UNUSED const char *end) {
  #ifdef HAVE_DRAND48
  return drand48() * palette_size;
  #else
  return rand() % palette_size;
  #endif
}

indexer_t get_indexer(const char *name) {
  if (!strcmp(name, "sequential")) {
    return index_sequential;
  } else if (!strcmp(name, "hash")) {
    return index_hash;
  } else if (!strcmp(name, "random")) {
    return index_random;
  }
  return NULL;
}
