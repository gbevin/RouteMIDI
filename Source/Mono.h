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

    void reset()
    {
        counter = 0;
        for (int c = 0; c < 16; ++c)
        {
            sounding[c] = -1;
            for (int n = 0; n < 128; ++n)
            {
                held[c][n] = false;
                velocity[c][n] = 0;
                order[c][n] = 0;
            }
        }
    }

    // processes one message, appending the resulting messages to output
    void process(Priority priority, const MidiMessage& msg, Array<MidiMessage>& output)
    {
        const int channel = msg.getChannel();
        const int c = channel - 1;
        const double ts = msg.getTimeStamp();

        if (msg.isNoteOn())
        {
            const int n = msg.getNoteNumber();
            held[c][n] = true;
            velocity[c][n] = msg.getVelocity();
            order[c][n] = ++counter;

            if (priorityNote(priority, c) == n)
            {
                if (sounding[c] >= 0)
                {
                    emit(output, false, channel, sounding[c], 0, ts);   // stop the current note
                }
                emit(output, true, channel, n, velocity[c][n], ts);     // (re)start this one
                sounding[c] = n;
            }
            // otherwise this note waits in the held set without sounding
        }
        else if (msg.isNoteOff())
        {
            const int n = msg.getNoteNumber();
            if (! held[c][n])
            {
                return;                          // never tracked; nothing to release
            }
            held[c][n] = false;

            if (sounding[c] != n)
            {
                return;                          // a waiting note left; the sound is unchanged
            }

            emit(output, false, channel, n, msg.getVelocity(), ts);
            sounding[c] = priorityNote(priority, c);
            if (sounding[c] >= 0)
            {
                emit(output, true, channel, sounding[c], velocity[c][sounding[c]], ts);
            }
        }
        else
        {
            output.add(msg);                     // everything else passes through
        }
    }

private:
    // the held note that wins priority on the channel, or -1 when none are held
    int priorityNote(Priority priority, int c) const
    {
        int best = -1;
        for (int n = 0; n < 128; ++n)
        {
            if (! held[c][n]) continue;
            if (best < 0
                || (priority == Low  && n < best)
                || (priority == High && n > best)
                || (priority == Last && order[c][n] > order[c][best]))
            {
                best = n;
            }
        }
        return best;
    }

    static void emit(Array<MidiMessage>& output, bool on, int channel, int note, int velocity, double ts)
    {
        MidiMessage m = on ? MidiMessage::noteOn(channel, note, (uint8) velocity)
                           : MidiMessage::noteOff(channel, note, (uint8) velocity);
        m.setTimeStamp(ts);
        output.add(m);
    }

    bool held[16][128];      // which notes are physically down, per channel
    int  velocity[16][128];  // their note-on velocity, for retrigger on fallback
    int  order[16][128];     // trigger order, for last-note priority
    int  sounding[16];       // the note currently sounding on each channel, -1 = none
    int  counter;            // monotonic trigger counter
};
