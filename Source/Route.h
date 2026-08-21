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

#pragma once

#include "JuceHeader.h"

#include "ApplicationCommand.h"
#include "Conversion.h"
#include "Latch.h"
#include "Mono.h"
#include "Mpe.h"
#include "ParamSelection.h"
#include "Sustain.h"

#include <deque>

// One routed message kept in a route's capture buffer, so an MCP client can poll
// the traffic flowing through the route (the read_route tool). Each carries a
// per-route monotonic sequence number so a caller can ask for only what's new.
struct CapturedMidi
{
    // per-route sequence number, assigned in arrival order
    int64 seq;
    // the input port the message came in on
    String input;
    // the message as the route emitted it (post-processing)
    MidiMessage message;
};

// A single MIDI input port of a route.
struct RouteInput
{
    // requested name (may be a substring)
    String inName;
    // resolved name once connected
    String fullInName;
    // resolved unique id once connected
    String fullInIdentifier;
    // created as a virtual port
    bool isVirtual { false };
    // reads MIDI as text from standard input
    bool isStdin { false };
    std::unique_ptr<MidiInput> midiIn;

    // converter runtime state, kept per input because (N)RPN reassembly and
    // 14-bit CC pairing are stateful per incoming MIDI stream
    conversion::State conv;

    // last MSB seen per channel and MSB controller (0-31), so the cc14range
    // filter can assemble the 14-bit value its range test needs (its own
    // memory, separate from the converter stage's pairing state)
    uint8 cc14RangeMsb[16][32] {};

    // (N)RPN parameter selection for the "nrpn N" / "rpn N" filters (its own
    // instance, separate from the converter stage's, because the filter stage
    // observes the raw input stream and the converter the transformed one)
    ParamSelection rpnFilter;

    // per-zone state (indexed [0] = Lower, [1] = Upper) so a Lower-zone and an
    // Upper-zone operation can run on the same input without sharing state
    // voice allocation state for MPE expansion
    mpe::Allocator mpeAlloc[2];
    // note tracking state for MPE collapse
    mpe::Collapser mpeCollapse[2];
    // collision tracking for MPE relocate
    mpe::Relocator mpeRelocate[2];
    // member Pitch Bend Sensitivity declaration
    mpe::SensitivityDeclarer mpeSens[2];
    // MPE zone reconfiguration detection (both zones)
    mpe::McmTracker mcm;

    // held-note tracking for the latch transform
    LatchState latch;
    // held-note tracking for the mono transform
    MonoState mono;
    // pedal tracking for the sustain transform
    SustainState sustain;
    // pedal tracking for the sostenuto transform
    SustainState sostenuto;
};

// A single MIDI output destination of a route.
struct OutputDest
{
    // requested name (may be a substring)
    String name;
    // resolved name once connected
    String fullName;
    // unique id of the connected port
    String fullOutIdentifier;
    // created as a virtual port
    bool isVirtual { false };
    // writes MIDI as text to standard output
    bool isStdout { false };
    std::unique_ptr<MidiOutput> out;
    // captures SysEx to a .syx file
    std::unique_ptr<FileOutputStream> syxFile;
};

// A route binds one or more MIDI input ports to one or more output ports,
// optionally filtering and transforming the messages that flow through it.
// Every input is forwarded to every output, so a route can merge inputs, split
// to several outputs, or both.
struct Route
{
    // stable identifier, assigned at creation; unlike an index it survives the
    // removal of other routes (used by the MCP tools)
    int id { 0 };

    OwnedArray<RouteInput> inputs;
    OwnedArray<OutputDest> outputs;

    // applied first, decide pass/block
    Array<ApplicationCommand> filters;
    // applied in order to passing messages
    Array<ApplicationCommand> transforms;
    // MPE zone relocate/collapse/expand rules
    Array<ApplicationCommand> mpeOps;
    // CC/CC14/RPN/NRPN inter-conversion rules
    Array<ApplicationCommand> converters;
    // converters compiled to numbers, rebuilt on demand
    Array<conversion::Rule> convertRules;
    // 0 or 1: distribute MPE voices across the outputs
    Array<ApplicationCommand> outputSplit;

    // per-route voice-to-output allocation state
    mpe::Splitter mpeSplit;

    // send all-notes-off to outputs on disconnect/shutdown
    bool panic { false };

    // rolling buffer of routed messages for the MCP read_route tool; filled only
    // in MCP mode, guarded by the same lock as the routing path (midiCallbackLock_)
    static constexpr int captureCapacity = 1024;
    std::deque<CapturedMidi> capture;
    // next sequence number to hand out
    int64 captureSeq { 0 };

    void captureMessage(const String& inputName, const MidiMessage& msg)
    {
        capture.push_back({ captureSeq++, inputName, msg });
        while ((int) capture.size() > captureCapacity)
        {
            capture.pop_front();
        }
    }
};
