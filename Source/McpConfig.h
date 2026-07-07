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

// Helpers that let RouteMIDI register itself as an MCP server with the local
// AI clients, so a user doesn't have to hand-edit JSON. --print-mcp-config
// emits the block; --install-mcp merges it into a known client's config file.
namespace mcpconfig
{
    // the standalone "mcpServers" JSON block that points an MCP client at this
    // binary running with --mcp, pretty-printed for pasting into a config file
    String serverBlock(const String& exePath);

    // the MCP clients --install-mcp knows how to write, for error messages and
    // the help text
    StringArray supportedClients();

    // the config file a client reads its MCP servers from, resolved for the
    // current platform; an invalid File (see File::exists on the parent) when
    // the client name isn't recognized
    File configPathForClient(const String& client);

    // merges a routemidi server entry into an MCP client config file in place,
    // creating it if needed and preserving any servers already there. Fails
    // without touching the file when it holds JSON that can't be parsed. On
    // success, summary describes what changed for the user.
    Result mergeConfigFile(const File& configFile, const String& exePath, String& summary);

    // resolves the client's config path and merges the entry into it; fails with
    // the list of supported clients when the name isn't recognized
    Result installToClient(const String& client, const String& exePath, String& summary);
}
