#include "rainbowpath.h"
#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
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

typedef size_t (*indexer)(
  size_t palette_size,
  size_t ind,
  const char *start,
  const char *end
);

static const struct style PATH_SEP_STYLE_8 = {
  .fg = { .set = true, .color = 7 },
  .dim = true
};

static const struct style PALETTE_8[] = {
  { .fg = { .set = true, .color = 1 } },
  { .fg = { .set = true, .color = 3 } },
  { .fg = { .set = true, .color = 2 } },
  { .fg = { .set = true, .color = 6 } },
  { .fg = { .set = true, .color = 4 } },
  { .fg = { .set = true, .color = 5 } }
};

static const struct style PATH_SEP_STYLE_256 = {
  .fg = { .set = true, .color = 239 },
  .bold = true
};

static const struct style PALETTE_256[] = {
  { .fg = {.set = true, .color = 160 } },
  { .fg = {.set = true, .color = 208 } },
  { .fg = {.set = true, .color = 220 } },
  { .fg = {.set = true, .color = 82 } },
  { .fg = {.set = true, .color = 39 } },
  { .fg = {.set = true, .color = 63 } }
};

static const size_t INITIAL_PALETTE_SIZE = 32;
static const size_t INITIAL_PATH_SIZE = 512;

static const char *USAGE =
    "Usage: " PACKAGE_NAME " [-p PALETTE] [-s STYLE] [-S SEPARATOR] [-m METHOD]\n"
    "                   [-l] [-c] [-n] [-b] [-h] [-v] [PATH]\n\n"
    "Color path components using a palette.\n\n"
    "Options:\n"
    "  -p, --palette=PALETTE             Semicolon-separated list of styles for\n"
    "                                    path components\n"
    "  -s, --separator=STYLE             Style for path separators\n"
    "  -S, --separator-string=SEPARATOR  String used to separate path components\n"
    "                                    in the output (defaults to '/')\n"
    "  -m, --method=METHOD               Method for selecting styles from palette.\n"
    "                                    One of sequential, hash, random\n"
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

static void print_path(indexer indexer,
                       const char *path,
                       const char *separator,
                       const struct style *sep_style,
                       const struct style *palette,
                       size_t palette_size,
                       bool skip_leading,
                       bool bash_escape) {
  const char *rest = path, *ptr;
  size_t ind = 0, selected;
  if (skip_leading && *rest == '/') {
    rest++;
  }
  while ((ptr = strchr(rest, '/'))) {
    if (ptr != rest) {
      selected = indexer(palette_size, ind, rest, ptr);
      begin_style(&palette[selected], bash_escape);
      fwrite(rest, 1, ptr - rest, stdout);
      end_style(bash_escape);
      ind++;
    }
    begin_style(sep_style, bash_escape);
    fputs(separator, stdout);
    end_style(bash_escape);
    rest = ptr + 1;
  }
  selected = indexer(palette_size, ind, rest, rest + strlen(rest));
  begin_style(&palette[selected], bash_escape);
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

static size_t parse_palette(const char *input, struct style **result) {
  const char *current = input, *next;
  size_t palette_size = INITIAL_PALETTE_SIZE, pos = 0;
  struct style *palette = check(calloc(palette_size, sizeof(*palette)));

  while (true) {
    if ((next = parse_style_inner(current, &palette[pos]))) {
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
        palette = check(realloc(palette, sizeof(*palette) * palette_size));
      }
      memset(&palette[pos], 0, sizeof(struct style));
    } else if ((next = peek_char(current, '\0'))) {
      current = next;
      break;
    } else {
      fputs("Expected ';'\n", stderr);
      goto error;
    }
  }

  *result = palette;
  return pos + 1;
error:
  if (palette) {
    free(palette);
  }
  return 0;
}

static void select_palette(const struct style **palette,
                           size_t *palette_size,
                           struct style *path_sep) {
  if (get_color_count() >= 256) {
    *palette_size = ARRAY_SIZE(PALETTE_256);
    *palette = PALETTE_256;
    *path_sep = PATH_SEP_STYLE_256;
  } else {
    *palette_size = ARRAY_SIZE(PALETTE_8);
    *palette = PALETTE_8;
    *path_sep = PATH_SEP_STYLE_8;
  }
}

static indexer select_indexer(const char *name) {
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
  char seed[sizeof(SEED_TYPE)] = {0};
  if (getrandom(&seed[0], sizeof(seed), 0) == sizeof(seed)) {
    INIT_FUNC(*(SEED_TYPE *)&seed[0]);
  } else {
    INIT_FUNC(time(NULL));
  }
#else
  INIT_FUNC(time(NULL));
#endif
#undef INIT_FUNC
#undef SEED_TYPE
}

static inline void usage(void) {
  fputs(USAGE, stderr);
}

static inline void version(void) {
  fputs(PACKAGE_STRING "\n", stderr);
}

int main(int argc, char *argv[]) {
  int arg, ret = EXIT_SUCCESS;
  bool new_line = true;
  bool bash_escape = false;
  bool compact = false;
  bool skip_leading = false;
  char *path = NULL;
  char *compacted = NULL;
  char *selected;
  char *separator = "/";
  indexer indexer = index_sequential;
  size_t palette_size;
  struct style path_sep;
  const struct style *default_palette;
  struct style *palette = NULL;

  setup_random();

  if (!setup_terminal()) {
    ret = EXIT_FAILURE;
    goto out;
  }

  select_palette(&default_palette, &palette_size, &path_sep);

  static const struct option options[] = {
      { "palette", required_argument, NULL, 'p' },
      { "separator", required_argument, NULL, 's' },
      { "separator-string", required_argument, NULL, 'S' },
      { "method", required_argument, NULL, 'm' },
      { "leading", no_argument, NULL, 'l' },
      { "compact", no_argument, NULL, 'c' },
      { "newline", no_argument, NULL, 'n' },
      { "bash", no_argument, NULL, 'b' },
      { "help", no_argument, NULL, 'h' },
      { "version", no_argument, NULL, 'v' },
      { 0 }
  };

  while ((arg = getopt_long(argc, argv, "p:s:S:m:lcnbhv", options, NULL)) != -1) {
    switch (arg) {
      case 'p':
        if ((palette_size = parse_palette(optarg, &palette)) == 0) {
          fputs("Invalid palette\n", stderr);
          ret = EXIT_FAILURE;
          goto out;
        }
        break;
      case 's':
        if (!parse_style(optarg, &path_sep)) {
          fputs("Invalid separator style\n", stderr);
          ret = EXIT_FAILURE;
          goto out;
        }
        break;
      case 'S':
        separator = optarg;
        break;
      case 'm':
        indexer = select_indexer(optarg);
        if (!indexer) {
          fputs("Invalid indexing method\n", stderr);
          ret = EXIT_FAILURE;
          goto out;
        }
        break;
      case 'l':
        skip_leading = true;
        break;
      case 'c':
        compact = true;
        break;
      case 'n':
        new_line = false;
        break;
      case 'b':
        bash_escape = true;
        break;
      case 'h':
        usage();
        goto out;
      case 'v':
        version();
        goto out;
      default:
        usage();
        ret = EXIT_FAILURE;
        goto out;
    }
  }

  if (optind == argc) {
    if ((path = get_working_directory()) == NULL) {
      fputs("Failed to get working directory\n", stderr);
      ret = EXIT_FAILURE;
      goto out;
    }
  } else if (optind > argc) {
    usage();
    ret = EXIT_FAILURE;
    goto out;
  }

  if (compact) {
    if ((compacted = compact_path(path ? path : argv[optind])) == NULL) {
      fputs("Failed to get home directory\n", stderr);
      ret = EXIT_FAILURE;
      goto out;
    }
    selected = compacted;
  } else {
    selected = path ? path : argv[optind];
  }

  print_path(indexer,
             selected,
             separator,
             &path_sep,
             palette ? palette : default_palette,
             palette_size,
             skip_leading,
             bash_escape);

  if (new_line) {
    fputc('\n', stdout);
  }

out:

  if (palette) {
    free(palette);
  }

  if (path) {
    free(path);
  }

  if (compacted) {
    free(compacted);
  }

  return ret;
}
