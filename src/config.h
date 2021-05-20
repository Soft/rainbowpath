#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#include "indexer.h"
#include "terminal.h"

struct override {
  ssize_t raw_index;
  struct style *style;
};

struct config {
  const char *path;
  char *separator;
  struct palette *path_palette;
  struct palette *separator_palette;
  struct list *path_overrides;
  struct list *separator_overrides;
  bool new_line;
  bool bash_escape;
  bool compact;
  bool strip_leading;
  indexer_t path_indexer;
  indexer_t separator_indexer;
};

size_t override_index(const struct override *override, size_t length);
void override_free(struct override *override);

struct config *config_create(void);
bool config_load(struct config *config);
const struct palette *config_path_palette(struct terminal *terminal, const struct config *config);
const struct palette *config_separator_palette(struct terminal *terminal, const struct config *config);
void config_free(struct config *config);

#endif
