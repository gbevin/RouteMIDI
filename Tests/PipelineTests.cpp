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

#include <iostream>

// Exercises whole routes built from real command lines, sending mixed message
// types through every stage at once (filters -> transforms -> MPE ->
// conversions), where the interesting behavior lives in the interactions
// between stateful stages rather than in any single command.
class PipelineTests : public UnitTest
{
public:
    PipelineTests() : UnitTest("Pipeline", "Pipeline") {}

    // feeds a command line into the parser; ports won't resolve to real
    // devices, so the resulting "couldn't find ... waiting" notices are
    // suppressed here
    void parse(ApplicationState& state, const String& line)
    {
        StringArray params;
        params.addTokens(line, true);
        params.removeEmptyStrings(true);
        for (auto& p : params)
        {
            p = p.trimCharactersAtStart("\"").trimCharactersAtEnd("\"");
        }

        auto* previous = std::cerr.rdbuf(nullptr);
        state.parseParameters(params);
        std::cerr.rdbuf(previous);
    }

    // runs one message through the route's full pipeline, collecting the
    // messages the route would send to its outputs
    Array<MidiMessage> run(ApplicationState& state, const MidiMessage& msg)
    {
        Route& route = *state.getRoutes().getFirst();
        RouteInput& input = *route.inputs.getFirst();
        Array<MidiMessage> outMsgs;
        Array<int> outPorts;
        state.processRouteMessage(route, input, msg, outMsgs, outPorts);
        return outMsgs;
    }

    Array<MidiMessage> runAll(ApplicationState& state, const Array<MidiMessage>& in)
    {
        Array<MidiMessage> out;
        for (const auto& m : in)
        {
            out.addArray(run(state, m));
        }
        return out;
    }

    // renders a message sequence compactly so a whole pipeline result can be
    // asserted in one comparison
    String describe(const Array<MidiMessage>& msgs)
    {
        StringArray parts;
        for (const auto& m : msgs)
        {
            if      (m.isNoteOn())         parts.add("on:" + String(m.getChannel()) + ":" + String(m.getNoteNumber()) + ":" + String(m.getVelocity()));
            else if (m.isNoteOff())        parts.add("off:" + String(m.getChannel()) + ":" + String(m.getNoteNumber()));
            else if (m.isController())     parts.add("cc:" + String(m.getChannel()) + ":" + String(m.getControllerNumber()) + ":" + String(m.getControllerValue()));
            else if (m.isChannelPressure())parts.add("cp:" + String(m.getChannel()) + ":" + String(m.getChannelPressureValue()));
            else if (m.isPitchWheel())     parts.add("pb:" + String(m.getChannel()) + ":" + String(m.getPitchWheelValue()));
            else if (m.isMidiClock())      parts.add("clock");
            else                           parts.add("other");
        }
        return parts.joinIntoString(" ");
    }

    void runTest() override
    {
        beginTest("System Real-Time interleaves inside an (N)RPN frame across filter and conversion");
        {
            // clocks can arrive between any two bytes of an NRPN frame; the
            // parameter filter and the converter must let them through untouched
            // while the frame still assembles, converts exactly, and keeps its
            // constituents (selects, data, closing null) consumed
            ApplicationState state;
            parse(state, "in X nrpn 1000 clock convert nrpn 1000 cc 7 out Y");

            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 99, 7));      // select MSB (buffered)
            in.add(MidiMessage::midiClock());
            in.add(MidiMessage::controllerEvent(1, 98, 104));    // select LSB -> intercepted
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));      // not whitelisted: blocked
            in.add(MidiMessage::controllerEvent(1, 6, 62));      // data MSB (warm-up emit)
            in.add(MidiMessage::midiClock());
            in.add(MidiMessage::controllerEvent(1, 38, 64));     // data LSB -> exact pair
            in.add(MidiMessage::controllerEvent(1, 99, 127));    // closing null
            in.add(MidiMessage::controllerEvent(1, 98, 127));

            expectEquals(describe(runAll(state, in)),
                         String("clock cc:1:7:62 clock cc:1:7:62"));
        }

        beginTest("A rechanneling transform feeds the converter's per-channel selection");
        {
            // chset runs in the transform stage, so the converter tracks the
            // (N)RPN selection on the rewritten channel and emits there
            ApplicationState state;
            parse(state, "in X chset 5 convert nrpn 1000 cc 7 out Y");

            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(2, 99, 7));
            in.add(MidiMessage::controllerEvent(2, 98, 104));
            in.add(MidiMessage::controllerEvent(2, 6, 62));
            in.add(MidiMessage::controllerEvent(2, 38, 64));

            expectEquals(describe(runAll(state, in)),
                         String("cc:5:7:62 cc:5:7:62"));
        }

        beginTest("A note-to-CC transform fabricates data entry for the active selection");
        {
            // notecc turns a note into CC 6 in the transform stage, so with a
            // selection active the fabricated controller is that parameter's
            // data entry, exactly as if the device had sent it
            ApplicationState state;
            parse(state, "in X notecc 60 6 convert nrpn 1000 cc 7 out Y");

            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 99, 7));
            in.add(MidiMessage::controllerEvent(1, 98, 104));
            in.add(MidiMessage::noteOn(1, 60, (uint8)90));       // becomes CC 6 = 90
            in.add(MidiMessage::controllerEvent(1, 38, 64));     // pairs with it

            expectEquals(describe(runAll(state, in)),
                         String("cc:1:7:90 cc:1:7:90"));
        }

        beginTest("Scale folding upstream of an any-note pressure collapse");
        {
            // C#3 folds onto C3, so two physical keys share one note. The
            // any-note pressure collapse tracks per note: releasing one of the
            // folded keys drops the combined pressure to 0 early, and the next
            // poly pressure restores it - the documented cost of note folding
            ApplicationState state;
            parse(state, "in X scale C major convert pp cp out Y");

            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));       // C3
            in.add(MidiMessage::noteOn(1, 61, (uint8)100));       // C#3 -> folds to C3
            in.add(MidiMessage::aftertouchChange(1, 60, 40));
            in.add(MidiMessage::aftertouchChange(1, 61, 70));     // folds to C3: max wins
            in.add(MidiMessage::noteOff(1, 61, (uint8)0));        // folded off releases C3
            in.add(MidiMessage::aftertouchChange(1, 60, 50));     // restores the pressure
            in.add(MidiMessage::noteOff(1, 60, (uint8)0));

            expectEquals(describe(runAll(state, in)),
                         String("on:1:60:100 on:1:60:100 cp:1:40 cp:1:70 cp:1:0 off:1:60 cp:1:50 cp:1:0 off:1:60"));
        }

        beginTest("Chord and sustain stack: the pedal holds fabricated chord notes");
        {
            // the chord transform fabricates the extra notes before sustain, so
            // the pedal holds the whole triad and releases it together
            ApplicationState state;
            parse(state, "in X chord 4 7 sustain out Y");

            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 64, 127));     // pedal down (consumed)
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in.add(MidiMessage::noteOff(1, 60, (uint8)0));        // whole triad held
            in.add(MidiMessage::controllerEvent(1, 64, 0));       // pedal up releases it

            expectEquals(describe(runAll(state, in)),
                         String("on:1:60:100 on:1:64:100 on:1:67:100 off:1:60 off:1:64 off:1:67"));
        }

        beginTest("A velocity-0 note-on releases like a note-off through the pedal");
        {
            // many devices send note-offs as velocity-0 note-ons (running
            // status); the pedal must hold and release them like real offs
            ApplicationState state;
            parse(state, "in X sustain out Y");

            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 64, 127));
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in.add(MidiMessage(0x90, 60, 0));                     // raw velocity-0 note-on
            in.add(MidiMessage::controllerEvent(1, 64, 0));

            const auto out = runAll(state, in);
            expectEquals(out.size(), 2);
            expect(out[0].isNoteOn());
            expect(out[1].isNoteOff());                           // released at pedal-up
            expectEquals(out[1].getNoteNumber(), 60);
        }

        beginTest("MPE collapse emits (N)RPN frames the converter leaves intact");
        {
            // collapsing a member channel makes the MPE stage fabricate a
            // complete RPN 0 declaration (with its closing null) for the
            // combined bend range; a converter rule on another controller must
            // convert the combined CC 74 while passing that frame untouched
            ApplicationState state;
            parse(state, "in X mpemono lower 1 convert cc 74 cc 1 out Y");

            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(2, 60, (uint8)100));       // member channel voice
            in.add(MidiMessage::controllerEvent(2, 74, 90));      // combined, then converted
            in.add(MidiMessage::pitchWheel(2, 9000));             // triggers the declaration
            in.add(MidiMessage::noteOff(2, 60, (uint8)0));

            expectEquals(describe(runAll(state, in)),
                         String("on:1:60:100 cc:1:1:90 "
                                "cc:1:101:0 cc:1:100:0 cc:1:6:50 cc:1:101:127 cc:1:100:127 "
                                "pb:1:8968 off:1:60"));
        }

        beginTest("Sustain before mono and mono before sustain order consistently");
        {
            // the two stateful note transforms compose differently in each
            // order, but both bookkeep cleanly: every note that sounds is
            // released and nothing sticks
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 64, 127));
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in.add(MidiMessage::noteOn(1, 64, (uint8)90));
            in.add(MidiMessage::noteOff(1, 64, (uint8)0));
            in.add(MidiMessage::noteOff(1, 60, (uint8)0));
            in.add(MidiMessage::controllerEvent(1, 64, 0));

            // sustain first: mono never sees the held note-offs, so the steal
            // decides everything and the pedal release cleans up
            ApplicationState first;
            parse(first, "in X sustain mono out Y");
            expectEquals(describe(runAll(first, in)),
                         String("on:1:60:100 off:1:60 on:1:64:90 off:1:64"));

            // mono first: the steal's note-off is held by the pedal, and mono's
            // fallback retriggers the sustained note before the final cleanup
            ApplicationState second;
            parse(second, "in X mono sustain out Y");
            expectEquals(describe(runAll(second, in)),
                         String("on:1:60:100 on:1:64:90 off:1:60 on:1:60:100 off:1:60 off:1:64"));
        }
    }
};

static PipelineTests pipelineTests;
