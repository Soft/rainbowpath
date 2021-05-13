#ifndef UTILS_H
#define UTILS_H

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define UNUSED __attribute__((unused))

void fatal(const char *message);
void *check(void *ptr);

#endif
