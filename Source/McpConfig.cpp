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

// the { command, args:["--mcp"] } entry every supported client's config uses to
// launch this binary as an MCP server
static var serverEntry(const String& exePath)
{
    auto* entry = new DynamicObject();
    entry->setProperty("command", exePath);

    Array<var> args;
    args.add("--mcp");
    entry->setProperty("args", args);

    return var(entry);
}

String serverBlock(const String& exePath)
{
    auto* servers = new DynamicObject();
    servers->setProperty("routemidi", serverEntry(exePath));

    auto* root = new DynamicObject();
    root->setProperty("mcpServers", var(servers));

    return JSON::toString(var(root));
}

StringArray supportedClients()
{
    return { "claude-desktop", "cursor" };
}

File configPathForClient(const String& client)
{
    const String name = client.trim().toLowerCase();

    // Claude Desktop keeps its MCP servers in claude_desktop_config.json under
    // the per-user application data directory, which JUCE maps to the right
    // place on each platform (~/Library/Application Support, %APPDATA%,
    // ~/.config).
    if (name == "claude" || name == "claude-desktop")
    {
        return File::getSpecialLocation(File::userApplicationDataDirectory)
                   .getChildFile("Claude")
                   .getChildFile("claude_desktop_config.json");
    }

    // Cursor reads global MCP servers from ~/.cursor/mcp.json, in the same
    // "mcpServers" shape.
    if (name == "cursor")
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
              + configFile.getFullPathName();
    return Result::ok();
}

Result installToClient(const String& client, const String& exePath, String& summary)
{
    const File configFile = configPathForClient(client);
    if (configFile == File())
    {
        return Result::fail("unknown MCP client \"" + client + "\"; supported: "
                            + supportedClients().joinIntoString(", ")
                            + " (use --print-mcp-config for other clients)");
    }
    return mergeConfigFile(configFile, exePath, summary);
}

} // namespace mcpconfig
