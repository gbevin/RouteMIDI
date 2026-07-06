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

#include "Conversion.h"

namespace conversion
{

bool parseType(const String& s, Type& out)
{
    if (s.equalsIgnoreCase("cc"))   { out = Cc7;  return true; }
    if (s.equalsIgnoreCase("cc14")) { out = Cc14; return true; }
    if (s.equalsIgnoreCase("rpn"))  { out = Rpn;  return true; }
    if (s.equalsIgnoreCase("nrpn")) { out = Nrpn; return true; }
    if (s.equalsIgnoreCase("pb"))   { out = Pb;   return true; }
    if (s.equalsIgnoreCase("cp"))   { out = Cp;   return true; }
    if (s.equalsIgnoreCase("pc"))   { out = Pc;   return true; }
    if (s.equalsIgnoreCase("pp"))   { out = Pp;   return true; }
    return false;
}

int parseSpec(const StringArray& opts, String& srcType, String& srcNum,
              String& dstType, String& dstNum)
{
    srcNum = "0";
    dstNum = "0";
    Type st, dt, tmp;
    int i = 0;

    if (i >= opts.size() || ! parseType(opts[i], st)) return -1;
    srcType = opts[i++];
    if (typeNeedsNumber(st))
    {
        if (i >= opts.size()) return -1;
        srcNum = opts[i++];
    }
    else if (st == Pp)
    {
        if (i >= opts.size()) return -1;                    // still need the dsttype
        if (parseType(opts[i], tmp)) srcNum = "-1";         // a type follows: any note
        else                         srcNum = opts[i++];    // otherwise it's the note
    }

    if (i >= opts.size() || ! parseType(opts[i], dt)) return -1;
    dstType = opts[i++];
    if (dstTypeNeedsNumber(dt))
    {
        if (i >= opts.size()) return -1;
        dstNum = opts[i++];
    }
    return i;
}

bool specComplete(const StringArray& opts)
{
    String a, b, c, d;
    return parseSpec(opts, a, b, c, d) >= 0;
}

bitscaling::ScaleMethod scaleMethodFor(Type srcType, int srcNum, Type dstType, int dstNum)
{
    auto isLegacyRpn = [](Type t, int num) { return t == Rpn && (num & 0x7f) <= 31; };
    return (isLegacyRpn(srcType, srcNum) || isLegacyRpn(dstType, dstNum))
        ? bitscaling::ZeroExtension : bitscaling::MinCenterMax;
}

int gammaCurve(int value, int maxValue, double gamma)
{
    if (gamma <= 0.0 || maxValue <= 0)
    {
        return value;
    }
    const double normalized = jlimit(0.0, 1.0, (double) value / (double) maxValue);
    return jlimit(0, maxValue, roundToInt(std::pow(normalized, gamma) * maxValue));
}

void emitRpn(Array<MidiMessage>& out, int channel, int param, int value,
             bool isNRPN, bool is14Bit, double timestamp)
{
    MidiBuffer buffer = MidiRPNGenerator::generate(channel, param & 0x3fff, value & 0x3fff, isNRPN, is14Bit);
    for (const auto meta : buffer)
    {
        auto m = meta.getMessage();
        m.setTimeStamp(timestamp);
        out.add(m);
    }
    auto a = MidiMessage::controllerEvent(channel, isNRPN ? 99 : 101, 127);
    a.setTimeStamp(timestamp);
    out.add(a);
    auto b = MidiMessage::controllerEvent(channel, isNRPN ? 98 : 100, 127);
    b.setTimeStamp(timestamp);
    out.add(b);
}

void emit(Array<MidiMessage>& out, int channel, int srcValue, int srcBits,
          Type dstType, int dstNum, bitscaling::ScaleMethod method, double timestamp)
{
    const int v = bitscaling::scaleValue(srcValue, srcBits, valueBits(dstType), method);

    switch (dstType)
    {
        case Cc7:
        {
            auto m = MidiMessage::controllerEvent(channel, dstNum & 0x7f, v & 0x7f);
            m.setTimeStamp(timestamp);
            out.add(m);
            break;
        }
        case Cc14:
        {
            const int n = dstNum & 0x1f;
            auto msb = MidiMessage::controllerEvent(channel, n, (v >> 7) & 0x7f);
            msb.setTimeStamp(timestamp);
            out.add(msb);
            auto lsb = MidiMessage::controllerEvent(channel, n + 32, v & 0x7f);
            lsb.setTimeStamp(timestamp);
            out.add(lsb);
            break;
        }
        case Rpn:
        case Nrpn:
            // (N)RPN destinations carry a 14-bit value
            emitRpn(out, channel, dstNum, v, dstType == Nrpn, true, timestamp);
            break;
        case Pb:
        {
            auto m = MidiMessage::pitchWheel(channel, jlimit(0, 16383, v));
            m.setTimeStamp(timestamp);
            out.add(m);
            break;
        }
        case Cp:
        {
            auto m = MidiMessage::channelPressureChange(channel, jlimit(0, 127, v));
            m.setTimeStamp(timestamp);
            out.add(m);
            break;
        }
        case Pc:
        {
            auto m = MidiMessage::programChange(channel, jlimit(0, 127, v));
            m.setTimeStamp(timestamp);
            out.add(m);
            break;
        }
        case Pp:
        {
            auto m = MidiMessage::aftertouchChange(channel, dstNum & 0x7f, jlimit(0, 127, v));
            m.setTimeStamp(timestamp);
            out.add(m);
            break;
        }
    }
}

//==============================================================================

void PressureCollapse::reset()
{
    for (int c = 0; c < 16; ++c)
    {
        lastMax[c] = -1;
        for (int n = 0; n < 128; ++n) pressure[c][n] = -1;
    }
}

int PressureCollapse::maxPressure(int channel) const
{
    int m = -1;
    for (int n = 0; n < 128; ++n) m = jmax(m, pressure[channel - 1][n]);
    return jmax(0, m);
}

State::State()
{
    for (int c = 0; c < 16; ++c)
    {
        valMsb[c] = -1;
        rpn14Param[c][0] = -1;
        rpn14Param[c][1] = -1;
    }
}

void State::trackSelect(int ch, int cc, int value)
{
    selection.trackSelect(ch, cc, value);
    valMsb[ch - 1] = -1;   // a (re)selection starts a fresh data-entry value
}

//==============================================================================
// The converter stage, driven by Rule values compiled once from the route's
// string rules (see ApplicationState::rebuildConvertRules). Split into the three
// source families it handles: single-message types, the RPN/NRPN controller set,
// and plain 7/14-bit CC.

// whether the route converts or transforms any RPN/NRPN parameter
static bool hasRpnRule(const Array<Rule>& rules)
{
    for (const auto& r : rules)
    {
        if ((r.isTransform && isRpnTransform(r.op)) || r.src == Rpn || r.src == Nrpn)
        {
            return true;
        }
    }
    return false;
}

// pb, cp, pc and pp sources, each carrying one value per message. Poly pressure
// with an "any note" source (srcNum -1) is collapsed to the maximum held pressure
// (matching how MPE combines channel pressure) before conversion.
static void convertNonController(const Array<Rule>& rules, State& state,
                                 const MidiMessage& msg, Array<MidiMessage>& output)
{
    const int ch = msg.getChannel();
    const double ts = msg.getTimeStamp();

    if (msg.isNoteOnOrOff() || msg.isAftertouch())
    {
        bool anyNotePP = false;
        for (const auto& r : rules)
        {
            if (! r.isTransform && r.src == Pp && r.srcNum == -1) { anyNotePP = true; break; }
        }

        if (anyNotePP)
        {
            const int note = msg.getNoteNumber();
            if      (msg.isNoteOn())  state.pressure.noteOn(ch, note);
            else if (msg.isNoteOff()) state.pressure.noteOff(ch, note);
            else                      state.pressure.set(ch, note, msg.getAfterTouchValue());

            // a note-on only adds a note at zero pressure, which never raises the
            // maximum, so only poly pressure and note-offs need to re-emit
            if (! msg.isNoteOn())
            {
                const int maxP = state.pressure.maxPressure(ch);
                if (state.pressure.changed(ch, maxP))
                {
                    for (const auto& r : rules)
                    {
                        if (! r.isTransform && r.src == Pp && r.srcNum == -1)
                        {
                            emit(output, ch, maxP, 7, r.dst, r.dstNum, r.method, ts);
                        }
                    }
                }
            }

            if (msg.isAftertouch())
            {
                // rules for this specific note still see the pressure before the
                // collapse consumes it, so any-note and per-note conversions coexist
                for (const auto& r : rules)
                {
                    if (! r.isTransform && r.src == Pp && r.srcNum == msg.getNoteNumber())
                    {
                        emit(output, ch, msg.getAfterTouchValue(), 7, r.dst, r.dstNum, r.method, ts);
                    }
                }
                return;   // the poly pressure is consumed by the collapse
            }
            // note-ons and note-offs fall through to pass the note message onward
        }
    }

    // every matching rule emits, so one source can fan out to several destinations;
    // the message is consumed once any rule claimed it
    bool matched = false;
    for (const auto& r : rules)
    {
        if (r.isTransform) continue;

        int srcValue = -1;
        if      (r.src == Pb && msg.isPitchWheel())        srcValue = msg.getPitchWheelValue();
        else if (r.src == Cp && msg.isChannelPressure())   srcValue = msg.getChannelPressureValue();
        else if (r.src == Pc && msg.isProgramChange())     srcValue = msg.getProgramChangeNumber();
        else if (r.src == Pp && msg.isAftertouch() && r.srcNum != -1
                 && msg.getNoteNumber() == r.srcNum)       srcValue = msg.getAfterTouchValue();

        if (srcValue >= 0)
        {
            emit(output, ch, srcValue, r.srcBits, r.dst, r.dstNum, r.method, ts);
            matched = true;
        }
    }
    if (matched)
    {
        return;
    }

    output.add(msg);
}

// applies one value-transform rule to a value in [0, maxValue]. The rescale
// bounds are given at 14-bit resolution and are scaled down when the value is
// 7-bit; the other operations work in the value's own resolution, like the
// (N)RPN transforms always have (an add is clamped, not rescaled)
static int applyValueTransform(const Rule& r, int value, int maxValue)
{
    switch (r.op)
    {
        case CC14_ADD:
        case NRPN_ADD:
        case RPN_ADD:
            return jlimit(0, maxValue, value + r.addAmount);
        case CC14_SCALE:
        case NRPN_SCALE:
        case RPN_SCALE:
            return jlimit(0, maxValue, roundToInt(value * (float) r.factor));
        case CC14_CURVE:
        case NRPN_CURVE:
        case RPN_CURVE:
            return gammaCurve(value, maxValue, r.gamma);
        case CC14_INVERT:
        case NRPN_INVERT:
        case RPN_INVERT:
            return maxValue - value;
        case CC14_SET:
        case NRPN_SET:
        case RPN_SET:
            // the set value is 14-bit and scales down for a 7-bit value, like
            // the rescale bounds
            return jlimit(0, maxValue, (maxValue == 127) ? r.addAmount >> 7 : r.addAmount);
        case CC14_RESCALE:
        case NRPN_RESCALE:
        case RPN_RESCALE:
        {
            const int shift = (maxValue == 127) ? 7 : 0;
            int inLo  = r.inLo  >> shift, inHi  = r.inHi  >> shift;
            int outLo = r.outLo >> shift, outHi = r.outHi >> shift;
            if (inLo > inHi)
            {
                // a reversed input range keeps its stated endpoint mapping
                std::swap(inLo, inHi);
                std::swap(outLo, outHi);
            }
            const int v = jlimit(inLo, inHi, value);
            const int mapped = inHi == inLo
                ? outLo
                : roundToInt(outLo + (v - inLo) * (double)(outHi - outLo) / (inHi - inLo));
            return jlimit(0, maxValue, mapped);
        }
        default:
            return value;
    }
}

// emit a 14-bit CC value as its MSB/LSB controller pair (or the MSB alone when
// the value only has 7-bit resolution), the framing the cc14 value transforms
// regenerate after modifying a value
static void emitCc14(Array<MidiMessage>& out, int channel, int msbController,
                     int value, bool is14Bit, double timestamp)
{
    auto add = [&](int cc, int v)
    {
        MidiMessage m = MidiMessage::controllerEvent(channel, cc, v);
        m.setTimeStamp(timestamp);
        out.add(m);
    };
    if (is14Bit)
    {
        add(msbController, (value >> 7) & 0x7f);
        add(msbController + 32, value & 0x7f);
    }
    else
    {
        add(msbController, value & 0x7f);
    }
}

// the RPN/NRPN controller set (6, 38, 96-101), reached only while a parameter
// selection is in progress. A parameter targeted by a rule is intercepted - its
// constituent CCs, and the null that closes it, are consumed and replaced by the
// converted or transformed result - while every other parameter passes through
// verbatim.
static void convertRpnSet(const Array<Rule>& rules, State& state,
                          const MidiMessage& msg, Array<MidiMessage>& output)
{
    const int ch = msg.getChannel();
    const double ts = msg.getTimeStamp();
    const int cc = msg.getControllerNumber();
    const int val = msg.getControllerValue();

    // whether any convert or transform rule targets this parameter as a source
    auto paramIntercepted = [&](int param, bool isNRPN) -> bool
    {
        for (const auto& r : rules)
        {
            if (! r.isTransform && ((r.src == Rpn && ! isNRPN) || (r.src == Nrpn && isNRPN))
                && r.srcNum == param)
            {
                return true;
            }
            if (r.isTransform && isRpnTransform(r.op) && r.nrpn == isNRPN && r.param == param)
            {
                return true;
            }
        }
        return false;
    };
    auto flushSelects = [&]()
    {
        for (int i = 0; i < state.selBufLen[ch - 1]; ++i)
        {
            output.add(state.selBuf[ch - 1][i]);
        }
        state.selBufLen[ch - 1] = 0;
    };

    // parameter select (98/99 = NRPN, 100/101 = RPN) or null: buffer the CC and,
    // once the buffered selects hold both halves of a pair, either drop them (the
    // parameter is intercepted, so the constituents disappear into the conversion)
    // or forward them verbatim (an untargeted parameter). The null (MSB+LSB both
    // 127) that closes a parameter is dropped along with it when that parameter
    // was intercepted, even if the device closes with the universal RPN null
    // (100/101) after selecting with NRPN (98/99), as the LinnStrument does.
    if (cc >= 98 && cc <= 101)
    {
        if (state.selBufLen[ch - 1] >= 4)   // shouldn't happen; drain as a safety valve
        {
            flushSelects();
        }
        state.selBuf[ch - 1][state.selBufLen[ch - 1]++] = msg;

        int msb = -1, lsb = -1;
        bool nrpnSel = false;
        for (int i = 0; i < state.selBufLen[ch - 1]; ++i)
        {
            const int c = state.selBuf[ch - 1][i].getControllerNumber();
            const int v = state.selBuf[ch - 1][i].getControllerValue();
            if (c == 99 || c == 101) msb = v;
            else                     lsb = v;
            nrpnSel = (c == 98 || c == 99);
        }
        if (msb >= 0 && lsb >= 0)
        {
            const int param = (msb << 7) | lsb;
            const bool isNull = (param == 0x3fff);
            const bool drop = isNull ? state.selIntercepted[ch - 1]
                                     : paramIntercepted(param, nrpnSel);
            if (drop)
            {
                state.selBufLen[ch - 1] = 0;   // replaced by the conversion output
            }
            else
            {
                flushSelects();                // pass through unchanged
            }
            // a real select updates which parameter is active; a null deselects
            state.selIntercepted[ch - 1] = isNull ? false : drop;
        }
        return;
    }

    // data increment/decrement (96/97) act on the selected parameter. When that
    // parameter is intercepted its selects were consumed, so an increment must not
    // leak downstream where it would land on whatever parameter is still selected
    // there; for untargeted parameters it passes through with its selects.
    if (cc == 96 || cc == 97)
    {
        const int param = state.selectedParam(ch);
        if (param >= 0 && paramIntercepted(param, state.selectionIsNrpn(ch)))
        {
            return;
        }
        flushSelects();
        output.add(msg);
        return;
    }

    // data entry (6/38): assemble the selected parameter's value, CC 6 carrying
    // the MSB and CC 38 the LSB. A device sending MSB+LSB pairs would produce two
    // values per pair (a 7-bit one on the MSB, the exact 14-bit one on the LSB):
    // once an LSB has been seen for a parameter, the MSB half of the next pair is
    // consumed silently and the pair converts once, exactly. MSB-only parameters -
    // like RPN 0 or the MPE Configuration Message - never see an LSB and keep
    // converting on every MSB. The learned parameter is cleared on each
    // suppression, so a device that stops sending LSBs loses a single value.
    const int param = state.selectedParam(ch);
    if (param >= 0)
    {
        int value = -1, bits = 7;
        if (cc == 6)
        {
            state.valMsb[ch - 1] = val;
            value = val;
        }
        else if (state.valMsb[ch - 1] >= 0)   // an LSB needs an MSB to pair with
        {
            value = (state.valMsb[ch - 1] << 7) | val;
            bits = 14;
        }

        if (value >= 0)
        {
            const bool nrpn = state.selectionIsNrpn(ch);
            const int typeIdx = nrpn ? 1 : 0;

            bool awaitLsb = false;
            if (bits == 14)
            {
                state.rpn14Param[ch - 1][typeIdx] = param;
            }
            else if (state.rpn14Param[ch - 1][typeIdx] == param)
            {
                awaitLsb = true;
                state.rpn14Param[ch - 1][typeIdx] = -1;
            }

            // convert rules: every matching rule emits (one source can fan out to
            // several destinations); the constituents are consumed either way
            bool converted = false;
            for (const auto& r : rules)
            {
                if (r.isTransform) continue;
                if (((r.src == Rpn && ! nrpn) || (r.src == Nrpn && nrpn)) && r.srcNum == param)
                {
                    state.selBufLen[ch - 1] = 0;        // drop any dangling selects
                    state.selIntercepted[ch - 1] = true;   // and consume the closing null
                    converted = true;
                    if (! awaitLsb)
                    {
                        emit(output, ch, value, bits, r.dst, r.dstNum, r.method, ts);
                    }
                }
            }
            if (converted)
            {
                return;
            }

            // value transform (add/scale/curve): apply every matching rule in
            // order and regenerate the (N)RPN with the modified value
            bool transformed = false;
            int newValue = value;
            const int maxValue = (bits == 14) ? 16383 : 127;
            for (const auto& r : rules)
            {
                if (! r.isTransform || ! isRpnTransform(r.op) || r.nrpn != nrpn || r.param != param)
                {
                    continue;
                }
                transformed = true;
                newValue = applyValueTransform(r, newValue, maxValue);
            }

            if (transformed)
            {
                // regenerate the whole (N)RPN, selects and closing null included,
                // so a transformed parameter is framed exactly like a converted
                // one; the suppressed 7-bit half of a pair regenerates nothing
                // (transforming an MSB at 7-bit resolution would glitch, e.g. an
                // add would land 128x too big)
                state.selBufLen[ch - 1] = 0;
                state.selIntercepted[ch - 1] = true;
                if (! awaitLsb)
                {
                    emitRpn(output, ch, param, newValue, nrpn, bits == 14, ts);
                }
                return;
            }
        }
    }

    // untargeted: forward any buffered selects and this data entry verbatim
    flushSelects();
    output.add(msg);
}

// plain CC sources: 14-bit (MSB on controller N in 0-31, LSB on N+32) then 7-bit.
// A controller acting as half of a targeted 14-bit pair is claimed entirely, so a
// cc7 rule never sees it. Every matching rule emits (one source can fan out to
// several destinations). Returns whether a rule claimed the message.
static bool convertCc(const Array<Rule>& rules, State& state,
                      const MidiMessage& msg, Array<MidiMessage>& output)
{
    const int ch = msg.getChannel();
    const double ts = msg.getTimeStamp();
    const int cc = msg.getControllerNumber();
    const int val = msg.getControllerValue();

    // the MSB (controller 0-31) or LSB (32-63) of a targeted 14-bit CC, paired
    // adaptively exactly like the (N)RPN data entries above
    if (cc < 64)
    {
        const bool isLsb = (cc >= 32);
        const int n = isLsb ? cc - 32 : cc;

        bool convertTargeted = false;
        bool transformTargeted = false;
        for (const auto& r : rules)
        {
            if (! r.isTransform && r.src == Cc14 && (r.srcNum & 0x1f) == n)
            {
                convertTargeted = true;
            }
            if (r.isTransform && isCc14Transform(r.op) && (r.param & 0x1f) == n)
            {
                transformTargeted = true;
            }
        }

        if (convertTargeted || transformTargeted)
        {
            // a completed value converts through every matching rule, or, when
            // no convert rule claims the controller, is transformed in place and
            // regenerated as the same 14-bit CC (mirroring the (N)RPN transforms)
            auto emitValue = [&](int value, int bits)
            {
                if (convertTargeted)
                {
                    for (const auto& r : rules)
                    {
                        if (! r.isTransform && r.src == Cc14 && (r.srcNum & 0x1f) == n)
                        {
                            emit(output, ch, value, bits, r.dst, r.dstNum, r.method, ts);
                        }
                    }
                    return;
                }
                const int maxValue = (bits == 14) ? 16383 : 127;
                int newValue = value;
                for (const auto& r : rules)
                {
                    if (r.isTransform && isCc14Transform(r.op) && (r.param & 0x1f) == n)
                    {
                        newValue = applyValueTransform(r, newValue, maxValue);
                    }
                }
                emitCc14(output, ch, n, newValue, bits == 14, ts);
            };

            if (isLsb)
            {
                state.cc14LsbSeen[ch - 1][n] = true;
                const int msb = state.ccMsbValid[ch - 1][n] ? state.ccMsb[ch - 1][n] : 0;
                emitValue((msb << 7) | val, 14);
            }
            else
            {
                state.ccMsb[ch - 1][n] = val;
                state.ccMsbValid[ch - 1][n] = true;
                if (state.cc14LsbSeen[ch - 1][n])
                {
                    state.cc14LsbSeen[ch - 1][n] = false;   // re-learned on the next LSB
                }
                else
                {
                    emitValue(val, 7);
                }
            }
            return true;
        }
    }

    bool matched = false;
    for (const auto& r : rules)
    {
        if (r.isTransform || r.src != Cc7) continue;
        if (cc == r.srcNum)
        {
            emit(output, ch, val, 7, r.dst, r.dstNum, r.method, ts);
            matched = true;
        }
    }

    return matched;
}

void processMessage(const Array<Rule>& rules, State& state,
                    const MidiMessage& msg, Array<MidiMessage>& output)
{
    if (! msg.isController())
    {
        convertNonController(rules, state, msg, output);
        return;
    }

    const int ch = msg.getChannel();
    const int cc = msg.getControllerNumber();

    // CC 6/38 do double duty: inside an (N)RPN parameter selection they are data
    // entry for the selected parameter, outside one they are the halves of a plain
    // 14-bit CC. The selection is tracked for every stream so the two never
    // collide: a device's genuine (N)RPN work - an MPE Configuration Message, a
    // Pitch Bend Sensitivity declaration - can pass through or convert as (N)RPN
    // while its bare CC 6/38 stream converts as a 14-bit controller.
    if (cc >= 98 && cc <= 101)
    {
        state.trackSelect(ch, cc, msg.getControllerValue());
        if (hasRpnRule(rules))
        {
            convertRpnSet(rules, state, msg, output);
            return;
        }
        // no (N)RPN rules: the selects fall through below like any other CC
    }
    else if (cc == 6 || cc == 38 || cc == 96 || cc == 97)
    {
        if (state.selectionActive(ch))
        {
            if (hasRpnRule(rules))
            {
                convertRpnSet(rules, state, msg, output);
                return;
            }
            // (N)RPN data for downstream: the cc rules must not eat it
            output.add(msg);
            return;
        }
        // no selection active: a plain (14-bit) controller, handled below
    }

    if (convertCc(rules, state, msg, output))
    {
        return;
    }

    output.add(msg);   // not managed by any conversion rule: forward unchanged
}

} // namespace conversion
