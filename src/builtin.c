#include "rainbowpath.h"

#include <inttypes.h>
#include <stdio.h>

#define COLOR_COUNT 256

bool setup_terminal(void) {
  return true;
}

int get_color_count(void) {
  return COLOR_COUNT;
}

void begin_fg(uint8_t color) {
  printf("\e[38;5;%" PRIu8 "m", color);
}

void begin_bg(uint8_t color) {
  printf("\e[48;5;%" PRIu8 "m", color);
}

void begin_bold(void) {
  fputs("\e[1m", stdout);
}

void begin_dim(void) {
  fputs("\e[2m", stdout);
}

void begin_underlined(void) {
  fputs("\e[4m", stdout);
}

void begin_blink(void) {
  fputs("\e[5m", stdout);
}

void reset_style(void) {
  fputs("\e[0m", stdout);
}
