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

        beginTest("The default server block is JSON naming this binary with --mcp");
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

        beginTest("A JSON client gets the same JSON block");
        {
            expectEquals(mcpconfig::serverBlock(exe, "cursor"), mcpconfig::serverBlock(exe));
            expectEquals(mcpconfig::serverBlock(exe, "claude-code"), mcpconfig::serverBlock(exe));
        }

        beginTest("Codex gets a TOML block in its own format");
        {
            const String block = mcpconfig::serverBlock(exe, "codex");
            expect(block.contains("[mcp_servers.routemidi]"));
            expect(block.contains("command = \"" + exe + "\""));
            expect(block.contains("args = [\"--mcp\"]"));
            // and it is TOML, not the JSON shape
            expect(! block.contains("mcpServers"));
        }

        beginTest("A Windows path is escaped for the TOML basic string");
        {
            const String block = mcpconfig::serverBlock("C:\\Program Files\\routemidi.exe", "codex");
            expect(block.contains("command = \"C:\\\\Program Files\\\\routemidi.exe\""));
        }

        beginTest("An unrecognized client yields no block");
        {
            expect(mcpconfig::serverBlock(exe, "nonesuch").isEmpty());
        }

        beginTest("Only JSON clients resolve to config paths");
        {
            expect(mcpconfig::configPathForClient("claude-desktop")
                       .getFileName() == "claude_desktop_config.json");
            expect(mcpconfig::configPathForClient("claude")
                       .getFileName() == "claude_desktop_config.json");
            expect(mcpconfig::configPathForClient("cursor").getFileName() == "mcp.json");
            // clients managed by their own CLI have no file for us to write
            expect(mcpconfig::configPathForClient("codex") == File());
            expect(mcpconfig::configPathForClient("claude-code") == File());
            expect(mcpconfig::configPathForClient("nonesuch") == File());
        }

        beginTest("Installing a CLI-managed client reports its command, writes nothing");
        {
            String summary;
            expect(mcpconfig::installToClient("codex", exe, summary).wasOk());
            expect(summary.contains("codex mcp add routemidi -- routemidi --mcp"));

            summary.clear();
            expect(mcpconfig::installToClient("claude-code", exe, summary).wasOk());
            expect(summary.contains("claude mcp add routemidi -- routemidi --mcp"));
        }

        beginTest("Installing an unknown client fails");
        {
            String summary;
            expect(mcpconfig::installToClient("nonesuch", exe, summary).failed());
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
