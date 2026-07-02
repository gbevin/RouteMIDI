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

#include "ApplicationCommand.h"

// The machine-readable command metadata for scripts and AI agents, printed by
// "routemidi --schema json" and returned by the MCP get_schema tool.
namespace schema
{

// renders the command table as a JSON document: every command with its stage,
// arity and argument names, plus the route rules, the processing order and the
// number/note conventions
String commandsJson(const Array<ApplicationCommand>& commands, int defaultOctaveMiddleC);

} // namespace schema
