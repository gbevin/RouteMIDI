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
    MPE_MASTER,
    MPE_MEMBER,
    MPE_ZONE,

    // transforms
    CHANNEL_MAP,
    CHANNEL_SET,
    CHANNEL_ADD,
    TRANSPOSE,
    NOTE_MAP,
    NOTE_TO_CC,
    CC_TO_NOTE,
    NOTE_TO_PROGRAM,
    VELOCITY_SCALE,
    VELOCITY_SET,
    VELOCITY_ADD,
    VELOCITY_CURVE,
    CONTROL_CHANGE_MAP,
    CONTROL_CHANGE_ADD,
    CONTROL_CHANGE_SCALE,
    CONTROL_CHANGE_CURVE,
    PROGRAM_CHANGE_MAP,
    PROGRAM_CHANGE_ADD,
    PITCH_BEND_ADD,
    PITCH_BEND_SCALE,
    PITCH_BEND_SET,
    CHANNEL_PRESSURE_ADD,
    CHANNEL_PRESSURE_SCALE,
    CHANNEL_PRESSURE_SET,
    CHANNEL_PRESSURE_CURVE,

    // RPN/NRPN value transforms (assembled in the converter stage, but grouped
    // here with the other value transforms they mirror)
    NRPN_ADD,
    NRPN_SCALE,
    NRPN_CURVE,
    RPN_ADD,
    RPN_SCALE,
    RPN_CURVE,

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

struct ApplicationCommand
{
    static ApplicationCommand Dummy();

    void clear();

    // true when this command selects which messages may pass through a route
    bool isFilter() const;
    // true when this command modifies the messages flowing through a route
    bool isTransform() const;

    // returns whether the message matches this filter (channel-aware for voice
    // messages); only meaningful when isFilter() is true and command_ != CHANNEL.
    // The channel context can be a single channel or an inclusive range; a low
    // of 0 means "any channel".
    bool matches(ApplicationState& state, const MidiMessage& msg, int channel) const;
    bool matches(ApplicationState& state, const MidiMessage& msg, int channelLow, int channelHigh) const;

    // applies this transform to the message in place; returns false when the
    // message should be dropped (for instance a transpose out of the 0-127 range)
    bool transform(ApplicationState& state, MidiMessage& msg) const;

    static bool checkChannel(const MidiMessage& msg, int channelLow, int channelHigh);

    String param_;
    String altParam_;
    CommandIndex command_;
    int expectedOptions_;
    StringArray optionsDescriptions_;
    StringArray commandDescriptions_;
    StringArray opts_;
    bool negate_ { false };
};
