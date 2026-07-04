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

// true for the RPN/NRPN value-transform commands handled in the converter stage
inline bool isRpnTransform(CommandIndex c)
{
    return c == NRPN_ADD || c == NRPN_SCALE || c == NRPN_CURVE ||
           c == NRPN_INVERT || c == NRPN_RESCALE || c == NRPN_SET ||
           c == RPN_ADD  || c == RPN_SCALE  || c == RPN_CURVE ||
           c == RPN_INVERT || c == RPN_RESCALE || c == RPN_SET;
}

// true for the 14-bit CC value-transform commands, which need the converter
// stage's stateful MSB/LSB pairing just like the (N)RPN transforms
inline bool isCc14Transform(CommandIndex c)
{
    return c == CC14_ADD || c == CC14_SCALE || c == CC14_CURVE ||
           c == CC14_INVERT || c == CC14_RESCALE || c == CC14_SET;
}

// true for every value-transform command that lives in the converter stage
inline bool isValueTransform(CommandIndex c)
{
    return isRpnTransform(c) || isCc14Transform(c);
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

    // transform fields (add/scale/curve/invert/rescale on a 14-bit CC or an
    // rpn/nrpn parameter)
    CommandIndex op = CONVERT;            // a CC14_*, NRPN_* or RPN_* transform
    bool   nrpn = false;                  // an (N)RPN transform targets an NRPN (vs RPN)
    int    param = 0;                     // parameter or MSB controller the transform acts on
    int    addAmount = 0;                 // add offset
    double factor = 1.0;                  // scale factor
    double gamma = 1.0;                   // curve gamma
    int    inLo = 0, inHi = 16383;        // rescale input range (14-bit resolution)
    int    outLo = 0, outHi = 16383;      // rescale output range (14-bit resolution)
};

// Per-input state for the "any note" poly-pressure collapse (convert pp -> a
// per-channel value without a source note). It tracks the pressure of every held
// note per channel so the combined value can be reduced with the maximum, the
// same way MPE combines channel pressure (see Mpe.h). A cache of the last value
// emitted per channel avoids redundant sends.
struct PressureCollapse
{
    PressureCollapse() { reset(); }

    void reset();

    void noteOn(int channel, int note)         { int& p = pressure[channel - 1][note]; if (p < 0) p = 0; }
    void noteOff(int channel, int note)        { pressure[channel - 1][note] = -1; }
    void set(int channel, int note, int value) { pressure[channel - 1][note] = value; }

    // the highest pressure among the notes currently held on the channel, or 0
    // when none are held
    int maxPressure(int channel) const;

    // returns true (updating the cache) when the channel's combined value differs
    // from the last one emitted, so unchanged values aren't resent
    bool changed(int channel, int value)
    {
        if (lastMax[channel - 1] == value) return false;
        lastMax[channel - 1] = value;
        return true;
    }

    int pressure[16][128];  // held-note pressures per channel, -1 = note not held
    int lastMax[16];        // last combined value emitted per channel, -1 = none yet
};

// The per-input runtime state of the converter stage: (N)RPN reassembly and
// 14-bit CC pairing are stateful per incoming MIDI stream.
struct State
{
    State();

    // --- (N)RPN parameter selection, tracked for every stream ----------------
    // CC 99/101 carry the select MSB and 98/100 the LSB; a 127 byte clears its
    // half, so a completed null (127/127) ends the selection. While a selection
    // is active, CC 6/38/96/97 are (N)RPN data for the selected parameter;
    // outside one they are plain (14-bit) controllers.
    void trackSelect(int ch, int cc, int value);   // also resets the data-entry MSB
    bool selectionActive(int ch) const;
    int  selectedParam(int ch) const;              // -1 until both halves are known
    bool selectionIsNrpn(int ch) const { return selNrpn[ch - 1]; }

    int  selMsb[16], selLsb[16];    // last select bytes seen, -1 = never
    bool selNrpn[16];               // whether the selection came in via 98/99

    // --- raw select CCs awaiting classification ------------------------------
    // buffered until their MSB+LSB pair is complete, then either dropped (the
    // parameter is a rule target) or forwarded verbatim
    MidiMessage selBuf[16][4];
    int  selBufLen[16] {};
    bool selIntercepted[16] {};     // the selected parameter is a rule target, so
                                    // its closing null (in either RPN or NRPN
                                    // form) is consumed too

    // --- (N)RPN data-entry value assembly -------------------------------------
    int valMsb[16];                 // last data-entry MSB (CC 6) since the last
                                    // select, -1 = none; pairs with CC 38

    // --- adaptive MSB+LSB pairing ---------------------------------------------
    // once an LSB has been seen for a source, the MSB half of the next pair is
    // consumed silently and the pair emits once, exactly, on its LSB; the flag
    // clears on each suppression so an MSB-only sender loses at most one value
    int  rpn14Param[16][2];         // last param with an LSB, per [0]=RPN/[1]=NRPN
    int  ccMsb[16][32] {};          // last 14-bit CC MSB, per channel and controller
    bool ccMsbValid[16][32] {};     // whether an MSB has been seen yet
    bool cc14LsbSeen[16][32] {};    // whether the controller's last pair had an LSB

    PressureCollapse pressure;      // held-note tracking for the any-note pp source
};

// runs one message through the converter stage: matching rules emit their
// destination messages, everything else is forwarded untouched
void processMessage(const Array<Rule>& rules, State& state,
                    const MidiMessage& msg, Array<MidiMessage>& output);

} // namespace conversion
