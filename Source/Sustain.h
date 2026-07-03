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

// Pedal pre-application: applies the sustain (CC 64) or sostenuto (CC 66) pedal
// to the note stream itself, for synths whose MIDI implementation ignores the
// pedal. The pedal message is consumed; the note-offs it suppresses are held
// back and sent when the pedal is lifted. Sustain holds every note-off that
// arrives while the pedal is down; sostenuto only holds the notes whose keys
// were down at the moment the pedal was pressed. Re-striking a note that is
// sounding with its key released retriggers it (a note-off is sent first).
// The pedal and the notes are tracked per channel; the state is kept per input
// so it survives across messages.
class SustainState
{
public:
    // processes one message, appending its result to output (none, one or
    // several messages); sostenuto selects the pedal that is applied
    void process(bool sostenuto, const MidiMessage& msg, Array<MidiMessage>& output);

    // releases every pedal-held note (used by the panic safety net); appends
    // the note-offs to output and forgets all state
    void reset(Array<MidiMessage>& output, double timestamp);

    // forgets the pedal and every tracked note without emitting anything
    void clear();

private:
    void releaseSustained(int channel, Array<MidiMessage>& output, double timestamp);

    bool pedal_[16] {};             // pedal currently down, per channel
    bool keyDown_[16][128] {};      // keys currently physically down
    bool eligible_[16][128] {};     // notes this pedal press applies to
    bool sustained_[16][128] {};    // sounding notes whose note-off is held back
};
