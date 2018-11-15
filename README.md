# rainbowpath 🌈

[![Build Status](https://api.travis-ci.org/Soft/rainbowpath.svg?branch=master)](https://travis-ci.org/Soft/rainbowpath)
[![GitHub release](https://img.shields.io/github/release/Soft/rainbowpath.svg)](https://github.com/Soft/rainbowpath/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Program for making paths pretty.

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
