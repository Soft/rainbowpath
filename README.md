# rainbowpath 🌈

Program for making paths pretty.

### Building

```shell
./autogen.sh
./configure
make
```

### Usage

```shell
Usage: rainbowpath [-p PALETTE] [-s COLOR] [-n] [-b] [-h] [PATH]
Color path components using a palette.

Options:
  -p PALETTE  Comma-separated list of colors for path components
              Colors are represented as numbers between 0 and 255
  -s COLOR    Color for path separators
  -n          Do not append newline
  -b          Escape color codes for use in Bash prompts
  -h          Display this help
```
