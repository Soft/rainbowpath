#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "styles.h"
#include "utils.h"

enum {
  INITIAL_PALETTE_SIZE = 32
};

struct palette {
  struct style *styles;
  size_t size;
  size_t cap;
};

struct palette *palette_create(void) {
  struct palette *palette = check(malloc(sizeof(*palette)));
  struct style *styles = check(calloc(INITIAL_PALETTE_SIZE, sizeof(*styles)));
  palette->size = 0;
  palette->cap = INITIAL_PALETTE_SIZE;
  palette->styles = styles;
  return palette;
}

struct style *palette_get(const struct palette *palette, size_t index) {
  assert(index < palette->size);
  return palette->styles + index;
}

struct style *palette_add(struct palette *palette) {
  if (palette->size >= palette->cap) {
    size_t new_cap = palette->cap * 2;
    palette->styles = check(realloc(palette->styles, new_cap));
    palette->cap = new_cap;
  }
  struct style *style = palette->styles + palette->size;
  memset(style, 0, sizeof(*style));
  palette->size++;
  return style;
}

size_t palette_size(const struct palette *palette) {
  return palette->size;
}

void palette_free(struct palette *palette) {
  free(palette->styles);
  free(palette);
}

// Default styles

static struct style SEPARATOR_STYLES_8[] = {
  {
    .fg = { .state = ATTR_STATE_SET, .value = 7 },
    .dim = { .state = ATTR_STATE_SET, .value = true },
  }
};

static struct palette SEPARATOR_PALETTE_8_ = {
  .styles = SEPARATOR_STYLES_8,
  .size = ARRAY_SIZE(SEPARATOR_STYLES_8),
  .cap  = ARRAY_SIZE(SEPARATOR_STYLES_8),
};


const struct palette *SEPARATOR_PALETTE_8 = &SEPARATOR_PALETTE_8_;

static struct style PATH_STYLES_8[] = {
  { .fg = { .state = ATTR_STATE_SET, .value = 1 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 3 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 2 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 6 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 4 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 5 } }
};

static struct palette PATH_PALETTE_8_ = {
  .styles = PATH_STYLES_8,
  .size = ARRAY_SIZE(PATH_STYLES_8),
  .cap = ARRAY_SIZE(PATH_STYLES_8),
};

const struct palette *PATH_PALETTE_8 = &PATH_PALETTE_8_;

static struct style SEPARATOR_STYLES_256[] = {
  {
    .fg = { .state = ATTR_STATE_SET, .value = 239 },
    .bold = { .state = ATTR_STATE_SET, .value = true },
  }
};

static struct palette SEPARATOR_PALETTE_256_ = {
  .styles = SEPARATOR_STYLES_256,
  .size = ARRAY_SIZE(SEPARATOR_STYLES_256),
  .cap = ARRAY_SIZE(SEPARATOR_STYLES_256),
};

const struct palette *SEPARATOR_PALETTE_256 = &SEPARATOR_PALETTE_256_;

static struct style PATH_STYLES_256[] = {
  { .fg = { .state = ATTR_STATE_SET, .value = 160 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 208 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 220 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 82 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 39 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 63 } },
};

static struct palette PATH_PALETTE_256_ = {
  .styles = PATH_STYLES_256,
  .size = ARRAY_SIZE(PATH_STYLES_256),
  .cap = ARRAY_SIZE(PATH_STYLES_256),
};

const struct palette *PATH_PALETTE_256 = &PATH_PALETTE_256_;
