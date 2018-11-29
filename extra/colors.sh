#!/usr/bin/env bash

for n in $(seq 0 255); do
    printf "%3s" $n
    echo -ne " \e[48;5;${n}m  \e[0m "
    if ! ((($n + 1) % 16)); then
        echo
    fi
done

