# rainbowpath 🌈

[![Build Status](https://api.travis-ci.org/Soft/rainbowpath.svg?branch=master)](https://travis-ci.org/Soft/rainbowpath)
[![GitHub release](https://img.shields.io/github/release/Soft/rainbowpath.svg)](https://github.com/Soft/rainbowpath/releases)
[![AUR version](https://img.shields.io/aur/version/rainbowpath.svg)](https://aur.archlinux.org/packages/rainbowpath/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

`rainbowpath` is a program for making paths pretty. It can be used to make pretty
shell prompts:

<img src="https://raw.githubusercontent.com/Soft/rainbowpath/master/extra/screenshot.png">

`rainbowpath` formats supplied path by coloring each path component with a color
selected from a palette. By default, colors for path components are selected
based on the order they appear in the palette. If no palette is supplied a
default one will be used. When invoked without a path `rainbowpath` displays the
current working directory.

### Installation

Statically linked release binaries are available on GitHub [releases
page](https://github.com/Soft/rainbowpath/releases). These should work on most
modern Linux systems. For Arch Linux, `rainbowpath` is also available on
[AUR](https://aur.archlinux.org/packages/rainbowpath/).

### Building

```shell
./autogen.sh
./configure
make
```

### Usage

```
Usage: rainbowpath [-p PALETTE] [-s PALETTE] [-S SEPARATOR] [-m METHOD]
                   [-M METHOD] [-o INDEX STYLE] [-O INDEX STYLE]
                   [-l] [-c] [-n] [-b] [-h] [-v] [PATH]

Color path components using a palette.

Options:
  -p, --palette PALETTE                 Semicolon separated list of styles for
                                        path components
  -s, --separator-palette PALETTE       Semicolon separated list of styles for
                                        path separators.
  -S, --separator SEPARATOR             String used to separate path components
                                        in the output (defaults to '/')
  -m, --method METHOD                   Method for selecting styles from palette.
                                        One of sequential, hash, random
                                        (defaults to sequential).
  -M, --separator-method METHOD         Method for selecting styles from separator
                                        palette. One of sequential, hash, random
                                        (defaults to sequential).
  -o, --override INDEX STYLE            Override style at the given index. This option
                                        can appear multiple times.
  -O, --separator-override INDEX STYLE  Override separator style at the given index.
                                        This option can appear multiple times.
  -l, --leading                         Do not display leading path separator
  -c, --compact                         Replace home directory path prefix with ~
  -n, --newline                         Do not append newline
  -b, --bash                            Escape control codes for use in Bash prompts
  -h, --help                            Display this help
  -v, --version                         Display version information
```

### Use in a Bash prompt

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

### Styles

Styles specify how path components should look. `--palette` and `--separator`
options accept styles as arguments. Style consists of a list of properties
separated by commas. The possible properties are:

| Property     | Description                     |
| ------------ | ------------------------------- |
| `fg=COLOR`   | Set text color to `COLOR`       |
| `bg=COLOR`   | Set background color to `COLOR` |
| `bold`       | Bold font                       |
| `dim`        | Dim color                       |
| `underlined` | Underlined text                 |
| `blink`      | Blinking text                   |

Where `COLOR` is an integer between 0 and 255 or one of: `black`, `red`,
`green`, `yellow`, `blue`, `magenta`, `cyan`, `white`.

For example, the following invocation will display the path of the current
working directory altering styles of path components between underlined green
text and bold yellow text on magenta background:

``` shell
rainbowpath --palette 'fg=green,underlined;fg=yellow,bg=magenta,bold'
```

### Style Overrides

`--override` and `--separator-override` options make it possible to selectively
override the style of particular path component at a specific index.

For example, the following command displays all the directory components of the
path as blue, except the first one (index `0` "this") which will be displayed as
yellow.

``` shell
rainbowpath -p 'fg=blue' -o 0 'fg=yellow' '/this/is/an/example/'
```

Component indexes can also be negative, in which case they start at the end of
the list of path components. For example, the following command will print
"example" as yellow.

``` shell
rainbowpath -p 'fg=blue' -o -1 'fg=yellow' '/this/is/an/example/'
```

Override styles are merged with the base style from the palette. For example,
the following command will display example in blue bold font.

``` shell
rainbowpath -p 'fg=blue' -o -1 'bold' '/this/is/an/example/'
```

Override styles are also able to revert style properties set by the base style.
This can be achieved by prefixing the property name with a `!`.

For example, the following command will print all path components in bold yellow
font, except the last one which will be printed in the regular font:

``` shell
rainbowpath -p 'fg=yellow,bold' -o -1 '!fg,!bold' '/this/is/an/example/'
```
