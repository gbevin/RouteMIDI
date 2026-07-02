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

    int lastPolyPressure(const Array<MidiMessage>& out, int note)
    {
        int v = -1;
        for (const auto& m : out)
            if (m.isAftertouch() && m.getNoteNumber() == note)
                v = m.getAfterTouchValue();
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

    // counts how many times a specific controller-value pair appears in the output
    int countController(const Array<MidiMessage>& out, int ccNum, int value)
    {
        int n = 0;
        for (const auto& m : out)
            if (m.isController() && m.getControllerNumber() == ccNum && m.getControllerValue() == value)
                ++n;
        return n;
    }

    bool isController(const MidiMessage& m, int ch, int ccNum, int value)
    {
        return m.isController() && m.getChannel() == ch
            && m.getControllerNumber() == ccNum && m.getControllerValue() == value;
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

    // runs several converter-stage commands together over a sequence of messages
    Array<MidiMessage> runConverters(ApplicationState& state,
                                     const Array<std::pair<CommandIndex, StringArray>>& cmds,
                                     const Array<MidiMessage>& in)
    {
        Route route;
        for (const auto& c : cmds)
            route.converters.add(makeCommand(c.first, c.second));
        route.inputs.add(new RouteInput());
        auto& input = *route.inputs[0];

        Array<MidiMessage> out;
        for (const auto& m : in)
            state.processConverters(route, input, m, out);
        return out;
    }

    // all complete 14-bit (N)RPN values that a downstream detector would see
    Array<int> collect14BitRpnValues(const Array<MidiMessage>& out)
    {
        MidiRPNDetector detector;
        Array<int> values;
        for (const auto& m : out)
            if (m.isController())
            {
                auto parsed = detector.tryParse(m.getChannel(), m.getControllerNumber(), m.getControllerValue());
                if (parsed.has_value() && parsed->is14BitValue)
                    values.add(parsed->value);
            }
        return values;
    }

    int countPitchWheels(const Array<MidiMessage>& out)
    {
        int n = 0;
        for (const auto& m : out)
            if (m.isPitchWheel()) ++n;
        return n;
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

        beginTest("Untargeted NRPN passes through verbatim (no regeneration or doubling)");
        {
            // with a convert rule for NRPN 4128, an unrelated NRPN 4129 must pass
            // through byte-for-byte - not be reassembled and re-emitted (which used
            // to duplicate the parameter select and add an intermediate 7-bit value)
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 99, 32));   // param MSB (4129 >> 7)
            in.add(MidiMessage::controllerEvent(1, 98, 33));   // param LSB (4129 & 127)
            in.add(MidiMessage::controllerEvent(1, 6, 9));     // data MSB
            in.add(MidiMessage::controllerEvent(1, 38, 44));   // data LSB
            auto out = runConvert(state, {"nrpn", "4128", "cc", "2"}, in);
            expectEquals(out.size(), 4);
            expect(isController(out[0], 1, 99, 32));
            expect(isController(out[1], 1, 98, 33));
            expect(isController(out[2], 1, 6, 9));
            expect(isController(out[3], 1, 38, 44));
        }

        beginTest("An intercepted parameter's closing null is consumed with it");
        {
            // the null (CC 101/100 = 127, or CC 99/98 = 127) that deselects an
            // intercepted parameter is part of its framing: the selects were consumed
            // and replaced by the CC, so the matching deselect is dropped too - even
            // when the device closes with the universal RPN null after an NRPN select
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 99, 32));    // NRPN 4128 select
            in.add(MidiMessage::controllerEvent(1, 98, 32));
            in.add(MidiMessage::controllerEvent(1, 6, 3));      // data -> converted to CC 2
            in.add(MidiMessage::controllerEvent(1, 38, 37));
            in.add(MidiMessage::controllerEvent(1, 101, 127));  // RPN null closing 4128
            in.add(MidiMessage::controllerEvent(1, 100, 127));
            auto out = runConvert(state, {"nrpn", "4128", "cc", "2"}, in);

            expect(lastCC(out, 2) >= 0);                        // the NRPN was converted
            expectEquals(countController(out, 101, 127), 0);    // closing null consumed
            expectEquals(countController(out, 100, 127), 0);
            expectEquals(countController(out, 99, 32), 0);      // 4128 selects consumed
            expectEquals(countController(out, 98, 32), 0);
        }

        beginTest("An untargeted parameter's null still passes through");
        {
            // a null that deselects a parameter the converter never intercepted must
            // reach the destination unchanged
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 99, 32));    // NRPN 4129 select (untargeted)
            in.add(MidiMessage::controllerEvent(1, 98, 33));
            in.add(MidiMessage::controllerEvent(1, 6, 3));
            in.add(MidiMessage::controllerEvent(1, 38, 37));
            in.add(MidiMessage::controllerEvent(1, 101, 127));  // RPN null closing 4129
            in.add(MidiMessage::controllerEvent(1, 100, 127));
            auto out = runConvert(state, {"nrpn", "4128", "cc", "2"}, in);

            expectEquals(countController(out, 101, 127), 1);    // null survives
            expectEquals(countController(out, 100, 127), 1);
        }

        beginTest("RPN interception mirrors NRPN: selects and closing null consumed");
        {
            // the RPN controller set (100/101) is intercepted exactly like the NRPN
            // set (98/99): a convert rpn rule consumes the selects and the null that
            // closes its parameter (param 4128 has LSB 32 > 31, so this stays a pure
            // framing test independent of the RPN zero-extension scaling)
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 101, 32));   // RPN 4128 select
            in.add(MidiMessage::controllerEvent(1, 100, 32));
            in.add(MidiMessage::controllerEvent(1, 6, 3));      // data -> converted to CC 2
            in.add(MidiMessage::controllerEvent(1, 38, 37));
            in.add(MidiMessage::controllerEvent(1, 101, 127));  // RPN null closing 4128
            in.add(MidiMessage::controllerEvent(1, 100, 127));
            auto out = runConvert(state, {"rpn", "4128", "cc", "2"}, in);

            expect(lastCC(out, 2) >= 0);                        // the RPN was converted
            expectEquals(countController(out, 101, 127), 0);    // closing null consumed
            expectEquals(countController(out, 100, 127), 0);
            expectEquals(countController(out, 101, 32), 0);     // 4128 selects consumed
            expectEquals(countController(out, 100, 32), 0);
        }

        beginTest("LinnStrument-style frame: 4129+null pass through, 4128 converts cleanly");
        {
            // the device streams NRPN 4129 then NRPN 4128, each followed by an RPN
            // null. Only 4128 is targeted: it becomes CC 2 and its whole frame - the
            // selects and the closing null - disappears, while the untargeted 4129 and
            // its null are forwarded untouched
            auto cc = [](int n, int v) { return MidiMessage::controllerEvent(1, n, v); };
            Array<MidiMessage> in;
            in.add(cc(99, 32)); in.add(cc(98, 33)); in.add(cc(6, 9)); in.add(cc(38, 44));   // NRPN 4129
            in.add(cc(101, 127)); in.add(cc(100, 127));                                     // 4129 null
            in.add(cc(99, 32)); in.add(cc(98, 32)); in.add(cc(6, 3)); in.add(cc(38, 37));   // NRPN 4128
            in.add(cc(101, 127)); in.add(cc(100, 127));                                     // 4128 null
            auto out = runConvert(state, {"nrpn", "4128", "cc", "2"}, in);

            expectEquals(countController(out, 2, 3), 2);        // 4128 -> CC 2 (7-bit + 14-bit)
            expectEquals(countController(out, 100, 127), 1);    // only 4129's null survives
            expectEquals(countController(out, 101, 127), 1);
            expectEquals(countController(out, 98, 33), 1);      // 4129 select forwarded once
            expectEquals(countController(out, 98, 32), 0);      // 4128 select consumed
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

        beginTest("Poly Pressure conversions");
        {
            // a specific source note converts only that note; others pass through
            auto out = runConvert(state, {"pp", "60", "cc", "1"},
                { MidiMessage::aftertouchChange(1, 60, 100), MidiMessage::aftertouchChange(1, 62, 100) });
            expectEquals(lastCC(out, 1), 100);
            expectEquals(lastPolyPressure(out, 62), 100);   // note 62 untouched
            expect(lastPolyPressure(out, 60) < 0);          // note 60 was converted away

            // channel pressure sprays onto a target note as poly pressure
            auto back = runConvert(state, {"cp", "0", "pp", "60"}, { MidiMessage::channelPressureChange(1, 90) });
            expectEquals(lastPolyPressure(back, 60), 90);

            // a CC lands on a note too
            auto fromCC = runConvert(state, {"cc", "74", "pp", "72"}, { MidiMessage::controllerEvent(1, 74, 127) });
            expectEquals(lastPolyPressure(fromCC, 72), 127);

            // poly pressure can be remapped from one note to another
            auto remap = runConvert(state, {"pp", "60", "pp", "72"}, { MidiMessage::aftertouchChange(1, 60, 55) });
            expectEquals(lastPolyPressure(remap, 72), 55);
        }

        beginTest("Any-note poly pressure collapses with the maximum of held notes");
        {
            // two notes held; the channel pressure follows the loudest, matching
            // how MPE combines pressure, and re-emits when a release lowers it
            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8)100));
            in.add(MidiMessage::noteOn(1, 64, (uint8)100));
            in.add(MidiMessage::aftertouchChange(1, 60, 100));  // C loudest -> 100
            in.add(MidiMessage::aftertouchChange(1, 64, 40));   // still 100 (suppressed)
            in.add(MidiMessage::aftertouchChange(1, 60, 30));   // E now loudest -> 40
            in.add(MidiMessage::noteOff(1, 64, (uint8)0));       // C now loudest -> 30
            in.add(MidiMessage::noteOff(1, 60, (uint8)0));       // none held -> 0

            auto out = runConvert(state, {"pp", "-1", "cp", "0"}, in);

            Array<int> cps;
            for (const auto& m : out)
                if (m.isChannelPressure())
                    cps.add(m.getChannelPressureValue());

            expectEquals(cps.size(), 4);           // 100, 40, 30, 0 (the 40-update is suppressed)
            expectEquals(cps[0], 100);
            expectEquals(cps[1], 40);
            expectEquals(cps[2], 30);
            expectEquals(cps[3], 0);

            // the note-ons and note-offs still pass through untouched
            int noteOns = 0, noteOffs = 0;
            for (const auto& m : out)
            {
                if (m.isNoteOn())  ++noteOns;
                if (m.isNoteOff()) ++noteOffs;
            }
            expectEquals(noteOns, 2);
            expectEquals(noteOffs, 2);
        }

        beginTest("convert parses an optional poly-pressure source note");
        {
            // "convert pp cp" -> any note (sentinel "-1"); "convert pp 60 cp" -> note 60
            auto normalizedConvert = [](StringArray params)
            {
                ApplicationState s;
                auto* prev = std::cerr.rdbuf(nullptr);
                s.parseParameters(params);
                std::cerr.rdbuf(prev);
                return s.getRoutes().getFirst()->converters.getReference(0).opts_;
            };

            auto anyRule = normalizedConvert({ "in", "PortA", "convert", "pp", "cp", "out", "PortB" });
            expectEquals(anyRule[0], String("pp"));
            expectEquals(anyRule[1], String("-1"));   // no note given -> any note
            expectEquals(anyRule[2], String("cp"));

            auto noteRule = normalizedConvert({ "in", "PortA", "convert", "pp", "60", "cp", "out", "PortB" });
            expectEquals(noteRule[1], String("60"));
            expectEquals(noteRule[2], String("cp"));
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

        beginTest("a value transform frames the regenerated parameter with a closing null");
        {
            // a transform regenerates the whole (N)RPN and deselects it, exactly as
            // a convert to (n)rpn does; a 7-bit source keeps this to one regeneration
            auto nrpn = runConverterCommand(state, NRPN_ADD, {"1000", "5"}, rpnInput(1, 1000, 100, true, false));
            expect(parseLastRpn(nrpn, rpn));
            expectEquals(rpn.value, 105);
            expectEquals(countController(nrpn, 99, 127), 1);   // NRPN null (CC 99/98 = 127)
            expectEquals(countController(nrpn, 98, 127), 1);

            // an RPN transform closes with the universal RPN null (CC 101/100 = 127)
            auto rpnOut = runConverterCommand(state, RPN_ADD, {"5", "10"}, rpnInput(1, 5, 100, false, false));
            expect(parseLastRpn(rpnOut, rpn));
            expectEquals(rpn.value, 110);
            expectEquals(countController(rpnOut, 101, 127), 1);
            expectEquals(countController(rpnOut, 100, 127), 1);
        }

        beginTest("a 14-bit CC pair converts to exactly one value once its LSB is known");
        {
            // a device streaming MSB+LSB pairs (like the LinnStrument's CC 6/38)
            // must produce one destination value per pair, not an approximate one on
            // the MSB and an exact one on the LSB. The first pair may still emit
            // twice (pairing is learned on the first LSB); from then on each pair
            // emits once, with the exact 14-bit value.
            Array<MidiMessage> in;
            in.addArray(cc14Input(1, 6, 0));                     // pair 1: learning
            in.addArray(cc14Input(1, 6, (44 << 7) | 8));         // pair 2
            in.addArray(cc14Input(1, 6, (44 << 7) | 9));         // pair 3
            auto out = runConvert(state, {"cc14", "6", "nrpn", "6"}, in);

            auto values = collect14BitRpnValues(out);
            expectEquals(values.size(), 4);          // 2 (learning pair) + 1 + 1
            expectEquals(values[2], (44 << 7) | 8);
            expectEquals(values[3], (44 << 7) | 9);
        }

        beginTest("an MSB-only 14-bit CC sender keeps converting on every MSB");
        {
            // a sender that never transmits the LSB half still converts each MSB
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 7, 100));
            in.add(MidiMessage::controllerEvent(1, 7, 101));
            in.add(MidiMessage::controllerEvent(1, 7, 102));
            auto out = runConvert(state, {"cc14", "7", "cc", "1"}, in);

            expectEquals(out.size(), 3);
            expectEquals(countController(out, 1, 100), 1);
            expectEquals(countController(out, 1, 101), 1);
            expectEquals(countController(out, 1, 102), 1);
        }

        beginTest("an (N)RPN data pair converts to exactly one value once its LSB is known");
        {
            // same adaptive pairing as the 14-bit CC: the second identical NRPN
            // frame produces a single destination CC instead of two
            Array<MidiMessage> in;
            in.addArray(rpnInput(1, 245, 12873, true, true));    // frame 1: learning
            in.addArray(rpnInput(1, 245, 12873, true, true));    // frame 2
            auto out = runConvert(state, {"nrpn", "245", "cc", "7"}, in);

            expectEquals(out.size(), 3);                         // 2 (learning) + 1
            expectEquals(countController(out, 7, 100), 3);       // 12873 >> 7 = 100
        }

        beginTest("a value transform regenerates a 14-bit pair once, at full resolution");
        {
            // transforms apply at the parsed resolution, so acting on the 7-bit MSB
            // half of a pair would glitch (an add lands 128x too big); once pairing
            // is learned, only the exact 14-bit value is transformed and regenerated
            Array<MidiMessage> in;
            in.addArray(rpnInput(1, 1000, 8000, true, true));    // frame 1: learning
            in.addArray(rpnInput(1, 1000, 8000, true, true));    // frame 2
            auto out = runConverterCommand(state, NRPN_ADD, {"1000", "100"}, in);

            auto values = collect14BitRpnValues(out);
            expectEquals(values.size(), 2);                      // one 14-bit regen per frame
            expectEquals(values[0], 8100);
            expectEquals(values[1], 8100);
            // frame 1's MSB half regenerated a (glitched, clamped) 7-bit NRPN while
            // pairing was unknown; frame 2's MSB half regenerated nothing
            expectEquals(countController(out, 6, 127), 1);
        }

        beginTest("an active (N)RPN selection protects CC 6/38 from cc14 rules");
        {
            // a device doing genuine RPN work - declaring Pitch Bend Sensitivity,
            // announcing an MPE zone - must not have its data entry hijacked by a
            // cc14 rule on controller 6: inside a selection CC 6/38 are (N)RPN data,
            // and only the bare pairs after the closing null convert as a 14-bit CC
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 101, 0));     // RPN 0 select
            in.add(MidiMessage::controllerEvent(1, 100, 0));
            in.add(MidiMessage::controllerEvent(1, 6, 24));      // sensitivity: protected
            in.add(MidiMessage::controllerEvent(1, 101, 127));   // null closes the selection
            in.add(MidiMessage::controllerEvent(1, 100, 127));
            in.add(MidiMessage::controllerEvent(1, 6, 40));      // bare pair: converts
            in.add(MidiMessage::controllerEvent(1, 38, 10));
            auto out = runConvert(state, {"cc14", "6", "pb", "0"}, in);

            expectEquals(countController(out, 6, 24), 1);        // RPN data survived
            expectEquals(countController(out, 101, 0), 1);       // selects and null too
            expectEquals(countController(out, 100, 0), 1);
            expectEquals(countController(out, 101, 127), 1);
            expectEquals(countController(out, 100, 127), 1);
            expectEquals(countPitchWheels(out), 2);              // only the bare pair
            expectEquals(lastPitchWheel(out), (40 << 7) | 10);
        }

        beginTest("cc14 rules work even when the route also has (N)RPN rules");
        {
            // with an RPN transform in the same route, bare CC 6/38 traffic (no
            // selection active) must still reach the cc14 rule instead of being
            // treated as stray (N)RPN data, including after a closing null
            Array<std::pair<CommandIndex, StringArray>> cmds;
            cmds.add({ CONVERT, {"cc14", "6", "nrpn", "6"} });
            cmds.add({ RPN_ADD, {"100", "10"} });

            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 101, 0));     // MCM: RPN 6, untargeted
            in.add(MidiMessage::controllerEvent(1, 100, 6));
            in.add(MidiMessage::controllerEvent(1, 6, 15));
            in.add(MidiMessage::controllerEvent(1, 101, 127));   // null
            in.add(MidiMessage::controllerEvent(1, 100, 127));
            in.add(MidiMessage::controllerEvent(1, 6, 44));      // bare pair -> nrpn 6
            in.add(MidiMessage::controllerEvent(1, 38, 8));
            auto out = runConverters(state, cmds, in);

            expectEquals(countController(out, 6, 15), 1);        // the MCM passed through
            expectEquals(countController(out, 100, 6), 1);
            expect(parseLastRpn(out, rpn));
            expect(rpn.isNRPN);                                  // the bare pair converted
            expectEquals(rpn.parameterNumber, 6);
            expectEquals(rpn.value, (44 << 7) | 8);
        }

        beginTest("data increment/decrement of an intercepted parameter is consumed");
        {
            // CC 96/97 act on the selected parameter; when that parameter's selects
            // were consumed by a rule, forwarding an increment would let it land on
            // whatever parameter is still selected downstream
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 99, 32));     // NRPN 4128: intercepted
            in.add(MidiMessage::controllerEvent(1, 98, 32));
            in.add(MidiMessage::controllerEvent(1, 96, 1));      // increment: consumed
            in.add(MidiMessage::controllerEvent(1, 101, 127));   // null: consumed
            in.add(MidiMessage::controllerEvent(1, 100, 127));
            auto out = runConvert(state, {"nrpn", "4128", "cc", "2"}, in);
            expectEquals(out.size(), 0);

            // for an untargeted parameter the increment passes through with its selects
            Array<MidiMessage> other;
            other.add(MidiMessage::controllerEvent(1, 99, 32));  // NRPN 4129: untargeted
            other.add(MidiMessage::controllerEvent(1, 98, 33));
            other.add(MidiMessage::controllerEvent(1, 96, 1));
            auto outOther = runConvert(state, {"nrpn", "4128", "cc", "2"}, other);
            expectEquals(outOther.size(), 3);
            expectEquals(countController(outOther, 96, 1), 1);
        }

        beginTest("the closing null of a parameter converted after split selects is consumed");
        {
            // a stray RPN select byte interleaved into an NRPN selection means the
            // NRPN select pair never completes cleanly, so interception can't be
            // decided at select time. The data entry still converts through the
            // tracked selection, and converting must mark the selection intercepted
            // so the closing null is consumed with the frame instead of leaking
            // downstream (where it would deselect an unrelated parameter)
            Array<MidiMessage> in;
            in.add(MidiMessage::controllerEvent(1, 99, 32));    // NRPN select MSB (4128)
            in.add(MidiMessage::controllerEvent(1, 100, 32));   // stray RPN select LSB
            in.add(MidiMessage::controllerEvent(1, 98, 32));    // NRPN select LSB completes 4128
            in.add(MidiMessage::controllerEvent(1, 6, 3));      // data -> converted to CC 2
            in.add(MidiMessage::controllerEvent(1, 101, 127));  // closing null
            in.add(MidiMessage::controllerEvent(1, 100, 127));
            auto out = runConvert(state, {"nrpn", "4128", "cc", "2"}, in);

            expectEquals(lastCC(out, 2), 3);                    // the NRPN was converted
            expectEquals(countController(out, 101, 127), 0);    // closing null consumed
            expectEquals(countController(out, 100, 127), 0);
            expectEquals(countController(out, 98, 32), 0);      // NRPN select consumed
            // the ambiguous 99/100 pair read as an untargeted RPN selection and was
            // forwarded verbatim before the NRPN interpretation could win
            expectEquals(countController(out, 99, 32), 1);
            expectEquals(countController(out, 100, 32), 1);
            expectEquals(out.size(), 3);                        // those two selects + CC 2
        }

        beginTest("one source fans out to every matching rule's destination");
        {
            Array<std::pair<CommandIndex, StringArray>> cmds;
            cmds.add({ CONVERT, {"pb", "0", "cc", "1"} });
            cmds.add({ CONVERT, {"pb", "0", "cc", "2"} });
            auto out = runConverters(state, cmds, { MidiMessage::pitchWheel(1, 16383) });
            expectEquals(lastCC(out, 1), 127);
            expectEquals(lastCC(out, 2), 127);

            Array<std::pair<CommandIndex, StringArray>> ccCmds;
            ccCmds.add({ CONVERT, {"cc", "7", "cc", "20"} });
            ccCmds.add({ CONVERT, {"cc", "7", "cc", "21"} });
            auto ccOut = runConverters(state, ccCmds, { MidiMessage::controllerEvent(1, 7, 90) });
            expectEquals(lastCC(ccOut, 20), 90);
            expectEquals(lastCC(ccOut, 21), 90);
        }

        beginTest("a specific-note pp rule fires alongside an any-note pp rule");
        {
            Array<std::pair<CommandIndex, StringArray>> cmds;
            cmds.add({ CONVERT, {"pp", "60", "cc", "1"} });
            cmds.add({ CONVERT, {"pp", "-1", "cp", "0"} });

            Array<MidiMessage> in;
            in.add(MidiMessage::noteOn(1, 60, (uint8) 100));
            in.add(MidiMessage::aftertouchChange(1, 60, 90));
            auto out = runConverters(state, cmds, in);

            expectEquals(lastCC(out, 1), 90);                    // the per-note rule
            expectEquals(lastChannelPressure(out), 90);          // and the collapse
        }
    }
};

static ConversionTests conversionTests;
