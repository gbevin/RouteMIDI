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
#include "../Source/ScriptMidiMessageClass.h"

// Drives the JavaScript "MIDI" object through a real JUCE Javascript engine, the
// same way ApplicationState does when running a js/jsf transform.
class ScriptTests : public UnitTest
{
public:
    ScriptTests() : UnitTest("JavaScript", "Script") {}

    void runTest() override
    {
        ApplicationState state;
        JavascriptEngine engine;
        auto* midi = new ScriptMidiMessageClass(state);
        engine.registerNativeObject("MIDI", midi);

        beginTest("Inspectors and mutators rewrite the current message");
        {
            midi->setMidiMessage(MidiMessage::noteOn(1, 60, (uint8)100));
            auto r = engine.execute("if (MIDI.isNoteOn()) { MIDI.setChannel(2); MIDI.setNote(72); MIDI.setVelocity(40); }");
            expect(r.wasOk(), r.getErrorMessage());
            const auto& m = midi->getMidiMessage();
            expect(m.isNoteOn());
            expectEquals(m.getChannel(), 2);
            expectEquals(m.getNoteNumber(), 72);
            expectEquals((int)m.getVelocity(), 40);
        }

        beginTest("Setters for pitch bend, channel pressure, poly pressure, controller number");
        {
            midi->setMidiMessage(MidiMessage::pitchWheel(1, 8192));
            engine.execute("MIDI.setPitchBend(0);");
            expectEquals(midi->getMidiMessage().getPitchWheelValue(), 0);

            midi->setMidiMessage(MidiMessage::channelPressureChange(1, 10));
            engine.execute("MIDI.setChannelPressure(100);");
            expectEquals(midi->getMidiMessage().getChannelPressureValue(), 100);

            midi->setMidiMessage(MidiMessage::aftertouchChange(1, 60, 10));
            engine.execute("MIDI.setPolyPressure(90);");
            expectEquals(midi->getMidiMessage().getAfterTouchValue(), 90);

            midi->setMidiMessage(MidiMessage::controllerEvent(1, 7, 64));
            engine.execute("MIDI.setControllerNumber(11);");
            expectEquals(midi->getMidiMessage().getControllerNumber(), 11);
            expectEquals(midi->getMidiMessage().getControllerValue(), 64);
        }

        beginTest("block() drops the message and resets between messages");
        {
            midi->setMidiMessage(MidiMessage::noteOn(1, 60, (uint8)100));
            engine.execute("MIDI.block();");
            expect(midi->isBlocked());

            midi->setMidiMessage(MidiMessage::noteOn(1, 60, (uint8)100));
            expect(! midi->isBlocked());
        }

        beginTest("send* factory emits extra messages");
        {
            midi->setMidiMessage(MidiMessage::noteOn(1, 60, (uint8)100));
            engine.execute("MIDI.sendNoteOn(1, 64, 80); MIDI.sendController(2, 7, 127);");
            const auto& e = midi->getEmitted();
            expectEquals(e.size(), 2);
            expect(e[0].isNoteOn());
            expectEquals(e[0].getNoteNumber(), 64);
            expectEquals((int)e[0].getVelocity(), 80);
            expect(e[1].isController());
            expectEquals(e[1].getChannel(), 2);
            expectEquals(e[1].getControllerNumber(), 7);
            expectEquals(e[1].getControllerValue(), 127);
        }

        beginTest("send(array) and sendSysEx build raw and SysEx messages");
        {
            midi->setMidiMessage(MidiMessage::noteOn(1, 60, (uint8)100));
            engine.execute("MIDI.send([144, 60, 100]); MIDI.sendSysEx([126, 0]);");
            const auto& e = midi->getEmitted();
            expectEquals(e.size(), 2);
            expect(e[0].isNoteOn());
            expectEquals(e[0].getNoteNumber(), 60);
            expect(e[1].isSysEx());
            expectEquals(e[1].getSysExDataSize(), 2);
        }

        beginTest("emitted is reset for each new message");
        {
            midi->setMidiMessage(MidiMessage::noteOn(1, 60, (uint8)100));
            expectEquals(midi->getEmitted().size(), 0);
        }

        beginTest("Global state persists across messages");
        {
            // the engine is reused for every message, so a global variable set in
            // one execution is still there in the next -- this is the accent-every-
            // fourth-note example from JAVASCRIPT.md, which counts note-ons
            const String accent =
                "if (MIDI.isNoteOn()) { if (typeof count == 'undefined') count = 0;"
                " if (++count % 4 == 0) MIDI.setVelocity(127); }";

            int velocities[5] = {};
            for (int i = 0; i < 5; ++i)
            {
                midi->setMidiMessage(MidiMessage::noteOn(1, 60, (uint8)50));
                auto r = engine.execute(accent);
                expect(r.wasOk(), r.getErrorMessage());
                velocities[i] = (int)midi->getMidiMessage().getVelocity();
            }

            expectEquals(velocities[0], 50);
            expectEquals(velocities[1], 50);
            expectEquals(velocities[2], 50);
            expectEquals(velocities[3], 127);   // the 4th note-on is accented
            expectEquals(velocities[4], 50);    // and the count keeps going
        }
    }
};

static ScriptTests scriptTests;
