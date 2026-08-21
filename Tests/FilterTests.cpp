/*
 * This file is part of RouteMIDI.
 * Copyright (c) 2017-2026 Uwyn LLC.  https://www.uwyn.com
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

class FilterTests : public UnitTest
{
public:
    FilterTests() : UnitTest("Filters", "Filters") {}

    void runTest() override
    {
        ApplicationState state;
        // per-input state for the stateful filters (cc14range)
        RouteInput input;

        beginTest("Type filters match the right messages");
        {
            const MidiMessage note = MidiMessage::noteOn(1, 60, (uint8)100);
            const MidiMessage cc   = MidiMessage::controllerEvent(1, 7, 100);
            const MidiMessage clk  = MidiMessage::midiClock();

            expect(  makeCommand(NOTE).matches(state, note, 0));
            expect(!makeCommand(NOTE).matches(state, cc, 0));

            expect(  makeCommand(VOICE).matches(state, note, 0));
            expect(  makeCommand(VOICE).matches(state, cc, 0));
            expect(!makeCommand(VOICE).matches(state, clk, 0));

            expect(  makeCommand(CONTROL_CHANGE).matches(state, cc, 0));
            expect(  makeCommand(CONTROL_CHANGE, {"7"}).matches(state, cc, 0));
            expect(!makeCommand(CONTROL_CHANGE, {"8"}).matches(state, cc, 0));

            expect(  makeCommand(NOTE_ON, {"60"}).matches(state, note, 0));
            expect(!makeCommand(NOTE_ON, {"61"}).matches(state, note, 0));

            expect(  makeCommand(CLOCK).matches(state, clk, 0));
            expect(!makeCommand(CLOCK).matches(state, note, 0));
        }

        beginTest("Channel-aware matching");
        {
            const MidiMessage ch1 = MidiMessage::noteOn(1, 60, (uint8)100);
            const MidiMessage ch2 = MidiMessage::noteOn(2, 60, (uint8)100);
            expect(  makeCommand(NOTE).matches(state, ch1, 1));
            expect(!makeCommand(NOTE).matches(state, ch2, 1));
            // 0 = any channel
            expect(  makeCommand(NOTE).matches(state, ch2, 0));
        }

        beginTest("No filters passes everything");
        {
            Route route;
            expect(state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(state.passesFilters(route, input, MidiMessage::midiClock()));
        }

        beginTest("Whitelist: only listed types pass");
        {
            Route route;
            route.filters.add(makeCommand(NOTE));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(!state.passesFilters(route, input, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("Multiple positive filters are OR-combined");
        {
            Route route;
            route.filters.add(makeCommand(NOTE));
            route.filters.add(makeCommand(CONTROL_CHANGE));
            expect(state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(state.passesFilters(route, input, MidiMessage::controllerEvent(1, 7, 100)));
            expect(!state.passesFilters(route, input, MidiMessage::midiClock()));
        }

        beginTest("'not' blacklists matching messages");
        {
            Route route;
            auto blockClock = makeCommand(CLOCK);
            blockClock.negate_ = true;
            route.filters.add(blockClock);
            expect(!state.passesFilters(route, input, MidiMessage::midiClock()));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
        }

        beginTest("Channel filter restricts the route");
        {
            Route route;
            route.filters.add(makeCommand(CHANNEL, {"1"}));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(2, 60, (uint8)100)));
        }

        beginTest("Channel filter provides context for type filters");
        {
            Route route;
            route.filters.add(makeCommand(CHANNEL, {"1"}));
            route.filters.add(makeCommand(NOTE));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(2, 60, (uint8)100)));
            expect(!state.passesFilters(route, input, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("Whitelist and blacklist combine");
        {
            Route route;
            route.filters.add(makeCommand(VOICE));
            auto blockCC = makeCommand(CONTROL_CHANGE);
            blockCC.negate_ = true;
            route.filters.add(blockCC);
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(!state.passesFilters(route, input, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("noterange passes only notes within the note range (key split)");
        {
            Route route;
            route.filters.add(makeCommand(NOTE_RANGE, {"60", "72"}));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 72, (uint8)100)));
            expect(  state.passesFilters(route, input, MidiMessage::noteOff(1, 65, (uint8)0)));
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(1, 59, (uint8)100)));
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(1, 73, (uint8)100)));
            expect(!state.passesFilters(route, input, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("velrange filters note-ons by velocity, always passing note-offs");
        {
            Route route;
            route.filters.add(makeCommand(VELOCITY_RANGE, {"64", "127"}));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)64)));
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)40)));
            // note-offs always pass so a velocity split can't leave notes stuck
            expect(  state.passesFilters(route, input, MidiMessage::noteOff(1, 60, (uint8)0)));
            expect(!state.passesFilters(route, input, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("ccrange passes a controller only within a value range");
        {
            Route route;
            route.filters.add(makeCommand(CONTROL_CHANGE_RANGE, {"1", "64", "127"}));
            // in range
            expect(  state.passesFilters(route, input, MidiMessage::controllerEvent(1, 1, 100)));
            // lower bound
            expect(  state.passesFilters(route, input, MidiMessage::controllerEvent(1, 1, 64)));
            // upper bound
            expect(  state.passesFilters(route, input, MidiMessage::controllerEvent(1, 1, 127)));
            // below range
            expect(!state.passesFilters(route, input, MidiMessage::controllerEvent(1, 1, 63)));
            // a different CC
            expect(!state.passesFilters(route, input, MidiMessage::controllerEvent(1, 7, 100)));
            // a non-CC
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
        }

        beginTest("cc14range passes a 14-bit CC pair only within a value range");
        {
            // controller 7 (MSB) pairs with 39 (LSB); the range is 0-16383
            Route route;
            // fresh MSB memory for this test
            RouteInput pairing;
            route.filters.add(makeCommand(CONTROL_CHANGE_14BIT_RANGE, {"7", "8000", "9000"}));

            // an in-range pair: both halves are judged by the MSB's 128-value
            // block, so they always travel together
            // block 8320-8447
            expect(  state.passesFilters(route, pairing, MidiMessage::controllerEvent(1, 7, 65)));
            expect(  state.passesFilters(route, pairing, MidiMessage::controllerEvent(1, 39, 64)));

            // an out-of-range pair: both halves are blocked
            // block 2560-2687
            expect(!state.passesFilters(route, pairing, MidiMessage::controllerEvent(1, 7, 20)));
            expect(!state.passesFilters(route, pairing, MidiMessage::controllerEvent(1, 39, 0)));

            // at a window edge inside a block the whole pair still passes: the
            // edge acts at MSB resolution rather than splitting the pair
            // block 8960-9087
            expect(  state.passesFilters(route, pairing, MidiMessage::controllerEvent(1, 7, 70)));
            // follows its MSB
            expect(  state.passesFilters(route, pairing, MidiMessage::controllerEvent(1, 39, 41)));

            // other controllers and non-CC messages don't match
            expect(!state.passesFilters(route, pairing, MidiMessage::controllerEvent(1, 8, 65)));
            expect(!state.passesFilters(route, pairing, MidiMessage::controllerEvent(1, 74, 65)));
            expect(!state.passesFilters(route, pairing, MidiMessage::noteOn(1, 60, (uint8)100)));

            // the MSB memory is per channel: channel 2 still assumes MSB 0
            // block 0-127
            expect(!state.passesFilters(route, pairing, MidiMessage::controllerEvent(2, 39, 64)));

            // a window starting inside the first block never orphans the LSB:
            // both halves of a low pair pass together
            Route low;
            RouteInput lowPairing;
            low.filters.add(makeCommand(CONTROL_CHANGE_14BIT_RANGE, {"7", "100", "8191"}));
            expect(  state.passesFilters(low, lowPairing, MidiMessage::controllerEvent(1, 7, 0)));
            expect(  state.passesFilters(low, lowPairing, MidiMessage::controllerEvent(1, 39, 120)));
        }

        beginTest("System Real-Time filters match their exact messages");
        {
            const MidiMessage clock = MidiMessage::midiClock();
            const MidiMessage start = MidiMessage::midiStart();
            const MidiMessage stop  = MidiMessage::midiStop();
            const MidiMessage cont  = MidiMessage::midiContinue();
            const MidiMessage sense(0xfe);
            const MidiMessage reset(0xff);

            expect(  makeCommand(START).matches(state, start, 0));
            expect(!makeCommand(START).matches(state, stop, 0));
            expect(  makeCommand(STOP).matches(state, stop, 0));
            expect(!makeCommand(STOP).matches(state, start, 0));
            expect(  makeCommand(CONTINUE).matches(state, cont, 0));
            expect(!makeCommand(CONTINUE).matches(state, clock, 0));
            expect(  makeCommand(ACTIVE_SENSING).matches(state, sense, 0));
            expect(!makeCommand(ACTIVE_SENSING).matches(state, reset, 0));
            expect(  makeCommand(RESET).matches(state, reset, 0));
            expect(!makeCommand(RESET).matches(state, sense, 0));

            // the composite passes the whole family, nothing else
            auto sr = makeCommand(SYSTEM_REALTIME);
            expect(  sr.matches(state, clock, 0));
            expect(  sr.matches(state, start, 0));
            expect(  sr.matches(state, stop, 0));
            expect(  sr.matches(state, cont, 0));
            expect(  sr.matches(state, sense, 0));
            expect(  sr.matches(state, reset, 0));
            expect(!sr.matches(state, MidiMessage::songPositionPointer(0), 0));
            expect(!sr.matches(state, MidiMessage::noteOn(1, 60, (uint8)100), 0));
        }

        beginTest("System Common filters match their exact messages");
        {
            const MidiMessage tc  = MidiMessage::quarterFrame(3, 5);
            const MidiMessage spp = MidiMessage::songPositionPointer(1234);
            const MidiMessage ss(0xf3, 42);
            const MidiMessage tun(0xf6);
            const uint8 syxData[] = { 0x7d, 0x11, 0x22 };
            const MidiMessage syx = MidiMessage::createSysExMessage(syxData, 3);

            expect(  makeCommand(TIME_CODE).matches(state, tc, 0));
            expect(!makeCommand(TIME_CODE).matches(state, spp, 0));
            expect(  makeCommand(SONG_POSITION).matches(state, spp, 0));
            expect(!makeCommand(SONG_POSITION).matches(state, tc, 0));
            expect(  makeCommand(SONG_SELECT).matches(state, ss, 0));
            expect(!makeCommand(SONG_SELECT).matches(state, tun, 0));
            expect(  makeCommand(TUNE_REQUEST).matches(state, tun, 0));
            expect(!makeCommand(TUNE_REQUEST).matches(state, ss, 0));

            // the composite passes the whole family, nothing else
            auto sc = makeCommand(SYSTEM_COMMON);
            expect(  sc.matches(state, syx, 0));
            expect(  sc.matches(state, tc, 0));
            expect(  sc.matches(state, spp, 0));
            expect(  sc.matches(state, ss, 0));
            expect(  sc.matches(state, tun, 0));
            expect(!sc.matches(state, MidiMessage::midiClock(), 0));
            expect(!sc.matches(state, MidiMessage::noteOn(1, 60, (uint8)100), 0));
        }

        beginTest("inscale passes only notes that belong to the key");
        {
            Route route;
            route.filters.add(makeCommand(IN_SCALE, {"C", "major"}));
            // C
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            // D
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 62, (uint8)100)));
            // C#
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(1, 61, (uint8)100)));
            // F#
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(1, 66, (uint8)100)));
            // in-key off
            expect(  state.passesFilters(route, input, MidiMessage::noteOff(1, 60, (uint8)0)));
            // out-of-key off
            expect(!state.passesFilters(route, input, MidiMessage::noteOff(1, 61, (uint8)0)));
            // non-note
            expect(!state.passesFilters(route, input, MidiMessage::controllerEvent(1, 7, 100)));

            // the root and scale matter: F# belongs to G major
            Route g;
            g.filters.add(makeCommand(IN_SCALE, {"G", "major"}));
            // F#
            expect(  state.passesFilters(g, input, MidiMessage::noteOn(1, 66, (uint8)100)));
            // F natural
            expect(!state.passesFilters(g, input, MidiMessage::noteOn(1, 65, (uint8)100)));
        }

        beginTest("'not inscale' passes only notes outside the key");
        {
            Route route;
            ApplicationCommand cmd = makeCommand(IN_SCALE, {"C", "major"});
            cmd.negate_ = true;
            route.filters.add(cmd);
            // C in key, blocked
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            // C# out of key, passes
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 61, (uint8)100)));
        }

        beginTest("cc14 matches an MSB/LSB controller pair");
        {
            // without a number, any 14-bit-capable controller (MSB 0-31) passes
            expect(  makeCommand(CONTROL_CHANGE_14BIT).matches(state, MidiMessage::controllerEvent(1, 10, 64), 0));
            expect(  makeCommand(CONTROL_CHANGE_14BIT).matches(state, MidiMessage::controllerEvent(1, 42, 64), 0));
            expect(!makeCommand(CONTROL_CHANGE_14BIT).matches(state, MidiMessage::controllerEvent(1, 64, 64), 0));

            // with a number, only that MSB controller and its +32 LSB pass
            auto cc14 = makeCommand(CONTROL_CHANGE_14BIT, {"10"});
            expect(  cc14.matches(state, MidiMessage::controllerEvent(1, 10, 64), 0));
            expect(  cc14.matches(state, MidiMessage::controllerEvent(1, 42, 64), 0));
            expect(!cc14.matches(state, MidiMessage::controllerEvent(1, 11, 64), 0));
            expect(!cc14.matches(state, MidiMessage::controllerEvent(1, 7, 64), 0));
        }

        beginTest("Number selectors accept a '..' range");
        {
            // cc range: 1..10
            auto ccRange = makeCommand(CONTROL_CHANGE, {"1..10"});
            expect(  ccRange.matches(state, MidiMessage::controllerEvent(1, 1, 0), 0));
            expect(  ccRange.matches(state, MidiMessage::controllerEvent(1, 10, 0), 0));
            expect(  ccRange.matches(state, MidiMessage::controllerEvent(1, 5, 0), 0));
            expect(!ccRange.matches(state, MidiMessage::controllerEvent(1, 11, 0), 0));
            expect(!ccRange.matches(state, MidiMessage::controllerEvent(1, 0, 0), 0));

            // single value still works
            auto ccOne = makeCommand(CONTROL_CHANGE, {"7"});
            expect(  ccOne.matches(state, MidiMessage::controllerEvent(1, 7, 0), 0));
            expect(!ccOne.matches(state, MidiMessage::controllerEvent(1, 8, 0), 0));

            // note range, by name (note the '-' in C-2 must not confuse the range)
            auto onRange = makeCommand(NOTE_ON, {"C3..C4"});
            // C3
            expect(  onRange.matches(state, MidiMessage::noteOn(1, 60, (uint8)100), 0));
            // C4
            expect(  onRange.matches(state, MidiMessage::noteOn(1, 72, (uint8)100), 0));
            expect(!onRange.matches(state, MidiMessage::noteOn(1, 59, (uint8)100), 0));
            auto onLowRange = makeCommand(NOTE_ON, {"C-2..C-1"});
            // C-2
            expect(  onLowRange.matches(state, MidiMessage::noteOn(1, 0, (uint8)100), 0));
            // C-1
            expect(  onLowRange.matches(state, MidiMessage::noteOn(1, 12, (uint8)100), 0));
            expect(!onLowRange.matches(state, MidiMessage::noteOn(1, 13, (uint8)100), 0));

            // pc range
            auto pcRange = makeCommand(PROGRAM_CHANGE, {"0..7"});
            expect(  pcRange.matches(state, MidiMessage::programChange(1, 0), 0));
            expect(  pcRange.matches(state, MidiMessage::programChange(1, 7), 0));
            expect(!pcRange.matches(state, MidiMessage::programChange(1, 8), 0));

            // cc14 range selects MSB controllers and their LSB partners
            auto cc14Range = makeCommand(CONTROL_CHANGE_14BIT, {"0..3"});
            // MSB 2
            expect(  cc14Range.matches(state, MidiMessage::controllerEvent(1, 2, 0), 0));
            // LSB of 2
            expect(  cc14Range.matches(state, MidiMessage::controllerEvent(1, 34, 0), 0));
            // MSB 4 out of range
            expect(!cc14Range.matches(state, MidiMessage::controllerEvent(1, 4, 0), 0));
        }

        beginTest("Channel filter accepts a '..' range");
        {
            Route route;
            route.filters.add(makeCommand(CHANNEL, {"1..4"}));
            route.filters.add(makeCommand(NOTE));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(  state.passesFilters(route, input, MidiMessage::noteOn(4, 60, (uint8)100)));
            expect(!state.passesFilters(route, input, MidiMessage::noteOn(5, 60, (uint8)100)));

            // a channel range with no type filter gates on its own
            Route chOnly;
            chOnly.filters.add(makeCommand(CHANNEL, {"10..12"}));
            expect(  state.passesFilters(chOnly, input, MidiMessage::controllerEvent(11, 7, 0)));
            expect(!state.passesFilters(chOnly, input, MidiMessage::controllerEvent(9, 7, 0)));
        }

        beginTest("nrpn and rpn match their constituent controllers");
        {
            auto nrpn = makeCommand(NRPN);
            auto rpn  = makeCommand(RPN);
            // NRPN parameter selectors (98/99) plus the data entry CCs (6/38)
            expect(  nrpn.matches(state, MidiMessage::controllerEvent(1, 99, 1), 0));
            expect(  nrpn.matches(state, MidiMessage::controllerEvent(1, 98, 2), 0));
            expect(  nrpn.matches(state, MidiMessage::controllerEvent(1, 6, 64), 0));
            expect(  nrpn.matches(state, MidiMessage::controllerEvent(1, 38, 0), 0));
            expect(!nrpn.matches(state, MidiMessage::controllerEvent(1, 100, 1), 0));
            // RPN parameter selectors (100/101) plus the data entry CCs (6/38)
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 101, 0), 0));
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 100, 0), 0));
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 6, 2), 0));
            expect(!rpn.matches(state, MidiMessage::controllerEvent(1, 98, 1), 0));
            // both pick up the shared data-entry controllers
            expect(  nrpn.matches(state, MidiMessage::controllerEvent(1, 38, 0), 0));
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 38, 0), 0));
            // and the shared data increment/decrement controllers
            expect(  nrpn.matches(state, MidiMessage::controllerEvent(1, 96, 1), 0));
            expect(  nrpn.matches(state, MidiMessage::controllerEvent(1, 97, 1), 0));
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 96, 1), 0));
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 97, 1), 0));
        }

        beginTest("nrpn/rpn filter by a specific parameter number");
        {
            // nrpn 1000 (MSB 7, LSB 104): pass only that parameter's select and
            // its shared data-entry controllers, tracking the selection per input
            Route route;
            RouteInput sel;
            route.filters.add(makeCommand(NRPN, {"1000"}));

            // select MSB
            expect(  state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 99, 7)));
            // select LSB
            expect(  state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 98, 104)));
            // data MSB
            expect(  state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 6, 50)));
            // data LSB
            expect(  state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 38, 20)));
            // data increment
            expect(  state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 96, 1)));
            // data decrement
            expect(  state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 97, 1)));

            // selecting a neighbouring parameter blocks its select and data
            // other MSB
            expect(!state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 99, 5)));
            // completes 645, not 1000
            expect(!state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 98, 5)));
            // its data blocked
            expect(!state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 6, 99)));

            // an RPN with the same number is different traffic and stays blocked
            // RPN MSB
            expect(!state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 101, 7)));
            // RPN 1000, but RPN
            expect(!state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 100, 104)));
            // RPN data, not NRPN
            expect(!state.passesFilters(route, sel, MidiMessage::controllerEvent(1, 6, 50)));

            // rpn 0, the pitch-bend-sensitivity parameter
            Route r;
            RouteInput rs;
            r.filters.add(makeCommand(RPN, {"0"}));
            // RPN 0 MSB
            expect(  state.passesFilters(r, rs, MidiMessage::controllerEvent(1, 101, 0)));
            // RPN 0 LSB
            expect(  state.passesFilters(r, rs, MidiMessage::controllerEvent(1, 100, 0)));
            // its data passes
            expect(  state.passesFilters(r, rs, MidiMessage::controllerEvent(1, 6, 12)));
            // NRPN 0 is a different selection, so its data is blocked by rpn 0
            expect(!state.passesFilters(r, rs, MidiMessage::controllerEvent(1, 99, 0)));
            expect(!state.passesFilters(r, rs, MidiMessage::controllerEvent(1, 98, 0)));
            expect(!state.passesFilters(r, rs, MidiMessage::controllerEvent(1, 6, 12)));

            // the selection is per channel: channel 2 has none of channel 1's
            Route c;
            RouteInput cs;
            c.filters.add(makeCommand(NRPN, {"1000"}));
            expect(  state.passesFilters(c, cs, MidiMessage::controllerEvent(1, 99, 7)));
            expect(  state.passesFilters(c, cs, MidiMessage::controllerEvent(1, 98, 104)));
            // channel 1 data passes
            expect(  state.passesFilters(c, cs, MidiMessage::controllerEvent(1, 6, 50)));
            // channel 2 has no selection
            expect(!state.passesFilters(c, cs, MidiMessage::controllerEvent(2, 6, 50)));
        }

        beginTest("the (N)RPN null passes as deselect framing and ends the selection");
        {
            // a complete RPN transmission ends with the 127/127 null (the same
            // framing conversion::emitRpn generates); a parameter filter passes
            // the null so the downstream deselection stays intact
            Route route;
            RouteInput in;
            route.filters.add(makeCommand(RPN, {"0"}));

            // select RPN 0
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 101, 0)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 100, 0)));
            // its data
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 12)));
            // closing null MSB
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 101, 127)));
            // closing null LSB
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 100, 127)));

            // the completed null ended the selection: no parameter's data matches
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 12)));
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 38, 1)));

            // the null of the other select family stays blocked (rpn vs nrpn)
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 127)));
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 127)));

            // a parameter whose select bytes include 127 is still matchable:
            // nrpn 127 is MSB 0 with LSB 127, which is not the completed null
            Route edge;
            RouteInput es;
            edge.filters.add(makeCommand(NRPN, {"127"}));
            // select MSB
            expect(  state.passesFilters(edge, es, MidiMessage::controllerEvent(1, 99, 0)));
            // select LSB
            expect(  state.passesFilters(edge, es, MidiMessage::controllerEvent(1, 98, 127)));
            // its data
            expect(  state.passesFilters(edge, es, MidiMessage::controllerEvent(1, 6, 30)));
        }

        beginTest("the (N)RPN selection is tracked before any filter exists");
        {
            // a route can start without filters and gain an "nrpn N" filter over
            // MCP later; the selection made in the meantime must already be known
            Route route;
            RouteInput in;
            // no filters: passes
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 7)));
            // and is tracked
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 104)));

            route.filters.add(makeCommand(NRPN, {"1000"}));
            // data matches at once
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 50)));
        }

        beginTest("several nrpn parameters whitelist together");
        {
            // filters OR-combine, so nrpn 1000 + nrpn 2000 passes both parameters
            Route route;
            RouteInput in;
            // MSB 7,  LSB 104
            route.filters.add(makeCommand(NRPN, {"1000"}));
            // MSB 15, LSB 80
            route.filters.add(makeCommand(NRPN, {"2000"}));

            // NRPN 1000
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 7)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 104)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 10)));
            // NRPN 2000
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 15)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 80)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 20)));
            // NRPN 3000, neither
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 23)));
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 56)));
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 30)));
        }

        beginTest("nrpn and rpn parameter filters share CC 6/38 by selection");
        {
            // nrpn 1000 + rpn 0: the shared data entry follows whichever was
            // selected last, even when the two are interleaved
            Route route;
            RouteInput in;
            route.filters.add(makeCommand(NRPN, {"1000"}));
            route.filters.add(makeCommand(RPN, {"0"}));

            // NRPN 1000
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 7)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 104)));
            // NRPN 1000 data
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 11)));
            // switch to RPN 0
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 101, 0)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 100, 0)));
            // now RPN 0 data
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 12)));

            // interleave a fresh NRPN 1000 select, then an RPN 0 select before data:
            // the data belongs to RPN 0, the selection that stuck
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 7)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 104)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 101, 0)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 100, 0)));
            // RPN 0 data
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 9)));

            // a parameter neither filter wants is blocked
            // NRPN 2000
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 15)));
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 80)));
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 5)));
        }

        beginTest("nrpn parameter filter coexists with plain CC and cc14 without cross-talk");
        {
            // nrpn 1000 + cc 7 + cc14 10: each passes its own traffic, and the
            // unrelated CC / 14-bit messages don't corrupt the (N)RPN selection
            Route route;
            RouteInput in;
            route.filters.add(makeCommand(NRPN, {"1000"}));
            route.filters.add(makeCommand(CONTROL_CHANGE, {"7"}));
            // CC 10 + CC 42
            route.filters.add(makeCommand(CONTROL_CHANGE_14BIT, {"10"}));

            // plain CC 7
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 7, 64)));
            // CC 8 blocked
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 8, 64)));
            // cc14 10 MSB
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 10, 100)));
            // cc14 10 LSB
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 42, 20)));

            // select NRPN 1000, then push CC 7 and a cc14 10 completion between the
            // select and its data; the data still passes as NRPN 1000
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 7)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 104)));
            // unrelated CC
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 7, 70)));
            // unrelated cc14
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 10, 90)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 42, 30)));
            // NRPN 1000 data
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 15)));
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 38, 25)));

            // a different NRPN's data is still blocked despite the coexisting filters
            // select NRPN 2000
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 15)));
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 80)));
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 5)));
        }

        beginTest("a cc14 filter on CC 6 claims the shared data entry unconditionally");
        {
            // cc14 6 uses CC 6/38, which overlap the (N)RPN data entry; being a
            // positive filter it passes them whatever parameter is selected, so
            // combined with nrpn 1000 the data always passes (whitelist OR)
            Route route;
            RouteInput in;
            route.filters.add(makeCommand(NRPN, {"1000"}));
            route.filters.add(makeCommand(CONTROL_CHANGE_14BIT, {"6"}));

            // select NRPN 2000
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 15)));
            expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 80)));
            // passes via cc14 6
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 30)));
            // passes via cc14 6
            expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 38, 40)));
        }

        beginTest("negated nrpn/rpn parameter filters block just that parameter");
        {
            // "not nrpn 1000" alone blocks only NRPN 1000, passing everything else
            {
                Route route;
                RouteInput in;
                ApplicationCommand notN = makeCommand(NRPN, {"1000"});
                notN.negate_ = true;
                route.filters.add(notN);
                // NRPN 1000 blocked
                expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 7)));
                expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 104)));
                expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 10)));
                // NRPN 2000 passes
                expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 15)));
                expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 80)));
                expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 20)));
                // notes pass
                expect(  state.passesFilters(route, in, MidiMessage::noteOn(1, 60, (uint8)100)));
            }

            // "nrpn" plus "not nrpn 1000" passes every NRPN parameter except 1000
            {
                Route route;
                RouteInput in;
                route.filters.add(makeCommand(NRPN));
                ApplicationCommand notN = makeCommand(NRPN, {"1000"});
                notN.negate_ = true;
                route.filters.add(notN);
                // NRPN 2000 passes
                expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 15)));
                expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 80)));
                expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 20)));
                // NRPN 1000 blocked
                expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 7)));
                expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 98, 104)));
                expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 10)));
                // positive nrpn excludes notes
                expect(!state.passesFilters(route, in, MidiMessage::noteOn(1, 60, (uint8)100)));
            }

            // "not rpn 0" blocks the pitch-bend-sensitivity RPN, passes NRPN and notes
            {
                Route route;
                RouteInput in;
                ApplicationCommand notR = makeCommand(RPN, {"0"});
                notR.negate_ = true;
                route.filters.add(notR);
                // RPN 0 blocked
                expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 101, 0)));
                expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 100, 0)));
                expect(!state.passesFilters(route, in, MidiMessage::controllerEvent(1, 6, 12)));
                // NRPN traffic passes
                expect(  state.passesFilters(route, in, MidiMessage::controllerEvent(1, 99, 7)));
                expect(  state.passesFilters(route, in, MidiMessage::noteOn(1, 60, (uint8)100)));
            }
        }
    }
};

static FilterTests filterTests;
