#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "style_parser.h"
#include "config_parser.h"
#include "styles.h"
#include "utils.h"

int main(int argc, char *argv[]) {
  int ret = EXIT_FAILURE;
  char *input = NULL;
  size_t length;
  if (argc != 2) {
    goto out;
  }
  if (!read_stream(stdin, &input, &length)) {
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
  } else if (!strcmp(argv[1], "config")) {
    struct list *options;
    const char *pos = parse_config(input, input + length, &options);
    if (!pos) {
      goto out;
    }
    list_free(options, (elem_free_t)option_free);
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

