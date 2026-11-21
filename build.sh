#!/bin/sh
set -e

CC=${CC:-cc}
ROOT=$(dirname "$0")
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' 0
trap 'exit 1' HUP INT TERM
CFLAGS="${CFLAGS:-} -std=c99 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wundef -Wwrite-strings -Werror"

for source in "$ROOT/src/lwimg.c" "$ROOT/examples/lwimg_demo.c"; do
    printf 'compiling %s\n' "$source"
    "$CC" $CFLAGS -I "$ROOT/src" -c "$source" -o "$OUT/$(basename "$source" .c).o"
done

printf 'ok\n'
