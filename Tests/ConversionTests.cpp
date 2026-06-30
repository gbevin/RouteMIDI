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

namespace
{
    Array<MidiMessage> rpnInput(int ch, int param, int value, bool isNRPN, bool use14)
    {
        Array<MidiMessage> in;
        for (const auto meta : MidiRPNGenerator::generate(ch, param, value, isNRPN, use14))
            in.add(meta.getMessage());
        return in;
    }

    Array<MidiMessage> cc14Input(int ch, int n, int value14)
    {
        Array<MidiMessage> in;
        in.add(MidiMessage::controllerEvent(ch, n, (value14 >> 7) & 0x7f));
        in.add(MidiMessage::controllerEvent(ch, n + 32, value14 & 0x7f));
        return in;
    }

    Array<MidiMessage> runConvert(ApplicationState& state, const StringArray& rule, const Array<MidiMessage>& in)
    {
        Route route;
        route.converters.add(makeCommand(CONVERT, rule));
        route.inputs.add(new RouteInput());
        auto& input = *route.inputs[0];

        Array<MidiMessage> out;
        for (const auto& m : in)
            state.processConverters(route, input, m, out);
        return out;
    }

    int lastCC(const Array<MidiMessage>& out, int ccNum)
    {
        int v = -1;
        for (const auto& m : out)
            if (m.isController() && m.getControllerNumber() == ccNum)
                v = m.getControllerValue();
        return v;
    }

    int lastPitchWheel(const Array<MidiMessage>& out)
    {
        int v = -1;
        for (const auto& m : out)
            if (m.isPitchWheel())
                v = m.getPitchWheelValue();
        return v;
    }

    int lastChannelPressure(const Array<MidiMessage>& out)
    {
        int v = -1;
        for (const auto& m : out)
            if (m.isChannelPressure())
                v = m.getChannelPressureValue();
        return v;
    }

    int lastProgramChange(const Array<MidiMessage>& out)
    {
        int v = -1;
        for (const auto& m : out)
            if (m.isProgramChange())
                v = m.getProgramChangeNumber();
        return v;
    }

    // runs an arbitrary converter-stage command (e.g. an RPN/NRPN value
    // transform) over a sequence of input messages
    Array<MidiMessage> runConverterCommand(ApplicationState& state, CommandIndex command,
                                           const StringArray& opts, const Array<MidiMessage>& in)
    {
        Route route;
        route.converters.add(makeCommand(command, opts));
        route.inputs.add(new RouteInput());
        auto& input = *route.inputs[0];

        Array<MidiMessage> out;
        for (const auto& m : in)
            state.processConverters(route, input, m, out);
        return out;
    }

    bool parseLastRpn(const Array<MidiMessage>& out, MidiRPNMessage& result)
    {
        MidiRPNDetector detector;
        bool found = false;
        for (const auto& m : out)
        {
            if (m.isController())
            {
                auto parsed = detector.tryParse(m.getChannel(), m.getControllerNumber(), m.getControllerValue());
                if (parsed.has_value()) { result = *parsed; found = true; }
            }
        }
        return found;
    }
}

class ConversionTests : public UnitTest
{
public:
    ConversionTests() : UnitTest("Conversions", "Conversions") {}

    void runTest() override
    {
        ApplicationState state;
        MidiRPNMessage rpn;

        beginTest("CC7 -> NRPN with Min-Center-Max upscale");
        {
            auto out = runConvert(state, {"cc", "7", "nrpn", "1000"},
                                  { MidiMessage::controllerEvent(1, 7, 100) });
            expect(parseLastRpn(out, rpn));
            expect(rpn.isNRPN);
            expectEquals(rpn.parameterNumber, 1000);
            expectEquals(rpn.value, 12873);
        }

        beginTest("CC7 maximum -> NRPN reaches full scale (Min-Center-Max)");
        {
            auto out = runConvert(state, {"cc", "7", "nrpn", "1000"},
                                  { MidiMessage::controllerEvent(1, 7, 127) });
            expect(parseLastRpn(out, rpn));
            expectEquals(rpn.value, 16383);
        }

        beginTest("NRPN -> CC7 downscales");
        {
            auto out = runConvert(state, {"nrpn", "245", "cc", "7"},
                                  rpnInput(1, 245, 12873, true, true));
            expectEquals(lastCC(out, 7), 100);
        }

        beginTest("14-bit CC -> RPN preserves the 14-bit value");
        {
            auto out = runConvert(state, {"cc14", "1", "rpn", "5"}, cc14Input(1, 1, 12873));
            expect(parseLastRpn(out, rpn));
            expect(! rpn.isNRPN);
            expectEquals(rpn.parameterNumber, 5);
            expectEquals(rpn.value, 12873);
        }

        beginTest("RPN -> NRPN carries parameter and value");
        {
            auto out = runConvert(state, {"rpn", "100", "nrpn", "245"},
                                  rpnInput(1, 100, 8192, false, true));
            expect(parseLastRpn(out, rpn));
            expect(rpn.isNRPN);
            expectEquals(rpn.parameterNumber, 245);
            expectEquals(rpn.value, 8192);
        }

        beginTest("Legacy RPN (param LSB 0-31) uses Zero-Extension upscale");
        {
            auto out = runConvert(state, {"cc", "7", "rpn", "0"},
                                  { MidiMessage::controllerEvent(1, 7, 127) });
            expect(parseLastRpn(out, rpn));
            expectEquals(rpn.parameterNumber, 0);
            expectEquals(rpn.value, 16256);   // zero-extension, not 16383
        }

        beginTest("NRPN with same number still uses Min-Center-Max");
        {
            auto out = runConvert(state, {"cc", "7", "nrpn", "0"},
                                  { MidiMessage::controllerEvent(1, 7, 127) });
            expect(parseLastRpn(out, rpn));
            expectEquals(rpn.value, 16383);
        }

        beginTest("Legacy RPN -> CC7 zero-extension downscale clamps");
        {
            auto out = runConvert(state, {"rpn", "0", "cc", "7"},
                                  rpnInput(1, 0, 16383, false, true));
            expectEquals(lastCC(out, 7), 127);
        }

        beginTest("Conversions to (N)RPN append the null terminator");
        {
            auto out = runConvert(state, {"cc", "7", "nrpn", "1000"},
                                  { MidiMessage::controllerEvent(1, 7, 100) });
            expectEquals(lastCC(out, 99), 127);
            expectEquals(lastCC(out, 98), 127);
        }

        beginTest("Non-targeted (N)RPN passes through unchanged");
        {
            auto out = runConvert(state, {"nrpn", "245", "cc", "7"},
                                  rpnInput(1, 99, 5000, true, true));
            expect(parseLastRpn(out, rpn));
            expect(rpn.isNRPN);
            expectEquals(rpn.parameterNumber, 99);
            expectEquals(rpn.value, 5000);
        }

        beginTest("Non Control Change messages pass through a converter route");
        {
            auto out = runConvert(state, {"cc", "7", "nrpn", "1000"},
                                  { MidiMessage::noteOn(1, 60, (uint8)100) });
            expectEquals(out.size(), 1);
            expect(out[0].isNoteOn());
        }

        beginTest("Pitch Bend <-> CC scaling");
        {
            auto down = runConvert(state, {"pb", "0", "cc", "7"}, { MidiMessage::pitchWheel(1, 16383) });
            expectEquals(lastCC(down, 7), 127);
            auto centre = runConvert(state, {"pb", "0", "cc", "7"}, { MidiMessage::pitchWheel(1, 8192) });
            expectEquals(lastCC(centre, 7), 64);

            auto up = runConvert(state, {"cc", "7", "pb", "0"}, { MidiMessage::controllerEvent(1, 7, 127) });
            expectEquals(lastPitchWheel(up), 16383);
            auto upCentre = runConvert(state, {"cc", "7", "pb", "0"}, { MidiMessage::controllerEvent(1, 7, 64) });
            expectEquals(lastPitchWheel(upCentre), 8192);
        }

        beginTest("Channel Pressure conversions");
        {
            auto out = runConvert(state, {"cp", "0", "cc", "11"}, { MidiMessage::channelPressureChange(1, 100) });
            expectEquals(lastCC(out, 11), 100);

            auto back = runConvert(state, {"cc", "7", "cp", "0"}, { MidiMessage::controllerEvent(1, 7, 50) });
            expectEquals(lastChannelPressure(back), 50);
        }

        beginTest("Program Change conversions");
        {
            auto out = runConvert(state, {"pc", "0", "cc", "20"}, { MidiMessage::programChange(1, 5) });
            expectEquals(lastCC(out, 20), 5);

            auto back = runConvert(state, {"cc", "7", "pc", "0"}, { MidiMessage::controllerEvent(1, 7, 10) });
            expectEquals(lastProgramChange(back), 10);
        }

        beginTest("Pitch Bend -> NRPN keeps 14-bit resolution");
        {
            auto out = runConvert(state, {"pb", "0", "nrpn", "1000"}, { MidiMessage::pitchWheel(1, 8192) });
            expect(parseLastRpn(out, rpn));
            expect(rpn.isNRPN);
            expectEquals(rpn.parameterNumber, 1000);
            expectEquals(rpn.value, 8192);
        }

        beginTest("nrpnadd offsets an NRPN value, clamped to its resolution");
        {
            auto out = runConverterCommand(state, NRPN_ADD, {"1000", "100"}, rpnInput(1, 1000, 8000, true, true));
            expect(parseLastRpn(out, rpn));
            expect(rpn.isNRPN);
            expectEquals(rpn.parameterNumber, 1000);
            expectEquals(rpn.value, 8100);

            // a 7-bit NRPN clamps to 127
            auto clamped = runConverterCommand(state, NRPN_ADD, {"1000", "50"}, rpnInput(1, 1000, 100, true, false));
            expect(parseLastRpn(clamped, rpn));
            expectEquals(rpn.value, 127);
        }

        beginTest("nrpnscale scales an NRPN value");
        {
            auto out = runConverterCommand(state, NRPN_SCALE, {"1000", "0.5"}, rpnInput(1, 1000, 8000, true, true));
            expect(parseLastRpn(out, rpn));
            expectEquals(rpn.value, 4000);
        }

        beginTest("nrpncurve applies a gamma curve, preserving the endpoints");
        {
            // gamma 1.0 is the identity
            auto identity = runConverterCommand(state, NRPN_CURVE, {"1000", "1.0"}, rpnInput(1, 1000, 8000, true, true));
            expect(parseLastRpn(identity, rpn));
            expectEquals(rpn.value, 8000);

            // the maximum stays at full scale for any gamma
            auto top = runConverterCommand(state, NRPN_CURVE, {"1000", "2.0"}, rpnInput(1, 1000, 16383, true, true));
            expect(parseLastRpn(top, rpn));
            expectEquals(rpn.value, 16383);
        }

        beginTest("rpnadd offsets an RPN value and leaves other parameters alone");
        {
            auto out = runConverterCommand(state, RPN_ADD, {"5", "-1000"}, rpnInput(1, 5, 8000, false, true));
            expect(parseLastRpn(out, rpn));
            expect(! rpn.isNRPN);
            expectEquals(rpn.parameterNumber, 5);
            expectEquals(rpn.value, 7000);

            // a transform targeting a different parameter passes through unchanged
            auto other = runConverterCommand(state, RPN_ADD, {"5", "-1000"}, rpnInput(1, 9, 8000, false, true));
            expect(parseLastRpn(other, rpn));
            expectEquals(rpn.parameterNumber, 9);
            expectEquals(rpn.value, 8000);
        }

        beginTest("an NRPN transform does not touch RPNs (and vice versa)");
        {
            // nrpnadd must ignore an RPN with the same parameter number
            auto out = runConverterCommand(state, NRPN_ADD, {"5", "100"}, rpnInput(1, 5, 8000, false, true));
            expect(parseLastRpn(out, rpn));
            expect(! rpn.isNRPN);
            expectEquals(rpn.value, 8000);   // unchanged
        }
    }
};

static ConversionTests conversionTests;
