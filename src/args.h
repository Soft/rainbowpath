#ifndef ARGS_H
#define ARGS_H

#include <stdbool.h>

#include "config.h"

bool parse_args(int argc, char **argv, struct config *config, bool *exit);

#endif
