#!/usr/bin/env bash
#
# Feeds text MIDI through a routemidi binary via standard input and output and
# checks the result byte-for-byte, exercising the text codec and the routing
# pipeline (filters, transforms, conversions) end-to-end on the real binary.
#
# Usage: pipe-test.sh <path-to-routemidi>
#
# Runs on macOS, Linux and Windows (through the bash shell of the CI runners);
# carriage returns are stripped so the Windows line endings compare equal.

set -u

BIN="$1"
failures=0

# run <name> <routemidi args...> ; the input is read from the INPUT variable
# and the expected output from the EXPECTED variable
run() {
    local name="$1"
    shift
    local actual
    actual="$(printf '%s\n' "$INPUT" | "$BIN" "$@" | tr -d '\r')"
    if [ "$actual" = "$EXPECTED" ]; then
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

# --- the text codec round-trips every message type unchanged ------------------
INPUT='channel 1 note-on C3 100
channel 16 note-off 127 0
channel 2 poly-pressure C3 90
channel 3 control-change 74 64
channel 4 program-change 5
channel 5 channel-pressure 99
channel 6 pitch-bend 8192
midi-clock
start
stop
continue
active-sensing
reset
song-position 1234
song-select 12
tune-request
time-code 3 9
system-exclusive hex 01 02 7F'
EXPECTED='channel  1   note-on           C3 100
channel 16   note-off          G8   0
channel  2   poly-pressure     C3  90
channel  3   control-change    74    64
channel  4   program-change         5
channel  5   channel-pressure      99
channel  6   pitch-bend          8192
midi-clock
start
stop
continue
active-sensing
reset
song-position  1234
song-select  12
tune-request
time-code  3 9
system-exclusive hex 01 02 7F dec'
run "codec round-trip" in - out -

# --- a channel filter only lets the selected channel through ------------------
INPUT='channel 1 note-on 60 100
channel 2 note-on 62 100
channel 1 note-off 60 0
channel 2 note-off 62 0'
EXPECTED='channel  1   note-on           C3 100
channel  1   note-off          C3   0'
run "channel filter" in - ch 1 out -

# --- a transform rewrites the stream --------------------------------------------
INPUT='channel 1 note-on 60 100
channel 1 note-off 60 0'
EXPECTED='channel  1   note-on           C4 100
channel  1   note-off          C4   0'
run "transpose transform" in - transp 12 out -

# --- a conversion turns one message type into another ---------------------------
INPUT='channel 1 control-change 7 127
channel 1 control-change 7 64
channel 1 control-change 7 0'
EXPECTED='channel  1   pitch-bend         16383
channel  1   pitch-bend          8192
channel  1   pitch-bend             0'
run "cc to pitch-bend conversion" in - convert cc 7 pb out -

# --- an MPE collapse folds member channels onto one channel ---------------------
INPUT='channel 2 note-on 60 100
channel 3 note-on 64 100
channel 3 note-off 64 0
channel 2 note-off 60 0'
EXPECTED='channel  1   note-on           C3 100
channel  1   note-on           E3 100
channel  1   note-off          E3   0
channel  1   note-off          C3   0'
run "mpemono collapse" in - mpemono lower 1 out -

if [ "$failures" -ne 0 ]; then
    echo "$failures pipe test(s) FAILED"
    exit 1
fi
echo "all pipe tests passed"
