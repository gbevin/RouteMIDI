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

} // namespace conversion
