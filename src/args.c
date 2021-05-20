#include "build.h"

#include "args.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "indexer.h"
#include "parser_common.h"
#include "style_parser.h"
#include "styles.h"

static const char *USAGE =
    "Usage: " PACKAGE_NAME " [-p PALETTE] [-s PALETTE] [-S SEPARATOR] [-m METHOD]\n"
    "                   [-M METHOD] [-o INDEX STYLE] [-O INDEX STYLE]\n"
    "                   [-l] [-c] [-n] [-b] [-h] [-v] [PATH]\n\n"
    "Color path components using a palette.\n\n"
    "Options:\n"
    "  -p, --palette PALETTE                 Semicolon separated list of styles for\n"
    "                                        path components.\n"
    "  -s, --separator-palette PALETTE       Semicolon separated list of styles for\n"
    "                                        path separators.\n"
    "  -S, --separator SEPARATOR             String used to separate path components\n"
    "                                        in the output (defaults to '/').\n"
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
    "  -l, --strip-leading                   Do not display leading path separator.\n"
    "  -c, --compact                         Replace home directory path prefix with ~.\n"
    "  -n, --newline                         Do not append newline.\n"
    "  -b, --bash                            Escape control codes for use in Bash prompts.\n"
    "  -h, --help                            Display this help.\n"
    "  -v, --version                         Display version information.\n";

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

static bool parse_palette_arg(char ***arg,
                              char **arg_end,
                              struct palette **result,
                              const char *flag) {
  if (!consume_argument(arg, arg_end, flag)) {
    return false;
  }
  struct palette *palette;
  if (!parse_palette_cstr(**arg, &palette)) {
    fputs("Invalid separator palette\n", stderr);
    return false;
  }
  if (*result) {
    palette_free(*result);
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
  indexer = get_indexer(**arg);
  if (!indexer) {
    fputs("Invalid indexing method\n", stderr);
    return false;
  }
  *result = indexer;
  return true;
}

static bool parse_override_arg(char ***arg,
                               char **arg_end,
                               struct list *result,
                               const char *flag) {
  struct override *override = check(malloc(sizeof(*override)));
  if (!consume_argument(arg, arg_end, flag)) {
    goto error;
  }
  if (!parse_ssize(**arg, &override->raw_index)) {
    fputs("Invalid override index\n", stderr);
    goto error;
  }
  if (!consume_argument(arg, arg_end, flag)) {
    goto error;
  }
  if (!parse_style_cstr(**arg, &override->style)) {
    fputs("Invalid override style\n", stderr);
    goto error;
  }
  list_append(result, override);
  return true;
 error:
  free(override);
  return false;
}


bool parse_args(int argc, char **argv, struct config *config, bool *exit) {
  char **arg = argv + 1;
  char **arg_end = argv + argc;

  *exit = false;
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
      config->separator = check(strdup(*arg));
    } else if (!strcmp("--method", flag) || !strcmp("-m", flag)) {
      if (!parse_indexer_arg(&arg, arg_end, &config->path_indexer, flag)) {
        goto error;
      }
    } else if (!strcmp("--separator-method", flag) || !strcmp("-M", flag)) {
      if (!parse_indexer_arg(&arg, arg_end, &config->separator_indexer, flag)) {
        goto error;
      }
    } else if (!strcmp("--override", flag) || !strcmp("-o", flag)) {
      if (!parse_override_arg(&arg, arg_end, config->path_overrides, flag)) {
        goto error;
      }
    } else if (!strcmp("--separator-override", flag) || !strcmp("-O", flag)) {
      if (!parse_override_arg(&arg, arg_end, config->separator_overrides, flag)) {
        goto error;
      }
    } else if (!strcmp("--strip-leading", flag) || !strcmp("-l", flag)) {
      config->strip_leading = true;
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
      *exit = true;
      return true;
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
  return false;
}
