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

#include "../Source/McpConfig.h"

// Verifies the MCP client-configuration helpers behind --print-mcp-config and
// --install-mcp: the emitted block, the client path resolution, and above all
// the in-place merge, which must preserve a user's existing servers and never
// clobber a config file it can't parse.
class McpConfigTests : public UnitTest
{
public:
    McpConfigTests() : UnitTest("McpConfig", "McpConfig") {}

    // a scratch config file that removes itself when the test scope ends
    struct TempConfig
    {
        TempConfig() : file(File::createTempFile(".json")) { file.deleteFile(); }
        ~TempConfig() { file.deleteFile(); }
        File file;
    };

    var parse(const File& f)
    {
        var result;
        JSON::parse(f.loadFileAsString(), result);
        return result;
    }

    void runTest() override
    {
        const String exe = "/opt/homebrew/bin/routemidi";

        beginTest("The server block names this binary running with --mcp");
        {
            var block;
            const Result parsed = JSON::parse(mcpconfig::serverBlock(exe), block);
            expect(parsed.wasOk());
            var entry = block["mcpServers"]["routemidi"];
            expect(entry.isObject());
            expectEquals(entry["command"].toString(), exe);
            expect(entry["args"].isArray());
            expectEquals(entry["args"][0].toString(), String("--mcp"));
        }

        beginTest("Client names resolve to config paths, unknown ones don't");
        {
            expect(mcpconfig::configPathForClient("claude-desktop")
                       .getFileName() == "claude_desktop_config.json");
            expect(mcpconfig::configPathForClient("claude")
                       .getFileName() == "claude_desktop_config.json");
            expect(mcpconfig::configPathForClient("cursor").getFileName() == "mcp.json");
            expect(mcpconfig::configPathForClient("nonesuch") == File());
        }

        beginTest("Merging into a missing file creates the server entry");
        {
            TempConfig cfg;
            String summary;
            const Result r = mcpconfig::mergeConfigFile(cfg.file, exe, summary);
            expect(r.wasOk());
            expect(cfg.file.existsAsFile());
            expect(summary.contains("Added"));
            expectEquals(parse(cfg.file)["mcpServers"]["routemidi"]["command"].toString(), exe);
        }

        beginTest("Merging preserves servers and settings already in the file");
        {
            TempConfig cfg;
            cfg.file.replaceWithText(R"({
              "mcpServers": { "other": { "command": "/usr/bin/other", "args": [] } },
              "theme": "dark"
            })");
            String summary;
            const Result r = mcpconfig::mergeConfigFile(cfg.file, exe, summary);
            expect(r.wasOk());
            expect(summary.contains("Added"));

            const var root = parse(cfg.file);
            // the pre-existing server and unrelated setting survive
            expectEquals(root["mcpServers"]["other"]["command"].toString(), String("/usr/bin/other"));
            expectEquals(root["theme"].toString(), String("dark"));
            // and routemidi was added alongside
            expectEquals(root["mcpServers"]["routemidi"]["command"].toString(), exe);
        }

        beginTest("Merging twice updates in place instead of duplicating");
        {
            TempConfig cfg;
            String first, second;
            expect(mcpconfig::mergeConfigFile(cfg.file, "/old/routemidi", first).wasOk());
            expect(first.contains("Added"));
            expect(mcpconfig::mergeConfigFile(cfg.file, exe, second).wasOk());
            expect(second.contains("Updated"));

            const var servers = parse(cfg.file)["mcpServers"];
            expect(servers.getDynamicObject() != nullptr);
            expectEquals(servers.getDynamicObject()->getProperties().size(), 1);
            expectEquals(servers["routemidi"]["command"].toString(), exe);
        }

        beginTest("A config file that isn't valid JSON is left untouched");
        {
            TempConfig cfg;
            const String garbage = "this is not json {{{";
            cfg.file.replaceWithText(garbage);
            String summary;
            const Result r = mcpconfig::mergeConfigFile(cfg.file, exe, summary);
            expect(r.failed());
            expectEquals(cfg.file.loadFileAsString(), garbage);
        }
    }
};

static McpConfigTests mcpConfigTests;
