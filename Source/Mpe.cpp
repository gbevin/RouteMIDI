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

#include "Mpe.h"

namespace mpe
{

int Zone::memberChannel(int index) const
{
    if (index < 0 || index >= members)
    {
        return 0;
    }
    return lower ? (2 + index) : (15 - index);
}

int Zone::memberIndexOf(int channel) const
{
    if (lower)
    {
        return (channel >= 2 && channel <= 1 + members) ? channel - 2 : -1;
    }
    return (channel <= 15 && channel >= 16 - members) ? 15 - channel : -1;
}

bool parseZone(const String& token, Zone& out)
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

void appendConfigMessage(Array<MidiMessage>& out, const Zone& zone, double timestamp)
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

void appendPitchBendSensitivity(Array<MidiMessage>& out, int channel, int semitones, double timestamp)
{
    const MidiMessage msgs[] = {
        MidiMessage::controllerEvent(channel, 101, 0),                       // RPN MSB
        MidiMessage::controllerEvent(channel, 100, 0),                       // RPN LSB = Pitch Bend Sensitivity
        MidiMessage::controllerEvent(channel, 6, jlimit(0, 96, semitones)),  // Data Entry MSB = semitones
        MidiMessage::controllerEvent(channel, 101, 127),                     // RPN null
        MidiMessage::controllerEvent(channel, 100, 127)
    };
    for (auto m : msgs)
    {
        m.setTimeStamp(timestamp);
        out.add(m);
    }
}

void appendMemberSensitivity(Array<MidiMessage>& out, const Zone& zone, int semitones, double timestamp)
{
    const int s = jlimit(0, 96, semitones);
    for (int i = 0; i < zone.members; ++i)
    {
        const int ch = zone.memberChannel(i);
        const int ccs[3]  = { 101, 100, 6 };
        const int vals[3] = { 0,   0,   s };
        for (int k = 0; k < 3; ++k)
        {
            MidiMessage m = MidiMessage::controllerEvent(ch, ccs[k], vals[k]);
            m.setTimeStamp(timestamp);
            out.add(m);
        }
    }
}

void SensitivityDeclarer::reset()
{
    declared = false;
    for (int i = 0; i < 17; ++i) { rpnMsb[i] = -1; rpnLsb[i] = -1; }
}

bool SensitivityDeclarer::isMcm(const MidiMessage& msg)
{
    if (!msg.isController()) return false;
    const int ch = msg.getChannel();
    if (ch < 1 || ch > 16) return false;
    const int cc = msg.getControllerNumber();
    const int v = msg.getControllerValue();
    if (cc == 101) { rpnMsb[ch] = v; return false; }
    if (cc == 100) { rpnLsb[ch] = v; return false; }
    return cc == 6 && rpnMsb[ch] == 0 && rpnLsb[ch] == 6;
}

void McmTracker::reset()
{
    lower = -1;
    upper = -1;
    for (int i = 0; i < 17; ++i) { rpnMsb[i] = -1; rpnLsb[i] = -1; }
}

bool McmTracker::reconfigures(const MidiMessage& msg)
{
    if (!msg.isController()) return false;
    const int ch = msg.getChannel();
    if (ch < 1 || ch > 16) return false;

    const int cc = msg.getControllerNumber();
    const int v = msg.getControllerValue();
    if (cc == 101) { rpnMsb[ch] = v; return false; }
    if (cc == 100) { rpnLsb[ch] = v; return false; }
    if (cc == 6 && rpnMsb[ch] == 0 && rpnLsb[ch] == 6 && (ch == 1 || ch == 16))
    {
        int& last = (ch == 1) ? lower : upper;
        const bool changed = (last >= 0 && v != last);
        last = v;
        return changed;
    }
    return false;
}

void ExpressionState::resetSensitivity(const Zone& zone)
{
    for (int i = 0; i < 17; ++i) bendSense[i] = 48.0;
    bendSense[zone.masterChannel()] = 2.0;
}

void ExpressionState::resetForZone(const Zone& zone)
{
    for (int i = 0; i < 17; ++i)
    {
        bend[i] = 8192;
        pressure[i] = 0;
        cc74[i] = 0;
        rpnMsb[i] = -1;
        rpnLsb[i] = -1;
    }
    resetSensitivity(zone);
    initialized = true;
}

int ExpressionState::update(const Zone& zone, const MidiMessage& msg)
{
    const int ch = msg.getChannel();
    if (ch < 1 || ch > 16)
    {
        return None;
    }

    if (msg.isPitchWheel())      { bend[ch] = msg.getPitchWheelValue();        return Bend; }
    if (msg.isChannelPressure()) { pressure[ch] = msg.getChannelPressureValue(); return Pressure; }
    if (msg.isController())
    {
        const int cc = msg.getControllerNumber();
        const int v = msg.getControllerValue();
        if (cc == 74)  { cc74[ch] = v; return CC74; }
        if (cc == 101) { rpnMsb[ch] = v; return None; }
        if (cc == 100) { rpnLsb[ch] = v; return None; }
        if (cc == 6 && rpnMsb[ch] == 0 && rpnLsb[ch] == 6)
        {
            // an MPE Configuration Message restores the default Pitch Bend
            // Sensitivities (spec section 2.2.5), so an RPN 0 must follow
            // to change them again
            resetSensitivity(zone);
            return None;
        }
        if (cc == 6 && rpnMsb[ch] == 0 && rpnLsb[ch] == 0)
        {
            // Pitch Bend Sensitivity; on a Member Channel it applies to
            // every Member Channel of the zone (spec section 2.2.5)
            if (zone.isMaster(ch))
            {
                bendSense[ch] = (double) v;
            }
            else
            {
                for (int i = 0; i < zone.members; ++i) bendSense[zone.memberChannel(i)] = (double) v;
            }
            return None;
        }
    }
    return None;
}

int ExpressionState::combinedBendValue(int managerCh, int memberCh) const
{
    const double total = bendSemitones(managerCh) + bendSemitones(memberCh);
    const double sense = (double) combinedSensitivity(managerCh, memberCh);
    return jlimit(0, 16383, roundToInt(8192.0 + total / sense * 8191.0));
}

void Allocator::reset()
{
    configSent = false;
    bucketFilled = false;
    bucket.clear();
    for (int i = 0; i < 128; ++i) noteChannel[i] = -1;
}

void Collapser::reset()
{
    order = 0;
    outSense = 0;
    lastBend = 8192;
    lastCC74 = 0;
    for (int i = 0; i < 17; ++i)
    {
        channelNote[i] = -1;
        noteOrder[i] = 0;
        for (int c = 0; c < 128; ++c) channelCC[i][c] = -1;
    }
    for (int c = 0; c < 128; ++c) targetCC[c] = -1;
}

int Collapser::activeChannel() const
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

void Relocator::reset()
{
    counter = 0;
    masterRpnMsb = -1;
    masterRpnLsb = -1;
    for (int i = 0; i < 17; ++i)
    {
        active[i] = false;
        order[i] = 0;
        srcBend[i] = -1;
        srcPressure[i] = -1;
        destBend[i] = 8192;
        destPressure[i] = -1;
        for (int c = 0; c < 128; ++c) { channelCC[i][c] = -1; destCC[i][c] = -1; }
    }
}

void Splitter::ensureSize(int ports)
{
    if ((int) portChannel.size() == ports)
    {
        return;
    }
    portChannel.assign((size_t) ports, -1);
    portNote.assign((size_t) ports, -1);
    portOrder.assign((size_t) ports, 0);
    portOutSense.assign((size_t) ports, 0);
    portLastBend.assign((size_t) ports, 8192);
    portLastCC74.assign((size_t) ports, 0);
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

int Splitter::freePort(int ports)
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

int Splitter::oldestPort(int ports)
{
    int best = 0;
    for (int p = 1; p < ports; ++p)
        if (portOrder[(size_t) p] < portOrder[(size_t) best]) best = p;
    return best;
}

} // namespace mpe
