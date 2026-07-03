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

#include <thread>

class ApplicationState;

// The MCP (Model Context Protocol) server: speaks newline-delimited JSON-RPC
// over the stdio transport and implements the RouteMIDI tools (get_schema,
// list_midi_ports, start_route, list_routes, add_commands, replace_command,
// remove_command, panic_route and stop_route) on top of an ApplicationState.
// Run by "routemidi --mcp".
class McpServer
{
public:
    explicit McpServer(ApplicationState& state);
    ~McpServer();   // joins the reader thread, which exits when stdin closes

    // spawns the stdin reader thread: the message loop stays free to run the
    // reconnect timer, each request is handled on the message thread (which
    // serializes route changes with that timer), and responses are written
    // back from the reader thread; the application quits when stdin closes
    void start();

    // handles one parsed JSON-RPC request and returns the response, or a void var
    // for a notification (which has no response). The stdin reader thread calls
    // this for each incoming message; it is the server's core operation.
    var handleRequest(const var& message);

private:

    ApplicationState& state_;
    std::thread thread_;
};
