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

// Note latch: keeps notes sounding after their key is released. In toggle mode
// each press of a note flips it on or off. In hold mode the played note or chord
// keeps sounding until a new note starts a fresh gesture and replaces it, like an
// arpeggiator hold. Incoming note-offs are swallowed in both modes; the state is
// kept per input so it survives across messages.
class LatchState
{
public:
    // processes one message, appending its result to output (none, one or several
    // messages); hold selects hold mode, otherwise toggle mode is used
    void process(bool hold, const MidiMessage& msg, Array<MidiMessage>& output);

    // releases every latched note (used by the panic safety net); appends the
    // note-offs to output and forgets all state
    void reset(Array<MidiMessage>& output, double timestamp);

    // forgets all latched and held notes without emitting anything
    void clear();

private:
    void releaseLatched(Array<MidiMessage>& output, double timestamp);

    bool latched_[16][128] {};  // notes currently sounding, per channel and note
    bool held_[16][128] {};     // keys currently physically down, for hold mode
    int heldCount_ { 0 };       // number of keys currently down, for hold mode
};
