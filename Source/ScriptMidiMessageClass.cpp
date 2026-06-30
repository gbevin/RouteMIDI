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

#include "ScriptMidiMessageClass.h"

#include "ApplicationState.h"

ScriptMidiMessageClass::ScriptMidiMessageClass(ApplicationState& state) : applicationState_(state)
{
    setMethod("getRawData", getRawData);
    setMethod("rawData", getRawData);
    setMethod("getRawDataSize", getRawDataSize);
    setMethod("rawDataSize", getRawDataSize);

    setMethod("getDescription", getDescription);
    setMethod("description", getDescription);

    setMethod("getTimeStamp", getTimeStamp);
    setMethod("timeStamp", getTimeStamp);
    setMethod("getChannel", getChannel);
    setMethod("channel", getChannel);

    setMethod("isSysEx", isSysEx);
    setMethod("getSysExData", getSysExData);
    setMethod("sysExData", getSysExData);
    setMethod("getSysExDataSize", getSysExDataSize);
    setMethod("sysExDataSize", getSysExDataSize);

    setMethod("isNoteOn", isNoteOn);
    setMethod("isNoteOff", isNoteOff);
    setMethod("isNoteOnOrOff", isNoteOnOrOff);
    setMethod("getNoteNumber", getNoteNumber);
    setMethod("noteNumber", getNoteNumber);
    setMethod("getVelocity", getVelocity);
    setMethod("velocity", getVelocity);
    setMethod("getFloatVelocity", getFloatVelocity);
    setMethod("floatVelocity", getFloatVelocity);

    setMethod("isSustainPedalOn", isSustainPedalOn);
    setMethod("isSustainPedalOff", isSustainPedalOff);
    setMethod("isSostenutoPedalOn", isSostenutoPedalOn);
    setMethod("isSostenutoPedalOff", isSostenutoPedalOff);
    setMethod("isSoftPedalOn", isSoftPedalOn);
    setMethod("isSoftPedalOff", isSoftPedalOff);

    setMethod("isProgramChange", isProgramChange);
    setMethod("getProgramChangeNumber", getProgramChangeNumber);
    setMethod("programChange", getProgramChangeNumber);

    setMethod("isPitchWheel", isPitchWheel);
    setMethod("isPitchBend", isPitchWheel);
    setMethod("getPitchWheelValue", getPitchWheelValue);
    setMethod("getPitchBendValue", getPitchWheelValue);
    setMethod("pitchWheel", getPitchWheelValue);
    setMethod("pitchBend", getPitchWheelValue);

    setMethod("isAftertouch", isAftertouch);
    setMethod("isPolyPressure", isAftertouch);
    setMethod("getAfterTouchValue", getAfterTouchValue);
    setMethod("getPolyPressureValue", getAfterTouchValue);
    setMethod("afterTouch", getAfterTouchValue);
    setMethod("polyPressure", getAfterTouchValue);

    setMethod("isChannelPressure", isChannelPressure);
    setMethod("getChannelPressureValue", getChannelPressureValue);
    setMethod("channelPressure", getChannelPressureValue);

    setMethod("isController", isController);
    setMethod("getControllerNumber", getControllerNumber);
    setMethod("controllerNumber", getControllerNumber);
    setMethod("getControllerValue", getControllerValue);
    setMethod("controllerValue", getControllerValue);

    setMethod("isAllNotesOff", isAllNotesOff);
    setMethod("isAllSoundOff", isAllSoundOff);
    setMethod("isResetAllControllers", isResetAllControllers);

    setMethod("isActiveSense", isActiveSense);
    setMethod("isMidiStart", isMidiStart);
    setMethod("isMidiContinue", isMidiContinue);
    setMethod("isMidiStop", isMidiStop);
    setMethod("isMidiClock", isMidiClock);
    setMethod("isSongPositionPointer", isSongPositionPointer);
    setMethod("getSongPositionPointerMidiBeat", getSongPositionPointerMidiBeat);
    setMethod("songPositionPointerMidiBeat", getSongPositionPointerMidiBeat);

    setMethod("setChannel", setChannel);
    setMethod("setNote", setNote);
    setMethod("setNoteNumber", setNote);
    setMethod("setVelocity", setVelocity);
    setMethod("setControllerNumber", setControllerNumber);
    setMethod("setControllerValue", setControllerValue);
    setMethod("setProgram", setProgram);
    setMethod("setProgramChangeNumber", setProgram);
    setMethod("setPitchBend", setPitchBend);
    setMethod("setPitchWheel", setPitchBend);
    setMethod("setChannelPressure", setChannelPressure);
    setMethod("setPolyPressure", setPolyPressure);
    setMethod("setAfterTouch", setPolyPressure);
    setMethod("block", block);
    setMethod("drop", block);

    setMethod("sendNoteOn", sendNoteOn);
    setMethod("sendNoteOff", sendNoteOff);
    setMethod("sendController", sendController);
    setMethod("sendControlChange", sendController);
    setMethod("sendProgramChange", sendProgramChange);
    setMethod("sendPitchBend", sendPitchBend);
    setMethod("sendPitchWheel", sendPitchBend);
    setMethod("sendChannelPressure", sendChannelPressure);
    setMethod("sendPolyPressure", sendPolyPressure);
    setMethod("sendAfterTouch", sendPolyPressure);
    setMethod("sendSysEx", sendSysEx);
    setMethod("send", send);
}

void ScriptMidiMessageClass::setMidiMessage(MidiMessage msg)                                    { msg_ = msg; blocked_ = false; emitted_.clearQuick(); }

MidiMessage& ScriptMidiMessageClass::getMsg(const var::NativeFunctionArgs& a)                   { return ((ScriptMidiMessageClass*)a.thisObject.getDynamicObject())->msg_; }
ScriptMidiMessageClass& ScriptMidiMessageClass::getSelf(const var::NativeFunctionArgs& a)       { return *((ScriptMidiMessageClass*)a.thisObject.getDynamicObject()); }
ApplicationState& ScriptMidiMessageClass::getApplicationState(const var::NativeFunctionArgs& a) { return ((ScriptMidiMessageClass*)a.thisObject.getDynamicObject())->applicationState_; }

var ScriptMidiMessageClass::getRawDataSize(const var::NativeFunctionArgs& a)                    { return getMsg(a).getRawDataSize(); }
var ScriptMidiMessageClass::getDescription(const var::NativeFunctionArgs& a)                    { return getMsg(a).getDescription(); }
var ScriptMidiMessageClass::getTimeStamp(const var::NativeFunctionArgs& a)                      { return getMsg(a).getTimeStamp(); }
var ScriptMidiMessageClass::getChannel(const var::NativeFunctionArgs& a)                        { return getMsg(a).getChannel(); }

var ScriptMidiMessageClass::isSysEx(const var::NativeFunctionArgs& a)                           { return getMsg(a).isSysEx(); }
var ScriptMidiMessageClass::getSysExDataSize(const var::NativeFunctionArgs& a)                  { return getMsg(a).getSysExDataSize(); }

var ScriptMidiMessageClass::isNoteOn(const var::NativeFunctionArgs& a)                          { return getMsg(a).isNoteOn(); }
var ScriptMidiMessageClass::isNoteOff(const var::NativeFunctionArgs& a)                         { return getMsg(a).isNoteOff(); }
var ScriptMidiMessageClass::isNoteOnOrOff(const var::NativeFunctionArgs& a)                     { return getMsg(a).isNoteOnOrOff(); }
var ScriptMidiMessageClass::getNoteNumber(const var::NativeFunctionArgs& a)                     { return getMsg(a).getNoteNumber(); }
var ScriptMidiMessageClass::getVelocity(const var::NativeFunctionArgs& a)                       { return getMsg(a).getVelocity(); }
var ScriptMidiMessageClass::getFloatVelocity(const var::NativeFunctionArgs& a)                  { return getMsg(a).getFloatVelocity(); }

var ScriptMidiMessageClass::isSustainPedalOn(const var::NativeFunctionArgs& a)                  { return getMsg(a).isSustainPedalOn(); }
var ScriptMidiMessageClass::isSustainPedalOff(const var::NativeFunctionArgs& a)                 { return getMsg(a).isSustainPedalOff(); }
var ScriptMidiMessageClass::isSostenutoPedalOn(const var::NativeFunctionArgs& a)                { return getMsg(a).isSostenutoPedalOn(); }
var ScriptMidiMessageClass::isSostenutoPedalOff(const var::NativeFunctionArgs& a)               { return getMsg(a).isSostenutoPedalOff(); }
var ScriptMidiMessageClass::isSoftPedalOn(const var::NativeFunctionArgs& a)                     { return getMsg(a).isSoftPedalOn(); }
var ScriptMidiMessageClass::isSoftPedalOff(const var::NativeFunctionArgs& a)                    { return getMsg(a).isSoftPedalOff(); }

var ScriptMidiMessageClass::isProgramChange(const var::NativeFunctionArgs& a)                   { return getMsg(a).isProgramChange(); }
var ScriptMidiMessageClass::getProgramChangeNumber(const var::NativeFunctionArgs& a)            { return getMsg(a).getProgramChangeNumber(); }

var ScriptMidiMessageClass::isPitchWheel(const var::NativeFunctionArgs& a)                      { return getMsg(a).isPitchWheel(); }
var ScriptMidiMessageClass::getPitchWheelValue(const var::NativeFunctionArgs& a)                { return getMsg(a).getPitchWheelValue(); }

var ScriptMidiMessageClass::isAftertouch(const var::NativeFunctionArgs& a)                      { return getMsg(a).isAftertouch(); }
var ScriptMidiMessageClass::getAfterTouchValue(const var::NativeFunctionArgs& a)                { return getMsg(a).getAfterTouchValue(); }

var ScriptMidiMessageClass::isChannelPressure(const var::NativeFunctionArgs& a)                 { return getMsg(a).isChannelPressure(); }
var ScriptMidiMessageClass::getChannelPressureValue(const var::NativeFunctionArgs& a)           { return getMsg(a).getChannelPressureValue(); }

var ScriptMidiMessageClass::isController(const var::NativeFunctionArgs& a)                      { return getMsg(a).isController(); }
var ScriptMidiMessageClass::getControllerNumber(const var::NativeFunctionArgs& a)               { return getMsg(a).getControllerNumber(); }
var ScriptMidiMessageClass::getControllerValue(const var::NativeFunctionArgs& a)                { return getMsg(a).getControllerValue(); }

var ScriptMidiMessageClass::isAllNotesOff(const var::NativeFunctionArgs& a)                     { return getMsg(a).isAllNotesOff(); }
var ScriptMidiMessageClass::isAllSoundOff(const var::NativeFunctionArgs& a)                     { return getMsg(a).isAllSoundOff(); }
var ScriptMidiMessageClass::isResetAllControllers(const var::NativeFunctionArgs& a)             { return getMsg(a).isResetAllControllers(); }

var ScriptMidiMessageClass::isActiveSense(const var::NativeFunctionArgs& a)                     { return getMsg(a).isActiveSense(); }
var ScriptMidiMessageClass::isMidiStart(const var::NativeFunctionArgs& a)                       { return getMsg(a).isMidiStart(); }
var ScriptMidiMessageClass::isMidiContinue(const var::NativeFunctionArgs& a)                    { return getMsg(a).isMidiContinue(); }
var ScriptMidiMessageClass::isMidiStop(const var::NativeFunctionArgs& a)                        { return getMsg(a).isMidiStop(); }
var ScriptMidiMessageClass::isMidiClock(const var::NativeFunctionArgs& a)                       { return getMsg(a).isMidiClock(); }
var ScriptMidiMessageClass::isSongPositionPointer(const var::NativeFunctionArgs& a)             { return getMsg(a).isSongPositionPointer(); }
var ScriptMidiMessageClass::getSongPositionPointerMidiBeat(const var::NativeFunctionArgs& a)    { return getMsg(a).getSongPositionPointerMidiBeat(); }

var ScriptMidiMessageClass::getRawData(const var::NativeFunctionArgs& a)
{
    Array<var> data;

    const uint8* raw = getMsg(a).getRawData();
    int size = getMsg(a).getRawDataSize();
    for (int i = 0; i < size; ++i)
    {
        data.add(raw[i]);
    }

    return data;
}

var ScriptMidiMessageClass::getSysExData(const var::NativeFunctionArgs& a)
{
    Array<var> data;

    const uint8* raw = getMsg(a).getSysExData();
    int size = getMsg(a).getSysExDataSize();
    for (int i = 0; i < size; ++i)
    {
        data.add(raw[i]);
    }

    return data;
}

var ScriptMidiMessageClass::setChannel(const var::NativeFunctionArgs& a)
{
    MidiMessage& msg = getMsg(a);
    if (a.numArguments > 0 && msg.getChannel() > 0)
    {
        msg.setChannel(jlimit(1, 16, (int)a.arguments[0]));
    }
    return var::undefined();
}

var ScriptMidiMessageClass::setNote(const var::NativeFunctionArgs& a)
{
    MidiMessage& msg = getMsg(a);
    if (a.numArguments > 0 && (msg.isNoteOnOrOff() || msg.isAftertouch()))
    {
        msg.setNoteNumber(jlimit(0, 127, (int)a.arguments[0]));
    }
    return var::undefined();
}

var ScriptMidiMessageClass::setVelocity(const var::NativeFunctionArgs& a)
{
    MidiMessage& msg = getMsg(a);
    if (a.numArguments > 0 && msg.isNoteOnOrOff())
    {
        const double timestamp = msg.getTimeStamp();
        int v = jlimit(0, 127, (int)a.arguments[0]);
        if (msg.isNoteOn() && v > 0)
        {
            msg = MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(), (uint8)v);
        }
        else
        {
            msg = MidiMessage::noteOff(msg.getChannel(), msg.getNoteNumber(), (uint8)v);
        }
        msg.setTimeStamp(timestamp);
    }
    return var::undefined();
}

var ScriptMidiMessageClass::setControllerValue(const var::NativeFunctionArgs& a)
{
    MidiMessage& msg = getMsg(a);
    if (a.numArguments > 0 && msg.isController())
    {
        const double timestamp = msg.getTimeStamp();
        msg = MidiMessage::controllerEvent(msg.getChannel(), msg.getControllerNumber(),
                                           jlimit(0, 127, (int)a.arguments[0]));
        msg.setTimeStamp(timestamp);
    }
    return var::undefined();
}

var ScriptMidiMessageClass::setControllerNumber(const var::NativeFunctionArgs& a)
{
    MidiMessage& msg = getMsg(a);
    if (a.numArguments > 0 && msg.isController())
    {
        const double timestamp = msg.getTimeStamp();
        msg = MidiMessage::controllerEvent(msg.getChannel(), jlimit(0, 127, (int)a.arguments[0]),
                                           msg.getControllerValue());
        msg.setTimeStamp(timestamp);
    }
    return var::undefined();
}

var ScriptMidiMessageClass::setProgram(const var::NativeFunctionArgs& a)
{
    MidiMessage& msg = getMsg(a);
    if (a.numArguments > 0 && msg.isProgramChange())
    {
        const double timestamp = msg.getTimeStamp();
        msg = MidiMessage::programChange(msg.getChannel(), jlimit(0, 127, (int)a.arguments[0]));
        msg.setTimeStamp(timestamp);
    }
    return var::undefined();
}

var ScriptMidiMessageClass::setPitchBend(const var::NativeFunctionArgs& a)
{
    MidiMessage& msg = getMsg(a);
    if (a.numArguments > 0 && msg.isPitchWheel())
    {
        const double timestamp = msg.getTimeStamp();
        msg = MidiMessage::pitchWheel(msg.getChannel(), jlimit(0, 16383, (int)a.arguments[0]));
        msg.setTimeStamp(timestamp);
    }
    return var::undefined();
}

var ScriptMidiMessageClass::setChannelPressure(const var::NativeFunctionArgs& a)
{
    MidiMessage& msg = getMsg(a);
    if (a.numArguments > 0 && msg.isChannelPressure())
    {
        const double timestamp = msg.getTimeStamp();
        msg = MidiMessage::channelPressureChange(msg.getChannel(), jlimit(0, 127, (int)a.arguments[0]));
        msg.setTimeStamp(timestamp);
    }
    return var::undefined();
}

var ScriptMidiMessageClass::setPolyPressure(const var::NativeFunctionArgs& a)
{
    MidiMessage& msg = getMsg(a);
    if (a.numArguments > 0 && msg.isAftertouch())
    {
        const double timestamp = msg.getTimeStamp();
        msg = MidiMessage::aftertouchChange(msg.getChannel(), msg.getNoteNumber(),
                                            jlimit(0, 127, (int)a.arguments[0]));
        msg.setTimeStamp(timestamp);
    }
    return var::undefined();
}

var ScriptMidiMessageClass::block(const var::NativeFunctionArgs& a)
{
    getSelf(a).blocked_ = true;
    return var::undefined();
}

static int argInt(const var::NativeFunctionArgs& a, int index, int fallback = 0)
{
    return a.numArguments > index ? (int)a.arguments[index] : fallback;
}

var ScriptMidiMessageClass::sendNoteOn(const var::NativeFunctionArgs& a)
{
    getSelf(a).emitted_.add(MidiMessage::noteOn(jlimit(1, 16, argInt(a, 0, 1)),
                                                jlimit(0, 127, argInt(a, 1)),
                                                (uint8)jlimit(0, 127, argInt(a, 2))));
    return var::undefined();
}

var ScriptMidiMessageClass::sendNoteOff(const var::NativeFunctionArgs& a)
{
    getSelf(a).emitted_.add(MidiMessage::noteOff(jlimit(1, 16, argInt(a, 0, 1)),
                                                 jlimit(0, 127, argInt(a, 1)),
                                                 (uint8)jlimit(0, 127, argInt(a, 2))));
    return var::undefined();
}

var ScriptMidiMessageClass::sendController(const var::NativeFunctionArgs& a)
{
    getSelf(a).emitted_.add(MidiMessage::controllerEvent(jlimit(1, 16, argInt(a, 0, 1)),
                                                         jlimit(0, 127, argInt(a, 1)),
                                                         jlimit(0, 127, argInt(a, 2))));
    return var::undefined();
}

var ScriptMidiMessageClass::sendProgramChange(const var::NativeFunctionArgs& a)
{
    getSelf(a).emitted_.add(MidiMessage::programChange(jlimit(1, 16, argInt(a, 0, 1)),
                                                       jlimit(0, 127, argInt(a, 1))));
    return var::undefined();
}

var ScriptMidiMessageClass::sendPitchBend(const var::NativeFunctionArgs& a)
{
    getSelf(a).emitted_.add(MidiMessage::pitchWheel(jlimit(1, 16, argInt(a, 0, 1)),
                                                    jlimit(0, 16383, argInt(a, 1, 8192))));
    return var::undefined();
}

var ScriptMidiMessageClass::sendChannelPressure(const var::NativeFunctionArgs& a)
{
    getSelf(a).emitted_.add(MidiMessage::channelPressureChange(jlimit(1, 16, argInt(a, 0, 1)),
                                                               jlimit(0, 127, argInt(a, 1))));
    return var::undefined();
}

var ScriptMidiMessageClass::sendPolyPressure(const var::NativeFunctionArgs& a)
{
    getSelf(a).emitted_.add(MidiMessage::aftertouchChange(jlimit(1, 16, argInt(a, 0, 1)),
                                                          jlimit(0, 127, argInt(a, 1)),
                                                          jlimit(0, 127, argInt(a, 2))));
    return var::undefined();
}

static Array<uint8> bytesFromArgs(const var::NativeFunctionArgs& a)
{
    Array<uint8> bytes;
    // accept either a single array argument or a list of byte arguments
    if (a.numArguments == 1 && a.arguments[0].isArray())
    {
        for (const auto& b : *a.arguments[0].getArray())
        {
            bytes.add((uint8)(int)b);
        }
    }
    else
    {
        for (int i = 0; i < a.numArguments; ++i)
        {
            bytes.add((uint8)(int)a.arguments[i]);
        }
    }
    return bytes;
}

var ScriptMidiMessageClass::sendSysEx(const var::NativeFunctionArgs& a)
{
    Array<uint8> bytes = bytesFromArgs(a);
    if (!bytes.isEmpty())
    {
        getSelf(a).emitted_.add(MidiMessage::createSysExMessage(bytes.getRawDataPointer(), bytes.size()));
    }
    return var::undefined();
}

var ScriptMidiMessageClass::send(const var::NativeFunctionArgs& a)
{
    Array<uint8> bytes = bytesFromArgs(a);
    if (!bytes.isEmpty())
    {
        getSelf(a).emitted_.add(MidiMessage(bytes.getRawDataPointer(), bytes.size()));
    }
    return var::undefined();
}
