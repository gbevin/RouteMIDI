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

// Helpers that let RouteMIDI register itself as an MCP server with the local AI
// clients, so a user doesn't have to hand-edit configuration. --print-mcp-config
// emits the entry in a client's own format; --install-mcp either merges it into
// a client's JSON configuration file or, for a client with its own command,
// reports that command. Each known client declares its format and whether its
// configuration is a JSON file RouteMIDI can safely round-trip or is managed by
// the client's own CLI.
namespace mcpconfig
{
    // The server entry that points an MCP client at this binary running with
    // --mcp, in the given client's own configuration format, ready to paste. An
    // empty client name yields the generic JSON "mcpServers" block. Returns an
    // empty string for a client name that isn't recognized.
    String serverBlock(const String& exePath, const String& client = {});

    // Every client name --print-mcp-config and --install-mcp accept.
    StringArray supportedClients();

    // The configuration file a JSON client reads its MCP servers from, resolved
    // for the current platform; an invalid File (File()) for a client managed by
    // its own CLI, or a name that isn't recognized.
    File configPathForClient(const String& client);

    // Performs --install-mcp for a client. For a JSON client it merges a
    // routemidi entry into the configuration file in place, creating it if
    // needed and preserving any servers already there. For a client with its own
    // command it doesn't touch any file, filling summary with the command to run
    // instead. Fails without writing anything for an unrecognized client or when
    // a file it can't parse would be overwritten. On success, summary is the full
    // message to show the user.
    Result installToClient(const String& client, const String& exePath, String& summary);

    // Merges a routemidi server entry into a JSON "mcpServers" configuration file
    // in place, preserving whatever else is there. Fails without touching the
    // file when it holds JSON that can't be parsed. Exposed for testing.
    Result mergeConfigFile(const File& configFile, const String& exePath, String& summary);
}
