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

class ApplicationState;

class ScriptMidiMessageClass : public DynamicObject
{
public:
    ScriptMidiMessageClass(ApplicationState& state);

    // load the message that the script will inspect and possibly modify
    void setMidiMessage(MidiMessage msg);
    // the (possibly modified) message after the script ran
    const MidiMessage& getMidiMessage() const            { return msg_; }
    // whether the script asked for this message to be dropped
    bool isBlocked() const                               { return blocked_; }
    // any extra messages the script asked to emit
    const Array<MidiMessage>& getEmitted() const         { return emitted_; }

    static MidiMessage& getMsg(const var::NativeFunctionArgs&);
    static ScriptMidiMessageClass& getSelf(const var::NativeFunctionArgs&);
    static ApplicationState& getApplicationState(const var::NativeFunctionArgs&);

    static var getRawData(const var::NativeFunctionArgs&);
    static var getRawDataSize(const var::NativeFunctionArgs&);

    static var getDescription(const var::NativeFunctionArgs&);

    static var getTimeStamp(const var::NativeFunctionArgs&);
    static var getChannel(const var::NativeFunctionArgs&);

    static var isSysEx(const var::NativeFunctionArgs&);
    static var getSysExData(const var::NativeFunctionArgs&);
    static var getSysExDataSize(const var::NativeFunctionArgs&);

    static var isNoteOn(const var::NativeFunctionArgs&);
    static var isNoteOff(const var::NativeFunctionArgs&);
    static var isNoteOnOrOff(const var::NativeFunctionArgs&);
    static var getNoteNumber(const var::NativeFunctionArgs&);
    static var getVelocity(const var::NativeFunctionArgs&);
    static var getFloatVelocity(const var::NativeFunctionArgs&);

    static var isSustainPedalOn(const var::NativeFunctionArgs&);
    static var isSustainPedalOff(const var::NativeFunctionArgs&);
    static var isSostenutoPedalOn(const var::NativeFunctionArgs&);
    static var isSostenutoPedalOff(const var::NativeFunctionArgs&);
    static var isSoftPedalOn(const var::NativeFunctionArgs&);
    static var isSoftPedalOff(const var::NativeFunctionArgs&);

    static var isProgramChange(const var::NativeFunctionArgs&);
    static var getProgramChangeNumber(const var::NativeFunctionArgs&);

    static var isPitchWheel(const var::NativeFunctionArgs&);
    static var getPitchWheelValue(const var::NativeFunctionArgs&);

    static var isAftertouch(const var::NativeFunctionArgs&);
    static var getAfterTouchValue(const var::NativeFunctionArgs&);

    static var isChannelPressure(const var::NativeFunctionArgs&);
    static var getChannelPressureValue(const var::NativeFunctionArgs&);

    static var isController(const var::NativeFunctionArgs&);
    static var getControllerNumber(const var::NativeFunctionArgs&);
    static var getControllerValue(const var::NativeFunctionArgs&);

    static var isAllNotesOff(const var::NativeFunctionArgs&);
    static var isAllSoundOff(const var::NativeFunctionArgs&);
    static var isResetAllControllers(const var::NativeFunctionArgs&);

    static var isActiveSense(const var::NativeFunctionArgs&);
    static var isMidiStart(const var::NativeFunctionArgs&);
    static var isMidiContinue(const var::NativeFunctionArgs&);
    static var isMidiStop(const var::NativeFunctionArgs&);
    static var isMidiClock(const var::NativeFunctionArgs&);
    static var isSongPositionPointer(const var::NativeFunctionArgs&);
    static var getSongPositionPointerMidiBeat(const var::NativeFunctionArgs&);

    // mutators used by transform scripts to rewrite the forwarded message
    static var setChannel(const var::NativeFunctionArgs&);
    static var setNote(const var::NativeFunctionArgs&);
    static var setVelocity(const var::NativeFunctionArgs&);
    static var setControllerNumber(const var::NativeFunctionArgs&);
    static var setControllerValue(const var::NativeFunctionArgs&);
    static var setProgram(const var::NativeFunctionArgs&);
    static var setPitchBend(const var::NativeFunctionArgs&);
    static var setChannelPressure(const var::NativeFunctionArgs&);
    static var setPolyPressure(const var::NativeFunctionArgs&);
    static var block(const var::NativeFunctionArgs&);

    // emit extra messages alongside (or instead of) the current one
    static var sendNoteOn(const var::NativeFunctionArgs&);
    static var sendNoteOff(const var::NativeFunctionArgs&);
    static var sendController(const var::NativeFunctionArgs&);
    static var sendProgramChange(const var::NativeFunctionArgs&);
    static var sendPitchBend(const var::NativeFunctionArgs&);
    static var sendChannelPressure(const var::NativeFunctionArgs&);
    static var sendPolyPressure(const var::NativeFunctionArgs&);
    static var sendSysEx(const var::NativeFunctionArgs&);
    static var send(const var::NativeFunctionArgs&);

private:
    ApplicationState& applicationState_;
    MidiMessage msg_;
    bool blocked_ { false };
    Array<MidiMessage> emitted_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScriptMidiMessageClass)
};
