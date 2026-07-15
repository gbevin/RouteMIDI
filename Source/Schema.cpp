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

#include "Schema.h"

namespace schema
{

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

static const char* commandStage(CommandIndex command)
{
    switch (command)
    {
        case INPUT:
        case OUTPUT:
        case VIRTUAL_IN:
        case VIRTUAL_OUT:
        case LIST:
        case PANIC:
        case SYSEX_FILE:
            return "routing";

        case TXTFILE:
        case DECIMAL:
        case HEXADECIMAL:
        case OCTAVE_MIDDLE_C:
            return "configuration";

        case NOTE_NUMBERS:
        case TIMESTAMP:
        case MONITOR:
        case MONITOR_SOURCE:
            return "monitoring";

        case NOT:
            return "filter-modifier";

        case CHANNEL:
        case VOICE:
        case NOTE:
        case NOTE_ON:
        case NOTE_OFF:
        case POLY_PRESSURE:
        case CONTROL_CHANGE:
        case CONTROL_CHANGE_14BIT:
        case NRPN:
        case RPN:
        case PROGRAM_CHANGE:
        case CHANNEL_PRESSURE:
        case PITCH_BEND:
        case SYSTEM_REALTIME:
        case CLOCK:
        case START:
        case STOP:
        case CONTINUE:
        case ACTIVE_SENSING:
        case RESET:
        case SYSTEM_COMMON:
        case SYSTEM_EXCLUSIVE:
        case TIME_CODE:
        case SONG_POSITION:
        case SONG_SELECT:
        case TUNE_REQUEST:
        case NOTE_RANGE:
        case VELOCITY_RANGE:
        case CONTROL_CHANGE_RANGE:
        case CONTROL_CHANGE_14BIT_RANGE:
        case IN_SCALE:
        case MPE_MANAGER:
        case MPE_MEMBER:
        case MPE_ZONE:
            return "filters";

        case CHANNEL_MAP:
        case CHANNEL_SET:
        case CHANNEL_ADD:
        case TRANSPOSE:
        case DIATONIC_TRANSPOSE:
        case NOTE_MAP:
        case SCALE:
        case CHORD:
        case LATCH:
        case MONO:
        case SUSTAIN:
        case SOSTENUTO:
        case NOTE_TO_CC:
        case CC_TO_NOTE:
        case NOTE_TO_PROGRAM:
        case VELOCITY_SCALE:
        case VELOCITY_SET:
        case VELOCITY_ADD:
        case VELOCITY_CURVE:
        case VELOCITY_CLIP:
        case VELOCITY_COMPRESS:
        case VELOCITY_INVERT:
        case CONTROL_CHANGE_MAP:
        case CONTROL_CHANGE_ADD:
        case CONTROL_CHANGE_SCALE:
        case CONTROL_CHANGE_CURVE:
        case CONTROL_CHANGE_INVERT:
        case CONTROL_CHANGE_RESCALE:
        case CONTROL_CHANGE_SET:
        case PROGRAM_CHANGE_MAP:
        case PROGRAM_CHANGE_ADD:
        case PITCH_BEND_ADD:
        case PITCH_BEND_SCALE:
        case PITCH_BEND_SET:
        case PITCH_BEND_INVERT:
        case CHANNEL_PRESSURE_ADD:
        case CHANNEL_PRESSURE_SCALE:
        case CHANNEL_PRESSURE_SET:
        case CHANNEL_PRESSURE_CURVE:
        case CHANNEL_PRESSURE_INVERT:
        case POLY_PRESSURE_ADD:
        case POLY_PRESSURE_SCALE:
        case POLY_PRESSURE_SET:
        case POLY_PRESSURE_CURVE:
        case POLY_PRESSURE_INVERT:
        case JAVASCRIPT:
        case JAVASCRIPT_FILE:
            return "transforms";

        case CONVERT:
        case CC14_ADD:
        case CC14_SCALE:
        case CC14_CURVE:
        case CC14_INVERT:
        case CC14_RESCALE:
        case CC14_SET:
        case NRPN_ADD:
        case NRPN_SCALE:
        case NRPN_CURVE:
        case NRPN_INVERT:
        case NRPN_RESCALE:
        case NRPN_SET:
        case RPN_ADD:
        case RPN_SCALE:
        case RPN_CURVE:
        case RPN_INVERT:
        case RPN_RESCALE:
        case RPN_SET:
            // the 14-bit CC and RPN/NRPN value transforms are assembled in the
            // converter stage, so the route-editing tools address them there
            return "conversions";

        case MPE_RELOCATE:
        case MPE_COLLAPSE:
        case MPE_EXPAND:
        case MPE_BEND:
        case MPE_SENS:
            return "mpe";

        case MPE_SPLIT:
            return "split";

        case NONE:
            return "none";
    }
    return "unknown";
}

static const char* commandArityKind(int expectedOptions)
{
    return expectedOptions < 0 ? "variable" : "fixed";
}

static int commandMinArgs(int expectedOptions)
{
    return expectedOptions < 0 ? 0 : expectedOptions;
}

String commandsJson(const Array<ApplicationCommand>& commands, int defaultOctaveMiddleC)
{
    auto root = new DynamicObject();
    root->setProperty("schema", "https://github.com/gbevin/RouteMIDI/schema/commands-v1");
    root->setProperty("tool", ProjectInfo::projectName);
    // this metadata and the MCP tools are experimental: their shapes and names
    // may change between releases, unlike the command-line interface
    root->setProperty("experimental", true);
    root->setProperty("version", ProjectInfo::versionString);
    root->setProperty("defaultOctaveMiddleC", defaultOctaveMiddleC);
    root->setProperty("defaultNumberBase", "decimal");

    Array<var> commandArray;
    for (const auto& cmd : commands)
    {
        auto command = new DynamicObject();
        command->setProperty("name", cmd.param_);
        if (cmd.altParam_.isNotEmpty())
        {
            command->setProperty("alias", cmd.altParam_);
        }
        command->setProperty("index", static_cast<int>(cmd.command_));
        command->setProperty("section", cmd.section_.isNotEmpty() ? cmd.section_ : String());
        command->setProperty("stage", commandStage(cmd.command_));
        command->setProperty("arity", commandArityKind(cmd.expectedOptions_));
        command->setProperty("minArgs", commandMinArgs(cmd.expectedOptions_));
        command->setProperty("maxArgs", cmd.expectedOptions_ < 0 ? var() : var(cmd.expectedOptions_));
        command->setProperty("args", var(stringArrayToVarArray(cmd.optionsDescriptions_, true)));
        command->setProperty("description", cmd.commandDescriptions_.isEmpty() ? String() : cmd.commandDescriptions_[0]);
        commandArray.add(var(command));
    }
    root->setProperty("commands", var(commandArray));

    Array<var> longAliases;
    for (const auto& cmd : commands)
    {
        if (cmd.altParam_.isNotEmpty())
        {
            auto alias = new DynamicObject();
            alias->setProperty("alias", cmd.altParam_);
            alias->setProperty("name", cmd.param_);
            longAliases.add(var(alias));
        }
    }
    root->setProperty("aliases", var(longAliases));

    Array<var> routeRules;
    routeRules.add("A route starts with in or vin.");
    routeRules.add("Further in or vin commands add inputs to the current route until an out or vout is added.");
    routeRules.add("After outputs exist, the next in or vin starts a new route.");
    routeRules.add("Every input of a route is forwarded to every output of that route, unless mpesplit distributes voices across outputs.");
    routeRules.add("Use - as an input or output name for text MIDI over stdin or stdout.");
    root->setProperty("routeRules", var(routeRules));

    Array<var> stageOrder;
    stageOrder.add("filters");
    stageOrder.add("transforms");
    stageOrder.add("mpe");
    stageOrder.add("conversions");
    stageOrder.add("outputs");
    root->setProperty("processingOrder", var(stageOrder));

    Array<var> notes;
    notes.add("The stage of a processing command matches the stage argument of the MCP route-editing tools: filters, transforms, mpe, conversions or split.");
    notes.add("Filters whitelist matching messages when one or more positive filters are present.");
    notes.add("The not command negates the following filter.");
    notes.add("Transforms run in the written order within the transform stage.");
    notes.add("MPE operations run in the written order within the MPE stage.");
    notes.add("convert accepts the dynamic shape srctype [number] dsttype [number]; pb, cp and pc take no number.");
    notes.add("Numbers are decimal by default; hex changes the default, and M/H suffixes force decimal or hexadecimal.");
    notes.add("Note names use C3 as middle C by default; omc changes the displayed and parsed octave.");
    notes.add("Selectors for ch, on, off, pp, cc, cc14 and pc may be single values or inclusive lo..hi ranges.");
    root->setProperty("notes", var(notes));

    Array<var> textMidiExamples;
    textMidiExamples.add("printf 'channel 1 note-on 60 100\\n' | routemidi in - transp 12 out -");
    textMidiExamples.add("printf 'channel 1 control-change 7 127\\n' | routemidi in - convert cc 7 pb out -");
    root->setProperty("textMidiExamples", var(textMidiExamples));

    return JSON::toString(var(root), true);
}

} // namespace schema
