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
#include "../Source/McpServer.h"

namespace
{
    // feeds a command line into the parser; ports won't resolve to real devices,
    // so the resulting "couldn't find ... waiting" notices are suppressed here
    void parse(ApplicationState& state, const String& line)
    {
        StringArray params;
        params.addTokens(line, true);
        params.removeEmptyStrings(true);
        for (auto& p : params)
        {
            p = p.trimCharactersAtStart("\"").trimCharactersAtEnd("\"");
        }

        auto* previous = std::cerr.rdbuf(nullptr);
        state.parseParameters(params);
        std::cerr.rdbuf(previous);
    }

    // drives the MCP request handler directly (no stdio transport); a throwaway
    // McpServer is fine because all state lives in the ApplicationState it wraps
    var mcp(ApplicationState& state, const String& requestJson, bool quiet = false)
    {
        std::streambuf* previous = nullptr;
        if (quiet)
        {
            previous = std::cerr.rdbuf(nullptr);
        }

        McpServer server(state);
        const var response = server.handleRequest(JSON::parse(requestJson));

        if (quiet)
        {
            std::cerr.rdbuf(previous);
        }

        return response;
    }

    String mcpText(const var& response)
    {
        if (auto* result = response.getProperty("result", var()).getDynamicObject())
        {
            if (auto* content = result->getProperty("content").getArray())
            {
                if (!content->isEmpty())
                {
                    return content->getReference(0).getProperty("text", var()).toString();
                }
            }
        }
        return {};
    }
}

class ParsingTests : public UnitTest
{
public:
    ParsingTests() : UnitTest("Parsing", "Parsing") {}

    void runTest() override
    {
        beginTest("A route binds an input to its outputs");
        {
            ApplicationState state;
            parse(state, "in PortA out PortB");
            expectEquals(state.getRoutes().size(), 1);
            expectEquals(state.getRoutes()[0]->inputs.size(), 1);
            expect(state.getRoutes()[0]->inputs[0]->inName == "PortA");
            expectEquals(state.getRoutes()[0]->outputs.size(), 1);
            expect(state.getRoutes()[0]->outputs[0]->name == "PortB");
        }

        beginTest("One input splits to several outputs");
        {
            ApplicationState state;
            parse(state, "in A out X out Y out Z");
            expectEquals(state.getRoutes().size(), 1);
            expectEquals(state.getRoutes()[0]->inputs.size(), 1);
            expectEquals(state.getRoutes()[0]->outputs.size(), 3);
        }

        beginTest("Several inputs merge to shared outputs");
        {
            ApplicationState state;
            parse(state, "in A in B in C out X out Y");
            expectEquals(state.getRoutes().size(), 1);
            auto* route = state.getRoutes()[0];
            expectEquals(route->inputs.size(), 3);
            expect(route->inputs[0]->inName == "A");
            expect(route->inputs[2]->inName == "C");
            expectEquals(route->outputs.size(), 2);
        }

        beginTest("An input after an output starts a new route");
        {
            ApplicationState state;
            parse(state, "in A out X in B out Y out Z");
            expectEquals(state.getRoutes().size(), 2);
            expectEquals(state.getRoutes()[0]->inputs.size(), 1);
            expect(state.getRoutes()[0]->inputs[0]->inName == "A");
            expect(state.getRoutes()[1]->inputs[0]->inName == "B");
            expectEquals(state.getRoutes()[1]->outputs.size(), 2);
        }

        beginTest("Filters and transforms attach to the current route");
        {
            ApplicationState state;
            parse(state, "in A note transp 12 out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->filters.size(), 1);
            expect(route->filters[0].command_ == NOTE);
            expectEquals(route->transforms.size(), 1);
            expect(route->transforms[0].command_ == TRANSPOSE);
            expect(route->transforms[0].opts_[0] == "12");
        }

        beginTest("'not' marks the following filter as negated");
        {
            ApplicationState state;
            parse(state, "in A not clock out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->filters.size(), 1);
            expect(route->filters[0].command_ == CLOCK);
            expect(route->filters[0].negate_);
        }

        beginTest("'convert' captures its four arguments literally");
        {
            ApplicationState state;
            parse(state, "in A convert nrpn 245 cc14 1 out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->converters.size(), 1);
            const auto& opts = route->converters[0].opts_;
            expectEquals(opts.size(), 4);
            expect(opts[0] == "nrpn");
            expect(opts[1] == "245");
            expect(opts[2] == "cc14");
            expect(opts[3] == "1");
        }

        beginTest("'convert' with pb/cp/pc needs no number and normalizes to four options");
        {
            ApplicationState state;
            // pb takes no number; it normalizes to [pb, 0, cc, 7] and "out" still parses
            parse(state, "in A convert pb cc 7 out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->converters.size(), 1);
            const auto& opts = route->converters[0].opts_;
            expectEquals(opts.size(), 4);
            expect(opts[0] == "pb");
            expect(opts[1] == "0");
            expect(opts[2] == "cc");
            expect(opts[3] == "7");
            expectEquals(route->outputs.size(), 1);
            expect(route->outputs[0]->name == "B");
        }

        beginTest("'convert' to a no-number type stops collecting at the right token");
        {
            ApplicationState state;
            parse(state, "in A convert cc 7 cp out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->converters.size(), 1);
            const auto& opts = route->converters[0].opts_;
            expectEquals(opts.size(), 4);
            expect(opts[0] == "cc");
            expect(opts[1] == "7");
            expect(opts[2] == "cp");
            expect(opts[3] == "0");
            // "out B" is not swallowed by the converter
            expectEquals(route->outputs.size(), 1);
        }

        beginTest("Fixed arguments are taken literally even when they look like commands");
        {
            ApplicationState state;
            parse(state, "in A out cc");
            auto* route = state.getRoutes()[0];
            expectEquals(route->outputs.size(), 1);
            expect(route->outputs[0]->name == "cc");
            expectEquals(route->filters.size(), 0);
        }

        beginTest("Long command names land in the correct route buckets");
        {
            ApplicationState state;
            parse(state, "input A control-change pitch-bend transpose 12 channel-set 2 "
                         "nrpn-add 1000 50 mpe-mono lower 1 output B");
            expectEquals(state.getRoutes().size(), 1);
            auto* route = state.getRoutes()[0];

            expectEquals(route->inputs.size(), 1);
            expect(route->inputs[0]->inName == "A");
            expectEquals(route->outputs.size(), 1);
            expect(route->outputs[0]->name == "B");

            // filters, in order
            expectEquals(route->filters.size(), 2);
            expect(route->filters[0].command_ == CONTROL_CHANGE);
            expect(route->filters[1].command_ == PITCH_BEND);

            // transforms keep their order and arguments
            expectEquals(route->transforms.size(), 2);
            expect(route->transforms[0].command_ == TRANSPOSE);
            expect(route->transforms[0].opts_[0] == "12");
            expect(route->transforms[1].command_ == CHANNEL_SET);
            expect(route->transforms[1].opts_[0] == "2");

            // RPN/NRPN value transforms live in the converter bucket
            expectEquals(route->converters.size(), 1);
            expect(route->converters[0].command_ == NRPN_ADD);
            expect(route->converters[0].opts_[0] == "1000");
            expect(route->converters[0].opts_[1] == "50");

            // MPE operations land in their own bucket
            expectEquals(route->mpeOps.size(), 1);
            expect(route->mpeOps[0].command_ == MPE_COLLAPSE);
            expect(route->mpeOps[0].opts_[0] == "lower");
            expect(route->mpeOps[0].opts_[1] == "1");
        }

        beginTest("MPE zone filter and split parse into the right buckets (long names)");
        {
            ApplicationState state;
            parse(state, "input A mpe-member lower:7 mpe-split lower:15 5 output B output C");
            auto* route = state.getRoutes()[0];

            expectEquals(route->filters.size(), 1);
            expect(route->filters[0].command_ == MPE_MEMBER);
            expect(route->filters[0].opts_[0] == "lower:7");

            expectEquals(route->outputSplit.size(), 1);
            expect(route->outputSplit[0].command_ == MPE_SPLIT);
            expect(route->outputSplit[0].opts_[0] == "lower:15");
            expect(route->outputSplit[0].opts_[1] == "5");

            expectEquals(route->outputs.size(), 2);
        }

        beginTest("Variable-argument commands collect an optional value without swallowing the next command");
        {
            // cc with no number: the following "out" is not consumed as its argument
            {
                ApplicationState state;
                parse(state, "in A cc out B");
                auto* route = state.getRoutes()[0];
                expectEquals(route->filters.size(), 1);
                expect(route->filters[0].command_ == CONTROL_CHANGE);
                expect(route->filters[0].opts_.isEmpty());
                expectEquals(route->outputs.size(), 1);
                expect(route->outputs[0]->name == "B");
            }
            // cc with a number captures it
            {
                ApplicationState state;
                parse(state, "in A cc 7 out B");
                auto* route = state.getRoutes()[0];
                expectEquals(route->filters.size(), 1);
                expect(route->filters[0].opts_[0] == "7");
                expectEquals(route->outputs.size(), 1);
            }
            // cc14 (optional, omitted) followed by pc (optional, given)
            {
                ApplicationState state;
                parse(state, "in A cc14 pc 5 out B");
                auto* route = state.getRoutes()[0];
                expectEquals(route->filters.size(), 2);
                expect(route->filters[0].command_ == CONTROL_CHANGE_14BIT);
                expect(route->filters[0].opts_.isEmpty());
                expect(route->filters[1].command_ == PROGRAM_CHANGE);
                expect(route->filters[1].opts_[0] == "5");
            }
        }

        beginTest("Stdin and stdout ports are recognized from '-'");
        {
            ApplicationState state;
            parse(state, "in - out -");
            auto* route = state.getRoutes()[0];
            expectEquals(route->inputs.size(), 1);
            expect(route->inputs[0]->isStdin);
            expect(route->inputs[0]->inName == "stdin");
            expectEquals(route->outputs.size(), 1);
            expect(route->outputs[0]->isStdout);
        }

        beginTest("Range selectors are captured as option tokens");
        {
            ApplicationState state;
            parse(state, "in A cc 1..10 ch 1..4 out B");
            auto* route = state.getRoutes()[0];
            expectEquals(route->filters.size(), 2);
            expect(route->filters[0].command_ == CONTROL_CHANGE);
            expect(route->filters[0].opts_[0] == "1..10");
            expect(route->filters[1].command_ == CHANNEL);
            expect(route->filters[1].opts_[0] == "1..4");
        }

        beginTest("Commands before an input start no route");
        {
            ApplicationState state;
            parse(state, "transp 12 out B cc");
            expectEquals(state.getRoutes().size(), 0);
        }

        beginTest("Decimal and hexadecimal number parsing");
        {
            ApplicationState dec;
            expectEquals(dec.asDecOrHexIntValue("10"), 10);

            ApplicationState hex;
            parse(hex, "hex");
            expectEquals(hex.asDecOrHexIntValue("10"), 16);

            // explicit suffixes override the current base
            expectEquals(dec.asDecOrHexIntValue("10H"), 16);
            expectEquals(dec.asDecOrHexIntValue("7FH"), 127);
            expectEquals(hex.asDecOrHexIntValue("10M"), 10);
        }

        beginTest("Note name parsing");
        {
            ApplicationState state;
            expectEquals((int)state.asNoteNumber("C3"),  60);
            expectEquals((int)state.asNoteNumber("C#3"), 61);
            expectEquals((int)state.asNoteNumber("Db3"), 61);
            expectEquals((int)state.asNoteNumber("C-2"), 0);
            expectEquals((int)state.asNoteNumber("G8"),  127);
            expectEquals((int)state.asNoteNumber("64"),  64);   // plain numbers still work
        }

        beginTest("Octave for middle C shifts the note names");
        {
            ApplicationState state;
            parse(state, "omc 4");
            expectEquals((int)state.asNoteNumber("C4"), 60);
        }

        beginTest("Command schema is valid JSON and describes command arity");
        {
            ApplicationState state;
            const var schema = JSON::parse(state.schemaJson());
            auto* root = schema.getDynamicObject();
            expect(root != nullptr);
            if (root != nullptr)
            {
                expect(root->getProperty("tool").toString() == ProjectInfo::projectName);
                expectEquals((int)root->getProperty("defaultOctaveMiddleC"), 3);

                const auto& commands = *root->getProperty("commands").getArray();
                expect(commands.size() > 0);

                auto findCommand = [&commands](const String& name) -> DynamicObject*
                {
                    for (const auto& command : commands)
                    {
                        if (auto* object = command.getDynamicObject())
                        {
                            if (object->getProperty("name").toString() == name)
                            {
                                return object;
                            }
                        }
                    }
                    return nullptr;
                };

                auto* transp = findCommand("transp");
                expect(transp != nullptr);
                if (transp != nullptr)
                {
                    expect(transp->getProperty("alias").toString() == "transpose");
                    expect(transp->getProperty("stage").toString() == "transforms");
                    expect(transp->getProperty("arity").toString() == "fixed");
                    expectEquals((int)transp->getProperty("minArgs"), 1);
                    expectEquals((int)transp->getProperty("maxArgs"), 1);
                }

                auto* convert = findCommand("convert");
                expect(convert != nullptr);
                if (convert != nullptr)
                {
                    expect(convert->getProperty("stage").toString() == "conversions");
                    expectEquals((int)convert->getProperty("minArgs"), 4);
                    expectEquals((int)convert->getProperty("maxArgs"), 4);
                }

                // the stage names double as the stage argument of the MCP
                // route-editing tools, including the two commands whose stage
                // differs from their help grouping
                auto* nrpnadd = findCommand("nrpnadd");
                expect(nrpnadd != nullptr);
                if (nrpnadd != nullptr)
                {
                    expect(nrpnadd->getProperty("stage").toString() == "conversions");
                }
                auto* mpesplit = findCommand("mpesplit");
                expect(mpesplit != nullptr);
                if (mpesplit != nullptr)
                {
                    expect(mpesplit->getProperty("stage").toString() == "split");
                }

                auto* chord = findCommand("chord");
                expect(chord != nullptr);
                if (chord != nullptr)
                {
                    expect(chord->getProperty("arity").toString() == "variable");
                    expect(chord->getProperty("maxArgs").isVoid());
                }
            }
        }

        beginTest("MCP initialize advertises RouteMIDI tools");
        {
            ApplicationState state;
            const var response = mcp(state, R"json({
                "jsonrpc": "2.0",
                "id": 1,
                "method": "initialize",
                "params": {
                    "protocolVersion": "2025-06-18",
                    "capabilities": {},
                    "clientInfo": { "name": "test", "version": "1" }
                }
            })json");

            auto* result = response.getProperty("result", var()).getDynamicObject();
            expect(result != nullptr);
            if (result != nullptr)
            {
                expect(result->getProperty("protocolVersion").toString() == "2025-06-18");
                expect(result->getProperty("serverInfo").getProperty("name", var()).toString()
                       == ProjectInfo::projectName);
                expect(result->getProperty("capabilities").getProperty("tools", var()).isObject());
            }
        }

        beginTest("MCP tools/list exposes schema, ports and routing tools");
        {
            ApplicationState state;
            const var response = mcp(state, R"json({
                "jsonrpc": "2.0",
                "id": 2,
                "method": "tools/list"
            })json");

            auto* tools = response.getProperty("result", var()).getProperty("tools", var()).getArray();
            expect(tools != nullptr);
            if (tools != nullptr)
            {
                StringArray names;
                for (auto&& tool : *tools)
                {
                    names.add(tool.getProperty("name", var()).toString());
                }

                expect(names.contains("get_schema"));
                expect(names.contains("list_midi_ports"));
                expect(names.contains("start_route"));
            }
        }

        beginTest("MCP get_schema returns structured command metadata");
        {
            ApplicationState state;
            const var response = mcp(state, R"json({
                "jsonrpc": "2.0",
                "id": 3,
                "method": "tools/call",
                "params": {
                    "name": "get_schema",
                    "arguments": {}
                }
            })json");

            auto schema = response.getProperty("result", var()).getProperty("structuredContent", var());
            expect(schema.getProperty("tool", var()).toString() == ProjectInfo::projectName);
            auto* commands = schema.getProperty("commands", var()).getArray();
            expect(commands != nullptr);
            if (commands != nullptr)
            {
                expect(commands->size() > 0);
            }
            expect(JSON::parse(mcpText(response)).isObject());
        }

        beginTest("MCP list_midi_ports returns structured input and output arrays");
        {
            ApplicationState state;
            const var response = mcp(state, R"json({
                "jsonrpc": "2.0",
                "id": 4,
                "method": "tools/call",
                "params": {
                    "name": "list_midi_ports",
                    "arguments": {}
                }
            })json", true);

            auto ports = response.getProperty("result", var()).getProperty("structuredContent", var());
            expect(ports.getProperty("inputs", var()).isArray());
            expect(ports.getProperty("outputs", var()).isArray());
        }

        beginTest("MCP start_route rejects stdin/stdout routes");
        {
            ApplicationState state;
            const var response = mcp(state, R"json({
                "jsonrpc": "2.0",
                "id": 5,
                "method": "tools/call",
                "params": {
                    "name": "start_route",
                    "arguments": {
                        "commands": ["in", "-", "out", "Output"]
                    }
                }
            })json");

            expect(response.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(response).contains("stdin/stdout"));
            expectEquals(state.getRoutes().size(), 0);
        }

        beginTest("MCP start_route starts a live route from command tokens");
        {
            ApplicationState state;
            const var response = mcp(state, R"json({
                "jsonrpc": "2.0",
                "id": 6,
                "method": "tools/call",
                "params": {
                    "name": "start_route",
                    "arguments": {
                        "commands": [
                            "in", "NoSuchInput",
                            "transp", "12",
                            "out", "NoSuchOutput"
                        ]
                    }
                }
            })json", true);

            auto structured = response.getProperty("result", var()).getProperty("structuredContent", var());
            expectEquals((int)structured.getProperty("routesBefore", var()), 0);
            expectEquals((int)structured.getProperty("routesAfter", var()), 1);
            expect(structured.getProperty("running", var()));
            expectEquals(state.getRoutes().size(), 1);
            expectEquals(state.getRoutes()[0]->transforms.size(), 1);
            expect(state.getRoutes()[0]->transforms[0].command_ == TRANSPOSE);

            // the result reports how each port of the new route resolved, so a
            // client can tell a running route from one waiting for its ports
            auto* routeStatus = structured.getProperty("routes", var()).getArray();
            expect(routeStatus != nullptr && routeStatus->size() == 1);
            if (routeStatus != nullptr && routeStatus->size() == 1)
            {
                const var route = routeStatus->getReference(0);
                auto* inputs = route.getProperty("inputs", var()).getArray();
                expect(inputs != nullptr && inputs->size() == 1);
                if (inputs != nullptr && inputs->size() == 1)
                {
                    expect(inputs->getReference(0).getProperty("name", var()).toString() == "NoSuchInput");
                    expect(! inputs->getReference(0).getProperty("connected", var()));
                }
            }
        }

        beginTest("MCP start_route rejects incomplete commands atomically");
        {
            // a trailing half-finished command must not leave the parser waiting
            // for arguments; otherwise the first tokens of the next tool call would
            // be consumed as the missing arguments (an "in" with no name taking the
            // next call's "out" as its port)
            ApplicationState state;
            const var first = mcp(state, R"json({
                "jsonrpc": "2.0",
                "id": 7,
                "method": "tools/call",
                "params": {
                    "name": "start_route",
                    "arguments": { "commands": ["in"] }
                }
            })json", true);
            expect(first.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(first).contains("Incomplete command"));
            expectEquals(state.getRoutes().size(), 0);

            // the next call starts from a clean parser state
            const var second = mcp(state, R"json({
                "jsonrpc": "2.0",
                "id": 8,
                "method": "tools/call",
                "params": {
                    "name": "start_route",
                    "arguments": { "commands": ["in", "SomeInput", "out", "SomeOutput"] }
                }
            })json", true);
            auto structured = second.getProperty("result", var()).getProperty("structuredContent", var());
            expectEquals((int)structured.getProperty("routesAfter", var()), 1);
            expectEquals(state.getRoutes().size(), 1);
            expect(state.getRoutes()[0]->inputs[0]->inName == "SomeInput");
        }

        beginTest("MCP start_route rejects semantic parse failures atomically");
        {
            // token-shape validation cannot catch everything: an invalid MPE zone
            // has the right arity but fails when the command is added. Such
            // failures must reject the whole call instead of splicing a partial
            // route into the live set with "running": true
            ApplicationState state;
            const var badZone = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 30, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "mpemono", "middle", "1", "out", "B"] } }
            })json", true);
            expect(badZone.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(badZone).contains("Invalid MPE zone"));
            expectEquals(state.getRoutes().size(), 0);

            // a processing command before the first "in" has no route to attach
            // to; over MCP that is returned as an error rather than ignored
            const var early = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 31, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["transp", "12", "in", "A", "out", "B"] } }
            })json", true);
            expect(early.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(early).contains("no input route was started yet"));
            expectEquals(state.getRoutes().size(), 0);

            // configuration-only tokens create nothing and must not report success
            const var configOnly = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 32, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["hex"] } }
            })json", true);
            expect(configOnly.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(configOnly).contains("did not create a route"));
            expectEquals(state.getRoutes().size(), 0);

            // a valid call afterwards still succeeds
            const var good = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 33, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "mpemono", "lower", "1", "out", "B"] } }
            })json", true);
            expect(! good.getProperty("result", var()).getProperty("isError", var()));
            expectEquals(state.getRoutes().size(), 1);
            expectEquals(state.getRoutes()[0]->mpeOps.size(), 1);
        }

        beginTest("MCP start_route restores session settings when a call is rejected");
        {
            ApplicationState state;

            // a rejected config-only call must not leave hex mode behind
            mcp(state, R"json({
                "jsonrpc": "2.0", "id": 40, "method": "tools/call",
                "params": { "name": "start_route", "arguments": { "commands": ["hex"] } }
            })json", true);
            const var route = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 41, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "transp", "12", "out", "B"] } }
            })json", true);
            expect(! route.getProperty("result", var()).getProperty("isError", var()));
            {
                // "12" still parses as decimal: 60 + 12 = 72 (hex mode carried
                // over would make it 60 + 0x12 = 78)
                Array<MidiMessage> out = state.applyTransforms(*state.getRoutes()[0],
                                                               *state.getRoutes()[0]->inputs[0],
                                                               MidiMessage::noteOn(1, 60, (uint8) 100));
                expectEquals(out[0].getNoteNumber(), 72);
            }

            // settings inside a larger rejected call are rolled back too
            mcp(state, R"json({
                "jsonrpc": "2.0", "id": 42, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["omc", "5", "in", "C", "mpemono", "middle", "1", "out", "D"] } }
            })json", true);
            expectEquals((int) state.asNoteNumber("C3"), 60);   // omc 5 would make this 36

            // a successful call keeps its settings, as on the command line
            const var hexRoute = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 43, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["hex", "in", "E", "transp", "12", "out", "F"] } }
            })json", true);
            expect(! hexRoute.getProperty("result", var()).getProperty("isError", var()));
            {
                // "12" now parses as hexadecimal: 60 + 0x12 = 78
                Array<MidiMessage> out = state.applyTransforms(*state.getRoutes()[1],
                                                               *state.getRoutes()[1]->inputs[0],
                                                               MidiMessage::noteOn(1, 60, (uint8) 100));
                expectEquals(out[0].getNoteNumber(), 78);
            }
        }

        beginTest("MCP start_route clears a trailing 'not' between calls");
        {
            ApplicationState state;
            mcp(state, R"json({
                "jsonrpc": "2.0", "id": 44, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "out", "B", "not"] } }
            })json", true);
            mcp(state, R"json({
                "jsonrpc": "2.0", "id": 45, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "C", "clock", "out", "D"] } }
            })json", true);
            expectEquals(state.getRoutes().size(), 2);
            expectEquals(state.getRoutes()[1]->filters.size(), 1);
            expect(! state.getRoutes()[1]->filters[0].negate_);   // the stray "not" did not attach
        }

        beginTest("MCP start_route rejects stdout monitoring and file capture");
        {
            ApplicationState state;

            // "src" enables monitoring, which writes routed messages to the
            // stdout that the JSON-RPC framing owns, so it is rejected like "mon"
            const var src = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 50, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "src", "out", "B"] } }
            })json", true);
            expect(src.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(src).contains("stdout"));
            expectEquals(state.getRoutes().size(), 0);

            const var srcLong = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 51, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "monitor-source", "out", "B"] } }
            })json", true);
            expect(srcLong.getProperty("result", var()).getProperty("isError", var()));

            // "syf" deletes and recreates its target file while parsing, which a
            // rejected call could never undo, so it is rejected up front and a
            // pre-existing capture file survives the attempt untouched
            File sentinel = File::getSpecialLocation(File::tempDirectory)
                                .getChildFile("routemidi-mcp-syf-test.syx");
            sentinel.replaceWithText("precious");

            const var syf = mcp(state, String(R"json({
                "jsonrpc": "2.0", "id": 52, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "syx", "out", "B", "syf", ")json")
                + sentinel.getFullPathName().replace("\\", "\\\\") + R"json("] } } })json", true);
            expect(syf.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(syf).contains("SysEx"));
            expectEquals(state.getRoutes().size(), 0);
            expect(sentinel.existsAsFile());
            expect(sentinel.loadFileAsString() == "precious");
            sentinel.deleteFile();
        }

        beginTest("MCP command availability is an allow-list at the command level");
        {
            ApplicationState state;

            // commands outside the allow-list are rejected with a reason,
            // including the monitor-formatting settings nn and ts
            const var nn = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 60, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "nn", "out", "B"] } }
            })json", true);
            expect(nn.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(nn).contains("not available in MCP mode"));

            const var list = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 61, "method": "tools/call",
                "params": { "name": "start_route", "arguments": { "commands": ["list"] } }
            })json", true);
            expect(list.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(list).contains("list_midi_ports"));

            // the check works at the command level, not the token level: a port
            // that happens to be NAMED like a forbidden command is a fixed
            // argument and is accepted
            const var portNamedMon = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 62, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "mon", "out", "syf"] } }
            })json", true);
            expect(! portNamedMon.getProperty("result", var()).getProperty("isError", var()));
            expectEquals(state.getRoutes().size(), 1);
            expect(state.getRoutes()[0]->inputs[0]->inName == "mon");
            expect(state.getRoutes()[0]->outputs[0]->name == "syf");
        }

        beginTest("MCP rejects jsf and keeps file contents out of tool responses");
        {
            // jsf reads a local file into the command's options at parse time,
            // and route reporting echoes options verbatim, so MCP rejects it; the
            // response must not carry the file's contents
            ApplicationState state;
            File secretFile = File::getSpecialLocation(File::tempDirectory)
                                  .getChildFile("routemidi-mcp-jsf-test.js");
            const String secret = "SECRET-FILE-CONTENT-1f2e3d";
            secretFile.replaceWithText(secret);
            const String path = secretFile.getFullPathName().replace("\\", "\\\\");

            const var started = mcp(state, String(R"json({
                "jsonrpc": "2.0", "id": 70, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "jsf", ")json")
                + path + R"json(", "out", "B"] } } })json", true);
            expect(started.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(started).contains("JavaScript"));
            expect(! JSON::toString(started, true).contains(secret));
            expectEquals(state.getRoutes().size(), 0);

            // the same protection holds when editing a running route
            mcp(state, R"json({
                "jsonrpc": "2.0", "id": 71, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "out", "B"] } }
            })json", true);
            const var added = mcp(state, String(R"json({
                "jsonrpc": "2.0", "id": 72, "method": "tools/call",
                "params": { "name": "add_commands", "arguments": {
                    "route": 1, "commands": ["jsf", ")json")
                + path + R"json("] } } })json", true);
            expect(added.getProperty("result", var()).getProperty("isError", var()));
            expect(! JSON::toString(added, true).contains(secret));
            expectEquals(state.getRoutes()[0]->transforms.size(), 0);

            secretFile.deleteFile();
        }

        beginTest("MCP rejects inline js");
        {
            // the scripting engine can run shell commands (Util.command), open
            // network connections (OSC) and write to stdout, so MCP rejects js
            // like jsf; scripting stays on the command line
            ApplicationState state;
            const var started = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 74, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "js", "Util.command('id');", "out", "B"] } }
            })json", true);
            expect(started.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(started).contains("JavaScript"));
            expectEquals(state.getRoutes().size(), 0);

            // and on a running route
            mcp(state, R"json({
                "jsonrpc": "2.0", "id": 75, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "out", "B"] } }
            })json", true);
            const var added = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 76, "method": "tools/call",
                "params": { "name": "add_commands", "arguments": {
                    "route": 1, "commands": ["js", "MIDI.setVelocity(100);"] } }
            })json", true);
            expect(added.getProperty("result", var()).getProperty("isError", var()));
            expectEquals(state.getRoutes()[0]->transforms.size(), 0);
        }

        beginTest("MCP rejected calls leave the process exit code unchanged");
        {
            // a rejected tool call is a recovered session error: the server keeps
            // serving, so it must not mark the process for a non-zero exit
            JUCEApplicationBase::getInstance()->setApplicationReturnValue(0);

            ApplicationState state;
            mcp(state, R"json({
                "jsonrpc": "2.0", "id": 80, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "A", "mpemono", "middle", "1", "out", "B"] } }
            })json", true);
            expectEquals(JUCEApplicationBase::getInstance()->getApplicationReturnValue(), 0);
        }

        beginTest("MCP start_route rejects unknown tokens instead of loading files");
        {
            // an unrecognized bare token would be tried as a command-file path by
            // the command-line parser; over MCP that must be rejected outright
            ApplicationState state;
            const var response = mcp(state, R"json({
                "jsonrpc": "2.0",
                "id": 9,
                "method": "tools/call",
                "params": {
                    "name": "start_route",
                    "arguments": { "commands": ["/etc/hosts"] }
                }
            })json", true);
            expect(response.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(response).contains("Unknown command"));
            expectEquals(state.getRoutes().size(), 0);
        }

        beginTest("MCP route lifecycle: list, edit and stop a running route");
        {
            ApplicationState state;
            mcp(state, R"json({
                "jsonrpc": "2.0", "id": 10, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "LifecycleIn", "transp", "12", "out", "LifecycleOut"] } }
            })json", true);

            // list_routes reports the stable id and the staged commands
            const var listed = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 11, "method": "tools/call",
                "params": { "name": "list_routes", "arguments": {} }
            })json");
            auto* routes = listed.getProperty("result", var()).getProperty("structuredContent", var())
                                 .getProperty("routes", var()).getArray();
            expect(routes != nullptr && routes->size() == 1);
            int routeId = -1;
            if (routes != nullptr && routes->size() == 1)
            {
                const var route = routes->getReference(0);
                routeId = (int) route.getProperty("id", var());
                auto* transforms = route.getProperty("transforms", var()).getArray();
                expect(transforms != nullptr && transforms->size() == 1);
                if (transforms != nullptr && transforms->size() == 1)
                {
                    expect(transforms->getReference(0).getProperty("command", var()).toString() == "transp");
                }
            }
            expect(routeId >= 1);

            // add a scale quantizer behind the transpose and verify the behavior
            mcp(state, String(R"json({
                "jsonrpc": "2.0", "id": 12, "method": "tools/call",
                "params": { "name": "add_commands", "arguments": {
                    "route": )json") + String(routeId) + R"json(,
                    "commands": ["scale", "C", "major"] } } })json", true);
            expectEquals(state.getRoutes()[0]->transforms.size(), 2);
            {
                // 61 transposes to 73 (C#4), which C major snaps down to 72
                Array<MidiMessage> out = state.applyTransforms(*state.getRoutes()[0],
                                                               *state.getRoutes()[0]->inputs[0],
                                                               MidiMessage::noteOn(1, 61, (uint8) 100));
                expectEquals(out.size(), 1);
                expectEquals(out[0].getNoteNumber(), 72);
            }

            // replace the scale in place, keeping its position after the transpose
            const var replaced = mcp(state, String(R"json({
                "jsonrpc": "2.0", "id": 13, "method": "tools/call",
                "params": { "name": "replace_command", "arguments": {
                    "route": )json") + String(routeId) + R"json(,
                    "stage": "transforms", "index": 1,
                    "commands": ["scale", "D", "minor"] } } })json", true);
            expect(! replaced.getProperty("result", var()).getProperty("isError", var()));
            expectEquals(state.getRoutes()[0]->transforms.size(), 2);
            expect(state.getRoutes()[0]->transforms[1].opts_[0] == "D");

            // remove the transpose; only the D minor quantizer remains
            mcp(state, String(R"json({
                "jsonrpc": "2.0", "id": 14, "method": "tools/call",
                "params": { "name": "remove_command", "arguments": {
                    "route": )json") + String(routeId) + R"json(,
                    "stage": "transforms", "index": 0 } } })json", true);
            expectEquals(state.getRoutes()[0]->transforms.size(), 1);
            {
                // 61 (C#) is not in D minor and snaps down to 60 (C)
                Array<MidiMessage> out = state.applyTransforms(*state.getRoutes()[0],
                                                               *state.getRoutes()[0]->inputs[0],
                                                               MidiMessage::noteOn(1, 61, (uint8) 100));
                expectEquals(out.size(), 1);
                expectEquals(out[0].getNoteNumber(), 60);
            }

            // panic then stop; the route disappears
            const var panicked = mcp(state, String(R"json({
                "jsonrpc": "2.0", "id": 15, "method": "tools/call",
                "params": { "name": "panic_route", "arguments": { "route": )json")
                + String(routeId) + "} } }", true);
            expect(! panicked.getProperty("result", var()).getProperty("isError", var()));

            const var stopped = mcp(state, String(R"json({
                "jsonrpc": "2.0", "id": 16, "method": "tools/call",
                "params": { "name": "stop_route", "arguments": { "route": )json")
                + String(routeId) + "} } }", true);
            expect(! stopped.getProperty("result", var()).getProperty("isError", var()));
            expectEquals(state.getRoutes().size(), 0);
        }

        beginTest("MCP route editing rejects invalid edits");
        {
            ApplicationState state;
            mcp(state, R"json({
                "jsonrpc": "2.0", "id": 20, "method": "tools/call",
                "params": { "name": "start_route", "arguments": {
                    "commands": ["in", "EditIn", "transp", "12", "out", "EditOut"] } }
            })json", true);

            // topology commands cannot be added to a running route
            const var addPort = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 21, "method": "tools/call",
                "params": { "name": "add_commands", "arguments": {
                    "route": 1, "commands": ["out", "Elsewhere"] } }
            })json", true);
            expect(addPort.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(addPort).contains("cannot be added"));

            // a replacement must belong to the stage being replaced
            const var wrongStage = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 22, "method": "tools/call",
                "params": { "name": "replace_command", "arguments": {
                    "route": 1, "stage": "transforms", "index": 0,
                    "commands": ["ch", "1"] } }
            })json", true);
            expect(wrongStage.getProperty("result", var()).getProperty("isError", var()));

            // unknown route ids are reported
            const var missing = mcp(state, R"json({
                "jsonrpc": "2.0", "id": 23, "method": "tools/call",
                "params": { "name": "stop_route", "arguments": { "route": 99 } }
            })json", true);
            expect(missing.getProperty("result", var()).getProperty("isError", var()));
            expect(mcpText(missing).contains("No route with id"));

            // the failed edits left the route untouched
            expectEquals(state.getRoutes().size(), 1);
            expectEquals(state.getRoutes()[0]->transforms.size(), 1);
        }

        beginTest("Text MIDI codec round-trips through messageToText/parseTextMidi");
        {
            ApplicationState state;
            // notes are rendered as numbers so the round-trip is base-independent
            parse(state, "nn");

            auto roundTrip = [&state] (const MidiMessage& msg)
            {
                const String text = state.messageToText(msg);
                StringArray tokens;
                tokens.addTokens(text, " ", "");
                tokens.removeEmptyStrings(true);
                Array<MidiMessage> parsed;
                state.parseTextMidi(tokens, parsed);
                return parsed;
            };

            const MidiMessage cases[] = {
                MidiMessage::noteOn(1, 60, (uint8)100),
                MidiMessage::noteOff(5, 72, (uint8)0),
                MidiMessage::controllerEvent(3, 74, 42),
                MidiMessage::programChange(2, 10),
                MidiMessage::channelPressureChange(7, 64),
                MidiMessage::aftertouchChange(4, 60, 33),
                MidiMessage::pitchWheel(9, 12000),
                MidiMessage::midiClock(),
                MidiMessage::midiStart(),
                MidiMessage::midiStop(),
                MidiMessage::midiContinue(),
                MidiMessage::songPositionPointer(2000),
            };

            for (const auto& msg : cases)
            {
                auto parsed = roundTrip(msg);
                expectEquals(parsed.size(), 1);
                if (parsed.size() == 1)
                {
                    // compare the raw bytes for an exact match
                    expect(parsed[0].getRawDataSize() == msg.getRawDataSize());
                    expect(memcmp(parsed[0].getRawData(), msg.getRawData(),
                                  (size_t)msg.getRawDataSize()) == 0);
                }
            }
        }

        beginTest("Text MIDI codec round-trips System Exclusive");
        {
            ApplicationState state;
            const uint8 sysexData[] = { 0x43, 0x12, 0x00, 0x7f };
            const MidiMessage syx = MidiMessage::createSysExMessage(sysexData, numElementsInArray(sysexData));

            const String text = state.messageToText(syx);
            StringArray tokens;
            tokens.addTokens(text, " ", "");
            tokens.removeEmptyStrings(true);
            Array<MidiMessage> parsed;
            state.parseTextMidi(tokens, parsed);

            expectEquals(parsed.size(), 1);
            if (parsed.size() == 1)
            {
                expect(parsed[0].isSysEx());
                expect(parsed[0].getRawDataSize() == syx.getRawDataSize());
                expect(memcmp(parsed[0].getRawData(), syx.getRawData(),
                              (size_t)syx.getRawDataSize()) == 0);
            }
        }

        beginTest("A long realistic routing spans config, filters, transforms, conversion and monitoring");
        {
            // A keyboard into a synth: pass only channel 1 (dropping clock and
            // aftertouch), transpose up an octave, quantise to C major, shape the
            // velocity, move everything to channel 5, halve the pitch bend, trim
            // and remap the mod wheel -- with monitoring switched on for good measure.
            ApplicationState state;
            parse(state, "dec omc 3 in Keyboard ch 1 not clock not cp "
                         "transp 12 scale C major velclip 40 120 veladd 10 chset 5 pbscale 0.5 ccadd 1 -5 "
                         "convert cc 1 cc 11 mon nn ts src out Synth");

            // the parser sorted each command into its own stage bucket; the config
            // and monitoring words (dec, omc, mon, nn, ts, src) stay global
            expectEquals(state.getRoutes().size(), 1);
            Route& route = *state.getRoutes().getFirst();
            expectEquals(route.filters.size(), 3);       // ch, not clock, not cp
            expectEquals(route.transforms.size(), 7);    // transp scale velclip veladd chset pbscale ccadd
            expectEquals(route.converters.size(), 1);    // convert cc 1 cc 11
            expect(route.mpeOps.isEmpty());
            RouteInput& input = *route.inputs.getFirst();

            // runs a message through the whole transformation pipeline the way
            // routeMessage does: filters -> transforms -> MPE -> converters
            auto run = [&state, &route, &input](const MidiMessage& msg)
            {
                Array<MidiMessage> out;
                if (! state.passesFilters(route, msg))
                {
                    return out;
                }
                for (auto& t : state.applyTransforms(route, input, msg))
                {
                    Array<MidiMessage> afterMpe;
                    if (route.mpeOps.isEmpty()) afterMpe.add(t);
                    else                        state.processMpe(route, input, t, afterMpe);
                    for (auto& m : afterMpe)
                    {
                        if (route.converters.isEmpty()) out.add(m);
                        else                            state.processConverters(route, input, m, out);
                    }
                }
                return out;
            };

            // a played C#4 climbs an octave, snaps to C in the scale, keeps its
            // (in-range, boosted) velocity, and lands on channel 5
            auto a = run(MidiMessage::noteOn(1, 61, (uint8)100));
            expectEquals(a.size(), 1);
            expect(a[0].isNoteOn());
            expectEquals(a[0].getChannel(), 5);
            expectEquals(a[0].getNoteNumber(), 72);           // 61 +12 -> 73, snapped down to 72
            expectEquals((int)a[0].getVelocity(), 110);       // 100 (within 40-120) +10

            // a soft note has its velocity clamped up to the floor before the boost
            auto b = run(MidiMessage::noteOn(1, 60, (uint8)20));
            expectEquals(b.size(), 1);
            expectEquals(b[0].getNoteNumber(), 72);           // 60 +12 -> 72, already in scale
            expectEquals((int)b[0].getVelocity(), 50);        // 20 -> clamped 40 -> +10

            // the mod wheel is trimmed by ccadd and converted to CC 11 on channel 5
            auto c = run(MidiMessage::controllerEvent(1, 1, 90));
            expectEquals(c.size(), 1);
            expect(c[0].isController());
            expectEquals(c[0].getChannel(), 5);
            expectEquals(c[0].getControllerNumber(), 11);     // convert cc 1 -> cc 11
            expectEquals(c[0].getControllerValue(), 85);      // 90 - 5 (ccadd), 7-bit unchanged by convert

            // pitch bend is halved around centre and moved to channel 5
            auto d = run(MidiMessage::pitchWheel(1, 12288));
            expectEquals(d.size(), 1);
            expect(d[0].isPitchWheel());
            expectEquals(d[0].getChannel(), 5);
            expectEquals(d[0].getPitchWheelValue(), 10240);   // 8192 + (12288-8192)/2

            // clock and channel pressure are blacklisted, and a wrong channel is out
            expect(run(MidiMessage::midiClock()).isEmpty());
            expect(run(MidiMessage::channelPressureChange(1, 50)).isEmpty());
            expect(run(MidiMessage::noteOn(2, 60, (uint8)100)).isEmpty());
        }
    }
};

static ParsingTests parsingTests;
