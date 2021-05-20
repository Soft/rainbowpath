#include "config_parser.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bytes.h"
#include "utils.h"
#include "parser_common.h"

struct option {
  char *name;
  bool has_index;
  ssize_t index;
  enum option_kind kind;
  union {
    char *string_value;
    bool bool_value;
  };
};

struct option *option_create_bool(char *name, bool b) {
  struct option *option = check(malloc(sizeof(*option)));
  option->name = name;
  option->has_index = false;
  option->kind = OPTION_KIND_BOOL;
  option->bool_value = b;
  return option;
}

struct option *option_create_string(char *name, char *string) {
  struct option *option = check(malloc(sizeof(*option)));
  option->name = name;
  option->has_index = false;
  option->kind = OPTION_KIND_STRING;
  option->string_value = string;
  return option;
}

const char *option_name(const struct option *option) {
  return option->name;
}

void option_set_index(struct option *option, ssize_t index) {
  option->has_index = true;
  option->index = index;
}

void option_unset_index(struct option *option) {
  option->has_index = false;
}

bool option_has_index(const struct option *option) {
  return option->has_index;
}

ssize_t option_index(const struct option *option) {
  assert(option->has_index);
  return option->index;
}

enum option_kind option_kind(const struct option *option) {
  return option->kind;
}

bool option_bool_value(const struct option *option) {
  assert(option->kind == OPTION_KIND_BOOL);
  return option->bool_value;
}

const char *option_string_value(const struct option *option) {
  assert(option->kind == OPTION_KIND_STRING);
  return option->string_value;
}

char *option_take_string_value(struct option *option) {
  assert(option->kind == OPTION_KIND_STRING);
  char *ret = option->string_value;
  option->string_value = NULL;
  return ret;
}

void option_free(struct option *option) {
  free(option->name);
  if (option->kind == OPTION_KIND_STRING && option->string_value) {
    free(option->string_value);
  }
  free(option);
}

// Config Parser

static const char *skip_line(const char *pos, const char *end) {
  while (!at_end(pos, end) && *pos != '\n') {
    pos++;
  }
  if (!at_end(pos, end)) {
    pos++;
  }
  return pos;
}

static bool map_escape(char c, char *ret) {
  switch (c) {
  case '\\':
  case '"':
    *ret = c;
    break;
  case 'n':
    *ret = '\n';
    break;
  case 't':
    *ret = '\t';
    break;
  case 'r':
    *ret = '\r';
    break;
  case 'f':
    *ret = '\f';
    break;
  case 'v':
    *ret = '\v';
    break;
  default:
    return false;
  }
  return true;
}

static const char *parse_string(const char *pos, const char *end, char **str) {
  pos = parse_char(pos, end, '"');
  if (!pos) {
    parse_error("Expected string value");
    return NULL;
  }
  struct bytes *str_ = bytes_create();
  while (true) {
    if (at_end(pos, end)) {
      parse_error("Unterminated string");
      goto error;
    }
    switch (*pos) {
    case '\\':
      pos++;
      if (at_end(pos, end)) {
        parse_error("Unterminated escape sequence");
        goto error;
      }
      char escaped;
      if (!map_escape(*pos, &escaped)) {
        parse_error("Invalid escape sequence");
        goto error;
      }
      bytes_append_char(str_, escaped);
      break;
    case '\0':
      parse_error("Unexpected null byte");
      goto error;
    case '"':
      pos++;
      goto out;
    default:
      bytes_append_char(str_, *pos);
      break;
    }
    pos++;
  }
 out:
  bytes_append_char(str_, '\0');
  *str = bytes_take(str_);
  return pos;
 error:
  bytes_free(str_);
  return NULL;
}

static const char *parse_bool(const char *pos, const char *end, bool *b) {
  char *token;
  pos = parse_token(pos, end, &token);
  if (!pos) {
    return NULL;
  }
  if (!strcmp(token, "true")) {
    *b = true;
  } else if (!strcmp(token, "false")) {
    *b = false;
  } else {
    parse_error("Expected boolean value");
    pos = NULL;
  }
  free(token);
  return pos;
}

static const char *parse_index(const char *pos, const char *end, ssize_t *i) {
  pos = parse_char(pos, end, '[');
  if (!pos) {
    parse_error("Expected '['");
    return NULL;
  }
  char *token;
  pos = parse_token(pos, end, &token);
  if (!pos) {
    parse_error("Expected index");
    return NULL;
  }
  ssize_t i_;
  if (!parse_ssize(token, &i_)) {
    parse_error("Invalid index");
    pos = NULL;
    goto out;
  }
  pos = parse_char(pos, end, ']');
  if (!pos) {
    parse_error("Expected ']'");
    goto out;
  }
  *i = i_;
 out:
  free(token);
  return pos;
}

static const char *parse_option_assignment(const char *pos, const char *end, struct option **option) {
  const char *endl = skip_line(pos, end);
  char *name = NULL;
  struct option *option_ = NULL;
  pos = parse_token(pos, endl, &name);
  if (!pos) {
    parse_error("Expected option");
    goto error;
  }
  bool has_index = false;
  ssize_t index;
  if (parse_char(pos, endl, '[')) {
    pos = parse_index(pos, endl, &index);
    if (!pos) {
      goto error;
    }
    has_index = true;
  }
  pos = parse_char(pos, endl, '=');
  if (!pos) {
    parse_error("Expected '='");
    goto error;
  }
  if (parse_char(pos, endl, '"')) {
    char *str;
    pos = parse_string(pos, endl, &str);
    if (!pos) {
      goto error;
    }
    option_ = option_create_string(name, str);
  } else if (parse_char(pos, endl, 't') || parse_char(pos, endl, 'f')) {
    bool b;
    pos = parse_bool(pos, endl, &b);
    if (!pos) {
      goto error;
    }
    option_ = option_create_bool(name, b);
  } else {
    parse_error("Invalid value");
    goto error;
  }
  name = NULL;
  pos = skip_whitespace(pos, endl);
  if (!at_end(pos, endl)) {
    parse_error("Expected end of line");
    goto error;
  }
  if (has_index) {
    option_set_index(option_, index);
  }
  *option = option_;
  return pos;
 error:
  if (name) {
    free(name);
  }
  if (option_) {
    option_free(option_);
  }
  return NULL;
}

const char *parse_config(const char *pos, const char *end, struct list **options) {
  struct list *options_ = list_create();
  while (!at_end(pos, end)) {
    pos = skip_whitespace(pos, end);
    if (at_end(pos, end)) {
      break;
    }
    if (parse_char(pos, end, '#')) {
      pos = skip_line(pos, end);
      continue;
    }
    struct option *option;
    pos = parse_option_assignment(pos, end, &option);
    if (!pos) {
      goto error;
    }
    list_append(options_, option);
  }
  *options = options_;
  return pos;
 error:
  list_free(options_, (elem_free_t)option_free);
  return NULL;
}
