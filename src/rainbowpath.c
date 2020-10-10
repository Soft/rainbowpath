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

struct maybe_color {
  bool set;
  uint8_t color;
};

struct style {
  struct maybe_color fg;
  struct maybe_color bg;
  bool bold;
  bool dim;
  bool underlined;
  bool blink;
};

struct palette {
  struct style *styles;
  size_t size;
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
  bool new_line;
  bool bash_escape;
  bool compact;
  bool skip_leading;
  indexer_t path_indexer;
  indexer_t separator_indexer;
};

static struct style SEPARATOR_STYLES_8[] = {
  {
    .fg = { .set = true, .color = 7 },
    .dim = true
  }
};

static struct palette SEPARATOR_PALETTE_8 = {
  .styles = SEPARATOR_STYLES_8,
  .size = ARRAY_SIZE(SEPARATOR_STYLES_8),
};

static struct style PATH_STYLES_8[] = {
  { .fg = { .set = true, .color = 1 } },
  { .fg = { .set = true, .color = 3 } },
  { .fg = { .set = true, .color = 2 } },
  { .fg = { .set = true, .color = 6 } },
  { .fg = { .set = true, .color = 4 } },
  { .fg = { .set = true, .color = 5 } }
};

static struct palette PATH_PALETTE_8 = {
  .styles = PATH_STYLES_8,
  .size = ARRAY_SIZE(PATH_STYLES_8),
};

static struct style SEPARATOR_STYLES_256[] = {
  {
    .fg = { .set = true, .color = 239 },
    .bold = true,
  }
};

static struct palette SEPARATOR_PALETTE_256 = {
  .styles = SEPARATOR_STYLES_256,
  .size = ARRAY_SIZE(SEPARATOR_STYLES_256),
};

static struct style PATH_STYLES_256[] = {
  { .fg = {.set = true, .color = 160 } },
  { .fg = {.set = true, .color = 208 } },
  { .fg = {.set = true, .color = 220 } },
  { .fg = {.set = true, .color = 82 } },
  { .fg = {.set = true, .color = 39 } },
  { .fg = {.set = true, .color = 63 } },
};

static struct palette PATH_PALETTE_256 = {
  .styles = PATH_STYLES_256,
  .size = ARRAY_SIZE(PATH_STYLES_256),
};

static const size_t INITIAL_PALETTE_SIZE = 32;
static const size_t INITIAL_PATH_SIZE = 512;

static const char *USAGE =
    "Usage: " PACKAGE_NAME " [-p PALETTE] [-s PALETTE] [-S SEPARATOR] [-m METHOD]\n"
    "                   [-M METHOD] [-l] [-c] [-n] [-b] [-h] [-v] [PATH]\n\n"
    "Color path components using a palette.\n\n"
    "Options:\n"
    "  -p, --path-palette PALETTE        Semicolon separated list of styles for\n"
    "                                    path components\n"
    "  -s, --separator-palette PALETTE   Semicolon separated list of styles for\n"
    "                                    path separators.\n"
    "  -S, --separator SEPARATOR         String used to separate path components\n"
    "                                    in the output (defaults to '/')\n"
    "  -m, --path-method METHOD          Method for selecting styles from palette.\n"
    "                                    One of sequential, hash, random\n"
    "                                    (defaults to sequential).\n"
    "  -M, --separator-method METHOD     Method for selecting separator styles from\n"
    "                                    palette. One of sequential, hash, random\n"
    "                                    (defaults to sequential).\n"
    "  -l, --leading                     Do not display leading path separator\n"
    "  -c, --compact                     Replace home directory path prefix with ~\n"
    "  -n, --newline                     Do not append newline\n"
    "  -b, --bash                        Escape control codes for use in Bash prompts\n"
    "  -h, --help                        Display this help\n"
    "  -v, --version                     Display version information\n";

static inline void *check(void *ptr) {
  if (!ptr) {
    fputs("Failed to allocate memory\n", stderr);
    abort();
  }
  return ptr;
}

static void begin_style(const struct style *style, bool bash_escape) {
  if (bash_escape) {
    fputs("\\[", stdout);
  }
  if (style->bold) {
    begin_bold();
  }
  if (style->dim) {
    begin_dim();
  }
  if (style->underlined) {
    begin_underlined();
  }
  if (style->blink) {
    begin_blink();
  }
  if (style->bg.set) {
    begin_bg(style->bg.color);
  }
  if (style->fg.set) {
    begin_fg(style->fg.color);
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

static void palette_free(struct palette *palette) {
  if (palette->styles) {
    free(palette->styles);
  }
  free(palette);
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
                               const char *start,
                               const char *end) {
  return ind % palette_size;
}

static size_t index_hash(size_t palette_size,
                         size_t ind,
                         const char *start,
                         const char *end) {
  size_t hash = 5381;
  for (const char *c = start; c < end; c++) {
    hash = ((hash << 5) + hash) + (size_t)*c;
  }
  return hash % palette_size;
}

static size_t index_random(size_t palette_size,
                           size_t ind,
                           const char *start,
                           const char *end) {
  #ifdef HAVE_DRAND48
  return drand48() * palette_size;
  #else
  return rand() % palette_size;
  #endif
}

static void print_path(const char *path,
                       const char *separator,
                       const struct palette *path_palette,
                       const struct palette *separator_palette,
                       bool skip_leading,
                       bool bash_escape,
                       indexer_t path_indexer,
                       indexer_t separator_indexer) {
  const char *rest = path;
  const char *ptr;
  size_t path_ind = 0;
  size_t sep_ind = 0;
  size_t selected;

  if (skip_leading && *rest == '/') {
    rest++;
  }

  while ((ptr = strchr(rest, '/'))) {
    if (ptr != rest) {
      selected = path_indexer(path_palette->size, path_ind, rest, ptr);
      begin_style(&path_palette->styles[selected], bash_escape);
      fwrite(rest, 1, ptr - rest, stdout);
      end_style(bash_escape);
      path_ind++;
    }

    selected = separator_indexer(separator_palette->size, sep_ind, ptr, ptr + 1);
    begin_style(&separator_palette->styles[selected], bash_escape);
    fputs(separator, stdout);
    end_style(bash_escape);
    sep_ind++;

    rest = ptr + 1;
  }

  selected = path_indexer(path_palette->size, path_ind, rest, rest + strlen(rest));
  begin_style(&path_palette->styles[selected], bash_escape);
  fputs(rest, stdout);
  end_style(bash_escape);
}

static inline char *make_string(const char *begin, const char *end) {
  assert(end >= begin);
  size_t size = end - begin;
  char *result = check(calloc(size + 1, sizeof(char)));
  return memcpy(result, begin, size);
}

static inline void consume_whitespace(const char **input) {
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

static inline const char *parse_char(const char *input, char c) {
  consume_whitespace(&input);
  return *input == c ? ++input : NULL;
}

static inline const char *peek_char(const char *input, char c) {
  consume_whitespace(&input);
  return *input == c ? input : NULL;
}

static const char *parse_color_assignment(const char *input, uint8_t *result) {
  const char *next, *current;
  char *value = NULL, *num_end;
  unsigned long num;
  int max_color = get_color_count() - 1;
  max_color = MAX(max_color, 0);
  if ((next = parse_char(input, '='))) {
    current = next;
    if ((next = parse_token(current, &value))) {
      num = strtoul(value, &num_end, 10);
      if (num == 0 && num_end == value) {
        goto error;
      };
      if (num > MIN(UINT8_MAX, max_color)) {
        fputs("Color outside acceptable range", stderr);
        goto error;
      }
      if (*num_end != '\0') {
        goto error;
      }
      *result = (uint8_t)num;
    }
  }
  return next;
error:
  if (value) {
    free(value);
  }
  return NULL;
}

static const char *parse_style_inner(const char *input, struct style *style) {
  const char *current = input, *next;
  char *key = NULL;
  uint8_t color;
  memset(style, 0, sizeof(struct style));
  while (true) {
    if ((next = parse_token(current, &key))) {
      current = next;
      if (strcmp(key, "fg") == 0) {
        if ((next = parse_color_assignment(current, &color))) {
          current = next;
          style->fg.set = true;
          style->fg.color = color;
        } else {
          fputs("Foreground color expected\n", stderr);
          goto error;
        }
      } else if (strcmp(key, "bg") == 0) {
        if ((next = parse_color_assignment(current, &color))) {
          current = next;
          style->bg.set = true;
          style->bg.color = color;
        } else {
          fputs("Background color expected\n", stderr);
          goto error;
        }
      } else if (strcmp(key, "bold") == 0) {
        style->bold = true;
      } else if (strcmp(key, "dim") == 0) {
        style->dim = true;
      } else if (strcmp(key, "underlined") == 0) {
        style->underlined = true;
      } else if (strcmp(key, "blink") == 0) {
        style->blink = true;
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
  #define INIT_FUNC srand48
  #define SEED_TYPE long int
#else
  #define INIT_FUNC srand
  #define SEED_TYPE unsigned int
#endif
#ifdef HAVE_GETRANDOM
  char seed_buf[sizeof(SEED_TYPE)] = {0};
  SEED_TYPE seed = 0;
  if (getrandom(seed_buf, sizeof(seed_buf), 0) == sizeof(seed_buf)) {
    memcpy(&seed, seed_buf, sizeof(seed));
    INIT_FUNC(seed);
  } else {
    INIT_FUNC(time(NULL));
  }
#else
  INIT_FUNC(time(NULL));
#endif
#undef INIT_FUNC
#undef SEED_TYPE
}

static void usage(void) {
  fputs(USAGE, stderr);
}

static void version(void) {
  fputs(PACKAGE_STRING "\n", stderr);
}

static bool consume_argument(char ***arg, char **arg_end) {
  const char *flag = **arg;
  if (++*arg >= arg_end) {
    fprintf(stderr, "Invalid usage: %s requires an argument\n", flag);
    return false;
  }
  return true;
}

static bool parse_args(int argc,
                       char **argv,
                       struct config *config,
                       bool *error) {
  char **arg = argv + 1;
  char **arg_end = argv + argc;
  size_t palette_size;
  struct palette *palette;
  indexer_t indexer;

  *error = false;
  for (; arg < arg_end; arg++) {
    const char *flag = *arg;
    if (!strcmp("--path-palette", flag) || !strcmp("-p", flag)) {
      if (!consume_argument(&arg, arg_end)) {
        goto error;
      }
      palette = parse_palette(*arg);
      if (!palette) {
        fputs("Invalid palette\n", stderr);
        goto error;
      }
      config->path_palette = palette;
    } else if (!strcmp("--separator-palette", flag) || !strcmp("-s", flag)) {
      if (!consume_argument(&arg, arg_end)) {
        goto error;
      }
      palette = parse_palette(*arg);
      if (!palette) {
        fputs("Invalid separator palette\n", stderr);
        goto error;
      }
      config->separator_palette = palette;
    } else if (!strcmp("--separator", flag) || !strcmp("-S", flag)) {
      if (!consume_argument(&arg, arg_end)) {
        goto error;
      }
      config->separator = *arg;
    } else if (!strcmp("--path-method", flag) || !strcmp("-m", flag)) {
      if (!consume_argument(&arg, arg_end)) {
        goto error;
      }
      indexer = select_indexer(*arg);
      if (!indexer) {
        fputs("Invalid indexing method\n", stderr);
        goto error;
      }
      config->path_indexer = indexer;
    } else if (!strcmp("--separator-method", flag) || !strcmp("-M", flag)) {
      if (!consume_argument(&arg, arg_end)) {
        goto error;
      }
      indexer = select_indexer(*arg);
      if (!indexer) {
        fputs("Invalid indexing method\n", stderr);
        goto error;
      }
      config->separator_indexer = indexer;
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
  const struct palette *default_path_palette;
  const struct palette *default_separator_palette;
  struct config config = {
    .path = NULL,
    .separator = "/",
    .path_palette = NULL,
    .separator_palette = NULL,
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

  print_path(path,
             config.separator,
             config.path_palette ? config.path_palette : default_path_palette,
             config.separator_palette ? config.separator_palette : default_separator_palette,
             config.skip_leading,
             config.bash_escape,
             config.path_indexer,
             config.separator_indexer);

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

  if (cwd_path) {
    free(cwd_path);
  }

  if (compacted_path) {
    free(compacted_path);
  }

  return ret;
}
