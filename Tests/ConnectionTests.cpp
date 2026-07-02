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

#include "../Source/ApplicationState.h"

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
        state.pollConnectionsForTest();
        auto& routes = state.getRoutes();
        if (routes.isEmpty() || routes[0]->inputs.isEmpty() || routes[0]->outputs.isEmpty()
            || routes[0]->inputs[0]->midiIn == nullptr || routes[0]->outputs[0]->out == nullptr)
        {
            logMessage("  skipped: could not open the virtual ports in this process");
            return false;
        }

        state.startOutputSenderForTest();
        virtualSource->sendMessageNow(toSend);

        // wait for the routed message to make the round trip through CoreMIDI
        const uint32 start = Time::getMillisecondCounter();
        for (;;)
        {
            { const ScopedLock sl(capture.lock); if (capture.received.size() > 0) break; }
            if ((int) (Time::getMillisecondCounter() - start) > 2000) break;
            Thread::sleep(10);
        }
        state.stopOutputSenderForTest();

        const ScopedLock sl(capture.lock);
        out = capture.received;
        return true;
    }

    void runTest() override
    {
        beginTest("timer connects a waiting input once its port appears, and drops it when it vanishes");
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
            state.pollConnectionsForTest();
            expect(input->midiIn != nullptr);
            expect(input->fullInName.contains(portName));

            // remove the port; a reconcile pass should notice it is gone and drop it
            virtualSource.reset();
            if (! waitForPort([] { return MidiInput::getAvailableDevices(); }, portName, false, 3000))
            {
                logMessage("  note: port lingered in the list; skipping disconnect check");
                return;
            }
            state.pollConnectionsForTest();
            expect(input->midiIn == nullptr);
            expect(input->fullInName.isEmpty());
        }

        beginTest("timer opens a waiting output once its port appears");
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

            state.pollConnectionsForTest();
            expect(dest->out != nullptr);
            expect(dest->fullName.contains(portName));
        }

        beginTest("a note routed through 'transp' reaches the connected output transposed");
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
        }

        beginTest("a note routed through 'chmap' reaches the connected output on the mapped channel");
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
        }

        beginTest("MCP start_route and stop_route stay safe under a live MIDI stream");
        {
            // a background thread floods a connected route through the real MIDI
            // callback path while routes are started and stopped over MCP. This
            // exercises the teardown ordering of stop_route (unlink, panic, drain,
            // delete: a message arriving mid-teardown must never leave a dangling
            // output pointer in the send queue) and the lock-free device opens of
            // start_route against live callbacks. It is a concurrency stress, not
            // a deterministic race reproducer: a regression shows up as a crash or
            // a stalled stream, most reliably under a sanitizer.
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
            state.startOutputSenderForTest();

            auto mcp = [&state](const String& json)
            {
                auto* previous = std::cerr.rdbuf(nullptr);
                const String response = state.handleMcpJsonForTest(json);
                std::cerr.rdbuf(previous);
                return JSON::parse(response);
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

                // finally stop the live route itself mid-stream: its outputs are
                // being enqueued at this very moment, the exact teardown race
                const var stopLive = mcp(String(R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":)")
                                         + R"({"name":"stop_route","arguments":{"route":)"
                                         + String(liveId) + "}}}");
                expect(! stopLive.getProperty("result", var()).getProperty("isError", var()));
                expectEquals(state.getRoutes().size(), 0);
            }

            stopFlood = true;
            flood.join();
            state.stopOutputSenderForTest();
        }
    }
};

static ConnectionTests connectionTests;

#endif // JUCE_MAC || JUCE_LINUX
