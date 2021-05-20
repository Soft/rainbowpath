#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include <stdint.h>

struct terminal;

struct terminal *terminal_create(void);
int terminal_color_count(struct terminal *terminal);
void terminal_fg(struct terminal *terminal, uint8_t color);
void terminal_bg(struct terminal *terminal, uint8_t color);
void terminal_bold(struct terminal *terminal);
void terminal_dim(struct terminal *terminal);
void terminal_underlined(struct terminal *terminal);
void terminal_blink(struct terminal *terminal);
void terminal_reset_style(struct terminal *terminal);
void terminal_free(struct terminal *terminal);

#endif
