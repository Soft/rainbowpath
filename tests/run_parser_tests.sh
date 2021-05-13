#!/usr/bin/env bash

set -e

should_pass() {
    echo -n "$2" | "$TEST_PARSER" "$1"
}

should_fail() {
    ! should_pass "$1" "$2"
}

# Colors
should_pass style "fg=green"

# Color and property
should_pass style "fg=green,blink"

# Extra white space
should_pass style "  fg = green , underlined  "

# Numeric colors
should_pass style "fg=100"

# Bold
should_pass style "bg=magenta,bold"

# Reverts
should_pass style "!fg,!bold"

# Empty
should_fail style ""

# Unknown property
should_fail style "hello"

# Color overflow
should_fail style "fg=256,fg=yellow"

# Unknown color
should_fail style "bg=hello"

# Unexpected trailer
should_fail style "fg=0,fg=yellow;"

# Color value missing
should_fail style "bg="

# Unexpected color value
should_fail style "!bg=100"

# Example from README
should_pass palette "fg=green,underlined;fg=yellow,bg=magenta,bold"

# Example from README
should_pass palette "fg=yellow,bold"

# Extra white space
should_pass palette "fg=green,underlined ;  fg=yellow,bg=magenta,bold"

# Unexpected trailer
should_fail palette "fg=green,underlined;fg=yellow,bg=magenta,bold;"


