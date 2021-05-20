#include "terminal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stddef.h>

#include "utils.h"

struct term_cap {
  const char *name;
  int colors;
};

static const struct term_cap TERMINALS[] = {
  { "linux", 8 },
  { "xterm", 8 },
  { "screen-256color", 256 },
  { "xterm-256color", 256 },
  { "rxvt-unicode-256color", 256 },
  { "alacritty", 256 },
};

struct terminal {
  int color_count;
};

#define DEFAULT_COLOR_COUNT 256

struct terminal *terminal_create(void) {
  struct terminal *terminal = check(malloc(sizeof(*terminal)));
  terminal->color_count = DEFAULT_COLOR_COUNT;
  const char *term = get_env("TERM");
  if (term) {
    for (size_t i = 0; i < ARRAY_SIZE(TERMINALS); i++) {
      if (!strcmp(TERMINALS[i].name, term)) {
        terminal->color_count = TERMINALS[i].colors;
        break;
      }
    }
  }
  return terminal;
}

int terminal_color_count(struct terminal *terminal) {
  return terminal->color_count;
}

void terminal_fg(UNUSED struct terminal *terminal, uint8_t color) {
  printf("\e[38;5;%" PRIu8 "m", color);
}
void terminal_bg(UNUSED struct terminal *terminal, uint8_t color) {
  printf("\e[48;5;%" PRIu8 "m", color);
}

void terminal_bold(UNUSED struct terminal *terminal) {
  fputs("\e[1m", stdout);
}

void terminal_dim(UNUSED struct terminal *terminal) {
  fputs("\e[2m", stdout);
}

void terminal_underlined(UNUSED struct terminal *terminal) {
  fputs("\e[4m", stdout);
}

void terminal_blink(UNUSED struct terminal *terminal) {
  fputs("\e[5m", stdout);
}

void terminal_reset_style(UNUSED struct terminal *terminal) {
  fputs("\e[0m", stdout);
}

void terminal_free(struct terminal *terminal) {
  free(terminal);
}


