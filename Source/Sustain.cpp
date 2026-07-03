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

#include "Sustain.h"

void SustainState::process(bool sostenuto, const MidiMessage& msg, Array<MidiMessage>& output)
{
    const int channel = msg.getChannel();
    const int pedalController = sostenuto ? 66 : 64;

    if (msg.isController() && msg.getControllerNumber() == pedalController)
    {
        // the pedal itself is consumed: its effect is applied to the notes
        const bool down = msg.getControllerValue() >= 64;
        const int c = channel - 1;
        if (down && !pedal_[c])
        {
            pedal_[c] = true;
            // sustain applies to every note; sostenuto only to the keys that
            // are down at the moment the pedal is pressed
            for (int n = 0; n < 128; ++n)
            {
                eligible_[c][n] = sostenuto ? keyDown_[c][n] : true;
            }
        }
        else if (!down && pedal_[c])
        {
            pedal_[c] = false;
            releaseSustained(channel, output, msg.getTimeStamp());
            for (int n = 0; n < 128; ++n)
            {
                eligible_[c][n] = false;
            }
        }
    }
    else if (msg.isNoteOn())
    {
        const int note = msg.getNoteNumber();
        keyDown_[channel - 1][note] = true;
        if (sustained_[channel - 1][note])
        {
            // the note is sounding with its key released: retrigger it
            sustained_[channel - 1][note] = false;
            MidiMessage off = MidiMessage::noteOff(channel, note);
            off.setTimeStamp(msg.getTimeStamp());
            output.add(off);
        }
        output.add(msg);
    }
    else if (msg.isNoteOff())
    {
        const int note = msg.getNoteNumber();
        keyDown_[channel - 1][note] = false;
        if (pedal_[channel - 1] && eligible_[channel - 1][note])
        {
            // hold the note-off back until the pedal is lifted
            sustained_[channel - 1][note] = true;
        }
        else
        {
            output.add(msg);
        }
    }
    else
    {
        output.add(msg);
    }
}

void SustainState::reset(Array<MidiMessage>& output, double timestamp)
{
    for (int c = 0; c < 16; ++c)
    {
        releaseSustained(c + 1, output, timestamp);
    }
    clear();
}

void SustainState::clear()
{
    for (int c = 0; c < 16; ++c)
    {
        pedal_[c] = false;
        for (int n = 0; n < 128; ++n)
        {
            keyDown_[c][n] = false;
            eligible_[c][n] = false;
            sustained_[c][n] = false;
        }
    }
}

void SustainState::releaseSustained(int channel, Array<MidiMessage>& output, double timestamp)
{
    for (int n = 0; n < 128; ++n)
    {
        if (sustained_[channel - 1][n])
        {
            sustained_[channel - 1][n] = false;
            MidiMessage off = MidiMessage::noteOff(channel, n);
            off.setTimeStamp(timestamp);
            output.add(off);
        }
    }
}
