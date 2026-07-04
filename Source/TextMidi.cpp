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

#include "TextMidi.h"

namespace textmidi
{

uint8 limit7Bit(int value)
{
    return (uint8)jlimit(0, 0x7f, value);
}

uint16 limit14Bit(int value)
{
    return (uint16)jlimit(0, 0x3fff, value);
}

uint8 asNoteNumber(const String& value, const Format& format)
{
    if (value.length() >= 2)
    {
        const String v = value.toUpperCase();
        String first = v.substring(0, 1);
        if (first.containsOnly("CDEFGABH") && v.substring(v.length() - 1).containsOnly("1234567890"))
        {
            int note = 0;
            switch (first[0])
            {
                case 'C': note = 0; break;
                case 'D': note = 2; break;
                case 'E': note = 4; break;
                case 'F': note = 5; break;
                case 'G': note = 7; break;
                case 'A': note = 9; break;
                case 'B': note = 11; break;
                case 'H': note = 11; break;
            }

            if (v[1] == 'B')
            {
                note -= 1;
            }
            else if (v[1] == '#')
            {
                note += 1;
            }

            note += (v.getTrailingIntValue() + 5 - format.octaveMiddleC) * 12;

            return limit7Bit(note);
        }
    }

    return limit7Bit(asDecOrHexIntValue(value, format));
}

uint8 asDecOrHex7BitValue(const String& value, const Format& format)
{
    return limit7Bit(asDecOrHexIntValue(value, format));
}

uint16 asDecOrHex14BitValue(const String& value, const Format& format)
{
    return limit14Bit(asDecOrHexIntValue(value, format));
}

int asDecOrHexIntValue(const String& value, const Format& format)
{
    if (value.endsWithIgnoreCase("H"))
    {
        return value.dropLastCharacters(1).getHexValue32();
    }
    else if (value.endsWithIgnoreCase("M"))
    {
        return value.getIntValue();
    }
    else if (format.hexadecimal)
    {
        return value.getHexValue32();
    }
    else
    {
        return value.getIntValue();
    }
}

String output7BitAsHex(int value)
{
    return String::toHexString(value).paddedLeft('0', 2).toUpperCase();
}

String output7Bit(int value, const Format& format)
{
    return format.hexadecimal ? output7BitAsHex(value) : String(value);
}

String output14BitAsHex(int value)
{
    return String::toHexString(value).paddedLeft('0', 4).toUpperCase();
}

String output14Bit(int value, const Format& format)
{
    return format.hexadecimal ? output14BitAsHex(value) : String(value);
}

String outputNote(const MidiMessage& msg, const Format& format)
{
    if (format.noteNumbers)
    {
        return output7Bit(msg.getNoteNumber(), format).paddedLeft(' ', 4);
    }
    return MidiMessage::getMidiNoteName(msg.getNoteNumber(), true, true, format.octaveMiddleC).paddedLeft(' ', 4);
}

String messageToText(const MidiMessage& msg, const Format& format)
{
    String line;

    if (msg.getChannel() > 0)
    {
        line << "channel " << output7Bit(msg.getChannel(), format).paddedLeft(' ', 2) << "   ";
    }

    if (msg.isNoteOn())
    {
        line << "note-on         " << outputNote(msg, format) << " " << output7Bit(msg.getVelocity(), format).paddedLeft(' ', 3);
    }
    else if (msg.isNoteOff())
    {
        line << "note-off        " << outputNote(msg, format) << " " << output7Bit(msg.getVelocity(), format).paddedLeft(' ', 3);
    }
    else if (msg.isAftertouch())
    {
        line << "poly-pressure   " << outputNote(msg, format) << " " << output7Bit(msg.getAfterTouchValue(), format).paddedLeft(' ', 3);
    }
    else if (msg.isController())
    {
        line << "control-change   " << output7Bit(msg.getControllerNumber(), format).paddedLeft(' ', 3) << "   "
             << output7Bit(msg.getControllerValue(), format).paddedLeft(' ', 3);
    }
    else if (msg.isProgramChange())
    {
        line << "program-change   " << output7Bit(msg.getProgramChangeNumber(), format).paddedLeft(' ', 7);
    }
    else if (msg.isChannelPressure())
    {
        line << "channel-pressure " << output7Bit(msg.getChannelPressureValue(), format).paddedLeft(' ', 7);
    }
    else if (msg.isPitchWheel())
    {
        line << "pitch-bend       " << output14Bit(msg.getPitchWheelValue(), format).paddedLeft(' ', 7);
    }
    else if (msg.isMidiClock())
    {
        line << "midi-clock";
    }
    else if (msg.isMidiStart())
    {
        line << "start";
    }
    else if (msg.isMidiStop())
    {
        line << "stop";
    }
    else if (msg.isMidiContinue())
    {
        line << "continue";
    }
    else if (msg.isActiveSense())
    {
        line << "active-sensing";
    }
    else if (msg.getRawDataSize() == 1 && msg.getRawData()[0] == 0xff)
    {
        line << "reset";
    }
    else if (msg.isSysEx())
    {
        // the bytes are emitted in hexadecimal so SendMIDI can read them back
        line << "system-exclusive";
        if (!format.hexadecimal)
        {
            line << " hex";
        }
        const uint8* data = msg.getSysExData();
        const int size = msg.getSysExDataSize();
        for (int i = 0; i < size; ++i)
        {
            line << " " << output7BitAsHex(data[i]);
        }
        if (!format.hexadecimal)
        {
            line << " dec";
        }
    }
    else if (msg.isQuarterFrame())
    {
        line << "time-code " << output7Bit(msg.getQuarterFrameSequenceNumber(), format).paddedLeft(' ', 2) << " "
             << output7Bit(msg.getQuarterFrameValue(), format);
    }
    else if (msg.isSongPositionPointer())
    {
        line << "song-position " << output14Bit(msg.getSongPositionPointerMidiBeat(), format).paddedLeft(' ', 5);
    }
    else if (msg.getRawDataSize() == 2 && msg.getRawData()[0] == 0xf3)
    {
        line << "song-select " << output7Bit(msg.getRawData()[1], format).paddedLeft(' ', 3);
    }
    else if (msg.getRawDataSize() == 1 && msg.getRawData()[0] == 0xf6)
    {
        line << "tune-request";
    }
    else
    {
        line << msg.getDescription();
    }

    return line;
}

void parseTextMidi(const StringArray& tokens, const Format& format, Array<MidiMessage>& output)
{
    int channel = 1;
    bool lineHex = format.hexadecimal;
    int i = 0;

    auto num = [&](const String& s) -> int
    {
        if (s.endsWithIgnoreCase("H"))
        {
            return s.dropLastCharacters(1).getHexValue32();
        }
        if (s.endsWithIgnoreCase("M"))
        {
            return s.getIntValue();
        }
        return lineHex ? s.getHexValue32() : s.getIntValue();
    };
    auto next = [&]() -> String { return i < tokens.size() ? tokens[i++] : String(); };
    // note names resolve against the global format (numeric notes follow the
    // global hex setting, not the line-local hex/dec toggles)
    auto note = [&]() -> int { return asNoteNumber(next(), format); };

    while (i < tokens.size())
    {
        const String tok = tokens[i++].toLowerCase();

        if (tok == "hex")                                     { lineHex = true; }
        else if (tok == "dec")                                { lineHex = false; }
        else if (tok == "channel" || tok == "ch")             { channel = jlimit(1, 16, num(next())); }
        else if (tok == "note-on" || tok == "on")
        {
            const int n = note();
            output.add(MidiMessage::noteOn(channel, n, (uint8)jlimit(0, 127, num(next()))));
        }
        else if (tok == "note-off" || tok == "off")
        {
            const int n = note();
            output.add(MidiMessage::noteOff(channel, n, (uint8)jlimit(0, 127, num(next()))));
        }
        else if (tok == "poly-pressure" || tok == "pp")
        {
            const int n = note();
            output.add(MidiMessage::aftertouchChange(channel, n, jlimit(0, 127, num(next()))));
        }
        else if (tok == "control-change" || tok == "cc")
        {
            const int n = jlimit(0, 127, num(next()));
            output.add(MidiMessage::controllerEvent(channel, n, jlimit(0, 127, num(next()))));
        }
        else if (tok == "control-change-14" || tok == "cc14")
        {
            // a 14-bit CC in the SendMIDI/ReceiveMIDI vocabulary: the value is
            // sent as its MSB (controller 0-31) and LSB (32 higher) pair
            const int n = jlimit(0, 31, num(next()));
            const int v = jlimit(0, 16383, num(next()));
            output.add(MidiMessage::controllerEvent(channel, n, (v >> 7) & 0x7f));
            output.add(MidiMessage::controllerEvent(channel, n + 32, v & 0x7f));
        }
        else if (tok == "program-change" || tok == "pc")      { output.add(MidiMessage::programChange(channel, jlimit(0, 127, num(next())))); }
        else if (tok == "channel-pressure" || tok == "cp")    { output.add(MidiMessage::channelPressureChange(channel, jlimit(0, 127, num(next())))); }
        else if (tok == "pitch-bend" || tok == "pb")          { output.add(MidiMessage::pitchWheel(channel, jlimit(0, 16383, num(next())))); }
        else if (tok == "midi-clock" || tok == "mc")          { output.add(MidiMessage::midiClock()); }
        else if (tok == "start")                              { output.add(MidiMessage::midiStart()); }
        else if (tok == "stop")                               { output.add(MidiMessage::midiStop()); }
        else if (tok == "continue" || tok == "cont")          { output.add(MidiMessage::midiContinue()); }
        else if (tok == "active-sensing" || tok == "as")      { output.add(MidiMessage(0xfe)); }
        else if (tok == "reset" || tok == "rst")              { output.add(MidiMessage(0xff)); }
        else if (tok == "song-position" || tok == "spp")      { output.add(MidiMessage::songPositionPointer(jlimit(0, 16383, num(next())))); }
        else if (tok == "song-select" || tok == "ss")         { output.add(MidiMessage(0xf3, jlimit(0, 127, num(next())))); }
        else if (tok == "tune-request" || tok == "tun")       { output.add(MidiMessage(0xf6)); }
        else if (tok == "time-code" || tok == "tc")
        {
            const int type = jlimit(0, 7, num(next()));
            output.add(MidiMessage(0xf1, ((type << 4) | jlimit(0, 15, num(next()))) & 0x7f));
        }
        else if (tok == "system-exclusive" || tok == "syx")
        {
            // the rest of the line is the SysEx data (with optional hex/dec toggles)
            Array<uint8> bytes;
            while (i < tokens.size())
            {
                const String b = tokens[i++];
                if (b.equalsIgnoreCase("hex"))      { lineHex = true; }
                else if (b.equalsIgnoreCase("dec")) { lineHex = false; }
                else                                { bytes.add((uint8)(num(b) & 0x7f)); }
            }
            if (!bytes.isEmpty())
            {
                output.add(MidiMessage::createSysExMessage(bytes.getRawDataPointer(), bytes.size()));
            }
        }
        // unknown tokens are ignored
    }
}

} // namespace textmidi
