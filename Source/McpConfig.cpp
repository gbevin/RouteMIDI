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

#include "McpConfig.h"

namespace mcpconfig
{

// how a client's server entry is written
enum class Format { json, toml };

// how --install-mcp reaches a client: a JSON configuration file RouteMIDI can
// safely parse and rewrite, or a command the client provides to register servers
// itself (which RouteMIDI reports rather than editing an unfamiliar file)
enum class Kind { jsonFile, cli };

struct ClientInfo
{
    String  name;         // canonical name
    String  displayName;  // for user-facing messages
    Format  format;       // the format --print-mcp-config emits for it
    Kind    kind;         // how --install-mcp handles it
    String  cliCommand;   // the command to run, for a cli client
};

// the known clients; the first match on name or alias wins
static bool lookupClient(const String& client, ClientInfo& out)
{
    const String n = client.trim().toLowerCase();

    if (n == "claude-desktop" || n == "claude")
    {
        out = { "claude-desktop", "Claude Desktop", Format::json, Kind::jsonFile, {} };
        return true;
    }
    if (n == "cursor")
    {
        out = { "cursor", "Cursor", Format::json, Kind::jsonFile, {} };
        return true;
    }
    if (n == "codex")
    {
        // Codex's configuration is TOML, a format RouteMIDI can print but not
        // safely round-trip, and it ships its own command, so --install-mcp
        // reports that instead of editing ~/.codex/config.toml
        out = { "codex", "Codex", Format::toml, Kind::cli,
                "codex mcp add routemidi -- routemidi --mcp" };
        return true;
    }
    if (n == "claude-code")
    {
        out = { "claude-code", "Claude Code", Format::json, Kind::cli,
                "claude mcp add routemidi -- routemidi --mcp" };
        return true;
    }
    return false;
}

// the { command, args:["--mcp"] } that launches this binary as an MCP server
static var serverEntry(const String& exePath)
{
    auto* entry = new DynamicObject();
    entry->setProperty("command", exePath);

    Array<var> args;
    args.add("--mcp");
    entry->setProperty("args", args);

    return var(entry);
}

// the JSON "mcpServers" block Claude Desktop, Cursor, Claude Code and most other
// clients use
static String jsonServerBlock(const String& exePath)
{
    auto* servers = new DynamicObject();
    servers->setProperty("routemidi", serverEntry(exePath));

    auto* root = new DynamicObject();
    root->setProperty("mcpServers", var(servers));

    return JSON::toString(var(root));
}

// escapes a value for a TOML basic (double-quoted) string: backslash first so
// the escapes it introduces aren't escaped again, then the quote
static String tomlEscape(const String& s)
{
    return s.replace("\\", "\\\\").replace("\"", "\\\"");
}

// the TOML table Codex uses under ~/.codex/config.toml
static String tomlServerBlock(const String& exePath)
{
    return String("[mcp_servers.routemidi]") + newLine
         + "command = \"" + tomlEscape(exePath) + "\"" + newLine
         + "args = [\"--mcp\"]";
}

String serverBlock(const String& exePath, const String& client)
{
    if (client.isNotEmpty())
    {
        ClientInfo info;
        if (! lookupClient(client, info))
        {
            return {};
        }
        if (info.format == Format::toml)
        {
            return tomlServerBlock(exePath);
        }
    }
    return jsonServerBlock(exePath);
}

StringArray supportedClients()
{
    return { "claude-desktop", "cursor", "codex", "claude-code" };
}

File configPathForClient(const String& client)
{
    ClientInfo info;
    if (! lookupClient(client, info) || info.kind != Kind::jsonFile)
    {
        return {};
    }

    // Claude Desktop keeps its MCP servers in claude_desktop_config.json under
    // the per-user application data directory, which JUCE maps to the right
    // place on each platform (~/Library/Application Support, %APPDATA%,
    // ~/.config).
    if (info.name == "claude-desktop")
    {
        return File::getSpecialLocation(File::userApplicationDataDirectory)
                   .getChildFile("Claude")
                   .getChildFile("claude_desktop_config.json");
    }

    // Cursor reads global MCP servers from ~/.cursor/mcp.json, in the same
    // "mcpServers" shape.
    if (info.name == "cursor")
    {
        return File::getSpecialLocation(File::userHomeDirectory)
                   .getChildFile(".cursor")
                   .getChildFile("mcp.json");
    }

    return {};
}

Result mergeConfigFile(const File& configFile, const String& exePath, String& summary)
{
    // start from the existing config so other servers and unrelated settings
    // survive; refuse to proceed if it is present but unparseable rather than
    // overwrite something we don't understand
    var root;
    if (configFile.existsAsFile())
    {
        const String existing = configFile.loadFileAsString();
        if (existing.trim().isNotEmpty())
        {
            const Result parsed = JSON::parse(existing, root);
            if (parsed.failed() || ! root.getDynamicObject())
            {
                return Result::fail("the existing configuration at "
                                    + configFile.getFullPathName()
                                    + " isn't valid JSON, leaving it untouched");
            }
        }
    }

    if (root.getDynamicObject() == nullptr)
    {
        root = var(new DynamicObject());
    }
    auto* rootObj = root.getDynamicObject();

    var servers = rootObj->getProperty("mcpServers");
    if (servers.getDynamicObject() == nullptr)
    {
        servers = var(new DynamicObject());
        rootObj->setProperty("mcpServers", servers);
    }
    auto* serversObj = servers.getDynamicObject();

    const bool wasUpdate = serversObj->hasProperty("routemidi");
    serversObj->setProperty("routemidi", serverEntry(exePath));

    if (! configFile.getParentDirectory().createDirectory())
    {
        return Result::fail("couldn't create the folder for "
                            + configFile.getFullPathName());
    }
    if (! configFile.replaceWithText(JSON::toString(root) + newLine))
    {
        return Result::fail("couldn't write " + configFile.getFullPathName());
    }

    summary = (wasUpdate ? "Updated" : "Added")
              + String(" the routemidi MCP server in ")
              + configFile.getFullPathName()
              + newLine + "Restart the client to pick up RouteMIDI.";
    return Result::ok();
}

Result installToClient(const String& client, const String& exePath, String& summary)
{
    ClientInfo info;
    if (! lookupClient(client, info))
    {
        return Result::fail("unknown MCP client \"" + client + "\"; supported: "
                            + supportedClients().joinIntoString(", ")
                            + " (use --print-mcp-config for other clients)");
    }

    if (info.kind == Kind::cli)
    {
        summary = info.displayName + " manages its own configuration; run this "
                  "to add RouteMIDI:" + newLine + "  " + info.cliCommand;
        return Result::ok();
    }

    return mergeConfigFile(configPathForClient(info.name), exePath, summary);
}

} // namespace mcpconfig
