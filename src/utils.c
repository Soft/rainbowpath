#include "build.h"

#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <linux/limits.h>


void fatal(const char *message) {
  fprintf(stderr, "%s\n", message);
  abort();
}

void *check(void *ptr) {
  if (!ptr) {
    fatal("Failed to allocate memory");
  }
  return ptr;
}

char *check_asprintf(const char *fmt, ...) {
  char *str;
  va_list args;
  va_start(args, fmt);
  if (vasprintf(&str, fmt, args) == -1) {
    va_end(args);
    fatal("Format string failed");
  }
  va_end(args);
  return str;
}

bool read_stream(FILE *stream, char **data, size_t *length) {
  size_t capacity = getpagesize();
  size_t available = capacity;
  size_t offset = 0;
  char *buffer = check(malloc(capacity));
  while (true) {
    size_t read = fread(buffer + offset, 1, available, stream);
    offset += read;
    if (read != available) {
      break;
    }
    capacity *= 2;
    char *buf = check(realloc(buffer, capacity));
    if (!buf) {
      free(buffer);
      return false;
    }
    buffer = buf;
    available = capacity - offset;
  }
  if (ferror(stream)) {
    free(buffer);
    return false;
  }
  *data = buffer;
  *length = offset;
  return true;
}

char *get_working_directory(void) {
  size_t buffer_size = PATH_MAX;
  char *buffer = check(malloc(buffer_size));
  while (true) {
    if ((buffer = getcwd(buffer, buffer_size))) {
      return buffer;
    } else if (errno == ERANGE) {
      buffer_size *= 2;
      buffer = check(realloc(buffer, buffer_size));
    } else {
      free(buffer);
      return NULL;
    }
  }
}

const char *get_home_directory(void) {
  char *home = getenv("HOME");
  if (!home) {
    struct passwd *info = getpwuid(getuid());
    if (info) {
      home = info->pw_dir;
    }
  }
  return home;
}

const char *get_env(const char *var) {
  char *value = getenv(var);
  if (!value || !strcmp(value, "")) {
    return NULL;
  }
  return value;
}
