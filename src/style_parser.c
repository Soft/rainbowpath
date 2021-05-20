#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "style_parser.h"
#include "parser_common.h"
#include "bytes.h"
#include "list.h"
#include "utils.h"


static bool parse_symbolic_color(const char *value, uint8_t *color) {
  static const char *colors[] = {
    "black",
    "red",
    "green",
    "yellow",
    "blue",
    "magenta",
    "cyan",
    "white",
  };
  for (uint8_t i = 0; i < ARRAY_SIZE(colors); i++) {
    if (!strcmp(colors[i], value)) {
      *color = i;
      return true;
    }
  }
  return false;
}

static bool parse_numeric_color(const char *value, uint8_t *color) {
  char *num_end;
  unsigned long num = strtoul(value, &num_end, 10);
  if (num == 0 && num_end == value) {
    return false;
  };
  if (*num_end != '\0') {
    return false;
  }
  if (num > (unsigned long)UINT8_MAX) {
    parse_error("Color outside acceptable range");
    return false;
  }
  *color = (uint8_t)num;
  return true;
}

static const char *parse_color(const char *pos, const char *end, uint8_t *color) {
  char *token;
  pos = parse_token(pos, end, &token);
  if (!pos) {
    return NULL;
  }
  if (parse_symbolic_color(token, color)) {
    goto out;
  } else if (parse_numeric_color(token, color)) {
    goto out;
  } else {
    parse_error("Invalid color");
    pos = NULL;
  }
out:
  free(token);
  return pos;
}

static const char *parse_color_assignment(const char *pos, const char *end, struct color_attr *color_attr) {
  pos = parse_char(pos, end, '=');
  if (!pos) {
    return NULL;
  }
  uint8_t color;
  pos = parse_color(pos, end, &color);
  if (!pos) {
    return NULL;
  }
  color_attr->state = ATTR_STATE_SET;
  color_attr->value = color;
  return pos;
}

static const char *parse_property(const char *pos, const char *end, struct style *style) {
  bool revert = false;
  const char *pos_ = parse_char(pos, end, '!');
  if (pos_) {
    revert = true;
    pos = pos_;
  }
  char *token;
  pos = parse_token(pos, end, &token);
  if (!pos) {
    return NULL;
  }
  if (!strcmp(token, "fg")) {
    if (!revert) {
      pos = parse_color_assignment(pos, end, &style->fg);
      if (!pos) {
        goto out;
      }
    } else {
      style->fg.state = ATTR_STATE_REVERTED;
    }
  } else if (!strcmp(token, "bg")) {
    if (!revert) {
      pos = parse_color_assignment(pos, end, &style->bg);
      if (!pos) {
        goto out;
      }
    } else {
      style->bg.state = ATTR_STATE_REVERTED;
    }
  } else if (!strcmp(token, "bold")) {
    style->bold.state = revert ? ATTR_STATE_REVERTED : ATTR_STATE_SET;
    style->bold.value = true;
  } else if (!strcmp(token, "dim")) {
    style->dim.state = revert ? ATTR_STATE_REVERTED : ATTR_STATE_SET;
    style->dim.value = true;
  } else if (!strcmp(token, "underlined")) {
    style->underlined.state = revert ? ATTR_STATE_REVERTED : ATTR_STATE_SET;
    style->underlined.value = true;
  } else if (!strcmp(token, "blink")) {
    style->blink.state = revert ? ATTR_STATE_REVERTED : ATTR_STATE_SET;
    style->blink.value = true;
  } else {
    parse_error("Unknown property");
    pos = NULL;
  }
 out:
  free(token);
  return pos;
}

static const char *parse_style_inner(const char *pos, const char *end, struct style *style) {
  memset(style, 0, sizeof(*style));
  pos = skip_whitespace(pos, end);
  pos = parse_property(pos, end, style);
  if (!pos) {
    parse_error("Expected property");
    return NULL;
  }
  while (true) {
    const char *pos_ = parse_char(pos, end, ',');
    if (!pos_) {
      break;
    }
    pos = parse_property(pos_, end, style);
    if (!pos) {
      parse_error("Expected property");
      return NULL;
    }
  }
  return pos;
}

const char *parse_style(const char *pos, const char *end, struct style **style) {
  struct style *style_ = check(malloc(sizeof(*style_)));
  pos = parse_style_inner(pos, end, style_);
  if (!pos) {
    parse_error("Expected style");
    free(style_);
    return NULL;
  }
  pos = skip_whitespace(pos, end);
  if (!at_end(pos, end)) {
    parse_error("Expected end of style");
    free(style_);
    return NULL;
  }
  *style = style_;
  return pos;
}

const char *parse_style_cstr(const char *str, struct style **style) {
  return parse_style(str, str + strlen(str), style);
}

const char *parse_palette(const char *pos, const char *end, struct palette **palette) {
  struct palette *palette_ = palette_create();
  struct style *style = palette_add(palette_);
  pos = parse_style_inner(pos, end, style);
  if (!pos) {
    parse_error("Expected style");
    palette_free(palette_);
    return NULL;
  }
  while (true) {
    const char *pos_ = parse_char(pos, end, ';');
    if (!pos_) {
      break;
    }
    style = palette_add(palette_);
    pos = parse_style_inner(pos_, end, style);
    if (!pos) {
      parse_error("Expected style");
      palette_free(palette_);
      return NULL;
    }
  }
  pos = skip_whitespace(pos, end);
  if (!at_end(pos, end)) {
    parse_error("Expected end of palette");
    palette_free(palette_);
    return NULL;
  }
  *palette = palette_;
  return pos;
}

const char *parse_palette_cstr(const char *str, struct palette **palette) {
  return parse_palette(str, str + strlen(str), palette);
}
