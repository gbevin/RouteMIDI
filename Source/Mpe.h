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

#include <vector>

// Helpers describing a MIDI Polyphonic Expression (MPE) zone and the per-input
// state needed to expand a regular channel into one. An MPE zone has a single
// master channel carrying zone-wide messages and a contiguous block of member
// channels carrying one note each (with per-note pitch bend, channel pressure
// and CC 74). The Lower Zone has master channel 1 and members counting up from
// channel 2; the Upper Zone has master channel 16 and members counting down
// from channel 15.
namespace mpe
{
    struct Zone
    {
        bool lower { true };   // true = Lower Zone (master 1), false = Upper Zone (master 16)
        int members { 15 };    // number of member channels, 1-15

        int masterChannel() const { return lower ? 1 : 16; }

        // the MIDI channel (1-16) of the index-th member channel (0-based), or
        // 0 when the index is out of range for this zone
        int memberChannel(int index) const
        {
            if (index < 0 || index >= members)
            {
                return 0;
            }
            return lower ? (2 + index) : (15 - index);
        }

        // the 0-based member index of a MIDI channel, or -1 when the channel is
        // not one of this zone's member channels
        int memberIndexOf(int channel) const
        {
            if (lower)
            {
                return (channel >= 2 && channel <= 1 + members) ? channel - 2 : -1;
            }
            return (channel <= 15 && channel >= 16 - members) ? 15 - channel : -1;
        }

        bool isMaster(int channel) const { return channel == masterChannel(); }
        bool contains(int channel) const { return isMaster(channel) || memberIndexOf(channel) >= 0; }
    };

    // Parses a zone token of the form "<side>[:<members>]" where side starts
    // with 'l' (lower) or 'u' (upper), e.g. "lower", "upper", "lower:7", "u:5".
    // The member count defaults to 15 and is clamped to 1-15. Returns false for
    // an unrecognized side.
    inline bool parseZone(const String& token, Zone& out)
    {
        const String t = token.trim().toLowerCase();
        if (t.isEmpty())
        {
            return false;
        }

        const String side = t.upToFirstOccurrenceOf(":", false, false);
        if (side.startsWithChar('l'))
        {
            out.lower = true;
        }
        else if (side.startsWithChar('u'))
        {
            out.lower = false;
        }
        else
        {
            return false;
        }

        if (t.contains(":"))
        {
            out.members = jlimit(1, 15, t.fromFirstOccurrenceOf(":", false, false).getIntValue());
        }
        else
        {
            out.members = 15;
        }
        return true;
    }

    // Appends a standard MPE Configuration Message (RPN 6 on the master channel)
    // announcing the zone's member-channel count, followed by the RPN null to
    // deselect the parameter.
    inline void appendConfigMessage(Array<MidiMessage>& out, const Zone& zone, double timestamp)
    {
        const int ch = zone.masterChannel();
        const MidiMessage msgs[] = {
            MidiMessage::controllerEvent(ch, 101, 0),            // RPN MSB
            MidiMessage::controllerEvent(ch, 100, 6),            // RPN LSB = MPE Configuration
            MidiMessage::controllerEvent(ch, 6, zone.members),   // Data Entry MSB = member count
            MidiMessage::controllerEvent(ch, 101, 127),          // RPN null
            MidiMessage::controllerEvent(ch, 100, 127)
        };
        for (auto m : msgs)
        {
            m.setTimeStamp(timestamp);
            out.add(m);
        }
    }

    // Per-input voice-allocation state used to expand a single channel into an
    // MPE zone: it tracks which member channel each held note was assigned to so
    // that note-offs and per-note expression reach the same channel.
    struct Allocator
    {
        Allocator() { reset(); }

        void reset()
        {
            configSent = false;
            roundRobin = 0;
            for (int i = 0; i < 128; ++i) noteChannel[i] = -1;
            for (int i = 0; i < 17; ++i)  channelNote[i] = -1;
        }

        bool configSent { false };
        int roundRobin { 0 };       // next member index to try, for round-robin allocation
        int noteChannel[128];       // member channel (1-16) holding a note number, -1 = not held
        int channelNote[17];        // note number on each MIDI channel (index 1-16), -1 = free
    };

    // Per-input state used to collapse an MPE zone onto a single channel: it
    // remembers which note each member channel holds (so per-note channel
    // pressure can be turned into polyphonic aftertouch) and the trigger order
    // of those notes (so the most recently played note wins for the channel-wide
    // expression dimensions of pitch bend and timbre).
    struct Collapser
    {
        Collapser() { reset(); }

        void reset()
        {
            order = 0;
            for (int i = 0; i < 17; ++i) { channelNote[i] = -1; noteOrder[i] = 0; }
        }

        // the member channel of the most recently triggered note that is still
        // held, or 0 when no note is held
        int activeChannel() const
        {
            int channel = 0, best = 0;
            for (int c = 1; c < 17; ++c)
            {
                if (channelNote[c] >= 0 && noteOrder[c] > best)
                {
                    best = noteOrder[c];
                    channel = c;
                }
            }
            return channel;
        }

        int order { 0 };            // monotonically increasing note-trigger counter
        int channelNote[17];        // note held on each member channel (index 1-16), -1 = none
        int noteOrder[17];          // trigger order per member channel (higher = more recent)
    };

    // Per-route state used to fan an MPE zone out across a route's output ports,
    // treating each output as one monophonic voice. It maps each active member
    // channel to an output port so a note (and its per-note expression) reaches
    // the same port until it is released or its slot is stolen.
    struct Splitter
    {
        // (re)initialize the per-port tables when the number of output ports of
        // the route becomes known
        void ensureSize(int ports)
        {
            if ((int) portChannel.size() == ports)
            {
                return;
            }
            portChannel.assign((size_t) ports, -1);
            portNote.assign((size_t) ports, -1);
            portOrder.assign((size_t) ports, 0);
            for (int i = 0; i < 17; ++i)
            {
                channelPort[i] = -1;
                rpnMsb[i] = -1;
                rpnLsb[i] = -1;
                rpnSelectionSent[i] = false;
            }
            order = 0;
            roundRobin = 0;
        }

        // the first free port at or after the round-robin cursor, or -1 if all
        // ports are currently in use
        int freePort(int ports)
        {
            for (int k = 0; k < ports; ++k)
            {
                const int p = (roundRobin + k) % ports;
                if (portChannel[(size_t) p] < 0)
                {
                    roundRobin = (p + 1) % ports;
                    return p;
                }
            }
            return -1;
        }

        // the oldest-allocated port, stolen when every port is busy
        int oldestPort(int ports)
        {
            int best = 0;
            for (int p = 1; p < ports; ++p)
                if (portOrder[(size_t) p] < portOrder[(size_t) best]) best = p;
            return best;
        }

        int order { 0 };               // monotonically increasing allocation counter
        int roundRobin { 0 };          // next port index to try when allocating
        int channelPort[17];           // output port assigned to each member channel, -1 = none
        std::vector<int> portChannel;  // member channel occupying each port, -1 = free
        std::vector<int> portNote;     // note number sounding on each port, -1 = none
        std::vector<int> portOrder;    // allocation order per port (for stealing)

        // tracks the RPN selected on each channel so the MPE Configuration
        // Message (RPN 6) can be suppressed without disturbing other RPNs
        int rpnMsb[17];                // last RPN MSB (CC 101) per channel, -1 = none
        int rpnLsb[17];                // last RPN LSB (CC 100) per channel, -1 = none
        bool rpnSelectionSent[17];     // whether the current selection has been forwarded
    };
}
