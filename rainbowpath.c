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
static const char *RESET = "\e[0m";
static const size_t INITIAL_BUFFER_SIZE = 32;
static const size_t INITIAL_PATH_SIZE = 512;

static inline void *check(void *ptr) {
  if (!ptr) {
    fputs("Failed to allocate memory\n", stderr);
    abort();
  }
  return ptr;
}

static inline void begin_color(uint8_t color) {
  printf("\e[38;5;%" PRIu8 "m", color);
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
                       const size_t palette_size) {
  const char *rest = path, *ptr;
  size_t ind = 0;
  while ((ptr = strchr(rest, PATH_SEP))) {
    if (ptr != rest) {
        begin_color(palette[ind % palette_size]);
        fwrite(rest, 1, ptr - rest, stdout);
        ind++;
    }
    begin_color(sep_color);
    fputc('/', stdout);
    rest = ptr + 1;
  }
  begin_color(palette[ind % palette_size]);
  fputs(rest, stdout);
  fputs(RESET, stdout);
}

static size_t parse_palette(const char *input,
                            const uint8_t **palette) {
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

static void usage() {
  fputs("Invalid usage: rainbowpath [-p PALETTE] [-s COLOR] [-n] [-h] [PATH]\n", stderr);
}

int main(int argc, char *argv[]) {
  int arg;
  bool new_line = true;
  size_t palette_size = sizeof(PALETTE) / sizeof(uint8_t);
  const uint8_t *palette = PALETTE;
  uint8_t path_sep = PATH_SEP_COLOR;
  const char *path;
  while ((arg = getopt(argc, argv, "p:s:nh")) != -1) {
    switch (arg) {
    case 'p':
      if ((palette_size = parse_palette(optarg, &palette)) == 0) {
        fputs("Invalid palette\n", stderr);
        return EXIT_FAILURE;
      }
      break;
    case 's':
      if (sscanf(optarg, "%" SCNu8, &path_sep) != 1) {
        fputs("Invalid color\n", stderr);
        return EXIT_FAILURE;
      }
      break;
    case 'n':
      new_line = false;
      break;
    case 'h':
      usage();
      return EXIT_SUCCESS;
    default:
      usage();
      return EXIT_FAILURE;
    }
  }

  if (optind == argc - 1) {
    path = argv[optind];
  } else if (optind == argc) {
    if ((path = get_working_directory()) == NULL) {
      fputs("Failed to get working directory\n", stderr);
      return EXIT_FAILURE;
    }
  } else {
    usage();
    return EXIT_FAILURE;
  }

  print_path(path, path_sep, palette, palette_size);
  if (new_line) {
    fputc('\n', stdout);
  }

  return EXIT_SUCCESS;
}


