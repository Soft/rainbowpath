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
#include <getopt.h>

#ifdef HAVE_CURSES
#include <curses.h>
#include <term.h>
#endif

#define ARRAY_SIZE(array)                       \
  (sizeof(array) / sizeof(array[0]))

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
  { .fg = { .set = true, .color = 160 } },
  { .fg = { .set = true, .color = 208 } },
  { .fg = { .set = true, .color = 220 } },
  { .fg = { .set = true, .color = 82 } },
  { .fg = { .set = true, .color = 39 } },
  { .fg = { .set = true, .color = 63 } }
};

static const size_t INITIAL_PALETTE_SIZE = 32;
static const size_t INITIAL_PATH_SIZE = 512;

struct terminal {
  int color_count;
  char *fg;
  char *bg;
  char *bold;
  char *dim;
  char *underlined;
  char *blink;
  char *reset;
};

static const char *USAGE =
  "Usage: " PACKAGE_NAME " [-p PALETTE] [-s STYLE] [-S SEPARATOR] [-l] [-c] [-n] [-b] [-h] [-v] [PATH]\n"
  "Color path components using a palette.\n\n"
  "Options:\n"
  "  -p, --palette=PALETTE             Semicolon-separated list of styles for\n"
  "                                    path components\n"
  "  -s, --separator=STYLE             Style for path separators\n"
  "  -S, --separator-string=SEPARATOR  String used to separate path components\n"
  "                                    in the output (defaults to '/')\n"
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

#ifdef HAVE_CURSES

static inline void begin_attr(const char *code) {
  putp(code);
}

static inline void begin_color(const char *code, uint8_t color) {
  char *result = tiparm(code, color);
  putp(result);
}

#else

static inline void begin_attr(const char *code) {
  fputs(code, stdout);
}

static inline void begin_color(const char *code, uint8_t color) {
  fprintf(stdout, code, color);
}

#endif

static void begin_style(const struct terminal *terminal,
                        const struct style *style,
                        bool bash_escape) {
  if (bash_escape) {
    fputs("\\[", stdout);
  }
  if (style->bold) {
    begin_attr(terminal->bold);
  }
  if (style->dim) {
    begin_attr(terminal->dim);
  }
  if (style->underlined) {
    begin_attr(terminal->underlined);
  }
  if (style->blink) {
    begin_attr(terminal->blink);
  }
  if (style->bg.set) {
    begin_color(terminal->bg, style->bg.color);
  }
  if (style->fg.set) {
    begin_color(terminal->fg, style->fg.color);
  }
  if (bash_escape) {
    fputs("\\]", stdout);
  }
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

static void print_path(const struct terminal *terminal,
                       const char *path,
                       const char *separator,
                       const struct style *sep_style,
                       const struct style *palette,
                       const size_t palette_size,
                       bool skip_leading,
                       bool bash_escape) {
  const char *rest = path, *ptr;
  size_t ind = 0;
  if (skip_leading && *rest == '/') {
    rest++;
  }
  while ((ptr = strchr(rest, '/'))) {
    if (ptr != rest) {
      begin_style(terminal, &palette[ind % palette_size], bash_escape);
      fwrite(rest, 1, ptr - rest, stdout);
      end_style(bash_escape);
      ind++;
    }
    begin_style(terminal, sep_style, bash_escape);
    fputs(separator, stdout);
    end_style(bash_escape);
    rest = ptr + 1;
  }
  begin_style(terminal, &palette[ind % palette_size], bash_escape);
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

static const char *parse_color_assignment(const char *input,
                                          uint8_t *result) {
  const char *next, *current;
  char *value = NULL, *num_end;
  unsigned long num;
  if ((next = parse_char(input, '='))) {
    current = next;
    if ((next = parse_token(current, &value))) {
      num = strtoul(value, &num_end, 10);
      if (num == 0 && num_end == value) {
        goto error;
      };
      if (num > UINT8_MAX) {
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

static bool parse_style(const char *input,
                        struct style *style) {
  const char *next;
  if ((next = parse_style_inner(input, style))) {
    if ((next = peek_char(next, '\0'))) {
      return true;
    } else {
      fputs("Expected eof\n", stderr);
    }
  } else {
    fputs("Expected style\n", stderr);
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

#ifdef HAVE_CURSES

static bool setup_terminal(struct terminal *terminal) {
  int err, int_value;
  char *str_value;

  if (setupterm(NULL, STDOUT_FILENO, &err) == ERR) {
    switch (err) {
    case 1:
      fputs("Invalid terminal type\n", stderr);
      return false;
    case 0:
      fputs("Failed to determine terminal type\n", stderr);
      return false;
    case -1:
      fputs("Failed to access terminfo database\n", stderr);
      return false;
    default:
      fputs("Unknown error\n", stderr);
      return false;
    }
  }

  if ((int_value = tigetnum("colors")) < 0) {
    return false;
  }
  terminal->color_count = int_value;

  if ((str_value = tigetstr("setaf")) < (char *)0) {
    return false;
  }
  terminal->fg = str_value;

  if ((str_value = tigetstr("setab")) < (char *)0) {
    return false;
  }
  terminal->bg = str_value;

  if ((str_value = tigetstr("bold")) < (char *)0) {
    return false;
  }
  terminal->bold = str_value;

  if ((str_value = tigetstr("dim")) < (char *)0) {
    return false;
  }
  terminal->dim = str_value;

  if ((str_value = tigetstr("smul")) < (char *)0) {
    return false;
  }
  terminal->underlined = str_value;

  if ((str_value = tigetstr("blink")) < (char *)0) {
    return false;
  }
  terminal->blink = str_value;

  if ((str_value = tigetstr("sgr0")) < (char *)0) {
    return false;
  }
  terminal->reset = str_value;

  return true;
}

#else

static bool setup_terminal(struct terminal *terminal) {
  terminal->color_count = 256;
  terminal->fg = "\e[38;5;%" PRIu8 "m";
  terminal->bg = "\e[48;5;%" PRIu8 "m";
  terminal->bold = "\e[1m";
  terminal->dim = "\e[2m";
  terminal->underlined = "\e[4m";
  terminal->blink = "\e[5m";
  terminal->reset = "\e[0m";
  return true;
}

#endif

static inline void usage(void) {
  fputs(USAGE, stderr);
}

static inline void version(void) {
  fputs(PACKAGE_STRING "\n", stderr);
}

int main(int argc, char *argv[]) {
  int arg, ret = EXIT_SUCCESS;
  bool new_line = true, bash_escape = false, compact = false, skip_leading = false;
  char *path = NULL, *compacted = NULL, *selected, *separator = "/";
  size_t palette_size;
  struct style path_sep;
  const struct style *default_palette;
  struct style *palette = NULL;
  struct terminal terminal;

  if (!setup_terminal(&terminal)) {
    ret = EXIT_FAILURE;
    goto out;
  }

  if (terminal.color_count >= 256) {
    palette_size = ARRAY_SIZE(PALETTE_256);
    default_palette = PALETTE_256;
    path_sep = PATH_SEP_STYLE_256;
  } else {
    palette_size = ARRAY_SIZE(PALETTE_8);
    default_palette = PALETTE_8;
    path_sep = PATH_SEP_STYLE_8;
  }

  static const struct option options[] = {
    { "palette",          required_argument, NULL, 'p' },
    { "separator",        required_argument, NULL, 's' },
    { "separator-string", required_argument, NULL, 'S' },
    { "leading",          no_argument,       NULL, 'l' },
    { "compact",          no_argument,       NULL, 'c' },
    { "newline",          no_argument,       NULL, 'n' },
    { "bash",             no_argument,       NULL, 'b' },
    { "help",             no_argument,       NULL, 'h' },
    { "version",          no_argument,       NULL, 'v' },
    { 0 }
  };

  while ((arg = getopt_long(argc, argv, "p:s:S:lcnbhv", options, NULL)) != -1) {
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

  print_path(&terminal,
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

