#!/usr/bin/env bash
#
# End-to-end tests that drive routemidi through real MIDI ports, with sendmidi
# feeding its inputs and receivemidi reading its outputs, so port enumeration,
# connection handling, reconnection and the MIDI backend are exercised on the
# real binaries. On macOS and Linux the ports are virtual ones the tools create;
# on Windows E2E_PORT names an existing loopback port (loopMIDI), which limits
# the cases to the ones that need a single port.
#
# Usage: e2e-test.sh <path-to-routemidi> <path-to-sendmidi> <path-to-receivemidi>
#
# A case brackets its traffic with CC 119 marker messages: the start marker is
# repeated until it shows up at the far end, which proves every port on the way
# is open, and the end marker tells when everything before it has arrived.

set -u
if [ -n "${E2E_TRACE:-}" ]; then
    set -x
fi

ROUTEMIDI="$1"
SENDMIDI="$2"
RECEIVEMIDI="$3"
PORT="${E2E_PORT:-}"
WORK="$(mktemp -d)"
MARK_START='control-change +119 +1$'
MARK_END='control-change +119 +2$'
failures=0
router_pid=""
receiver_pid=""
received=""

virtual_ports() { [ -z "$PORT" ]; }

pass() { echo "ok   $1"; }

fail() {
    echo "FAIL $1"
    shift
    printf '     %s\n' "$@"
    failures=$((failures+1))
}

# compares two multi-line strings and reports the difference
check() {
    local name="$1" expected="$2" actual="$3"
    if [ "$expected" = "$actual" ]; then
        pass "$name"
    else
        fail "$name"
        echo "--- expected -------"; printf '%s\n' "$expected"
        echo "--- actual ---------"; printf '%s\n' "$actual"
        echo "--------------------"
    fi
}

stop() {
    local pid="$1"
    if [ -n "$pid" ]; then
        kill "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null
        # the MIDI backend may refuse a port created right after one vanished
        sleep 0.5
    fi
}

stop_all() {
    stop "$router_pid"; router_pid=""
    stop "$receiver_pid"; receiver_pid=""
}

# sends the start marker into a port until it shows up in the given output file
wait_for_marker() {
    local in="$1" out="$2"
    local i
    for i in $(seq 1 40); do
        sleep 0.25
        "$SENDMIDI" dev "$in" cc 119 1 > /dev/null 2>&1
        if grep -qE "$MARK_START" "$out"; then
            return 0
        fi
    done
    return 1
}

# sends the end marker into a port, waits for it in the output file and leaves
# the lines between the markers in $received
collect() {
    local in="$1" out="$2"
    local i
    for i in $(seq 1 40); do
        "$SENDMIDI" dev "$in" cc 119 2 > /dev/null 2>&1
        if grep -qE "$MARK_END" "$out"; then
            break
        fi
        sleep 0.25
    done
    received="$(tr -d '\r' < "$out" | awk -v s="$MARK_START" -v e="$MARK_END" \
        '$0 ~ s { buf = ""; next } $0 ~ e { printf "%s", buf; exit } { buf = buf $0 "\n" }')"
}

# the port a case sends into: the loopback port on Windows, a fresh name else
input_port() {
    if virtual_ports; then
        echo "E2E routemidi in $$ $RANDOM"
    else
        echo "$PORT"
    fi
}

# runs "routemidi in <port> <commands> out -" against a fixed set of messages
# and compares the text it prints
route_to_text() {
    local name="$1" expected="$2"
    shift 2
    local in
    in="$(input_port)"
    if virtual_ports; then
        "$ROUTEMIDI" vin "$in" "$@" out - > "$WORK/text.txt" 2>&1 &
    else
        "$ROUTEMIDI" in "$in" "$@" out - > "$WORK/text.txt" 2>&1 &
    fi
    router_pid=$!
    if wait_for_marker "$in" "$WORK/text.txt"; then
        "$SENDMIDI" dev "$in" on 60 100 cc 74 64 pb 100 ch 2 on 61 50 off 61 0 cc 1 2 mc hex syx 7E 7F 09 01
        collect "$in" "$WORK/text.txt"
        stop "$router_pid"; router_pid=""
        check "$name" "$expected" "$received"
    else
        fail "$name" "the marker never came out of the route" "$(cat "$WORK/text.txt")"
        stop "$router_pid"; router_pid=""
    fi
}

trap 'stop_all; rm -rf "$WORK"' EXIT

# --- MIDI in through a route to text out ------------------------------------
route_to_text "a route passes every message type from a port to text" 'channel  1   note-on           C3 100
channel  1   control-change    74    64
channel  1   pitch-bend           100
channel  2   note-on          C#3  50
channel  2   note-off         C#3   0
channel  2   control-change     1     2
midi-clock
system-exclusive hex 7E 7F 09 01 dec'

route_to_text "transforms rewrite the routed messages" 'channel  5   note-on           C4 100
channel  5   control-change    74    64
channel  5   pitch-bend           100
channel  6   note-on          C#4  50
channel  6   note-off         C#4   0
channel  6   control-change     1     2
midi-clock
system-exclusive hex 7E 7F 09 01 dec' transp 12 chmap 1 5 chmap 2 6

route_to_text "filters drop what the route should not carry" 'channel  1   note-on           C3 100
channel  2   note-on          C#3  50
channel  2   note-off         C#3   0' note cc 119

# --- text in through a route to a MIDI port ---------------------------------
if virtual_ports; then
    out="E2E routemidi out $$ $RANDOM"
else
    out="$PORT"
fi
"$RECEIVEMIDI" dev "$out" > "$WORK/received.txt" 2>&1 &
receiver_pid=$!
if virtual_ports; then
    ( sleep 3; printf 'channel 1 control-change 119 1\nchannel 1 note-on C3 100\nchannel 1 control-change 1 2\nchannel 1 note-off C3 0\nchannel 1 control-change 119 2\n'; sleep 2 ) \
        | "$ROUTEMIDI" in - vout "$out" transp 12 > "$WORK/router.txt" 2>&1
else
    ( sleep 3; printf 'channel 1 control-change 119 1\nchannel 1 note-on C3 100\nchannel 1 control-change 1 2\nchannel 1 note-off C3 0\nchannel 1 control-change 119 2\n'; sleep 2 ) \
        | "$ROUTEMIDI" in - out "$out" transp 12 > "$WORK/router.txt" 2>&1
fi
sleep 0.5
stop "$receiver_pid"; receiver_pid=""
received="$(tr -d '\r' < "$WORK/received.txt" | awk -v s="$MARK_START" -v e="$MARK_END" \
    '$0 ~ s { buf = ""; next } $0 ~ e { printf "%s", buf; exit } { buf = buf $0 "\n" }')"
check "text on standard input is routed out of a port" 'channel  1   note-on           C4 100
channel  1   control-change     1     2
channel  1   note-off          C4   0' "$received"

if virtual_ports; then
    # --- port to port, two routes side by side, monitoring -----------------
    in1="$(input_port)"
    in2="$(input_port)"
    out1="E2E routemidi out $$ $RANDOM"
    out2="E2E routemidi out $$ $RANDOM"
    "$ROUTEMIDI" vin "$in1" transp 12 mon vout "$out1" vin "$in2" chmap 1 3 vout "$out2" > "$WORK/monitor.txt" 2>&1 &
    router_pid=$!
    sleep 1
    "$RECEIVEMIDI" dev "$out1" > "$WORK/out1.txt" 2>&1 &
    receiver_pid=$!
    "$RECEIVEMIDI" dev "$out2" > "$WORK/out2.txt" 2>&1 &
    receiver2=$!
    if wait_for_marker "$in1" "$WORK/out1.txt" && wait_for_marker "$in2" "$WORK/out2.txt"; then
        "$SENDMIDI" dev "$in1" on 60 100 off 60 0
        "$SENDMIDI" dev "$in2" on 60 100 off 60 0
        collect "$in1" "$WORK/out1.txt"
        first="$received"
        collect "$in2" "$WORK/out2.txt"
        second="$received"
        stop "$router_pid"; router_pid=""
        check "two routes side by side reach their own output ports" \
            "$(printf 'channel  1   note-on           C4 100\nchannel  1   note-off          C4   0\n---\nchannel  3   note-on           C3 100\nchannel  3   note-off          C3   0')" \
            "$(printf '%s\n---\n%s' "$first" "$second")"
        check "mon prints what the routes send" 'channel  1   note-on           C4 100
channel  1   note-off          C4   0
channel  3   note-on           C3 100
channel  3   note-off          C3   0' "$(tr -d '\r' < "$WORK/monitor.txt" | awk -v s="$MARK_START" -v e="$MARK_END" \
            '$0 ~ s { buf = ""; next } $0 ~ e { printf "%s", buf; exit } { buf = buf $0 "\n" }')"
    else
        fail "two routes side by side reach their own output ports" "the marker never came out of the routes" \
            "$(cat "$WORK/monitor.txt" "$WORK/out1.txt" "$WORK/out2.txt")"
        stop "$router_pid"; router_pid=""
    fi
    stop "$receiver_pid"; receiver_pid=""
    stop "$receiver2"

    # --- an output that disappears is reconnected when its port returns ----
    in="$(input_port)"
    out="E2E routemidi out $$ $RANDOM"
    "$ROUTEMIDI" vin "$in" out "$out" > "$WORK/reconnect.txt" 2>&1 &
    router_pid=$!
    sleep 1
    "$RECEIVEMIDI" virt "$out" > "$WORK/first.txt" 2>&1 &
    receiver_pid=$!
    if wait_for_marker "$in" "$WORK/first.txt"; then
        "$SENDMIDI" dev "$in" on 60 100
        collect "$in" "$WORK/first.txt"
        first="$received"
        stop "$receiver_pid"; receiver_pid=""
        sleep 1
        "$RECEIVEMIDI" virt "$out" > "$WORK/second.txt" 2>&1 &
        receiver_pid=$!
        if wait_for_marker "$in" "$WORK/second.txt"; then
            "$SENDMIDI" dev "$in" on 62 100
            collect "$in" "$WORK/second.txt"
            check "a vanished output is reconnected when its port comes back" \
                "$(printf 'channel  1   note-on           C3 100\n---\nchannel  1   note-on           D3 100')" \
                "$(printf '%s\n---\n%s' "$first" "$received")"
        else
            fail "a vanished output is reconnected when its port comes back" "the route never reached the recreated port" "$(cat "$WORK/reconnect.txt")"
        fi
    else
        fail "a vanished output is reconnected when its port comes back" "the route never reached the port" "$(cat "$WORK/reconnect.txt")"
    fi
    stop_all

    # --- a terminating signal still runs the exit panic ---------------------
    in="$(input_port)"
    out="E2E routemidi out $$ $RANDOM"
    "$ROUTEMIDI" vin "$in" vout "$out" panic > "$WORK/signal.txt" 2>&1 &
    router_pid=$!
    sleep 1
    "$RECEIVEMIDI" dev "$out" > "$WORK/panic.txt" 2>&1 &
    receiver_pid=$!
    if wait_for_marker "$in" "$WORK/panic.txt"; then
        "$SENDMIDI" dev "$in" on 60 100
        kill -TERM "$router_pid"
        for i in $(seq 1 40); do
            if ! kill -0 "$router_pid" 2>/dev/null; then
                break
            fi
            sleep 0.25
        done
        if kill -0 "$router_pid" 2>/dev/null; then
            fail "SIGTERM runs the exit panic before quitting" "routemidi did not exit within ten seconds"
        else
            sleep 1
            expected="$(for channel in $(seq 1 16); do
                printf 'channel %2d   control-change    64     0\n' "$channel"
                printf 'channel %2d   control-change    66     0\n' "$channel"
                printf 'channel %2d   control-change   123     0\n' "$channel"
            done)"
            check "SIGTERM runs the exit panic before quitting" "$expected" \
                "$(tr -d '\r' < "$WORK/panic.txt" | grep -E 'control-change +(64|66|123) +0$')"
        fi
        wait "$router_pid" 2>/dev/null
        router_pid=""
    else
        fail "SIGTERM runs the exit panic before quitting" "the route never reached the port" "$(cat "$WORK/signal.txt")"
    fi
    stop_all
fi

# --- a program file builds the same route -----------------------------------
in="$(input_port)"
if virtual_ports; then
    printf 'vin "%s"\ntransp 12\nout -\n' "$in" > "$WORK/program.txt"
else
    printf 'in "%s"\ntransp 12\nout -\n' "$in" > "$WORK/program.txt"
fi
"$ROUTEMIDI" file "$WORK/program.txt" > "$WORK/program-out.txt" 2>&1 &
router_pid=$!
if wait_for_marker "$in" "$WORK/program-out.txt"; then
    "$SENDMIDI" dev "$in" on 60 100
    collect "$in" "$WORK/program-out.txt"
    stop "$router_pid"; router_pid=""
    check "a program file builds the route" 'channel  1   note-on           C4 100' "$received"
else
    fail "a program file builds the route" "the marker never came out of the route" "$(cat "$WORK/program-out.txt")"
    stop "$router_pid"; router_pid=""
fi

# --- the MCP server answers over standard input and output ------------------
mcp="$(printf '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"e2e","version":"0"}}}\n{"jsonrpc":"2.0","id":2,"method":"ping"}\n{"jsonrpc":"2.0","id":3,"method":"tools/list"}\n' \
    | "$ROUTEMIDI" --mcp 2>/dev/null | tr -d '\r')"
if printf '%s\n' "$mcp" | grep -q '"id": 1, "result": {"protocolVersion"' \
   && printf '%s\n' "$mcp" | grep -q '"id": 2, "result": {}' \
   && [ "$(printf '%s\n' "$mcp" | grep -o '"name": "' | wc -l)" -ge 11 ]; then
    pass "the MCP server initializes, answers ping and lists its tools"
else
    fail "the MCP server initializes, answers ping and lists its tools" "$mcp"
fi

echo
if [ "$failures" -eq 0 ]; then
    echo "all end-to-end tests passed"
else
    echo "$failures end-to-end test(s) failed"
    exit 1
fi
