#include "rainbowpath.h"
#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#ifdef HAVE_GETRANDOM
#include <sys/random.h>
#endif

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))
#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define UNUSED __attribute__((unused))

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

struct palette {
  struct style *styles;
  size_t size;
};

struct override_list {
  struct override *head;
  struct override *tail;
};

struct override {
  ssize_t raw_index;
  size_t index;
  struct style style;
  struct override *next;
};

typedef size_t (*indexer_t)(
  size_t palette_size,
  size_t ind,
  const char *start,
  const char *end
);

struct config {
  char *path;
  char *separator;
  struct palette *path_palette;
  struct palette *separator_palette;
  struct override_list path_overrides;
  struct override_list separator_overrides;
  bool new_line;
  bool bash_escape;
  bool compact;
  bool skip_leading;
  indexer_t path_indexer;
  indexer_t separator_indexer;
};

static struct style SEPARATOR_STYLES_8[] = {
  {
    .fg = { .state = ATTR_STATE_SET, .value = 7 },
    .dim = { .state = ATTR_STATE_SET, .value = true },
  }
};

static struct palette SEPARATOR_PALETTE_8 = {
  .styles = SEPARATOR_STYLES_8,
  .size = ARRAY_SIZE(SEPARATOR_STYLES_8),
};

static struct style PATH_STYLES_8[] = {
  { .fg = { .state = ATTR_STATE_SET, .value = 1 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 3 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 2 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 6 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 4 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 5 } }
};

static struct palette PATH_PALETTE_8 = {
  .styles = PATH_STYLES_8,
  .size = ARRAY_SIZE(PATH_STYLES_8),
};

static struct style SEPARATOR_STYLES_256[] = {
  {
    .fg = { .state = ATTR_STATE_SET, .value = 239 },
    .bold = { .state = ATTR_STATE_SET, .value = true },
  }
};

static struct palette SEPARATOR_PALETTE_256 = {
  .styles = SEPARATOR_STYLES_256,
  .size = ARRAY_SIZE(SEPARATOR_STYLES_256),
};

static struct style PATH_STYLES_256[] = {
  { .fg = { .state = ATTR_STATE_SET, .value = 160 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 208 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 220 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 82 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 39 } },
  { .fg = { .state = ATTR_STATE_SET, .value = 63 } },
};

static struct palette PATH_PALETTE_256 = {
  .styles = PATH_STYLES_256,
  .size = ARRAY_SIZE(PATH_STYLES_256),
};

static const size_t INITIAL_PALETTE_SIZE = 32;
static const size_t INITIAL_PATH_SIZE = 512;

static const char *USAGE =
    "Usage: " PACKAGE_NAME " [-p PALETTE] [-s PALETTE] [-S SEPARATOR] [-m METHOD]\n"
    "                   [-M METHOD] [-o INDEX STYLE] [-O INDEX STYLE]\n"
    "                   [-l] [-c] [-n] [-b] [-h] [-v] [PATH]\n\n"
    "Color path components using a palette.\n\n"
    "Options:\n"
    "  -p, --palette PALETTE                 Semicolon separated list of styles for\n"
    "                                        path components\n"
    "  -s, --separator-palette PALETTE       Semicolon separated list of styles for\n"
    "                                        path separators.\n"
    "  -S, --separator SEPARATOR             String used to separate path components\n"
    "                                        in the output (defaults to '/')\n"
    "  -m, --method METHOD                   Method for selecting styles from palette.\n"
    "                                        One of sequential, hash, random\n"
    "                                        (defaults to sequential).\n"
    "  -M, --separator-method METHOD         Method for selecting styles from separator\n"
    "                                        palette. One of sequential, hash, random\n"
    "                                        (defaults to sequential).\n"
    "  -o, --override INDEX STYLE            Override style at the given index. This option\n"
    "                                        can appear multiple times.\n"
    "  -O, --separator-override INDEX STYLE  Override separator style at the given index.\n"
    "                                        This option can appear multiple times.\n"
    "  -l, --leading                         Do not display leading path separator\n"
    "  -c, --compact                         Replace home directory path prefix with ~\n"
    "  -n, --newline                         Do not append newline\n"
    "  -b, --bash                            Escape control codes for use in Bash prompts\n"
    "  -h, --help                            Display this help\n"
    "  -v, --version                         Display version information\n";

static inline void *check(void *ptr) {
  if (!ptr) {
    fputs("Failed to allocate memory\n", stderr);
    abort();
  }
  return ptr;
}

static bool bool_attr_enabled(const struct bool_attr *attr) {
  return attr->state == ATTR_STATE_SET && attr->value;
}

static void begin_style(const struct style *style, bool bash_escape) {
  if (bash_escape) {
    fputs("\\[", stdout);
  }
  if (bool_attr_enabled(&style->bold)) {
    begin_bold();
  }
  if (bool_attr_enabled(&style->dim)) {
    begin_dim();
  }
  if (bool_attr_enabled(&style->underlined)) {
    begin_underlined();
  }
  if (bool_attr_enabled(&style->blink)) {
    begin_blink();
  }
  if (style->bg.state == ATTR_STATE_SET) {
    begin_bg(style->bg.value);
  }
  if (style->fg.state == ATTR_STATE_SET) {
    begin_fg(style->fg.value);
  }
  if (bash_escape) {
    fputs("\\]", stdout);
  }
}

static inline void end_style(bool bash_escape) {
  if (bash_escape) {
    fputs("\\[", stdout);
  }
  reset_style();
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

static void palette_free(struct palette *palette) {
  if (palette->styles) {
    free(palette->styles);
  }
  free(palette);
}

static void override_free(struct override_list *overrides) {
  struct override *next;
  for (struct override *current = overrides->head; current; current = next) {
    next = current->next;
    free(current);
  }
  overrides->head = NULL;
  overrides->tail = NULL;
}

static void override_push(struct override_list *overrides, struct override *new) {
  if (overrides->tail) {
    overrides->tail->next = new;
    overrides->tail = new;
  } else {
    overrides->head = new;
    overrides->tail = new;
  }
}

static bool override_normalize(struct override_list *overrides, size_t length) {
  struct override *current = overrides->head;
  for (; current; current = current->next) {
    if (current->raw_index < 0) {
      if (current->raw_index < -(ssize_t)length) {
        goto error;
      }
      current->index = length + current->raw_index;
    } else {
      if (current->raw_index >= (int)length) {
        goto error;
      }
      current->index = current->raw_index;
    }
  }
  return true;
 error:
  fprintf(stderr, "Invalid override index %ld\n", current->raw_index);
  return false;
}

static char *get_working_directory(void) {
  size_t buffer_size = INITIAL_PATH_SIZE;
  char *buffer = check(malloc(buffer_size));
  while (true) {
    if ((buffer = getcwd(buffer, buffer_size))) {
      return buffer;
    } else if (errno == ERANGE) {
      buffer_size *= 2;
      buffer = check(realloc(buffer, buffer_size));
    } else {
      free(buffer);
      return NULL;
    }
  }
}

static const char *get_home_directory(void) {
  char *home = getenv("HOME");
  if (!home) {
    struct passwd *info = getpwuid(getuid());
    if (info) {
      home = info->pw_dir;
    }
  }
  return home;
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
    if (next == '/' || next == '\0') {
      const size_t result_size = (path_len - home_len + 2) * sizeof(char);
      char *result = check(malloc(result_size));
      strncpy(result, "~", result_size);
      strncat(result, path + home_len, result_size - 1);
      return result;
    }
  }
  return check(strdup(path));
}

static size_t index_sequential(size_t palette_size,
                               size_t ind,
                               UNUSED const char *start,
                               UNUSED const char *end) {
  return ind % palette_size;
}

static size_t index_hash(size_t palette_size,
                         UNUSED size_t ind,
                         const char *start,
                         const char *end) {
  size_t hash = 5381;
  for (const char *c = start; c < end; c++) {
    hash = ((hash << 5) + hash) + (size_t)*c;
  }
  return hash % palette_size;
}

static size_t index_random(size_t palette_size,
                           UNUSED size_t ind,
                           UNUSED const char *start,
                           UNUSED const char *end) {
  #ifdef HAVE_DRAND48
  return drand48() * palette_size;
  #else
  return rand() % palette_size;
  #endif
}

static char *strip_leading(char *path) {
  for (; *path == '/'; path++);
  return path;
}

static void path_components(const char *path,
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

static const struct style *choose_style(const struct palette *palette,
                                        const struct override_list *overrides,
                                        indexer_t indexer,
                                        size_t index,
                                        const char *start,
                                        const char *end,
                                        struct style *tmp) {
  bool merged = false;
  size_t selected = indexer(palette->size, index, start, end);
  const struct style *style = &palette->styles[selected];
  for (const struct override *override = overrides->head; override; override = override->next) {
    if (override->index == index) {
      merge_styles(merged ? tmp : style, &override->style, tmp);
      merged = true;
    }
  }
  return merged ? tmp : style;
}

static void print_path(const char *path,
                       const char *separator,
                       const struct palette *path_palette,
                       const struct palette *separator_palette,
                       const struct override_list *path_overrides,
                       const struct override_list *separator_overrides,
                       indexer_t path_indexer,
                       indexer_t separator_indexer,
                       bool bash_escape) {
  const char *sep;
  size_t path_index = 0;
  size_t separator_index = 0;
  const struct style *style;
  struct style tmp;

  while ((sep = strchr(path, '/'))) {
    if (sep != path) {
      style = choose_style(path_palette, path_overrides,
                           path_indexer, path_index,
                           path, sep, &tmp);
      begin_style(style, bash_escape);
      fwrite(path, 1, sep - path, stdout);
      end_style(bash_escape);
      path_index++;
    }

    style = choose_style(separator_palette, separator_overrides,
                         separator_indexer, separator_index,
                         sep, sep + 1, &tmp);
    begin_style(style, bash_escape);
    fputs(separator, stdout);
    end_style(bash_escape);
    separator_index++;
    path = sep + 1;
  }

  if (*path) {
    style = choose_style(path_palette, path_overrides,
                         path_indexer, path_index,
                         path, path + strlen(path), &tmp);
    begin_style(style, bash_escape);
    fputs(path, stdout);
    end_style(bash_escape);
  }
}

static char *make_string(const char *begin, const char *end) {
  assert(end >= begin);
  size_t size = end - begin;
  char *result = check(calloc(size + 1, sizeof(char)));
  return memcpy(result, begin, size);
}

static void consume_whitespace(const char **input) {
  while (**input && isspace(**input)) {
    (*input)++;
  }
}

static const char *parse_token(const char *input, char **result) {
  consume_whitespace(&input);
  bool match = false;
  const char *curr = input;
  while (*curr && isalnum(*curr)) {
    match = true;
    curr++;
  }
  if (match) {
    *result = make_string(input, curr);
    return curr;
  } else {
    return NULL;
  }
}

static const char *parse_char(const char *input, char c) {
  consume_whitespace(&input);
  return *input == c ? ++input : NULL;
}

static const char *peek_char(const char *input, char c) {
  consume_whitespace(&input);
  return *input == c ? input : NULL;
}

static bool parse_symbolic_color(const char *str, uint8_t *result) {
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
    if (!strcmp(colors[i], str)) {
      *result = i;
      return true;
    }
  }
  return false;
}

static bool parse_color(const char *str, uint8_t *result) {
  uint8_t color;
  unsigned long int num;
  int max_color = get_color_count() - 1;
  max_color = MAX(max_color, 0);
  if (parse_symbolic_color(str, &color)) {
    num = color;
  } else {
    char *num_end;
    num = strtoul(str, &num_end, 10);
    if (num == 0 && num_end == str) {
      return false;
    };
    if (*num_end != '\0') {
      return false;
    }
  }
  if (num > (long unsigned int)MIN(UINT8_MAX, max_color)) {
    fputs("Color outside acceptable range\n", stderr);
    return false;
  }
  *result = (uint8_t)num;
  return true;
}

static const char *parse_color_assignment(const char *input, struct color_attr *result) {
  const char *next, *current;
  char *value = NULL;
  uint8_t color;
  if ((next = parse_char(input, '='))) {
    current = next;
    if ((next = parse_token(current, &value))) {
      if (!parse_color(value, &color)) {
        free(value);
        return NULL;
      }
      result->state = ATTR_STATE_SET;
      result->value = color;
      free(value);
    }
  }
  return next;
}

static const char *parse_style_inner(const char *input, struct style *style) {
  const char *current = input, *next;
  char *key = NULL;
  bool revert;
  memset(style, 0, sizeof(struct style));
  while (true) {
    revert = false;
    if ((next = parse_char(current, '!'))) {
      current = next;
      revert = true;
    }
    if ((next = parse_token(current, &key))) {
      current = next;
      if (!strcmp(key, "fg")) {
        if (!revert) {
          if ((next = parse_color_assignment(current, &style->fg))) {
            current = next;
          } else {
            fputs("Foreground color expected\n", stderr);
            goto error;
          }
        } else {
          style->fg.state = ATTR_STATE_REVERTED;
        }
      } else if (!strcmp(key, "bg")) {
        if (!revert) {
          if ((next = parse_color_assignment(current, &style->bg))) {
            current = next;
          } else {
            fputs("Background color expected\n", stderr);
            goto error;
          }
        } else {
          style->bg.state = ATTR_STATE_REVERTED;
        } 
      } else if (!strcmp(key, "bold")) {
        style->bold.state = revert ? ATTR_STATE_REVERTED : ATTR_STATE_SET;
        style->bold.value = true;
      } else if (!strcmp(key, "dim")) {
        style->dim.state = revert ? ATTR_STATE_REVERTED : ATTR_STATE_SET;
        style->dim.value = true;
      } else if (!strcmp(key, "underlined")) {
        style->underlined.state = revert ? ATTR_STATE_REVERTED : ATTR_STATE_SET;
        style->underlined.value = true;
      } else if (!strcmp(key, "blink")) {
        style->blink.state = revert ? ATTR_STATE_REVERTED : ATTR_STATE_SET;
        style->blink.value = true;
      } else {
        fprintf(stderr, "Unknown property '%s'\n", key);
        goto error;
      }
      free(key);
      key = NULL;
    } else {
      fputs("Expected a property\n", stderr);
      goto error;
    }
    if ((next = parse_char(current, ','))) {
      current = next;
    } else {
      return current;
    }
  }
error:
  if (key) {
    free(key);
  }
  return NULL;
}

static bool parse_style(const char *input, struct style *style) {
  const char *next;
  if ((next = parse_style_inner(input, style))) {
    if ((next = peek_char(next, '\0'))) {
      return true;
    } else {
      fputs("Expected end of style\n", stderr);
    }
  } else {
    fputs("Expected style\n", stderr);
  }
  return false;
}

static struct palette *parse_palette(const char *input) {
  const char *current = input;
  const char *next;
  size_t palette_size = INITIAL_PALETTE_SIZE;
  size_t pos = 0;
  struct palette *palette = check(malloc(sizeof(*palette)));
  struct style *styles = check(calloc(palette_size, sizeof(*styles)));

  while (true) {
    if ((next = parse_style_inner(current, &styles[pos]))) {
      current = next;
    } else {
      fputs("Expected a style definition\n", stderr);
      goto error;
    }

    if ((next = parse_char(current, ';'))) {
      current = next;
      pos++;
      if (pos >= palette_size) {
        palette_size *= 2;
        styles = check(realloc(styles, sizeof(*styles) * palette_size));
      }
      memset(&styles[pos], 0, sizeof(struct style));
    } else if ((next = peek_char(current, '\0'))) {
      current = next;
      break;
    } else {
      fputs("Expected ';'\n", stderr);
      goto error;
    }
  }

  palette->styles = styles;
  palette->size = pos + 1;
  return palette;
error:
  if (palette) {
    free(palette);
  }
  if (styles) {
    free(styles);
  }
  return NULL;
}

static indexer_t select_indexer(const char *name) {
  if (!strcmp(name, "sequential")) {
    return index_sequential;
  } else if (!strcmp(name, "hash")) {
    return index_hash;
  } else if (!strcmp(name, "random")) {
    return index_random;
  }
  return NULL;
}

static void setup_random(void) {
#ifdef HAVE_DRAND48
  #define INIT_FN srand48
  #define SEED_TYPE long int
#else
  #define INIT_FN srand
  #define SEED_TYPE unsigned int
#endif
#ifdef HAVE_GETRANDOM
  SEED_TYPE seed = 0;
  if (getrandom(&seed, sizeof(seed), 0) == sizeof(seed)) {
    INIT_FN(seed);
  } else {
    INIT_FN(time(NULL));
  }
#else
  INIT_FN(time(NULL));
#endif
#undef INIT_FN
#undef SEED_TYPE
}

static void usage(void) {
  fputs(USAGE, stderr);
}

static void version(void) {
  fputs(PACKAGE_STRING "\n", stderr);
}

static bool consume_argument(char ***arg, char **arg_end, const char *flag) {
  if (++*arg >= arg_end) {
    fprintf(stderr, "Invalid usage: %s requires an argument\n", flag);
    return false;
  }
  return true;
}

static bool parse_index(const char *str, ssize_t *result) {
  long int num;
  char *num_end;
  if (!*str) {
    return false;
  }
  errno = 0;
  num = strtol(str, &num_end, 10);
  if (num == 0 && num_end == str) {
    return false;
  }
  if (errno == ERANGE) {
    return false;
  }
  if (*num_end != '\0') {
    return false;
  }
  *result = num;
  return true;
}

static bool parse_palette_arg(char ***arg,
                              char **arg_end,
                              struct palette **result,
                              const char *flag) {
  if (!consume_argument(arg, arg_end, flag)) {
    return false;
  }
  struct palette *palette = parse_palette(**arg);
  if (!palette) {
    fputs("Invalid separator palette\n", stderr);
    return false;
  }
  *result = palette;
  return true;
}

static bool parse_indexer_arg(char ***arg,
                              char **arg_end,
                              indexer_t *result,
                              const char *flag) {
  indexer_t indexer;
  if (!consume_argument(arg, arg_end, flag)) {
    return false;
  }
  indexer = select_indexer(**arg);
  if (!indexer) {
    fputs("Invalid indexing method\n", stderr);
    return false;
  }
  *result = indexer;
  return true;
}

static bool parse_override_arg(char ***arg,
                               char **arg_end,
                               struct override_list *result,
                               const char *flag) {
  struct override *override = check(malloc(sizeof(*override)));
  override->next = NULL;
  if (!consume_argument(arg, arg_end, flag)) {
    goto error;
  }
  if (!parse_index(**arg, &override->raw_index)) {
    fputs("Invalid override index\n", stderr);
    goto error;
  }
  if (!consume_argument(arg, arg_end, flag)) {
    goto error;
  }
  if (!parse_style(**arg, &override->style)) {
    fputs("Invalid override style\n", stderr);
    goto error;
  }
  override_push(result, override);
  return true;
 error:
  free(override);
  return false;
}

static bool parse_args(int argc,
                       char **argv,
                       struct config *config,
                       bool *error) {
  char **arg = argv + 1;
  char **arg_end = argv + argc;

  *error = false;
  for (; arg < arg_end; arg++) {
    const char *flag = *arg;
    if (!strcmp("--palette", flag) || !strcmp("-p", flag)) {
      if (!parse_palette_arg(&arg, arg_end, &config->path_palette, flag)) {
        goto error;
      }
    } else if (!strcmp("--separator-palette", flag) || !strcmp("-s", flag)) {
      if (!parse_palette_arg(&arg, arg_end, &config->separator_palette, flag)) {
        goto error;
      }
    } else if (!strcmp("--separator", flag) || !strcmp("-S", flag)) {
      if (!consume_argument(&arg, arg_end, flag)) {
        goto error;
      }
      config->separator = *arg;
    } else if (!strcmp("--method", flag) || !strcmp("-m", flag)) {
      if (!parse_indexer_arg(&arg, arg_end, &config->path_indexer, flag)) {
        goto error;
      }
    } else if (!strcmp("--separator-method", flag) || !strcmp("-M", flag)) {
      if (!parse_indexer_arg(&arg, arg_end, &config->separator_indexer, flag)) {
        goto error;
      }
    } else if (!strcmp("--override", flag) || !strcmp("-o", flag)) {
      if (!parse_override_arg(&arg, arg_end, &config->path_overrides, flag)) {
        goto error;
      }
    } else if (!strcmp("--separator-override", flag) || !strcmp("-O", flag)) {
      if (!parse_override_arg(&arg, arg_end, &config->separator_overrides, flag)) {
        goto error;
      }
    } else if (!strcmp("--leading", flag) || !strcmp("-l", flag)) {
      config->skip_leading = true;
    } else if (!strcmp("--compact", flag) || !strcmp("-c", flag)) {
      config->compact = true;
    } else if (!strcmp("--newline", flag) || !strcmp("-n", flag)) {
      config->new_line = false;
    } else if (!strcmp("--bash", flag) || !strcmp("-b", flag)) {
      config->bash_escape = true;
    } else if (!strcmp("--help", flag) || !strcmp("-h", flag)) {
      usage();
      return false;
    } else if (!strcmp("--version", flag) || !strcmp("-v", flag)) {
      version();
      return false;
    } else if (!strcmp("--", flag)) {
      arg++;
      break;
    } else if (!strncmp("--", flag, 2) || !strncmp("-", flag, 1)) {
      fprintf(stderr, "Invalid usage: unknown option %s\n", flag);
      goto error;
    } else {
      break;
    }
  }

  if (arg_end > arg + 1) {
    usage();
    goto error;
  }

  if (arg_end == arg) {
    config->path = NULL;
  } else {
    config->path = *arg;
  }

  return true;
 error:
  *error = true;
  return false;
}

int main(int argc, char *argv[]) {
  int ret = EXIT_SUCCESS;
  char *path = NULL;
  char *cwd_path = NULL;
  char *compacted_path = NULL;
  struct palette *default_path_palette;
  struct palette *default_separator_palette;
  struct config config = {
    .path = NULL,
    .separator = "/",
    .path_palette = NULL,
    .separator_palette = NULL,
    .path_overrides = {0},
    .separator_overrides = {0},
    .new_line = true,
    .bash_escape = false,
    .compact = false,
    .skip_leading = false,
    .path_indexer = index_sequential,
    .separator_indexer = index_sequential,
  };

  setup_random();

  if (!setup_terminal()) {
    ret = EXIT_FAILURE;
    goto out;
  }

  if (get_color_count() >= 256) {
    default_path_palette = &PATH_PALETTE_256;
    default_separator_palette = &SEPARATOR_PALETTE_256;
  } else {
    default_path_palette = &PATH_PALETTE_8;
    default_separator_palette = &SEPARATOR_PALETTE_8;
  }

  bool error;
  if (!parse_args(argc, argv, &config, &error)) {
    if (error) {
      ret = EXIT_FAILURE;
    }
    goto out;
  }

  path = config.path;
  if (!config.path) {
    cwd_path = get_working_directory();
    if (!cwd_path) {
      fputs("Failed to get working directory\n", stderr);
      ret = EXIT_FAILURE;
      goto out;
    }
    path = cwd_path;
  }

  if (config.compact) {
    compacted_path = compact_path(path);
    if (!compacted_path) {
      fputs("Failed to get home directory\n", stderr);
      ret = EXIT_FAILURE;
      goto out;
    }
    path = compacted_path;
  }

  if (config.skip_leading) {
    path = strip_leading(path);
  }

  size_t segment_count, separator_count;
  path_components(path, &segment_count, &separator_count);

  if (!override_normalize(&config.path_overrides, segment_count)) {
    ret = EXIT_FAILURE;
    goto out;
  }

  if (!override_normalize(&config.separator_overrides, separator_count)) {
    ret = EXIT_FAILURE;
    goto out;
  }

  print_path(path,
             config.separator,
             config.path_palette ? config.path_palette : default_path_palette,
             config.separator_palette ? config.separator_palette : default_separator_palette,
             &config.path_overrides,
             &config.separator_overrides,
             config.path_indexer,
             config.separator_indexer,
             config.bash_escape);

  if (config.new_line) {
    fputc('\n', stdout);
  }

out:
  if (config.path_palette) {
    palette_free(config.path_palette);
  }

  if (config.separator_palette) {
    palette_free(config.separator_palette);
  }

  override_free(&config.path_overrides);

  override_free(&config.separator_overrides);

  if (cwd_path) {
    free(cwd_path);
  }

  if (compacted_path) {
    free(compacted_path);
  }

  return ret;
}
