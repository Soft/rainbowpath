#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "parser.h"
#include "styles.h"
#include "utils.h"

bool consume_stream(FILE *stream, char **data, size_t *length) {
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

int main(int argc, char *argv[]) {
  int ret = EXIT_FAILURE;
  char *input = NULL;
  size_t length;
  if (argc != 2) {
    goto out;
  }
  if (!consume_stream(stdin, &input, &length)) {
    goto out;
  }
  if (!strcmp(argv[1], "style")) {
    struct style *style;
    const char *pos = parse_style(input, input + length, &style);
    if (!pos) {
      goto out;
    }
    free(style);
  } else if (!strcmp(argv[1], "palette")) {
    struct palette *palette;
    const char *pos = parse_palette(input, input + length, &palette);
    if (!pos) {
      goto out;
    }
    palette_free(palette);
  } else {
    goto out;
  }
  ret = EXIT_SUCCESS;
 out:
  if (input) {
    free(input);
  }
  return ret;
}

