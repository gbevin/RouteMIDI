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

#include "Latch.h"

void LatchState::process(bool hold, const MidiMessage& msg, Array<MidiMessage>& output)
{
    const int channel = msg.getChannel();

    if (msg.isNoteOn())
    {
        const int note = msg.getNoteNumber();
        if (hold)
        {
            // the first key of a new gesture replaces the held chord
            if (heldCount_ == 0)
            {
                releaseLatched(output, msg.getTimeStamp());
            }
            if (!held_[channel - 1][note])
            {
                held_[channel - 1][note] = true;
                ++heldCount_;
            }
            latched_[channel - 1][note] = true;
            output.add(msg);
        }
        else if (latched_[channel - 1][note])
        {
            // toggle off: release the sounding note
            latched_[channel - 1][note] = false;
            MidiMessage off = MidiMessage::noteOff(channel, note);
            off.setTimeStamp(msg.getTimeStamp());
            output.add(off);
        }
        else
        {
            // toggle on: let the note through and remember it
            latched_[channel - 1][note] = true;
            output.add(msg);
        }
    }
    else if (msg.isNoteOff())
    {
        // swallow the note-off so the note stays latched; in hold mode the key
        // is no longer physically down
        const int note = msg.getNoteNumber();
        if (hold && held_[channel - 1][note])
        {
            held_[channel - 1][note] = false;
            --heldCount_;
        }
    }
    else
    {
        output.add(msg);
    }
}

void LatchState::reset(Array<MidiMessage>& output, double timestamp)
{
    releaseLatched(output, timestamp);
    clear();
}

void LatchState::clear()
{
    for (int c = 0; c < 16; ++c)
    {
        for (int n = 0; n < 128; ++n)
        {
            latched_[c][n] = false;
            held_[c][n] = false;
        }
    }
    heldCount_ = 0;
}

void LatchState::releaseLatched(Array<MidiMessage>& output, double timestamp)
{
    for (int c = 0; c < 16; ++c)
    {
        for (int n = 0; n < 128; ++n)
        {
            if (latched_[c][n])
            {
                latched_[c][n] = false;
                MidiMessage off = MidiMessage::noteOff(c + 1, n);
                off.setTimeStamp(timestamp);
                output.add(off);
            }
        }
    }
}
