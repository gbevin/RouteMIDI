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

class FilterTests : public UnitTest
{
public:
    FilterTests() : UnitTest("Filters", "Filters") {}

    void runTest() override
    {
        ApplicationState state;

        beginTest("Type filters match the right messages");
        {
            const MidiMessage note = MidiMessage::noteOn(1, 60, (uint8)100);
            const MidiMessage cc   = MidiMessage::controllerEvent(1, 7, 100);
            const MidiMessage clk  = MidiMessage::midiClock();

            expect(  makeCommand(NOTE).matches(state, note, 0));
            expect(! makeCommand(NOTE).matches(state, cc, 0));

            expect(  makeCommand(VOICE).matches(state, note, 0));
            expect(  makeCommand(VOICE).matches(state, cc, 0));
            expect(! makeCommand(VOICE).matches(state, clk, 0));

            expect(  makeCommand(CONTROL_CHANGE).matches(state, cc, 0));
            expect(  makeCommand(CONTROL_CHANGE, {"7"}).matches(state, cc, 0));
            expect(! makeCommand(CONTROL_CHANGE, {"8"}).matches(state, cc, 0));

            expect(  makeCommand(NOTE_ON, {"60"}).matches(state, note, 0));
            expect(! makeCommand(NOTE_ON, {"61"}).matches(state, note, 0));

            expect(  makeCommand(CLOCK).matches(state, clk, 0));
            expect(! makeCommand(CLOCK).matches(state, note, 0));
        }

        beginTest("Channel-aware matching");
        {
            const MidiMessage ch1 = MidiMessage::noteOn(1, 60, (uint8)100);
            const MidiMessage ch2 = MidiMessage::noteOn(2, 60, (uint8)100);
            expect(  makeCommand(NOTE).matches(state, ch1, 1));
            expect(! makeCommand(NOTE).matches(state, ch2, 1));
            expect(  makeCommand(NOTE).matches(state, ch2, 0));  // 0 = any channel
        }

        beginTest("No filters passes everything");
        {
            Route route;
            expect(state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(state.passesFilters(route, MidiMessage::midiClock()));
        }

        beginTest("Whitelist: only listed types pass");
        {
            Route route;
            route.filters.add(makeCommand(NOTE));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(! state.passesFilters(route, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("Multiple positive filters are OR-combined");
        {
            Route route;
            route.filters.add(makeCommand(NOTE));
            route.filters.add(makeCommand(CONTROL_CHANGE));
            expect(state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(state.passesFilters(route, MidiMessage::controllerEvent(1, 7, 100)));
            expect(! state.passesFilters(route, MidiMessage::midiClock()));
        }

        beginTest("'not' blacklists matching messages");
        {
            Route route;
            auto blockClock = makeCommand(CLOCK);
            blockClock.negate_ = true;
            route.filters.add(blockClock);
            expect(! state.passesFilters(route, MidiMessage::midiClock()));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
        }

        beginTest("Channel filter restricts the route");
        {
            Route route;
            route.filters.add(makeCommand(CHANNEL, {"1"}));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(! state.passesFilters(route, MidiMessage::noteOn(2, 60, (uint8)100)));
        }

        beginTest("Channel filter provides context for type filters");
        {
            Route route;
            route.filters.add(makeCommand(CHANNEL, {"1"}));
            route.filters.add(makeCommand(NOTE));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(! state.passesFilters(route, MidiMessage::noteOn(2, 60, (uint8)100)));
            expect(! state.passesFilters(route, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("Whitelist and blacklist combine");
        {
            Route route;
            route.filters.add(makeCommand(VOICE));
            auto blockCC = makeCommand(CONTROL_CHANGE);
            blockCC.negate_ = true;
            route.filters.add(blockCC);
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(! state.passesFilters(route, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("noterange passes only notes within the note range (key split)");
        {
            Route route;
            route.filters.add(makeCommand(NOTE_RANGE, {"60", "72"}));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 72, (uint8)100)));
            expect(  state.passesFilters(route, MidiMessage::noteOff(1, 65, (uint8)0)));
            expect(! state.passesFilters(route, MidiMessage::noteOn(1, 59, (uint8)100)));
            expect(! state.passesFilters(route, MidiMessage::noteOn(1, 73, (uint8)100)));
            expect(! state.passesFilters(route, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("velrange filters note-ons by velocity, always passing note-offs");
        {
            Route route;
            route.filters.add(makeCommand(VELOCITY_RANGE, {"64", "127"}));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)64)));
            expect(! state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)40)));
            // note-offs always pass so a velocity split can't leave notes stuck
            expect(  state.passesFilters(route, MidiMessage::noteOff(1, 60, (uint8)0)));
            expect(! state.passesFilters(route, MidiMessage::controllerEvent(1, 7, 100)));
        }

        beginTest("inscale passes only notes that belong to the key");
        {
            Route route;
            route.filters.add(makeCommand(IN_SCALE, {"C", "major"}));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));   // C
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 62, (uint8)100)));   // D
            expect(! state.passesFilters(route, MidiMessage::noteOn(1, 61, (uint8)100)));   // C#
            expect(! state.passesFilters(route, MidiMessage::noteOn(1, 66, (uint8)100)));   // F#
            expect(  state.passesFilters(route, MidiMessage::noteOff(1, 60, (uint8)0)));    // in-key off
            expect(! state.passesFilters(route, MidiMessage::noteOff(1, 61, (uint8)0)));    // out-of-key off
            expect(! state.passesFilters(route, MidiMessage::controllerEvent(1, 7, 100)));  // non-note

            // the root and scale matter: F# belongs to G major
            Route g;
            g.filters.add(makeCommand(IN_SCALE, {"G", "major"}));
            expect(  state.passesFilters(g, MidiMessage::noteOn(1, 66, (uint8)100)));       // F#
            expect(! state.passesFilters(g, MidiMessage::noteOn(1, 65, (uint8)100)));       // F natural
        }

        beginTest("'not inscale' passes only notes outside the key");
        {
            Route route;
            ApplicationCommand cmd = makeCommand(IN_SCALE, {"C", "major"});
            cmd.negate_ = true;
            route.filters.add(cmd);
            expect(! state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));   // C in key, blocked
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 61, (uint8)100)));   // C# out of key, passes
        }

        beginTest("cc14 matches an MSB/LSB controller pair");
        {
            // without a number, any 14-bit-capable controller (MSB 0-31) passes
            expect(  makeCommand(CONTROL_CHANGE_14BIT).matches(state, MidiMessage::controllerEvent(1, 10, 64), 0));
            expect(  makeCommand(CONTROL_CHANGE_14BIT).matches(state, MidiMessage::controllerEvent(1, 42, 64), 0));
            expect(! makeCommand(CONTROL_CHANGE_14BIT).matches(state, MidiMessage::controllerEvent(1, 64, 64), 0));

            // with a number, only that MSB controller and its +32 LSB pass
            auto cc14 = makeCommand(CONTROL_CHANGE_14BIT, {"10"});
            expect(  cc14.matches(state, MidiMessage::controllerEvent(1, 10, 64), 0));
            expect(  cc14.matches(state, MidiMessage::controllerEvent(1, 42, 64), 0));
            expect(! cc14.matches(state, MidiMessage::controllerEvent(1, 11, 64), 0));
            expect(! cc14.matches(state, MidiMessage::controllerEvent(1, 7, 64), 0));
        }

        beginTest("Number selectors accept a '..' range");
        {
            // cc range: 1..10
            auto ccRange = makeCommand(CONTROL_CHANGE, {"1..10"});
            expect(  ccRange.matches(state, MidiMessage::controllerEvent(1, 1, 0), 0));
            expect(  ccRange.matches(state, MidiMessage::controllerEvent(1, 10, 0), 0));
            expect(  ccRange.matches(state, MidiMessage::controllerEvent(1, 5, 0), 0));
            expect(! ccRange.matches(state, MidiMessage::controllerEvent(1, 11, 0), 0));
            expect(! ccRange.matches(state, MidiMessage::controllerEvent(1, 0, 0), 0));

            // single value still works
            auto ccOne = makeCommand(CONTROL_CHANGE, {"7"});
            expect(  ccOne.matches(state, MidiMessage::controllerEvent(1, 7, 0), 0));
            expect(! ccOne.matches(state, MidiMessage::controllerEvent(1, 8, 0), 0));

            // note range, by name (note the '-' in C-2 must not confuse the range)
            auto onRange = makeCommand(NOTE_ON, {"C3..C4"});
            expect(  onRange.matches(state, MidiMessage::noteOn(1, 60, (uint8)100), 0));   // C3
            expect(  onRange.matches(state, MidiMessage::noteOn(1, 72, (uint8)100), 0));   // C4
            expect(! onRange.matches(state, MidiMessage::noteOn(1, 59, (uint8)100), 0));
            auto onLowRange = makeCommand(NOTE_ON, {"C-2..C-1"});
            expect(  onLowRange.matches(state, MidiMessage::noteOn(1, 0, (uint8)100), 0));  // C-2
            expect(  onLowRange.matches(state, MidiMessage::noteOn(1, 12, (uint8)100), 0)); // C-1
            expect(! onLowRange.matches(state, MidiMessage::noteOn(1, 13, (uint8)100), 0));

            // pc range
            auto pcRange = makeCommand(PROGRAM_CHANGE, {"0..7"});
            expect(  pcRange.matches(state, MidiMessage::programChange(1, 0), 0));
            expect(  pcRange.matches(state, MidiMessage::programChange(1, 7), 0));
            expect(! pcRange.matches(state, MidiMessage::programChange(1, 8), 0));

            // cc14 range selects MSB controllers and their LSB partners
            auto cc14Range = makeCommand(CONTROL_CHANGE_14BIT, {"0..3"});
            expect(  cc14Range.matches(state, MidiMessage::controllerEvent(1, 2, 0), 0));    // MSB 2
            expect(  cc14Range.matches(state, MidiMessage::controllerEvent(1, 34, 0), 0));   // LSB of 2
            expect(! cc14Range.matches(state, MidiMessage::controllerEvent(1, 4, 0), 0));    // MSB 4 out of range
        }

        beginTest("Channel filter accepts a '..' range");
        {
            Route route;
            route.filters.add(makeCommand(CHANNEL, {"1..4"}));
            route.filters.add(makeCommand(NOTE));
            expect(  state.passesFilters(route, MidiMessage::noteOn(1, 60, (uint8)100)));
            expect(  state.passesFilters(route, MidiMessage::noteOn(4, 60, (uint8)100)));
            expect(! state.passesFilters(route, MidiMessage::noteOn(5, 60, (uint8)100)));

            // a channel range with no type filter gates on its own
            Route chOnly;
            chOnly.filters.add(makeCommand(CHANNEL, {"10..12"}));
            expect(  state.passesFilters(chOnly, MidiMessage::controllerEvent(11, 7, 0)));
            expect(! state.passesFilters(chOnly, MidiMessage::controllerEvent(9, 7, 0)));
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
            expect(! nrpn.matches(state, MidiMessage::controllerEvent(1, 100, 1), 0));
            // RPN parameter selectors (100/101) plus the data entry CCs (6/38)
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 101, 0), 0));
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 100, 0), 0));
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 6, 2), 0));
            expect(! rpn.matches(state, MidiMessage::controllerEvent(1, 98, 1), 0));
            // both pick up the shared data-entry controllers
            expect(  nrpn.matches(state, MidiMessage::controllerEvent(1, 38, 0), 0));
            expect(  rpn.matches(state, MidiMessage::controllerEvent(1, 38, 0), 0));
        }
    }
};

static FilterTests filterTests;
