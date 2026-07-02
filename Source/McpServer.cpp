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

#include "McpServer.h"

#include "ApplicationState.h"

#if JUCE_WINDOWS
#include <fcntl.h>
#include <io.h>
#endif

static Array<var> stringArrayToVarArray(const StringArray& strings, bool omitEmpty)
{
    Array<var> result;
    for (auto&& value : strings)
    {
        if (!omitEmpty || value.isNotEmpty())
        {
            result.add(value);
        }
    }
    return result;
}

static DynamicObject* newObject()
{
    return new DynamicObject();
}

static Array<var> midiDeviceNames(const Array<MidiDeviceInfo>& devices)
{
    Array<var> names;
    for (auto&& device : devices)
    {
        names.add(device.name);
    }
    return names;
}

static DynamicObject* newStringSchema(const String& description)
{
    auto schema = newObject();
    schema->setProperty("type", "string");
    schema->setProperty("description", description);
    return schema;
}

static DynamicObject* newNoArgsSchema()
{
    auto schema = newObject();
    schema->setProperty("type", "object");
    schema->setProperty("properties", var(newObject()));
    return schema;
}

static DynamicObject* newIntegerProperty(const String& description)
{
    auto schema = newObject();
    schema->setProperty("type", "integer");
    schema->setProperty("description", description);
    return schema;
}

static DynamicObject* newCommandTokensProperty()
{
    auto itemSchema = newStringSchema("A single RouteMIDI command token");
    auto commandsSchema = newObject();
    commandsSchema->setProperty("type", "array");
    commandsSchema->setProperty("description", "RouteMIDI command tokens");
    commandsSchema->setProperty("items", var(itemSchema));
    return commandsSchema;
}

static DynamicObject* newStageProperty()
{
    auto stage = newObject();
    stage->setProperty("type", "string");
    stage->setProperty("description", "The processing stage holding the command");
    Array<var> allowed;
    allowed.add("filters");
    allowed.add("transforms");
    allowed.add("mpe");
    allowed.add("conversions");
    allowed.add("split");
    stage->setProperty("enum", var(allowed));
    return stage;
}

static DynamicObject* newObjectSchema(DynamicObject* properties, const StringArray& required)
{
    auto schema = newObject();
    schema->setProperty("type", "object");
    schema->setProperty("properties", var(properties));
    if (!required.isEmpty())
    {
        schema->setProperty("required", var(stringArrayToVarArray(required, false)));
    }
    return schema;
}

static DynamicObject* newRouteIdSchema()
{
    auto properties = newObject();
    properties->setProperty("route", var(newIntegerProperty("The route id, as reported by list_routes and start_route")));
    return newObjectSchema(properties, { "route" });
}

static DynamicObject* newAddCommandsSchema()
{
    auto properties = newObject();
    properties->setProperty("route", var(newIntegerProperty("The route id, as reported by list_routes and start_route")));
    properties->setProperty("commands", var(newCommandTokensProperty()));
    return newObjectSchema(properties, { "route", "commands" });
}

static DynamicObject* newStageIndexSchema(bool withCommands)
{
    auto properties = newObject();
    properties->setProperty("route", var(newIntegerProperty("The route id, as reported by list_routes and start_route")));
    properties->setProperty("stage", var(newStageProperty()));
    properties->setProperty("index", var(newIntegerProperty("The command's index within the stage, as reported by list_routes")));
    StringArray required { "route", "stage", "index" };
    if (withCommands)
    {
        properties->setProperty("commands", var(newCommandTokensProperty()));
        required.add("commands");
    }
    return newObjectSchema(properties, required);
}

static DynamicObject* newRouteCommandsSchema()
{
    auto properties = newObject();
    properties->setProperty("commands", var(newCommandTokensProperty()));
    return newObjectSchema(properties, { "commands" });
}

static DynamicObject* newMcpTool(const String& name, const String& title,
                                 const String& description,
                                 DynamicObject* inputSchema,
                                 bool readOnly)
{
    auto tool = newObject();
    tool->setProperty("name", name);
    tool->setProperty("title", title);
    tool->setProperty("description", description);
    tool->setProperty("inputSchema", var(inputSchema));

    auto annotations = newObject();
    annotations->setProperty("readOnlyHint", readOnly);
    tool->setProperty("annotations", var(annotations));
    return tool;
}

static Array<var> mcpTools()
{
    Array<var> tools;
    tools.add(var(newMcpTool("get_schema",
                             "Get RouteMIDI Command Schema",
                             "Return the machine-readable RouteMIDI command schema.",
                             newNoArgsSchema(),
                             true)));
    tools.add(var(newMcpTool("list_midi_ports",
                             "List MIDI Ports",
                             "Return the currently available MIDI input and output ports.",
                             newNoArgsSchema(),
                             true)));
    tools.add(var(newMcpTool("start_route",
                             "Start RouteMIDI Route",
                             "Start live RouteMIDI routing from explicit command tokens.",
                             newRouteCommandsSchema(),
                             false)));
    tools.add(var(newMcpTool("list_routes",
                             "List Active Routes",
                             "Return every active route: its id, the connection state of its "
                             "ports, and the commands in each processing stage (filters, "
                             "transforms, mpe, conversions, split) with their per-stage indexes.",
                             newNoArgsSchema(),
                             true)));
    tools.add(var(newMcpTool("stop_route",
                             "Stop Route",
                             "Stop and remove a route by id, sending all-notes-off to its "
                             "outputs first so nothing is left sounding.",
                             newRouteIdSchema(),
                             false)));
    tools.add(var(newMcpTool("panic_route",
                             "Panic Route",
                             "Send sustain-off and all-notes-off to a route's outputs and clear "
                             "its latch and mono state. Use after editing or removing stateful "
                             "commands (latch, mono, MPE operations) to release anything left "
                             "sounding.",
                             newRouteIdSchema(),
                             false)));
    tools.add(var(newMcpTool("add_commands",
                             "Add Commands To Route",
                             "Append processing commands (filters, transforms, MPE operations, "
                             "conversions) to a running route. Ports cannot be changed; commands "
                             "are appended at the end of their stage.",
                             newAddCommandsSchema(),
                             false)));
    tools.add(var(newMcpTool("remove_command",
                             "Remove Command From Route",
                             "Remove one command from a running route by stage name and index "
                             "(as reported by list_routes). Consider panic_route afterwards when "
                             "removing stateful commands.",
                             newStageIndexSchema(false),
                             false)));
    tools.add(var(newMcpTool("replace_command",
                             "Replace Command In Route",
                             "Replace one command of a running route in place, keeping its "
                             "position in the stage; the replacement must belong to the same "
                             "stage (for example swap 'scale C major' for 'scale D minor').",
                             newStageIndexSchema(true),
                             false)));
    return tools;
}

// the per-stage container a processing command lives in (mirroring where
// ApplicationState::addProcessingCommand places it)
static Array<ApplicationCommand>* stageContainer(Route& route, const String& stage)
{
    if (stage == "filters")
    {
        return &route.filters;
    }
    if (stage == "transforms")
    {
        return &route.transforms;
    }
    if (stage == "mpe")
    {
        return &route.mpeOps;
    }
    if (stage == "conversions")
    {
        return &route.converters;
    }
    if (stage == "split")
    {
        return &route.outputSplit;
    }
    return nullptr;
}

static var commandListToVar(const Array<ApplicationCommand>& list)
{
    Array<var> items;
    for (int i = 0; i < list.size(); ++i)
    {
        const auto& cmd = list.getReference(i);
        auto item = newObject();
        item->setProperty("index", i);
        item->setProperty("command", cmd.param_);
        item->setProperty("args", var(stringArrayToVarArray(cmd.opts_, false)));
        if (cmd.negate_)
        {
            item->setProperty("negated", true);
        }
        items.add(var(item));
    }
    return var(items);
}

// describes a route for the MCP client: its stable id, the connection state of
// its ports, and the commands of every processing stage with their indexes
static DynamicObject* routeToVar(const Route& route)
{
    Array<var> inputs;
    for (auto* input : route.inputs)
    {
        auto port = newObject();
        port->setProperty("name", input->inName);
        port->setProperty("connected", input->midiIn != nullptr);
        inputs.add(var(port));
    }
    Array<var> outputs;
    for (auto* dest : route.outputs)
    {
        auto port = newObject();
        port->setProperty("name", dest->name);
        port->setProperty("connected", dest->out != nullptr || dest->syxFile != nullptr);
        outputs.add(var(port));
    }

    auto result = newObject();
    result->setProperty("id", route.id);
    result->setProperty("inputs", var(inputs));
    result->setProperty("outputs", var(outputs));
    result->setProperty("filters", commandListToVar(route.filters));
    result->setProperty("transforms", commandListToVar(route.transforms));
    result->setProperty("mpe", commandListToVar(route.mpeOps));
    result->setProperty("conversions", commandListToVar(route.converters));
    result->setProperty("split", commandListToVar(route.outputSplit));
    return result;
}

// forgets the running state a removed or replaced command had accumulated, so
// it cannot keep affecting the stream (a removed latch must not keep swallowing
// note-offs, and a changed converter must be recompiled)
static void cleanupAfterCommandChange(Route& route, const ApplicationCommand& cmd)
{
    if (cmd.command_ == LATCH)
    {
        for (auto* input : route.inputs)
        {
            input->latch.clear();
        }
    }
    if (cmd.command_ == MONO)
    {
        for (auto* input : route.inputs)
        {
            input->mono.reset();
        }
    }
    if (cmd.command_ == MPE_SPLIT)
    {
        route.mpeSplit = mpe::Splitter();
    }
    if (cmd.command_ == CONVERT || conversion::isRpnTransform(cmd.command_))
    {
        route.convertRules.clearQuick();
    }
}

// a command with the parsed tokens it was given and whether a "not" preceded it
struct PendingCommand
{
    ApplicationCommand command;
    bool negate { false };
};

// parses tokens into processing commands the way the command-line parser would,
// folding a "not" into the following filter. The tokens must already have passed
// validateMcpCommandTokens. Returns an error, or fills the pending list.
static String collectProcessingCommands(const Array<ApplicationCommand>& table,
                                        const StringArray& tokens,
                                        Array<PendingCommand>& out)
{
    auto findCommand = [&table](const String& token) -> const ApplicationCommand*
    {
        for (const auto& cmd : table)
        {
            if (cmd.param_.equalsIgnoreCase(token) || cmd.altParam_.equalsIgnoreCase(token))
            {
                return &cmd;
            }
        }
        return nullptr;
    };
    auto isProcessing = [](const ApplicationCommand& cmd)
    {
        switch (cmd.command_)
        {
            case CONVERT:
            case NRPN_ADD:
            case NRPN_SCALE:
            case NRPN_CURVE:
            case RPN_ADD:
            case RPN_SCALE:
            case RPN_CURVE:
            case MPE_RELOCATE:
            case MPE_COLLAPSE:
            case MPE_EXPAND:
            case MPE_BEND:
            case MPE_SENS:
            case MPE_SPLIT:
                return true;
            default:
                return cmd.isFilter() || cmd.isTransform();
        }
    };

    PendingCommand current;
    bool collecting = false;
    bool pendingNegate = false;
    int remainingFixedArgs = 0;

    auto finishCurrent = [&]()
    {
        if (collecting)
        {
            out.add(current);
            collecting = false;
        }
    };

    for (const auto& token : tokens)
    {
        if (collecting && current.command.command_ == CONVERT)
        {
            current.command.opts_.add(token);
            if (conversion::specComplete(current.command.opts_))
            {
                finishCurrent();
            }
            continue;
        }
        if (collecting && remainingFixedArgs > 0)
        {
            current.command.opts_.add(token);
            remainingFixedArgs -= 1;
            if (remainingFixedArgs == 0)
            {
                finishCurrent();
            }
            continue;
        }

        const ApplicationCommand* cmd = findCommand(token);
        if (cmd == nullptr)
        {
            if (!collecting)
            {
                return "Unknown command: " + token;
            }
            // an optional argument of a variable-argument command
            current.command.opts_.add(token);
            continue;
        }

        finishCurrent();   // a new command ends a variable-argument command

        if (cmd->command_ == NOT)
        {
            pendingNegate = true;
            continue;
        }
        if (!isProcessing(*cmd))
        {
            return "\"" + token + "\" cannot be added to a running route";
        }

        current.command = *cmd;
        current.command.opts_.clearQuick();
        current.negate = pendingNegate;
        pendingNegate = false;
        collecting = true;
        remainingFixedArgs = jmax(0, cmd->expectedOptions_);
        if (remainingFixedArgs == 0 && cmd->expectedOptions_ >= 0 && cmd->command_ != CONVERT)
        {
            finishCurrent();   // takes no arguments
        }
    }
    finishCurrent();

    if (out.isEmpty())
    {
        return "No commands given";
    }
    return {};
}

static DynamicObject* newMcpTextContent(const String& text)
{
    auto content = newObject();
    content->setProperty("type", "text");
    content->setProperty("text", text);
    return content;
}

static DynamicObject* newMcpToolResult(const String& text, const var& structuredContent,
                                       bool isError)
{
    Array<var> content;
    content.add(var(newMcpTextContent(text)));

    auto result = newObject();
    result->setProperty("content", var(content));
    if (!structuredContent.isVoid())
    {
        result->setProperty("structuredContent", structuredContent);
    }
    result->setProperty("isError", isError);
    return result;
}

static DynamicObject* newMcpError(int code, const String& message)
{
    auto error = newObject();
    error->setProperty("code", code);
    error->setProperty("message", message);
    return error;
}

static DynamicObject* newMcpResponse(const var& id, DynamicObject* result)
{
    auto response = newObject();
    response->setProperty("jsonrpc", "2.0");
    response->setProperty("id", id);
    response->setProperty("result", var(result));
    return response;
}

static DynamicObject* newMcpErrorResponse(const var& id, int code, const String& message)
{
    auto response = newObject();
    response->setProperty("jsonrpc", "2.0");
    response->setProperty("id", id);
    response->setProperty("error", var(newMcpError(code, message)));
    return response;
}

static bool isMcpNotification(const var& id)
{
    return id.isVoid();
}

// The MCP stdio transport is newline-delimited JSON-RPC: one message per line,
// written as compact JSON so no newlines are embedded in a message.
static void writeMcpMessage(const var& message)
{
    const String body = JSON::toString(message, true);
    const size_t bodyBytes = body.getNumBytesAsUTF8();
    std::cout.write(body.toRawUTF8(), static_cast<std::streamsize>(bodyBytes));
    std::cout << "\n";
    std::cout.flush();
}

static bool readMcpMessage(String& body)
{
    body.clear();

    // read the next non-empty line; a failed read means the client closed stdin
    while (std::cin)
    {
        std::string rawLine;
        if (!std::getline(std::cin, rawLine))
        {
            return false;
        }
        body = String::fromUTF8(rawLine.c_str()).trim();
        if (body.isNotEmpty())
        {
            return true;
        }
    }
    return false;
}

// Validates that the tokens form complete commands before any of them is
// applied, mirroring the parser's argument-consumption rules. Rejecting up
// front keeps a tool call atomic: a trailing half-finished command would
// otherwise leave the parser waiting for arguments and silently swallow the
// first tokens of the next start_route call. Unknown tokens outside a command
// are rejected too (the command-line parser would try to load them as program
// files, which MCP must not do).
// The commands available over MCP form an allow-list: anything not explicitly
// permitted here is rejected with a reason, so a future command that writes to
// stdout (which MCP owns for JSON-RPC framing) or touches the file system is
// unavailable until it is deliberately reviewed and added. Returns an empty
// string for allowed commands.
static String mcpCommandDenialReason(const ApplicationCommand& cmd)
{
    switch (cmd.command_)
    {
        // routing and topology
        case INPUT:
        case OUTPUT:
        case VIRTUAL_IN:
        case VIRTUAL_OUT:
        case PANIC:
        // number and note conventions
        case DECIMAL:
        case HEXADECIMAL:
        case OCTAVE_MIDDLE_C:
        // filter modifier
        case NOT:
        // conversions and MPE operations
        case CONVERT:
        case NRPN_ADD:
        case NRPN_SCALE:
        case NRPN_CURVE:
        case RPN_ADD:
        case RPN_SCALE:
        case RPN_CURVE:
        case MPE_RELOCATE:
        case MPE_COLLAPSE:
        case MPE_EXPAND:
        case MPE_BEND:
        case MPE_SENS:
        case MPE_SPLIT:
            return {};

        case LIST:
            return "Use the list_midi_ports MCP tool instead of the list command.";
        case MONITOR:
        case MONITOR_SOURCE:
            return "Monitoring writes to stdout and cannot be used in MCP mode.";
        case NOTE_NUMBERS:
        case TIMESTAMP:
            return "Monitor formatting options are not available in MCP mode.";
        case TXTFILE:
            return "MCP mode does not load program files.";
        case SYSEX_FILE:
            return "MCP mode does not capture SysEx to files; use syf from the command line.";
        case JAVASCRIPT:
        case JAVASCRIPT_FILE:
            // the scripting engine can run shell commands (Util.command), open
            // network connections (OSC) and write to stdout, so it is a code
            // execution and exfiltration surface that must not be reachable by an
            // autonomously driven client; jsf additionally reads local files.
            // Scripting stays available from the command line.
            return "MCP mode does not run JavaScript (js/jsf); it is not available to remote clients.";

        default:
            break;
    }

    if (cmd.isFilter() || cmd.isTransform())
    {
        return {};
    }
    return "\"" + cmd.param_ + "\" is not available in MCP mode.";
}

static String validateMcpCommandTokens(const Array<ApplicationCommand>& commands,
                                       const StringArray& tokens)
{
    auto findCommand = [&commands](const String& token) -> const ApplicationCommand*
    {
        for (const auto& cmd : commands)
        {
            if (cmd.param_.equalsIgnoreCase(token) || cmd.altParam_.equalsIgnoreCase(token))
            {
                return &cmd;
            }
        }
        return nullptr;
    };

    const ApplicationCommand* current = nullptr;
    int remainingFixedArgs = 0;
    StringArray convertOpts;

    for (const auto& token : tokens)
    {
        if (current != nullptr && current->command_ == CONVERT)
        {
            convertOpts.add(token);
            if (conversion::specComplete(convertOpts))
            {
                current = nullptr;
            }
            continue;
        }
        if (remainingFixedArgs > 0)
        {
            // "-" as an in/out port name means text MIDI on stdout/stdin, which
            // MCP mode reserves for the JSON-RPC framing
            if (token == "-"
                && (current->command_ == INPUT || current->command_ == OUTPUT))
            {
                return "MCP mode does not support stdin/stdout '-' routes.";
            }
            remainingFixedArgs -= 1;
            if (remainingFixedArgs == 0)
            {
                current = nullptr;
            }
            continue;
        }

        const ApplicationCommand* cmd = findCommand(token);
        if (cmd != nullptr)
        {
            const String denial = mcpCommandDenialReason(*cmd);
            if (denial.isNotEmpty())
            {
                return denial;
            }
            current = cmd;
            convertOpts.clearQuick();
            remainingFixedArgs = jmax(0, cmd->expectedOptions_);
            if (remainingFixedArgs == 0 && cmd->command_ != CONVERT && cmd->expectedOptions_ >= 0)
            {
                current = nullptr;   // takes no arguments
            }
            continue;
        }

        if (current == nullptr)
        {
            return "Unknown command: " + token;
        }
        // a non-command token feeding a variable-argument command: consumed
    }

    if (current != nullptr && (remainingFixedArgs > 0 || current->command_ == CONVERT))
    {
        return "Incomplete command: " + current->param_ + " is missing arguments";
    }
    return {};
}


McpServer::McpServer(ApplicationState& state) : state_(state)
{
}

McpServer::~McpServer()
{
    if (thread_.joinable())
    {
        thread_.join();
    }
}

String McpServer::handleJsonForTest(const String& requestJson)
{
    const var response = handleRequest(JSON::parse(requestJson));
    return response.isVoid() ? String() : JSON::toString(response, true);
}

var McpServer::handleRequest(const var& message)
{
    auto* request = message.getDynamicObject();
    if (request == nullptr)
    {
        return var(newMcpErrorResponse(var(), -32700, "Parse error"));
    }

    const var id = request->getProperty("id");
    const String method = request->getProperty("method").toString();

    if (method == "notifications/initialized" || isMcpNotification(id))
    {
        return var();
    }

    if (method == "initialize")
    {
        auto capabilities = newObject();
        auto tools = newObject();
        tools->setProperty("listChanged", false);
        capabilities->setProperty("tools", var(tools));

        auto serverInfo = newObject();
        serverInfo->setProperty("name", ProjectInfo::projectName);
        serverInfo->setProperty("title", "RouteMIDI");
        serverInfo->setProperty("version", ProjectInfo::versionString);

        // answer with the client's protocol version when it is one we support,
        // otherwise with the latest version we do
        String protocolVersion = "2025-06-18";
        if (auto* params = request->getProperty("params").getDynamicObject())
        {
            const String requestedVersion = params->getProperty("protocolVersion").toString();
            const StringArray supported { "2024-11-05", "2025-03-26", "2025-06-18" };
            if (supported.contains(requestedVersion))
            {
                protocolVersion = requestedVersion;
            }
        }

        auto result = newObject();
        result->setProperty("protocolVersion", protocolVersion);
        result->setProperty("capabilities", var(capabilities));
        result->setProperty("serverInfo", var(serverInfo));
        result->setProperty("instructions",
                            "RouteMIDI is controlled through these MCP tools: "
                            "get_schema inspects commands, list_midi_ports "
                            "discovers ports and start_route starts live MIDI "
                            "routes from explicit command tokens.");
        return var(newMcpResponse(id, result));
    }

    if (method == "tools/list")
    {
        auto result = newObject();
        result->setProperty("tools", var(mcpTools()));
        return var(newMcpResponse(id, result));
    }

    if (method == "tools/call")
    {
        auto* params = request->getProperty("params").getDynamicObject();
        if (params == nullptr)
        {
            return var(newMcpErrorResponse(id, -32602, "Missing tools/call params"));
        }

        const String name = params->getProperty("name").toString();
        const var arguments = params->getProperty("arguments");

        if (name == "get_schema")
        {
            const String schema = state_.schemaJson();
            return var(newMcpResponse(id, newMcpToolResult(schema, JSON::parse(schema), false)));
        }
        if (name == "list_midi_ports")
        {
            auto ports = newObject();
            ports->setProperty("inputs", var(midiDeviceNames(MidiInput::getAvailableDevices())));
            ports->setProperty("outputs", var(midiDeviceNames(MidiOutput::getAvailableDevices())));
            const var structured(ports);
            return var(newMcpResponse(id, newMcpToolResult(JSON::toString(structured, true),
                                                          structured,
                                                          false)));
        }
        if (name == "start_route")
        {
            auto* args = arguments.getDynamicObject();
            auto* commandTokens = args == nullptr ? nullptr : args->getProperty("commands").getArray();
            if (commandTokens == nullptr)
            {
                return var(newMcpResponse(id, newMcpToolResult("Missing commands array",
                                                              var(),
                                                              true)));
            }

            StringArray commands;
            for (auto&& token : *commandTokens)
            {
                commands.add(token.toString());
            }

            // reject incomplete or unknown commands before anything is applied,
            // so a tool call is atomic and cannot leave the parser waiting for
            // arguments that would swallow the next call's tokens
            const String validationError = validateMcpCommandTokens(state_.commands_, commands);
            if (validationError.isNotEmpty())
            {
                return var(newMcpResponse(id, newMcpToolResult(validationError, var(), true)));
            }

            const int routesBefore = state_.routes_.size();
            {
                // build the routes into a staging list first, so the blocking
                // CoreMIDI device opens happen without the callback lock (holding
                // it across an open can drop live packets, see the reconnect
                // timer note in ApplicationState); the splice below only moves
                // pointers, so the lock is held for a moment
                // settings tokens ("hex", "omc", ...) mutate the session as they
                // are parsed; a rejected call must not leave them behind, so they
                // are snapshotted here and restored on either failure path (a
                // successful call keeps them, as on the command line)
                const bool hexBefore = state_.useHexadecimalsByDefault_;
                const bool nnBefore  = state_.noteNumbersOutput_;
                const bool tsBefore  = state_.timestampOutput_;
                const bool monBefore = state_.monitor_;
                const bool srcBefore = state_.monitorShowSource_;
                const int  omcBefore = state_.octaveMiddleC_;
                const int  returnValueBefore = JUCEApplicationBase::getInstance()->getApplicationReturnValue();
                auto restoreSettings = [&]
                {
                    state_.useHexadecimalsByDefault_ = hexBefore;
                    state_.noteNumbersOutput_ = nnBefore;
                    state_.timestampOutput_ = tsBefore;
                    state_.monitor_ = monBefore;
                    state_.monitorShowSource_ = srcBefore;
                    state_.octaveMiddleC_ = omcBefore;
                    // parse failures mark the process for a non-zero exit, but a
                    // rejected tool call is a recovered session error, not a
                    // process error: the server keeps serving and must exit clean
                    JUCEApplicationBase::getInstance()->setApplicationReturnValue(returnValueBefore);
                };

                OwnedArray<Route> staging;
                state_.parseParametersInto(staging, commands);

                // semantic failures (an invalid MPE zone, a missing script file,
                // commands before the first "in") reject the whole call and the
                // staged routes are discarded, so no partial route goes live
                if (!state_.parseErrors_.isEmpty())
                {
                    restoreSettings();
                    return var(newMcpResponse(id, newMcpToolResult(state_.parseErrors_.joinIntoString(" "),
                                                                  var(),
                                                                  true)));
                }
                if (staging.isEmpty())
                {
                    restoreSettings();
                    return var(newMcpResponse(id, newMcpToolResult("The commands did not create a route; start_route needs \"in ... out ...\" tokens",
                                                                  var(),
                                                                  true)));
                }

                const ScopedLock sl(state_.midiCallbackLock_);
                while (!staging.isEmpty())
                {
                    state_.routes_.add(staging.removeAndReturn(0));
                }
            }

            // report each new route in full (id, port connection state, commands),
            // so a client can tell a running route from one still waiting for its
            // ports to appear (the reconnect timer keeps retrying those)
            Array<var> routeStatus;
            for (int i = routesBefore; i < state_.routes_.size(); ++i)
            {
                routeStatus.add(var(routeToVar(*state_.routes_[i])));
            }

            auto result = newObject();
            result->setProperty("routesBefore", routesBefore);
            result->setProperty("routesAfter", state_.routes_.size());
            result->setProperty("routes", var(routeStatus));
            result->setProperty("running", true);
            const var structured(result);
            return var(newMcpResponse(id, newMcpToolResult(JSON::toString(structured, true),
                                                          structured,
                                                          false)));
        }

        // --- route lifecycle tools -------------------------------------------
        auto toolError = [&](const String& text)
        {
            return var(newMcpResponse(id, newMcpToolResult(text, var(), true)));
        };
        auto structuredOk = [&](DynamicObject* result)
        {
            const var structured(result);
            return var(newMcpResponse(id, newMcpToolResult(JSON::toString(structured, true),
                                                          structured,
                                                          false)));
        };
        auto* args = arguments.getDynamicObject();
        auto findRouteArg = [&](Route*& routeOut, int& indexOut) -> String
        {
            if (args == nullptr || !args->hasProperty("route"))
            {
                return "Missing route id";
            }
            const int routeId = (int) args->getProperty("route");
            for (int i = 0; i < state_.routes_.size(); ++i)
            {
                if (state_.routes_[i]->id == routeId)
                {
                    routeOut = state_.routes_[i];
                    indexOut = i;
                    return {};
                }
            }
            return "No route with id " + String(routeId);
        };
        auto allRoutesVar = [&]()
        {
            Array<var> routeArray;
            for (auto* route : state_.routes_)
            {
                routeArray.add(var(routeToVar(*route)));
            }
            return var(routeArray);
        };

        if (name == "list_routes")
        {
            auto result = newObject();
            result->setProperty("routes", allRoutesVar());
            return structuredOk(result);
        }
        if (name == "stop_route")
        {
            Route* route = nullptr;
            int index = -1;
            const String error = findRouteArg(route, index);
            if (error.isNotEmpty())
            {
                return toolError(error);
            }

            const int stoppedId = route->id;
            Route* removed = nullptr;
            {
                // unlink first: once the route is out of routes_, incoming MIDI
                // can no longer enqueue sends that point into its outputs, so
                // the drain below is guaranteed to flush the last of them
                const ScopedLock sl(state_.midiCallbackLock_);
                removed = state_.routes_.removeAndReturn(index);
            }
            state_.sendPanic(*removed);   // release anything still sounding
            state_.stopOutputSender();    // drains the panic and every queued send
                                          // that still references the route's outputs
            // deleted outside the lock: closing the inputs can wait for an
            // in-flight MIDI callback, which needs the lock to finish
            delete removed;
            state_.startOutputSender();

            auto result = newObject();
            result->setProperty("stopped", stoppedId);
            result->setProperty("routes", allRoutesVar());
            return structuredOk(result);
        }
        if (name == "panic_route")
        {
            Route* route = nullptr;
            int index = -1;
            const String error = findRouteArg(route, index);
            if (error.isNotEmpty())
            {
                return toolError(error);
            }
            {
                const ScopedLock sl(state_.midiCallbackLock_);
                state_.sendPanic(*route);
            }
            auto result = newObject();
            result->setProperty("panicked", route->id);
            return structuredOk(result);
        }
        if (name == "add_commands" || name == "replace_command")
        {
            Route* route = nullptr;
            int index = -1;
            String error = findRouteArg(route, index);
            if (error.isNotEmpty())
            {
                return toolError(error);
            }

            auto* commandTokens = args->getProperty("commands").getArray();
            if (commandTokens == nullptr)
            {
                return toolError("Missing commands array");
            }
            StringArray commands;
            for (auto&& token : *commandTokens)
            {
                commands.add(token.toString());
            }

            error = validateMcpCommandTokens(state_.commands_, commands);
            if (error.isNotEmpty())
            {
                return toolError(error);
            }
            Array<PendingCommand> pending;
            error = collectProcessingCommands(state_.commands_, commands, pending);
            if (error.isNotEmpty())
            {
                return toolError(error);
            }

            // apply the batch to a scratch route first, so validation problems
            // (a bad zone, an invalid convert spec) reject the call atomically
            // and the scratch containers hold the normalized commands
            Route scratch;
            for (const auto& entry : pending)
            {
                error = state_.addProcessingCommand(scratch, entry.command, entry.negate);
                if (error.isNotEmpty())
                {
                    return toolError(error);
                }
            }

            if (name == "add_commands")
            {
                const ScopedLock sl(state_.midiCallbackLock_);
                for (const auto& entry : pending)
                {
                    state_.addProcessingCommand(*route, entry.command, entry.negate);
                }
                return structuredOk(routeToVar(*route));
            }

            // replace_command: exactly one command, in the stage being replaced,
            // swapped in place so it keeps its position
            const String stage = args->getProperty("stage").toString();
            const int commandIndex = (int) args->getProperty("index");
            auto* container = stageContainer(*route, stage);
            if (container == nullptr)
            {
                return toolError("Unknown stage: " + stage);
            }
            if (commandIndex < 0 || commandIndex >= container->size())
            {
                return toolError("No command at index " + String(commandIndex) + " in " + stage);
            }
            auto* scratchContainer = stageContainer(scratch, stage);
            if (pending.size() != 1 || scratchContainer == nullptr || scratchContainer->size() != 1)
            {
                return toolError("replace_command needs exactly one replacement command in the " + stage + " stage");
            }
            {
                const ScopedLock sl(state_.midiCallbackLock_);
                const ApplicationCommand replaced = container->getReference(commandIndex);
                container->set(commandIndex, scratchContainer->getReference(0));
                cleanupAfterCommandChange(*route, replaced);
                cleanupAfterCommandChange(*route, container->getReference(commandIndex));
            }
            return structuredOk(routeToVar(*route));
        }
        if (name == "remove_command")
        {
            Route* route = nullptr;
            int index = -1;
            const String error = findRouteArg(route, index);
            if (error.isNotEmpty())
            {
                return toolError(error);
            }
            const String stage = args->getProperty("stage").toString();
            const int commandIndex = args->hasProperty("index") ? (int) args->getProperty("index") : -1;
            auto* container = stageContainer(*route, stage);
            if (container == nullptr)
            {
                return toolError("Unknown stage: " + stage);
            }
            if (commandIndex < 0 || commandIndex >= container->size())
            {
                return toolError("No command at index " + String(commandIndex) + " in " + stage);
            }
            {
                const ScopedLock sl(state_.midiCallbackLock_);
                const ApplicationCommand removed = container->getReference(commandIndex);
                container->remove(commandIndex);
                cleanupAfterCommandChange(*route, removed);
            }
            return structuredOk(routeToVar(*route));
        }

        return var(newMcpResponse(id, newMcpToolResult("Unknown tool: " + name,
                                                      var(),
                                                      true)));
    }

    return var(newMcpErrorResponse(id, -32601, "Method not found"));
}

void McpServer::start()
{
#if JUCE_WINDOWS
    // newline-delimited framing must not go through text-mode CRLF translation
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // stdin is read on a background thread so the message loop keeps running
    // (device reconnection and Timer callbacks depend on it); each request is
    // handled on the message thread, which serializes route changes with the
    // reconnect timer, and the response is written back from the reader thread
    thread_ = std::thread([this]
    {
        while (true)
        {
            String body;
            if (!readMcpMessage(body))
            {
                break;
            }

            const var request = JSON::parse(body);
            var response;
            WaitableEvent handled;
            MessageManager::callAsync([this, &request, &response, &handled]
            {
                response = handleRequest(request);
                handled.signal();
            });
            handled.wait();

            if (!response.isVoid())
            {
                writeMcpMessage(response);
            }
        }

        // the client closed stdin: shut the whole application down
        MessageManager::callAsync([] { JUCEApplicationBase::getInstance()->systemRequestedQuit(); });
    });
}
