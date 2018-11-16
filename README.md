# rainbowpath 🌈

[![Build Status](https://api.travis-ci.org/Soft/rainbowpath.svg?branch=master)](https://travis-ci.org/Soft/rainbowpath)
[![GitHub release](https://img.shields.io/github/release/Soft/rainbowpath.svg)](https://github.com/Soft/rainbowpath/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

`rainbowpath` is a program for making paths pretty. It can be used to make pretty
shell prompts:

<img src="https://raw.githubusercontent.com/Soft/rainbowpath/master/extra/screenshot.png">

`rainbowpath` formats supplied path by coloring each path component with a color
selected from a palette. Colors for path components are selected based on the
order they appear in the palette. If no palette is supplied a default one will
be used. `rainbowpath` should work on any modern terminal emulator that supports
256 colors. When invoked without a path `rainbowpath` displays the current
working directory.

### Installation

Statically linked release binaries are available on GitHub [releases
page](https://github.com/Soft/rainbowpath/releases). These should work on most
modern Linux systems.

### Building

```shell
./autogen.sh
./configure
make
```

### Usage

```
Usage: rainbowpath [-p PALETTE] [-s COLOR] [-n] [-b] [-h] [-v] [PATH]
Color path components using a palette.

Options:
  -p PALETTE  Comma-separated list of colors for path components
              Colors are represented as numbers between 0 and 255
  -s COLOR    Color for path separators
  -n          Do not append newline
  -b          Escape color codes for use in Bash prompts
  -h          Display this help
  -v          Display version information
```

### Example: Using in a Bash prompt

In Bash, what appears in the shell prompt can be controlled using the `PS1`
variable. In addition, `PROMPT_COMMAND` variable can be used to execute commands
before the prompt is displayed. This mechanism can be used to dynamically modify
the prompt using external commands. For example, to include `rainbowpath` output
into the shell prompt, we could define the following function:

```shell
function reset-prompt {
  PS1="\u@\h $(rainbowpath -b) \$ "
}

PROMPT_COMMAND=reset-prompt
```

With this setup, `rainbowpath` will be executed every time prompt is about to be
displayed and the output included into the prompt string.


