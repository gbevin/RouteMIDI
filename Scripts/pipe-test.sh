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

# --- the sustain pedal is applied to the notes themselves ------------------------
INPUT='channel 1 control-change 64 127
channel 1 note-on 60 100
channel 1 note-off 60 0
channel 1 control-change 64 0'
EXPECTED='channel  1   note-on           C3 100
channel  1   note-off          C3   0'
run "sustain pedal pre-application" in - sustain out -

# --- a controller value range is rescaled onto another range ---------------------
INPUT='channel 1 control-change 11 0
channel 1 control-change 11 127'
EXPECTED='channel  1   control-change    11    20
channel  1   control-change    11   100'
run "ccrescale range mapping" in - ccrescale 11 0 127 20 100 out -

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

# --- the MCP server speaks newline-delimited JSON-RPC over stdio -----------------
# a real MCP client sends one compact JSON message per line; the handshake, the
# tool listing and a route lifecycle (start, inject, edit, stop) must all come
# back as single-line JSON responses
MCP_OUT="$(printf '%s\n%s\n%s\n%s\n%s\n%s\n%s\n' \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"pipe-test","version":"1"}}}' \
    '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' \
    '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"start_route","arguments":{"commands":["in","PipeTestIn","transp","12","out","PipeTestOut"]}}}' \
    '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"inject_midi","arguments":{"route":1,"messages":["channel 1 note-on 60 100"]}}}' \
    '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"replace_command","arguments":{"route":1,"stage":"transforms","index":0,"commands":["transp","-12"]}}}' \
    '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"stop_route","arguments":{"route":1}}}' \
    '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"list_routes","arguments":{}}}' \
    | "$BIN" --mcp 2>/dev/null | tr -d '\r')"
if [ "$(printf '%s\n' "$MCP_OUT" | wc -l | tr -d ' ')" = "7" ] \
    && printf '%s\n' "$MCP_OUT" | sed -n 1p | grep -q '"serverInfo".*"routemidi"' \
    && printf '%s\n' "$MCP_OUT" | sed -n 2p | grep -q '"list_routes"' \
    && printf '%s\n' "$MCP_OUT" | sed -n 3p | grep -q '"id": 1' \
    && printf '%s\n' "$MCP_OUT" | sed -n 4p | grep -q 'note-on.*C4' \
    && printf '%s\n' "$MCP_OUT" | sed -n 5p | grep -q '\-12' \
    && printf '%s\n' "$MCP_OUT" | sed -n 6p | grep -q '"stopped": 1' \
    && printf '%s\n' "$MCP_OUT" | sed -n 7p | grep -q '"routes": \[\]'; then
    echo "ok   mcp handshake and route lifecycle"
else
    echo "FAIL mcp handshake and route lifecycle"
    echo "--- actual ---------"; printf '%s\n' "$MCP_OUT"
    echo "--------------------"
    failures=$((failures+1))
fi

if [ "$failures" -ne 0 ]; then
    echo "$failures pipe test(s) FAILED"
    exit 1
fi
echo "all pipe tests passed"
