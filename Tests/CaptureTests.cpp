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

#include <iostream>

// Exercises the per-route capture buffer behind the MCP read_route tool: the
// ring buffer itself, and that processRouteMessage records the right traffic
// (only what passes the route, post-processing, tagged by input) when capture
// is enabled.
class CaptureTests : public UnitTest
{
public:
    CaptureTests() : UnitTest("Capture", "Capture") {}

    void parse(ApplicationState& state, const String& line)
    {
        StringArray params;
        params.addTokens(line, true);
        params.removeEmptyStrings(true);
        auto* previous = std::cerr.rdbuf(nullptr);
        state.parseParameters(params);
        std::cerr.rdbuf(previous);
    }

    void runTest() override
    {
        beginTest("The ring keeps the most recent messages with monotonic sequence numbers");
        {
            Route route;
            const int extra = 50;
            for (int i = 0; i < Route::captureCapacity + extra; ++i)
            {
                route.captureMessage("In", MidiMessage::noteOn(1, 60, (uint8)100));
            }
            expectEquals((int) route.capture.size(), Route::captureCapacity);
            // the first `extra` sequence numbers aged out of the ring
            expect(route.capture.front().seq == (int64) extra);
            expect(route.capture.back().seq == (int64) (Route::captureCapacity + extra - 1));
            // sequence numbers are contiguous and strictly increasing
            for (size_t i = 1; i < route.capture.size(); ++i)
            {
                expect(route.capture[i].seq == route.capture[i - 1].seq + 1);
            }
        }

        beginTest("Capture is off by default so ordinary routing records nothing");
        {
            ApplicationState state;
            parse(state, "in X on out Y");
            Route& route = *state.getRoutes().getFirst();
            RouteInput& input = *route.inputs.getFirst();
            Array<MidiMessage> outMsgs;
            Array<int> outPorts;
            state.processRouteMessage(route, input, MidiMessage::noteOn(1, 60, (uint8)100), outMsgs, outPorts);
            expect(route.capture.empty());
        }

        beginTest("When enabled, only messages that pass the route are captured, tagged by input");
        {
            ApplicationState state;
            state.enableTrafficCapture(true);
            parse(state, "in X cc out Y");   // only Control Change passes
            Route& route = *state.getRoutes().getFirst();
            RouteInput& input = *route.inputs.getFirst();
            input.fullInName = "Keyboard";

            auto run = [&](const MidiMessage& m)
            {
                Array<MidiMessage> o;
                Array<int> p;
                state.processRouteMessage(route, input, m, o, p);
            };
            run(MidiMessage::noteOn(1, 60, (uint8)100));    // blocked by the cc filter
            run(MidiMessage::controllerEvent(1, 74, 55));   // passes
            run(MidiMessage::noteOff(1, 60, (uint8)0));      // blocked

            expectEquals((int) route.capture.size(), 1);
            expect(route.capture.front().seq == (int64) 0);
            expectEquals(route.capture.front().input, String("Keyboard"));
            expect(route.capture.front().message.isController());
            expectEquals(route.capture.front().message.getControllerNumber(), 74);
            expectEquals(route.capture.front().message.getControllerValue(), 55);
        }

        beginTest("The captured message is what the route emits, after transforms");
        {
            ApplicationState state;
            state.enableTrafficCapture(true);
            parse(state, "in X transp 12 out Y");   // transpose up an octave
            Route& route = *state.getRoutes().getFirst();
            RouteInput& input = *route.inputs.getFirst();

            Array<MidiMessage> o;
            Array<int> p;
            state.processRouteMessage(route, input, MidiMessage::noteOn(1, 60, (uint8)100), o, p);

            expectEquals((int) route.capture.size(), 1);
            expect(route.capture.front().message.isNoteOn());
            expectEquals(route.capture.front().message.getNoteNumber(), 72);   // 60 + 12
        }

        beginTest("One input message that fans out to several is captured per emitted message");
        {
            ApplicationState state;
            state.enableTrafficCapture(true);
            parse(state, "in X chord 4 7 out Y");   // a note becomes a triad
            Route& route = *state.getRoutes().getFirst();
            RouteInput& input = *route.inputs.getFirst();

            Array<MidiMessage> o;
            Array<int> p;
            state.processRouteMessage(route, input, MidiMessage::noteOn(1, 60, (uint8)100), o, p);

            expectEquals((int) route.capture.size(), 3);   // root, third, fifth
            expectEquals(route.capture[0].message.getNoteNumber(), 60);
            expectEquals(route.capture[1].message.getNoteNumber(), 64);
            expectEquals(route.capture[2].message.getNoteNumber(), 67);
        }
    }
};

static CaptureTests captureTests;
