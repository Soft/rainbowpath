#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdbool.h>
#include <sys/types.h>

#include "list.h"

enum option_kind {
  OPTION_KIND_BOOL,
  OPTION_KIND_STRING
};

struct option;

struct option *option_create_bool(char *name, bool b);
struct option *option_create_string(char *name, char *string);
const char *option_name(const struct option *option);
void option_set_index(struct option *option, ssize_t index);
void option_unset_index(struct option *option);
bool option_has_index(const struct option *option);
ssize_t option_index(const struct option *option);
enum option_kind option_kind(const struct option *option);
bool option_bool_value(const struct option *option);
const char *option_string_value(const struct option *option);
char *option_take_string_value(struct option *option);
void option_free(struct option *option);

const char *parse_config(const char *pos, const char *end, struct list **options);

#endif
