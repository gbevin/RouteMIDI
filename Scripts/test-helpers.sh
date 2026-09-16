#!/usr/bin/env bash
#
# Shared by the scripts that drive routemidi through text on standard input and
# standard output. Each of them sets BIN to the binary under test, then for
# every case sets INPUT to the text fed in and EXPECTED to the text that should
# come out, and calls run with a name and the routemidi arguments.
#
# Only the words are compared, not the column widths, so a change to the way
# the text is laid out does not fail these tests.

normalise() {
    printf '%s\n' "$1" | tr -s ' ' | sed 's/[[:space:]]*$//'
}

run() {
    local name="$1"
    shift
    local actual
    actual="$(printf '%s\n' "$INPUT" | "$BIN" "$@" | tr -d '\r')"
    if [ "$(normalise "$actual")" = "$(normalise "$EXPECTED")" ]; then
        echo "ok   $name"
    else
        echo "FAIL $name"
        echo "--- input ----------";    printf '%s\n' "$INPUT"
        echo "--- expected -------";    printf '%s\n' "$EXPECTED"
        echo "--- actual ---------";    printf '%s\n' "$actual"
        echo "--------------------"
        failures=$((failures+1))
    fi
}
