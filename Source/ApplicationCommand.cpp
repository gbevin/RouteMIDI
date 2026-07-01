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

#include "ApplicationCommand.h"

#include "ApplicationState.h"

// applies a gamma curve to a value in [0, maxValue]: gamma < 1 boosts low
// values (concave), gamma > 1 attenuates them (convex), gamma 1 is linear
static int applyGammaCurve(int value, int maxValue, double gamma)
{
    if (gamma <= 0.0 || maxValue <= 0)
    {
        return value;
    }
    const double normalized = jlimit(0.0, 1.0, (double)value / (double)maxValue);
    return jlimit(0, maxValue, roundToInt(std::pow(normalized, gamma) * maxValue));
}

// parses a pitch class (0-11) from a note name (C, C#, Db, ... with an optional
// octave that is ignored) or a plain number (interpreted modulo 12)
static int parsePitchClass(ApplicationState& state, const String& value)
{
    const String v = value.toUpperCase();
    if (v.isNotEmpty() && String("CDEFGABH").containsChar(v[0]))
    {
        int pc = 0;
        switch (v[0])
        {
            case 'C': pc = 0;  break;
            case 'D': pc = 2;  break;
            case 'E': pc = 4;  break;
            case 'F': pc = 5;  break;
            case 'G': pc = 7;  break;
            case 'A': pc = 9;  break;
            case 'B':
            case 'H': pc = 11; break;
        }
        if (v.length() >= 2)
        {
            if      (v[1] == 'B') pc -= 1;
            else if (v[1] == '#') pc += 1;
        }
        return ((pc % 12) + 12) % 12;
    }
    return ((state.asDecOrHexIntValue(value) % 12) + 12) % 12;
}

// builds a 12-bit mask of the pitch classes (relative to the root) that make up
// a named scale, or a custom comma-separated list of semitone degrees; returns
// 0 for an unrecognised name
static uint16 scaleMask(const String& name)
{
    // each scale lists the semitone degrees it contains, counting from the root;
    // the first spelling is canonical and the rest are accepted aliases
    static const struct { StringArray names; Array<int> degrees; } scales[] =
    {
        { {"chromatic"}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11} },
        { {"major", "ionian"}, {0, 2, 4, 5, 7, 9, 11} },
        { {"minor", "aeolian", "naturalminor"}, {0, 2, 3, 5, 7, 8, 10} },
        { {"dorian"}, {0, 2, 3, 5, 7, 9, 10} },
        { {"phrygian"}, {0, 1, 3, 5, 7, 8, 10} },
        { {"lydian"}, {0, 2, 4, 6, 7, 9, 11} },
        { {"mixolydian"}, {0, 2, 4, 5, 7, 9, 10} },
        { {"locrian"}, {0, 1, 3, 5, 6, 8, 10} },
        { {"harmonicminor"}, {0, 2, 3, 5, 7, 8, 11} },
        { {"melodicminor"}, {0, 2, 3, 5, 7, 9, 11} },
        { {"majorpentatonic", "majpent", "pentatonic"}, {0, 2, 4, 7, 9} },
        { {"minorpentatonic", "minpent"}, {0, 3, 5, 7, 10} },
        { {"majorblues", "majblues"}, {0, 2, 3, 4, 7, 9} },
        { {"minorblues", "minblues", "blues"}, {0, 3, 5, 6, 7, 10} },
        { {"diminished", "dim"}, {0, 2, 3, 5, 6, 8, 9, 11} },
        { {"wholetone"}, {0, 2, 4, 6, 8, 10} },
        { {"spanish", "phrygiandominant"}, {0, 1, 4, 5, 7, 8, 10} },
        { {"romani", "gypsy", "hungarianminor"}, {0, 2, 3, 6, 7, 8, 11} },
        { {"arabian"}, {0, 2, 4, 5, 6, 8, 10} },
        { {"egyptian"}, {0, 2, 5, 7, 10} },
        { {"ryukyu"}, {0, 4, 5, 7, 11} },
        { {"augmented", "maj3rd"}, {0, 4, 8} },
        { {"diminished7", "dim7", "min3rd"}, {0, 3, 6, 9} },
        { {"fifth", "power", "5th"}, {0, 7} },
    };

    const String n = name.toLowerCase().removeCharacters("-_ ");

    for (const auto& scale : scales)
    {
        if (scale.names.contains(n))
        {
            uint16 m = 0;
            for (int d : scale.degrees) m |= (uint16)(1 << d);
            return m;
        }
    }

    // custom scale: a comma-separated list of semitone degrees (e.g. 0,2,4,7,9)
    if (name.containsChar(','))
    {
        uint16 m = 0;
        StringArray parts;
        parts.addTokens(name, ",", "");
        for (const auto& p : parts)
        {
            const String t = p.trim();
            if (t.isNotEmpty())
            {
                m |= (uint16)(1 << (((t.getIntValue() % 12) + 12) % 12));
            }
        }
        return m;
    }

    return 0;
}

// snaps a note to the nearest note belonging to the scale, preferring the lower
// note on a tie and staying within 0-127; returns -1 when no scale note fits in
// range (which never happens for a non-empty scale)
static int snapToScale(int note, int root, uint16 mask)
{
    auto inScale = [root, mask](int n)
    {
        return (mask >> ((((n - root) % 12) + 12) % 12)) & 1;
    };

    if (inScale(note)) return note;

    for (int d = 1; d <= 12; ++d)
    {
        const int down = note - d;
        if (down >= 0 && inScale(down)) return down;   // ties resolve downward
        const int up = note + d;
        if (up <= 127 && inScale(up)) return up;
    }
    return -1;
}

ApplicationCommand ApplicationCommand::Dummy()
{
    return {"", "", NONE, 0, {""}, {""}};
}

void ApplicationCommand::clear()
{
    param_ = "";
    command_ = NONE;
    expectedOptions_ = 0;
    optionsDescriptions_ = StringArray({""});
    commandDescriptions_ = StringArray({""});
    opts_.clear();
    negate_ = false;
}

bool ApplicationCommand::isFilter() const
{
    switch (command_)
    {
        case CHANNEL:
        case VOICE:
        case NOTE:
        case NOTE_ON:
        case NOTE_OFF:
        case POLY_PRESSURE:
        case CONTROL_CHANGE:
        case CONTROL_CHANGE_14BIT:
        case NRPN:
        case RPN:
        case PROGRAM_CHANGE:
        case CHANNEL_PRESSURE:
        case PITCH_BEND:
        case SYSTEM_REALTIME:
        case CLOCK:
        case START:
        case STOP:
        case CONTINUE:
        case ACTIVE_SENSING:
        case RESET:
        case SYSTEM_COMMON:
        case SYSTEM_EXCLUSIVE:
        case TIME_CODE:
        case SONG_POSITION:
        case SONG_SELECT:
        case TUNE_REQUEST:
        case NOTE_RANGE:
        case VELOCITY_RANGE:
        case MPE_MASTER:
        case MPE_MEMBER:
        case MPE_ZONE:
            return true;
        default:
            return false;
    }
}

bool ApplicationCommand::isTransform() const
{
    switch (command_)
    {
        case CHANNEL_MAP:
        case CHANNEL_SET:
        case CHANNEL_ADD:
        case TRANSPOSE:
        case NOTE_MAP:
        case NOTE_TO_CC:
        case CC_TO_NOTE:
        case NOTE_TO_PROGRAM:
        case SCALE:
        case CHORD:
        case VELOCITY_SCALE:
        case VELOCITY_SET:
        case VELOCITY_ADD:
        case VELOCITY_CURVE:
        case CONTROL_CHANGE_MAP:
        case CONTROL_CHANGE_ADD:
        case CONTROL_CHANGE_SCALE:
        case CONTROL_CHANGE_CURVE:
        case PROGRAM_CHANGE_MAP:
        case PROGRAM_CHANGE_ADD:
        case PITCH_BEND_ADD:
        case PITCH_BEND_SCALE:
        case PITCH_BEND_SET:
        case CHANNEL_PRESSURE_ADD:
        case CHANNEL_PRESSURE_SCALE:
        case CHANNEL_PRESSURE_SET:
        case CHANNEL_PRESSURE_CURVE:
        case JAVASCRIPT:
        case JAVASCRIPT_FILE:
            return true;
        default:
            return false;
    }
}

bool ApplicationCommand::checkChannel(const MidiMessage& msg, int channelLow, int channelHigh)
{
    return channelLow == 0 || (msg.getChannel() >= channelLow && msg.getChannel() <= channelHigh);
}

bool ApplicationCommand::matches(ApplicationState& state, const MidiMessage& msg, int channel) const
{
    return matches(state, msg, channel, channel);
}

bool ApplicationCommand::matches(ApplicationState& state, const MidiMessage& msg, int channelLow, int channelHigh) const
{
    switch (command_)
    {
        case VOICE:
            return checkChannel(msg, channelLow, channelHigh) &&
                (msg.isNoteOnOrOff() || msg.isAftertouch() || msg.isController() ||
                 msg.isProgramChange() || msg.isChannelPressure() || msg.isPitchWheel());
        case NOTE:
            return checkChannel(msg, channelLow, channelHigh) && msg.isNoteOnOrOff();
        case NOTE_ON:
            return checkChannel(msg, channelLow, channelHigh) && msg.isNoteOn() &&
                (opts_.isEmpty() || state.selectorMatches(opts_[0], msg.getNoteNumber(), true));
        case NOTE_OFF:
            return checkChannel(msg, channelLow, channelHigh) && msg.isNoteOff() &&
                (opts_.isEmpty() || state.selectorMatches(opts_[0], msg.getNoteNumber(), true));
        case POLY_PRESSURE:
            return checkChannel(msg, channelLow, channelHigh) && msg.isAftertouch() &&
                (opts_.isEmpty() || state.selectorMatches(opts_[0], msg.getNoteNumber(), true));
        case CONTROL_CHANGE:
            return checkChannel(msg, channelLow, channelHigh) && msg.isController() &&
                (opts_.isEmpty() || state.selectorMatches(opts_[0], msg.getControllerNumber(), false));
        case CONTROL_CHANGE_14BIT:
            // a 14-bit CC is an MSB controller (0-31) paired with its LSB (32-63)
            if (!checkChannel(msg, channelLow, channelHigh) || !msg.isController())
            {
                return false;
            }
            if (opts_.isEmpty())
            {
                return msg.getControllerNumber() < 64;
            }
            else
            {
                // match the MSB controller or its LSB partner against the selector
                const int cc = msg.getControllerNumber();
                const int msbIndex = (cc < 32) ? cc : (cc < 64 ? cc - 32 : -1);
                return msbIndex >= 0 && state.selectorMatches(opts_[0], msbIndex, false);
            }
        case NRPN:
            // the NRPN parameter-select (98/99) and data-entry (6/38) controllers
            return checkChannel(msg, channelLow, channelHigh) && msg.isController() &&
                (msg.getControllerNumber() == 98 || msg.getControllerNumber() == 99 ||
                 msg.getControllerNumber() == 6  || msg.getControllerNumber() == 38);
        case RPN:
            // the RPN parameter-select (100/101) and data-entry (6/38) controllers
            return checkChannel(msg, channelLow, channelHigh) && msg.isController() &&
                (msg.getControllerNumber() == 100 || msg.getControllerNumber() == 101 ||
                 msg.getControllerNumber() == 6   || msg.getControllerNumber() == 38);
        case PROGRAM_CHANGE:
            return checkChannel(msg, channelLow, channelHigh) && msg.isProgramChange() &&
                (opts_.isEmpty() || state.selectorMatches(opts_[0], msg.getProgramChangeNumber(), false));
        case CHANNEL_PRESSURE:
            return checkChannel(msg, channelLow, channelHigh) && msg.isChannelPressure();
        case PITCH_BEND:
            return checkChannel(msg, channelLow, channelHigh) && msg.isPitchWheel();

        case SYSTEM_REALTIME:
            return msg.isMidiClock() || msg.isMidiStart() || msg.isMidiStop() || msg.isMidiContinue() ||
                msg.isActiveSense() || (msg.getRawDataSize() == 1 && msg.getRawData()[0] == 0xff);
        case CLOCK:
            return msg.isMidiClock();
        case START:
            return msg.isMidiStart();
        case STOP:
            return msg.isMidiStop();
        case CONTINUE:
            return msg.isMidiContinue();
        case ACTIVE_SENSING:
            return msg.isActiveSense();
        case RESET:
            return msg.getRawDataSize() == 1 && msg.getRawData()[0] == 0xff;

        case SYSTEM_COMMON:
            return msg.isSysEx() || msg.isQuarterFrame() || msg.isSongPositionPointer() ||
                (msg.getRawDataSize() == 2 && msg.getRawData()[0] == 0xf3) ||
                (msg.getRawDataSize() == 1 && msg.getRawData()[0] == 0xf6);
        case SYSTEM_EXCLUSIVE:
            return msg.isSysEx();
        case TIME_CODE:
            return msg.isQuarterFrame();
        case SONG_POSITION:
            return msg.isSongPositionPointer();
        case SONG_SELECT:
            return msg.getRawDataSize() == 2 && msg.getRawData()[0] == 0xf3;
        case TUNE_REQUEST:
            return msg.getRawDataSize() == 1 && msg.getRawData()[0] == 0xf6;

        case NOTE_RANGE:
            return checkChannel(msg, channelLow, channelHigh) && (msg.isNoteOnOrOff() || msg.isAftertouch()) &&
                msg.getNoteNumber() >= state.asNoteNumber(opts_[0]) &&
                msg.getNoteNumber() <= state.asNoteNumber(opts_[1]);
        case VELOCITY_RANGE:
            if (!checkChannel(msg, channelLow, channelHigh))
            {
                return false;
            }
            // always pass note-offs so a velocity split can't leave notes stuck
            if (msg.isNoteOff())
            {
                return true;
            }
            return msg.isNoteOn() &&
                msg.getVelocity() >= state.asDecOrHex7BitValue(opts_[0]) &&
                msg.getVelocity() <= state.asDecOrHex7BitValue(opts_[1]);

        case MPE_MASTER:
        {
            // pass messages on the master channel of the given MPE zone
            mpe::Zone zone;
            return mpe::parseZone(opts_[0], zone) && msg.getChannel() == zone.masterChannel();
        }
        case MPE_MEMBER:
        {
            // pass messages on any member channel of the given MPE zone
            mpe::Zone zone;
            return mpe::parseZone(opts_[0], zone) && zone.memberIndexOf(msg.getChannel()) >= 0;
        }
        case MPE_ZONE:
        {
            // pass a whole zone: its master channel and all its member channels
            mpe::Zone zone;
            return mpe::parseZone(opts_[0], zone) && zone.contains(msg.getChannel());
        }

        default:
            return false;
    }
}

bool ApplicationCommand::transform(ApplicationState& state, MidiMessage& msg) const
{
    const double timestamp = msg.getTimeStamp();

    switch (command_)
    {
        case CHANNEL_MAP:
            if (msg.getChannel() > 0 &&
                msg.getChannel() == jlimit(1, 16, state.asDecOrHexIntValue(opts_[0])))
            {
                msg.setChannel(jlimit(1, 16, state.asDecOrHexIntValue(opts_[1])));
            }
            break;
        case CHANNEL_SET:
            if (msg.getChannel() > 0)
            {
                msg.setChannel(jlimit(1, 16, state.asDecOrHexIntValue(opts_[0])));
            }
            break;
        case CHANNEL_ADD:
            if (msg.getChannel() > 0)
            {
                int offset = state.asDecOrHexIntValue(opts_[0]);
                int ch = ((msg.getChannel() - 1 + offset) % 16 + 16) % 16;
                msg.setChannel(ch + 1);
            }
            break;
        case TRANSPOSE:
            if (msg.isNoteOnOrOff() || msg.isAftertouch())
            {
                int note = msg.getNoteNumber() + state.asDecOrHexIntValue(opts_[0]);
                if (note < 0 || note > 127)
                {
                    return false;
                }
                msg.setNoteNumber(note);
            }
            break;
        case NOTE_MAP:
            if ((msg.isNoteOnOrOff() || msg.isAftertouch()) &&
                msg.getNoteNumber() == state.asNoteNumber(opts_[0]))
            {
                msg.setNoteNumber(state.asNoteNumber(opts_[1]));
            }
            break;
        case NOTE_TO_CC:
            // a note becomes a Control Change: note-on velocity is the value,
            // note-off sends value 0
            if (msg.isNoteOnOrOff() && msg.getNoteNumber() == state.asNoteNumber(opts_[0]))
            {
                const int value = msg.isNoteOn() ? msg.getVelocity() : 0;
                msg = MidiMessage::controllerEvent(msg.getChannel(), state.asDecOrHex7BitValue(opts_[1]), value);
                msg.setTimeStamp(timestamp);
            }
            break;
        case CC_TO_NOTE:
            // a Control Change becomes a note: a value of 64 or more triggers a
            // note-on (the value is the velocity), below 64 a note-off
            if (msg.isController() && msg.getControllerNumber() == state.asDecOrHex7BitValue(opts_[0]))
            {
                const int note = state.asNoteNumber(opts_[1]);
                const int value = msg.getControllerValue();
                msg = value >= 64 ? MidiMessage::noteOn(msg.getChannel(), note, (uint8)value)
                                  : MidiMessage::noteOff(msg.getChannel(), note, (uint8)0);
                msg.setTimeStamp(timestamp);
            }
            break;
        case NOTE_TO_PROGRAM:
            // a note-on becomes a Program Change; the note-off is dropped
            if (msg.isNoteOnOrOff() && msg.getNoteNumber() == state.asNoteNumber(opts_[0]))
            {
                if (!msg.isNoteOn())
                {
                    return false;
                }
                msg = MidiMessage::programChange(msg.getChannel(), state.asDecOrHex7BitValue(opts_[1]));
                msg.setTimeStamp(timestamp);
            }
            break;
        case SCALE:
            // snap the note to the nearest note of the given key and scale
            if (msg.isNoteOnOrOff() || msg.isAftertouch())
            {
                const uint16 mask = scaleMask(opts_[1]);
                if (mask == 0)
                {
                    break;   // unrecognised scale, leave the note untouched
                }
                const int snapped = snapToScale(msg.getNoteNumber(), parsePitchClass(state, opts_[0]), mask);
                if (snapped < 0)
                {
                    return false;
                }
                msg.setNoteNumber(snapped);
            }
            break;
        case VELOCITY_SCALE:
            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                int v = roundToInt(msg.getVelocity() * opts_[0].getFloatValue());
                msg = MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(), (uint8)jlimit(1, 127, v));
                msg.setTimeStamp(timestamp);
            }
            break;
        case VELOCITY_SET:
            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                int v = jlimit(1, 127, state.asDecOrHexIntValue(opts_[0]));
                msg = MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(), (uint8)v);
                msg.setTimeStamp(timestamp);
            }
            break;
        case VELOCITY_ADD:
            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                int v = jlimit(1, 127, (int)msg.getVelocity() + state.asDecOrHexIntValue(opts_[0]));
                msg = MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(), (uint8)v);
                msg.setTimeStamp(timestamp);
            }
            break;
        case VELOCITY_CURVE:
            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                int v = jlimit(1, 127, applyGammaCurve(msg.getVelocity(), 127, opts_[0].getDoubleValue()));
                msg = MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(), (uint8)v);
                msg.setTimeStamp(timestamp);
            }
            break;
        case CONTROL_CHANGE_MAP:
            if (msg.isController() &&
                msg.getControllerNumber() == state.asDecOrHex7BitValue(opts_[0]))
            {
                msg = MidiMessage::controllerEvent(msg.getChannel(),
                                                   state.asDecOrHex7BitValue(opts_[1]),
                                                   msg.getControllerValue());
                msg.setTimeStamp(timestamp);
            }
            break;
        case CONTROL_CHANGE_ADD:
            if (msg.isController() &&
                msg.getControllerNumber() == state.asDecOrHex7BitValue(opts_[0]))
            {
                int v = jlimit(0, 127, msg.getControllerValue() + state.asDecOrHexIntValue(opts_[1]));
                msg = MidiMessage::controllerEvent(msg.getChannel(), msg.getControllerNumber(), v);
                msg.setTimeStamp(timestamp);
            }
            break;
        case CONTROL_CHANGE_SCALE:
            if (msg.isController() &&
                msg.getControllerNumber() == state.asDecOrHex7BitValue(opts_[0]))
            {
                int v = jlimit(0, 127, roundToInt(msg.getControllerValue() * opts_[1].getFloatValue()));
                msg = MidiMessage::controllerEvent(msg.getChannel(), msg.getControllerNumber(), v);
                msg.setTimeStamp(timestamp);
            }
            break;
        case CONTROL_CHANGE_CURVE:
            if (msg.isController() &&
                msg.getControllerNumber() == state.asDecOrHex7BitValue(opts_[0]))
            {
                int v = applyGammaCurve(msg.getControllerValue(), 127, opts_[1].getDoubleValue());
                msg = MidiMessage::controllerEvent(msg.getChannel(), msg.getControllerNumber(), v);
                msg.setTimeStamp(timestamp);
            }
            break;
        case PROGRAM_CHANGE_MAP:
            if (msg.isProgramChange() &&
                msg.getProgramChangeNumber() == state.asDecOrHex7BitValue(opts_[0]))
            {
                msg = MidiMessage::programChange(msg.getChannel(), state.asDecOrHex7BitValue(opts_[1]));
                msg.setTimeStamp(timestamp);
            }
            break;
        case PROGRAM_CHANGE_ADD:
            if (msg.isProgramChange())
            {
                int p = jlimit(0, 127, msg.getProgramChangeNumber() + state.asDecOrHexIntValue(opts_[0]));
                msg = MidiMessage::programChange(msg.getChannel(), p);
                msg.setTimeStamp(timestamp);
            }
            break;
        case PITCH_BEND_ADD:
            if (msg.isPitchWheel())
            {
                int pb = jlimit(0, 16383, msg.getPitchWheelValue() + state.asDecOrHexIntValue(opts_[0]));
                msg = MidiMessage::pitchWheel(msg.getChannel(), pb);
                msg.setTimeStamp(timestamp);
            }
            break;
        case PITCH_BEND_SCALE:
            if (msg.isPitchWheel())
            {
                // scale the bend around the centre so the bend depth is adjusted
                int pb = jlimit(0, 16383, roundToInt(8192 + (msg.getPitchWheelValue() - 8192) * opts_[0].getFloatValue()));
                msg = MidiMessage::pitchWheel(msg.getChannel(), pb);
                msg.setTimeStamp(timestamp);
            }
            break;
        case PITCH_BEND_SET:
            if (msg.isPitchWheel())
            {
                int pb = jlimit(0, 16383, state.asDecOrHexIntValue(opts_[0]));
                msg = MidiMessage::pitchWheel(msg.getChannel(), pb);
                msg.setTimeStamp(timestamp);
            }
            break;
        case CHANNEL_PRESSURE_ADD:
            if (msg.isChannelPressure())
            {
                int v = jlimit(0, 127, msg.getChannelPressureValue() + state.asDecOrHexIntValue(opts_[0]));
                msg = MidiMessage::channelPressureChange(msg.getChannel(), v);
                msg.setTimeStamp(timestamp);
            }
            break;
        case CHANNEL_PRESSURE_SCALE:
            if (msg.isChannelPressure())
            {
                int v = jlimit(0, 127, roundToInt(msg.getChannelPressureValue() * opts_[0].getFloatValue()));
                msg = MidiMessage::channelPressureChange(msg.getChannel(), v);
                msg.setTimeStamp(timestamp);
            }
            break;
        case CHANNEL_PRESSURE_SET:
            if (msg.isChannelPressure())
            {
                int v = jlimit(0, 127, state.asDecOrHexIntValue(opts_[0]));
                msg = MidiMessage::channelPressureChange(msg.getChannel(), v);
                msg.setTimeStamp(timestamp);
            }
            break;
        case CHANNEL_PRESSURE_CURVE:
            if (msg.isChannelPressure())
            {
                int v = applyGammaCurve(msg.getChannelPressureValue(), 127, opts_[0].getDoubleValue());
                msg = MidiMessage::channelPressureChange(msg.getChannel(), v);
                msg.setTimeStamp(timestamp);
            }
            break;
        default:
            // JAVASCRIPT and JAVASCRIPT_FILE are handled by ApplicationState
            break;
    }

    return true;
}
