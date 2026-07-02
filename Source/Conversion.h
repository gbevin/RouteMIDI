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

#pragma once

#include "JuceHeader.h"

#include "ApplicationCommand.h"
#include "BitScaling.h"

// Inter-conversion between controller value types. cc, cc14, rpn and nrpn take a
// number selecting which controller or parameter; pp takes a note number (which
// is optional on the source side, where it means "any note"); pb, cp and pc have
// a single value per channel and take no number.
namespace conversion
{

enum Type { Cc7, Cc14, Rpn, Nrpn, Pb, Cp, Pc, Pp };

bool parseType(const String& s, Type& out);

// whether a type always takes a number argument (the controller/parameter types)
inline bool typeNeedsNumber(Type type)
{
    return type == Cc7 || type == Cc14 || type == Rpn || type == Nrpn;
}

// whether a type requires a number when it is the destination: the controller and
// parameter types, plus pp (poly pressure must know which note to land on)
inline bool dstTypeNeedsNumber(Type type)
{
    return typeNeedsNumber(type) || type == Pp;
}

// Walks a convert spec ("srctype [num] dsttype [num]") and fills the normalized
// [srctype, srcnum, dsttype, dstnum] fields. A source pp may omit its note (stored
// as "-1", meaning any note); a destination pp requires one. Returns the number of
// tokens consumed, or -1 if the spec is still incomplete or invalid.
int parseSpec(const StringArray& opts, String& srcType, String& srcNum,
              String& dstType, String& dstNum);

// given the options collected so far for a convert command, returns true once all
// required tokens are present
bool specComplete(const StringArray& opts);

// the bit resolution of a type's value (pitch bend and the 14-bit parameter
// types are 14-bit, everything else is 7-bit)
inline int valueBits(Type type)
{
    return (type == Cc14 || type == Rpn || type == Nrpn || type == Pb) ? 14 : 7;
}

// returns the scaling method for a conversion: Zero-Extension when either side
// is an RPN whose parameter LSB is 0-31 (an absolute MIDI 1.0 RPN), else
// Min-Center-Max
bitscaling::ScaleMethod scaleMethodFor(Type srcType, int srcNum, Type dstType, int dstNum);

// applies a gamma curve to a value in [0, maxValue]: gamma < 1 boosts low values
// (concave), gamma > 1 attenuates them (convex), gamma 1 is linear
int gammaCurve(int value, int maxValue, double gamma);

// true for the six RPN/NRPN value-transform commands handled in the converter stage
inline bool isRpnTransform(CommandIndex c)
{
    return c == NRPN_ADD || c == NRPN_SCALE || c == NRPN_CURVE ||
           c == RPN_ADD  || c == RPN_SCALE  || c == RPN_CURVE;
}

// emit a complete (N)RPN transmission: the parameter select and data entry
// followed by the null that deselects the parameter (MA07 / MIDI 1.0 best
// practice). Both the convert path and the value-transform path go through here
// so their framing stays identical.
void emitRpn(Array<MidiMessage>& out, int channel, int param, int value,
             bool isNRPN, bool is14Bit, double timestamp);

// emit the destination representation of a parameter value, scaling the bit
// width as needed; appends an RPN/NRPN null for (N)RPN destinations
void emit(Array<MidiMessage>& out, int channel, int srcValue, int srcBits,
          Type dstType, int dstNum, bitscaling::ScaleMethod method, double timestamp);

// A convert or transform command compiled to plain numbers once, so the real-time
// converter stage matches rules without re-parsing option strings per message.
struct Rule
{
    bool isTransform = false;             // false: convert; true: rpn/nrpn value transform

    // convert fields
    Type src = Cc7;
    Type dst = Cc7;
    int  srcNum = 0;                      // controller/parameter, or note; -1 = any note (pp source)
    int  dstNum = 0;                      // controller/parameter, or note (pp destination)
    int  srcBits = 7;                     // resolution of the source value
    bitscaling::ScaleMethod method = bitscaling::MinCenterMax;

    // transform fields (add/scale/curve on an rpn or nrpn parameter)
    CommandIndex op = CONVERT;            // NRPN_ADD/SCALE/CURVE or RPN_ADD/SCALE/CURVE
    bool   nrpn = false;                  // transform targets an NRPN (vs RPN) parameter
    int    param = 0;                     // parameter number the transform acts on
    int    addAmount = 0;                 // add offset
    double factor = 1.0;                  // scale factor
    double gamma = 1.0;                   // curve gamma
};

} // namespace conversion
