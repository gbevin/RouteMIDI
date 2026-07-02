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
#include "Conversion.h"
#include "Latch.h"
#include "Mono.h"
#include "Mpe.h"

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
    bool changed(int channel, int value);

    int pressure[16][128];  // held-note pressures per channel, -1 = note not held
    int lastMax[16];        // last combined value emitted per channel, -1 = none yet
};

// A single MIDI input port of a route.
struct RouteInput
{
    String inName;                        // requested name (may be a substring)
    String fullInName;                    // resolved name once connected
    bool isVirtual { false };             // created as a virtual port
    bool isStdin { false };               // reads MIDI as text from standard input
    std::unique_ptr<MidiInput> midiIn;

    // converter runtime state, kept per input because RPN/NRPN and 14-bit CC
    // detection is stateful per incoming MIDI stream
    MidiRPNDetector rpnDetector;          // assembles RPN/NRPN from constituent CCs
    int ccMsb[16][32] {};                 // last 14-bit CC MSB, per channel and controller
    bool ccMsbValid[16][32] {};           // whether an MSB has been seen yet

    // pending RPN/NRPN parameter-select bytes, per channel, buffered until the
    // MSB+LSB pair is complete so the converter can decide whether the selected
    // parameter is intercepted (drop its constituents) or passes through (forward
    // the raw selects verbatim)
    int rpnSelMSB[16];                    // pending select MSB byte, -1 = none yet
    int rpnSelLSB[16];                    // pending select LSB byte, -1 = none yet
    MidiMessage rpnSelBuf[16][4];         // raw select CCs awaiting classification
    int rpnSelBufLen[16] {};              // number buffered per channel
    bool rpnSelIntercepted[16] {};        // the currently-selected parameter is a
                                          // converter target, so its closing null
                                          // (in either RPN or NRPN form) is consumed too

    RouteInput();

    // per-zone state (indexed [0] = Lower, [1] = Upper) so a Lower-zone and an
    // Upper-zone operation can run on the same input without sharing state
    mpe::Allocator mpeAlloc[2];           // voice allocation state for MPE expansion
    mpe::Collapser mpeCollapse[2];        // note tracking state for MPE collapse
    mpe::Relocator mpeRelocate[2];        // collision tracking for MPE relocate
    mpe::SensitivityDeclarer mpeSens[2];  // member Pitch Bend Sensitivity declaration
    mpe::McmTracker mcm;                  // MPE zone reconfiguration detection (both zones)

    LatchState latch;                     // held-note tracking for the latch transform
    MonoState mono;                       // held-note tracking for the mono transform
    PressureCollapse pressureCollapse;    // held-note pressure tracking for convert pp -> value
};

// A single MIDI output destination of a route.
struct OutputDest
{
    String name;                          // requested name (may be a substring)
    String fullName;                      // resolved name once connected
    bool isVirtual { false };             // created as a virtual port
    bool isStdout { false };              // writes MIDI as text to standard output
    std::unique_ptr<MidiOutput> out;
    std::unique_ptr<FileOutputStream> syxFile;  // captures SysEx to a .syx file
};

// A route binds one or more MIDI input ports to one or more output ports,
// optionally filtering and transforming the messages that flow through it.
// Every input is forwarded to every output, so a route can merge inputs, split
// to several outputs, or both.
struct Route
{
    OwnedArray<RouteInput> inputs;
    OwnedArray<OutputDest> outputs;

    Array<ApplicationCommand> filters;    // applied first, decide pass/block
    Array<ApplicationCommand> transforms; // applied in order to passing messages
    Array<ApplicationCommand> mpeOps;     // MPE zone relocate/collapse/expand rules
    Array<ApplicationCommand> converters; // CC/CC14/RPN/NRPN inter-conversion rules
    Array<conversion::Rule> convertRules; // converters compiled to numbers, rebuilt on demand
    Array<ApplicationCommand> outputSplit; // 0 or 1: distribute MPE voices across the outputs

    mpe::Splitter mpeSplit;               // per-route voice-to-output allocation state

    bool panic { false };                 // send all-notes-off to outputs on disconnect/shutdown
};
