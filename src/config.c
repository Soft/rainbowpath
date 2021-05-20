#include "build.h"

#include "config.h"
#include "utils.h"
#include "config_parser.h"
#include "style_parser.h"
#include "indexer.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define CONFIG_FILE "rainbowpath.conf"
#define MOD(a,b) ((((a)%(b))+(b))%(b))

static inline bool file_exists(const char *path) {
  struct stat buf = { 0 };
  return stat(path, &buf) == 0 && S_ISREG(buf.st_mode);
}

static char *get_config_path(void) {
  const char *xdg_config_home;
  const char *xdg_config_dirs;
  const char *home = get_home_directory();
  char *path;
  if (!home) {
    return NULL;
  }
  path = check_asprintf("%s/." CONFIG_FILE, home);
  if (file_exists(path)) {
    return path;
  }
  free(path);
  xdg_config_home = get_env("XDG_CONFIG_HOME");
  if (xdg_config_home) {
    path = check_asprintf("%s/" PACKAGE_NAME "/" CONFIG_FILE, xdg_config_home);
  } else {
    path = check_asprintf("%s/.config/" PACKAGE_NAME "/" CONFIG_FILE, home);
  }
  if (file_exists(path)) {
    return path;
  }
  free(path);
  xdg_config_dirs = get_env("XDG_CONFIG_DIRS");
  if (xdg_config_dirs) {
    char *component;
    char *state;
    char *dirs = check(strdup(xdg_config_dirs));
    for (component = strtok_r(dirs, ":", &state);
         component;
         component = strtok_r(NULL, ":", &state)) {
      path = check_asprintf("%s/" PACKAGE_NAME "/" CONFIG_FILE, component);
      if (file_exists(path)) {
        free(dirs);
        return path;
      }
      free(path);
    }
    free(dirs);
  }
  path = SYSCONFDIR "/xdg/" PACKAGE_NAME "/" CONFIG_FILE;
  if (file_exists(path)) {
    return check(strdup(path));
  }
  return NULL;
}

static bool option_load_palette(struct option *option, struct palette **palette) {
  if (option_kind(option) != OPTION_KIND_STRING) {
    return false;
  }
  const char *string = option_string_value(option);
  struct palette *palette_;
  if (!parse_palette_cstr(string, &palette_)) {
    return false;
  }
  if (*palette) {
    palette_free(*palette);
  }
  *palette = palette_;
  return true;
}

static bool option_load_bool(struct option *option, bool *value) {
  if (option_kind(option) != OPTION_KIND_BOOL) {
    return false;
  }
  if (option_has_index(option)) {
    return false;
  }
  *value = option_bool_value(option);
  return true;
}

static bool option_load_string(struct option *option, char **value) {
  if (option_kind(option) != OPTION_KIND_STRING) {
    return false;
  }
  if (option_has_index(option)) {
    return false;
  }
  if (*value) {
    free(*value);
  }
  *value = option_take_string_value(option);
  return true;
}

static bool option_load_indexer(struct option *option, indexer_t *indexer) {
  if (option_kind(option) != OPTION_KIND_STRING) {
    return false;
  }
  if (option_has_index(option)) {
    return false;
  }
  indexer_t indexer_ = get_indexer(option_string_value(option));
  if (!indexer_) {
    return false;
  }
  *indexer = indexer_;
  return true;
}

static bool option_load_override(struct option *option, struct list *overrides) {
  if (option_kind(option) != OPTION_KIND_STRING) {
    return false;
  }
  if (!option_has_index(option)) {
    return false;
  }
  struct override *override = check(malloc(sizeof(*override)));
  override->raw_index = option_index(option);
  if (!parse_style_cstr(option_string_value(option), &override->style)) {
    free(override);
    return false;
  }
  list_append(overrides, override);
  return true;
}

static bool config_load_path(struct config *config, const char *path) {
  bool ret = false;
  FILE *handle = fopen(path, "r");
  if (!handle) {
    goto out;
  }
  char *data = NULL;
  size_t data_len;
  if (!read_stream(handle, &data, &data_len)) {
    goto out;
  }
  struct list *options = NULL;
  if (!parse_config(data, data + data_len, &options)) {
    goto out;
  }
  struct list_elem *elem = list_first(options);
  while (elem) {
    struct option *option = list_elem_value(elem);
    const char *name = option_name(option);
    if (!strcmp(name, "palette")) {
      if (!option_load_palette(option, &config->path_palette)) {
        goto out;
      }
    } else if (!strcmp(name, "separator-palette")) {
      if (!option_load_palette(option, &config->separator_palette)) {
        goto out;
      }
    } else if (!strcmp(name, "separator")) {
      if (!option_load_string(option, &config->separator)) {
        goto out;
      }
    } else if (!strcmp(name, "method")) {
      if (!option_load_indexer(option, &config->path_indexer)) {
        goto out;
      }
    } else if (!strcmp(name, "separator-method")) {
      if (!option_load_indexer(option, &config->separator_indexer)) {
        goto out;
      }
    } else if (!strcmp(name, "override")) {
      if (!option_load_override(option, config->path_overrides)) {
        goto out;
      }
    } else if (!strcmp(name, "separator-override")) {
      if (!option_load_override(option, config->separator_overrides)) {
        goto out;
      }
    } else if (!strcmp(name, "strip-leading")) {
      if (!option_load_bool(option, &config->strip_leading)) {
        goto out;
      }
    } else if (!strcmp(name, "compact")) {
      if (!option_load_bool(option, &config->compact)) {
        goto out;
      }
    } else if (!strcmp(name, "newline")) {
      if (!option_load_bool(option, &config->new_line)) {
        goto out;
      }
    } else if (!strcmp(name, "bash")) {
      if (!option_load_bool(option, &config->bash_escape)) {
        goto out;
      }
    } else {
      fprintf(stderr, "Invalid option '%s'\n", name);
      goto out;
    }
    elem = list_elem_next(elem);
  }
  ret = true;
 out:
  if (options) {
    list_free(options, (elem_free_t)option_free);
  }
  if (data) {
    free(data);
  }
  if (handle) {
    fclose(handle);
  }
  return ret;
}

bool config_load(struct config *config) {
  char *path = get_config_path();
  if (!path) {
    return true;
  }
  bool ret = config_load_path(config, path);
  free(path);
  return ret;
}

size_t override_index(const struct override *override, size_t length) {
  return MOD(override->raw_index, ((ssize_t)length));
}

void override_free(struct override *override) {
  free(override->style);
  free(override);
}

struct config *config_create(void) {
  struct config *config = check(malloc(sizeof(*config)));
  config->path = NULL;
  config->separator = check(strdup("/"));
  config->path_palette = NULL;
  config->separator_palette = NULL;
  config->path_overrides = list_create();
  config->separator_overrides = list_create();
  config->new_line = true;
  config->bash_escape = false;
  config->compact = false;
  config->strip_leading = false;
  config->path_indexer = index_sequential;
  config->separator_indexer = index_sequential;
  return config;
}

const struct palette *config_path_palette(struct terminal *terminal,
                                          const struct config *config) {
  if (config->path_palette) {
    return config->path_palette;
  }
  if (terminal_color_count(terminal) >= 256) {
    return PATH_PALETTE_256;
  }
  return PATH_PALETTE_8;
}

const struct palette *config_separator_palette(struct terminal *terminal,
                                               const struct config *config) {
  if (config->separator_palette) {
    return config->separator_palette;
  }
  if (terminal_color_count(terminal) >= 256) {
    return SEPARATOR_PALETTE_256;
  }
  return SEPARATOR_PALETTE_8;
}

void config_free(struct config *config) {
  if (config->separator) {
    free(config->separator);
  }
  if (config->path_palette) {
    palette_free(config->path_palette);
  }
  if (config->separator_palette) {
    palette_free(config->separator_palette);
  }
  list_free(config->path_overrides, (elem_free_t)override_free);
  list_free(config->separator_overrides, (elem_free_t)override_free);
  free(config);
}

