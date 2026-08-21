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
        case PROGRAM_TO_CC:
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

// an option description in parentheses is optional; every other one is required
static bool isOptionalArg(const String& desc)
{
    return desc.startsWithChar('(');
}

bool availableViaMcp(const ApplicationCommand& command)
{
    switch (command.command_)
    {
        // these write to stdout, read local files, or run code, so a remote
        // client cannot use them; everything else that forms a route can
        case NONE:
        case LIST:
        case MONITOR:
        case MONITOR_SOURCE:
        case NOTE_NUMBERS:
        case TIMESTAMP:
        case TXTFILE:
        case SYSEX_FILE:
        case JAVASCRIPT:
        case JAVASCRIPT_FILE:
            return false;
        default:
            return true;
    }
}

String commandsJson(const Array<ApplicationCommand>& commands, int octaveMiddleC, bool hexadecimal)
{
    auto root = new DynamicObject();
    root->setProperty("schema", "https://github.com/gbevin/RouteMIDI/schema/commands-v1");
    root->setProperty("tool", ProjectInfo::projectName);
    // the contract version is bumped only on a breaking change to this document's
    // shape, independently of the release version, so a client can gate on it;
    // the release version is reported separately
    root->setProperty("contractVersion", 1);
    // the contract is stable as of 1.0.0: a breaking change to this document's
    // shape bumps contractVersion
    root->setProperty("stable", true);
    root->setProperty("version", ProjectInfo::versionString);
    // these reflect the current session state (a route's "hex" or "omc"
    // token persists), so a client sees how inject_midi values parse now
    root->setProperty("octaveMiddleC", octaveMiddleC);
    root->setProperty("numberBase", hexadecimal ? "hexadecimal" : "decimal");

    Array<var> commandArray;
    for (const auto& cmd : commands)
    {
        auto command = new DynamicObject();
        command->setProperty("name", cmd.param_);
        if (cmd.altParam_.isNotEmpty())
        {
            command->setProperty("alias", cmd.altParam_);
        }
        command->setProperty("section", cmd.section_.isNotEmpty() ? cmd.section_ : String());
        command->setProperty("stage", commandStage(cmd.command_));

        // arity: convert has a genuinely dynamic 2-4 token shape; every other
        // command is fixed (all args required) or variable (trailing optional
        // args marked with parentheses in its descriptions)
        int required = 0, total = 0;
        for (const auto& d : cmd.optionsDescriptions_)
        {
            if (d.isNotEmpty())
            {
                ++total;
                if (!isOptionalArg(d)) ++required;
            }
        }
        if (cmd.command_ == CONVERT)
        {
            command->setProperty("arity", "variable");
            command->setProperty("minArgs", 2);
            command->setProperty("maxArgs", 4);
        }
        else if (cmd.expectedOptions_ < 0)
        {
            command->setProperty("arity", "variable");
            command->setProperty("minArgs", required);
            command->setProperty("maxArgs", total);
        }
        else
        {
            command->setProperty("arity", "fixed");
            command->setProperty("minArgs", cmd.expectedOptions_);
            command->setProperty("maxArgs", cmd.expectedOptions_);
        }
        command->setProperty("args", var(stringArrayToVarArray(cmd.optionsDescriptions_, true)));
        command->setProperty("description", cmd.commandDescriptions_.isEmpty() ? String() : cmd.commandDescriptions_[0]);
        command->setProperty("mcpAvailable", availableViaMcp(cmd));
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
    routeRules.add("Use - as an input or output name for text MIDI over stdin or stdout; this is command-line only and not available over MCP.");
    root->setProperty("routeRules", var(routeRules));

    Array<var> stageOrder;
    stageOrder.add("filters");
    stageOrder.add("transforms");
    stageOrder.add("mpe");
    stageOrder.add("conversions");
    stageOrder.add("split");
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
    notes.add("The scale argument of scale, inscale and dtransp is one of scaleNames, or a custom comma-separated list of semitone degrees such as 0,2,4,7,9.");
    root->setProperty("notes", var(notes));

    root->setProperty("scaleNames", var(stringArrayToVarArray(ApplicationCommand::scaleNameList(), true)));

    Array<var> textMidiExamples;
    textMidiExamples.add("printf 'channel 1 note-on 60 100\\n' | routemidi in - transp 12 out -");
    textMidiExamples.add("printf 'channel 1 control-change 7 127\\n' | routemidi in - convert cc 7 pb out -");
    root->setProperty("textMidiExamples", var(textMidiExamples));

    return JSON::toString(var(root), true);
}

} // namespace schema
