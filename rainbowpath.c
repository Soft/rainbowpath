#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static const char PATH_SEP = '/';
static const uint8_t PATH_SEP_COLOR = 239;
static const uint8_t PALETTE[] =
  { 21, 56, 89, 160, 202, 205, 201, 165, 135, 69 };
static const size_t INITIAL_BUFFER_SIZE = 32;
static const size_t INITIAL_PATH_SIZE = 512;

static const char *USAGE =
  "Usage: rainbowpath [-p PALETTE] [-s COLOR] [-n] [-b] [-h] [PATH]\n"
  "Color path components using a palette.\n\n"
  "Options:\n"
  "  -p PALETTE  Comma-separated list of colors for path components\n"
  "              Colors are represented as numbers between 0 and 255\n"
  "  -s COLOR    Color for path separators\n"
  "  -n          Do not append newline\n"
  "  -b          Escape color codes for use in Bash prompts\n"
  "  -h          Display this help\n";

static inline void *check(void *ptr) {
  if (!ptr) {
    fputs("Failed to allocate memory\n", stderr);
    abort();
  }
  return ptr;
}

static inline void begin_color(uint8_t color,
                               bool bash_escape) {
  printf("%s\e[38;5;%" PRIu8 "m%s",
         bash_escape ? "\\[" : "",
         color,
         bash_escape ? "\\]" : "");
}

static inline void end_color(bool bash_escape) {
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

static void print_path(const char *path,
                       const uint8_t sep_color,
                       const uint8_t palette[],
                       const size_t palette_size,
                       bool bash_escape) {
  const char *rest = path, *ptr;
  size_t ind = 0;
  while ((ptr = strchr(rest, PATH_SEP))) {
    if (ptr != rest) {
      begin_color(palette[ind % palette_size], bash_escape);
      fwrite(rest, 1, ptr - rest, stdout);
      ind++;
    }
    begin_color(sep_color, bash_escape);
    fputc(PATH_SEP, stdout);
    rest = ptr + 1;
  }
  begin_color(palette[ind % palette_size], bash_escape);
  fputs(rest, stdout);
  end_color(bash_escape);
}

static size_t parse_palette(const char *input,
                            uint8_t **palette) {
  char *tmp = check(strdup(input)), *cur, *state;
  size_t pos = 0, buf_size = INITIAL_BUFFER_SIZE;
  uint8_t color;
  uint8_t *result = check(malloc(sizeof(uint8_t) * buf_size));
  for (cur = strtok_r(tmp, ",", &state); cur; cur = strtok_r(NULL, ",", &state)) {
    if (pos >= buf_size) {
      buf_size *= 2;
      result = check(realloc(result, sizeof(uint8_t) * buf_size));
    }
    if (sscanf(cur, "%" SCNu8, &color) == 1) {
      *(result + pos) = color;
      pos++;
    } else {
      free(result);
      free(tmp);
      return 0;
    }
  }
  free(tmp);
  *palette = result;
  return pos;
}

static inline void usage() {
  fputs(USAGE, stderr);
}

int main(int argc, char *argv[]) {
  int arg, ret = EXIT_SUCCESS;
  bool new_line = true, bash_escape = false;
  size_t palette_size = sizeof(PALETTE) / sizeof(uint8_t);
  uint8_t path_sep = PATH_SEP_COLOR;
  char *path = NULL;
  uint8_t *palette = NULL;

  while ((arg = getopt(argc, argv, "p:s:nbh")) != -1) {
    switch (arg) {
    case 'p':
      if ((palette_size = parse_palette(optarg, &palette)) == 0) {
        fputs("Invalid palette\n", stderr);
        ret = EXIT_FAILURE;
        goto out;
      }
      break;
    case 's':
      if (sscanf(optarg, "%" SCNu8, &path_sep) != 1) {
        fputs("Invalid color\n", stderr);
        ret = EXIT_FAILURE;
        goto out;
      }
      break;
    case 'n':
      new_line = false;
      break;
    case 'b':
      bash_escape = true;
      break;
    case 'h':
      usage();
      ret = EXIT_FAILURE;
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

  print_path(path ? path : argv[optind],
             path_sep,
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
  return ret;
}


