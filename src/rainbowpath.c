#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "args.h"
#include "styles.h"
#include "terminal.h"
#include "indexer.h"
#include "list.h"
#include "config.h"

static void begin_style(struct terminal *terminal,
                        const struct style *style,
                        bool bash_escape) {
  if (bash_escape) {
    fputs("\\[", stdout);
  }
  if (bool_attr_enabled(&style->bold)) {
    terminal_bold(terminal);
  }
  if (bool_attr_enabled(&style->dim)) {
    terminal_dim(terminal);
  }
  if (bool_attr_enabled(&style->underlined)) {
    terminal_underlined(terminal);
  }
  if (bool_attr_enabled(&style->blink)) {
    terminal_blink(terminal);
  }
  if (style->bg.state == ATTR_STATE_SET) {
    terminal_bg(terminal, style->bg.value);
  }
  if (style->fg.state == ATTR_STATE_SET) {
    terminal_fg(terminal, style->fg.value);
  }
  if (bash_escape) {
    fputs("\\]", stdout);
  }
}

static void end_style(struct terminal *terminal, bool bash_escape) {
  if (bash_escape) {
    fputs("\\[", stdout);
  }
  terminal_reset_style(terminal);
  if (bash_escape) {
    fputs("\\]", stdout);
  }
}

static void merge_color_attr(const struct color_attr *lower,
                             const struct color_attr *upper,
                             struct color_attr *result) {
  result->state = lower->state;
  result->value = lower->value;
  switch (upper->state){
  case ATTR_STATE_UNSET:
    break;
  case ATTR_STATE_SET:
    result->state = ATTR_STATE_SET;
    result->value = upper->value;
    break;
  case ATTR_STATE_REVERTED:
    result->state = ATTR_STATE_UNSET;
    break;
  }
}

static void merge_bool_attr(const struct bool_attr *lower,
                            const struct bool_attr *upper,
                            struct bool_attr *result) {
  result->state = lower->state;
  result->value = lower->value;
  switch (upper->state){
  case ATTR_STATE_UNSET:
    break;
  case ATTR_STATE_SET:
    result->state = ATTR_STATE_SET;
    result->value = upper->value;
    break;
  case ATTR_STATE_REVERTED:
    result->state = ATTR_STATE_UNSET;
    break;
  }
}

static void merge_styles(const struct style *lower,
                         const struct style *upper,
                         struct style *result) {
  merge_color_attr(&lower->fg, &upper->fg, &result->fg);
  merge_color_attr(&lower->bg, &upper->bg, &result->bg);
  merge_bool_attr(&lower->bold, &upper->bold, &result->bold);
  merge_bool_attr(&lower->dim, &upper->dim, &result->dim);
  merge_bool_attr(&lower->underlined, &upper->underlined, &result->underlined);
  merge_bool_attr(&lower->blink, &upper->blink, &result->blink);
}

static char *compact_path(const char *path) {
  const char *home = get_home_directory();
  if (!home) {
    return NULL;
  }
  const size_t path_len = strlen(path);
  const size_t home_len = strlen(home);
  if (strncmp(path, home, home_len) == 0) {
    const char next = *(path + home_len);
    if (next == '/' || !next) {
      const size_t result_size = path_len - home_len + 2;
      char *result = check(malloc(result_size));
      strncpy(result, "~", result_size);
      strncat(result, path + home_len, result_size - 1);
      return result;
    }
  }
  return check(strdup(path));
}

static void strip_leading(char *path) {
  const char *pos = path; 
  for (; *pos == '/'; pos++);
  memmove(path, pos, strlen(pos) + 1);
}

static void path_component_count(const char *path,
                                 size_t *segment_count,
                                 size_t *separator_count) {
  const char *sep;
  size_t segments = 0;
  size_t separators = 0;
  while ((sep = strchr(path, '/'))) {
    if (sep != path) {
      segments++;
    }
    separators++;
    path = sep + 1;
  }
  if (*path) {
    segments++;
  }
  *segment_count = segments;
  *separator_count = separators;
}

static const struct style *select_style(const struct palette *palette,
                                        const struct list *overrides,
                                        indexer_t indexer,
                                        size_t index,
                                        size_t element_count,
                                        const char *start,
                                        const char *end,
                                        struct style *tmp) {
  bool merged = false;
  size_t selected = indexer(palette_size(palette), index, start, end);
  const struct style *style = palette_get(palette, selected);
  struct list_elem *elem = list_first(overrides);
  while (elem) {
    struct override *override = (struct override *)list_elem_value(elem);
    if (override_index(override, element_count) == index) {
      merge_styles(merged ? tmp : style, override->style, tmp);
      merged = true;
    }
    elem = list_elem_next(elem);
  }
  return merged ? tmp : style;
}

static char *get_path(struct config *config) {
  char *path = NULL;
  if (config->path) {
    path = check(strdup(config->path));
  } else {
    path = get_working_directory();
    if (!path) {
      fputs("Failed to get working directory\n", stderr);
      goto error;
    }
  }
  if (config->compact) {
    char *compacted = compact_path(path);
    if (!compacted) {
      fputs("Failed to get home directory\n", stderr);
      goto error;
    }
    free(path);
    path = compacted;
  }
  if (config->strip_leading) {
    strip_leading(path);
  }
  return path;
 error:
  if (path) {
    free(path);
  }
  return NULL;
}

static bool print_path(struct terminal *terminal, struct config *config) {
  bool ret = false;
  const struct palette *path_palette = config_path_palette(terminal, config);
  const struct palette *separator_palette = config_separator_palette(terminal, config);
  const char *sep;
  size_t path_index = 0;
  size_t separator_index = 0;
  const struct style *style;
  struct style tmp_style;

  char *full_path = get_path(config);
  if (!full_path) {
    return false;
  }
  const char *path = full_path;

  size_t segment_count;
  size_t separator_count;
  path_component_count(path, &segment_count, &separator_count);

  while ((sep = strchr(path, '/'))) {
    if (sep != path) {
      style = select_style(path_palette,
                           config->path_overrides,
                           config->path_indexer,
                           path_index,
                           segment_count,
                           path,
                           sep,
                           &tmp_style);
      begin_style(terminal, style, config->bash_escape);
      fwrite(path, 1, sep - path, stdout);
      end_style(terminal, config->bash_escape);
      path_index++;
    }

    style = select_style(separator_palette,
                         config->separator_overrides,
                         config->separator_indexer,
                         separator_index,
                         separator_count,
                         sep,
                         sep + 1,
                         &tmp_style);
    begin_style(terminal, style, config->bash_escape);
    fputs(config->separator, stdout);
    end_style(terminal, config->bash_escape);
    separator_index++;
    path = sep + 1;
  }

  if (*path) {
    style = select_style(path_palette,
                         config->path_overrides,
                         config->path_indexer,
                         path_index,
                         segment_count,
                         path,
                         path + strlen(path),
                         &tmp_style);
    begin_style(terminal, style, config->bash_escape);
    fputs(path, stdout);
    end_style(terminal, config->bash_escape);
  }

  if (config->new_line) {
    fputc('\n', stdout);
  }

  ret = true;
  free(full_path);
  return ret;
}

int main(int argc, char *argv[]) {
  int ret = EXIT_FAILURE;
  struct config *config = config_create();

  init_random();

  struct terminal *terminal = terminal_create();
  if (!terminal) {
    goto out;
  }

  bool exit;
  if (!parse_args(argc, argv, config, &exit)) {
    goto out;
  }

  if (exit) {
    ret = EXIT_SUCCESS;
    goto out;
  }

  if (!config_load(config)) {
    fputs("Failed to load configuration file\n", stderr);
    goto out;
  }

  if (!print_path(terminal, config)) {
    goto out;
  }

  ret = EXIT_SUCCESS;

out:
  config_free(config);
  terminal_free(terminal);
  return ret;
}
