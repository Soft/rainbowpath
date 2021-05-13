#ifndef STYLES_H
#define STYLES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum attr_state {
  ATTR_STATE_UNSET,
  ATTR_STATE_SET,
  ATTR_STATE_REVERTED,
};

struct color_attr {
  enum attr_state state;
  uint8_t value; // Set if state == ATTR_STATE_SET
};

struct bool_attr {
  enum attr_state state;
  bool value; // Set if state == ATTR_STATE_SET
};

struct style {
  struct color_attr fg;
  struct color_attr bg;
  struct bool_attr bold;
  struct bool_attr dim;
  struct bool_attr underlined;
  struct bool_attr blink;
};

struct palette;

struct palette *palette_create(void);
struct style *palette_get(const struct palette *palette, size_t index);
struct style *palette_add(struct palette *palette);
size_t palette_size(const struct palette *palette);
void palette_free(struct palette *palette);

const struct palette *SEPARATOR_PALETTE_8;
const struct palette *SEPARATOR_PALETTE_256;
const struct palette *PATH_PALETTE_8;
const struct palette *PATH_PALETTE_256;

#endif
