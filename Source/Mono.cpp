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

#include "Mono.h"

void MonoState::reset()
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

void MonoState::process(Priority priority, const MidiMessage& msg, Array<MidiMessage>& output)
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

int MonoState::priorityNote(Priority priority, int c) const
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

void MonoState::emit(Array<MidiMessage>& output, bool on, int channel, int note, int velocity, double ts)
{
    MidiMessage m = on ? MidiMessage::noteOn(channel, note, (uint8) velocity)
                       : MidiMessage::noteOff(channel, note, (uint8) velocity);
    m.setTimeStamp(ts);
    output.add(m);
}
