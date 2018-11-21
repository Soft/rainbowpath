#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <assert.h>

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

static const char PATH_SEP = '/';
static const struct style PATH_SEP_STYLE =
  { .fg = { .set = true, .color = 239 }, .bold = true };
static const struct style PALETTE[] = {
  { .fg = { .set = true, .color = 160 } },
  { .fg = { .set = true, .color = 208 } },
  { .fg = { .set = true, .color = 220 } },
  { .fg = { .set = true, .color = 82 } },
  { .fg = { .set = true, .color = 39 } },
  { .fg = { .set = true, .color = 63 } }
};

static const size_t INITIAL_PALETTE_SIZE = 32;
static const size_t INITIAL_PATH_SIZE = 512;

static const char *USAGE =
  "Usage: " PACKAGE_NAME " [-p PALETTE] [-s COLOR] [-c] [-n] [-b] [-h] [-v] [PATH]\n"
  "Color path components using a palette.\n\n"
  "Options:\n"
  "  -p PALETTE  Comma-separated list of colors for path components\n"
  "              Colors are represented as numbers between 0 and 255\n"
  "  -s COLOR    Color for path separators\n"
  "  -c          Replace home directory path prefix with ~\n"
  "  -n          Do not append newline\n"
  "  -b          Escape color codes for use in Bash prompts\n"
  "  -h          Display this help\n"
  "  -v          Display version information\n";

static inline void *check(void *ptr) {
  if (!ptr) {
    fputs("Failed to allocate memory\n", stderr);
    abort();
  }
  return ptr;
}

static void begin_style(const struct style *style,
                        bool bash_escape) {
  if (bash_escape)
    fputs("\\[", stdout);
  if (style->bold)
    fputs("\e[1m", stdout);
  if (style->dim)
    fputs("\e[2m", stdout);
  if (style->underlined)
    fputs("\e[4m", stdout);
  if (style->blink)
    fputs("\e[5m", stdout);
  if (style->bg.set)
    printf("\e[48;5;%" PRIu8 "m", style->bg.color);
  if (style->fg.set)
    printf("\e[38;5;%" PRIu8 "m", style->fg.color);
  if (bash_escape)
    fputs("\\]", stdout);
}

static inline void end_style(bool bash_escape) {
  printf("%s\e[0m%s",
         bash_escape ? "\\[" : "",
         bash_escape ? "\\]" : "");
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
    if (next == PATH_SEP || next == '\0') {
      const size_t result_size = (path_len - home_len + 2) * sizeof(char);
      char *result = check(malloc(result_size));
      strncpy(result, "~", result_size);
      strncat(result, path + home_len, result_size - 1);
      return result;
    }
  }
  return check(strdup(path));
}

static void print_path(const char *path,
                       const struct style *sep_style,
                       const struct style *palette,
                       const size_t palette_size,
                       bool bash_escape) {
  const char *rest = path, *ptr;
  size_t ind = 0;
  while ((ptr = strchr(rest, PATH_SEP))) {
    if (ptr != rest) {
      begin_style(&palette[ind % palette_size], bash_escape);
      fwrite(rest, 1, ptr - rest, stdout);
      end_style(bash_escape);
      ind++;
    }
    begin_style(sep_style, bash_escape);
    fputc(PATH_SEP, stdout);
    end_style(bash_escape);
    rest = ptr + 1;
  }
  begin_style(&palette[ind % palette_size], bash_escape);
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
  while (**input && isspace(**input))
    (*input)++;
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

static const char *parse_color_assignment(const char *input,
                                          uint8_t *result) {
  const char *next, *current;
  char *value = NULL, *num_end;
  unsigned long num;
  if ((next = parse_char(input, '='))) {
    current = next;
    if ((next = parse_token(current, &value))) {
      num = strtoul(value, &num_end, 10);
      if (num == 0 && num_end == value)
        goto error;;
      if (num > UINT8_MAX)
        goto error;
      if (*num_end != '\0')
        goto error;
      *result = (uint8_t)num;
    }
  }
  return next;
 error:
  if (value)
    free(value);
  return NULL;
}

static const char *parse_style_inner(const char *input,
                                     struct style *style) {
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
          fputs("foreground color expected\n", stderr);
          goto error;
        }
      } else if (strcmp(key, "bg") == 0) {
        if ((next = parse_color_assignment(current, &color))) {
          current = next;
          style->bg.set = true;
          style->bg.color = color;
        } else {
          fputs("background color expected\n", stderr);
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
        fprintf(stderr, "unknown property '%s'\n", key);
        goto error;
      }
      free(key);
      key = NULL;
    } else {
      fputs("expected a property\n", stderr);
      goto error;
    }
    if ((next = parse_char(current, ','))) {
      current = next;
    } else {
      return current;
    }
  }
 error:
  if (key)
    free(key);
  return NULL;
}

static bool parse_style(const char *input,
                        struct style *style) {
  const char *next;
  if ((next = parse_style_inner(input, style))) {
    if ((next = peek_char(next, '\0'))) {
      return true;
    } else {
      fputs("expected eof\n", stderr);
    }
  } else {
      fputs("expected style\n", stderr);
  }
  return false;
}

static size_t parse_palette(const char *input,
                            struct style **result) {
  const char *current = input, *next;
  size_t palette_size = INITIAL_PALETTE_SIZE, pos = 0;
  struct style *palette = check(calloc(palette_size, sizeof(*palette)));

  while (true) {
    if ((next = parse_style_inner(current, &palette[pos]))) {
      current = next;
    } else {
      fputs("expected a style definition\n", stderr);
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
      fputs("expected ';'\n", stderr);
      goto error;
    }
  }

  *result = palette;
  return pos + 1;
 error:
  if (palette)
    free(palette);
  return 0;
}

static inline void usage(void) {
  fputs(USAGE, stderr);
}

static inline void version(void) {
  fputs(PACKAGE_STRING "\n", stderr);
}

int main(int argc, char *argv[]) {
  int arg, ret = EXIT_SUCCESS;
  bool new_line = true, bash_escape = false, compact = false;
  char *path = NULL, *compacted = NULL, *selected;
  size_t palette_size = sizeof(PALETTE) / sizeof(struct style);
  struct style path_sep = PATH_SEP_STYLE;
  struct style *palette = NULL;

  while ((arg = getopt(argc, argv, "p:s:cnbhv")) != -1) {
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

  print_path(selected,
             &path_sep,
             palette ? palette : PALETTE,
             palette_size,
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


