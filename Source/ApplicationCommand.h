/*
 * This file is part of RouteMIDI.
 * Copyright (command) 2017-2026 Uwyn LLC.  https://www.uwyn.com
 *
 * RouteMIDI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RouteMIDI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "JuceHeader.h"

#include "Mpe.h"

enum CommandIndex
{
    NONE,

    // routing and ports
    INPUT,
    OUTPUT,
    VIRTUAL_IN,
    VIRTUAL_OUT,
    LIST,
    PANIC,

    // global configuration
    TXTFILE,
    DECIMAL,
    HEXADECIMAL,
    OCTAVE_MIDDLE_C,
    NOTE_NUMBERS,
    TIMESTAMP,
    MONITOR,
    MONITOR_SOURCE,
    SYSEX_FILE,

    // filter modifier
    NOT,

    // filters
    CHANNEL,
    VOICE,
    NOTE,
    NOTE_ON,
    NOTE_OFF,
    POLY_PRESSURE,
    CONTROL_CHANGE,
    CONTROL_CHANGE_14BIT,
    NRPN,
    RPN,
    PROGRAM_CHANGE,
    CHANNEL_PRESSURE,
    PITCH_BEND,
    SYSTEM_REALTIME,
    CLOCK,
    START,
    STOP,
    CONTINUE,
    ACTIVE_SENSING,
    RESET,
    SYSTEM_COMMON,
    SYSTEM_EXCLUSIVE,
    TIME_CODE,
    SONG_POSITION,
    SONG_SELECT,
    TUNE_REQUEST,
    NOTE_RANGE,
    VELOCITY_RANGE,
    CONTROL_CHANGE_RANGE,
    CONTROL_CHANGE_14BIT_RANGE,
    IN_SCALE,
    MPE_MASTER,
    MPE_MEMBER,
    MPE_ZONE,

    // transforms
    CHANNEL_MAP,
    CHANNEL_SET,
    CHANNEL_ADD,
    TRANSPOSE,
    DIATONIC_TRANSPOSE,
    NOTE_MAP,
    SCALE,
    CHORD,
    LATCH,
    MONO,
    SUSTAIN,
    SOSTENUTO,
    NOTE_TO_CC,
    CC_TO_NOTE,
    NOTE_TO_PROGRAM,
    VELOCITY_SCALE,
    VELOCITY_SET,
    VELOCITY_ADD,
    VELOCITY_CURVE,
    VELOCITY_CLIP,
    VELOCITY_COMPRESS,
    VELOCITY_INVERT,
    CONTROL_CHANGE_MAP,
    CONTROL_CHANGE_ADD,
    CONTROL_CHANGE_SCALE,
    CONTROL_CHANGE_CURVE,
    CONTROL_CHANGE_INVERT,
    CONTROL_CHANGE_RESCALE,
    CONTROL_CHANGE_SET,
    PROGRAM_CHANGE_MAP,
    PROGRAM_CHANGE_ADD,
    PITCH_BEND_ADD,
    PITCH_BEND_SCALE,
    PITCH_BEND_SET,
    PITCH_BEND_INVERT,
    CHANNEL_PRESSURE_ADD,
    CHANNEL_PRESSURE_SCALE,
    CHANNEL_PRESSURE_SET,
    CHANNEL_PRESSURE_CURVE,
    CHANNEL_PRESSURE_INVERT,

    // 14-bit CC and RPN/NRPN value transforms (assembled in the converter
    // stage, but grouped here with the other value transforms they mirror)
    CC14_ADD,
    CC14_SCALE,
    CC14_CURVE,
    CC14_INVERT,
    CC14_RESCALE,
    CC14_SET,
    NRPN_ADD,
    NRPN_SCALE,
    NRPN_CURVE,
    NRPN_INVERT,
    NRPN_RESCALE,
    NRPN_SET,
    RPN_ADD,
    RPN_SCALE,
    RPN_CURVE,
    RPN_INVERT,
    RPN_RESCALE,
    RPN_SET,

    JAVASCRIPT,
    JAVASCRIPT_FILE,

    // converters (7-bit CC, 14-bit CC, RPN and NRPN inter-conversion)
    CONVERT,

    // MPE zone routing (relocate between zones, collapse to one channel, expand
    // a single channel into a zone, split voices across output ports)
    MPE_RELOCATE,
    MPE_COLLAPSE,
    MPE_EXPAND,
    MPE_SPLIT,
    MPE_BEND,
    MPE_SENS
};

class ApplicationState;

// One option token pre-parsed into every interpretation the filters and
// transforms may need, so the real-time path reads numbers instead of parsing
// option strings for every message. Compiled lazily on first use, when the
// complete command line has been processed and the hex and octave settings are
// final (matching the previous per-message parse, which also saw those final
// settings).
struct CompiledOption
{
    int    intValue { 0 };                    // dec/hex integer
    int    value7 { 0 };                      // dec/hex 7-bit value
    int    note { 0 };                        // note name or number
    int    selNoteLo { 0 }, selNoteHi { 0 };  // "lo..hi" selector parsed as notes
    int    sel7Lo { 0 },    sel7Hi { 0 };     // selector parsed as 7-bit values
    int    selIntLo { 0 },  selIntHi { 0 };   // selector parsed as plain integers
    double number { 0.0 };                    // floating point value
    uint16 scaleMask { 0 };                   // scale name or degree list, 0 = none
    int    pitchClass { 0 };                  // scale root (0-11)
    mpe::Zone zone;                           // MPE zone token
    bool   zoneValid { false };
    int    keyword { 0 };                     // 1 = hold, 2 = low, 3 = high
};

struct ApplicationCommand
{
    static ApplicationCommand Dummy();

    void clear();

    // true when this command selects which messages may pass through a route
    bool isFilter() const;
    // true when this command modifies the messages flowing through a route
    bool isTransform() const;

    // compiles opts_ to copts_ if that hasn't happened yet; called automatically
    // by matches() and transform(), and explicitly by the stages that read
    // copts_ directly (chord/latch/mono, the channel filter, the MPE operations)
    void ensureCompiled(const ApplicationState& state) const;

    // returns whether the message matches this filter (channel-aware for voice
    // messages); only meaningful when isFilter() is true and command_ != CHANNEL.
    // The channel context can be a single channel or an inclusive range; a low
    // of 0 means "any channel".
    bool matches(const ApplicationState& state, const MidiMessage& msg, int channel) const;
    bool matches(const ApplicationState& state, const MidiMessage& msg, int channelLow, int channelHigh) const;

    // applies this transform to the message in place; returns false when the
    // message should be dropped (for instance a transpose out of the 0-127 range)
    bool transform(const ApplicationState& state, MidiMessage& msg) const;

    static bool checkChannel(const MidiMessage& msg, int channelLow, int channelHigh);

    String param_;
    String altParam_;
    CommandIndex command_;
    int expectedOptions_;
    StringArray optionsDescriptions_;
    StringArray commandDescriptions_;
    String section_;                    // when set, a help header printed before this command
    StringArray opts_;
    bool negate_ { false };

    // options compiled to numbers once (see ensureCompiled); mutable because the
    // lazy compilation happens from the const matches()/transform() entry points
    mutable Array<CompiledOption> copts_;
    mutable bool compiled_ { false };

private:
    void compileOpts(const ApplicationState& state) const;

    // selector matching against a compiled option: a single value compiles to
    // lo == hi, a "lo..hi" range to its (swapped if needed) bounds
    bool selNote(int optIndex, int value) const
    {
        return value >= copts_[optIndex].selNoteLo && value <= copts_[optIndex].selNoteHi;
    }
    bool sel7(int optIndex, int value) const
    {
        return value >= copts_[optIndex].sel7Lo && value <= copts_[optIndex].sel7Hi;
    }
};
