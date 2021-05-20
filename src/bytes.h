#ifndef BYTES_H
#define BYTES_H

#include <stddef.h>

struct bytes;

struct bytes *bytes_create(void);
size_t bytes_size(const struct bytes *bytes);
void bytes_append_char(struct bytes *bytes, char c);
const char *bytes_data(const struct bytes *bytes);
char *bytes_take(struct bytes *bytes);
void bytes_free(struct bytes *bytes);

#endif
