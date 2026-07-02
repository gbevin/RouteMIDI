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

// Forces monophony: of all the notes physically held on a channel, only the one
// chosen by the priority rule sounds at a time. Playing a note that wins priority
// retriggers the sound; releasing the sounding note falls back to the next-priority
// held note. Waiting notes never sound (and so need no note-off of their own). The
// state is kept per input so it survives across messages.
class MonoState
{
public:
    enum Priority { Last, Low, High };

    MonoState() { reset(); }

    void reset();

    // processes one message, appending the resulting messages to output
    void process(Priority priority, const MidiMessage& msg, Array<MidiMessage>& output);

private:
    // the held note that wins priority on the channel, or -1 when none are held
    int priorityNote(Priority priority, int c) const;

    static void emit(Array<MidiMessage>& output, bool on, int channel, int note, int velocity, double ts);

    bool held[16][128];      // which notes are physically down, per channel
    int  velocity[16][128];  // their note-on velocity, for retrigger on fallback
    int  order[16][128];     // trigger order, for last-note priority
    int  sounding[16];       // the note currently sounding on each channel, -1 = none
    int  counter;            // monotonic trigger counter
};
