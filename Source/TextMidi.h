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

// The text MIDI codec: renders MIDI messages as the SendMIDI/ReceiveMIDI
// compatible text format and parses that format back into messages, plus the
// note-name and dec/hex value conventions shared with the command-line options.
namespace textmidi
{

// how numbers and notes are rendered and parsed
struct Format
{
    bool hexadecimal { false };    // numbers are hexadecimal unless suffixed
                                   // ("...M" forces decimal, "...H" forces hex)
    bool noteNumbers { false };    // print notes as numbers instead of names
    int  octaveMiddleC { 3 };      // octave number that denotes middle C
};

uint8 limit7Bit(int value);
uint16 limit14Bit(int value);

// --- value parsing, shared by the codec and the command-line options ---------
uint8  asNoteNumber(const String& value, const Format& format);
uint8  asDecOrHex7BitValue(const String& value, const Format& format);
uint16 asDecOrHex14BitValue(const String& value, const Format& format);
int    asDecOrHexIntValue(const String& value, const Format& format);

// --- value formatting ---------------------------------------------------------
String output7BitAsHex(int value);
String output7Bit(int value, const Format& format);
String output14BitAsHex(int value);
String output14Bit(int value, const Format& format);
String outputNote(const MidiMessage& msg, const Format& format);

// renders one MIDI message as a line of text
String messageToText(const MidiMessage& msg, const Format& format);

// parses the tokens of one line of text into MIDI messages (a line can carry
// several messages and inline hex/dec toggles); unknown tokens are ignored
void parseTextMidi(const StringArray& tokens, const Format& format, Array<MidiMessage>& output);

} // namespace textmidi
