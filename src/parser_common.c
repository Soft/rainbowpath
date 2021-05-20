#include "parser_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "utils.h"

void parse_error(const char *message) {
  fprintf(stderr, "%s\n", message);
}

static char *make_string(const char *begin, const char *end) {
  assert(end >= begin);
  size_t size = end - begin;
  char *result = check(calloc(size + 1, sizeof(char)));
  return memcpy(result, begin, size);
}

bool at_end(const char *pos, const char *end) {
  return pos >= end;
}

const char *skip_whitespace(const char *pos, const char *end) {
  while (!at_end(pos, end)) {
    switch (*pos) {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
      pos++;
      break;
    default:
      goto out;
    }
  }
 out:
  return pos;
}

static bool is_token_char(char c) {
  return ('0' <= c && c <= '9')
    || ('A' <= c && c <= 'Z')
    || ('a' <= c && c <= 'z')
    || c == '-';
}

const char *parse_char(const char *pos, const char *end, char c) {
  pos = skip_whitespace(pos, end);
  if (at_end(pos, end) || *pos != c) {
    return NULL;
  }
  return ++pos;
}

const char *parse_token(const char *pos, const char *end, char **token) {
  pos = skip_whitespace(pos, end);
  if (at_end(pos, end)) {
    return NULL;
  }
  const char *start = pos;
  while (!at_end(pos, end) && is_token_char(*pos)) {
    pos++;
  }
  if (start == pos) {
    return NULL;
  }
  *token = make_string(start, pos);
  return pos;
}

bool parse_ssize(const char *str, ssize_t *result) {
  long int num;
  char *num_end;
  if (!*str) {
    return false;
  }
  errno = 0;
  num = strtol(str, &num_end, 10);
  if (num == 0 && num_end == str) {
    return false;
  }
  if (errno == ERANGE) {
    return false;
  }
  if (*num_end != '\0') {
    return false;
  }
  *result = num;
  return true;
}

