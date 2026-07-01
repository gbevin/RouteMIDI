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

#include "JuceHeader.h"

#include "TestHelpers.h"
#include "../Source/ApplicationState.h"
#include "../Source/Mpe.h"

namespace
{
    // runs a single MPE operation over a sequence of input messages, preserving
    // the per-input allocator state across them (as real routing would)
    Array<MidiMessage> runMpe(ApplicationState& state, CommandIndex op, const StringArray& opts,
                              const Array<MidiMessage>& in)
    {
        Route route;
        route.mpeOps.add(makeCommand(op, opts));
        route.inputs.add(new RouteInput());
        auto& input = *route.inputs[0];

        Array<MidiMessage> out;
        for (const auto& m : in)
            state.processMpe(route, input, m, out);
        return out;
    }

    struct SplitResult { Array<MidiMessage> msgs; Array<int> ports; };

    // runs an mpesplit over a sequence of messages with the given number of
    // output ports, keeping the per-route allocation state across them
    SplitResult runSplit(ApplicationState& state, const StringArray& opts, int numPorts,
                         const Array<MidiMessage>& in)
    {
        Route route;
        route.outputSplit.add(makeCommand(MPE_SPLIT, opts));
        for (int i = 0; i < numPorts; ++i)
            route.outputs.add(new OutputDest());

        SplitResult r;
        for (const auto& m : in)
            state.processSplit(route, m, r.msgs, r.ports);
        return r;
    }
}

class MpeTests : public UnitTest
{
public:
    MpeTests() : UnitTest("MPE", "MPE") {}

    void runTest() override
    {
        beginTest("Zone channel math for the Lower and Upper zones");
        {
            mpe::Zone lower; lower.lower = true;  lower.members = 15;
            expectEquals(lower.masterChannel(), 1);
            expectEquals(lower.memberChannel(0), 2);
            expectEquals(lower.memberChannel(14), 16);
            expectEquals(lower.memberIndexOf(2), 0);
            expectEquals(lower.memberIndexOf(16), 14);
            expectEquals(lower.memberIndexOf(1), -1);   // master is not a member
            expect(lower.contains(1) && lower.contains(16) && !lower.contains(0));

            mpe::Zone upper; upper.lower = false; upper.members = 7;
            expectEquals(upper.masterChannel(), 16);
            expectEquals(upper.memberChannel(0), 15);
            expectEquals(upper.memberChannel(6), 9);
            expectEquals(upper.memberIndexOf(15), 0);
            expectEquals(upper.memberIndexOf(9), 6);
            expectEquals(upper.memberIndexOf(8), -1);   // outside the 7 members
            expectEquals(upper.memberIndexOf(16), -1);  // master is not a member
        }

        beginTest("ExpressionState combines Manager and Member bend per the MPE spec");
        {
            mpe::Zone zone; zone.lower = true; zone.members = 15;
            mpe::ExpressionState es;
            es.resetForZone(zone);

            // spec defaults: Manager 2 semitones, Members 48 (section 2.2.5),
            // used whenever no Pitch Bend Sensitivity RPN has been provided
            expectEquals((int) es.bendSense[1], 2);
            expectEquals((int) es.bendSense[2], 48);
            expectEquals(es.combinedSensitivity(1, 2), 50);   // 2 + 48 by default

            // Appendix C receiver example: Member +7 (value 9387), Manager +2 (value 16383)
            es.update(zone, MidiMessage::pitchWheel(1, 16383));   // Manager
            es.update(zone, MidiMessage::pitchWheel(2, 9387));    // Member channel 2
            expect(std::abs(es.bendSemitones(2) - 7.0) < 0.01);
            expect(std::abs(es.bendSemitones(1) - 2.0) < 0.01);

            // combined ~= 9 semitones, expressed at 2 + 48 = 50 semitone range
            // (9387 is round(7*8192/48)+8192, i.e. marginally over 7 semitones)
            expectEquals(es.combinedSensitivity(1, 2), 50);
            expectEquals(es.combinedBendValue(1, 2), 9667);
        }

        beginTest("ExpressionState tracks Pitch Bend Sensitivity from RPN 0 across all members");
        {
            mpe::Zone zone; zone.lower = true; zone.members = 15;
            mpe::ExpressionState es;
            es.resetForZone(zone);

            // RPN 0 on a single Member Channel applies to all Member Channels
            es.update(zone, MidiMessage::controllerEvent(5, 101, 0));
            es.update(zone, MidiMessage::controllerEvent(5, 100, 0));
            es.update(zone, MidiMessage::controllerEvent(5, 6, 24));   // 24 semitones
            expectEquals((int) es.bendSense[2], 24);
            expectEquals((int) es.bendSense[16], 24);
            expectEquals((int) es.bendSense[1], 2);   // Manager unaffected

            // RPN 0 on the Manager Channel only affects the Manager
            es.update(zone, MidiMessage::controllerEvent(1, 101, 0));
            es.update(zone, MidiMessage::controllerEvent(1, 100, 0));
            es.update(zone, MidiMessage::controllerEvent(1, 6, 12));
            expectEquals((int) es.bendSense[1], 12);
            expectEquals((int) es.bendSense[2], 24);

            // an MPE Configuration Message restores the defaults (section 2.2.5)
            es.update(zone, MidiMessage::controllerEvent(1, 101, 0));
            es.update(zone, MidiMessage::controllerEvent(1, 100, 6));
            es.update(zone, MidiMessage::controllerEvent(1, 6, 15));   // MCM: 15 members
            expectEquals((int) es.bendSense[1], 2);
            expectEquals((int) es.bendSense[2], 48);
        }

        beginTest("McmTracker detects a zone reconfiguration");
        {
            mpe::McmTracker t;
            auto cc = [](int ch, int n, int v) { return MidiMessage::controllerEvent(ch, n, v); };

            // first MCM for the Lower zone (15 members): configures, not a change
            expect(! t.reconfigures(cc(1, 101, 0)));
            expect(! t.reconfigures(cc(1, 100, 6)));
            expect(! t.reconfigures(cc(1, 6, 15)));

            // the same MCM again: no change
            t.reconfigures(cc(1, 101, 0)); t.reconfigures(cc(1, 100, 6));
            expect(! t.reconfigures(cc(1, 6, 15)));

            // a different member count: a reconfiguration
            t.reconfigures(cc(1, 101, 0)); t.reconfigures(cc(1, 100, 6));
            expect(t.reconfigures(cc(1, 6, 8)));

            // turning the zone off (0 members): a reconfiguration
            t.reconfigures(cc(1, 101, 0)); t.reconfigures(cc(1, 100, 6));
            expect(t.reconfigures(cc(1, 6, 0)));

            // the Upper zone is tracked independently (first MCM, not a change)
            t.reconfigures(cc(16, 101, 0)); t.reconfigures(cc(16, 100, 6));
            expect(! t.reconfigures(cc(16, 6, 7)));

            // an ordinary RPN 6-less controller is never a reconfiguration
            expect(! t.reconfigures(cc(1, 7, 100)));
        }

        beginTest("ExpressionState combines pressure and CC74 with the maximum");
        {
            mpe::Zone zone; zone.lower = true; zone.members = 15;
            mpe::ExpressionState es;
            es.resetForZone(zone);

            es.update(zone, MidiMessage::channelPressureChange(1, 40));   // Manager
            es.update(zone, MidiMessage::channelPressureChange(2, 90));   // Member
            expectEquals(es.combinedPressure(1, 2), 90);

            es.update(zone, MidiMessage::controllerEvent(1, 74, 100));    // Manager CC74
            es.update(zone, MidiMessage::controllerEvent(2, 74, 30));     // Member CC74
            expectEquals(es.combinedCC74(1, 2), 100);
        }

        beginTest("parseZone accepts sides, abbreviations and member counts");
        {
            mpe::Zone z;
            expect(mpe::parseZone("lower", z) && z.lower && z.members == 15);
            expect(mpe::parseZone("upper", z) && !z.lower && z.members == 15);
            expect(mpe::parseZone("l:7", z) && z.lower && z.members == 7);
            expect(mpe::parseZone("U:5", z) && !z.lower && z.members == 5);
            expect(mpe::parseZone("lower:99", z) && z.members == 15);  // clamped
            expect(! mpe::parseZone("x", z));
            expect(! mpe::parseZone("", z));
        }

        ApplicationState state;

        beginTest("mpemaster and mpemember filters select the right channels");
        {
            auto master = makeCommand(MPE_MASTER, {"lower"});
            auto member = makeCommand(MPE_MEMBER, {"lower:7"});

            // lower zone with 7 members: master 1, members 2-8
            expect(  master.matches(state, MidiMessage::controllerEvent(1, 7, 64), 0));
            expect(! master.matches(state, MidiMessage::controllerEvent(2, 7, 64), 0));

            expect(  member.matches(state, MidiMessage::noteOn(2, 60, (uint8)100), 0));
            expect(  member.matches(state, MidiMessage::noteOn(8, 60, (uint8)100), 0));
            expect(! member.matches(state, MidiMessage::noteOn(9, 60, (uint8)100), 0));  // beyond 7 members
            expect(! member.matches(state, MidiMessage::noteOn(1, 60, (uint8)100), 0));  // master, not member

            // mpezone passes the whole zone (master and members), to isolate one
            // zone of a two-zone controller
            auto zone = makeCommand(MPE_ZONE, {"lower:7"});
            expect(  zone.matches(state, MidiMessage::controllerEvent(1, 7, 64), 0));   // master
            expect(  zone.matches(state, MidiMessage::noteOn(2, 60, (uint8)100), 0));   // member
            expect(  zone.matches(state, MidiMessage::noteOn(8, 60, (uint8)100), 0));   // member
            expect(! zone.matches(state, MidiMessage::noteOn(9, 60, (uint8)100), 0));   // upper zone, dropped
            expect(! zone.matches(state, MidiMessage::controllerEvent(16, 7, 64), 0));  // upper master, dropped
        }

        beginTest("relocate remaps the Lower zone onto the Upper zone");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 74, 64));            // master message
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));            // first member
            in.add(MidiMessage::pitchWheel(3, 9000));                  // second member
            in.add(MidiMessage::noteOn(7, 64, (uint8)100));            // unrelated channel? no, ch7 is a member

            auto out = runMpe(state, MPE_RELOCATE, {"lower:15", "upper:15"}, in);
            expectEquals(out.size(), 4);
            // lower master 1 -> upper master 16
            expectEquals(out[0].getChannel(), 16);
            // lower member index 0 (ch2) -> upper member index 0 (ch15)
            expectEquals(out[1].getChannel(), 15);
            // lower member index 1 (ch3) -> upper member index 1 (ch14)
            expectEquals(out[2].getChannel(), 14);
            // lower member index 5 (ch7) -> upper member index 5 (ch10)
            expectEquals(out[3].getChannel(), 10);
        }

        beginTest("relocate leaves channels outside the source zone untouched");
        {
            // a Lower zone of 3 members covers channels 1-4; channel 10 is outside
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(10, 60, (uint8)100));
            auto out = runMpe(state, MPE_RELOCATE, {"lower:3", "upper:3"}, in);
            expectEquals(out.size(), 1);
            expectEquals(out[0].getChannel(), 10);
        }

        beginTest("relocate keeps per-note expression when destinations don't collide");
        {
            // lower:2 -> lower:2: members map one-to-one, so both notes keep their
            // own pitch bend
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::noteOn(3, 64, (uint8)100));
            in.add(MidiMessage::pitchWheel(2, 9000));
            in.add(MidiMessage::pitchWheel(3, 5000));
            auto out = runMpe(state, MPE_RELOCATE, {"lower:2", "lower:2"}, in);

            int bendCount = 0, bend2 = -1, bend3 = -1;
            for (const auto& m : out)
                if (m.isPitchWheel())
                {
                    ++bendCount;
                    if (m.getChannel() == 2) bend2 = m.getPitchWheelValue();
                    if (m.getChannel() == 3) bend3 = m.getPitchWheelValue();
                }
            expectEquals(bendCount, 2);     // both per-note bends pass through
            expectEquals(bend2, 9000);
            expectEquals(bend3, 5000);
        }

        beginTest("relocate folds colliding notes' expression to last-note-wins");
        {
            // lower:4 (members 2-5) -> lower:2 (members 2-3): source channels 2 and
            // 4 both fold onto destination channel 2
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));            // older note
            in.add(MidiMessage::noteOn(4, 64, (uint8)100));            // newest -> owns ch 2
            in.add(MidiMessage::pitchWheel(2, 9000));                  // older: suppressed
            in.add(MidiMessage::pitchWheel(4, 5000));                  // active: passes
            in.add(MidiMessage::channelPressureChange(2, 50));        // older: suppressed
            in.add(MidiMessage::channelPressureChange(4, 70));        // active: passes (as channel pressure)
            auto out = runMpe(state, MPE_RELOCATE, {"lower:4", "lower:2"}, in);

            int bend = -1, bendCount = 0, cp = -1, cpCount = 0;
            for (const auto& m : out)
            {
                if (m.isPitchWheel())       { bend = m.getPitchWheelValue(); ++bendCount; expectEquals(m.getChannel(), 2); }
                if (m.isChannelPressure())  { cp = m.getChannelPressureValue(); ++cpCount; expectEquals(m.getChannel(), 2); }
            }
            expectEquals(bendCount, 1);
            expectEquals(bend, 5000);       // the active note's bend
            expectEquals(cpCount, 1);
            expectEquals(cp, 70);           // pressure stays channel pressure, last note wins
        }

        beginTest("mpebend rescales member-channel pitch bend to a new range");
        {
            // a controller using 48 semitones feeding a synth fixed at 12: a +7
            // semitone bend (value 9387) must become the same +7 at 12 semitones
            Array<MidiMessage> in;
            in.add(MidiMessage::pitchWheel(2, 9387));         // member, +7 st at 48
            in.add(MidiMessage::pitchWheel(1, 10000));        // master: left alone
            in.add(MidiMessage::controllerEvent(2, 74, 50));  // not pitch bend: left alone
            auto out = runMpe(state, MPE_BEND, {"lower:15", "48", "12"}, in);

            int memberBend = -1, masterBend = -1; bool cc74Seen = false;
            for (const auto& m : out)
            {
                if (m.isPitchWheel() && m.getChannel() == 2) memberBend = m.getPitchWheelValue();
                if (m.isPitchWheel() && m.getChannel() == 1) masterBend = m.getPitchWheelValue();
                if (m.isController() && m.getControllerNumber() == 74) cc74Seen = true;
            }
            // 8192 + (9387-8192) * 48/12 = 8192 + 1195*4 = 12972
            expectEquals(memberBend, 12972);
            expectEquals(masterBend, 10000);   // master bend unchanged
            expect(cc74Seen);                  // non-bend messages pass through
        }

        beginTest("mpesens declares the member Pitch Bend Sensitivity before the first note");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));   // first note triggers the declaration
            auto out = runMpe(state, MPE_SENS, {"lower:3", "24"}, in);

            // RPN 0 = 24 declared on every member channel (2, 3, 4), before the note
            int noteIdx = -1, lastSenseIdx = -1, sense = -1;
            bool onMaster = false;
            for (int i = 0; i < out.size(); ++i)
            {
                if (out[i].isNoteOn()) noteIdx = i;
                if (out[i].isController() && out[i].getControllerNumber() == 6)
                {
                    sense = out[i].getControllerValue();
                    lastSenseIdx = i;
                    if (out[i].getChannel() == 1) onMaster = true;
                }
            }
            expectEquals(sense, 24);
            expect(! onMaster);              // member channels only, not the master
            expect(lastSenseIdx < noteIdx);  // declared before the Note On
        }

        beginTest("mpesens re-declares the sensitivity after an MPE Configuration Message");
        {
            // an MCM resets member sensitivity to 48 on the receiver, so it is
            // re-declared right after the MCM is forwarded
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 101, 0));
            in.add(MidiMessage::controllerEvent(1, 100, 6));
            in.add(MidiMessage::controllerEvent(1, 6, 3));    // MCM: lower, 3 members
            auto out = runMpe(state, MPE_SENS, {"lower:3", "24"}, in);

            bool mcmForwarded = false;
            int memberSense = -1;
            for (const auto& m : out)
            {
                if (m.isController() && m.getControllerNumber() == 6 && m.getChannel() == 1 && m.getControllerValue() == 3)
                    mcmForwarded = true;
                if (m.isController() && m.getControllerNumber() == 6 && m.getChannel() != 1)
                    memberSense = m.getControllerValue();
            }
            expect(mcmForwarded);          // the MCM passes through untouched
            expectEquals(memberSense, 24); // and our sensitivity follows it
        }

        beginTest("relocate rewrites the MCM member count to the destination zone");
        {
            // an MPE Configuration Message announcing 15 members, relocated to an
            // 8-member zone, should announce 8 (and move to the destination master)
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 101, 0));
            in.add(MidiMessage::controllerEvent(1, 100, 6));
            in.add(MidiMessage::controllerEvent(1, 6, 15));
            auto out = runMpe(state, MPE_RELOCATE, {"lower:15", "lower:8"}, in);

            int memberCount = -1;
            for (const auto& m : out)
            {
                expectEquals(m.getChannel(), 1);   // lower -> lower master stays channel 1
                if (m.isController() && m.getControllerNumber() == 6) memberCount = m.getControllerValue();
            }
            expectEquals(memberCount, 8);
        }

        beginTest("relocate falls back to the remaining note after a collision releases");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));            // older note (ch 2)
            in.add(MidiMessage::noteOn(4, 64, (uint8)100));            // newest -> owns dst ch 2
            in.add(MidiMessage::noteOff(4, 64, (uint8)0));            // it releases
            in.add(MidiMessage::pitchWheel(2, 7000));                 // ch 2 owns dst ch 2 again
            in.add(MidiMessage::pitchWheel(4, 3000));                 // ch 4 no longer held: suppressed
            auto out = runMpe(state, MPE_RELOCATE, {"lower:4", "lower:2"}, in);

            int bend = -1, bendCount = 0;
            for (const auto& m : out)
                if (m.isPitchWheel()) { bend = m.getPitchWheelValue(); ++bendCount; }
            expectEquals(bendCount, 1);
            expectEquals(bend, 7000);
        }

        beginTest("collapse folds every zone channel onto one channel");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));   // master
            in.add(MidiMessage::noteOn(2, 64, (uint8)100));   // member
            in.add(MidiMessage::noteOn(5, 67, (uint8)100));   // member
            in.add(MidiMessage::noteOn(11, 70, (uint8)100));  // outside a 4-member zone (ch 1-5)

            auto out = runMpe(state, MPE_COLLAPSE, {"lower:4", "1"}, in);
            expectEquals(out.size(), 4);
            expectEquals(out[0].getChannel(), 1);
            expectEquals(out[1].getChannel(), 1);
            expectEquals(out[2].getChannel(), 1);
            expectEquals(out[3].getChannel(), 11);  // untouched
        }

        beginTest("collapse turns per-note channel pressure into poly aftertouch");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));            // note on member 2
            in.add(MidiMessage::noteOn(3, 64, (uint8)100));            // note on member 3
            in.add(MidiMessage::channelPressureChange(2, 90));        // pressure for note 60
            in.add(MidiMessage::channelPressureChange(3, 40));        // pressure for note 64

            auto out = runMpe(state, MPE_COLLAPSE, {"lower:15", "1"}, in);

            // the two pressures keep their identity as polyphonic aftertouch on
            // the target channel, one per note, instead of fighting over a single
            // channel-pressure value
            int at60 = -1, at64 = -1;
            for (const auto& m : out)
            {
                expect(! m.isChannelPressure());  // none survive as channel pressure
                if (m.isAftertouch() && m.getNoteNumber() == 60) at60 = m.getAfterTouchValue();
                if (m.isAftertouch() && m.getNoteNumber() == 64) at64 = m.getAfterTouchValue();
            }
            expectEquals(at60, 90);
            expectEquals(at64, 40);
            // and they land on the collapse target channel
            for (const auto& m : out)
                if (m.isAftertouch())
                    expectEquals(m.getChannel(), 1);
        }

        beginTest("collapse applies pitch bend and CC74 for the active note only");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));           // older note
            in.add(MidiMessage::noteOn(3, 64, (uint8)100));           // newest note -> active
            in.add(MidiMessage::pitchWheel(2, 7000));                 // from older note: suppressed
            in.add(MidiMessage::pitchWheel(3, 9000));                 // from active note: passes
            in.add(MidiMessage::controllerEvent(2, 74, 20));          // older timbre: suppressed
            in.add(MidiMessage::controllerEvent(3, 74, 100));         // active timbre: passes

            auto out = runMpe(state, MPE_COLLAPSE, {"lower:15", "1"}, in);

            int bend = -1, sense = -1, timbre = -1, bendCount = 0, timbreCount = 0;
            for (const auto& m : out)
            {
                if (m.isPitchWheel()) { bend = m.getPitchWheelValue(); ++bendCount; expectEquals(m.getChannel(), 1); }
                if (m.isController() && m.getControllerNumber() == 74) { timbre = m.getControllerValue(); ++timbreCount; }
                if (m.isController() && m.getControllerNumber() == 6) sense = m.getControllerValue();   // RPN 0 data entry
            }
            // only the active note's bend survived, summed with the (neutral) Manager
            // bend at 48 + 2 = 50 semitone range: 9000 -> ~4.73 st -> value 8968
            expectEquals(bendCount, 1);
            expectEquals(sense, 50);
            expectEquals(bend, 8968);
            expectEquals(timbreCount, 1);
            expectEquals(timbre, 100);       // Max(Manager 0, Member 100)
        }

        beginTest("collapse falls back to the previous note when the active one releases");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));           // older note (member 2)
            in.add(MidiMessage::noteOn(3, 64, (uint8)100));           // newest note (member 3) -> active
            in.add(MidiMessage::noteOff(3, 64, (uint8)0));           // active releases
            in.add(MidiMessage::pitchWheel(2, 5000));                // member 2 is active again
            in.add(MidiMessage::pitchWheel(3, 6000));                // member 3 no longer held: suppressed

            auto out = runMpe(state, MPE_COLLAPSE, {"lower:15", "1"}, in);

            int bend = -1, bendCount = 0;
            for (const auto& m : out)
                if (m.isPitchWheel()) { bend = m.getPitchWheelValue(); ++bendCount; }
            expectEquals(bendCount, 1);
            expectEquals(bend, 5128);        // member 2's 5000 summed with neutral Manager at 50-st range
        }

        beginTest("collapse sums Manager and Member pitch bend (Appendix C)");
        {
            // Appendix C receiver example: Member +7 (9387) and Manager +2 (16383)
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::pitchWheel(1, 16383));               // Manager +2 semitones
            in.add(MidiMessage::pitchWheel(2, 9387));                // Member +7 semitones
            auto out = runMpe(state, MPE_COLLAPSE, {"lower:15", "1"}, in);

            int lastBend = -1, sense = -1;
            for (const auto& m : out)
            {
                if (m.isPitchWheel()) lastBend = m.getPitchWheelValue();
                if (m.isController() && m.getControllerNumber() == 6) sense = m.getControllerValue();
            }
            expectEquals(sense, 50);         // 2 + 48
            expectEquals(lastBend, 9667);    // ~9 semitones at 50-semitone range
        }

        beginTest("collapse combines Manager and Member pressure/CC74 with the maximum (Appendix D)");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::channelPressureChange(1, 40));       // Manager pressure
            in.add(MidiMessage::channelPressureChange(2, 90));       // Member pressure
            in.add(MidiMessage::controllerEvent(1, 74, 100));        // Manager CC74
            in.add(MidiMessage::controllerEvent(2, 74, 30));         // Member CC74
            auto out = runMpe(state, MPE_COLLAPSE, {"lower:15", "1"}, in);

            int lastAt = -1, lastCC74 = -1;
            for (const auto& m : out)
            {
                if (m.isAftertouch() && m.getNoteNumber() == 60) lastAt = m.getAfterTouchValue();
                if (m.isController() && m.getControllerNumber() == 74) lastCC74 = m.getControllerValue();
            }
            expectEquals(lastAt, 90);        // Max(Manager 40, Member 90)
            expectEquals(lastCC74, 100);     // Max(Manager 100, Member 30)
        }

        beginTest("two single-zone collapses in one route keep independent per-zone state");
        {
            // a Lower-zone and an Upper-zone collapse on the same input must not
            // share state: the Upper collapse needs Manager sensitivity 2 on
            // channel 16, not the 48 that the Lower collapse leaves there
            Route route;
            route.mpeOps.add(makeCommand(MPE_COLLAPSE, {"lower:7", "1"}));   // Lower -> ch 1
            route.mpeOps.add(makeCommand(MPE_COLLAPSE, {"upper:7", "2"}));   // Upper -> ch 2
            route.inputs.add(new RouteInput());
            auto& input = *route.inputs[0];

            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));     // Lower member -> ch 1
            in.add(MidiMessage::noteOn(9, 64, (uint8)100));     // Upper member -> ch 2
            in.add(MidiMessage::pitchWheel(16, 16383));         // Upper Manager bend, +2 semitones

            Array<MidiMessage> out;
            for (const auto& m : in)
                state.processMpe(route, input, m, out);

            int onCh1 = 0, onCh2 = 0, upperSense = -1;
            for (const auto& m : out)
            {
                if (m.isNoteOn() && m.getChannel() == 1) ++onCh1;
                if (m.isNoteOn() && m.getChannel() == 2) ++onCh2;
                if (m.isController() && m.getControllerNumber() == 6 && m.getChannel() == 2) upperSense = m.getControllerValue();
            }
            expectEquals(onCh1, 1);        // Lower note collapsed to its channel
            expectEquals(onCh2, 1);        // Upper note collapsed to its channel
            expectEquals(upperSense, 50);  // 2 (ch 16 Manager) + 48 (member); 96 if state were shared
        }

        beginTest("collapse sends a note's initial expression before its Note On (section 2.4)");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::pitchWheel(1, 10000));        // Manager bend, before any note
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            auto out = runMpe(state, MPE_COLLAPSE, {"lower:15", "1"}, in);

            int bendIdx = -1, noteIdx = -1;
            for (int i = 0; i < out.size(); ++i)
            {
                if (out[i].isPitchWheel()) bendIdx = i;
                if (out[i].isNoteOn())     noteIdx = i;
            }
            expect(bendIdx >= 0 && noteIdx >= 0);
            expect(bendIdx < noteIdx);   // the combined bend precedes the Note On
        }

        beginTest("expand emits the configuration message before the first note");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            auto out = runMpe(state, MPE_EXPAND, {"1", "lower:15"}, in);

            // the MPE Configuration Message (RPN 6 on the master) comes first
            expect(out.size() >= 6);
            expect(out[0].isController() && out[0].getControllerNumber() == 101 && out[0].getControllerValue() == 0);
            expect(out[1].isController() && out[1].getControllerNumber() == 100 && out[1].getControllerValue() == 6);
            expect(out[2].isController() && out[2].getControllerNumber() == 6   && out[2].getControllerValue() == 15);
            expect(out[1].getChannel() == 1);  // sent on the master channel
            // then the note, on the first member channel
            const auto& note = out[out.size() - 1];
            expect(note.isNoteOn());
            expectEquals(note.getChannel(), 2);
            expectEquals(note.getNoteNumber(), 60);
        }

        beginTest("expand spreads notes round-robin across member channels");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in.add(MidiMessage::noteOn(1, 64, (uint8)100));
            in.add(MidiMessage::noteOn(1, 67, (uint8)100));
            auto out = runMpe(state, MPE_EXPAND, {"1", "lower:3"}, in);

            // collect the channels of the three note-ons (after the config block)
            Array<int> noteChannels;
            for (const auto& m : out)
                if (m.isNoteOn())
                    noteChannels.add(m.getChannel());
            expectEquals(noteChannels.size(), 3);
            // a 3-member Lower zone uses channels 2, 3, 4, each once
            expect(noteChannels.contains(2) && noteChannels.contains(3) && noteChannels.contains(4));
        }

        beginTest("expand routes a note-off to the same member channel as its note-on");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in.add(MidiMessage::noteOff(1, 60, (uint8)0));
            auto out = runMpe(state, MPE_EXPAND, {"1", "lower:15"}, in);

            int onChannel = -1, offChannel = -2;
            for (const auto& m : out)
            {
                if (m.isNoteOn())  onChannel = m.getChannel();
                if (m.isNoteOff()) offChannel = m.getChannel();
            }
            expectEquals(onChannel, 2);
            expectEquals(offChannel, onChannel);
        }

        beginTest("expand shares a member channel when polyphony exceeds the zone");
        {
            // a single-member zone (channel 2 only): a second simultaneous note
            // shares the channel rather than stealing the first, as the channel
            // bucket and the MPE spec (2.2.4.1) prescribe
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in.add(MidiMessage::noteOn(1, 64, (uint8)100));
            auto out = runMpe(state, MPE_EXPAND, {"1", "lower:1"}, in);

            int noteOns = 0, noteOffs = 0;
            for (const auto& m : out)
            {
                if (m.isNoteOn())  { ++noteOns;  expectEquals(m.getChannel(), 2); }
                if (m.isNoteOff()) ++noteOffs;
            }
            expectEquals(noteOns, 2);
            expectEquals(noteOffs, 0);   // shared, not stolen

            // both notes can be released independently afterwards (on the shared channel)
            Array<MidiMessage> in2;
            in2.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in2.add(MidiMessage::noteOn(1, 64, (uint8)100));
            in2.add(MidiMessage::noteOff(1, 60, (uint8)0));
            in2.add(MidiMessage::noteOff(1, 64, (uint8)0));
            auto out2 = runMpe(state, MPE_EXPAND, {"1", "lower:1"}, in2);
            int offs = 0;
            for (const auto& m : out2)
                if (m.isNoteOff()) { ++offs; expectEquals(m.getChannel(), 2); }
            expectEquals(offs, 2);
        }

        beginTest("expand allocator reuses a channel only after others have been used");
        {
            // 3-member zone: after using and releasing channel 2, the next note
            // takes channel 3 (reuse is postponed), matching the channel bucket
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));   // -> member 2
            in.add(MidiMessage::noteOff(1, 60, (uint8)0));    // release 2
            in.add(MidiMessage::noteOn(1, 64, (uint8)100));   // -> member 3, not 2
            auto out = runMpe(state, MPE_EXPAND, {"1", "lower:3"}, in);

            Array<int> onChannels;
            for (const auto& m : out)
                if (m.isNoteOn()) onChannels.add(m.getChannel());
            expectEquals(onChannels.size(), 2);
            expectEquals(onChannels[0], 2);
            expectEquals(onChannels[1], 3);   // postponed reuse of channel 2
        }

        beginTest("expand zeroes Channel Pressure before Note On and Note Off (Appendix A.4.2)");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in.add(MidiMessage::noteOff(1, 60, (uint8)0));
            auto out = runMpe(state, MPE_EXPAND, {"1", "lower:15"}, in);

            int noteOnIdx = -1, noteOffIdx = -1;
            for (int i = 0; i < out.size(); ++i)
            {
                if (out[i].isNoteOn())  noteOnIdx = i;
                if (out[i].isNoteOff()) noteOffIdx = i;
            }
            expect(noteOnIdx > 0);
            expect(out[noteOnIdx - 1].isChannelPressure() && out[noteOnIdx - 1].getChannelPressureValue() == 0);
            expect(noteOffIdx > 0);
            expect(out[noteOffIdx - 1].isChannelPressure() && out[noteOffIdx - 1].getChannelPressureValue() == 0);
        }

        beginTest("expand sends zone-wide messages to the master channel");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::pitchWheel(1, 10000));
            in.add(MidiMessage::channelPressureChange(1, 50));
            auto out = runMpe(state, MPE_EXPAND, {"1", "lower:15"}, in);

            // no notes, so no config block; both go to master channel 1
            for (const auto& m : out)
                expectEquals(m.getChannel(), 1);
            expectEquals(out.size(), 2);
        }

        beginTest("expand turns polyphonic aftertouch into per-channel channel pressure");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));          // -> member channel 2
            in.add(MidiMessage::noteOn(1, 64, (uint8)100));          // -> member channel 3
            in.add(MidiMessage::aftertouchChange(1, 60, 77));       // poly pressure for note 60
            in.add(MidiMessage::aftertouchChange(1, 64, 33));       // poly pressure for note 64

            auto out = runMpe(state, MPE_EXPAND, {"1", "lower:15"}, in);

            int cp2 = -1, cp3 = -1;
            for (const auto& m : out)
            {
                expect(! m.isAftertouch());  // none survive as poly aftertouch
                if (m.isChannelPressure() && m.getChannel() == 2) cp2 = m.getChannelPressureValue();
                if (m.isChannelPressure() && m.getChannel() == 3) cp3 = m.getChannelPressureValue();
            }
            // note 60's pressure becomes channel pressure on its member channel 2,
            // note 64's on member channel 3 -- independent per-note pressure
            expectEquals(cp2, 77);
            expectEquals(cp3, 33);
        }

        beginTest("split sends each voice to its own port, rechanneled to one channel");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::noteOn(3, 64, (uint8)100));
            in.add(MidiMessage::noteOn(4, 67, (uint8)100));
            auto r = runSplit(state, {"lower:15"}, 3, in);

            expectEquals(r.msgs.size(), 3);
            // three distinct ports, in round-robin order
            expectEquals(r.ports[0], 0);
            expectEquals(r.ports[1], 1);
            expectEquals(r.ports[2], 2);
            // every note collapsed onto channel 1
            for (const auto& m : r.msgs)
            {
                expect(m.isNoteOn());
                expectEquals(m.getChannel(), 1);
            }
        }

        beginTest("split steals the oldest port when every port is busy");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::noteOn(3, 64, (uint8)100));
            in.add(MidiMessage::noteOn(4, 67, (uint8)100));   // no free port -> steal port 0
            auto r = runSplit(state, {"lower:15"}, 2, in);

            // on, on, then (note-off for the stolen note 60) + (note-on 67), all on port 0
            expectEquals(r.msgs.size(), 4);
            expect(r.msgs[2].isNoteOff());
            expectEquals(r.msgs[2].getNoteNumber(), 60);
            expectEquals(r.ports[2], 0);
            expect(r.msgs[3].isNoteOn());
            expectEquals(r.msgs[3].getNoteNumber(), 67);
            expectEquals(r.ports[3], 0);
        }

        beginTest("split routes per-note expression to the note's port, combined and declared");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::pitchWheel(2, 9000));         // expression for that note
            auto r = runSplit(state, {"lower:15"}, 4, in);

            int notePort = -2, bendPort = -3, bend = -1, sense = -1;
            for (int i = 0; i < r.msgs.size(); ++i)
            {
                if (r.msgs[i].isNoteOn())     notePort = r.ports[i];
                if (r.msgs[i].isPitchWheel()) { bend = r.msgs[i].getPitchWheelValue(); bendPort = r.ports[i]; expectEquals(r.msgs[i].getChannel(), 1); }
                if (r.msgs[i].isController() && r.msgs[i].getControllerNumber() == 6) sense = r.msgs[i].getControllerValue();
            }
            expectEquals(bendPort, notePort);   // bend goes to the note's port
            expectEquals(sense, 50);            // combined sensitivity declared on the port
            expectEquals(bend, 8968);           // 9000 + neutral Manager at 50-semitone range
        }

        beginTest("split folds zone-wide Manager bend into each active note's port");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));   // -> a port
            in.add(MidiMessage::pitchWheel(1, 10000));        // Manager bend (zone-wide)
            auto r = runSplit(state, {"lower:15"}, 3, in);

            int notePort = -2, bendPort = -3, bend = -1;
            for (int i = 0; i < r.msgs.size(); ++i)
            {
                if (r.msgs[i].isNoteOn())     notePort = r.ports[i];
                if (r.msgs[i].isPitchWheel()) { bend = r.msgs[i].getPitchWheelValue(); bendPort = r.ports[i]; }
            }
            // the Manager bend is applied to the note's port (not broadcast as -1)
            expectEquals(bendPort, notePort);
            expect(bendPort >= 0);
            expectEquals(bend, 8264);           // +2-st Manager at 10000 summed onto the note
        }

        beginTest("split sums Manager and Member bend per port (Appendix C)");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::pitchWheel(1, 16383));        // Manager +2 semitones
            in.add(MidiMessage::pitchWheel(2, 9387));         // Member +7 semitones
            auto r = runSplit(state, {"lower:15"}, 3, in);

            int lastBend = -1, sense = -1;
            for (const auto& m : r.msgs)
            {
                if (m.isPitchWheel()) lastBend = m.getPitchWheelValue();
                if (m.isController() && m.getControllerNumber() == 6) sense = m.getControllerValue();
            }
            expectEquals(sense, 50);
            expectEquals(lastBend, 9667);
        }

        beginTest("split combines Manager and Member pressure/CC74 with the maximum per port");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::channelPressureChange(1, 40));   // Manager
            in.add(MidiMessage::channelPressureChange(2, 90));   // Member
            in.add(MidiMessage::controllerEvent(1, 74, 100));    // Manager CC74
            in.add(MidiMessage::controllerEvent(2, 74, 30));     // Member CC74
            auto r = runSplit(state, {"lower:15"}, 3, in);

            int lastCp = -1, lastCC74 = -1;
            for (const auto& m : r.msgs)
            {
                if (m.isChannelPressure()) lastCp = m.getChannelPressureValue();
                if (m.isController() && m.getControllerNumber() == 74) lastCC74 = m.getControllerValue();
            }
            expectEquals(lastCp, 90);     // Max(Manager 40, Member 90)
            expectEquals(lastCC74, 100);  // Max(Manager 100, Member 30)
        }

        beginTest("split sends a note's initial expression before its Note On (section 2.4)");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::pitchWheel(1, 10000));        // Manager bend, before any note
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            auto r = runSplit(state, {"lower:15"}, 3, in);

            int bendIdx = -1, noteIdx = -1;
            for (int i = 0; i < r.msgs.size(); ++i)
            {
                if (r.msgs[i].isPitchWheel()) bendIdx = i;
                if (r.msgs[i].isNoteOn())     noteIdx = i;
            }
            expect(bendIdx >= 0 && noteIdx >= 0);
            expect(bendIdx < noteIdx);
        }

        beginTest("split drops expression with no active note on its channel");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::pitchWheel(2, 9000));         // member channel, but no note held
            auto r = runSplit(state, {"lower:15"}, 3, in);
            expectEquals(r.msgs.size(), 0);
        }

        beginTest("split frees a port on note-off so it can be reused cleanly");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::noteOff(2, 60, (uint8)0));
            in.add(MidiMessage::noteOn(3, 64, (uint8)100));   // reuses the freed port, no steal
            auto r = runSplit(state, {"lower:15"}, 1, in);

            int noteOns = 0, noteOffs = 0;
            for (const auto& m : r.msgs)
            {
                if (m.isNoteOn())  ++noteOns;
                if (m.isNoteOff()) ++noteOffs;
                expectEquals(m.getChannel(), 1);
            }
            expectEquals(noteOns, 2);
            expectEquals(noteOffs, 1);     // only the explicit note-off, no stolen one
            for (const auto& p : r.ports)
                expectEquals(p, 0);
        }

        beginTest("split rechannels onto a chosen target channel");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            auto r = runSplit(state, {"lower:15", "10"}, 2, in);
            expectEquals(r.msgs.size(), 1);
            expectEquals(r.msgs[0].getChannel(), 10);
        }

        beginTest("split suppresses the MPE Configuration Message (RPN 6)");
        {
            // the MCM as emitted on the master channel: select RPN 6, set it, null
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 101, 0));
            in.add(MidiMessage::controllerEvent(1, 100, 6));
            in.add(MidiMessage::controllerEvent(1, 6, 15));
            in.add(MidiMessage::controllerEvent(1, 101, 127));
            in.add(MidiMessage::controllerEvent(1, 100, 127));
            auto r = runSplit(state, {"lower:15"}, 2, in);
            expectEquals(r.msgs.size(), 0);   // the whole MCM is dropped
        }

        beginTest("split lets other RPNs through, replaying their selection");
        {
            // a non-MPE RPN (here RPN 3, neither the MCM's RPN 6 nor the
            // sensitivity RPN 0) must survive and pass through to the ports
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 101, 0));
            in.add(MidiMessage::controllerEvent(1, 100, 3));
            in.add(MidiMessage::controllerEvent(1, 6, 5));
            auto r = runSplit(state, {"lower:15"}, 2, in);

            expectEquals(r.msgs.size(), 3);   // selection (101, 100) replayed, then data (6)
            for (int i = 0; i < r.msgs.size(); ++i)
            {
                expect(r.msgs[i].isController());
                expectEquals(r.ports[i], -1);          // broadcast to all ports
                expectEquals(r.msgs[i].getChannel(), 1);
            }
            expectEquals(r.msgs[0].getControllerNumber(), 101);
            expectEquals(r.msgs[1].getControllerNumber(), 100);
            expectEquals(r.msgs[2].getControllerNumber(), 6);
            expectEquals(r.msgs[2].getControllerValue(), 5);
        }
    }
};

static MpeTests mpeTests;
