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

        beginTest("collapse passes pitch bend for the most recently triggered note only");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));           // older note
            in.add(MidiMessage::noteOn(3, 64, (uint8)100));           // newest note -> active
            in.add(MidiMessage::pitchWheel(2, 7000));                 // from older note: suppressed
            in.add(MidiMessage::pitchWheel(3, 9000));                 // from active note: passes
            in.add(MidiMessage::controllerEvent(2, 74, 20));          // older timbre: suppressed
            in.add(MidiMessage::controllerEvent(3, 74, 100));         // active timbre: passes

            auto out = runMpe(state, MPE_COLLAPSE, {"lower:15", "1"}, in);

            int bend = -1, timbre = -1, bendCount = 0, timbreCount = 0;
            for (const auto& m : out)
            {
                if (m.isPitchWheel()) { bend = m.getPitchWheelValue(); ++bendCount; expectEquals(m.getChannel(), 1); }
                if (m.isController() && m.getControllerNumber() == 74) { timbre = m.getControllerValue(); ++timbreCount; }
            }
            expectEquals(bendCount, 1);     // only the active note's bend survived
            expectEquals(bend, 9000);
            expectEquals(timbreCount, 1);
            expectEquals(timbre, 100);
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
            expectEquals(bend, 5000);
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

        beginTest("expand reuses freed channels and steals when full");
        {
            // a single-member zone (channel 2 only): a second simultaneous note
            // must steal the channel, which means releasing the first note there
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in.add(MidiMessage::noteOn(1, 64, (uint8)100));
            auto out = runMpe(state, MPE_EXPAND, {"1", "lower:1"}, in);

            int noteOns = 0, noteOffs = 0;
            for (const auto& m : out)
            {
                if (m.isNoteOn())  { ++noteOns;  expectEquals(m.getChannel(), 2); }
                if (m.isNoteOff()) { ++noteOffs; expectEquals(m.getChannel(), 2); expectEquals(m.getNoteNumber(), 60); }
            }
            expectEquals(noteOns, 2);
            expectEquals(noteOffs, 1);  // the stolen note 60 was released
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

        beginTest("split routes per-note expression to the note's port");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));
            in.add(MidiMessage::pitchWheel(2, 9000));         // expression for that note
            auto r = runSplit(state, {"lower:15"}, 4, in);

            expectEquals(r.msgs.size(), 2);
            expect(r.msgs[1].isPitchWheel());
            expectEquals(r.ports[1], r.ports[0]);             // same port as its note
            expectEquals(r.msgs[1].getChannel(), 1);          // rechanneled
        }

        beginTest("split broadcasts zone-wide master messages to all ports");
        {
            Array<MidiMessage> in;
            in.add(MidiMessage::pitchWheel(1, 10000));        // master channel of the Lower zone
            auto r = runSplit(state, {"lower:15"}, 3, in);

            expectEquals(r.msgs.size(), 1);
            expectEquals(r.ports[0], -1);                     // -1 = broadcast to every port
            expect(r.msgs[0].isPitchWheel());
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
            // RPN 0 (pitch-bend sensitivity) must survive the RPN-6 suppression
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 101, 0));
            in.add(MidiMessage::controllerEvent(1, 100, 0));
            in.add(MidiMessage::controllerEvent(1, 6, 2));
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
            expectEquals(r.msgs[2].getControllerValue(), 2);
        }
    }
};

static MpeTests mpeTests;
