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

#include "../Source/ApplicationState.h"

namespace
{
    // feeds a command line into the parser; ports won't resolve to real devices,
    // so the resulting "couldn't find ... waiting" notices are suppressed here
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
}

class ParsingTests : public UnitTest
{
public:
    ParsingTests() : UnitTest("Parsing", "Parsing") {}

    void runTest() override
    {
        beginTest("A route binds an input to its outputs");
        {
            ApplicationState state;
            parse(state, "in PortA out PortB");
            expectEquals(state.getRoutes().size(), 1);
            expectEquals(state.getRoutes()[0]->inputs.size(), 1);
            expect(state.getRoutes()[0]->inputs[0]->inName == "PortA");
            expectEquals(state.getRoutes()[0]->outputs.size(), 1);
            expect(state.getRoutes()[0]->outputs[0]->name == "PortB");
        }

        beginTest("One input splits to several outputs");
        {
            ApplicationState state;
            parse(state, "in A out X out Y out Z");
            expectEquals(state.getRoutes().size(), 1);
            expectEquals(state.getRoutes()[0]->inputs.size(), 1);
            expectEquals(state.getRoutes()[0]->outputs.size(), 3);
        }

        beginTest("Several inputs merge to shared outputs");
        {
            ApplicationState state;
            parse(state, "in A in B in C out X out Y");
            expectEquals(state.getRoutes().size(), 1);
            auto* route = state.getRoutes()[0];
            expectEquals(route->inputs.size(), 3);
            expect(route->inputs[0]->inName == "A");
            expect(route->inputs[2]->inName == "C");
            expectEquals(route->outputs.size(), 2);
        }

        beginTest("An input after an output starts a new route");
        {
            ApplicationState state;
            parse(state, "in A out X in B out Y out Z");
            expectEquals(state.getRoutes().size(), 2);
            expectEquals(state.getRoutes()[0]->inputs.size(), 1);
            expect(state.getRoutes()[0]->inputs[0]->inName == "A");
            expect(state.getRoutes()[1]->inputs[0]->inName == "B");
            expectEquals(state.getRoutes()[1]->outputs.size(), 2);
        }

        beginTest("Filters and transforms attach to the current route");
        {
            ApplicationState state;
            parse(state, "in A note transp 12 out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->filters.size(), 1);
            expect(route->filters[0].command_ == NOTE);
            expectEquals(route->transforms.size(), 1);
            expect(route->transforms[0].command_ == TRANSPOSE);
            expect(route->transforms[0].opts_[0] == "12");
        }

        beginTest("'not' marks the following filter as negated");
        {
            ApplicationState state;
            parse(state, "in A not clock out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->filters.size(), 1);
            expect(route->filters[0].command_ == CLOCK);
            expect(route->filters[0].negate_);
        }

        beginTest("'convert' captures its four arguments literally");
        {
            ApplicationState state;
            parse(state, "in A convert nrpn 245 cc14 1 out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->converters.size(), 1);
            const auto& opts = route->converters[0].opts_;
            expectEquals(opts.size(), 4);
            expect(opts[0] == "nrpn");
            expect(opts[1] == "245");
            expect(opts[2] == "cc14");
            expect(opts[3] == "1");
        }

        beginTest("'convert' with pb/cp/pc needs no number and normalizes to four options");
        {
            ApplicationState state;
            // pb takes no number; it normalizes to [pb, 0, cc, 7] and "out" still parses
            parse(state, "in A convert pb cc 7 out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->converters.size(), 1);
            const auto& opts = route->converters[0].opts_;
            expectEquals(opts.size(), 4);
            expect(opts[0] == "pb");
            expect(opts[1] == "0");
            expect(opts[2] == "cc");
            expect(opts[3] == "7");
            expectEquals(route->outputs.size(), 1);
            expect(route->outputs[0]->name == "B");
        }

        beginTest("'convert' to a no-number type stops collecting at the right token");
        {
            ApplicationState state;
            parse(state, "in A convert cc 7 cp out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->converters.size(), 1);
            const auto& opts = route->converters[0].opts_;
            expectEquals(opts.size(), 4);
            expect(opts[0] == "cc");
            expect(opts[1] == "7");
            expect(opts[2] == "cp");
            expect(opts[3] == "0");
            // "out B" is not swallowed by the converter
            expectEquals(route->outputs.size(), 1);
        }

        beginTest("Fixed arguments are taken literally even when they look like commands");
        {
            ApplicationState state;
            parse(state, "in A out cc");
            auto* route = state.getRoutes()[0];
            expectEquals(route->outputs.size(), 1);
            expect(route->outputs[0]->name == "cc");
            expectEquals(route->filters.size(), 0);
        }

        beginTest("Long command names land in the correct route buckets");
        {
            ApplicationState state;
            parse(state, "input A control-change pitch-bend transpose 12 channel-set 2 "
                         "nrpn-add 1000 50 mpe-mono lower 1 output B");
            expectEquals(state.getRoutes().size(), 1);
            auto* route = state.getRoutes()[0];

            expectEquals(route->inputs.size(), 1);
            expect(route->inputs[0]->inName == "A");
            expectEquals(route->outputs.size(), 1);
            expect(route->outputs[0]->name == "B");

            // filters, in order
            expectEquals(route->filters.size(), 2);
            expect(route->filters[0].command_ == CONTROL_CHANGE);
            expect(route->filters[1].command_ == PITCH_BEND);

            // transforms keep their order and arguments
            expectEquals(route->transforms.size(), 2);
            expect(route->transforms[0].command_ == TRANSPOSE);
            expect(route->transforms[0].opts_[0] == "12");
            expect(route->transforms[1].command_ == CHANNEL_SET);
            expect(route->transforms[1].opts_[0] == "2");

            // RPN/NRPN value transforms live in the converter bucket
            expectEquals(route->converters.size(), 1);
            expect(route->converters[0].command_ == NRPN_ADD);
            expect(route->converters[0].opts_[0] == "1000");
            expect(route->converters[0].opts_[1] == "50");

            // MPE operations land in their own bucket
            expectEquals(route->mpeOps.size(), 1);
            expect(route->mpeOps[0].command_ == MPE_COLLAPSE);
            expect(route->mpeOps[0].opts_[0] == "lower");
            expect(route->mpeOps[0].opts_[1] == "1");
        }

        beginTest("MPE zone filter and split parse into the right buckets (long names)");
        {
            ApplicationState state;
            parse(state, "input A mpe-member lower:7 mpe-split lower:15 5 output B output C");
            auto* route = state.getRoutes()[0];

            expectEquals(route->filters.size(), 1);
            expect(route->filters[0].command_ == MPE_MEMBER);
            expect(route->filters[0].opts_[0] == "lower:7");

            expectEquals(route->outputSplit.size(), 1);
            expect(route->outputSplit[0].command_ == MPE_SPLIT);
            expect(route->outputSplit[0].opts_[0] == "lower:15");
            expect(route->outputSplit[0].opts_[1] == "5");

            expectEquals(route->outputs.size(), 2);
        }

        beginTest("Variable-argument commands collect an optional value without swallowing the next command");
        {
            // cc with no number: the following "out" is not consumed as its argument
            {
                ApplicationState state;
                parse(state, "in A cc out B");
                auto* route = state.getRoutes()[0];
                expectEquals(route->filters.size(), 1);
                expect(route->filters[0].command_ == CONTROL_CHANGE);
                expect(route->filters[0].opts_.isEmpty());
                expectEquals(route->outputs.size(), 1);
                expect(route->outputs[0]->name == "B");
            }
            // cc with a number captures it
            {
                ApplicationState state;
                parse(state, "in A cc 7 out B");
                auto* route = state.getRoutes()[0];
                expectEquals(route->filters.size(), 1);
                expect(route->filters[0].opts_[0] == "7");
                expectEquals(route->outputs.size(), 1);
            }
            // cc14 (optional, omitted) followed by pc (optional, given)
            {
                ApplicationState state;
                parse(state, "in A cc14 pc 5 out B");
                auto* route = state.getRoutes()[0];
                expectEquals(route->filters.size(), 2);
                expect(route->filters[0].command_ == CONTROL_CHANGE_14BIT);
                expect(route->filters[0].opts_.isEmpty());
                expect(route->filters[1].command_ == PROGRAM_CHANGE);
                expect(route->filters[1].opts_[0] == "5");
            }
        }

        beginTest("Stdin and stdout ports are recognized from '-'");
        {
            ApplicationState state;
            parse(state, "in - out -");
            auto* route = state.getRoutes()[0];
            expectEquals(route->inputs.size(), 1);
            expect(route->inputs[0]->isStdin);
            expect(route->inputs[0]->inName == "stdin");
            expectEquals(route->outputs.size(), 1);
            expect(route->outputs[0]->isStdout);
        }

        beginTest("Range selectors are captured as option tokens");
        {
            ApplicationState state;
            parse(state, "in A cc 1..10 ch 1..4 out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->filters.size(), 2);
            expect(route->filters[0].command_ == CONTROL_CHANGE);
            expect(route->filters[0].opts_[0] == "1..10");
            expect(route->filters[1].command_ == CHANNEL);
            expect(route->filters[1].opts_[0] == "1..4");
        }

        beginTest("Commands before an input start no route");
        {
            ApplicationState state;
            parse(state, "transp 12 out B cc");
            expectEquals(state.getRoutes().size(), 0);
        }

        beginTest("Decimal and hexadecimal number parsing");
        {
            ApplicationState dec;
            expectEquals(dec.asDecOrHexIntValue("10"), 10);

            ApplicationState hex;
            parse(hex, "hex");
            expectEquals(hex.asDecOrHexIntValue("10"), 16);

            // explicit suffixes override the current base
            expectEquals(dec.asDecOrHexIntValue("10H"), 16);
            expectEquals(dec.asDecOrHexIntValue("7FH"), 127);
            expectEquals(hex.asDecOrHexIntValue("10M"), 10);
        }

        beginTest("Note name parsing");
        {
            ApplicationState state;
            expectEquals((int)state.asNoteNumber("C3"),  60);
            expectEquals((int)state.asNoteNumber("C#3"), 61);
            expectEquals((int)state.asNoteNumber("Db3"), 61);
            expectEquals((int)state.asNoteNumber("C-2"), 0);
            expectEquals((int)state.asNoteNumber("G8"),  127);
            expectEquals((int)state.asNoteNumber("64"),  64);   // plain numbers still work
        }

        beginTest("Octave for middle C shifts the note names");
        {
            ApplicationState state;
            parse(state, "omc 4");
            expectEquals((int)state.asNoteNumber("C4"), 60);
        }

        beginTest("Text MIDI codec round-trips through messageToText/parseTextMidi");
        {
            ApplicationState state;
            // notes are rendered as numbers so the round-trip is base-independent
            parse(state, "nn");

            auto roundTrip = [&state] (const MidiMessage& msg)
            {
                const String text = state.messageToText(msg);
                StringArray tokens;
                tokens.addTokens(text, " ", "");
                tokens.removeEmptyStrings(true);
                Array<MidiMessage> parsed;
                state.parseTextMidi(tokens, parsed);
                return parsed;
            };

            const MidiMessage cases[] = {
                MidiMessage::noteOn(1, 60, (uint8)100),
                MidiMessage::noteOff(5, 72, (uint8)0),
                MidiMessage::controllerEvent(3, 74, 42),
                MidiMessage::programChange(2, 10),
                MidiMessage::channelPressureChange(7, 64),
                MidiMessage::aftertouchChange(4, 60, 33),
                MidiMessage::pitchWheel(9, 12000),
                MidiMessage::midiClock(),
                MidiMessage::midiStart(),
                MidiMessage::midiStop(),
                MidiMessage::midiContinue(),
                MidiMessage::songPositionPointer(2000),
            };

            for (const auto& msg : cases)
            {
                auto parsed = roundTrip(msg);
                expectEquals(parsed.size(), 1);
                if (parsed.size() == 1)
                {
                    // compare the raw bytes for an exact match
                    expect(parsed[0].getRawDataSize() == msg.getRawDataSize());
                    expect(memcmp(parsed[0].getRawData(), msg.getRawData(),
                                  (size_t)msg.getRawDataSize()) == 0);
                }
            }
        }

        beginTest("Text MIDI codec round-trips System Exclusive");
        {
            ApplicationState state;
            const uint8 sysexData[] = { 0x43, 0x12, 0x00, 0x7f };
            const MidiMessage syx = MidiMessage::createSysExMessage(sysexData, numElementsInArray(sysexData));

            const String text = state.messageToText(syx);
            StringArray tokens;
            tokens.addTokens(text, " ", "");
            tokens.removeEmptyStrings(true);
            Array<MidiMessage> parsed;
            state.parseTextMidi(tokens, parsed);

            expectEquals(parsed.size(), 1);
            if (parsed.size() == 1)
            {
                expect(parsed[0].isSysEx());
                expect(parsed[0].getRawDataSize() == syx.getRawDataSize());
                expect(memcmp(parsed[0].getRawData(), syx.getRawData(),
                              (size_t)syx.getRawDataSize()) == 0);
            }
        }

        beginTest("A long realistic routing spans config, filters, transforms, conversion and monitoring");
        {
            // A keyboard into a synth: pass only channel 1 (dropping clock and
            // aftertouch), transpose up an octave, quantise to C major, shape the
            // velocity, move everything to channel 5, halve the pitch bend, trim
            // and remap the mod wheel -- with monitoring switched on for good measure.
            ApplicationState state;
            parse(state, "dec omc 3 in Keyboard ch 1 not clock not cp "
                         "transp 12 scale C major velclip 40 120 veladd 10 chset 5 pbscale 0.5 ccadd 1 -5 "
                         "convert cc 1 cc 11 mon nn ts src out Synth");

            // the parser sorted each command into its own stage bucket; the config
            // and monitoring words (dec, omc, mon, nn, ts, src) stay global
            expectEquals(state.getRoutes().size(), 1);
            Route& route = *state.getRoutes().getFirst();
            expectEquals(route.filters.size(), 3);       // ch, not clock, not cp
            expectEquals(route.transforms.size(), 7);    // transp scale velclip veladd chset pbscale ccadd
            expectEquals(route.converters.size(), 1);    // convert cc 1 cc 11
            expect(route.mpeOps.isEmpty());
            RouteInput& input = *route.inputs.getFirst();

            // runs a message through the whole transformation pipeline the way
            // routeMessage does: filters -> transforms -> MPE -> converters
            auto run = [&state, &route, &input](const MidiMessage& msg)
            {
                Array<MidiMessage> out;
                if (! state.passesFilters(route, msg))
                {
                    return out;
                }
                for (auto& t : state.applyTransforms(route, input, msg))
                {
                    Array<MidiMessage> afterMpe;
                    if (route.mpeOps.isEmpty()) afterMpe.add(t);
                    else                        state.processMpe(route, input, t, afterMpe);
                    for (auto& m : afterMpe)
                    {
                        if (route.converters.isEmpty()) out.add(m);
                        else                            state.processConverters(route, input, m, out);
                    }
                }
                return out;
            };

            // a played C#4 climbs an octave, snaps to C in the scale, keeps its
            // (in-range, boosted) velocity, and lands on channel 5
            auto a = run(MidiMessage::noteOn(1, 61, (uint8)100));
            expectEquals(a.size(), 1);
            expect(a[0].isNoteOn());
            expectEquals(a[0].getChannel(), 5);
            expectEquals(a[0].getNoteNumber(), 72);           // 61 +12 -> 73, snapped down to 72
            expectEquals((int)a[0].getVelocity(), 110);       // 100 (within 40-120) +10

            // a soft note has its velocity clamped up to the floor before the boost
            auto b = run(MidiMessage::noteOn(1, 60, (uint8)20));
            expectEquals(b.size(), 1);
            expectEquals(b[0].getNoteNumber(), 72);           // 60 +12 -> 72, already in scale
            expectEquals((int)b[0].getVelocity(), 50);        // 20 -> clamped 40 -> +10

            // the mod wheel is trimmed by ccadd and converted to CC 11 on channel 5
            auto c = run(MidiMessage::controllerEvent(1, 1, 90));
            expectEquals(c.size(), 1);
            expect(c[0].isController());
            expectEquals(c[0].getChannel(), 5);
            expectEquals(c[0].getControllerNumber(), 11);     // convert cc 1 -> cc 11
            expectEquals(c[0].getControllerValue(), 85);      // 90 - 5 (ccadd), 7-bit unchanged by convert

            // pitch bend is halved around centre and moved to channel 5
            auto d = run(MidiMessage::pitchWheel(1, 12288));
            expectEquals(d.size(), 1);
            expect(d[0].isPitchWheel());
            expectEquals(d[0].getChannel(), 5);
            expectEquals(d[0].getPitchWheelValue(), 10240);   // 8192 + (12288-8192)/2

            // clock and channel pressure are blacklisted, and a wrong channel is out
            expect(run(MidiMessage::midiClock()).isEmpty());
            expect(run(MidiMessage::channelPressureChange(1, 50)).isEmpty());
            expect(run(MidiMessage::noteOn(2, 60, (uint8)100)).isEmpty());
        }
    }
};

static ParsingTests parsingTests;
