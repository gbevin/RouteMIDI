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
    const int ch = zone.managerChannel();
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
    bendSense[zone.managerChannel()] = 2.0;
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
            if (zone.isManager(ch))
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
    managerRpnMsb = -1;
    managerRpnLsb = -1;
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
    {
        if (portOrder[(size_t) p] < portOrder[(size_t) best]) best = p;
    }
    return best;
}

//==============================================================================

void relocate(Relocator& rel, const Zone& src, const Zone& dst,
              const MidiMessage& msg, Array<MidiMessage>& output)
{
    const double ts = msg.getTimeStamp();
    const int ch = msg.getChannel();

    // the manager channel is zone-wide; just move it across, but keep the
    // MPE Configuration Message's announced member count accurate for the
    // destination zone (rewrite the RPN 6 data entry)
    if (ch == src.managerChannel())
    {
        MidiMessage out = msg;
        out.setChannel(dst.managerChannel());
        if (msg.isController())
        {
            const int cc = msg.getControllerNumber();
            const int v = msg.getControllerValue();
            if (cc == 101) rel.managerRpnMsb = v;
            else if (cc == 100) rel.managerRpnLsb = v;
            else if (cc == 6 && rel.managerRpnMsb == 0 && rel.managerRpnLsb == 6)
            {
                out = MidiMessage::controllerEvent(dst.managerChannel(), 6, dst.members);
            }
        }
        out.setTimeStamp(ts);
        output.add(out);
        return;
    }

    const int idx = src.memberIndexOf(ch);
    if (idx < 0)
    {
        // not part of the source zone; pass through untouched
        output.add(msg);
        return;
    }

    // when the destination has fewer members, several source channels
    // fold onto one destination channel by wrapping the member index
    const int bucket = idx % dst.members;
    const int dstChannel = dst.memberChannel(bucket);

    // the most recently triggered held source note among the channels that
    // fold onto the same destination channel, or 0 if none is held
    auto activeForBucket = [&](int b) -> int
    {
        int activeChannel = 0, bestOrder = 0;
        for (int i = 0; i < src.members; ++i)
        {
            if ((i % dst.members) != b) continue;
            const int sourceChannel = src.memberChannel(i);
            if (rel.active[sourceChannel] && rel.order[sourceChannel] > bestOrder)
            {
                bestOrder = rel.order[sourceChannel];
                activeChannel = sourceChannel;
            }
        }
        return activeChannel;
    };

    // re-send a source note's held per-note expression onto its destination
    // channel, but only the values that differ from what the destination last
    // received (ascending CC order keeps a 14-bit MSB ahead of its LSB)
    auto reassert = [&](int sc, int dc)
    {
        if (sc < 1) return;
        if (rel.srcBend[sc] >= 0 && rel.srcBend[sc] != rel.destBend[dc])
        {
            MidiMessage m = MidiMessage::pitchWheel(dc, rel.srcBend[sc]);
            m.setTimeStamp(ts); output.add(m);
            rel.destBend[dc] = rel.srcBend[sc];
        }
        if (rel.srcPressure[sc] >= 0 && rel.srcPressure[sc] != rel.destPressure[dc])
        {
            MidiMessage m = MidiMessage::channelPressureChange(dc, rel.srcPressure[sc]);
            m.setTimeStamp(ts); output.add(m);
            rel.destPressure[dc] = rel.srcPressure[sc];
        }
        for (int cc = 0; cc < 128; ++cc)
        {
            const int v = rel.channelCC[sc][cc];
            if (v >= 0 && v != rel.destCC[dc][cc])
            {
                MidiMessage m = MidiMessage::controllerEvent(dc, cc, v);
                m.setTimeStamp(ts); output.add(m);
                rel.destCC[dc][cc] = v;
            }
        }
    };

    if (msg.isNoteOn() && msg.getVelocity() > 0)
    {
        rel.active[ch] = true;
        rel.order[ch] = ++rel.counter;
        // a fresh note starts with no known per-note expression, so stale
        // values from a previous note on this channel are forgotten
        rel.srcBend[ch] = -1;
        rel.srcPressure[ch] = -1;
        for (int cc = 0; cc < 128; ++cc) rel.channelCC[ch][cc] = -1;
        MidiMessage out = msg;
        out.setChannel(dstChannel);
        output.add(out);
        return;
    }
    if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
    {
        MidiMessage out = msg;
        out.setChannel(dstChannel);
        output.add(out);
        rel.active[ch] = false;
        // hand the destination channel to whichever folded note now owns it,
        // re-asserting its held expression so it stops tracking the released
        // note instead of waiting for the next update to arrive
        reassert(activeForBucket(bucket), dstChannel);
        return;
    }

    // pitch bend, channel pressure and every controller are channel-wide, so
    // they collide when several notes share a destination channel; remember
    // each note's latest value so a note that later regains the channel can
    // re-assert it, and let only the owning note reach the destination
    if (msg.isPitchWheel())           rel.srcBend[ch] = msg.getPitchWheelValue();
    else if (msg.isChannelPressure()) rel.srcPressure[ch] = msg.getChannelPressureValue();
    else if (msg.isController())      rel.channelCC[ch][msg.getControllerNumber()] = msg.getControllerValue();

    const bool isPerNote = msg.isPitchWheel() || msg.isChannelPressure() || msg.isController();
    if (isPerNote)
    {
        // with at most one note on the destination channel the expression is
        // genuinely per-note and passes through; once notes collide, only the
        // most recently triggered note's expression is kept
        const int activeChannel = activeForBucket(bucket);
        if (activeChannel != 0 && ch != activeChannel)
        {
            return;  // suppressed: another note owns this destination channel
        }
    }

    MidiMessage out = msg;
    out.setChannel(dstChannel);
    output.add(out);
    // remember what the destination channel now holds so a later re-assertion
    // only re-sends genuine differences
    if (msg.isPitchWheel())           rel.destBend[dstChannel] = msg.getPitchWheelValue();
    else if (msg.isChannelPressure()) rel.destPressure[dstChannel] = msg.getChannelPressureValue();
    else if (msg.isController())      rel.destCC[dstChannel][msg.getControllerNumber()] = msg.getControllerValue();
}

void collapse(Collapser& col, const Zone& src, int target,
              const MidiMessage& msg, Array<MidiMessage>& output)
{
    const double ts = msg.getTimeStamp();
    const int ch = msg.getChannel();
    const int managerCh = src.managerChannel();

    // messages outside the zone pass straight through
    if (!src.contains(ch))
    {
        output.add(msg);
        return;
    }

    if (!col.expr.initialized)
    {
        col.expr.resetForZone(src);
    }
    const int change = col.expr.update(src, msg);

    // emits the combined pitch bend for a member note, declaring the
    // combined Pitch Bend Sensitivity on the target first if it changed
    auto emitBend = [&](int memberCh)
    {
        if (memberCh < 1) return;
        const int value = col.expr.combinedBendValue(managerCh, memberCh);
        if (value == col.lastBend) return;   // nothing changed on the target
        const int sense = col.expr.combinedSensitivity(managerCh, memberCh);
        if (sense != col.outSense)
        {
            appendPitchBendSensitivity(output, target, sense, ts);
            col.outSense = sense;
        }
        MidiMessage pb = MidiMessage::pitchWheel(target, value);
        pb.setTimeStamp(ts);
        output.add(pb);
        col.lastBend = value;
    };
    auto emitCC74 = [&](int memberCh)
    {
        if (memberCh < 1) return;
        const int value = col.expr.combinedCC74(managerCh, memberCh);
        if (value == col.lastCC74) return;
        MidiMessage cc = MidiMessage::controllerEvent(target, 74, value);
        cc.setTimeStamp(ts);
        output.add(cc);
        col.lastCC74 = value;
    };
    // when a note takes over the channel, re-send every per-note controller
    // it still holds whose value differs from what the target last received,
    // so a mono synth adopts the new note's expression instead of keeping the
    // previous note's (ascending order keeps a 14-bit MSB before its LSB)
    auto reassertCCs = [&](int memberCh)
    {
        if (memberCh < 1) return;
        for (int cc = 0; cc < 128; ++cc)
        {
            const int v = col.channelCC[memberCh][cc];
            if (v >= 0 && v != col.targetCC[cc])
            {
                MidiMessage m = MidiMessage::controllerEvent(target, cc, v);
                m.setTimeStamp(ts);
                output.add(m);
                col.targetCC[cc] = v;
            }
        }
    };
    // a member note's pressure rides on polyphonic aftertouch (so each
    // note keeps its own), combined with the Manager pressure via Max
    auto emitPressure = [&](int memberCh)
    {
        const int note = col.channelNote[memberCh];
        if (note < 0) return;
        MidiMessage at = MidiMessage::aftertouchChange(target, note, col.expr.combinedPressure(managerCh, memberCh));
        at.setTimeStamp(ts);
        output.add(at);
    };

    // a member RPN 0 data entry sets the source sensitivity (captured
    // above); it must not reach the target, which uses our combined RPN 0
    auto isSensitivityData = [&]()
    {
        return msg.isController() &&
            (msg.getControllerNumber() == 6 || msg.getControllerNumber() == 38) &&
            col.expr.rpnMsb[ch] == 0 && col.expr.rpnLsb[ch] == 0;
    };

    // --- Manager (zone-wide) messages affect the note that owns the channel ---
    if (src.isManager(ch))
    {
        if (change == ExpressionState::Bend)     { emitBend(col.activeChannel()); return; }
        if (change == ExpressionState::CC74)     { emitCC74(col.activeChannel()); return; }
        if (change == ExpressionState::Pressure)
        {
            for (int c = 1; c < 17; ++c) if (col.channelNote[c] >= 0) emitPressure(c);
            return;
        }
        if (isSensitivityData()) return;

        MidiMessage out = msg;
        out.setChannel(target);
        output.add(out);
        return;
    }

    // --- Member channel ---
    if (msg.isNoteOn() && msg.getVelocity() > 0)
    {
        col.channelNote[ch] = msg.getNoteNumber();
        col.noteOrder[ch] = ++col.order;
        // a fresh note starts with no known per-note controller state, so
        // stale values from a previous note on this channel are forgotten
        for (int cc = 0; cc < 128; ++cc) col.channelCC[ch][cc] = -1;
        // the new note owns the channel-wide dimensions; send its initial
        // combined expression before the Note On (spec section 2.4)
        emitBend(ch);
        emitCC74(ch);
        if (col.expr.combinedPressure(managerCh, ch) > 0) emitPressure(ch);
        MidiMessage out = msg;
        out.setChannel(target);
        output.add(out);
        return;
    }
    if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
    {
        col.channelNote[ch] = -1;
        col.noteOrder[ch] = 0;
        MidiMessage out = msg;
        out.setChannel(target);
        output.add(out);
        // hand the channel-wide dimensions to whichever note now owns them
        const int a = col.activeChannel();
        if (a >= 1) { emitBend(a); emitCC74(a); reassertCCs(a); }
        return;
    }
    if (change == ExpressionState::Bend)     { if (ch == col.activeChannel()) emitBend(ch); return; }
    if (change == ExpressionState::CC74)     { if (ch == col.activeChannel()) emitCC74(ch); return; }
    if (change == ExpressionState::Pressure) { emitPressure(ch); return; }
    if (isSensitivityData()) return;

    // any other member-channel controller (e.g. a high-resolution 14-bit
    // CC like the LinnStrument's CC 6/38) is a per-note expression too:
    // remember it for every held note, but only let the note that currently
    // owns the channel reach the target so simultaneous touches never fight,
    // and a note that later regains the channel can re-assert its own value
    if (msg.isController())
    {
        const int cc = msg.getControllerNumber();
        const int v  = msg.getControllerValue();
        col.channelCC[ch][cc] = v;
        if (ch == col.activeChannel() && col.targetCC[cc] != v)
        {
            MidiMessage m = MidiMessage::controllerEvent(target, cc, v);
            m.setTimeStamp(ts);
            output.add(m);
            col.targetCC[cc] = v;
        }
        return;
    }

    // any other member-channel message is forwarded rechanneled
    MidiMessage out = msg;
    out.setChannel(target);
    output.add(out);
}

void expand(Allocator& alloc, int srcChannel, const Zone& dst,
            const MidiMessage& msg, Array<MidiMessage>& output)
{
    const double ts = msg.getTimeStamp();
    const int ch = msg.getChannel();

    if (ch != srcChannel)
    {
        // not the channel we expand; leave it untouched
        output.add(msg);
        return;
    }

    // announce the zone once, just before the first note is forwarded
    auto ensureConfig = [&]()
    {
        if (!alloc.configSent)
        {
            appendConfigMessage(output, dst, ts);
            alloc.configSent = true;
        }
    };

    if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
    {
        const int note = msg.getNoteNumber();
        const int mch = (note >= 0 && note < 128) ? alloc.noteChannel[note] : -1;
        if (mch >= 1)
        {
            // set Channel Pressure to zero before the Note Off (Appendix A.4.2)
            MidiMessage zero = MidiMessage::channelPressureChange(mch, 0);
            zero.setTimeStamp(ts);
            output.add(zero);
            MidiMessage off = MidiMessage::noteOff(mch, note, msg.getVelocity());
            off.setTimeStamp(ts);
            output.add(off);
            alloc.bucket.release(mch);
            alloc.noteChannel[note] = -1;
        }
        return;
    }

    if (msg.isNoteOn())
    {
        ensureConfig();
        const int note = msg.getNoteNumber();

        // fill the channel bucket with the destination zone's member
        // channels the first time it is used
        if (!alloc.bucketFilled)
        {
            alloc.bucket.clear();
            for (int i = 0; i < dst.members; ++i) alloc.bucket.add(dst.memberChannel(i));
            alloc.bucketFilled = true;
        }

        // if this note is somehow still held, release its channel first
        if (alloc.noteChannel[note] >= 1)
        {
            const int prev = alloc.noteChannel[note];
            MidiMessage off = MidiMessage::noteOff(prev, note, (uint8)0);
            off.setTimeStamp(ts);
            output.add(off);
            alloc.bucket.release(prev);
            alloc.noteChannel[note] = -1;
        }

        // the bucket hands out a member channel, reusing one as late as
        // possible and sharing a channel when the polyphony exceeds the
        // number of member channels (MPE spec Appendix A.3)
        const int chosen = alloc.bucket.take();
        if (chosen >= 1)
        {
            // set Channel Pressure to zero before the Note On (Appendix A.4.2)
            MidiMessage zero = MidiMessage::channelPressureChange(chosen, 0);
            zero.setTimeStamp(ts);
            output.add(zero);
            MidiMessage on = MidiMessage::noteOn(chosen, note, msg.getVelocity());
            on.setTimeStamp(ts);
            output.add(on);
            alloc.noteChannel[note] = chosen;
        }
        return;
    }

    if (msg.isAftertouch())
    {
        // polyphonic aftertouch for a note becomes channel pressure on
        // that note's member channel, which is how MPE expresses per-note
        // pressure (the Z dimension)
        const int note = msg.getNoteNumber();
        const int mch = (note >= 0 && note < 128) ? alloc.noteChannel[note] : -1;
        MidiMessage out = MidiMessage::channelPressureChange(mch >= 1 ? mch : dst.managerChannel(),
                                                             msg.getAfterTouchValue());
        out.setTimeStamp(ts);
        output.add(out);
        return;
    }

    // remaining channel-voice messages (CC, pitch bend, channel
    // pressure, program change) are zone-wide: send them to the manager
    MidiMessage out = msg;
    out.setChannel(dst.managerChannel());
    output.add(out);
}

void rescaleBend(const Zone& zone, double from, double to,
                 const MidiMessage& msg, Array<MidiMessage>& output)
{
    if (!msg.isPitchWheel() || zone.memberIndexOf(msg.getChannel()) < 0 || to <= 0.0)
    {
        output.add(msg);   // only member-channel pitch bend is rescaled
        return;
    }

    const int value = jlimit(0, 16383, roundToInt(8192.0 + (msg.getPitchWheelValue() - 8192) * from / to));
    MidiMessage out = MidiMessage::pitchWheel(msg.getChannel(), value);
    out.setTimeStamp(msg.getTimeStamp());
    output.add(out);
}

void declareSensitivity(SensitivityDeclarer& sd, const Zone& zone, int semitones,
                        const MidiMessage& msg, Array<MidiMessage>& output)
{
    const double ts = msg.getTimeStamp();

    if (sd.isMcm(msg))
    {
        // forward the MCM (it resets member sensitivity to 48 on the
        // synth), then re-declare ours just after it
        output.add(msg);
        appendMemberSensitivity(output, zone, semitones, ts);
        sd.declared = true;
        return;
    }
    // otherwise declare it once, just before the first note is forwarded
    if (msg.isNoteOn() && msg.getVelocity() > 0 && !sd.declared)
    {
        appendMemberSensitivity(output, zone, semitones, ts);
        sd.declared = true;
    }
    output.add(msg);
}

void split(Splitter& sp, const Zone& zone, int targetCh, int ports,
           const MidiMessage& msg, Array<MidiMessage>& outMsgs, Array<int>& outPorts)
{
    const double ts = msg.getTimeStamp();
    const int ch = msg.getChannel();

    if (ports == 0)
    {
        return;
    }

    sp.ensureSize(ports);
    if (!sp.expr.initialized)
    {
        sp.expr.resetForZone(zone);
    }

    const int managerCh = zone.managerChannel();
    const int memberIndex = zone.memberIndexOf(ch);
    const bool inZone = (ch >= 1 && zone.contains(ch));

    // keep the expression cache current for every in-zone message
    const int change = inZone ? sp.expr.update(zone, msg) : ExpressionState::None;

    auto emit = [&](int port, const MidiMessage& m) { outMsgs.add(m); outPorts.add(port); };
    auto rechannel = [&](const MidiMessage& m)
    {
        MidiMessage x = m;
        if (x.getChannel() > 0) x.setChannel(targetCh);
        x.setTimeStamp(ts);
        return x;
    };

    // each port is one voice, so its expression is the combination of the Manager
    // and the port's own Member channel (MPE spec 2.2.6-2.2.8)
    auto emitPortBend = [&](int port)
    {
        const int mch = sp.portChannel[(size_t) port];
        if (mch < 1) return;
        const int value = sp.expr.combinedBendValue(managerCh, mch);
        if (value == sp.portLastBend[(size_t) port]) return;
        const int sense = sp.expr.combinedSensitivity(managerCh, mch);
        if (sense != sp.portOutSense[(size_t) port])
        {
            Array<MidiMessage> rpn;
            appendPitchBendSensitivity(rpn, targetCh, sense, ts);
            for (auto& r : rpn) emit(port, r);
            sp.portOutSense[(size_t) port] = sense;
        }
        MidiMessage pb = MidiMessage::pitchWheel(targetCh, value);
        pb.setTimeStamp(ts);
        emit(port, pb);
        sp.portLastBend[(size_t) port] = value;
    };
    auto emitPortPressure = [&](int port)
    {
        const int mch = sp.portChannel[(size_t) port];
        if (mch < 1) return;
        MidiMessage cp = MidiMessage::channelPressureChange(targetCh, sp.expr.combinedPressure(managerCh, mch));
        cp.setTimeStamp(ts);
        emit(port, cp);
    };
    auto emitPortCC74 = [&](int port)
    {
        const int mch = sp.portChannel[(size_t) port];
        if (mch < 1) return;
        const int value = sp.expr.combinedCC74(managerCh, mch);
        if (value == sp.portLastCC74[(size_t) port]) return;
        MidiMessage cc = MidiMessage::controllerEvent(targetCh, 74, value);
        cc.setTimeStamp(ts);
        emit(port, cc);
        sp.portLastCC74[(size_t) port] = value;
    };
    auto isSensitivityData = [&](int channel)
    {
        return msg.isController() &&
            (msg.getControllerNumber() == 6 || msg.getControllerNumber() == 38) &&
            sp.expr.rpnMsb[channel] == 0 && sp.expr.rpnLsb[channel] == 0;
    };

    // --- Manager (zone-wide), non-zone, and channelless messages ---
    if (ch == 0 || memberIndex < 0)
    {
        if (zone.isManager(ch))
        {
            // zone-wide expression is folded into every occupied port's combined value
            if (change == ExpressionState::Bend)     { for (int p = 0; p < ports; ++p) emitPortBend(p);     return; }
            if (change == ExpressionState::Pressure) { for (int p = 0; p < ports; ++p) emitPortPressure(p); return; }
            if (change == ExpressionState::CC74)     { for (int p = 0; p < ports; ++p) emitPortCC74(p);     return; }
            if (isSensitivityData(ch)) return;   // Manager RPN 0 data: consumed (captured above)

            // suppress the MPE Configuration Message (RPN 6) but let other RPNs
            // pass, replaying the selection that was held back
            if (msg.isController())
            {
                const int cc = msg.getControllerNumber();
                const int v = msg.getControllerValue();
                if (cc == 101) { sp.rpnMsb[ch] = v; sp.rpnSelectionSent[ch] = false; return; }
                if (cc == 100) { sp.rpnLsb[ch] = v; sp.rpnSelectionSent[ch] = false; return; }
                if (cc == 6 || cc == 38)
                {
                    if (sp.rpnMsb[ch] == 0 && sp.rpnLsb[ch] == 6) return;  // RPN 6 data: drop
                    if (!sp.rpnSelectionSent[ch])
                    {
                        if (sp.rpnMsb[ch] >= 0) emit(-1, rechannel(MidiMessage::controllerEvent(ch, 101, sp.rpnMsb[ch])));
                        if (sp.rpnLsb[ch] >= 0) emit(-1, rechannel(MidiMessage::controllerEvent(ch, 100, sp.rpnLsb[ch])));
                        sp.rpnSelectionSent[ch] = true;
                    }
                }
            }
        }
        emit(-1, rechannel(msg));
        return;
    }

    // --- Member channel ---
    // a note-on claims a port for this member channel, allocating a free one or
    // stealing the oldest, and is rechanneled onto the target channel
    if (msg.isNoteOn() && msg.getVelocity() > 0)
    {
        int port = sp.channelPort[ch];
        if (port < 0)
        {
            port = sp.freePort(ports);
            if (port < 0)
            {
                port = sp.oldestPort(ports);
                if (sp.portNote[(size_t) port] >= 0)
                {
                    MidiMessage off = MidiMessage::noteOff(targetCh, sp.portNote[(size_t) port], (uint8) 0);
                    off.setTimeStamp(ts);
                    emit(port, off);
                }
                const int oldCh = sp.portChannel[(size_t) port];
                if (oldCh >= 1) sp.channelPort[oldCh] = -1;
            }
            sp.channelPort[ch] = port;
            sp.portChannel[(size_t) port] = ch;
        }
        sp.portNote[(size_t) port] = msg.getNoteNumber();
        sp.portOrder[(size_t) port] = ++sp.order;

        // send the freshly assigned port's initial combined expression before
        // the Note On (spec section 2.4)
        emitPortBend(port);
        emitPortCC74(port);
        if (sp.expr.combinedPressure(managerCh, ch) > 0) emitPortPressure(port);

        MidiMessage on = MidiMessage::noteOn(targetCh, msg.getNoteNumber(), (uint8) msg.getVelocity());
        on.setTimeStamp(ts);
        emit(port, on);
        return;
    }

    // a note-off releases this member channel's port
    if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
    {
        const int port = sp.channelPort[ch];
        if (port >= 0)
        {
            MidiMessage off = MidiMessage::noteOff(targetCh, msg.getNoteNumber(), (uint8) msg.getVelocity());
            off.setTimeStamp(ts);
            emit(port, off);
            sp.portNote[(size_t) port] = -1;
            sp.portChannel[(size_t) port] = -1;
            sp.channelPort[ch] = -1;
        }
        return;
    }

    // per-note expression follows its note to the same port, combined with the
    // Manager; expression with no active note on its channel is dropped
    const int port = sp.channelPort[ch];
    if (port < 0) return;
    if (change == ExpressionState::Bend)     { emitPortBend(port);     return; }
    if (change == ExpressionState::Pressure) { emitPortPressure(port); return; }
    if (change == ExpressionState::CC74)     { emitPortCC74(port);     return; }
    if (isSensitivityData(ch)) return;   // Member RPN 0 data: consumed

    // any other member message is forwarded rechanneled to the port
    emit(port, rechannel(msg));
}

} // namespace mpe
