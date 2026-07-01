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

#include "ApplicationState.h"

#include "BitScaling.h"
#include "ScriptOscClass.h"
#include "ScriptUtilClass.h"

static const int DEFAULT_OCTAVE_MIDDLE_C = 3;
static const String DEFAULT_VIRTUAL_IN_NAME = "RouteMIDI In";
static const String DEFAULT_VIRTUAL_OUT_NAME = "RouteMIDI Out";

//==============================================================================
// Inter-conversion between controller value types. cc, cc14, rpn and nrpn take a
// number argument selecting which controller or parameter; pb, cp and pc have a
// single value per channel and take no number.

enum ConvType { CONV_CC7, CONV_CC14, CONV_RPN, CONV_NRPN, CONV_PB, CONV_CP, CONV_PC };

static bool parseConvType(const String& s, ConvType& out)
{
    if (s.equalsIgnoreCase("cc"))   { out = CONV_CC7;  return true; }
    if (s.equalsIgnoreCase("cc14")) { out = CONV_CC14; return true; }
    if (s.equalsIgnoreCase("rpn"))  { out = CONV_RPN;  return true; }
    if (s.equalsIgnoreCase("nrpn")) { out = CONV_NRPN; return true; }
    if (s.equalsIgnoreCase("pb"))   { out = CONV_PB;   return true; }
    if (s.equalsIgnoreCase("cp"))   { out = CONV_CP;   return true; }
    if (s.equalsIgnoreCase("pc"))   { out = CONV_PC;   return true; }
    return false;
}

// whether a type takes a number argument
static bool typeNeedsNumber(ConvType type)
{
    return type == CONV_CC7 || type == CONV_CC14 || type == CONV_RPN || type == CONV_NRPN;
}

// given the options collected so far for a convert command (the grammar is
// "srctype [num] dsttype [num]"), returns true once all required tokens are present
static bool convertSpecComplete(const StringArray& opts)
{
    if (opts.size() < 1) return false;
    ConvType t;
    const bool srcNeedsNum = parseConvType(opts[0], t) && typeNeedsNumber(t);
    const int dstTypeIndex = srcNeedsNum ? 2 : 1;
    if (opts.size() <= dstTypeIndex) return false;
    const bool dstNeedsNum = parseConvType(opts[dstTypeIndex], t) && typeNeedsNumber(t);
    const int required = dstTypeIndex + 1 + (dstNeedsNum ? 1 : 0);
    return opts.size() >= required;
}

// the bit resolution of a type's value (pitch bend and the 14-bit parameter
// types are 14-bit, everything else is 7-bit)
static int valueBitsFor(ConvType type)
{
    return (type == CONV_CC14 || type == CONV_RPN || type == CONV_NRPN || type == CONV_PB) ? 14 : 7;
}

// returns the scaling method for a conversion: Zero-Extension when either side
// is an RPN whose parameter LSB is 0-31 (an absolute MIDI 1.0 RPN), else
// Min-Center-Max
static bitscaling::ScaleMethod scaleMethodFor(ConvType srcType, int srcNum, ConvType dstType, int dstNum)
{
    auto isLegacyRpn = [](ConvType t, int num) { return t == CONV_RPN && (num & 0x7f) <= 31; };
    return (isLegacyRpn(srcType, srcNum) || isLegacyRpn(dstType, dstNum))
        ? bitscaling::ZeroExtension : bitscaling::MinCenterMax;
}

// applies a gamma curve to a value in [0, maxValue]: gamma < 1 boosts low values
// (concave), gamma > 1 attenuates them (convex), gamma 1 is linear
static int gammaCurve(int value, int maxValue, double gamma)
{
    if (gamma <= 0.0 || maxValue <= 0)
    {
        return value;
    }
    const double normalized = jlimit(0.0, 1.0, (double) value / (double) maxValue);
    return jlimit(0, maxValue, roundToInt(std::pow(normalized, gamma) * maxValue));
}

// true for the six RPN/NRPN value-transform commands handled in the converter stage
static bool isRpnTransform(CommandIndex c)
{
    return c == NRPN_ADD || c == NRPN_SCALE || c == NRPN_CURVE ||
           c == RPN_ADD  || c == RPN_SCALE  || c == RPN_CURVE;
}

// emit the destination representation of a parameter value, scaling the bit
// width as needed; appends an RPN/NRPN null for (N)RPN destinations
static void emitConversion(Array<MidiMessage>& out, int channel, int srcValue, int srcBits,
                           ConvType dstType, int dstNum, bitscaling::ScaleMethod method, double timestamp)
{
    const int v = bitscaling::scaleValue(srcValue, srcBits, valueBitsFor(dstType), method);

    switch (dstType)
    {
        case CONV_CC7:
        {
            auto m = MidiMessage::controllerEvent(channel, dstNum & 0x7f, v & 0x7f);
            m.setTimeStamp(timestamp);
            out.add(m);
            break;
        }
        case CONV_CC14:
        {
            const int n = dstNum & 0x1f;
            auto msb = MidiMessage::controllerEvent(channel, n, (v >> 7) & 0x7f);
            msb.setTimeStamp(timestamp);
            out.add(msb);
            auto lsb = MidiMessage::controllerEvent(channel, n + 32, v & 0x7f);
            lsb.setTimeStamp(timestamp);
            out.add(lsb);
            break;
        }
        case CONV_RPN:
        case CONV_NRPN:
        {
            const bool isNRPN = (dstType == CONV_NRPN);
            MidiBuffer buffer = MidiRPNGenerator::generate(channel, dstNum & 0x3fff, v & 0x3fff, isNRPN, true);
            for (const auto meta : buffer)
            {
                auto m = meta.getMessage();
                m.setTimeStamp(timestamp);
                out.add(m);
            }
            // RPN/NRPN null to deselect the parameter (MA07 / MIDI 1.0 best practice)
            auto a = MidiMessage::controllerEvent(channel, isNRPN ? 99 : 101, 127);
            a.setTimeStamp(timestamp);
            out.add(a);
            auto b = MidiMessage::controllerEvent(channel, isNRPN ? 98 : 100, 127);
            b.setTimeStamp(timestamp);
            out.add(b);
            break;
        }
        case CONV_PB:
        {
            auto m = MidiMessage::pitchWheel(channel, jlimit(0, 16383, v));
            m.setTimeStamp(timestamp);
            out.add(m);
            break;
        }
        case CONV_CP:
        {
            auto m = MidiMessage::channelPressureChange(channel, jlimit(0, 127, v));
            m.setTimeStamp(timestamp);
            out.add(m);
            break;
        }
        case CONV_PC:
        {
            auto m = MidiMessage::programChange(channel, jlimit(0, 127, v));
            m.setTimeStamp(timestamp);
            out.add(m);
            break;
        }
    }
}

ApplicationState::ApplicationState()
{
    commands_.add({"in",        "input",                  INPUT,                   1, {"name"},             {"Add a MIDI input (- for stdin text); a new route starts after outputs"}});
    commands_.add({"out",       "output",                 OUTPUT,                  1, {"name"},             {"Add a MIDI output to the route (- for stdout text)"}});
    commands_.add({"vin",       "virtual-in",             VIRTUAL_IN,             -1, {"(name)"},           {"Add a virtual MIDI input to the route (Linux/macOS)"}});
    commands_.add({"vout",      "virtual-out",            VIRTUAL_OUT,            -1, {"(name)"},           {"Add a virtual MIDI output to the route (Linux/macOS)"}});
    commands_.add({"list",      "",                       LIST,                    0, {""},                 {"List the available MIDI input and output ports"}});
    commands_.add({"panic",     "",                       PANIC,                   0, {""},                 {"Send all-notes-off on disconnect, exit and zone change"}});
    commands_.add({"file",      "",                       TXTFILE,                 1, {"path"},             {"Load commands from the specified program file"}});
    commands_.add({"dec",       "decimal",                DECIMAL,                 0, {""},                 {"Interpret the next numbers as decimals by default"}});
    commands_.add({"hex",       "hexadecimal",            HEXADECIMAL,             0, {""},                 {"Interpret the next numbers as hexadecimals by default"}});
    commands_.add({"omc",       "octave-middle-c",        OCTAVE_MIDDLE_C,         1, {"number"},           {"Set octave for middle C, defaults to 3"}});
    commands_.add({"nn",        "note-numbers",           NOTE_NUMBERS,            0, {""},                 {"Monitor notes as numbers instead of names"}});
    commands_.add({"ts",        "timestamp",              TIMESTAMP,               0, {""},                 {"Prefix monitored messages with a timestamp"}});
    commands_.add({"mon",       "verbose",                MONITOR,                 0, {""},                 {"Print each routed message (quiet by default)"}});
    commands_.add({"src",       "monitor-source",         MONITOR_SOURCE,          0, {""},                 {"Prefix monitored messages with the input port name"}});
    commands_.add({"not",       "",                       NOT,                     0, {""},                 {"Negate the next filter, blocking matching messages"}});
    commands_.add({"ch",        "channel",                CHANNEL,                 1, {"number"},           {"Restrict the route to a MIDI channel (1-16)"}});
    commands_.add({"voice",     "",                       VOICE,                   0, {""},                 {"Pass all Channel Voice messages"}});
    commands_.add({"note",      "",                       NOTE,                    0, {""},                 {"Pass all Note messages"}});
    commands_.add({"on",        "note-on",                NOTE_ON,                -1, {"(note)"},           {"Pass Note On, optionally for note (0-127)"}});
    commands_.add({"off",       "note-off",               NOTE_OFF,               -1, {"(note)"},           {"Pass Note Off, optionally for note (0-127)"}});
    commands_.add({"pp",        "poly-pressure",          POLY_PRESSURE,          -1, {"(note)"},           {"Pass Poly Pressure, optionally for note (0-127)"}});
    commands_.add({"cc",        "control-change",         CONTROL_CHANGE,         -1, {"(number)"},         {"Pass Control Change, optionally for controller (0-127)"}});
    commands_.add({"cc14",      "control-change-14",      CONTROL_CHANGE_14BIT,   -1, {"(number)"},         {"Pass 14-bit CC, optionally for MSB controller (0-31)"}});
    commands_.add({"nrpn",      "",                       NRPN,                    0, {""},                 {"Pass NRPN traffic (CC 6, 38, 98, 99)"}});
    commands_.add({"rpn",       "",                       RPN,                     0, {""},                 {"Pass RPN traffic (CC 6, 38, 100, 101)"}});
    commands_.add({"pc",        "program-change",         PROGRAM_CHANGE,         -1, {"(number)"},         {"Pass Program Change, optionally for program (0-127)"}});
    commands_.add({"cp",        "channel-pressure",       CHANNEL_PRESSURE,        0, {""},                 {"Pass Channel Pressure"}});
    commands_.add({"pb",        "pitch-bend",             PITCH_BEND,              0, {""},                 {"Pass Pitch Bend"}});
    commands_.add({"sr",        "system-realtime",        SYSTEM_REALTIME,         0, {""},                 {"Pass all System Real-Time messages"}});
    commands_.add({"clock",     "",                       CLOCK,                   0, {""},                 {"Pass Timing Clock"}});
    commands_.add({"start",     "",                       START,                   0, {""},                 {"Pass Start"}});
    commands_.add({"stop",      "",                       STOP,                    0, {""},                 {"Pass Stop"}});
    commands_.add({"cont",      "continue",               CONTINUE,                0, {""},                 {"Pass Continue"}});
    commands_.add({"as",        "active-sensing",         ACTIVE_SENSING,          0, {""},                 {"Pass Active Sensing"}});
    commands_.add({"rst",       "reset",                  RESET,                   0, {""},                 {"Pass Reset"}});
    commands_.add({"sc",        "system-common",          SYSTEM_COMMON,           0, {""},                 {"Pass all System Common messages"}});
    commands_.add({"syx",       "system-exclusive",       SYSTEM_EXCLUSIVE,        0, {""},                 {"Pass System Exclusive"}});
    commands_.add({"syf",       "system-exclusive-file",  SYSEX_FILE,              1, {"path"},             {"Capture routed System Exclusive to a .syx file"}});
    commands_.add({"tc",        "time-code",              TIME_CODE,               0, {""},                 {"Pass MIDI Time Code Quarter Frame"}});
    commands_.add({"spp",       "song-position",          SONG_POSITION,           0, {""},                 {"Pass Song Position Pointer"}});
    commands_.add({"ss",        "song-select",            SONG_SELECT,             0, {""},                 {"Pass Song Select"}});
    commands_.add({"tun",       "tune-request",           TUNE_REQUEST,            0, {""},                 {"Pass Tune Request"}});
    commands_.add({"noterange", "note-range",             NOTE_RANGE,              2, {"low", "high"},      {"Pass notes within a note range (key split)"}});
    commands_.add({"velrange",  "velocity-range",         VELOCITY_RANGE,          2, {"low", "high"},      {"Pass note-ons within a velocity range (vel split)"}});
    commands_.add({"mpemaster", "mpe-master",             MPE_MASTER,              1, {"zone[:n]"},         {"Pass the master channel of an MPE zone (e.g. lower)"}});
    commands_.add({"mpemember", "mpe-member",             MPE_MEMBER,              1, {"zone[:n]"},         {"Pass the member channels of an MPE zone (e.g. upper:7)"}});
    commands_.add({"mpezone",   "mpe-zone",               MPE_ZONE,                1, {"zone[:n]"},         {"Pass a whole MPE zone (its master and member channels)"}});
    commands_.add({"chmap",     "channel-map",            CHANNEL_MAP,             2, {"from", "to"},       {"Remap channel-voice messages from one channel to another"}});
    commands_.add({"chset",     "channel-set",            CHANNEL_SET,             1, {"number"},           {"Force all channel-voice messages onto a channel"}});
    commands_.add({"chadd",     "channel-add",            CHANNEL_ADD,             1, {"number"},           {"Add N to the channel, wrapping 1-16 (may be negative)"}});
    commands_.add({"transp",    "transpose",              TRANSPOSE,               1, {"semitones"},        {"Transpose notes by N semitones (out-of-range dropped)"}});
    commands_.add({"notemap",   "note-map",               NOTE_MAP,                2, {"from", "to"},       {"Remap a specific note number to another"}});
    commands_.add({"notecc",    "note-to-control-change", NOTE_TO_CC,              2, {"note", "cc"},       {"Turn a note into a Control Change (velocity as value)"}});
    commands_.add({"ccnote",    "control-change-to-note", CC_TO_NOTE,              2, {"cc", "note"},       {"Turn a Control Change into a note (64+ on, else off)"}});
    commands_.add({"notepc",    "note-to-program-change", NOTE_TO_PROGRAM,         2, {"note", "program"},  {"Turn a note-on into a Program Change (note-off dropped)"}});
    commands_.add({"velscale",  "velocity-scale",         VELOCITY_SCALE,          1, {"factor"},           {"Scale note-on velocity by a factor (clamped 1-127)"}});
    commands_.add({"velset",    "velocity-set",           VELOCITY_SET,            1, {"number"},           {"Set a fixed note-on velocity (1-127)"}});
    commands_.add({"veladd",    "velocity-add",           VELOCITY_ADD,            1, {"number"},           {"Add an offset to note-on velocity (clamped 1-127)"}});
    commands_.add({"velcurve",  "velocity-curve",         VELOCITY_CURVE,          1, {"gamma"},            {"Apply a gamma curve to note-on velocity (1-127)"}});
    commands_.add({"ccmap",     "control-change-map",     CONTROL_CHANGE_MAP,      2, {"from", "to"},       {"Remap a Control Change controller number"}});
    commands_.add({"ccadd",     "control-change-add",     CONTROL_CHANGE_ADD,      2, {"number", "value"},  {"Add an offset to a controller's value (clamped 0-127)"}});
    commands_.add({"ccscale",   "control-change-scale",   CONTROL_CHANGE_SCALE,    2, {"number", "factor"}, {"Scale a controller's value by a factor (clamped 0-127)"}});
    commands_.add({"cccurve",   "control-change-curve",   CONTROL_CHANGE_CURVE,    2, {"number", "gamma"},  {"Apply a gamma curve to a controller's value"}});
    commands_.add({"pcmap",     "program-change-map",     PROGRAM_CHANGE_MAP,      2, {"from", "to"},       {"Remap a Program Change number"}});
    commands_.add({"pcadd",     "program-change-add",     PROGRAM_CHANGE_ADD,      1, {"number"},           {"Add an offset to Program Change number (clamped 0-127)"}});
    commands_.add({"pbadd",     "pitch-bend-add",         PITCH_BEND_ADD,          1, {"number"},           {"Add an offset to Pitch Bend (clamped 0-16383)"}});
    commands_.add({"pbscale",   "pitch-bend-scale",       PITCH_BEND_SCALE,        1, {"factor"},           {"Scale Pitch Bend around center by a factor (0-16383)"}});
    commands_.add({"pbset",     "pitch-bend-set",         PITCH_BEND_SET,          1, {"number"},           {"Set a fixed Pitch Bend value (0-16383)"}});
    commands_.add({"cpadd",     "channel-pressure-add",   CHANNEL_PRESSURE_ADD,    1, {"number"},           {"Add an offset to Channel Pressure (clamped 0-127)"}});
    commands_.add({"cpscale",   "channel-pressure-scale", CHANNEL_PRESSURE_SCALE,  1, {"factor"},           {"Scale Channel Pressure by a factor (clamped 0-127)"}});
    commands_.add({"cpset",     "channel-pressure-set",   CHANNEL_PRESSURE_SET,    1, {"number"},           {"Set a fixed Channel Pressure value (0-127)"}});
    commands_.add({"cpcurve",   "channel-pressure-curve", CHANNEL_PRESSURE_CURVE,  1, {"gamma"},            {"Apply a gamma curve to Channel Pressure"}});
    commands_.add({"nrpnadd",   "nrpn-add",               NRPN_ADD,                2, {"param", "number"},  {"Add an offset to an NRPN value (clamped to its resolution)"}});
    commands_.add({"nrpnscale", "nrpn-scale",             NRPN_SCALE,              2, {"param", "factor"},  {"Scale an NRPN value by a factor (clamped to its resolution)"}});
    commands_.add({"nrpncurve", "nrpn-curve",             NRPN_CURVE,              2, {"param", "gamma"},   {"Apply a gamma curve to an NRPN value"}});
    commands_.add({"rpnadd",    "rpn-add",                RPN_ADD,                 2, {"param", "number"},  {"Add an offset to an RPN value (clamped to its resolution)"}});
    commands_.add({"rpnscale",  "rpn-scale",              RPN_SCALE,               2, {"param", "factor"},  {"Scale an RPN value by a factor (clamped to its resolution)"}});
    commands_.add({"rpncurve",  "rpn-curve",              RPN_CURVE,               2, {"param", "gamma"},   {"Apply a gamma curve to an RPN value"}});
    commands_.add({"js",        "javascript",             JAVASCRIPT,              1, {"code"},             {"Transform each message with this script"}});
    commands_.add({"jsf",       "javascript-file",        JAVASCRIPT_FILE,         1, {"path"},             {"Transform each message with the script in this file"}});
    commands_.add({"convert",   "",                       CONVERT,                 4, {"srctype", "[number]", "dsttype", "[number]"},
                                                                                                            {"Convert a value between cc, cc14, rpn, nrpn, pb, cp & pc. Types cc, cc14, rpn and nrpn take a number selecting the controller or parameter, pb, cp and pc do not, and the value is rescaled to the destination resolution"}});
    commands_.add({"mpe",       "",                       MPE_RELOCATE,            2, {"zone[:n]", "zone[:n]"},
                                                                                                            {"Relocate an MPE stream between zones (e.g. lower upper), remapping channels"}});
    commands_.add({"mpemono",   "mpe-mono",               MPE_COLLAPSE,            2, {"zone[:n]", "channel"},
                                                                                                            {"Collapse an MPE zone onto a single channel for non-MPE gear (e.g. upper 1)"}});
    commands_.add({"mpexp",     "mpe-expand",             MPE_EXPAND,              2, {"channel", "zone[:n]"},
                                                                                                            {"Spread a channel's notes across an MPE zone's member channels (e.g. 1 lower)"}});
    commands_.add({"mpesplit",  "mpe-split",              MPE_SPLIT,              -1, {"zone[:n]", "(channel)"},
                                                                                                            {"Distribute an MPE zone's voices over the output ports, one per port, each rechanneled to channel (default 1)"}});
    commands_.add({"mpebend",   "mpe-bend",               MPE_BEND,                3, {"zone[:n]", "from", "to"},
                                                                                                            {"Rescale member-channel pitch bend from one semitone range to another"}});
    commands_.add({"mpesens",   "mpe-sensitivity",        MPE_SENS,                2, {"zone[:n]", "semitones"},
                                                                                                            {"Declare a member-channel pitch bend range (RPN 0) for synths that honor it"}});

    pendingNegate_ = false;
    useHexadecimalsByDefault_ = false;
    noteNumbersOutput_ = false;
    timestampOutput_ = false;
    monitor_ = false;
    monitorShowSource_ = false;
    octaveMiddleC_ = DEFAULT_OCTAVE_MIDDLE_C;
    scriptMidiMessage_ = nullptr;
    hasScript_ = false;
    currentCommand_ = ApplicationCommand::Dummy();
}

void ApplicationState::initialise(JUCEApplicationBase& app)
{
    StringArray cmdLineParams(app.getCommandLineParameterArray());
    if (cmdLineParams.contains("--help") || cmdLineParams.contains("-h"))
    {
        printUsage();
        app.systemRequestedQuit();
        return;
    }
    else if (cmdLineParams.contains("--version"))
    {
        printVersion();
        app.systemRequestedQuit();
        return;
    }

    scriptEngine_.maximumExecutionTime = RelativeTime::days(365);
    scriptMidiMessage_ = new ScriptMidiMessageClass(*this);
    scriptEngine_.registerNativeObject("MIDI", scriptMidiMessage_);
    scriptEngine_.registerNativeObject("OSC",  new ScriptOscClass());
    scriptEngine_.registerNativeObject("Util", new ScriptUtilClass());

    parseParameters(cmdLineParams);

    if (cmdLineParams.contains("--"))
    {
        while (std::cin)
        {
            std::string line;
            getline(std::cin, line);
            StringArray params = parseLineAsParameters(line);
            parseParameters(params);
        }
    }

    if (cmdLineParams.isEmpty())
    {
        printUsage();
        app.systemRequestedQuit();
    }
    else if (routes_.isEmpty())
    {
        // nothing to route (for instance only "list" was requested)
        app.systemRequestedQuit();
    }
    else if (hasStdinInput())
    {
        // a "in -" source reads text MIDI from standard input until it closes,
        // then there is nothing left to route, so quit
        startTimer(200);
        readStdinMidi();
        app.systemRequestedQuit();
    }
    else
    {
        startTimer(200);
    }
}

void ApplicationState::shutdown()
{
    for (auto* route : routes_)
    {
        if (route->panic)
        {
            sendPanic(*route);
        }
        for (auto* dest : route->outputs)
        {
            if (dest->syxFile)
            {
                dest->syxFile->flush();
            }
        }
    }
}

bool ApplicationState::hasStdinInput() const
{
    for (auto* route : routes_)
    {
        for (auto* input : route->inputs)
        {
            if (input->isStdin)
            {
                return true;
            }
        }
    }
    return false;
}

void ApplicationState::readStdinMidi()
{
    while (std::cin)
    {
        std::string raw;
        getline(std::cin, raw);
        StringArray tokens = parseLineAsParameters(raw);
        if (tokens.isEmpty())
        {
            continue;
        }

        Array<MidiMessage> messages;
        parseTextMidi(tokens, messages);

        const ScopedLock sl(midiCallbackLock_);
        for (auto& message : messages)
        {
            for (auto* route : routes_)
            {
                for (auto* input : route->inputs)
                {
                    if (input->isStdin)
                    {
                        routeMessage(*route, *input, message);
                    }
                }
            }
        }
        std::cout.flush();
    }
}

Route* ApplicationState::currentRoute()
{
    return routes_.isEmpty() ? nullptr : routes_.getLast();
}

Route* ApplicationState::routeForNewInput()
{
    // keep accumulating inputs on the current route until it has outputs; once
    // outputs have been added, a further input starts a new route
    Route* route = currentRoute();
    if (route == nullptr || !route->outputs.isEmpty())
    {
        route = new Route();
        routes_.add(route);
    }
    return route;
}

bool ApplicationState::isMidiInDeviceAvailable(const String& name)
{
    for (auto&& device : MidiInput::getAvailableDevices())
    {
        if (device.name == name)
        {
            return true;
        }
    }
    return false;
}

void ApplicationState::timerCallback()
{
    const ScopedLock sl(midiCallbackLock_);

    for (auto* route : routes_)
    {
        for (auto* input : route->inputs)
        {
            if (input->isVirtual || input->isStdin)
            {
                continue;
            }

            if (input->fullInName.isNotEmpty() && !isMidiInDeviceAvailable(input->fullInName))
            {
                std::cerr << "MIDI input port \"" << input->fullInName << "\" got disconnected, waiting" << std::endl;
                input->fullInName = String();
                input->midiIn = nullptr;
                if (route->panic)
                {
                    sendPanic(*route);
                }
            }
            else if (input->inName.isNotEmpty() && input->midiIn == nullptr)
            {
                if (tryToConnectInput(*input))
                {
                    std::cerr << "Connected to MIDI input port \"" << input->fullInName << "\"" << std::endl;
                }
            }
        }

        // retry any output ports that could not be opened yet
        for (auto* dest : route->outputs)
        {
            if (!dest->isVirtual && !dest->isStdout && dest->out == nullptr && dest->name.isNotEmpty())
            {
                for (auto&& device : MidiOutput::getAvailableDevices())
                {
                    if (device.name.containsIgnoreCase(dest->name))
                    {
                        dest->out = MidiOutput::openDevice(device.identifier);
                        if (dest->out)
                        {
                            dest->fullName = device.name;
                            std::cerr << "Connected to MIDI output port \"" << dest->fullName << "\"" << std::endl;
                        }
                        break;
                    }
                }
            }
        }
    }
}

ApplicationCommand* ApplicationState::findApplicationCommand(const String& param)
{
    for (auto&& cmd : commands_)
    {
        if (cmd.param_.equalsIgnoreCase(param) || cmd.altParam_.equalsIgnoreCase(param))
        {
            return &cmd;
        }
    }
    return nullptr;
}

StringArray ApplicationState::parseLineAsParameters(const String& line)
{
    StringArray parameters;
    if (!line.startsWith("#"))
    {
        StringArray tokens;
        tokens.addTokens(line, true);
        tokens.removeEmptyStrings(true);
        for (String token : tokens)
        {
            parameters.add(token.trimCharactersAtStart("\"").trimCharactersAtEnd("\""));
        }
    }
    return parameters;
}

void ApplicationState::executeCurrentCommand()
{
    ApplicationCommand cmd = currentCommand_;
    currentCommand_ = ApplicationCommand::Dummy();
    executeCommand(cmd);
}

void ApplicationState::handleVarArgCommand()
{
    if (currentCommand_.expectedOptions_ < 0)
    {
        executeCurrentCommand();
    }
}

void ApplicationState::parseParameters(StringArray& parameters)
{
    for (String param : parameters)
    {
        if (param == "--") continue;

        if (currentCommand_.command_ == CONVERT && currentCommand_.expectedOptions_ != 0)
        {
            // convert has a dynamic number of arguments ("srctype [num] dsttype
            // [num]"), so collect tokens until the specification is complete
            currentCommand_.opts_.add(param);
            if (convertSpecComplete(currentCommand_.opts_))
            {
                currentCommand_.expectedOptions_ = 0;
            }
        }
        else if (currentCommand_.expectedOptions_ > 0)
        {
            // a command is still collecting fixed arguments: take this token
            // literally as a value, even when it happens to match a command name
            // (for instance the "cc"/"rpn" type words of the "convert" command)
            currentCommand_.opts_.add(param);
            currentCommand_.expectedOptions_ -= 1;
        }
        else
        {
            ApplicationCommand* cmd = findApplicationCommand(param);
            if (cmd)
            {
                // handle configuration commands immediately without setting up a new one
                switch (cmd->command_)
                {
                    case DECIMAL:
                        useHexadecimalsByDefault_ = false;
                        break;
                    case HEXADECIMAL:
                        useHexadecimalsByDefault_ = true;
                        break;
                    default:
                        handleVarArgCommand();

                        currentCommand_ = *cmd;
                        break;
                }
            }
            else if (currentCommand_.command_ == NONE)
            {
                File file = File::getCurrentWorkingDirectory().getChildFile(param);
                if (file.existsAsFile())
                {
                    parseFile(file);
                }
            }
            else if (currentCommand_.expectedOptions_ != 0)
            {
                // optional argument of a variable-argument command
                currentCommand_.opts_.add(param);
                currentCommand_.expectedOptions_ -= 1;
            }
        }

        // handle fixed arg commands
        if (currentCommand_.expectedOptions_ == 0)
        {
            executeCurrentCommand();
        }
    }

    handleVarArgCommand();
}

void ApplicationState::parseFile(File file)
{
    StringArray parameters;

    StringArray lines;
    file.readLines(lines);
    for (String line : lines)
    {
        parameters.addArray(parseLineAsParameters(line));
    }

    parseParameters(parameters);
}

void ApplicationState::executeCommand(ApplicationCommand& cmd)
{
    const bool negate = pendingNegate_;

    switch (cmd.command_)
    {
        case NONE:
            break;
        case LIST:
        {
            std::cout << "MIDI input ports:" << std::endl;
            for (auto&& device : MidiInput::getAvailableDevices())
            {
                std::cout << "  " << device.name << std::endl;
            }
            std::cout << std::endl << "MIDI output ports:" << std::endl;
            for (auto&& device : MidiOutput::getAvailableDevices())
            {
                std::cout << "  " << device.name << std::endl;
            }
            break;
        }
        case INPUT:
        {
            auto* input = new RouteInput();
            if (cmd.opts_[0] == "-")
            {
                // read MIDI as text from standard input
                input->isStdin = true;
                input->inName = "stdin";
                input->fullInName = "stdin";
                routeForNewInput()->inputs.add(input);
            }
            else
            {
                input->inName = cmd.opts_[0];
                routeForNewInput()->inputs.add(input);
                openInput(*input);
            }
            break;
        }
        case VIRTUAL_IN:
        {
            auto* input = new RouteInput();
            routeForNewInput()->inputs.add(input);
            String name = cmd.opts_.size() ? cmd.opts_[0] : DEFAULT_VIRTUAL_IN_NAME;
            createVirtualInput(*input, name);
            break;
        }
        case OUTPUT:
        {
            if (auto* route = currentRoute())
            {
                if (cmd.opts_[0] == "-")
                {
                    // write routed MIDI as text to standard output
                    auto* dest = new OutputDest();
                    dest->isStdout = true;
                    dest->name = "stdout";
                    dest->fullName = "stdout";
                    route->outputs.add(dest);
                }
                else
                {
                    route->outputs.add(openOutput(cmd.opts_[0]));
                }
            }
            else
            {
                std::cerr << "Ignoring \"out\" because no input route was started yet (use \"in\" first)" << std::endl;
            }
            break;
        }
        case VIRTUAL_OUT:
        {
            if (auto* route = currentRoute())
            {
                String name = cmd.opts_.size() ? cmd.opts_[0] : DEFAULT_VIRTUAL_OUT_NAME;
                if (auto* dest = createVirtualOutput(name))
                {
                    route->outputs.add(dest);
                }
            }
            else
            {
                std::cerr << "Ignoring \"vout\" because no input route was started yet (use \"in\" first)" << std::endl;
            }
            break;
        }
        case TXTFILE:
        {
            String path(cmd.opts_[0]);
            File file = File::getCurrentWorkingDirectory().getChildFile(path);
            if (file.existsAsFile())
            {
                parseFile(file);
            }
            else
            {
                std::cerr << "Couldn't find file \"" << path << "\"" << std::endl;
                JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
            }
            break;
        }
        case DECIMAL:
        case HEXADECIMAL:
            // handled directly in parseParameters
            break;
        case OCTAVE_MIDDLE_C:
            octaveMiddleC_ = asDecOrHex7BitValue(cmd.opts_[0]);
            break;
        case NOTE_NUMBERS:
            noteNumbersOutput_ = true;
            break;
        case TIMESTAMP:
            timestampOutput_ = true;
            break;
        case MONITOR:
            monitor_ = true;
            break;
        case MONITOR_SOURCE:
            monitor_ = true;
            monitorShowSource_ = true;
            break;
        case SYSEX_FILE:
        {
            if (auto* route = currentRoute())
            {
                String path(cmd.opts_[0]);
                File file = File::getCurrentWorkingDirectory().getChildFile(path);
                file.deleteFile();
                auto* dest = new OutputDest();
                dest->name = path;
                dest->fullName = path;
                dest->syxFile = file.createOutputStream();
                if (dest->syxFile == nullptr)
                {
                    std::cerr << "Couldn't create file \"" << path << "\"" << std::endl;
                    JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
                    delete dest;
                }
                else
                {
                    route->outputs.add(dest);
                }
            }
            else
            {
                std::cerr << "Ignoring \"syf\" because no input route was started yet (use \"in\" first)" << std::endl;
            }
            break;
        }
        case PANIC:
            if (auto* route = currentRoute())
            {
                route->panic = true;
            }
            else
            {
                std::cerr << "Ignoring \"panic\" because no input route was started yet (use \"in\" first)" << std::endl;
            }
            break;
        case NOT:
            pendingNegate_ = true;
            return;
        case JAVASCRIPT_FILE:
        {
            String path(cmd.opts_[0]);
            File file = File::getCurrentWorkingDirectory().getChildFile(path);
            if (file.existsAsFile())
            {
                cmd.opts_.set(0, file.loadFileAsString());
            }
            else
            {
                std::cerr << "Couldn't find file \"" << path << "\"" << std::endl;
                JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
                break;
            }
            if (auto* route = currentRoute())
            {
                route->transforms.add(cmd);
                hasScript_ = true;
            }
            else
            {
                std::cerr << "Ignoring transform \"" << cmd.param_ << "\" because no input route was started yet (use \"in\" first)" << std::endl;
            }
            break;
        }
        case CONVERT:
        {
            // the collected options have a dynamic shape ("srctype [num] dsttype
            // [num]"); normalize them to a fixed [srctype, srcnum, dsttype, dstnum]
            // form (with "0" where a type takes no number) for the converter
            const StringArray in = cmd.opts_;
            ConvType st, dt;
            String srcType, srcNum = "0", dstType, dstNum = "0";
            int idx = 0;
            bool ok = idx < in.size() && parseConvType(in[idx], st);
            if (ok)
            {
                srcType = in[idx++];
                if (typeNeedsNumber(st))
                {
                    if (idx < in.size()) srcNum = in[idx++]; else ok = false;
                }
            }
            if (ok)
            {
                ok = idx < in.size() && parseConvType(in[idx], dt);
                if (ok)
                {
                    dstType = in[idx++];
                    if (typeNeedsNumber(dt))
                    {
                        if (idx < in.size()) dstNum = in[idx++]; else ok = false;
                    }
                }
            }

            if (!ok)
            {
                std::cerr << "Invalid convert specification, expected \"<srctype> [number] <dsttype> [number]\""
                          << " with types cc, cc14, rpn, nrpn, pb, cp or pc" << std::endl;
                JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
            }
            else if (auto* route = currentRoute())
            {
                cmd.opts_.clearQuick();
                cmd.opts_.add(srcType);
                cmd.opts_.add(srcNum);
                cmd.opts_.add(dstType);
                cmd.opts_.add(dstNum);
                route->converters.add(cmd);
            }
            else
            {
                std::cerr << "Ignoring \"convert\" because no input route was started yet (use \"in\" first)" << std::endl;
            }
            break;
        }
        case MPE_RELOCATE:
        case MPE_COLLAPSE:
        case MPE_EXPAND:
        case MPE_BEND:
        case MPE_SENS:
        {
            // validate the zone tokens up front so mistakes are reported during
            // parsing rather than silently doing nothing at routing time
            mpe::Zone zone;
            const String& zoneToken = (cmd.command_ == MPE_EXPAND) ? cmd.opts_[1] : cmd.opts_[0];
            bool ok = mpe::parseZone(zoneToken, zone);
            if (cmd.command_ == MPE_RELOCATE)
            {
                mpe::Zone dst;
                ok = ok && mpe::parseZone(cmd.opts_[1], dst);
            }

            if (!ok)
            {
                std::cerr << "Invalid MPE zone, expected \"lower\" or \"upper\" with an optional"
                          << " \":<members>\" count (for example \"lower:7\")" << std::endl;
                JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
            }
            else if (auto* route = currentRoute())
            {
                route->mpeOps.add(cmd);
            }
            else
            {
                std::cerr << "Ignoring \"" << cmd.param_ << "\" because no input route was started yet (use \"in\" first)" << std::endl;
            }
            break;
        }
        case NRPN_ADD:
        case NRPN_SCALE:
        case NRPN_CURVE:
        case RPN_ADD:
        case RPN_SCALE:
        case RPN_CURVE:
            // RPN/NRPN value transforms are assembled and regenerated in the
            // converter stage, so they live alongside the convert rules
            if (auto* route = currentRoute())
            {
                route->converters.add(cmd);
            }
            else
            {
                std::cerr << "Ignoring \"" << cmd.param_ << "\" because no input route was started yet (use \"in\" first)" << std::endl;
            }
            break;
        case MPE_SPLIT:
        {
            mpe::Zone zone;
            if (cmd.opts_.isEmpty() || !mpe::parseZone(cmd.opts_[0], zone))
            {
                std::cerr << "Invalid \"mpesplit\" zone, expected \"lower\" or \"upper\" with an optional"
                          << " \":<members>\" count (for example \"lower:7\")" << std::endl;
                JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
            }
            else if (auto* route = currentRoute())
            {
                route->outputSplit.clearQuick();
                route->outputSplit.add(cmd);
            }
            else
            {
                std::cerr << "Ignoring \"mpesplit\" because no input route was started yet (use \"in\" first)" << std::endl;
            }
            break;
        }
        default:
            if (cmd.isFilter())
            {
                if (auto* route = currentRoute())
                {
                    cmd.negate_ = negate;
                    route->filters.add(cmd);
                }
                else
                {
                    std::cerr << "Ignoring filter \"" << cmd.param_ << "\" because no input route was started yet (use \"in\" first)" << std::endl;
                }
            }
            else if (cmd.isTransform())
            {
                if (auto* route = currentRoute())
                {
                    route->transforms.add(cmd);
                    if (cmd.command_ == JAVASCRIPT)
                    {
                        hasScript_ = true;
                    }
                }
                else
                {
                    std::cerr << "Ignoring transform \"" << cmd.param_ << "\" because no input route was started yet (use \"in\" first)" << std::endl;
                }
            }
            break;
    }

    pendingNegate_ = false;
}

void ApplicationState::openInput(RouteInput& input)
{
    input.midiIn = nullptr;
    if (!tryToConnectInput(input))
    {
        std::cerr << "Couldn't find MIDI input port \"" << input.inName << "\", waiting" << std::endl;
    }
}

bool ApplicationState::tryToConnectInput(RouteInput& input)
{
    std::unique_ptr<MidiInput> midiInput = nullptr;
    String midiInputName;

    auto devices = MidiInput::getAvailableDevices();
    for (auto&& device : devices)
    {
        if (device.name == input.inName)
        {
            midiInput = MidiInput::openDevice(device.identifier, this);
            midiInputName = device.name;
            break;
        }
    }

    if (midiInput == nullptr)
    {
        for (auto&& device : devices)
        {
            if (device.name.containsIgnoreCase(input.inName))
            {
                midiInput = MidiInput::openDevice(device.identifier, this);
                midiInputName = device.name;
                break;
            }
        }
    }

    if (midiInput)
    {
        midiInput->start();
        input.midiIn.swap(midiInput);
        input.fullInName = midiInputName;
        return true;
    }

    return false;
}

void ApplicationState::createVirtualInput(RouteInput& input, const String& name)
{
#if (JUCE_LINUX || JUCE_MAC)
    input.midiIn = MidiInput::createNewDevice(name, this);
    if (input.midiIn == nullptr)
    {
        std::cerr << "Couldn't create virtual MIDI input port \"" << name << "\"" << std::endl;
        JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
    }
    else
    {
        input.isVirtual = true;
        input.inName = name;
        input.fullInName = name;
        input.midiIn->start();
    }
#else
    ignoreUnused(input, name);
    std::cerr << "Virtual MIDI input ports are not supported on Windows" << std::endl;
    JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
#endif
}

OutputDest* ApplicationState::openOutput(const String& name)
{
    auto dest = std::make_unique<OutputDest>();
    dest->name = name;

    auto devices = MidiOutput::getAvailableDevices();
    for (auto&& device : devices)
    {
        if (device.name == name)
        {
            dest->out = MidiOutput::openDevice(device.identifier);
            dest->fullName = device.name;
            break;
        }
    }
    if (dest->out == nullptr)
    {
        for (auto&& device : devices)
        {
            if (device.name.containsIgnoreCase(name))
            {
                dest->out = MidiOutput::openDevice(device.identifier);
                dest->fullName = device.name;
                break;
            }
        }
    }

    if (dest->out == nullptr)
    {
        std::cerr << "Couldn't find MIDI output port \"" << name << "\", waiting" << std::endl;
    }

    return dest.release();
}

OutputDest* ApplicationState::createVirtualOutput(const String& name)
{
#if (JUCE_LINUX || JUCE_MAC)
    auto dest = std::make_unique<OutputDest>();
    dest->name = name;
    dest->isVirtual = true;
    dest->out = MidiOutput::createNewDevice(name);
    if (dest->out == nullptr)
    {
        std::cerr << "Couldn't create virtual MIDI output port \"" << name << "\"" << std::endl;
        JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
        return nullptr;
    }
    dest->fullName = name;
    return dest.release();
#else
    ignoreUnused(name);
    std::cerr << "Virtual MIDI output ports are not supported on Windows" << std::endl;
    JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
    return nullptr;
#endif
}

void ApplicationState::handleIncomingMidiMessage(MidiInput* source, const MidiMessage& msg)
{
    const ScopedLock sl(midiCallbackLock_);

    for (auto* route : routes_)
    {
        for (auto* input : route->inputs)
        {
            if (input->midiIn.get() == source)
            {
                routeMessage(*route, *input, msg);
                return;
            }
        }
    }
}

void ApplicationState::sendPanic(Route& route)
{
    for (auto* dest : route.outputs)
    {
        if (dest->out == nullptr)
        {
            continue;
        }
        for (int channel = 1; channel <= 16; ++channel)
        {
            dest->out->sendMessageNow(MidiMessage::controllerEvent(channel, 64, 0));   // sustain off
            dest->out->sendMessageNow(MidiMessage::controllerEvent(channel, 123, 0));  // all notes off
        }
    }
}

void ApplicationState::sendZoneReset(Route& route)
{
    // stop sounding notes and reset controllers downstream when an MPE zone is
    // reconfigured (spec section 2.2.3); non-MPE devices won't do this themselves
    for (auto* dest : route.outputs)
    {
        if (dest->out == nullptr)
        {
            continue;
        }
        for (int channel = 1; channel <= 16; ++channel)
        {
            dest->out->sendMessageNow(MidiMessage::controllerEvent(channel, 123, 0));  // all notes off
            dest->out->sendMessageNow(MidiMessage::controllerEvent(channel, 121, 0));  // reset all controllers
        }
    }
}

void ApplicationState::routeMessage(Route& route, RouteInput& input, const MidiMessage& msg)
{
    // when the safety net is on, flush stuck notes if a zone is reconfigured
    if (route.panic && input.mcm.reconfigures(msg))
    {
        sendZoneReset(route);
    }

    if (!passesFilters(route, msg))
    {
        return;
    }

    Array<MidiMessage> outputMessages;
    for (auto& transformed : applyTransforms(route, msg))
    {
        // the MPE stage may rechannel a message or, when expanding, turn one
        // message into several, before the converter stage runs
        Array<MidiMessage> afterMpe;
        if (route.mpeOps.isEmpty())
        {
            afterMpe.add(transformed);
        }
        else
        {
            processMpe(route, input, transformed, afterMpe);
        }

        for (auto& m : afterMpe)
        {
            if (route.converters.isEmpty())
            {
                outputMessages.add(m);
            }
            else
            {
                processConverters(route, input, m, outputMessages);
            }
        }
    }

    for (auto& outMsg : outputMessages)
    {
        if (route.outputSplit.isEmpty())
        {
            // ordinary routing: every message goes to every output
            for (auto* dest : route.outputs)
            {
                sendToDest(dest, outMsg);
            }
            if (monitor_)
            {
                printMonitor(input.fullInName, outMsg);
            }
        }
        else
        {
            // split routing: each message is dispatched to a specific output
            // (a voice's port) or broadcast to all (port -1)
            Array<MidiMessage> splitMsgs;
            Array<int> splitPorts;
            processSplit(route, outMsg, splitMsgs, splitPorts);

            for (int i = 0; i < splitMsgs.size(); ++i)
            {
                const MidiMessage& m = splitMsgs.getReference(i);
                if (splitPorts[i] < 0)
                {
                    for (auto* dest : route.outputs)
                    {
                        sendToDest(dest, m);
                    }
                }
                else if (splitPorts[i] < route.outputs.size())
                {
                    sendToDest(route.outputs[splitPorts[i]], m);
                }

                if (monitor_)
                {
                    printMonitor(input.fullInName, m);
                }
            }
        }
    }
}

void ApplicationState::sendToDest(OutputDest* dest, const MidiMessage& msg)
{
    if (dest->isStdout)
    {
        std::cout << messageToText(msg) << std::endl;
    }
    else if (dest->syxFile)
    {
        if (msg.isSysEx())
        {
            dest->syxFile->write(msg.getRawData(), (size_t)msg.getRawDataSize());
            dest->syxFile->flush();
        }
    }
    else if (dest->out)
    {
        dest->out->sendMessageNow(msg);
    }
}

void ApplicationState::processSplit(Route& route, const MidiMessage& msg, Array<MidiMessage>& outMsgs, Array<int>& outPorts)
{
    const ApplicationCommand& cmd = route.outputSplit.getReference(0);
    mpe::Zone zone;
    mpe::parseZone(cmd.opts_[0], zone);
    const int targetCh = cmd.opts_.size() > 1 ? jlimit(1, 16, asDecOrHexIntValue(cmd.opts_[1])) : 1;
    const int ports = route.outputs.size();
    const double ts = msg.getTimeStamp();
    const int ch = msg.getChannel();

    if (ports == 0)
    {
        return;
    }

    auto& sp = route.mpeSplit;
    sp.ensureSize(ports);
    if (!sp.expr.initialized)
    {
        sp.expr.resetForZone(zone);
    }

    const int managerCh = zone.masterChannel();
    const int memberIndex = zone.memberIndexOf(ch);
    const bool inZone = (ch >= 1 && zone.contains(ch));

    // keep the expression cache current for every in-zone message
    const int change = inZone ? sp.expr.update(zone, msg) : mpe::ExpressionState::None;

    auto emit = [&](int port, const MidiMessage& m) { outMsgs.add(m); outPorts.add(port); };
    auto rechannel = [&](const MidiMessage& m)
    {
        MidiMessage x = m;
        if (x.getChannel() > 0) x.setChannel(targetCh);
        x.setTimeStamp(ts);
        return x;
    };

    // each port is one voice, so its expression is the combination of the Manager
    // and the port's own Member channel (MPE spec 2.2.6-2.2.8)
    auto emitPortBend = [&](int port)
    {
        const int mch = sp.portChannel[(size_t) port];
        if (mch < 1) return;
        const int value = sp.expr.combinedBendValue(managerCh, mch);
        if (value == sp.portLastBend[(size_t) port]) return;
        const int sense = sp.expr.combinedSensitivity(managerCh, mch);
        if (sense != sp.portOutSense[(size_t) port])
        {
            Array<MidiMessage> rpn;
            mpe::appendPitchBendSensitivity(rpn, targetCh, sense, ts);
            for (auto& r : rpn) emit(port, r);
            sp.portOutSense[(size_t) port] = sense;
        }
        MidiMessage pb = MidiMessage::pitchWheel(targetCh, value);
        pb.setTimeStamp(ts);
        emit(port, pb);
        sp.portLastBend[(size_t) port] = value;
    };
    auto emitPortPressure = [&](int port)
    {
        const int mch = sp.portChannel[(size_t) port];
        if (mch < 1) return;
        MidiMessage cp = MidiMessage::channelPressureChange(targetCh, sp.expr.combinedPressure(managerCh, mch));
        cp.setTimeStamp(ts);
        emit(port, cp);
    };
    auto emitPortCC74 = [&](int port)
    {
        const int mch = sp.portChannel[(size_t) port];
        if (mch < 1) return;
        const int value = sp.expr.combinedCC74(managerCh, mch);
        if (value == sp.portLastCC74[(size_t) port]) return;
        MidiMessage cc = MidiMessage::controllerEvent(targetCh, 74, value);
        cc.setTimeStamp(ts);
        emit(port, cc);
        sp.portLastCC74[(size_t) port] = value;
    };
    auto isSensitivityData = [&](int channel)
    {
        return msg.isController() &&
            (msg.getControllerNumber() == 6 || msg.getControllerNumber() == 38) &&
            sp.expr.rpnMsb[channel] == 0 && sp.expr.rpnLsb[channel] == 0;
    };

    // --- Manager (zone-wide), non-zone, and channelless messages ---
    if (ch == 0 || memberIndex < 0)
    {
        if (zone.isMaster(ch))
        {
            // zone-wide expression is folded into every occupied port's combined value
            if (change == mpe::ExpressionState::Bend)     { for (int p = 0; p < ports; ++p) emitPortBend(p);     return; }
            if (change == mpe::ExpressionState::Pressure) { for (int p = 0; p < ports; ++p) emitPortPressure(p); return; }
            if (change == mpe::ExpressionState::CC74)     { for (int p = 0; p < ports; ++p) emitPortCC74(p);     return; }
            if (isSensitivityData(ch)) return;   // Manager RPN 0 data: consumed (captured above)

            // suppress the MPE Configuration Message (RPN 6) but let other RPNs
            // pass, replaying the selection that was held back
            if (msg.isController())
            {
                const int cc = msg.getControllerNumber();
                const int v = msg.getControllerValue();
                if (cc == 101) { sp.rpnMsb[ch] = v; sp.rpnSelectionSent[ch] = false; return; }
                if (cc == 100) { sp.rpnLsb[ch] = v; sp.rpnSelectionSent[ch] = false; return; }
                if (cc == 6 || cc == 38)
                {
                    if (sp.rpnMsb[ch] == 0 && sp.rpnLsb[ch] == 6) return;  // RPN 6 data: drop
                    if (!sp.rpnSelectionSent[ch])
                    {
                        if (sp.rpnMsb[ch] >= 0) emit(-1, rechannel(MidiMessage::controllerEvent(ch, 101, sp.rpnMsb[ch])));
                        if (sp.rpnLsb[ch] >= 0) emit(-1, rechannel(MidiMessage::controllerEvent(ch, 100, sp.rpnLsb[ch])));
                        sp.rpnSelectionSent[ch] = true;
                    }
                }
            }
        }
        emit(-1, rechannel(msg));
        return;
    }

    // --- Member channel ---
    // a note-on claims a port for this member channel, allocating a free one or
    // stealing the oldest, and is rechanneled onto the target channel
    if (msg.isNoteOn() && msg.getVelocity() > 0)
    {
        int port = sp.channelPort[ch];
        if (port < 0)
        {
            port = sp.freePort(ports);
            if (port < 0)
            {
                port = sp.oldestPort(ports);
                if (sp.portNote[(size_t) port] >= 0)
                {
                    MidiMessage off = MidiMessage::noteOff(targetCh, sp.portNote[(size_t) port], (uint8) 0);
                    off.setTimeStamp(ts);
                    emit(port, off);
                }
                const int oldCh = sp.portChannel[(size_t) port];
                if (oldCh >= 1) sp.channelPort[oldCh] = -1;
            }
            sp.channelPort[ch] = port;
            sp.portChannel[(size_t) port] = ch;
        }
        sp.portNote[(size_t) port] = msg.getNoteNumber();
        sp.portOrder[(size_t) port] = ++sp.order;

        // send the freshly assigned port's initial combined expression before
        // the Note On (spec section 2.4)
        emitPortBend(port);
        emitPortCC74(port);
        if (sp.expr.combinedPressure(managerCh, ch) > 0) emitPortPressure(port);

        MidiMessage on = MidiMessage::noteOn(targetCh, msg.getNoteNumber(), (uint8) msg.getVelocity());
        on.setTimeStamp(ts);
        emit(port, on);
        return;
    }

    // a note-off releases this member channel's port
    if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
    {
        const int port = sp.channelPort[ch];
        if (port >= 0)
        {
            MidiMessage off = MidiMessage::noteOff(targetCh, msg.getNoteNumber(), (uint8) msg.getVelocity());
            off.setTimeStamp(ts);
            emit(port, off);
            sp.portNote[(size_t) port] = -1;
            sp.portChannel[(size_t) port] = -1;
            sp.channelPort[ch] = -1;
        }
        return;
    }

    // per-note expression follows its note to the same port, combined with the
    // Manager; expression with no active note on its channel is dropped
    const int port = sp.channelPort[ch];
    if (port < 0) return;
    if (change == mpe::ExpressionState::Bend)     { emitPortBend(port);     return; }
    if (change == mpe::ExpressionState::Pressure) { emitPortPressure(port); return; }
    if (change == mpe::ExpressionState::CC74)     { emitPortCC74(port);     return; }
    if (isSensitivityData(ch)) return;   // Member RPN 0 data: consumed

    // any other member message is forwarded rechanneled to the port
    emit(port, rechannel(msg));
}

bool ApplicationState::selectorMatches(const String& token, int value, bool asNote) const
{
    const int sep = token.indexOf("..");
    if (sep < 0)
    {
        const int v = asNote ? (int) asNoteNumber(token) : (int) asDecOrHex7BitValue(token);
        return value == v;
    }

    int lo = asNote ? (int) asNoteNumber(token.substring(0, sep)) : (int) asDecOrHex7BitValue(token.substring(0, sep));
    int hi = asNote ? (int) asNoteNumber(token.substring(sep + 2)) : (int) asDecOrHex7BitValue(token.substring(sep + 2));
    if (lo > hi)
    {
        std::swap(lo, hi);
    }
    return value >= lo && value <= hi;
}

bool ApplicationState::passesFilters(Route& route, const MidiMessage& msg)
{
    if (route.filters.isEmpty())
    {
        return true;
    }

    // the channel filter can be a single channel or an inclusive "lo..hi" range;
    // a low of 0 means "any channel". This parses one channel selector token.
    auto parseChannel = [this](const String& token, int& low, int& high)
    {
        const int sep = token.indexOf("..");
        if (sep < 0)
        {
            low = high = jlimit(0, 16, asDecOrHexIntValue(token));
        }
        else
        {
            low  = jlimit(0, 16, asDecOrHexIntValue(token.substring(0, sep)));
            high = jlimit(0, 16, asDecOrHexIntValue(token.substring(sep + 2)));
            if (low > high) std::swap(low, high);
        }
    };

    int channelLow = 0, channelHigh = 0;   // context for type filters, 0 = any
    bool hasPositiveType = false;
    bool positiveMatched = false;
    bool negativeMatched = false;
    bool hasPositiveChannel = false;
    int positiveLow = 0, positiveHigh = 0;

    for (auto& cmd : route.filters)
    {
        if (cmd.command_ == CHANNEL)
        {
            int lo, hi;
            parseChannel(cmd.opts_[0], lo, hi);
            const bool inRange = (lo != 0 && msg.getChannel() >= lo && msg.getChannel() <= hi);
            if (cmd.negate_)
            {
                negativeMatched = negativeMatched || inRange;
            }
            else
            {
                channelLow = lo;
                channelHigh = hi;
                hasPositiveChannel = true;
                positiveLow = lo;
                positiveHigh = hi;
            }
        }
        else
        {
            bool m = cmd.matches(*this, msg, channelLow, channelHigh);
            if (cmd.negate_)
            {
                negativeMatched = negativeMatched || m;
            }
            else
            {
                positiveMatched = positiveMatched || m;
                hasPositiveType = true;
            }
        }
    }

    bool whitelistOK;
    if (hasPositiveType)
    {
        whitelistOK = positiveMatched;
    }
    else if (hasPositiveChannel)
    {
        whitelistOK = (positiveLow == 0 || (msg.getChannel() >= positiveLow && msg.getChannel() <= positiveHigh));
    }
    else
    {
        whitelistOK = true;
    }

    return whitelistOK && !negativeMatched;
}

Array<MidiMessage> ApplicationState::applyTransforms(Route& route, const MidiMessage& msg)
{
    Array<MidiMessage> current;
    current.add(msg);

    for (auto& cmd : route.transforms)
    {
        Array<MidiMessage> next;
        for (auto& m : current)
        {
            if (cmd.command_ == JAVASCRIPT || cmd.command_ == JAVASCRIPT_FILE)
            {
                scriptMidiMessage_->setMidiMessage(m);
                auto result = scriptEngine_.execute(cmd.opts_[0]);
                if (result.failed())
                {
                    std::cerr << result.getErrorMessage() << std::endl;
                }
                if (!scriptMidiMessage_->isBlocked())
                {
                    next.add(scriptMidiMessage_->getMidiMessage());
                }
                next.addArray(scriptMidiMessage_->getEmitted());
            }
            else
            {
                MidiMessage transformed = m;
                if (cmd.transform(*this, transformed))
                {
                    next.add(transformed);
                }
            }
        }
        current.swapWith(next);
    }

    return current;
}

void ApplicationState::processMpe(Route& route, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output)
{
    // run the message through each MPE operation in order; an operation can
    // pass a message through, rechannel it, drop it, or emit several messages
    Array<MidiMessage> current;
    current.add(msg);

    for (auto& cmd : route.mpeOps)
    {
        Array<MidiMessage> next;
        for (auto& m : current)
        {
            applyMpeOp(cmd, input, m, next);
        }
        current.swapWith(next);
    }

    output.addArray(current);
}

void ApplicationState::applyMpeOp(const ApplicationCommand& cmd, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output)
{
    const double ts = msg.getTimeStamp();
    const int ch = msg.getChannel();

    switch (cmd.command_)
    {
        case MPE_RELOCATE:
        {
            // remap every channel of the source zone onto the destination zone:
            // master -> master and member index i -> destination member i
            mpe::Zone src, dst;
            mpe::parseZone(cmd.opts_[0], src);
            mpe::parseZone(cmd.opts_[1], dst);
            auto& rel = input.mpeRelocate[src.lower ? 0 : 1];

            // the master channel is zone-wide; just move it across, but keep the
            // MPE Configuration Message's announced member count accurate for the
            // destination zone (rewrite the RPN 6 data entry)
            if (ch == src.masterChannel())
            {
                MidiMessage out = msg;
                out.setChannel(dst.masterChannel());
                if (msg.isController())
                {
                    const int cc = msg.getControllerNumber();
                    const int v = msg.getControllerValue();
                    if (cc == 101) rel.masterRpnMsb = v;
                    else if (cc == 100) rel.masterRpnLsb = v;
                    else if (cc == 6 && rel.masterRpnMsb == 0 && rel.masterRpnLsb == 6)
                        out = MidiMessage::controllerEvent(dst.masterChannel(), 6, dst.members);
                }
                out.setTimeStamp(ts);
                output.add(out);
                break;
            }

            const int idx = src.memberIndexOf(ch);
            if (idx < 0)
            {
                // not part of the source zone; pass through untouched
                output.add(msg);
                break;
            }

            // when the destination has fewer members, several source channels
            // fold onto one destination channel by wrapping the member index
            const int dstChannel = dst.memberChannel(idx % dst.members);

            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                rel.active[ch] = true;
                rel.order[ch] = ++rel.counter;
                MidiMessage out = msg;
                out.setChannel(dstChannel);
                output.add(out);
                break;
            }
            if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            {
                MidiMessage out = msg;
                out.setChannel(dstChannel);
                output.add(out);
                rel.active[ch] = false;
                break;
            }

            // pitch bend, channel pressure and timbre (CC 74) are channel-wide,
            // so they collide when several notes share a destination channel
            const bool isExpression = msg.isPitchWheel() || msg.isChannelPressure() ||
                (msg.isController() && msg.getControllerNumber() == 74);
            if (isExpression)
            {
                // find the most recently triggered note among the source channels
                // that fold onto this same destination channel
                int activeChannel = 0, bestOrder = 0;
                for (int i = 0; i < src.members; ++i)
                {
                    if ((i % dst.members) != (idx % dst.members))
                    {
                        continue;
                    }
                    const int sourceChannel = src.memberChannel(i);
                    if (rel.active[sourceChannel] && rel.order[sourceChannel] > bestOrder)
                    {
                        bestOrder = rel.order[sourceChannel];
                        activeChannel = sourceChannel;
                    }
                }

                // with at most one note on the destination channel the expression
                // is genuinely per-note and passes through; once notes collide,
                // only the most recently triggered note's expression is kept
                if (activeChannel != 0 && ch != activeChannel)
                {
                    break;  // suppressed: another note owns this destination channel
                }
            }

            MidiMessage out = msg;
            out.setChannel(dstChannel);
            output.add(out);
            break;
        }
        case MPE_COLLAPSE:
        {
            // fold every channel of the source zone onto a single channel for a
            // non-MPE synth, combining Manager (zone-wide) and Member (per-note)
            // expression the way an MPE receiver would (MPE spec 2.2.6-2.2.8)
            mpe::Zone src;
            mpe::parseZone(cmd.opts_[0], src);
            const int target = jlimit(1, 16, asDecOrHexIntValue(cmd.opts_[1]));
            auto& col = input.mpeCollapse[src.lower ? 0 : 1];
            const int managerCh = src.masterChannel();

            // messages outside the zone pass straight through
            if (!src.contains(ch))
            {
                output.add(msg);
                break;
            }

            if (!col.expr.initialized)
            {
                col.expr.resetForZone(src);
            }
            const int change = col.expr.update(src, msg);

            // emits the combined pitch bend for a member note, declaring the
            // combined Pitch Bend Sensitivity on the target first if it changed
            auto emitBend = [&](int memberCh)
            {
                if (memberCh < 1) return;
                const int value = col.expr.combinedBendValue(managerCh, memberCh);
                if (value == col.lastBend) return;   // nothing changed on the target
                const int sense = col.expr.combinedSensitivity(managerCh, memberCh);
                if (sense != col.outSense)
                {
                    mpe::appendPitchBendSensitivity(output, target, sense, ts);
                    col.outSense = sense;
                }
                MidiMessage pb = MidiMessage::pitchWheel(target, value);
                pb.setTimeStamp(ts);
                output.add(pb);
                col.lastBend = value;
            };
            auto emitCC74 = [&](int memberCh)
            {
                if (memberCh < 1) return;
                const int value = col.expr.combinedCC74(managerCh, memberCh);
                if (value == col.lastCC74) return;
                MidiMessage cc = MidiMessage::controllerEvent(target, 74, value);
                cc.setTimeStamp(ts);
                output.add(cc);
                col.lastCC74 = value;
            };
            // a member note's pressure rides on polyphonic aftertouch (so each
            // note keeps its own), combined with the Manager pressure via Max
            auto emitPressure = [&](int memberCh)
            {
                const int note = col.channelNote[memberCh];
                if (note < 0) return;
                MidiMessage at = MidiMessage::aftertouchChange(target, note, col.expr.combinedPressure(managerCh, memberCh));
                at.setTimeStamp(ts);
                output.add(at);
            };

            // a member RPN 0 data entry sets the source sensitivity (captured
            // above); it must not reach the target, which uses our combined RPN 0
            auto isSensitivityData = [&]()
            {
                return msg.isController() &&
                    (msg.getControllerNumber() == 6 || msg.getControllerNumber() == 38) &&
                    col.expr.rpnMsb[ch] == 0 && col.expr.rpnLsb[ch] == 0;
            };

            // --- Manager (zone-wide) messages affect the note that owns the channel ---
            if (src.isMaster(ch))
            {
                if (change == mpe::ExpressionState::Bend)     { emitBend(col.activeChannel()); break; }
                if (change == mpe::ExpressionState::CC74)     { emitCC74(col.activeChannel()); break; }
                if (change == mpe::ExpressionState::Pressure)
                {
                    for (int c = 1; c < 17; ++c) if (col.channelNote[c] >= 0) emitPressure(c);
                    break;
                }
                if (isSensitivityData()) break;

                MidiMessage out = msg;
                out.setChannel(target);
                output.add(out);
                break;
            }

            // --- Member channel ---
            if (msg.isNoteOn() && msg.getVelocity() > 0)
            {
                col.channelNote[ch] = msg.getNoteNumber();
                col.noteOrder[ch] = ++col.order;
                // the new note owns the channel-wide dimensions; send its initial
                // combined expression before the Note On (spec section 2.4)
                emitBend(ch);
                emitCC74(ch);
                if (col.expr.combinedPressure(managerCh, ch) > 0) emitPressure(ch);
                MidiMessage out = msg;
                out.setChannel(target);
                output.add(out);
                break;
            }
            if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            {
                col.channelNote[ch] = -1;
                col.noteOrder[ch] = 0;
                MidiMessage out = msg;
                out.setChannel(target);
                output.add(out);
                // hand the channel-wide dimensions to whichever note now owns them
                const int a = col.activeChannel();
                if (a >= 1) { emitBend(a); emitCC74(a); }
                break;
            }
            if (change == mpe::ExpressionState::Bend)     { if (ch == col.activeChannel()) emitBend(ch); break; }
            if (change == mpe::ExpressionState::CC74)     { if (ch == col.activeChannel()) emitCC74(ch); break; }
            if (change == mpe::ExpressionState::Pressure) { emitPressure(ch); break; }
            if (isSensitivityData()) break;

            // any other member-channel message is forwarded rechanneled
            MidiMessage out = msg;
            out.setChannel(target);
            output.add(out);
            break;
        }
        case MPE_EXPAND:
        {
            // spread the notes arriving on a single channel across an MPE zone's
            // member channels, sending zone-wide messages to the master channel
            const int srcCh = jlimit(1, 16, asDecOrHexIntValue(cmd.opts_[0]));
            mpe::Zone dst;
            mpe::parseZone(cmd.opts_[1], dst);
            auto& alloc = input.mpeAlloc[dst.lower ? 0 : 1];

            if (ch != srcCh)
            {
                // not the channel we expand; leave it untouched
                output.add(msg);
                break;
            }

            // announce the zone once, just before the first note is forwarded
            auto ensureConfig = [&]()
            {
                if (!alloc.configSent)
                {
                    mpe::appendConfigMessage(output, dst, ts);
                    alloc.configSent = true;
                }
            };

            if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            {
                const int note = msg.getNoteNumber();
                const int mch = (note >= 0 && note < 128) ? alloc.noteChannel[note] : -1;
                if (mch >= 1)
                {
                    // set Channel Pressure to zero before the Note Off (Appendix A.4.2)
                    MidiMessage zero = MidiMessage::channelPressureChange(mch, 0);
                    zero.setTimeStamp(ts);
                    output.add(zero);
                    MidiMessage off = MidiMessage::noteOff(mch, note, msg.getVelocity());
                    off.setTimeStamp(ts);
                    output.add(off);
                    alloc.bucket.release(mch);
                    alloc.noteChannel[note] = -1;
                }
                break;
            }

            if (msg.isNoteOn())
            {
                ensureConfig();
                const int note = msg.getNoteNumber();

                // fill the channel bucket with the destination zone's member
                // channels the first time it is used
                if (!alloc.bucketFilled)
                {
                    alloc.bucket.clear();
                    for (int i = 0; i < dst.members; ++i) alloc.bucket.add(dst.memberChannel(i));
                    alloc.bucketFilled = true;
                }

                // if this note is somehow still held, release its channel first
                if (alloc.noteChannel[note] >= 1)
                {
                    const int prev = alloc.noteChannel[note];
                    MidiMessage off = MidiMessage::noteOff(prev, note, (uint8)0);
                    off.setTimeStamp(ts);
                    output.add(off);
                    alloc.bucket.release(prev);
                    alloc.noteChannel[note] = -1;
                }

                // the bucket hands out a member channel, reusing one as late as
                // possible and sharing a channel when the polyphony exceeds the
                // number of member channels (MPE spec Appendix A.3)
                const int chosen = alloc.bucket.take();
                if (chosen >= 1)
                {
                    // set Channel Pressure to zero before the Note On (Appendix A.4.2)
                    MidiMessage zero = MidiMessage::channelPressureChange(chosen, 0);
                    zero.setTimeStamp(ts);
                    output.add(zero);
                    MidiMessage on = MidiMessage::noteOn(chosen, note, msg.getVelocity());
                    on.setTimeStamp(ts);
                    output.add(on);
                    alloc.noteChannel[note] = chosen;
                }
                break;
            }

            if (msg.isAftertouch())
            {
                // polyphonic aftertouch for a note becomes channel pressure on
                // that note's member channel, which is how MPE expresses per-note
                // pressure (the Z dimension)
                const int note = msg.getNoteNumber();
                const int mch = (note >= 0 && note < 128) ? alloc.noteChannel[note] : -1;
                MidiMessage out = MidiMessage::channelPressureChange(mch >= 1 ? mch : dst.masterChannel(),
                                                                     msg.getAfterTouchValue());
                out.setTimeStamp(ts);
                output.add(out);
                break;
            }

            // remaining channel-voice messages (CC, pitch bend, channel
            // pressure, program change) are zone-wide: send them to the master
            MidiMessage out = msg;
            out.setChannel(dst.masterChannel());
            output.add(out);
            break;
        }
        case MPE_BEND:
        {
            // rescale per-note pitch bend from a source semitone range to a
            // destination range so a controller and a synth whose member-channel
            // bend range differs play in tune (spec section 2.2.5)
            mpe::Zone zone;
            mpe::parseZone(cmd.opts_[0], zone);
            const double from = cmd.opts_[1].getDoubleValue();
            const double to = cmd.opts_[2].getDoubleValue();

            if (!msg.isPitchWheel() || zone.memberIndexOf(ch) < 0 || to <= 0.0)
            {
                output.add(msg);   // only member-channel pitch bend is rescaled
                break;
            }

            const int value = jlimit(0, 16383, roundToInt(8192.0 + (msg.getPitchWheelValue() - 8192) * from / to));
            MidiMessage out = MidiMessage::pitchWheel(ch, value);
            out.setTimeStamp(ts);
            output.add(out);
            break;
        }
        case MPE_SENS:
        {
            // declare a member-channel Pitch Bend Sensitivity (RPN 0) for a synth
            // that honours it, so it interprets the controller's bend correctly
            // without rescaling the values (spec section 2.2.5)
            mpe::Zone zone;
            mpe::parseZone(cmd.opts_[0], zone);
            const int semitones = jlimit(0, 96, asDecOrHexIntValue(cmd.opts_[1]));
            auto& sd = input.mpeSens[zone.lower ? 0 : 1];

            if (sd.isMcm(msg))
            {
                // forward the MCM (it resets member sensitivity to 48 on the
                // synth), then re-declare ours just after it
                output.add(msg);
                mpe::appendMemberSensitivity(output, zone, semitones, ts);
                sd.declared = true;
                break;
            }
            // otherwise declare it once, just before the first note is forwarded
            if (msg.isNoteOn() && msg.getVelocity() > 0 && !sd.declared)
            {
                mpe::appendMemberSensitivity(output, zone, semitones, ts);
                sd.declared = true;
            }
            output.add(msg);
            break;
        }
        default:
            output.add(msg);
            break;
    }
}

void ApplicationState::processConverters(Route& route, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output)
{
    const int ch = msg.getChannel();
    const double ts = msg.getTimeStamp();

    // single-message source types: pitch bend, channel pressure and program change
    if (! msg.isController())
    {
        for (auto& cmd : route.converters)
        {
            ConvType st, dt;
            if (! parseConvType(cmd.opts_[0], st) || ! parseConvType(cmd.opts_[2], dt))
            {
                continue;
            }

            const int srcNum = asDecOrHexIntValue(cmd.opts_[1]);
            int srcValue = -1;
            int srcBits = 7;

            if (st == CONV_PB && msg.isPitchWheel())
            {
                srcValue = msg.getPitchWheelValue();
                srcBits = 14;
            }
            else if (st == CONV_CP && msg.isChannelPressure())
            {
                srcValue = msg.getChannelPressureValue();
            }
            else if (st == CONV_PC && msg.isProgramChange())
            {
                srcValue = msg.getProgramChangeNumber();
            }

            if (srcValue >= 0)
            {
                const int dstNum = asDecOrHexIntValue(cmd.opts_[3]);
                emitConversion(output, ch, srcValue, srcBits, dt, dstNum,
                               scaleMethodFor(st, srcNum, dt, dstNum), ts);
                return;
            }
        }

        output.add(msg);
        return;
    }

    const int cc = msg.getControllerNumber();
    const int val = msg.getControllerValue();

    bool haveRpnRule = false;
    for (auto& cmd : route.converters)
    {
        ConvType st;
        if (parseConvType(cmd.opts_[0], st) && (st == CONV_RPN || st == CONV_NRPN))
        {
            haveRpnRule = true;
            break;
        }
        if (isRpnTransform(cmd.command_))
        {
            haveRpnRule = true;
            break;
        }
    }

    // When the route converts any RPN/NRPN, the converter takes over the whole
    // RPN controller set (6, 38, 98-101) and reassembles every (N)RPN. Targeted
    // parameters are converted; all others are regenerated and passed through.
    const bool isRpnCC = (cc == 6 || cc == 38 || cc == 98 || cc == 99 || cc == 100 || cc == 101);
    if (haveRpnRule && isRpnCC)
    {
        auto parsed = input.rpnDetector.tryParse(ch, cc, val);
        if (parsed.has_value())
        {
            const auto& rpn = *parsed;
            const int srcBits = rpn.is14BitValue ? 14 : 7;

            for (auto& cmd : route.converters)
            {
                ConvType st, dt;
                if (!parseConvType(cmd.opts_[0], st) || !parseConvType(cmd.opts_[2], dt))
                {
                    continue;
                }
                const int srcNum = asDecOrHexIntValue(cmd.opts_[1]);
                if (((st == CONV_RPN && !rpn.isNRPN) || (st == CONV_NRPN && rpn.isNRPN)) &&
                    srcNum == rpn.parameterNumber)
                {
                    const int dstNum = asDecOrHexIntValue(cmd.opts_[3]);
                    emitConversion(output, ch, rpn.value, srcBits, dt, dstNum,
                                   scaleMethodFor(st, srcNum, dt, dstNum), ts);
                    return;
                }
            }

            // no convert rule claimed it; apply any matching value transforms
            // (add/scale/curve) and regenerate the (N)RPN, unchanged if none match
            int newValue = rpn.value;
            const int maxValue = rpn.is14BitValue ? 16383 : 127;
            for (auto& cmd : route.converters)
            {
                if (!isRpnTransform(cmd.command_))
                {
                    continue;
                }
                const bool isNrpnTransform = (cmd.command_ == NRPN_ADD || cmd.command_ == NRPN_SCALE || cmd.command_ == NRPN_CURVE);
                if (isNrpnTransform != rpn.isNRPN)
                {
                    continue;
                }
                if (asDecOrHexIntValue(cmd.opts_[0]) != rpn.parameterNumber)
                {
                    continue;
                }
                switch (cmd.command_)
                {
                    case NRPN_ADD:
                    case RPN_ADD:
                        newValue = jlimit(0, maxValue, newValue + asDecOrHexIntValue(cmd.opts_[1]));
                        break;
                    case NRPN_SCALE:
                    case RPN_SCALE:
                        newValue = jlimit(0, maxValue, roundToInt(newValue * cmd.opts_[1].getFloatValue()));
                        break;
                    case NRPN_CURVE:
                    case RPN_CURVE:
                        newValue = gammaCurve(newValue, maxValue, cmd.opts_[1].getDoubleValue());
                        break;
                    default:
                        break;
                }
            }

            MidiBuffer buffer = MidiRPNGenerator::generate(rpn.channel, rpn.parameterNumber, newValue,
                                                           rpn.isNRPN, rpn.is14BitValue);
            for (const auto meta : buffer)
            {
                auto m = meta.getMessage();
                m.setTimeStamp(ts);
                output.add(m);
            }
        }
        // the constituent CC is consumed in all cases
        return;
    }

    // 14-bit CC source: MSB on controller N (0-31), LSB on controller N+32
    for (auto& cmd : route.converters)
    {
        ConvType st, dt;
        if (!parseConvType(cmd.opts_[0], st) || st != CONV_CC14)
        {
            continue;
        }
        parseConvType(cmd.opts_[2], dt);
        const int n = asDecOrHexIntValue(cmd.opts_[1]) & 0x1f;
        const int dstNum = asDecOrHexIntValue(cmd.opts_[3]);
        const auto method = scaleMethodFor(CONV_CC14, n, dt, dstNum);
        if (cc == n)
        {
            input.ccMsb[ch - 1][n] = val;
            input.ccMsbValid[ch - 1][n] = true;
            emitConversion(output, ch, val, 7, dt, dstNum, method, ts);
            return;
        }
        if (cc == n + 32)
        {
            const int msb = input.ccMsbValid[ch - 1][n] ? input.ccMsb[ch - 1][n] : 0;
            emitConversion(output, ch, (msb << 7) | val, 14, dt, dstNum, method, ts);
            return;
        }
    }

    // 7-bit CC source
    for (auto& cmd : route.converters)
    {
        ConvType st, dt;
        if (!parseConvType(cmd.opts_[0], st) || st != CONV_CC7)
        {
            continue;
        }
        const int srcNum = asDecOrHexIntValue(cmd.opts_[1]);
        if (cc == srcNum)
        {
            parseConvType(cmd.opts_[2], dt);
            const int dstNum = asDecOrHexIntValue(cmd.opts_[3]);
            emitConversion(output, ch, val, 7, dt, dstNum,
                           scaleMethodFor(CONV_CC7, srcNum, dt, dstNum), ts);
            return;
        }
    }

    // not managed by any conversion rule: forward unchanged
    output.add(msg);
}

String ApplicationState::messageToText(const MidiMessage& msg) const
{
    String line;

    if (msg.getChannel() > 0)
    {
        line << "channel " << output7Bit(msg.getChannel()).paddedLeft(' ', 2) << "   ";
    }

    if (msg.isNoteOn())
    {
        line << "note-on         " << outputNote(msg) << " " << output7Bit(msg.getVelocity()).paddedLeft(' ', 3);
    }
    else if (msg.isNoteOff())
    {
        line << "note-off        " << outputNote(msg) << " " << output7Bit(msg.getVelocity()).paddedLeft(' ', 3);
    }
    else if (msg.isAftertouch())
    {
        line << "poly-pressure   " << outputNote(msg) << " " << output7Bit(msg.getAfterTouchValue()).paddedLeft(' ', 3);
    }
    else if (msg.isController())
    {
        line << "control-change   " << output7Bit(msg.getControllerNumber()).paddedLeft(' ', 3) << "   "
             << output7Bit(msg.getControllerValue()).paddedLeft(' ', 3);
    }
    else if (msg.isProgramChange())
    {
        line << "program-change   " << output7Bit(msg.getProgramChangeNumber()).paddedLeft(' ', 7);
    }
    else if (msg.isChannelPressure())
    {
        line << "channel-pressure " << output7Bit(msg.getChannelPressureValue()).paddedLeft(' ', 7);
    }
    else if (msg.isPitchWheel())
    {
        line << "pitch-bend       " << output14Bit(msg.getPitchWheelValue()).paddedLeft(' ', 7);
    }
    else if (msg.isMidiClock())
    {
        line << "midi-clock";
    }
    else if (msg.isMidiStart())
    {
        line << "start";
    }
    else if (msg.isMidiStop())
    {
        line << "stop";
    }
    else if (msg.isMidiContinue())
    {
        line << "continue";
    }
    else if (msg.isActiveSense())
    {
        line << "active-sensing";
    }
    else if (msg.getRawDataSize() == 1 && msg.getRawData()[0] == 0xff)
    {
        line << "reset";
    }
    else if (msg.isSysEx())
    {
        // the bytes are emitted in hexadecimal so SendMIDI can read them back
        line << "system-exclusive";
        if (!useHexadecimalsByDefault_)
        {
            line << " hex";
        }
        const uint8* data = msg.getSysExData();
        const int size = msg.getSysExDataSize();
        for (int i = 0; i < size; ++i)
        {
            line << " " << output7BitAsHex(data[i]);
        }
        if (!useHexadecimalsByDefault_)
        {
            line << " dec";
        }
    }
    else if (msg.isQuarterFrame())
    {
        line << "time-code " << output7Bit(msg.getQuarterFrameSequenceNumber()).paddedLeft(' ', 2) << " "
             << output7Bit(msg.getQuarterFrameValue());
    }
    else if (msg.isSongPositionPointer())
    {
        line << "song-position " << output14Bit(msg.getSongPositionPointerMidiBeat()).paddedLeft(' ', 5);
    }
    else if (msg.getRawDataSize() == 2 && msg.getRawData()[0] == 0xf3)
    {
        line << "song-select " << output7Bit(msg.getRawData()[1]).paddedLeft(' ', 3);
    }
    else if (msg.getRawDataSize() == 1 && msg.getRawData()[0] == 0xf6)
    {
        line << "tune-request";
    }
    else
    {
        line << msg.getDescription();
    }

    return line;
}

void ApplicationState::parseTextMidi(const StringArray& tokens, Array<MidiMessage>& output) const
{
    int channel = 1;
    bool lineHex = useHexadecimalsByDefault_;
    int i = 0;

    auto num = [&](const String& s) -> int
    {
        if (s.endsWithIgnoreCase("H")) return s.dropLastCharacters(1).getHexValue32();
        if (s.endsWithIgnoreCase("M")) return s.getIntValue();
        return lineHex ? s.getHexValue32() : s.getIntValue();
    };
    auto next = [&]() -> String { return i < tokens.size() ? tokens[i++] : String(); };

    while (i < tokens.size())
    {
        const String tok = tokens[i++].toLowerCase();

        if (tok == "hex")                                     { lineHex = true; }
        else if (tok == "dec")                                { lineHex = false; }
        else if (tok == "channel" || tok == "ch")             { channel = jlimit(1, 16, num(next())); }
        else if (tok == "note-on" || tok == "on")
        {
            const int note = asNoteNumber(next());
            output.add(MidiMessage::noteOn(channel, note, (uint8)jlimit(0, 127, num(next()))));
        }
        else if (tok == "note-off" || tok == "off")
        {
            const int note = asNoteNumber(next());
            output.add(MidiMessage::noteOff(channel, note, (uint8)jlimit(0, 127, num(next()))));
        }
        else if (tok == "poly-pressure" || tok == "pp")
        {
            const int note = asNoteNumber(next());
            output.add(MidiMessage::aftertouchChange(channel, note, jlimit(0, 127, num(next()))));
        }
        else if (tok == "control-change" || tok == "cc")
        {
            const int n = jlimit(0, 127, num(next()));
            output.add(MidiMessage::controllerEvent(channel, n, jlimit(0, 127, num(next()))));
        }
        else if (tok == "program-change" || tok == "pc")      { output.add(MidiMessage::programChange(channel, jlimit(0, 127, num(next())))); }
        else if (tok == "channel-pressure" || tok == "cp")    { output.add(MidiMessage::channelPressureChange(channel, jlimit(0, 127, num(next())))); }
        else if (tok == "pitch-bend" || tok == "pb")          { output.add(MidiMessage::pitchWheel(channel, jlimit(0, 16383, num(next())))); }
        else if (tok == "midi-clock" || tok == "mc")          { output.add(MidiMessage::midiClock()); }
        else if (tok == "start")                              { output.add(MidiMessage::midiStart()); }
        else if (tok == "stop")                               { output.add(MidiMessage::midiStop()); }
        else if (tok == "continue" || tok == "cont")          { output.add(MidiMessage::midiContinue()); }
        else if (tok == "active-sensing" || tok == "as")      { output.add(MidiMessage(0xfe)); }
        else if (tok == "reset" || tok == "rst")              { output.add(MidiMessage(0xff)); }
        else if (tok == "song-position" || tok == "spp")      { output.add(MidiMessage::songPositionPointer(jlimit(0, 16383, num(next())))); }
        else if (tok == "song-select" || tok == "ss")         { output.add(MidiMessage(0xf3, jlimit(0, 127, num(next())))); }
        else if (tok == "tune-request" || tok == "tun")       { output.add(MidiMessage(0xf6)); }
        else if (tok == "time-code" || tok == "tc")
        {
            const int type = jlimit(0, 7, num(next()));
            output.add(MidiMessage(0xf1, ((type << 4) | jlimit(0, 15, num(next()))) & 0x7f));
        }
        else if (tok == "system-exclusive" || tok == "syx")
        {
            // the rest of the line is the SysEx data (with optional hex/dec toggles)
            Array<uint8> bytes;
            while (i < tokens.size())
            {
                const String b = tokens[i++];
                if (b.equalsIgnoreCase("hex"))      { lineHex = true; }
                else if (b.equalsIgnoreCase("dec")) { lineHex = false; }
                else                                { bytes.add((uint8)(num(b) & 0x7f)); }
            }
            if (!bytes.isEmpty())
            {
                output.add(MidiMessage::createSysExMessage(bytes.getRawDataPointer(), bytes.size()));
            }
        }
        // unknown tokens are ignored
    }
}

void ApplicationState::printMonitor(const String& inName, const MidiMessage& msg)
{
    String line;

    if (timestampOutput_)
    {
        Time t = Time::getCurrentTime();
        line << String(t.getHours()).paddedLeft('0', 2) << ":"
             << String(t.getMinutes()).paddedLeft('0', 2) << ":"
             << String(t.getSeconds()).paddedLeft('0', 2) << "."
             << String(t.getMilliseconds()).paddedLeft('0', 3) << "  ";
    }

    if (monitorShowSource_)
    {
        line << inName.paddedRight(' ', 16) << "  ";
    }

    line << messageToText(msg);
    std::cout << line << std::endl;
}

String ApplicationState::output7BitAsHex(int v) const
{
    return String::toHexString(v).paddedLeft('0', 2).toUpperCase();
}

String ApplicationState::output7Bit(int v) const
{
    return useHexadecimalsByDefault_ ? output7BitAsHex(v) : String(v);
}

String ApplicationState::output14BitAsHex(int v) const
{
    return String::toHexString(v).paddedLeft('0', 4).toUpperCase();
}

String ApplicationState::output14Bit(int v) const
{
    return useHexadecimalsByDefault_ ? output14BitAsHex(v) : String(v);
}

String ApplicationState::outputNote(const MidiMessage& msg) const
{
    if (noteNumbersOutput_)
    {
        return output7Bit(msg.getNoteNumber()).paddedLeft(' ', 4);
    }
    return MidiMessage::getMidiNoteName(msg.getNoteNumber(), true, true, octaveMiddleC_).paddedLeft(' ', 4);
}

uint8 ApplicationState::asNoteNumber(String value) const
{
    if (value.length() >= 2)
    {
        value = value.toUpperCase();
        String first = value.substring(0, 1);
        if (first.containsOnly("CDEFGABH") && value.substring(value.length() - 1).containsOnly("1234567890"))
        {
            int note = 0;
            switch (first[0])
            {
                case 'C': note = 0; break;
                case 'D': note = 2; break;
                case 'E': note = 4; break;
                case 'F': note = 5; break;
                case 'G': note = 7; break;
                case 'A': note = 9; break;
                case 'B': note = 11; break;
                case 'H': note = 11; break;
            }

            if (value[1] == 'B')
            {
                note -= 1;
            }
            else if (value[1] == '#')
            {
                note += 1;
            }

            note += (value.getTrailingIntValue() + 5 - octaveMiddleC_) * 12;

            return (uint8)limit7Bit(note);
        }
    }

    return (uint8)limit7Bit(asDecOrHexIntValue(value));
}

uint8 ApplicationState::asDecOrHex7BitValue(String value) const
{
    return (uint8)limit7Bit(asDecOrHexIntValue(value));
}

uint16 ApplicationState::asDecOrHex14BitValue(String value) const
{
    return (uint16)limit14Bit(asDecOrHexIntValue(value));
}

int ApplicationState::asDecOrHexIntValue(String value) const
{
    if (value.endsWithIgnoreCase("H"))
    {
        return value.dropLastCharacters(1).getHexValue32();
    }
    else if (value.endsWithIgnoreCase("M"))
    {
        return value.getIntValue();
    }
    else if (useHexadecimalsByDefault_)
    {
        return value.getHexValue32();
    }
    else
    {
        return value.getIntValue();
    }
}

uint8 ApplicationState::limit7Bit(int value)
{
    return (uint8)jlimit(0, 0x7f, value);
}

uint16 ApplicationState::limit14Bit(int value)
{
    return (uint16)jlimit(0, 0x3fff, value);
}

void ApplicationState::printVersion()
{
    std::cout << ProjectInfo::projectName << " v" << ProjectInfo::versionString << std::endl;
    std::cout << "https://github.com/gbevin/RouteMIDI" << std::endl;
}

// word-wraps text into lines no wider than the given number of characters
static StringArray wrapText(const String& text, int width)
{
    StringArray words;
    words.addTokens(text, " ", "");
    words.removeEmptyStrings();

    StringArray lines;
    String current;
    for (auto&& word : words)
    {
        if (current.isEmpty())
        {
            current = word;
        }
        else if (current.length() + 1 + word.length() <= width)
        {
            current << " " << word;
        }
        else
        {
            lines.add(current);
            current = word;
        }
    }
    if (current.isNotEmpty())
    {
        lines.add(current);
    }
    return lines;
}

void ApplicationState::printUsage()
{
    printVersion();
    std::cout << std::endl;
    std::cout << "Usage: " << ProjectInfo::projectName << " [ commands ] [ programfile ] [ -- ]" << std::endl << std::endl
              << "Commands:" << std::endl;
    const int optionColumn = 12;       // where the option names start
    const int descriptionColumn = 23;  // where the description starts
    for (auto&& cmd : commands_)
    {
        // the options share the option column, wrapping onto extra lines only
        // when they don't all fit within it (reserving one space so they never
        // butt up against the description column)
        String joinedOptions;
        for (auto&& option : cmd.optionsDescriptions_)
        {
            if (option.isNotEmpty())
            {
                joinedOptions << (joinedOptions.isEmpty() ? "" : " ") << option;
            }
        }
        StringArray options = wrapText(joinedOptions, descriptionColumn - optionColumn - 1);

        // the description starts on the first option's line, wrapping if needed
        StringArray descriptionLines;
        if (!cmd.commandDescriptions_.isEmpty())
        {
            descriptionLines = wrapText(cmd.commandDescriptions_.getReference(0), 80 - descriptionColumn);
        }

        for (int i = 0; i < jmax(1, options.size(), descriptionLines.size()); ++i)
        {
            String line;
            if (i == 0)
            {
                line << "  " << cmd.param_.paddedRight(' ', 9) << " ";
            }
            else
            {
                line = String().paddedRight(' ', optionColumn);
            }

            if (i < options.size())
            {
                line << options.getReference(i);
            }

            if (i < descriptionLines.size())
            {
                line = line.paddedRight(' ', descriptionColumn);
                line << descriptionLines.getReference(i);
            }

            std::cout << line.trimEnd() << std::endl;
        }
    }
    std::cout << "  -h  or  --help       Print Help (this message) and exit" << std::endl;
    std::cout << "  --version            Print version information and exit" << std::endl;
    std::cout << "  --                   Read commands from standard input until it's closed" << std::endl;
    std::cout << std::endl;
    std::cout << "Alternatively, you can use the following long versions of the commands:" << std::endl;
    String line = " ";
    for (auto&& cmd : commands_)
    {
        if (cmd.altParam_.isNotEmpty())
        {
            if (line.length() + cmd.altParam_.length() + 1 >= 80)
            {
                std::cout << line << std::endl;
                line = " ";
            }
            line << " " << cmd.altParam_;
        }
    }
    std::cout << line << std::endl << std::endl;
    std::cout << "A route forwards every \"in\" (or \"vin\") port to every \"out\" (or \"vout\") port," << std::endl
              << "so it can split, merge, or both. More \"in\" ports keep adding to the route" << std::endl
              << "until an \"out\" is given; the next \"in\" after that begins a new route." << std::endl;
    std::cout << std::endl;
    std::cout << "Filters select which messages may pass: with one or more positive filters only" << std::endl
              << "matching messages are forwarded. Prefix a filter with \"not\" to block matches" << std::endl
              << "instead. Transforms modify the messages that pass, in the order given." << std::endl;
    std::cout << std::endl;
    std::cout << "By default, numbers are interpreted in the decimal system, this can be changed" << std::endl
              << "to hexadecimal by sending the \"hex\" command. Additionally, by suffixing a" << std::endl
              << "number with \"M\" or \"H\", it will be interpreted as a decimal or hexadecimal" << std::endl
              << "respectively." << std::endl;
    std::cout << std::endl;
    std::cout << "The MIDI port names don't have to be an exact match." << std::endl;
    std::cout << "If RouteMIDI can't find the exact name that was specified, it will pick the" << std::endl
              << "first MIDI port that contains the provided text, irrespective of case." << std::endl;
    std::cout << std::endl;
    std::cout << "Where notes can be provided as arguments, they can also be written as note" << std::endl
              << "names, by default from C-2 to G8 which corresponds to note numbers 0 to 127." << std::endl
              << "By setting the octave for middle C, the note name range can be changed." << std::endl
              << "Sharps can be added by using the \"#\" symbol after the note letter, and flats" << std::endl
              << "by using the letter \"b\"." << std::endl;
    std::cout << std::endl;
    std::cout << "The number that selects which message a filter matches (for \"ch\", \"on\"," << std::endl
              << "\"off\", \"pp\", \"cc\", \"cc14\" and \"pc\") may be a single value or an inclusive" << std::endl
              << "range written as \"lo..hi\", for example \"cc 1..10\", \"on C3..C5\" or \"ch 1..4\"." << std::endl;
    std::cout << std::endl;
    std::cout << "The MPE commands take a zone written as \"<side>[:<members>]\", where the side" << std::endl
              << "is \"lower\" or \"upper\" (with master channel 1 or 16) and the optional member" << std::endl
              << "count is 1-15, defaulting to 15. For example \"lower\", \"upper:7\" or \"l:5\"." << std::endl;
}
