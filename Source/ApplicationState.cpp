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

#include "McpServer.h"
#include "Schema.h"

#include "BitScaling.h"
#include "Conversion.h"
#include "ScriptOscClass.h"
#include "ScriptUtilClass.h"

static const int DEFAULT_OCTAVE_MIDDLE_C = 3;
static const String DEFAULT_VIRTUAL_IN_NAME = "RouteMIDI In";
static const String DEFAULT_VIRTUAL_OUT_NAME = "RouteMIDI Out";

ApplicationState::ApplicationState()
{
    commands_.add({"in",           "input",                      INPUT,                       1, {"name"},                   {"Add a MIDI input (- for stdin text); a new route starts after outputs"}, "Routing and ports"});
    commands_.add({"out",          "output",                     OUTPUT,                      1, {"name"},                   {"Add a MIDI output to the route (- for stdout text)"}});
    commands_.add({"vin",          "virtual-in",                 VIRTUAL_IN,                 -1, {"(name)"},                 {"Add a virtual MIDI input to the route (Linux/macOS)"}});
    commands_.add({"vout",         "virtual-out",                VIRTUAL_OUT,                -1, {"(name)"},                 {"Add a virtual MIDI output to the route (Linux/macOS)"}});
    commands_.add({"list",         "",                           LIST,                        0, {""},                       {"List the available MIDI input and output ports"}});
    commands_.add({"panic",        "",                           PANIC,                       0, {""},                       {"Send all-notes-off on disconnect, exit and zone change"}});
    commands_.add({"file",         "",                           TXTFILE,                     1, {"path"},                   {"Load commands from the specified program file"}, "Configuration"});
    commands_.add({"dec",          "decimal",                    DECIMAL,                     0, {""},                       {"Interpret the next numbers as decimals by default"}});
    commands_.add({"hex",          "hexadecimal",                HEXADECIMAL,                 0, {""},                       {"Interpret the next numbers as hexadecimals by default"}});
    commands_.add({"omc",          "octave-middle-c",            OCTAVE_MIDDLE_C,             1, {"number"},                 {"Set octave for middle C, defaults to 3"}});
    commands_.add({"nn",           "note-numbers",               NOTE_NUMBERS,                0, {""},                       {"Monitor notes as numbers instead of names"}, "Monitoring"});
    commands_.add({"ts",           "timestamp",                  TIMESTAMP,                   0, {""},                       {"Prefix monitored messages with a timestamp"}});
    commands_.add({"mon",          "verbose",                    MONITOR,                     0, {""},                       {"Print each routed message (quiet by default)"}});
    commands_.add({"src",          "monitor-source",             MONITOR_SOURCE,              0, {""},                       {"Prefix monitored messages with the input port name"}});
    commands_.add({"not",          "",                           NOT,                         0, {""},                       {"Negate the next filter, blocking matching messages"}, "Filters"});
    commands_.add({"ch",           "channel",                    CHANNEL,                     1, {"number"},                 {"Restrict the route to a MIDI channel (1-16)"}});
    commands_.add({"voice",        "",                           VOICE,                       0, {""},                       {"Pass all Channel Voice messages"}});
    commands_.add({"note",         "",                           NOTE,                        0, {""},                       {"Pass all Note messages"}});
    commands_.add({"on",           "note-on",                    NOTE_ON,                    -1, {"(note)"},                 {"Pass Note On, optionally for note (0-127)"}});
    commands_.add({"off",          "note-off",                   NOTE_OFF,                   -1, {"(note)"},                 {"Pass Note Off, optionally for note (0-127)"}});
    commands_.add({"pp",           "poly-pressure",              POLY_PRESSURE,              -1, {"(note)"},                 {"Pass Poly Pressure, optionally for note (0-127)"}});
    commands_.add({"cc",           "control-change",             CONTROL_CHANGE,             -1, {"(number)"},               {"Pass Control Change, optionally for controller (0-127)"}});
    commands_.add({"cc14",         "control-change-14",          CONTROL_CHANGE_14BIT,       -1, {"(number)"},               {"Pass 14-bit CC, optionally for MSB controller (0-31)"}});
    commands_.add({"nrpn",         "",                           NRPN,                        0, {""},                       {"Pass NRPN traffic (CC 6, 38, 98, 99)"}});
    commands_.add({"rpn",          "",                           RPN,                         0, {""},                       {"Pass RPN traffic (CC 6, 38, 100, 101)"}});
    commands_.add({"pc",           "program-change",             PROGRAM_CHANGE,             -1, {"(number)"},               {"Pass Program Change, optionally for program (0-127)"}});
    commands_.add({"cp",           "channel-pressure",           CHANNEL_PRESSURE,            0, {""},                       {"Pass Channel Pressure"}});
    commands_.add({"pb",           "pitch-bend",                 PITCH_BEND,                  0, {""},                       {"Pass Pitch Bend"}});
    commands_.add({"sr",           "system-realtime",            SYSTEM_REALTIME,             0, {""},                       {"Pass all System Real-Time messages"}});
    commands_.add({"clock",        "",                           CLOCK,                       0, {""},                       {"Pass Timing Clock"}});
    commands_.add({"start",        "",                           START,                       0, {""},                       {"Pass Start"}});
    commands_.add({"stop",         "",                           STOP,                        0, {""},                       {"Pass Stop"}});
    commands_.add({"cont",         "continue",                   CONTINUE,                    0, {""},                       {"Pass Continue"}});
    commands_.add({"as",           "active-sensing",             ACTIVE_SENSING,              0, {""},                       {"Pass Active Sensing"}});
    commands_.add({"rst",          "reset",                      RESET,                       0, {""},                       {"Pass Reset"}});
    commands_.add({"sc",           "system-common",              SYSTEM_COMMON,               0, {""},                       {"Pass all System Common messages"}});
    commands_.add({"syx",          "system-exclusive",           SYSTEM_EXCLUSIVE,            0, {""},                       {"Pass System Exclusive"}});
    commands_.add({"syf",          "system-exclusive-file",      SYSEX_FILE,                  1, {"path"},                   {"Capture routed System Exclusive to a .syx file"}});
    commands_.add({"tc",           "time-code",                  TIME_CODE,                   0, {""},                       {"Pass MIDI Time Code Quarter Frame"}});
    commands_.add({"spp",          "song-position",              SONG_POSITION,               0, {""},                       {"Pass Song Position Pointer"}});
    commands_.add({"ss",           "song-select",                SONG_SELECT,                 0, {""},                       {"Pass Song Select"}});
    commands_.add({"tun",          "tune-request",               TUNE_REQUEST,                0, {""},                       {"Pass Tune Request"}});
    commands_.add({"noterange",    "note-range",                 NOTE_RANGE,                  2, {"low", "high"},            {"Pass notes within a note range (key split)"}});
    commands_.add({"velrange",     "velocity-range",             VELOCITY_RANGE,              2, {"low", "high"},            {"Pass note-ons within a velocity range (vel split)"}});
    commands_.add({"ccrange",      "control-change-range",       CONTROL_CHANGE_RANGE,        3, {"number", "low", "high"},  {"Pass a Control Change only when its value is in a range"}});
    commands_.add({"cc14range",    "control-change-14-range",    CONTROL_CHANGE_14BIT_RANGE,  3, {"number", "low", "high"},  {"Pass a 14-bit CC only when its value is in a range (0-16383)"}});
    commands_.add({"inscale",      "in-scale",                   IN_SCALE,                    2, {"root", "scale"},          {"Pass notes that belong to a scale (root and name)"}});
    commands_.add({"mpemaster",    "mpe-master",                 MPE_MASTER,                  1, {"zone[:n]"},               {"Pass the master channel of an MPE zone (e.g. lower)"}});
    commands_.add({"mpemember",    "mpe-member",                 MPE_MEMBER,                  1, {"zone[:n]"},               {"Pass the member channels of an MPE zone (e.g. upper:7)"}});
    commands_.add({"mpezone",      "mpe-zone",                   MPE_ZONE,                    1, {"zone[:n]"},               {"Pass a whole MPE zone (its master and member channels)"}});
    commands_.add({"chmap",        "channel-map",                CHANNEL_MAP,                 2, {"from", "to"},             {"Remap channel-voice messages from one channel to another"}, "Transforms"});
    commands_.add({"chset",        "channel-set",                CHANNEL_SET,                 1, {"number"},                 {"Force all channel-voice messages onto a channel"}});
    commands_.add({"chadd",        "channel-add",                CHANNEL_ADD,                 1, {"number"},                 {"Add N to the channel, wrapping 1-16 (may be negative)"}});
    commands_.add({"transp",       "transpose",                  TRANSPOSE,                   1, {"semitones"},              {"Transpose notes by N semitones (out-of-range dropped)"}});
    commands_.add({"dtransp",      "diatonic-transpose",         DIATONIC_TRANSPOSE,          3, {"root", "scale", "steps"}, {"Transpose within a scale by N scale steps (stays in key)"}});
    commands_.add({"notemap",      "note-map",                   NOTE_MAP,                    2, {"from", "to"},             {"Remap a specific note number to another"}});
    commands_.add({"scale",        "",                           SCALE,                       2, {"root", "scale"},          {"Snap notes to the nearest note of a scale (root, name)"}});
    commands_.add({"chord",        "",                           CHORD,                      -1, {"intervals"},              {"Stack notes at the given semitone intervals (a chord)"}});
    commands_.add({"latch",        "",                           LATCH,                      -1, {"(mode)"},                 {"Keep notes on after release; toggle (default) or hold"}});
    commands_.add({"mono",         "",                           MONO,                       -1, {"(priority)"},             {"Force monophony; priority last (default), low or high"}});
    commands_.add({"sustain",      "sustain-pedal",              SUSTAIN,                     0, {""},                       {"Apply the sustain pedal (CC 64) to the notes themselves"}});
    commands_.add({"sost",         "sostenuto-pedal",            SOSTENUTO,                   0, {""},                       {"Apply the sostenuto pedal (CC 66) to the notes themselves"}});
    commands_.add({"notecc",       "note-to-control-change",     NOTE_TO_CC,                  2, {"note", "cc"},             {"Turn a note into a Control Change (velocity as value)"}});
    commands_.add({"ccnote",       "control-change-to-note",     CC_TO_NOTE,                  2, {"cc", "note"},             {"Turn a Control Change into a note (64+ on, else off)"}});
    commands_.add({"notepc",       "note-to-program-change",     NOTE_TO_PROGRAM,             2, {"note", "program"},        {"Turn a note-on into a Program Change (note-off dropped)"}});
    commands_.add({"velscale",     "velocity-scale",             VELOCITY_SCALE,              1, {"factor"},                 {"Scale note-on velocity by a factor (clamped 1-127)"}});
    commands_.add({"velset",       "velocity-set",               VELOCITY_SET,                1, {"number"},                 {"Set a fixed note-on velocity (1-127)"}});
    commands_.add({"veladd",       "velocity-add",               VELOCITY_ADD,                1, {"number"},                 {"Add an offset to note-on velocity (clamped 1-127)"}});
    commands_.add({"velcurve",     "velocity-curve",             VELOCITY_CURVE,              1, {"gamma"},                  {"Apply a gamma curve to note-on velocity (1-127)"}});
    commands_.add({"velclip",      "velocity-clip",              VELOCITY_CLIP,               2, {"min", "max"},             {"Clamp note-on velocity into a min-max range"}});
    commands_.add({"velcomp",      "velocity-compress",          VELOCITY_COMPRESS,           1, {"amount"},                 {"Squeeze note-on velocity toward the mid-range (0-1)"}});
    commands_.add({"velinvert",    "velocity-invert",            VELOCITY_INVERT,             0, {""},                       {"Invert note-on velocity (soft becomes loud)"}});
    commands_.add({"ccmap",        "control-change-map",         CONTROL_CHANGE_MAP,          2, {"from", "to"},             {"Remap a Control Change controller number"}});
    commands_.add({"ccadd",        "control-change-add",         CONTROL_CHANGE_ADD,          2, {"number", "value"},        {"Add an offset to a controller's value (clamped 0-127)"}});
    commands_.add({"ccscale",      "control-change-scale",       CONTROL_CHANGE_SCALE,        2, {"number", "factor"},       {"Scale a controller's value by a factor (clamped 0-127)"}});
    commands_.add({"cccurve",      "control-change-curve",       CONTROL_CHANGE_CURVE,        2, {"number", "gamma"},        {"Apply a gamma curve to a controller's value"}});
    commands_.add({"ccinvert",     "control-change-invert",      CONTROL_CHANGE_INVERT,       1, {"number"},                 {"Invert a controller's value (0-127 mirrored)"}});
    commands_.add({"ccrescale",    "control-change-rescale",     CONTROL_CHANGE_RESCALE,      5, {"number", "inlow", "inhigh", "outlow", "outhigh"},
                                                                                                                             {"Rescale a controller's value from one range onto another (a reversed output range inverts)"}});
    commands_.add({"ccset",        "control-change-set",         CONTROL_CHANGE_SET,          2, {"number", "value"},        {"Set a fixed value for a controller (0-127)"}});
    commands_.add({"cc14add",      "control-change-14-add",      CC14_ADD,                    2, {"number", "value"},        {"Add an offset to a 14-bit CC value (clamped to its resolution)"}});
    commands_.add({"cc14scale",    "control-change-14-scale",    CC14_SCALE,                  2, {"number", "factor"},       {"Scale a 14-bit CC value by a factor (clamped to its resolution)"}});
    commands_.add({"cc14curve",    "control-change-14-curve",    CC14_CURVE,                  2, {"number", "gamma"},        {"Apply a gamma curve to a 14-bit CC value"}});
    commands_.add({"cc14invert",   "control-change-14-invert",   CC14_INVERT,                 1, {"number"},                 {"Invert a 14-bit CC value (0-16383 mirrored)"}});
    commands_.add({"cc14rescale",  "control-change-14-rescale",  CC14_RESCALE,                5, {"number", "inlow", "inhigh", "outlow", "outhigh"},
                                                                                                                             {"Rescale a 14-bit CC value from one range onto another (a reversed output range inverts)"}});
    commands_.add({"cc14set",      "control-change-14-set",      CC14_SET,                    2, {"number", "value"},        {"Set a fixed 14-bit CC value (0-16383)"}});
    commands_.add({"nrpnadd",      "nrpn-add",                   NRPN_ADD,                    2, {"param", "number"},        {"Add an offset to an NRPN value (clamped to its resolution)"}});
    commands_.add({"nrpnscale",    "nrpn-scale",                 NRPN_SCALE,                  2, {"param", "factor"},        {"Scale an NRPN value by a factor (clamped to its resolution)"}});
    commands_.add({"nrpncurve",    "nrpn-curve",                 NRPN_CURVE,                  2, {"param", "gamma"},         {"Apply a gamma curve to an NRPN value"}});
    commands_.add({"nrpninvert",   "nrpn-invert",                NRPN_INVERT,                 1, {"param"},                  {"Invert an NRPN value (mirrored in its resolution)"}});
    commands_.add({"nrpnrescale",  "nrpn-rescale",               NRPN_RESCALE,                5, {"param", "inlow", "inhigh", "outlow", "outhigh"},
                                                                                                                             {"Rescale an NRPN value from one range onto another (a reversed output range inverts)"}});
    commands_.add({"nrpnset",      "nrpn-set",                   NRPN_SET,                    2, {"param", "value"},         {"Set a fixed NRPN value (scaled to its resolution)"}});
    commands_.add({"rpnadd",       "rpn-add",                    RPN_ADD,                     2, {"param", "number"},        {"Add an offset to an RPN value (clamped to its resolution)"}});
    commands_.add({"rpnscale",     "rpn-scale",                  RPN_SCALE,                   2, {"param", "factor"},        {"Scale an RPN value by a factor (clamped to its resolution)"}});
    commands_.add({"rpncurve",     "rpn-curve",                  RPN_CURVE,                   2, {"param", "gamma"},         {"Apply a gamma curve to an RPN value"}});
    commands_.add({"rpninvert",    "rpn-invert",                 RPN_INVERT,                  1, {"param"},                  {"Invert an RPN value (mirrored in its resolution)"}});
    commands_.add({"rpnrescale",   "rpn-rescale",                RPN_RESCALE,                 5, {"param", "inlow", "inhigh", "outlow", "outhigh"},
                                                                                                                             {"Rescale an RPN value from one range onto another (a reversed output range inverts)"}});
    commands_.add({"rpnset",       "rpn-set",                    RPN_SET,                     2, {"param", "value"},         {"Set a fixed RPN value (scaled to its resolution)"}});
    commands_.add({"pcmap",        "program-change-map",         PROGRAM_CHANGE_MAP,          2, {"from", "to"},             {"Remap a Program Change number"}});
    commands_.add({"pcadd",        "program-change-add",         PROGRAM_CHANGE_ADD,          1, {"number"},                 {"Add an offset to Program Change number (clamped 0-127)"}});
    commands_.add({"cpadd",        "channel-pressure-add",       CHANNEL_PRESSURE_ADD,        1, {"number"},                 {"Add an offset to Channel Pressure (clamped 0-127)"}});
    commands_.add({"cpscale",      "channel-pressure-scale",     CHANNEL_PRESSURE_SCALE,      1, {"factor"},                 {"Scale Channel Pressure by a factor (clamped 0-127)"}});
    commands_.add({"cpset",        "channel-pressure-set",       CHANNEL_PRESSURE_SET,        1, {"number"},                 {"Set a fixed Channel Pressure value (0-127)"}});
    commands_.add({"cpcurve",      "channel-pressure-curve",     CHANNEL_PRESSURE_CURVE,      1, {"gamma"},                  {"Apply a gamma curve to Channel Pressure"}});
    commands_.add({"cpinvert",     "channel-pressure-invert",    CHANNEL_PRESSURE_INVERT,     0, {""},                       {"Invert Channel Pressure (0-127 mirrored)"}});
    commands_.add({"pbadd",        "pitch-bend-add",             PITCH_BEND_ADD,              1, {"number"},                 {"Add an offset to Pitch Bend (clamped 0-16383)"}});
    commands_.add({"pbscale",      "pitch-bend-scale",           PITCH_BEND_SCALE,            1, {"factor"},                 {"Scale Pitch Bend around center by a factor (0-16383)"}});
    commands_.add({"pbset",        "pitch-bend-set",             PITCH_BEND_SET,              1, {"number"},                 {"Set a fixed Pitch Bend value (0-16383)"}});
    commands_.add({"pbinvert",     "pitch-bend-invert",          PITCH_BEND_INVERT,           0, {""},                       {"Invert Pitch Bend around the center (up becomes down)"}});
    commands_.add({"js",           "javascript",                 JAVASCRIPT,                  1, {"code"},                   {"Transform each message with this script"}});
    commands_.add({"jsf",          "javascript-file",            JAVASCRIPT_FILE,             1, {"path"},                   {"Transform each message with the script in this file"}});
    commands_.add({"mpe",          "",                           MPE_RELOCATE,                2, {"zone[:n]", "zone[:n]"},   {"Relocate an MPE stream between zones (e.g. lower upper), remapping channels"}, "MPE routing"});
    commands_.add({"mpemono",      "mpe-mono",                   MPE_COLLAPSE,                2, {"zone[:n]", "channel"},    {"Collapse an MPE zone onto a single channel for non-MPE gear (e.g. upper 1)"}});
    commands_.add({"mpexp",        "mpe-expand",                 MPE_EXPAND,                  2, {"channel", "zone[:n]"},    {"Spread a channel's notes across an MPE zone's member channels (e.g. 1 lower)"}});
    commands_.add({"mpesplit",     "mpe-split",                  MPE_SPLIT,                  -1, {"zone[:n]", "(channel)"},  {"Distribute an MPE zone's voices over the output ports, one per port, each rechanneled to channel (default 1)"}});
    commands_.add({"mpebend",      "mpe-bend",                   MPE_BEND,                    3, {"zone[:n]", "from", "to"}, {"Rescale member-channel pitch bend from one semitone range to another"}});
    commands_.add({"mpesens",      "mpe-sensitivity",            MPE_SENS,                    2, {"zone[:n]", "semitones"},  {"Declare a member-channel pitch bend range (RPN 0) for synths that honor it"}});
    commands_.add({"convert",      "",                           CONVERT,                     4, {"srctype", "[number]", "dsttype", "[number]"},
                                                                                                                             {"Convert a value between cc, cc14, rpn, nrpn, pb, cp, pc & pp. Types cc, cc14, rpn and nrpn take a controller or parameter number and pp a note (optional on a source, meaning any note), while pb, cp and pc take none; the value is rescaled to the destination resolution"}, "Conversion"});

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

ApplicationState::~ApplicationState()
{
    stopOutputSender();
}

void ApplicationState::startOutputSender()
{
    if (senderThread_.joinable())
    {
        return;
    }
    senderShouldExit_ = false;
    senderThread_ = std::thread([this]
    {
        for (;;)
        {
            std::deque<OutgoingMidi> batch;
            {
                std::unique_lock<std::mutex> lock(sendQueueMutex_);
                sendQueueCv_.wait(lock, [this] { return senderShouldExit_.load() || !sendQueue_.empty(); });
                if (senderShouldExit_.load() && sendQueue_.empty())
                {
                    return;
                }
                batch.swap(sendQueue_);   // drain everything pending in one go, in order
            }
            for (auto& item : batch)
            {
                if (item.out != nullptr)
                {
                    item.out->sendMessageNow(item.msg);
                }
            }
        }
    });
}

void ApplicationState::stopOutputSender()
{
    if (!senderThread_.joinable())
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(sendQueueMutex_);
        senderShouldExit_ = true;
    }
    sendQueueCv_.notify_one();
    senderThread_.join();   // drains any messages still queued before returning
}

void ApplicationState::enqueueSend(MidiOutput* out, const MidiMessage& msg)
{
    if (out == nullptr)
    {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(sendQueueMutex_);
        sendQueue_.push_back({ out, msg });
    }
    sendQueueCv_.notify_one();
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
    else if (cmdLineParams.size() == 2
             && cmdLineParams[0] == "--schema"
             && cmdLineParams[1].equalsIgnoreCase("json"))
    {
        printSchemaJson();
        app.systemRequestedQuit();
        return;
    }
    else if (cmdLineParams.size() == 1 && cmdLineParams[0] == "--mcp")
    {
        initialiseScripting();
        // routes can be started at any time over MCP, so the sender and the
        // reconnect timer run for the whole session; requests are read on a
        // background thread so the message loop (and with it the timer) keeps
        // running, and each request is handled on the message thread
        startOutputSender();
        startTimer(200);
        mcpServer_ = std::make_unique<McpServer>(*this);
        mcpServer_->start();
        return;
    }

    initialiseScripting();

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
        startOutputSender();
        startTimer(200);
        readStdinMidi();
        app.systemRequestedQuit();
    }
    else
    {
        startOutputSender();
        startTimer(200);
    }
}

void ApplicationState::shutdown()
{
    // stop the reconnect timer first so it cannot enqueue more sends, then flush
    // any panic/notes-off through the sender before it is torn down
    stopTimer();

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

    stopOutputSender();   // drains the queue (including any panic) before returning

    mcpServer_.reset();   // joins the reader thread, which has already finished:
                          // quitting happens when the MCP client closes stdin
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
    OwnedArray<Route>& target = parseTarget_ != nullptr ? *parseTarget_ : routes_;
    return target.isEmpty() ? nullptr : target.getLast();
}

Route* ApplicationState::routeForNewInput()
{
    // keep accumulating inputs on the current route until it has outputs; once
    // outputs have been added, a further input starts a new route
    Route* route = currentRoute();
    if (route == nullptr || !route->outputs.isEmpty())
    {
        route = new Route();
        route->id = nextRouteId_++;
        (parseTarget_ != nullptr ? *parseTarget_ : routes_).add(route);
    }
    return route;
}

void ApplicationState::parseParametersInto(OwnedArray<Route>& target, StringArray& parameters)
{
    // routes created while parsing land in the staging list instead of routes_,
    // so live routing is never touched while the new routes open their devices;
    // the caller splices them into routes_ under the callback lock afterwards,
    // or discards them when parseErrors_ reports semantic failures
    parseErrors_.clearQuick();
    parseTarget_ = &target;
    parseParameters(parameters);
    parseTarget_ = nullptr;
    pendingNegate_ = false;   // a trailing "not" must not negate the next call's first filter
}

void ApplicationState::reportParseError(const String& message)
{
    std::cerr << message << std::endl;
    parseErrors_.add(message);
}

void ApplicationState::timerCallback()
{
    // Enumerate CoreMIDI devices WITHOUT holding midiCallbackLock_. These queries
    // can take milliseconds, and doing them under the lock stalls the real-time
    // MIDI input callback (which needs the same lock), risking dropped packets.
    auto availableIns  = MidiInput::getAvailableDevices();
    auto availableOuts = MidiOutput::getAvailableDevices();
    StringArray inNames;
    for (auto&& d : availableIns)
    {
        inNames.add(d.name);
    }

    // The route graph is built entirely during startup and never changes once the
    // timer is running, so it can be walked without the lock. The lock is taken only
    // in short bursts around the connection fields the MIDI callback also reads, and
    // never across a blocking CoreMIDI open below, so the callback waits as little as
    // possible (holding it across an openDevice was enough to drop incoming packets).
    for (auto* route : routes_)
    {
        for (auto* input : route->inputs)
        {
            if (input->isVirtual || input->isStdin)
            {
                continue;
            }

            // decide what to do under a brief lock, then act on it unlocked
            String lostName;    // a previously connected port that just vanished
            String toConnect;   // an inName still waiting to be (re)connected
            {
                const ScopedLock sl(midiCallbackLock_);
                if (input->fullInName.isNotEmpty() && !inNames.contains(input->fullInName))
                {
                    lostName = input->fullInName;
                    input->fullInName = String();
                    input->midiIn = nullptr;
                }
                else if (input->inName.isNotEmpty() && input->midiIn == nullptr)
                {
                    toConnect = input->inName;
                }
            }

            if (lostName.isNotEmpty())
            {
                std::cerr << "MIDI input port \"" << lostName << "\" got disconnected, waiting" << std::endl;
                if (route->panic)
                {
                    const ScopedLock sl(midiCallbackLock_);
                    sendPanic(*route);
                }
            }
            else if (toConnect.isNotEmpty())
            {
                // resolve a device from the snapshot (exact name first, then a
                // substring match) and open it WITHOUT the lock; only the swap-in is
                // done locked, and only if the input still wants it
                String identifier, fullName;
                for (auto&& d : availableIns)
                {
                    if (d.name == toConnect) { identifier = d.identifier; fullName = d.name; break; }
                }
                if (identifier.isEmpty())
                {
                    for (auto&& d : availableIns)
                    {
                        if (d.name.containsIgnoreCase(toConnect)) { identifier = d.identifier; fullName = d.name; break; }
                    }
                }

                if (identifier.isNotEmpty())
                {
                    std::unique_ptr<MidiInput> opened = MidiInput::openDevice(identifier, this);
                    if (opened)
                    {
                        opened->start();
                        bool connected = false;
                        {
                            const ScopedLock sl(midiCallbackLock_);
                            if (input->midiIn == nullptr && input->inName == toConnect)
                            {
                                input->midiIn.swap(opened);
                                input->fullInName = fullName;
                                connected = true;
                            }
                        }
                        if (connected)
                        {
                            std::cerr << "Connected to MIDI input port \"" << fullName << "\"" << std::endl;
                        }
                        // if not adopted, `opened` closes the device as it goes out of scope
                    }
                }
            }
        }

        // retry any output ports that could not be opened yet, opening unlocked
        for (auto* dest : route->outputs)
        {
            String toOpen;
            {
                const ScopedLock sl(midiCallbackLock_);
                if (!dest->isVirtual && !dest->isStdout && dest->out == nullptr && dest->name.isNotEmpty())
                {
                    toOpen = dest->name;
                }
            }
            if (toOpen.isEmpty())
            {
                continue;
            }

            String identifier, fullName;
            for (auto&& device : availableOuts)
            {
                if (device.name.containsIgnoreCase(toOpen)) { identifier = device.identifier; fullName = device.name; break; }
            }

            if (identifier.isNotEmpty())
            {
                std::unique_ptr<MidiOutput> opened = MidiOutput::openDevice(identifier);
                if (opened)
                {
                    bool connected = false;
                    {
                        const ScopedLock sl(midiCallbackLock_);
                        if (dest->out == nullptr && dest->name == toOpen)
                        {
                            dest->out.swap(opened);
                            dest->fullName = fullName;
                            connected = true;
                        }
                    }
                    if (connected)
                    {
                        std::cerr << "Connected to MIDI output port \"" << fullName << "\"" << std::endl;
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
            if (conversion::specComplete(currentCommand_.opts_))
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
                reportParseError("Ignoring \"out\" because no input route was started yet (use \"in\" first)");
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
                reportParseError("Ignoring \"vout\" because no input route was started yet (use \"in\" first)");
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
                    reportParseError("Couldn't create file \"" + path + "\"");
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
                reportParseError("Ignoring \"syf\" because no input route was started yet (use \"in\" first)");
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
                reportParseError("Ignoring \"panic\" because no input route was started yet (use \"in\" first)");
            }
            break;
        case NOT:
            pendingNegate_ = true;
            return;
        default:
            // everything else is a processing command (filter, transform, MPE
            // operation, conversion or split) belonging to the current route
            if (auto* route = currentRoute())
            {
                const String error = addProcessingCommand(*route, cmd, negate);
                if (error.isNotEmpty())
                {
                    reportParseError(error);
                    JUCEApplicationBase::getInstance()->setApplicationReturnValue(EXIT_FAILURE);
                }
            }
            else
            {
                reportParseError("Ignoring \"" + cmd.param_ + "\" because no input route was started yet (use \"in\" first)");
            }
            break;
    }

    pendingNegate_ = false;
}

String ApplicationState::addProcessingCommand(Route& route, ApplicationCommand cmd, bool negate)
{
    switch (cmd.command_)
    {
        case JAVASCRIPT_FILE:
        {
            String path(cmd.opts_[0]);
            File file = File::getCurrentWorkingDirectory().getChildFile(path);
            if (!file.existsAsFile())
            {
                return "Couldn't find file \"" + path + "\"";
            }
            cmd.opts_.set(0, file.loadFileAsString());
            route.transforms.add(cmd);
            hasScript_ = true;
            return {};
        }
        case CONVERT:
        {
            // the collected options have a dynamic shape ("srctype [num] dsttype
            // [num]"); normalize them to a fixed [srctype, srcnum, dsttype, dstnum]
            // form (with "0" where a type takes no number) for the converter
            String srcType, srcNum, dstType, dstNum;
            if (conversion::parseSpec(cmd.opts_, srcType, srcNum, dstType, dstNum) < 0)
            {
                return "Invalid convert specification, expected \"<srctype> [number] <dsttype> [number]\""
                       " with types cc, cc14, rpn, nrpn, pb, cp, pc or pp";
            }
            cmd.opts_.clearQuick();
            cmd.opts_.add(srcType);
            cmd.opts_.add(srcNum);
            cmd.opts_.add(dstType);
            cmd.opts_.add(dstNum);
            route.converters.add(cmd);
            route.convertRules.clearQuick();   // recompiled on the next message
            return {};
        }
        case MPE_RELOCATE:
        case MPE_COLLAPSE:
        case MPE_EXPAND:
        case MPE_BEND:
        case MPE_SENS:
        {
            // validate the zone tokens up front so mistakes are reported when the
            // command is added rather than silently doing nothing at routing time
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
                return "Invalid MPE zone, expected \"lower\" or \"upper\" with an optional"
                       " \":<members>\" count (for example \"lower:7\")";
            }
            route.mpeOps.add(cmd);
            return {};
        }
        case CC14_ADD:
        case CC14_SCALE:
        case CC14_CURVE:
        case CC14_INVERT:
        case CC14_RESCALE:
        case CC14_SET:
        case NRPN_ADD:
        case NRPN_SCALE:
        case NRPN_CURVE:
        case NRPN_INVERT:
        case NRPN_RESCALE:
        case NRPN_SET:
        case RPN_ADD:
        case RPN_SCALE:
        case RPN_CURVE:
        case RPN_INVERT:
        case RPN_RESCALE:
        case RPN_SET:
            // the 14-bit CC and RPN/NRPN value transforms are assembled and
            // regenerated in the converter stage, so they live alongside the
            // convert rules
            route.converters.add(cmd);
            route.convertRules.clearQuick();   // recompiled on the next message
            return {};
        case MPE_SPLIT:
        {
            mpe::Zone zone;
            if (cmd.opts_.isEmpty() || !mpe::parseZone(cmd.opts_[0], zone))
            {
                return "Invalid \"mpesplit\" zone, expected \"lower\" or \"upper\" with an optional"
                       " \":<members>\" count (for example \"lower:7\")";
            }
            route.outputSplit.clearQuick();
            route.outputSplit.add(cmd);
            return {};
        }
        default:
            if (cmd.isFilter())
            {
                cmd.negate_ = negate;
                route.filters.add(cmd);
                return {};
            }
            if (cmd.isTransform())
            {
                route.transforms.add(cmd);
                if (cmd.command_ == JAVASCRIPT)
                {
                    hasScript_ = true;
                }
                return {};
            }
            return "\"" + cmd.param_ + "\" is not a route processing command";
    }
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
        reportParseError("Couldn't create virtual MIDI input port \"" + name + "\"");
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
        reportParseError("Couldn't create virtual MIDI output port \"" + name + "\"");
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
            enqueueSend(dest->out.get(), MidiMessage::controllerEvent(channel, 64, 0));   // sustain off
            enqueueSend(dest->out.get(), MidiMessage::controllerEvent(channel, 66, 0));   // sostenuto off
            enqueueSend(dest->out.get(), MidiMessage::controllerEvent(channel, 123, 0));  // all notes off
        }
    }

    // the all-notes-off above silences any latched or pedal-held notes, so drop
    // their state too
    for (auto* input : route.inputs)
    {
        input->latch.clear();
        input->mono.reset();
        input->sustain.clear();
        input->sostenuto.clear();
        input->conv.pressure.reset();
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
            enqueueSend(dest->out.get(), MidiMessage::controllerEvent(channel, 123, 0));  // all notes off
            enqueueSend(dest->out.get(), MidiMessage::controllerEvent(channel, 121, 0));  // reset all controllers
        }
    }

    // a zone change flushes stuck notes downstream, so drop latched and
    // pedal-held state as well
    for (auto* input : route.inputs)
    {
        input->latch.clear();
        input->mono.reset();
        input->sustain.clear();
        input->sostenuto.clear();
        input->conv.pressure.reset();
    }
}

void ApplicationState::routeMessage(Route& route, RouteInput& input, const MidiMessage& msg)
{
    Array<MidiMessage> outMsgs;
    Array<int> outPorts;
    processRouteMessage(route, input, msg, outMsgs, outPorts);
    sendRouted(route, outMsgs, outPorts);

    if (monitor_)
    {
        for (auto& m : outMsgs)
        {
            printMonitor(input.fullInName, m);
        }
    }
}

bool ApplicationState::processRouteMessage(Route& route, RouteInput& input, const MidiMessage& msg,
                                           Array<MidiMessage>& outMsgs, Array<int>& outPorts)
{
    // when the safety net is on, flush stuck notes if a zone is reconfigured
    bool zoneReset = false;
    if (route.panic && input.mcm.reconfigures(msg))
    {
        sendZoneReset(route);
        zoneReset = true;
    }

    if (!passesFilters(route, input, msg))
    {
        return zoneReset;
    }

    Array<MidiMessage> outputMessages;
    for (auto& transformed : applyTransforms(route, input, msg))
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
            outMsgs.add(outMsg);
            outPorts.add(-1);
        }
        else
        {
            // split routing: each message is dispatched to a specific output
            // (a voice's port) or broadcast to all (port -1)
            processSplit(route, outMsg, outMsgs, outPorts);
        }
    }

    return zoneReset;
}

void ApplicationState::sendRouted(Route& route, const Array<MidiMessage>& outMsgs, const Array<int>& outPorts)
{
    for (int i = 0; i < outMsgs.size(); ++i)
    {
        const MidiMessage& m = outMsgs.getReference(i);
        if (outPorts[i] < 0)
        {
            for (auto* dest : route.outputs)
            {
                sendToDest(dest, m);
            }
        }
        else if (outPorts[i] < route.outputs.size())
        {
            sendToDest(route.outputs[outPorts[i]], m);
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
        enqueueSend(dest->out.get(), msg);
    }
}

void ApplicationState::processSplit(Route& route, const MidiMessage& msg, Array<MidiMessage>& outMsgs, Array<int>& outPorts)
{
    const ApplicationCommand& cmd = route.outputSplit.getReference(0);
    cmd.ensureCompiled(*this);
    const int targetCh = cmd.copts_.size() > 1 ? jlimit(1, 16, cmd.copts_[1].intValue) : 1;
    mpe::split(route.mpeSplit, cmd.copts_[0].zone, targetCh, route.outputs.size(), msg, outMsgs, outPorts);
}

// the cc14range filter: passes the MSB (controller 0-31) and LSB (32-63) halves
// of a 14-bit CC whose assembled value is in range. It needs the input's MSB
// memory, so it is matched here instead of in ApplicationCommand::matches. An
// MSB half is judged with LSB 0 (the MIDI convention after an MSB change); an
// LSB half is judged with the exact assembled value.
static bool matchesCc14Range(const ApplicationCommand& cmd, RouteInput& input,
                             const MidiMessage& msg, int channelLow, int channelHigh)
{
    if (!ApplicationCommand::checkChannel(msg, channelLow, channelHigh) || !msg.isController())
    {
        return false;
    }

    const int cc = msg.getControllerNumber();
    const int n = cmd.copts_[0].value7 & 0x1f;
    int value;
    if (cc == n)
    {
        input.cc14RangeMsb[msg.getChannel() - 1][n] = (uint8) msg.getControllerValue();
        value = msg.getControllerValue() << 7;
    }
    else if (cc == n + 32)
    {
        value = (input.cc14RangeMsb[msg.getChannel() - 1][n] << 7) | msg.getControllerValue();
    }
    else
    {
        return false;
    }

    return value >= jlimit(0, 16383, cmd.copts_[1].intValue) &&
           value <= jlimit(0, 16383, cmd.copts_[2].intValue);
}

bool ApplicationState::passesFilters(Route& route, RouteInput& input, const MidiMessage& msg)
{
    if (route.filters.isEmpty())
    {
        return true;
    }

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
            // the channel filter can be a single channel or an inclusive "lo..hi"
            // range; a low of 0 means "any channel"
            cmd.ensureCompiled(*this);
            const int lo = jlimit(0, 16, cmd.copts_[0].selIntLo);
            const int hi = jlimit(0, 16, cmd.copts_[0].selIntHi);
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
            bool m;
            if (cmd.command_ == CONTROL_CHANGE_14BIT_RANGE)
            {
                cmd.ensureCompiled(*this);
                m = matchesCc14Range(cmd, input, msg, channelLow, channelHigh);
            }
            else
            {
                m = cmd.matches(*this, msg, channelLow, channelHigh);
            }
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

Array<MidiMessage> ApplicationState::applyTransforms(Route& route, RouteInput& input, const MidiMessage& msg)
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
            else if (cmd.command_ == CHORD)
            {
                // the played note passes through and, for note-ons and
                // note-offs, gets extra notes stacked at the given intervals
                next.add(m);
                if (m.isNoteOnOrOff())
                {
                    const int base = m.getNoteNumber();
                    const int channel = m.getChannel();
                    const int velocity = m.getVelocity();
                    const bool on = m.isNoteOn();
                    const double ts = m.getTimeStamp();
                    cmd.ensureCompiled(*this);
                    for (const auto& interval : cmd.copts_)
                    {
                        const int n = base + interval.intValue;
                        if (n < 0 || n > 127) continue;   // out-of-range notes are dropped
                        MidiMessage chordNote = on ? MidiMessage::noteOn(channel, n, (uint8)velocity)
                                                   : MidiMessage::noteOff(channel, n, (uint8)velocity);
                        chordNote.setTimeStamp(ts);
                        next.add(chordNote);
                    }
                }
            }
            else if (cmd.command_ == LATCH)
            {
                // keeps notes sounding after release; toggle flips a note per
                // press, hold replaces the held chord when a new gesture starts
                cmd.ensureCompiled(*this);
                const bool hold = cmd.copts_.size() > 0 && cmd.copts_[0].keyword == 1;
                input.latch.process(hold, m, next);
            }
            else if (cmd.command_ == MONO)
            {
                // forces one note at a time, choosing among held notes by priority
                cmd.ensureCompiled(*this);
                MonoState::Priority priority = MonoState::Last;
                if (cmd.copts_.size() > 0)
                {
                    if      (cmd.copts_[0].keyword == 2) { priority = MonoState::Low; }
                    else if (cmd.copts_[0].keyword == 3) { priority = MonoState::High; }
                }
                input.mono.process(priority, m, next);
            }
            else if (cmd.command_ == SUSTAIN)
            {
                // pre-applies the sustain pedal to the notes, for synths that
                // ignore CC 64
                input.sustain.process(false, m, next);
            }
            else if (cmd.command_ == SOSTENUTO)
            {
                // pre-applies the sostenuto pedal to the notes held at press,
                // for synths that ignore CC 66
                input.sostenuto.process(true, m, next);
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
    // hand the message to the matching MPE operation (Mpe.cpp) with its compiled
    // options (zones, channels, ranges) and per-input state slot, indexed by zone
    // side so Lower and Upper operations never share state
    cmd.ensureCompiled(*this);
    switch (cmd.command_)
    {
        case MPE_RELOCATE:
        {
            const mpe::Zone& src = cmd.copts_[0].zone;
            const mpe::Zone& dst = cmd.copts_[1].zone;
            mpe::relocate(input.mpeRelocate[src.lower ? 0 : 1], src, dst, msg, output);
            break;
        }
        case MPE_COLLAPSE:
        {
            const mpe::Zone& src = cmd.copts_[0].zone;
            const int target = jlimit(1, 16, cmd.copts_[1].intValue);
            mpe::collapse(input.mpeCollapse[src.lower ? 0 : 1], src, target, msg, output);
            break;
        }
        case MPE_EXPAND:
        {
            const int srcCh = jlimit(1, 16, cmd.copts_[0].intValue);
            const mpe::Zone& dst = cmd.copts_[1].zone;
            mpe::expand(input.mpeAlloc[dst.lower ? 0 : 1], srcCh, dst, msg, output);
            break;
        }
        case MPE_BEND:
        {
            mpe::rescaleBend(cmd.copts_[0].zone, cmd.copts_[1].number, cmd.copts_[2].number, msg, output);
            break;
        }
        case MPE_SENS:
        {
            const mpe::Zone& zone = cmd.copts_[0].zone;
            const int semitones = jlimit(0, 96, cmd.copts_[1].intValue);
            mpe::declareSensitivity(input.mpeSens[zone.lower ? 0 : 1], zone, semitones, msg, output);
            break;
        }
        default:
            output.add(msg);
            break;
    }
}

//==============================================================================
// compile the route's string convert/transform commands to conversion::Rule
// numbers, so the real-time path never parses option strings. Numbers are decoded
// with the current hex/octave settings, matching the previous per-message behaviour.
void ApplicationState::rebuildConvertRules(Route& route)
{
    route.convertRules.clearQuick();
    for (const auto& cmd : route.converters)
    {
        conversion::Rule r;
        if (conversion::isValueTransform(cmd.command_))
        {
            r.isTransform = true;
            r.op    = cmd.command_;
            r.nrpn  = (cmd.command_ == NRPN_ADD || cmd.command_ == NRPN_SCALE ||
                       cmd.command_ == NRPN_CURVE || cmd.command_ == NRPN_INVERT ||
                       cmd.command_ == NRPN_RESCALE || cmd.command_ == NRPN_SET);
            r.param = asDecOrHexIntValue(cmd.opts_[0]);
            if (cmd.command_ == CC14_RESCALE || cmd.command_ == NRPN_RESCALE || cmd.command_ == RPN_RESCALE)
            {
                r.inLo  = jlimit(0, 16383, asDecOrHexIntValue(cmd.opts_[1]));
                r.inHi  = jlimit(0, 16383, asDecOrHexIntValue(cmd.opts_[2]));
                r.outLo = jlimit(0, 16383, asDecOrHexIntValue(cmd.opts_[3]));
                r.outHi = jlimit(0, 16383, asDecOrHexIntValue(cmd.opts_[4]));
            }
            else if (cmd.opts_.size() > 1)   // invert takes only the controller
            {
                r.addAmount = asDecOrHexIntValue(cmd.opts_[1]);
                r.factor    = cmd.opts_[1].getFloatValue();
                r.gamma     = cmd.opts_[1].getDoubleValue();
            }
        }
        else
        {
            conversion::parseType(cmd.opts_[0], r.src);
            conversion::parseType(cmd.opts_[2], r.dst);
            // a source pp keeps -1 (any note) or resolves its note; other sources
            // take a controller/parameter number
            r.srcNum = (r.src == conversion::Pp)
                         ? (cmd.opts_[1] == "-1" ? -1 : (int) asNoteNumber(cmd.opts_[1]))
                         : asDecOrHexIntValue(cmd.opts_[1]);
            r.dstNum = (r.dst == conversion::Pp) ? (int) asNoteNumber(cmd.opts_[3])
                                                 : asDecOrHexIntValue(cmd.opts_[3]);
            r.srcBits = conversion::valueBits(r.src);
            r.method  = conversion::scaleMethodFor(r.src, r.srcNum, r.dst, r.dstNum);
        }
        route.convertRules.add(r);
    }
}

void ApplicationState::processConverters(Route& route, RouteInput& input, const MidiMessage& msg, Array<MidiMessage>& output)
{
    // converters are only appended at setup, so a size mismatch means the compiled
    // cache is stale; rebuild it once and then match rules without parsing strings
    if (route.convertRules.size() != route.converters.size())
    {
        rebuildConvertRules(route);
    }

    conversion::processMessage(route.convertRules, input.conv, msg, output);
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

textmidi::Format ApplicationState::textFormat() const
{
    return { useHexadecimalsByDefault_, noteNumbersOutput_, octaveMiddleC_ };
}

void ApplicationState::printVersion()
{
    std::cout << ProjectInfo::projectName << " v" << ProjectInfo::versionString << std::endl;
    std::cout << "https://github.com/gbevin/RouteMIDI" << std::endl;
}

String ApplicationState::schemaJson() const
{
    return schema::commandsJson(commands_, DEFAULT_OCTAVE_MIDDLE_C);
}

void ApplicationState::printSchemaJson()
{
    std::cout << schemaJson() << std::endl;
}

void ApplicationState::initialiseScripting()
{
    scriptEngine_.maximumExecutionTime = RelativeTime::days(365);
    scriptMidiMessage_ = new ScriptMidiMessageClass(*this);
    scriptEngine_.registerNativeObject("MIDI", scriptMidiMessage_);
    scriptEngine_.registerNativeObject("OSC",  new ScriptOscClass());
    scriptEngine_.registerNativeObject("Util", new ScriptUtilClass());
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
              << "Commands:" << std::endl << std::endl;
    // the columns follow the longest command name, so long commands cannot
    // push their options and description out of alignment
    int longestCommand = 0;
    for (auto&& cmd : commands_)
    {
        longestCommand = jmax(longestCommand, cmd.param_.length());
    }
    const int optionColumn = longestCommand + 3;      // where the option names start
    const int descriptionColumn = optionColumn + 11;  // where the description starts
    bool firstSection = true;
    for (auto&& cmd : commands_)
    {
        // print a flush-left section header before the command that starts a group
        if (cmd.section_.isNotEmpty())
        {
            if (!firstSection)
            {
                std::cout << std::endl;
            }
            std::cout << cmd.section_ << ":" << std::endl;
            firstSection = false;
        }

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
                line << "  " << cmd.param_.paddedRight(' ', optionColumn - 3) << " ";
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
    std::cout << std::endl;
    auto builtin = [&](const String& flag, const String& description)
    {
        // wrapped like the command descriptions, so no line passes 80 columns
        const StringArray lines = wrapText(description, 80 - descriptionColumn);
        for (int i = 0; i < lines.size(); ++i)
        {
            std::cout << (i == 0 ? ("  " + flag).paddedRight(' ', descriptionColumn)
                                 : String().paddedRight(' ', descriptionColumn))
                      << lines.getReference(i) << std::endl;
        }
    };
    builtin("-h  or  --help", "Print Help (this message) and exit");
    builtin("--version", "Print version information and exit");
    builtin("--schema json", "Print machine-readable command JSON and exit [experimental]");
    builtin("--mcp", "Run a stdio MCP server [experimental]");
    builtin("--", "Read commands from standard input until it's closed");
    std::cout << std::endl;
    std::cout << "Use \"--schema json\" for command metadata for scripts, MCP servers and" << std::endl
              << "AI agents. Use \"--mcp\" to let MCP clients control RouteMIDI over stdio." << std::endl;
    std::cout << "These two features are experimental and fast-moving: their JSON and the MCP" << std::endl
              << "tools may change between releases. See AI.md for details." << std::endl;
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
