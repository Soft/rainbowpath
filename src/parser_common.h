#ifndef PARSER_COMMON_H
#define PARSER_COMMON_H

#include <stdbool.h>
#include <sys/types.h>

void parse_error(const char *message);
bool at_end(const char *pos, const char *end);
const char *skip_whitespace(const char *pos, const char *end);
const char *parse_char(const char *pos, const char *end, char c);
const char *parse_token(const char *pos, const char *end, char **token);
bool parse_ssize(const char *str, ssize_t *result);

#endif
