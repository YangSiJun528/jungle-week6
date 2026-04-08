#!/bin/sh

set -eu

PROGRAM="./program"

assert_contains() {
    file_path="$1"
    expected="$2"

    if ! grep -Fq "$expected" "$file_path"; then
        echo "Expected to find: $expected"
        echo "Actual content:"
        cat "$file_path"
        exit 1
    fi
}

assert_empty() {
    file_path="$1"

    if [ -s "$file_path" ]; then
        echo "Expected empty output, but got:"
        cat "$file_path"
        exit 1
    fi
}

help_out="$(mktemp)"
help_err="$(mktemp)"
"$PROGRAM" -h >"$help_out" 2>"$help_err"
assert_contains "$help_out" "Usage:"
assert_empty "$help_err"

success_out="$(mktemp)"
success_err="$(mktemp)"
printf 'SELECT * FROM users;\n' | "$PROGRAM" >"$success_out" 2>"$success_err"
assert_contains "$success_out" "OK"
assert_empty "$success_err"

tty_log="$(mktemp)"
tty_clean="$(mktemp)"
if script -q "$tty_log" "$PROGRAM" >/dev/null 2>&1; then
    echo "Expected tty execution to fail"
    exit 1
fi
tr -d '\r' <"$tty_log" >"$tty_clean"
assert_contains "$tty_clean" "Provide SQL with stdin redirection."

empty_out="$(mktemp)"
empty_err="$(mktemp)"
if printf '' | "$PROGRAM" >"$empty_out" 2>"$empty_err"; then
    echo "Expected empty input to fail"
    exit 1
fi
assert_empty "$empty_out"
assert_contains "$empty_err" "Input is empty."

long_out="$(mktemp)"
long_err="$(mktemp)"
if awk 'BEGIN { for (i = 0; i < 4096; i++) printf "a" }' | "$PROGRAM" >"$long_out" 2>"$long_err"; then
    echo "Expected long input to fail"
    exit 1
fi
assert_empty "$long_out"
assert_contains "$long_err" "Input is too long."

arg_out="$(mktemp)"
arg_err="$(mktemp)"
if "$PROGRAM" extra.sql >"$arg_out" 2>"$arg_err"; then
    echo "Expected positional argument to fail"
    exit 1
fi
assert_empty "$arg_out"
assert_contains "$arg_err" "Positional arguments are not supported."
