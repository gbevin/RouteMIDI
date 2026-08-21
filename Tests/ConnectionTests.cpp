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

#include <atomic>
#include <thread>

#include "JuceHeader.h"

#include "../Source/ApplicationState.h"
#include "../Source/McpServer.h"

// Exercises ApplicationState::timerCallback() - the connect/disconnect reconcile
// pass - against real (virtual) CoreMIDI/ALSA endpoints. Virtual MIDI ports only
// exist on macOS and Linux, and a headless machine may have no MIDI backend at
// all, so each test creates its own uniquely named virtual port and SKIPS (rather
// than fails) when the backend is unavailable, keeping CI green everywhere.
#if JUCE_MAC || JUCE_LINUX

class ConnectionTests : public UnitTest
{
public:
    ConnectionTests() : UnitTest("Connections", "Connections") {}

    // spins until a device with a name containing `needle` is present (or absent)
    // in the freshly enumerated list, or the timeout elapses; returns whether the
    // wanted state was actually reached
    template <typename Enumerate>
    static bool waitForPort(Enumerate enumerate, const String& needle, bool wantPresent, int timeoutMs)
    {
        const uint32 start = Time::getMillisecondCounter();
        for (;;)
        {
            bool present = false;
            for (auto&& d : enumerate())
            {
                if (d.name.contains(needle)) { present = true; break; }
            }
            if (present == wantPresent) return true;
            if ((int) (Time::getMillisecondCounter() - start) > timeoutMs) return false;
            Thread::sleep(20);
        }
    }

    struct NullMidiCallback : public MidiInputCallback
    {
        void handleIncomingMidiMessage(MidiInput*, const MidiMessage&) override {}
    };

    // captures everything an opened MIDI destination receives, from the CoreMIDI
    // callback thread, so a test can inspect it from the message thread
    struct CaptureMidiCallback : public MidiInputCallback
    {
        CriticalSection lock;
        Array<MidiMessage> received;
        void handleIncomingMidiMessage(MidiInput*, const MidiMessage& m) override
        {
            const ScopedLock sl(lock);
            received.add(m);
        }
    };

    // Builds a route "in <virtual source> <transform...> out <virtual destination>",
    // connects it through a reconcile pass (i.e. through timerCallback, not the
    // parse-time open), then sends `toSend` into the source and collects whatever
    // arrives at the destination. Returns false (having logged a skip) when the
    // MIDI backend cannot provide the virtual ports, so CI without one stays green.
    bool routeThrough(const StringArray& transform, const MidiMessage& toSend, Array<MidiMessage>& out)
    {
        const String inName  = "RouteMIDI RTIn "  + Uuid().toString();
        const String outName = "RouteMIDI RTOut " + Uuid().toString();

        CaptureMidiCallback capture;
        auto virtualDest   = MidiInput::createNewDevice(outName, &capture);   // shows up as an output
        auto virtualSource = MidiOutput::createNewDevice(inName);             // shows up as an input
        if (virtualDest == nullptr || virtualSource == nullptr)
        {
            logMessage("  skipped: virtual MIDI not available on this system");
            return false;
        }
        virtualDest->start();

        if (! waitForPort([] { return MidiInput::getAvailableDevices();  }, inName,  true, 3000) ||
            ! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName, true, 3000))
        {
            logMessage("  skipped: virtual ports never appeared in the device lists");
            return false;
        }

        ApplicationState state;
        StringArray params;
        params.add("in");  params.add(inName);
        params.addArray(transform);
        params.add("out"); params.add(outName);
        state.parseParameters(params);

        // the reconcile pass is what opens both ends
        ApplicationState::Control(state).reconcileConnections();
        auto& routes = state.getRoutes();
        if (routes.isEmpty() || routes[0]->inputs.isEmpty() || routes[0]->outputs.isEmpty()
            || routes[0]->inputs[0]->midiIn == nullptr || routes[0]->outputs[0]->out == nullptr)
        {
            logMessage("  skipped: could not open the virtual ports in this process");
            return false;
        }

        ApplicationState::Control(state).startOutputSender();
        virtualSource->sendMessageNow(toSend);

        // wait for the routed message to make the round trip through CoreMIDI
        const uint32 start = Time::getMillisecondCounter();
        for (;;)
        {
            { const ScopedLock sl(capture.lock); if (capture.received.size() > 0) break; }
            if ((int) (Time::getMillisecondCounter() - start) > 2000) break;
            Thread::sleep(10);
        }
        ApplicationState::Control(state).stopOutputSender();

        const ScopedLock sl(capture.lock);
        out = capture.received;
        return true;
    }

    void runTest() override
    {
        beginTest("timer connects a waiting input once its port appears, and drops it when it vanishes");
        [&]
        {
            const String portName = "RouteMIDI TestIn " + Uuid().toString();

            ApplicationState state;
            StringArray params { "in", portName };
            state.parseParameters(params);   // port does not exist yet: stays waiting

            auto& routes = state.getRoutes();
            expect(routes.size() == 1);
            expect(routes[0]->inputs.size() == 1);
            auto* input = routes[0]->inputs[0];
            expect(input->midiIn == nullptr);   // nothing to connect to yet

            // publish a virtual source so the name shows up as an available input;
            // if the backend cannot create one (e.g. headless CI), skip the test
            auto virtualSource = MidiOutput::createNewDevice(portName);
            if (virtualSource == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            if (! waitForPort([] { return MidiInput::getAvailableDevices(); }, portName, true, 3000))
            {
                logMessage("  skipped: virtual input never appeared in the device list");
                return;
            }

            // a reconcile pass should now find and open the port
            ApplicationState::Control(state).reconcileConnections();
            expect(input->midiIn != nullptr);
            expect(input->fullInName.contains(portName));

            // remove the port; a reconcile pass should notice it is gone and drop it
            virtualSource.reset();
            if (! waitForPort([] { return MidiInput::getAvailableDevices(); }, portName, false, 3000))
            {
                logMessage("  note: port lingered in the list; skipping disconnect check");
                return;
            }
            ApplicationState::Control(state).reconcileConnections();
            expect(input->midiIn == nullptr);
            expect(input->fullInName.isEmpty());
        } ();

        beginTest("timer opens a waiting output once its port appears");
        [&]
        {
            const String portName = "RouteMIDI TestOut " + Uuid().toString();

            ApplicationState state;
            StringArray params { "in", "-", "out", portName };   // stdin in, waiting out
            state.parseParameters(params);

            auto& routes = state.getRoutes();
            expect(routes.size() == 1);
            expect(routes[0]->outputs.size() == 1);
            auto* dest = routes[0]->outputs[0];
            expect(dest->out == nullptr);   // nothing to open yet

            // publish a virtual destination so the name shows up as an available output
            NullMidiCallback nullCallback;
            auto virtualDest = MidiInput::createNewDevice(portName, &nullCallback);
            if (virtualDest == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            if (! waitForPort([] { return MidiOutput::getAvailableDevices(); }, portName, true, 3000))
            {
                logMessage("  skipped: virtual output never appeared in the device list");
                return;
            }

            ApplicationState::Control(state).reconcileConnections();
            expect(dest->out != nullptr);
            expect(dest->fullName.contains(portName));
        } ();

        beginTest("shutdown closes open inputs so no callback outlives the state");
        [&]
        {
            const String inName = "RouteMIDI ShutIn " + Uuid().toString();
            auto virtualSource = MidiOutput::createNewDevice(inName);
            if (virtualSource == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            if (! waitForPort([] { return MidiInput::getAvailableDevices(); }, inName, true, 3000))
            {
                logMessage("  skipped: the virtual input never appeared");
                return;
            }

            ApplicationState state;
            {
                StringArray params;
                params.add("in");  params.add(inName);
                params.add("out"); params.add("RouteMIDI ShutOut Unconnected");
                auto* previous = std::cerr.rdbuf(nullptr);
                state.parseParameters(params);
                ApplicationState::Control(state).reconcileConnections();
                std::cerr.rdbuf(previous);
            }
            auto& routes = state.getRoutes();
            if (routes.isEmpty() || routes[0]->inputs.isEmpty() || routes[0]->inputs[0]->midiIn == nullptr)
            {
                logMessage("  skipped: could not open the virtual input in this process");
                return;
            }

            state.shutdown();
            // the input is closed before shutdown returns, so nothing can fire
            // a callback into the state as it destructs
            expect(routes[0]->inputs[0]->midiIn == nullptr, "shutdown left an input open");
        } ();

        beginTest("shutdown's panic sends the full safety net to a connected output");
        [&]
        {
            const String outName = "RouteMIDI Panic " + Uuid().toString();
            CaptureMidiCallback capture;
            auto virtualDest = MidiInput::createNewDevice(outName, &capture);
            if (virtualDest == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            virtualDest->start();
            if (! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName, true, 3000))
            {
                logMessage("  skipped: the virtual port never appeared");
                return;
            }

            ApplicationState state;
            {
                StringArray params;
                params.add("in");  params.add("RouteMIDI PanicIn Unconnected");
                params.add("panic");
                params.add("out"); params.add(outName);
                auto* previous = std::cerr.rdbuf(nullptr);
                state.parseParameters(params);
                ApplicationState::Control(state).reconcileConnections();
                std::cerr.rdbuf(previous);
            }
            auto& routes = state.getRoutes();
            if (routes.isEmpty() || routes[0]->outputs.isEmpty() || routes[0]->outputs[0]->out == nullptr)
            {
                logMessage("  skipped: could not open the virtual port in this process");
                return;
            }

            ApplicationState::Control(state).startOutputSender();
            state.shutdown();   // panic-enabled route: drains the safety net before returning

            // sustain off, sostenuto off and all-notes-off on every channel
            const uint32 startTime = Time::getMillisecondCounter();
            for (;;)
            {
                { const ScopedLock sl(capture.lock); if (capture.received.size() >= 48) break; }
                if ((int) (Time::getMillisecondCounter() - startTime) > 3000) break;
                Thread::sleep(10);
            }
            const ScopedLock sl(capture.lock);
            expectEquals(capture.received.size(), 48);
            int perChannel[17][128] = {};
            for (const auto& m : capture.received)
            {
                expect(m.isController());
                expectEquals(m.getControllerValue(), 0);
                perChannel[m.getChannel()][m.getControllerNumber()]++;
            }
            for (int channel = 1; channel <= 16; ++channel)
            {
                expectEquals(perChannel[channel][64], 1);
                expectEquals(perChannel[channel][66], 1);
                expectEquals(perChannel[channel][123], 1);
            }
        } ();

        beginTest("timer drops a vanished output and reconnects it when its port returns");
        [&]
        {
            const String inName  = "RouteMIDI OutRecIn "  + Uuid().toString();
            const String outName = "RouteMIDI OutRec "    + Uuid().toString();

            CaptureMidiCallback capture;
            auto virtualDest   = MidiInput::createNewDevice(outName, &capture);
            auto virtualSource = MidiOutput::createNewDevice(inName);
            if (virtualDest == nullptr || virtualSource == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            virtualDest->start();
            if (! waitForPort([] { return MidiInput::getAvailableDevices();  }, inName,  true, 3000) ||
                ! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName, true, 3000))
            {
                logMessage("  skipped: virtual ports never appeared in the device lists");
                return;
            }

            ApplicationState state;
            {
                StringArray params;
                params.add("in");  params.add(inName);
                params.add("out"); params.add(outName);
                state.parseParameters(params);
            }
            ApplicationState::Control(state).reconcileConnections();
            auto& routes = state.getRoutes();
            if (routes.isEmpty() || routes[0]->outputs.isEmpty() || routes[0]->outputs[0]->out == nullptr
                || routes[0]->inputs.isEmpty() || routes[0]->inputs[0]->midiIn == nullptr)
            {
                logMessage("  skipped: could not open the virtual ports in this process");
                return;
            }

            // unplug the destination: the reconcile pass must drop the dead port
            virtualDest = nullptr;
            if (! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName, false, 3000))
            {
                logMessage("  skipped: the virtual port never left the device list");
                return;
            }
            {
                auto* previous = std::cerr.rdbuf(nullptr);
                ApplicationState::Control(state).reconcileConnections();
                std::cerr.rdbuf(previous);
            }
            expect(routes[0]->outputs[0]->out == nullptr, "a vanished output port was not dropped");

            // replug it: the reconcile pass reopens it and messages flow again
            virtualDest = MidiInput::createNewDevice(outName, &capture);
            if (virtualDest == nullptr
                || ! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName, true, 3000))
            {
                logMessage("  skipped: the virtual port could not be recreated");
                return;
            }
            virtualDest->start();
            {
                auto* previous = std::cerr.rdbuf(nullptr);
                ApplicationState::Control(state).reconcileConnections();
                std::cerr.rdbuf(previous);
            }
            expect(routes[0]->outputs[0]->out != nullptr, "a returned output port was not reconnected");

            ApplicationState::Control(state).startOutputSender();
            virtualSource->sendMessageNow(MidiMessage::noteOn(1, 60, (uint8) 100));
            const uint32 start = Time::getMillisecondCounter();
            for (;;)
            {
                { const ScopedLock sl(capture.lock); if (capture.received.size() > 0) break; }
                if ((int) (Time::getMillisecondCounter() - start) > 2000) break;
                Thread::sleep(10);
            }
            ApplicationState::Control(state).stopOutputSender();
            const ScopedLock sl(capture.lock);
            expect(capture.received.size() > 0, "no message arrived at the reconnected output");
        } ();

        beginTest("a note routed through 'transp' reaches the connected output transposed");
        [&]
        {
            Array<MidiMessage> received;
            if (routeThrough({ "transp", "12" }, MidiMessage::noteOn(1, 60, (uint8) 100), received))
            {
                expect(received.size() >= 1);
                if (received.size() >= 1)
                {
                    const auto& m = received.getReference(0);
                    expect(m.isNoteOn());
                    expectEquals(m.getNoteNumber(), 72);        // 60 + 12
                    expectEquals(m.getChannel(), 1);
                    expectEquals((int) m.getVelocity(), 100);
                }
            }
        } ();

        beginTest("a note routed through 'chmap' reaches the connected output on the mapped channel");
        [&]
        {
            Array<MidiMessage> received;
            if (routeThrough({ "chmap", "1", "5" }, MidiMessage::noteOn(1, 64, (uint8) 90), received))
            {
                expect(received.size() >= 1);
                if (received.size() >= 1)
                {
                    const auto& m = received.getReference(0);
                    expect(m.isNoteOn());
                    expectEquals(m.getChannel(), 5);            // remapped 1 -> 5
                    expectEquals(m.getNoteNumber(), 64);
                }
            }
        } ();

        beginTest("MCP inject_midi reaches a connected output through the route's pipeline");
        [&]
        {
            // injection needs no connected input: the route's input port never
            // exists, only the output is live, and the injected message must
            // arrive there transformed
            const String inName  = "RouteMIDI InjIn "  + Uuid().toString();
            const String outName = "RouteMIDI InjOut " + Uuid().toString();

            CaptureMidiCallback capture;
            auto virtualDest = MidiInput::createNewDevice(outName, &capture);
            if (virtualDest == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            virtualDest->start();
            if (! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName, true, 3000))
            {
                logMessage("  skipped: virtual ports never appeared in the device lists");
                return;
            }

            ApplicationState state;
            ApplicationState::Control(state).startOutputSender();

            McpServer mcpServer(state);
            auto mcp = [&mcpServer](const String& json)
            {
                auto* previous = std::cerr.rdbuf(nullptr);
                const var response = mcpServer.handleRequest(JSON::parse(json));
                std::cerr.rdbuf(previous);
                return response;
            };

            mcp(String(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":)")
                + R"({"name":"start_route","arguments":{"commands":["in",")" + inName
                + R"(","transp","12","out",")" + outName + R"("]}}})");
            expectEquals(state.getRoutes().size(), 1);
            ApplicationState::Control(state).reconcileConnections();
            if (state.getRoutes().isEmpty() || state.getRoutes()[0]->outputs.isEmpty()
                || state.getRoutes()[0]->outputs[0]->out == nullptr)
            {
                logMessage("  skipped: could not open the virtual output in this process");
                ApplicationState::Control(state).stopOutputSender();
                return;
            }

            const var response = mcp(R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":)"
                                     R"({"name":"inject_midi","arguments":{"route":1,)"
                                     R"("messages":["channel 1 note-on 60 100"]}}})");
            auto structured = response.getProperty("result", var()).getProperty("structuredContent", var());
            expectEquals((int)structured.getProperty("injected", var()), 1);

            // wait for the injected message to make the trip through CoreMIDI
            const uint32 start = Time::getMillisecondCounter();
            for (;;)
            {
                { const ScopedLock sl(capture.lock); if (capture.received.size() > 0) break; }
                if ((int) (Time::getMillisecondCounter() - start) > 2000) break;
                Thread::sleep(10);
            }
            ApplicationState::Control(state).stopOutputSender();

            const ScopedLock sl(capture.lock);
            expect(capture.received.size() >= 1);
            if (capture.received.size() >= 1)
            {
                const auto& m = capture.received.getReference(0);
                expect(m.isNoteOn());
                expectEquals(m.getNoteNumber(), 72);   // 60 + 12
            }
        } ();

        beginTest("MCP start_route reports connected=true when both ports exist");
        [&]
        {
            const String inName  = "RouteMIDI ConnIn "  + Uuid().toString();
            const String outName = "RouteMIDI ConnOut " + Uuid().toString();

            CaptureMidiCallback capture;
            auto virtualDest   = MidiInput::createNewDevice(outName, &capture);
            auto virtualSource = MidiOutput::createNewDevice(inName);
            if (virtualDest == nullptr || virtualSource == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            virtualDest->start();
            if (! waitForPort([] { return MidiInput::getAvailableDevices();  }, inName,  true, 3000) ||
                ! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName, true, 3000))
            {
                logMessage("  skipped: virtual ports never appeared in the device lists");
                return;
            }

            ApplicationState state;
            McpServer mcpServer(state);
            auto* previous = std::cerr.rdbuf(nullptr);
            const var response = mcpServer.handleRequest(JSON::parse(
                String(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":)")
                + R"({"name":"start_route","arguments":{"commands":["in",")" + inName
                + R"(","out",")" + outName + R"("]}}})"));
            std::cerr.rdbuf(previous);

            const var structured = response.getProperty("result", var()).getProperty("structuredContent", var());
            if (state.getRoutes().isEmpty() || state.getRoutes()[0]->inputs.isEmpty()
                || state.getRoutes()[0]->inputs[0]->midiIn == nullptr
                || state.getRoutes()[0]->outputs.isEmpty()
                || state.getRoutes()[0]->outputs[0]->out == nullptr)
            {
                logMessage("  skipped: could not open the virtual ports in this process");
                return;
            }
            // both ports resolved at start time, so the informative flag is true
            expect((bool) structured.getProperty("connected", var()),
                   "start_route reported connected=false with both ports live");
        } ();

        beginTest("MCP start_route and stop_route stay safe under a live MIDI stream");
        [&]
        {
            // a background thread floods a connected route through the real MIDI
            // callback path while routes are started and stopped over MCP, so the
            // teardown ordering of stop_route (unlink, panic, drain, delete) and
            // the lock-free device opens of start_route run concurrently with live
            // callbacks. It exercises that a message arriving during teardown is
            // handled cleanly and the stream keeps flowing; run it under a
            // sanitizer for the strongest signal.
            const String inName  = "RouteMIDI LiveIn "  + Uuid().toString();
            const String outName = "RouteMIDI LiveOut " + Uuid().toString();

            CaptureMidiCallback capture;
            auto virtualDest   = MidiInput::createNewDevice(outName, &capture);
            auto virtualSource = MidiOutput::createNewDevice(inName);
            if (virtualDest == nullptr || virtualSource == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            virtualDest->start();
            if (! waitForPort([] { return MidiInput::getAvailableDevices();  }, inName,  true, 3000) ||
                ! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName, true, 3000))
            {
                logMessage("  skipped: virtual ports never appeared in the device lists");
                return;
            }

            ApplicationState state;
            ApplicationState::Control(state).startOutputSender();

            McpServer mcpServer(state);
            auto mcp = [&mcpServer](const String& json)
            {
                auto* previous = std::cerr.rdbuf(nullptr);
                const var response = mcpServer.handleRequest(JSON::parse(json));
                std::cerr.rdbuf(previous);
                return response;
            };
            auto receivedCount = [&capture]
            {
                const ScopedLock sl(capture.lock);
                return capture.received.size();
            };
            auto waitForMoreThan = [&receivedCount](int count, int timeoutMs)
            {
                const uint32 start = Time::getMillisecondCounter();
                while (receivedCount() <= count)
                {
                    if ((int) (Time::getMillisecondCounter() - start) > timeoutMs)
                    {
                        return false;
                    }
                    Thread::sleep(10);
                }
                return true;
            };

            // a live route from the virtual source to the virtual destination
            mcp(String(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":)")
                + R"({"name":"start_route","arguments":{"commands":["in",")" + inName
                + R"(","out",")" + outName + R"("]}}})");
            expectEquals(state.getRoutes().size(), 1);
            const int liveId = state.getRoutes()[0]->id;

            // flood the route from a background thread, as a controller would
            std::atomic<bool> stopFlood { false };
            std::thread flood([&stopFlood, &virtualSource]
            {
                int note = 0;
                while (!stopFlood.load())
                {
                    virtualSource->sendMessageNow(MidiMessage::noteOn(1, 1 + (note % 100), (uint8) 100));
                    virtualSource->sendMessageNow(MidiMessage::noteOff(1, 1 + (note % 100), (uint8) 0));
                    ++note;
                    if ((note & 63) == 0)
                    {
                        Thread::sleep(1);   // brief yields keep the flood fast but fair
                    }
                }
            });

            const bool flowing = waitForMoreThan(20, 3000);
            expect(flowing);
            if (flowing)
            {
                // churn side routes over MCP while the stream keeps running
                for (int i = 0; i < 3; ++i)
                {
                    const var started = mcp(R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":)"
                                            R"({"name":"start_route","arguments":{"commands":)"
                                            R"(["in","LiveNoSuchIn","out","LiveNoSuchOut"]}}})");
                    auto structured = started.getProperty("result", var()).getProperty("structuredContent", var());
                    auto* routes = structured.getProperty("routes", var()).getArray();
                    expect(routes != nullptr && routes->size() == 1);
                    const int sideId = (routes != nullptr && routes->size() == 1)
                                           ? (int) routes->getReference(0).getProperty("id", var()) : -1;

                    const var stopped = mcp(String(R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":)")
                                            + R"({"name":"stop_route","arguments":{"route":)"
                                            + String(sideId) + "}}}");
                    expect(! stopped.getProperty("result", var()).getProperty("isError", var()));
                }
                expectEquals(state.getRoutes().size(), 1);   // only the live route remains

                // the live stream survived the churn and is still flowing
                expect(waitForMoreThan(receivedCount(), 3000));

                // finally stop the live route itself mid-stream, while its outputs
                // are being enqueued on the callback thread
                const var stopLive = mcp(String(R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":)")
                                         + R"({"name":"stop_route","arguments":{"route":)"
                                         + String(liveId) + "}}}");
                expect(! stopLive.getProperty("result", var()).getProperty("isError", var()));
                expectEquals(state.getRoutes().size(), 0);
            }

            stopFlood = true;
            flood.join();
            ApplicationState::Control(state).stopOutputSender();
        } ();

        beginTest("a lost input is closed without deadlocking against a live stream");
        [&]
        {
            // closing a MIDI input waits on the device-callback lock, which a
            // concurrent callback holds while it waits for the midi callback
            // lock; if the reconcile pass closed the lost port while holding
            // that lock, an unplug during a stream would hang both threads. A
            // second port floods continuously to keep the device-callback lock
            // busy while the first port is repeatedly unplugged; the reconcile
            // runs on a worker so a deadlock shows as a failure, not a hang
            const String goneName  = "RouteMIDI GoneIn "  + Uuid().toString();
            const String floodName = "RouteMIDI KeepIn "  + Uuid().toString();
            const String outName   = "RouteMIDI GoneOut " + Uuid().toString();

            CaptureMidiCallback capture;
            auto virtualDest  = MidiInput::createNewDevice(outName, &capture);
            auto floodSource  = MidiOutput::createNewDevice(floodName);
            auto goneSource   = MidiOutput::createNewDevice(goneName);
            if (virtualDest == nullptr || floodSource == nullptr || goneSource == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            virtualDest->start();
            if (! waitForPort([] { return MidiInput::getAvailableDevices();  }, goneName,  true, 3000) ||
                ! waitForPort([] { return MidiInput::getAvailableDevices();  }, floodName, true, 3000) ||
                ! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName,   true, 3000))
            {
                logMessage("  skipped: virtual ports never appeared in the device lists");
                return;
            }

            // heap-allocated so a detected deadlock can leak it instead of
            // hanging the suite in its destructor
            auto* state = new ApplicationState();
            {
                StringArray params;
                params.add("in");  params.add(goneName);
                params.add("in");  params.add(floodName);
                params.add("out"); params.add(outName);
                state->parseParameters(params);
            }
            ApplicationState::Control(*state).reconcileConnections();
            auto& routes = state->getRoutes();
            if (routes.isEmpty() || routes[0]->inputs.size() < 2
                || routes[0]->inputs[0]->midiIn == nullptr || routes[0]->inputs[1]->midiIn == nullptr)
            {
                logMessage("  skipped: could not open the virtual ports in this process");
                delete state;
                return;
            }
            ApplicationState::Control(*state).startOutputSender();

            std::atomic<bool> stopFlood { false };
            std::thread flood([&stopFlood, &floodSource]
            {
                int note = 0;
                while (!stopFlood.load())
                {
                    floodSource->sendMessageNow(MidiMessage::noteOn(1, 1 + (note % 100), (uint8) 100));
                    floodSource->sendMessageNow(MidiMessage::noteOff(1, 1 + (note % 100), (uint8) 0));
                    ++note;
                    if ((note & 63) == 0)
                    {
                        Thread::sleep(1);
                    }
                }
            });

            bool deadlocked = false;
            int completedCycles = 0;
            for (int cycle = 0; cycle < 5 && !deadlocked; ++cycle)
            {
                // unplug the port, then reconcile the loss under the flood
                goneSource = nullptr;
                if (! waitForPort([] { return MidiInput::getAvailableDevices(); }, goneName, false, 3000))
                {
                    logMessage("  port removal not observed, ending after "
                               + String(completedCycles) + " cycles");
                    break;
                }

                std::atomic<bool> done { false };
                std::thread reconcile([state, &done]
                {
                    auto* previous = std::cerr.rdbuf(nullptr);
                    ApplicationState::Control(*state).reconcileConnections();
                    std::cerr.rdbuf(previous);
                    done = true;
                });
                const uint32 start = Time::getMillisecondCounter();
                while (!done.load() && (int) (Time::getMillisecondCounter() - start) < 10000)
                {
                    Thread::sleep(5);
                }
                if (!done.load())
                {
                    deadlocked = true;
                    reconcile.detach();
                    break;
                }
                reconcile.join();
                ++completedCycles;

                // replug and reconnect for the next round
                goneSource = MidiOutput::createNewDevice(goneName);
                if (goneSource == nullptr
                    || ! waitForPort([] { return MidiInput::getAvailableDevices(); }, goneName, true, 3000))
                {
                    logMessage("  replug not observed, ending after "
                               + String(completedCycles) + " cycles");
                    break;
                }
                auto* previous = std::cerr.rdbuf(nullptr);
                ApplicationState::Control(*state).reconcileConnections();
                std::cerr.rdbuf(previous);
            }

            expect(! deadlocked, "closing a lost input deadlocked against a live MIDI stream");
            // a pass means nothing unless the scenario actually ran
            expect(completedCycles > 0, "no unplug cycle completed; the deadlock scenario was never exercised");

            stopFlood = true;
            flood.join();
            if (deadlocked)
            {
                // the wedged threads hold the device-callback lock; tearing
                // anything MIDI down would hang, so leak deliberately
                virtualDest.release();
                floodSource.release();
                goneSource.release();
            }
            else
            {
                ApplicationState::Control(*state).stopOutputSender();
                delete state;
            }
        } ();

        beginTest("shutdown stays safe while a stream is still arriving");
        [&]
        {
            // shutdown flushes panic state that the callbacks also touch, so it
            // has to keep them out with the callback lock while it runs; run
            // under a sanitizer for the strongest signal. The panic must still
            // reach the output before the sender is torn down
            const String inName  = "RouteMIDI DownIn "  + Uuid().toString();
            const String outName = "RouteMIDI DownOut " + Uuid().toString();

            CaptureMidiCallback capture;
            auto virtualDest   = MidiInput::createNewDevice(outName, &capture);
            auto virtualSource = MidiOutput::createNewDevice(inName);
            if (virtualDest == nullptr || virtualSource == nullptr)
            {
                logMessage("  skipped: virtual MIDI not available on this system");
                return;
            }
            virtualDest->start();
            if (! waitForPort([] { return MidiInput::getAvailableDevices();  }, inName,  true, 3000) ||
                ! waitForPort([] { return MidiOutput::getAvailableDevices(); }, outName, true, 3000))
            {
                logMessage("  skipped: virtual ports never appeared in the device lists");
                return;
            }

            ApplicationState state;
            {
                StringArray params;
                params.add("in");  params.add(inName);
                params.add("panic");
                params.add("out"); params.add(outName);
                state.parseParameters(params);
            }
            ApplicationState::Control(state).reconcileConnections();
            auto& routes = state.getRoutes();
            if (routes.isEmpty() || routes[0]->inputs.isEmpty() || routes[0]->outputs.isEmpty()
                || routes[0]->inputs[0]->midiIn == nullptr || routes[0]->outputs[0]->out == nullptr)
            {
                logMessage("  skipped: could not open the virtual ports in this process");
                return;
            }
            ApplicationState::Control(state).startOutputSender();

            std::atomic<bool> stopFlood { false };
            std::thread flood([&stopFlood, &virtualSource]
            {
                int note = 0;
                while (!stopFlood.load())
                {
                    virtualSource->sendMessageNow(MidiMessage::noteOn(1, 1 + (note % 100), (uint8) 100));
                    virtualSource->sendMessageNow(MidiMessage::noteOff(1, 1 + (note % 100), (uint8) 0));
                    ++note;
                    if ((note & 63) == 0)
                    {
                        Thread::sleep(1);
                    }
                }
            });

            // wait until the stream demonstrably flows, then shut down under it;
            // without flowing traffic the test's premise doesn't hold, so that
            // must fail rather than silently degrade to a no-stream shutdown
            bool streaming = false;
            {
                const uint32 start = Time::getMillisecondCounter();
                for (;;)
                {
                    { const ScopedLock sl(capture.lock); if (capture.received.size() > 20) { streaming = true; break; } }
                    if ((int) (Time::getMillisecondCounter() - start) > 3000) break;
                    Thread::sleep(10);
                }
            }
            expect(streaming, "no MIDI flowed before shutdown; the under-stream premise never held");
            state.shutdown();
            stopFlood = true;
            flood.join();

            // the panic's all-notes-off went through the sender before it stopped
            bool sawAllNotesOff = false;
            const uint32 start = Time::getMillisecondCounter();
            while (!sawAllNotesOff && (int) (Time::getMillisecondCounter() - start) < 3000)
            {
                {
                    const ScopedLock sl(capture.lock);
                    for (auto& m : capture.received)
                    {
                        if (m.isController() && m.getControllerNumber() == 123)
                        {
                            sawAllNotesOff = true;
                            break;
                        }
                    }
                }
                Thread::sleep(10);
            }
            expect(sawAllNotesOff, "shutdown's panic never reached the output");
        } ();
    }
};

static ConnectionTests connectionTests;

#endif // JUCE_MAC || JUCE_LINUX
