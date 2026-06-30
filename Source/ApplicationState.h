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
#include "Route.h"
#include "ScriptMidiMessageClass.h"

class ApplicationState : public MidiInputCallback, public Timer
{
public:
    ApplicationState();
    void initialise(JUCEApplicationBase& app);
    void shutdown();

    // value parsing helpers, shared with ApplicationCommand
    uint8 asNoteNumber(String value) const;
    uint8 asDecOrHex7BitValue(String value) const;
    uint16 asDecOrHex14BitValue(String value) const;
    int asDecOrHexIntValue(String value) const;

    static uint8 limit7Bit(int value);
    static uint16 limit14Bit(int value);

    // matches a value against a selector token that is either a single value or
    // an inclusive "lo..hi" range; asNote interprets the tokens as note names
    bool selectorMatches(const String& token, int value, bool asNote) const;

    // entry points also used by the test suite (Tests/) to drive the parser,
    // the filter stage and the converter stage without real MIDI hardware
    void parseParameters(StringArray& parameters);
    bool passesFilters(Route& route, const MidiMessage& msg);
    void processMpe(Route& route, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output);
    void processConverters(Route& route, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output);
    // distributes a message across a route's output ports as MPE voices; fills
    // parallel arrays where outPorts[i] is the destination index for outMsgs[i]
    // (-1 means broadcast to every output)
    void processSplit(Route& route, const MidiMessage& msg, Array<MidiMessage>& outMsgs, Array<int>& outPorts);
    const OwnedArray<Route>& getRoutes() const { return routes_; }

    // text MIDI codec, also exercised by the test suite to check round-tripping
    // against the SendMIDI/ReceiveMIDI-compatible text format
    String messageToText(const MidiMessage& msg) const;
    void parseTextMidi(const StringArray& tokens, Array<MidiMessage>& output) const;

private:
    void timerCallback() override;
    void handleIncomingMidiMessage(MidiInput* source, const MidiMessage& msg) override;

    ApplicationCommand* findApplicationCommand(const String& param);
    StringArray parseLineAsParameters(const String& line);
    void executeCurrentCommand();
    void handleVarArgCommand();
    void parseFile(File file);
    void executeCommand(ApplicationCommand& cmd);

    Route* currentRoute();
    Route* routeForNewInput();
    void openInput(RouteInput& input);
    bool tryToConnectInput(RouteInput& input);
    void createVirtualInput(RouteInput& input, const String& name);
    OutputDest* openOutput(const String& name);
    OutputDest* createVirtualOutput(const String& name);
    static bool isMidiInDeviceAvailable(const String& name);

    void routeMessage(Route& route, RouteInput& input, const MidiMessage& msg);
    void applyMpeOp(const ApplicationCommand& cmd, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output);
    void sendToDest(OutputDest* dest, const MidiMessage& msg);
    void sendPanic(Route& route);
    bool hasStdinInput() const;
    void readStdinMidi();

    Array<MidiMessage> applyTransforms(Route& route, const MidiMessage& msg);
    void printMonitor(const String& inName, const MidiMessage& msg);

    String output7BitAsHex(int v) const;
    String output7Bit(int v) const;
    String output14BitAsHex(int v) const;
    String output14Bit(int v) const;
    String outputNote(const MidiMessage& msg) const;

    void printVersion();
    void printUsage();

    Array<ApplicationCommand> commands_;
    ApplicationCommand currentCommand_;

    OwnedArray<Route> routes_;

    bool pendingNegate_;

    bool useHexadecimalsByDefault_;
    bool noteNumbersOutput_;
    bool timestampOutput_;
    bool monitor_;
    bool monitorShowSource_;
    int octaveMiddleC_;

    JavascriptEngine scriptEngine_;
    ScriptMidiMessageClass* scriptMidiMessage_;
    bool hasScript_;

    CriticalSection midiCallbackLock_;
};
