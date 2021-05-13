#ifndef PARSER_H
#define PARSER_H

#include "styles.h"
#include "list.h"

const char *parse_style(const char *pos, const char *end, struct style **style);
const char *parse_style_cstr(const char *str, struct style **style);
const char *parse_palette(const char *pos, const char *end, struct palette **palette);
const char *parse_palette_cstr(const char *str, struct palette **palette);

#endif
