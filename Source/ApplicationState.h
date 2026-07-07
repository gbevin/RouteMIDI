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
#include "TextMidi.h"

#include <atomic>
#include <memory>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

class McpServer;

class ApplicationState : public MidiInputCallback, public Timer
{
public:
    ApplicationState();
    ~ApplicationState() override;
    void initialise(JUCEApplicationBase& app);
    void shutdown();

    // the current number/note conventions, from the hex/octave/note settings
    textmidi::Format textFormat() const;

    // value parsing helpers, shared with ApplicationCommand; thin wrappers around
    // the textmidi helpers using the current settings
    uint8 asNoteNumber(String value) const          { return textmidi::asNoteNumber(value, textFormat()); }
    uint8 asDecOrHex7BitValue(String value) const   { return textmidi::asDecOrHex7BitValue(value, textFormat()); }
    uint16 asDecOrHex14BitValue(String value) const { return textmidi::asDecOrHex14BitValue(value, textFormat()); }
    int asDecOrHexIntValue(String value) const      { return textmidi::asDecOrHexIntValue(value, textFormat()); }

    static uint8 limit7Bit(int value)   { return textmidi::limit7Bit(value); }
    static uint16 limit14Bit(int value) { return textmidi::limit14Bit(value); }

    // the parser and the per-message processing stages, exposed so a route can
    // be built and driven a message at a time without real MIDI hardware
    void parseParameters(StringArray& parameters);
    // like parseParameters, but newly created routes land in the given staging
    // list instead of routes_ (see the MCP start_route tool)
    void parseParametersInto(OwnedArray<Route>& target, StringArray& parameters);
    String schemaJson() const;
    // the input carries the per-stream state the stateful filters need (the
    // cc14range filter remembers each controller's MSB to assemble its value)
    bool passesFilters(Route& route, RouteInput& input, const MidiMessage& msg);
    Array<MidiMessage> applyTransforms(Route& route, RouteInput& input, const MidiMessage& msg);
    void processMpe(Route& route, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output);
    void processConverters(Route& route, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output);
    void rebuildConvertRules(Route& route);   // compile route.converters to route.convertRules
    // Control (defined below) drives the internal lifecycle steps that the JUCE
    // application object and its timer otherwise run, so an ApplicationState can
    // be driven directly, without the application's event loop
    class Control;

    // distributes a message across a route's output ports as MPE voices; fills
    // parallel arrays where outPorts[i] is the destination index for outMsgs[i]
    // (-1 means broadcast to every output)
    void processSplit(Route& route, const MidiMessage& msg, Array<MidiMessage>& outMsgs, Array<int>& outPorts);

    // runs one message through every stage of a route (filters, transforms,
    // MPE operations, conversions, split) and appends the messages the route
    // emits, with outPorts[i] the destination output index for outMsgs[i]
    // (-1 means every output); routeMessage sends what this produces, and the
    // MCP inject_midi tool uses it to inject messages and echo the result.
    // Returns whether the message tripped the panic safety net's zone reset,
    // which sends all-notes-off and reset-all-controllers to the outputs
    // directly, beyond what lands in outMsgs
    bool processRouteMessage(Route& route, RouteInput& input, const MidiMessage& msg,
                             Array<MidiMessage>& outMsgs, Array<int>& outPorts);
    const OwnedArray<Route>& getRoutes() const { return routes_; }

    // text MIDI codec (TextMidi.cpp) with the current settings: renders and
    // parses the SendMIDI/ReceiveMIDI-compatible text format
    String messageToText(const MidiMessage& msg) const
    {
        return textmidi::messageToText(msg, textFormat());
    }
    void parseTextMidi(const StringArray& tokens, Array<MidiMessage>& output) const
    {
        textmidi::parseTextMidi(tokens, textFormat(), output);
    }

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
    // prints a parse-time error and records it in parseErrors_, so the MCP
    // start_route tool can reject a call whose commands only partially applied
    void reportParseError(const String& message);
    // adds a processing command (filter, transform, MPE operation, conversion or
    // split) to the route, with the same normalization and validation as the
    // command-line parser; returns an error message, or an empty string on success
    String addProcessingCommand(Route& route, ApplicationCommand cmd, bool negate);
    void openInput(RouteInput& input);
    bool tryToConnectInput(RouteInput& input);
    void createVirtualInput(RouteInput& input, const String& name);
    OutputDest* openOutput(const String& name);
    OutputDest* createVirtualOutput(const String& name);

    void routeMessage(Route& route, RouteInput& input, const MidiMessage& msg);
    void applyMpeOp(const ApplicationCommand& cmd, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output);
    void sendToDest(OutputDest* dest, const MidiMessage& msg);
    // sends processRouteMessage results to the route's outputs, honoring the
    // per-message destination index (-1 = every output)
    void sendRouted(Route& route, const Array<MidiMessage>& outMsgs, const Array<int>& outPorts);

    // Outgoing hardware sends are handed to a dedicated thread so the real-time
    // MIDI input callback never blocks on MIDI output I/O. Under a heavy stream
    // (e.g. a full-hand MPE controller flooding NRPN) doing sendMessageNow inline
    // on the callback made the callback slow enough that CoreMIDI dropped incoming
    // packets - a lost note-off is a stuck note. The callback now only routes and
    // enqueues; this thread drains the queue.
    void startOutputSender();
    void stopOutputSender();
    void enqueueSend(MidiOutput* out, const MidiMessage& msg);

    struct OutgoingMidi { MidiOutput* out; MidiMessage msg; };
    std::deque<OutgoingMidi> sendQueue_;
    std::mutex sendQueueMutex_;
    std::condition_variable sendQueueCv_;
    std::thread senderThread_;
    std::atomic<bool> senderShouldExit_ { false };
    void sendZoneReset(Route& route);
    void sendPanic(Route& route);
    bool hasStdinInput() const;
    void readStdinMidi();

    void printMonitor(const String& inName, const MidiMessage& msg);

    void printVersion();
    void printSchemaJson();
    void printMcpConfig();
    void installMcpConfig(const String& client);
    void printUsage();
    void initialiseScripting();

    Array<ApplicationCommand> commands_;
    ApplicationCommand currentCommand_;

    OwnedArray<Route> routes_;
    int nextRouteId_ { 1 };
    OwnedArray<Route>* parseTarget_ { nullptr };   // non-null while parsing into a
                                                   // staging list instead of routes_
    StringArray parseErrors_;                      // semantic errors of the last
                                                   // parseParametersInto run

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

    // the MCP stdio server (McpServer.cpp), created by "--mcp"; it drives routes_
    // and the processing commands, so it is a friend
    friend class McpServer;
    std::unique_ptr<McpServer> mcpServer_;

    CriticalSection midiCallbackLock_;
};

// Direct control over the lifecycle steps that the JUCE application object and
// its Timer normally drive: a connection-reconcile pass, and the background
// output sender that hands MIDI to the hardware. It lets an ApplicationState be
// driven step by step without the application's event loop. As a nested class
// it reaches those private members without widening the public interface.
class ApplicationState::Control
{
public:
    explicit Control(ApplicationState& state) : state_(state) {}

    // run one connect/disconnect reconcile pass; the Timer otherwise runs these
    // periodically once the application is live
    void reconcileConnections() { state_.timerCallback(); }

    // start and stop the background output sender that initialise() otherwise
    // owns, so routed messages reach a connected output
    void startOutputSender() { state_.startOutputSender(); }
    void stopOutputSender()  { state_.stopOutputSender(); }

private:
    ApplicationState& state_;
};
