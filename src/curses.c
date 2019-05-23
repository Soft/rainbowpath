#include "rainbowpath.h"

#include <stdlib.h>
#include <unistd.h>

#include <curses.h>
#include <term.h>

struct terminal {
  int color_count;
  char *fg;
  char *bg;
  char *bold;
  char *dim;
  char *underlined;
  char *blink;
  char *reset;
} terminal;

bool setup_terminal(void) {
  int err, int_value;
  char *str_value;

  if (setupterm(NULL, STDOUT_FILENO, &err) == ERR) {
    switch (err) {
      case 1:
        fputs("Invalid terminal type\n", stderr);
        return false;
      case 0:
        fputs("Failed to determine terminal type\n", stderr);
        return false;
      case -1:
        fputs("Failed to access terminfo database\n", stderr);
        return false;
      default:
        fputs("Unknown error\n", stderr);
        return false;
    }
  }

  if ((int_value = tigetnum("colors")) < 0) {
    return false;
  }
  terminal.color_count = int_value;

  if ((str_value = tigetstr("setaf")) < (char *)0) {
    return false;
  }
  terminal.fg = str_value;

  if ((str_value = tigetstr("setab")) < (char *)0) {
    return false;
  }
  terminal.bg = str_value;

  if ((str_value = tigetstr("bold")) < (char *)0) {
    return false;
  }
  terminal.bold = str_value;

  if ((str_value = tigetstr("dim")) < (char *)0) {
    return false;
  }
  terminal.dim = str_value;

  if ((str_value = tigetstr("smul")) < (char *)0) {
    return false;
  }
  terminal.underlined = str_value;

  if ((str_value = tigetstr("blink")) < (char *)0) {
    return false;
  }
  terminal.blink = str_value;

  if ((str_value = tigetstr("sgr0")) < (char *)0) {
    return false;
  }
  terminal.reset = str_value;

  return true;
}

int get_color_count(void) {
  return terminal.color_count;
}

void begin_fg(uint8_t color) {
  char *result = tiparm(terminal.fg, color);
  putp(result);
}

void begin_bg(uint8_t color) {
  char *result = tiparm(terminal.bg, color);
  putp(result);
}

void begin_bold(void) {
  putp(terminal.bold);
}

void begin_dim(void) {
  putp(terminal.dim);
}

void begin_underlined(void) {
  putp(terminal.underlined);
}

void begin_blink(void) {
  putp(terminal.blink);
}

void reset_style(void) {
  putp(terminal.reset);
}
