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

#include "TestHelpers.h"
#include "../Source/ApplicationState.h"

// Exercises every native transform through ApplicationCommand::transform.
class TransformTests : public UnitTest
{
public:
    TransformTests() : UnitTest("Transforms", "Transforms") {}

    void runTest() override
    {
        ApplicationState state;

        beginTest("Channel transforms");
        {
            MidiMessage m = MidiMessage::noteOn(3, 60, (uint8)100);
            expect(makeCommand(CHANNEL_MAP, {"3", "5"}).transform(state, m));
            expectEquals(m.getChannel(), 5);

            // a non-matching channel is left alone
            m = MidiMessage::noteOn(4, 60, (uint8)100);
            expect(makeCommand(CHANNEL_MAP, {"3", "5"}).transform(state, m));
            expectEquals(m.getChannel(), 4);

            m = MidiMessage::controllerEvent(7, 10, 20);
            expect(makeCommand(CHANNEL_SET, {"2"}).transform(state, m));
            expectEquals(m.getChannel(), 2);

            m = MidiMessage::noteOn(16, 60, (uint8)100);
            expect(makeCommand(CHANNEL_ADD, {"2"}).transform(state, m));
            expectEquals(m.getChannel(), 2);   // 16 + 2 wraps to 2

            m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(CHANNEL_ADD, {"-1"}).transform(state, m));
            expectEquals(m.getChannel(), 16);  // 1 - 1 wraps to 16
        }

        beginTest("Note transforms");
        {
            MidiMessage m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(TRANSPOSE, {"12"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 72);

            // out of range is dropped
            m = MidiMessage::noteOn(1, 120, (uint8)100);
            expect(! makeCommand(TRANSPOSE, {"12"}).transform(state, m));

            m = MidiMessage::noteOn(1, 5, (uint8)100);
            expect(! makeCommand(TRANSPOSE, {"-10"}).transform(state, m));

            m = MidiMessage::noteOn(1, 36, (uint8)100);
            expect(makeCommand(NOTE_MAP, {"36", "60"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 60);

            m = MidiMessage::noteOn(1, 37, (uint8)100);
            expect(makeCommand(NOTE_MAP, {"36", "60"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 37);   // unchanged
        }

        beginTest("Velocity transforms (note-on only)");
        {
            MidiMessage m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(VELOCITY_SCALE, {"0.5"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 50);

            m = MidiMessage::noteOn(1, 60, (uint8)10);
            expect(makeCommand(VELOCITY_SCALE, {"0.01"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 1);   // clamped to minimum 1

            m = MidiMessage::noteOn(1, 60, (uint8)10);
            expect(makeCommand(VELOCITY_SET, {"64"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 64);

            m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(VELOCITY_ADD, {"50"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 127);  // clamped to maximum

            // note-off velocity is left untouched
            m = MidiMessage::noteOff(1, 60, (uint8)80);
            expect(makeCommand(VELOCITY_SET, {"10"}).transform(state, m));
            expect(m.isNoteOff());
            expectEquals((int)m.getVelocity(), 80);
        }

        beginTest("Control Change transforms");
        {
            MidiMessage m = MidiMessage::controllerEvent(1, 7, 100);
            expect(makeCommand(CONTROL_CHANGE_MAP, {"7", "11"}).transform(state, m));
            expectEquals(m.getControllerNumber(), 11);
            expectEquals(m.getControllerValue(), 100);

            m = MidiMessage::controllerEvent(1, 7, 100);
            expect(makeCommand(CONTROL_CHANGE_ADD, {"7", "10"}).transform(state, m));
            expectEquals(m.getControllerValue(), 110);

            m = MidiMessage::controllerEvent(1, 7, 100);
            expect(makeCommand(CONTROL_CHANGE_SCALE, {"7", "0.5"}).transform(state, m));
            expectEquals(m.getControllerValue(), 50);

            // a different controller is left alone
            m = MidiMessage::controllerEvent(1, 8, 100);
            expect(makeCommand(CONTROL_CHANGE_SCALE, {"7", "0.5"}).transform(state, m));
            expectEquals(m.getControllerValue(), 100);
        }

        beginTest("Program Change transforms");
        {
            MidiMessage m = MidiMessage::programChange(1, 5);
            expect(makeCommand(PROGRAM_CHANGE_MAP, {"5", "10"}).transform(state, m));
            expectEquals(m.getProgramChangeNumber(), 10);

            m = MidiMessage::programChange(1, 5);
            expect(makeCommand(PROGRAM_CHANGE_ADD, {"1"}).transform(state, m));
            expectEquals(m.getProgramChangeNumber(), 6);
        }

        beginTest("Pitch Bend transforms");
        {
            MidiMessage m = MidiMessage::pitchWheel(1, 8192);
            expect(makeCommand(PITCH_BEND_ADD, {"100"}).transform(state, m));
            expectEquals(m.getPitchWheelValue(), 8292);

            m = MidiMessage::pitchWheel(1, 12288);
            expect(makeCommand(PITCH_BEND_SCALE, {"0.5"}).transform(state, m));
            expectEquals(m.getPitchWheelValue(), 10240);   // scaled around centre 8192

            m = MidiMessage::pitchWheel(1, 12288);
            expect(makeCommand(PITCH_BEND_SET, {"0"}).transform(state, m));
            expectEquals(m.getPitchWheelValue(), 0);
        }

        beginTest("Channel Pressure transforms");
        {
            MidiMessage m = MidiMessage::channelPressureChange(1, 50);
            expect(makeCommand(CHANNEL_PRESSURE_ADD, {"10"}).transform(state, m));
            expectEquals(m.getChannelPressureValue(), 60);

            m = MidiMessage::channelPressureChange(1, 100);
            expect(makeCommand(CHANNEL_PRESSURE_SCALE, {"0.5"}).transform(state, m));
            expectEquals(m.getChannelPressureValue(), 50);

            m = MidiMessage::channelPressureChange(1, 10);
            expect(makeCommand(CHANNEL_PRESSURE_SET, {"64"}).transform(state, m));
            expectEquals(m.getChannelPressureValue(), 64);
        }

        beginTest("Gamma curves");
        {
            // gamma 1.0 is linear (identity)
            MidiMessage m = MidiMessage::noteOn(1, 60, (uint8)64);
            expect(makeCommand(VELOCITY_CURVE, {"1.0"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 64);

            // gamma 2.0 attenuates: 64/127 -> ^2 -> ~32
            m = MidiMessage::noteOn(1, 60, (uint8)64);
            expect(makeCommand(VELOCITY_CURVE, {"2.0"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 32);

            // endpoints are preserved (max stays max)
            m = MidiMessage::noteOn(1, 60, (uint8)127);
            expect(makeCommand(VELOCITY_CURVE, {"2.0"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 127);

            // CC value curve
            m = MidiMessage::controllerEvent(1, 7, 64);
            expect(makeCommand(CONTROL_CHANGE_CURVE, {"7", "2.0"}).transform(state, m));
            expectEquals(m.getControllerValue(), 32);
            m = MidiMessage::controllerEvent(1, 7, 0);
            expect(makeCommand(CONTROL_CHANGE_CURVE, {"7", "2.0"}).transform(state, m));
            expectEquals(m.getControllerValue(), 0);

            // channel pressure curve
            m = MidiMessage::channelPressureChange(1, 64);
            expect(makeCommand(CHANNEL_PRESSURE_CURVE, {"2.0"}).transform(state, m));
            expectEquals(m.getChannelPressureValue(), 32);
        }

        beginTest("Transforms ignore unrelated message types");
        {
            MidiMessage m = MidiMessage::noteOn(1, 60, (uint8)100);
            const MidiMessage original = m;
            expect(makeCommand(CONTROL_CHANGE_MAP, {"7", "11"}).transform(state, m));
            expectEquals(m.getNoteNumber(), original.getNoteNumber());
            expect(m.isNoteOn());
        }
    }
};

static TransformTests transformTests;
