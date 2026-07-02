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

#include "ChannelBucket.h"

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
        int memberChannel(int index) const;

        // the 0-based member index of a MIDI channel, or -1 when the channel is
        // not one of this zone's member channels
        int memberIndexOf(int channel) const;

        bool isMaster(int channel) const { return channel == masterChannel(); }
        bool contains(int channel) const { return isMaster(channel) || memberIndexOf(channel) >= 0; }
    };

    // Parses a zone token of the form "<side>[:<members>]" where side starts
    // with 'l' (lower) or 'u' (upper), e.g. "lower", "upper", "lower:7", "u:5".
    // The member count defaults to 15 and is clamped to 1-15. Returns false for
    // an unrecognized side.
    bool parseZone(const String& token, Zone& out);

    // Appends a standard MPE Configuration Message (RPN 6 on the master channel)
    // announcing the zone's member-channel count, followed by the RPN null to
    // deselect the parameter.
    void appendConfigMessage(Array<MidiMessage>& out, const Zone& zone, double timestamp);

    // Builds a Pitch Bend Sensitivity RPN (RPN 0) declaring a whole-semitone
    // range on a channel, followed by the RPN null. Used when narrowing an MPE
    // zone so the device receiving the combined Manager+Member bend knows the
    // range the combined value is expressed in.
    void appendPitchBendSensitivity(Array<MidiMessage>& out, int channel, int semitones, double timestamp);

    // Declares the Pitch Bend Sensitivity (RPN 0) on a zone's member channels.
    // The spec (section 2.2.5) says a receiver applies the last sensitivity seen
    // on any Member Channel to all of them, but sending to every Member Channel
    // improves compatibility, so this declares it on each one.
    void appendMemberSensitivity(Array<MidiMessage>& out, const Zone& zone, int semitones, double timestamp);

    // Per-input state for declaring a member-channel Pitch Bend Sensitivity:
    // tracks whether it has been declared and watches for MPE Configuration
    // Messages, which reset the sensitivity and so call for a re-declaration.
    struct SensitivityDeclarer
    {
        SensitivityDeclarer() { reset(); }

        void reset();

        // returns true when the message completes any MPE Configuration Message
        bool isMcm(const MidiMessage& msg);

        bool declared;
        int rpnMsb[17], rpnLsb[17];
    };

    // Tracks the MPE Configuration Messages (RPN 6) of an incoming stream so a
    // reconfiguration of a zone (a changed member count, or a zone turned off)
    // can be detected. The MPE spec (section 2.2.3) requires stopping sounding
    // notes and resetting controls when a zone configuration changes.
    struct McmTracker
    {
        McmTracker() { reset(); }

        void reset();

        // feeds a message; returns true when it completes an MCM that changes a
        // previously announced zone (the first MCM for a zone is not a change)
        bool reconfigures(const MidiMessage& msg);

        int rpnMsb[17], rpnLsb[17];
        int lower, upper;   // last announced member count per zone, -1 = none yet
    };

    // Tracks the per-channel expression state of an incoming MPE stream so that
    // Manager (zone-wide) and Member (per-note) expression can be combined the
    // way an MPE receiver would when the channels are narrowed (see MPE spec
    // sections 2.2.6-2.2.8 and Appendices C and D): pitch bend is summed in
    // semitones (so the per-channel Pitch Bend Sensitivity matters), while
    // Channel Pressure and CC 74 are combined with the maximum of the two.
    struct ExpressionState
    {
        enum Change { None = 0, Bend = 1, Pressure = 2, CC74 = 4 };

        // sets the Pitch Bend Sensitivity defaults for the zone: 2 semitones on
        // the Manager Channel, 48 on every Member Channel (spec section 2.2.5).
        // These apply whenever no RPN 0 has been received for the channel.
        void resetSensitivity(const Zone& zone);

        // applies the MPE spec defaults for the given zone; called once before
        // the first message is tracked
        void resetForZone(const Zone& zone);

        // caches the value carried by a channel-voice message and reports which
        // expression dimension it changed; also follows RPN 0 to keep the
        // per-channel Pitch Bend Sensitivity current
        int update(const Zone& zone, const MidiMessage& msg);

        double bendSemitones(int ch) const { return bendSense[ch] * (bend[ch] - 8192) / 8191.0; }

        // the sensitivity (in whole semitones) needed to express the combined
        // Manager+Member bend without clipping
        int combinedSensitivity(int managerCh, int memberCh) const
        {
            return jlimit(1, 96, roundToInt(bendSense[managerCh] + bendSense[memberCh]));
        }

        // the 14-bit pitch bend value of the summed Manager+Member bend, expressed
        // at combinedSensitivity()
        int combinedBendValue(int managerCh, int memberCh) const;

        int combinedPressure(int managerCh, int memberCh) const { return jmax(pressure[managerCh], pressure[memberCh]); }
        int combinedCC74(int managerCh, int memberCh) const { return jmax(cc74[managerCh], cc74[memberCh]); }

        bool initialized { false };
        int bend[17], pressure[17], cc74[17];
        int rpnMsb[17], rpnLsb[17];
        double bendSense[17];
    };

    // Per-input voice-allocation state used to expand a single channel into an
    // MPE zone: it allocates a member channel for each note with the LinnStrument
    // ChannelBucket (reuse channels as late as possible, share on overflow, as
    // recommended by the MPE spec Appendix A.3) and tracks which channel each
    // held note was assigned to so note-offs and per-note expression follow it.
    struct Allocator
    {
        Allocator() { reset(); }

        void reset();

        bool configSent { false };
        bool bucketFilled { false };  // whether the zone's member channels were added yet
        ChannelBucket bucket;         // member-channel allocator
        int noteChannel[128];         // member channel (1-16) holding a note number, -1 = not held
    };

    // Per-input state used to collapse an MPE zone onto a single channel: it
    // remembers which note each member channel holds (so per-note channel
    // pressure can be turned into polyphonic aftertouch) and the trigger order
    // of those notes (so the most recently played note wins for the channel-wide
    // expression dimensions of pitch bend and timbre).
    struct Collapser
    {
        Collapser() { reset(); }

        void reset();

        // the member channel of the most recently triggered note that is still
        // held, or 0 when no note is held
        int activeChannel() const;

        int order { 0 };            // monotonically increasing note-trigger counter
        int channelNote[17];        // note held on each member channel (index 1-16), -1 = none
        int noteOrder[17];          // trigger order per member channel (higher = more recent)
        int outSense { 0 };         // Pitch Bend Sensitivity last declared on the target, 0 = none
        int lastBend { 8192 };      // last pitch bend value sent to the target (avoids redundant sends)
        int lastCC74 { 0 };         // last CC 74 value sent to the target
        int channelCC[17][128];     // last value of each CC held on each member channel, -1 = none
        int targetCC[128];          // last value of each CC sent to the target, -1 = none
        ExpressionState expr;       // Manager/Member expression cache for combining
    };

    // Per-input state used when relocating an MPE zone onto a smaller one, where
    // several source member channels fold onto one destination channel. It tracks
    // which source member channels currently hold a note (and their trigger order)
    // so the channel-wide expression of an oversubscribed destination channel can
    // follow the most recently triggered note.
    struct Relocator
    {
        Relocator() { reset(); }

        void reset();

        int counter { 0 };   // monotonically increasing note-trigger counter
        bool active[17];     // whether a note is held on each source member channel
        int order[17];       // trigger order per source channel (higher = more recent)
        int masterRpnMsb { -1 };  // RPN selection on the master channel, to spot the MCM (RPN 6)
        int masterRpnLsb { -1 };
        int srcBend[17];         // last pitch bend held on each source member channel, -1 = none
        int srcPressure[17];     // last channel pressure held on each source channel, -1 = none
        int channelCC[17][128];  // last value of each CC held on each source channel, -1 = none
        int destBend[17];        // last pitch bend sent to each destination channel
        int destPressure[17];    // last channel pressure sent to each destination channel, -1 = none
        int destCC[17][128];     // last value of each CC sent to each destination channel, -1 = none
    };

    // Per-route state used to fan an MPE zone out across a route's output ports,
    // treating each output as one monophonic voice. It maps each active member
    // channel to an output port so a note (and its per-note expression) reaches
    // the same port until it is released or its slot is stolen.
    struct Splitter
    {
        // (re)initialize the per-port tables when the number of output ports of
        // the route becomes known
        void ensureSize(int ports);

        // the first free port at or after the round-robin cursor, or -1 if all
        // ports are currently in use
        int freePort(int ports);

        // the oldest-allocated port, stolen when every port is busy
        int oldestPort(int ports);

        int order { 0 };               // monotonically increasing allocation counter
        int roundRobin { 0 };          // next port index to try when allocating
        int channelPort[17];           // output port assigned to each member channel, -1 = none
        std::vector<int> portChannel;  // member channel occupying each port, -1 = free
        std::vector<int> portNote;     // note number sounding on each port, -1 = none
        std::vector<int> portOrder;    // allocation order per port (for stealing)
        std::vector<int> portOutSense; // Pitch Bend Sensitivity last declared on each port, 0 = none
        std::vector<int> portLastBend; // last pitch bend value sent to each port
        std::vector<int> portLastCC74; // last CC 74 value sent to each port

        // tracks the RPN selected on each channel so the MPE Configuration
        // Message (RPN 6) can be suppressed without disturbing other RPNs
        int rpnMsb[17];                // last RPN MSB (CC 101) per channel, -1 = none
        int rpnLsb[17];                // last RPN LSB (CC 100) per channel, -1 = none
        bool rpnSelectionSent[17];     // whether the current selection has been forwarded

        ExpressionState expr;          // Manager/Member expression cache for combining
    };
}
