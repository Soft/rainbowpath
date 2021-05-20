#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#define UNUSED __attribute__((unused))

void fatal(const char *message);

void *check(void *ptr);
char *check_asprintf(const char *fmt, ...);

char *get_working_directory(void);
const char *get_home_directory(void);

bool read_stream(FILE *stream, char **data, size_t *length);

const char *get_env(const char *var);

#endif
