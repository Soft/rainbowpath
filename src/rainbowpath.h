#ifndef RAINBOWPATH_H
#define RAINBOWPATH_H

#include <stdbool.h>
#include <stdint.h>

bool setup_terminal(void);

int get_color_count(void);

void begin_fg(uint8_t color);

void begin_bg(uint8_t color);

void begin_bold(void);

void begin_dim(void);

void begin_underlined(void);

void begin_blink(void);

void reset_style(void);

#endif
