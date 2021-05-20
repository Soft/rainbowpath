#include "terminal.h"

#include <stdlib.h>
#include <unistd.h>
#include <curses.h>
#include <term.h>

#include "utils.h"

struct terminal {
  int color_count;
  const char *fg;
  const char *bg;
  const char *bold;
  const char *dim;
  const char *underlined;
  const char *blink;
  const char *reset;
};

struct terminal *terminal_create(void) {
  int err;
  int int_value;
  char *str_value;

  if (setupterm(NULL, STDOUT_FILENO, &err) == ERR) {
    switch (err) {
      case 1:
        fputs("Invalid terminal type\n", stderr);
        return NULL;
      case 0:
        fputs("Failed to determine terminal type\n", stderr);
        return NULL;
      case -1:
        fputs("Failed to access terminfo database\n", stderr);
        return NULL;
      default:
        fputs("Unknown error\n", stderr);
        return NULL;
    }
  }

  struct terminal *terminal = check(malloc(sizeof(*terminal)));

  if ((int_value = tigetnum("colors")) < 0) {
    goto error;
  }
  terminal->color_count = int_value;

  if ((str_value = tigetstr("setaf")) < (char *)0) {
    goto error;
  }
  terminal->fg = str_value;

  if ((str_value = tigetstr("setab")) < (char *)0) {
    goto error;
  }
  terminal->bg = str_value;

  if ((str_value = tigetstr("bold")) < (char *)0) {
    goto error;
  }
  terminal->bold = str_value;

  if ((str_value = tigetstr("dim")) < (char *)0) {
    goto error;
  }
  terminal->dim = str_value;

  if ((str_value = tigetstr("smul")) < (char *)0) {
    goto error;
  }
  terminal->underlined = str_value;

  if ((str_value = tigetstr("blink")) < (char *)0) {
    goto error;
  }
  terminal->blink = str_value;

  if ((str_value = tigetstr("sgr0")) < (char *)0) {
    goto error;
  }
  terminal->reset = str_value;

  return terminal;

 error:
  free(terminal);
  return NULL;
}

int terminal_color_count(struct terminal *terminal) {
  return terminal->color_count;
}

void terminal_fg(struct terminal *terminal, uint8_t color) {
  char *result = tiparm(terminal->fg, color);
  putp(result);
}

void terminal_bg(struct terminal *terminal, uint8_t color) {
  char *result = tiparm(terminal->bg, color);
  putp(result);
}

void terminal_bold(struct terminal *terminal) {
  putp(terminal->bold);
}

void terminal_dim(struct terminal *terminal) {
  putp(terminal->dim);
}

void terminal_underlined(struct terminal *terminal) {
  putp(terminal->underlined);
}

void terminal_blink(struct terminal *terminal) {
  putp(terminal->blink);
}

void terminal_reset_style(struct terminal *terminal) {
  putp(terminal->reset);
}

void terminal_free(struct terminal *terminal) {
  free(terminal);
}

