#!/usr/bin/env bash
#
# End-to-end examples based on common MIDI routing use cases:
# splits, cleanup filters, controller remaps, velocity shaping, musical
# transforms, MPE expansion and JavaScript state.
#
# Usage: use-case-test.sh <path-to-routemidi>

set -u

BIN="$1"
failures=0

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

# Split one keyboard at middle C: bass notes to channel 2, lead notes to channel 3.
INPUT='channel 1 note-on 59 90
channel 1 note-on 60 91
channel 1 note-off 59 0
channel 1 note-off 60 0'
EXPECTED='channel  2   note-on           B2  90
channel  3   note-on           C3  91
channel  2   note-off          B2   0
channel  3   note-off          C3   0'
run "key split into two layers" \
    in - noterange C-2 B2 chset 2 out - \
    in - noterange C3 G8 chset 3 out -

# Clean up a sequencer stream by dropping clock and active sensing.
INPUT='midi-clock
active-sensing
channel 1 note-on 60 100
channel 1 control-change 64 127
stop'
EXPECTED='channel  1   note-on           C3 100
channel  1   control-change    64   127
stop'
run "drop clock and active sensing" in - not clock not as out -

# Turn a volume fader into expression and scale it for a quieter synth.
INPUT='channel 1 control-change 7 100
channel 1 control-change 74 20
channel 1 control-change 7 0'
EXPECTED='channel  1   control-change    11    50
channel  1   control-change    74    20
channel  1   control-change    11     0'
run "map CC 7 volume to half-scale CC 11 expression" \
    in - ccmap 7 11 ccscale 11 0.5 out -

# Keep an uneven keyboard controller inside a useful dynamic range.
INPUT='channel 1 note-on 60 20
channel 1 note-on 61 80
channel 1 note-on 62 127
channel 1 note-off 60 0'
EXPECTED='channel  1   note-on           C3  40
channel  1   note-on          C#3  80
channel  1   note-on           D3 100
channel  1   note-off          C3   0'
run "clip note velocity range" in - velclip 40 100 out -

# Keep a part in C minor by snapping out-of-key notes.
INPUT='channel 1 note-on 61 100
channel 1 note-off 61 0
channel 1 note-on 64 100
channel 1 note-off 64 0'
EXPECTED='channel  1   note-on           C3 100
channel  1   note-off          C3   0
channel  1   note-on          D#3 100
channel  1   note-off         D#3   0'
run "scale quantize to C minor" in - scale C minor out -

# Generate diatonic triads from single notes.
INPUT='channel 1 note-on 65 100
channel 1 note-off 65 0'
EXPECTED='channel  1   note-on           F3 100
channel  1   note-on           A3 100
channel  1   note-on           C4 100
channel  1   note-off          F3   0
channel  1   note-off          A3   0
channel  1   note-off          C4   0'
run "diatonic chord layer" in - chord 4 7 scale C major out -

# Expand a normal channel-1 keyboard into a three-member lower MPE zone.
INPUT='channel 1 note-on 60 100
channel 1 poly-pressure 60 70
channel 1 note-on 64 90
channel 1 note-off 60 0
channel 1 note-off 64 0'
EXPECTED='channel  1   control-change   101     0
channel  1   control-change   100     6
channel  1   control-change     6     3
channel  1   control-change   101   127
channel  1   control-change   100   127
channel  2   channel-pressure       0
channel  2   note-on           C3 100
channel  2   channel-pressure      70
channel  3   channel-pressure       0
channel  3   note-on           E3  90
channel  2   channel-pressure       0
channel  2   note-off          C3   0
channel  3   channel-pressure       0
channel  3   note-off          E3   0'
run "regular keyboard to MPE" in - mpexp 1 lower:3 out -

# Use stateful JavaScript to accent every fourth note.
INPUT='channel 1 note-on 60 50
channel 1 note-on 62 50
channel 1 note-on 64 50
channel 1 note-on 65 50'
EXPECTED='channel  1   note-on           C3  50
channel  1   note-on           D3  50
channel  1   note-on           E3  50
channel  1   note-on           F3 127'
run "javascript accent every fourth note" \
    in - js 'if (MIDI.isNoteOn()) { if (typeof count == "undefined") count = 0; if (++count % 4 == 0) MIDI.setVelocity(127); }' out -

if [ "$failures" -ne 0 ]; then
    echo "$failures use-case test(s) FAILED"
    exit 1
fi
echo "all use-case tests passed"
