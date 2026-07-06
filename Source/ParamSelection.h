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

// The current (N)RPN parameter selection of a MIDI stream, tracked per channel.
// CC 99/98 carry an NRPN parameter's select MSB/LSB and CC 101/100 an RPN's;
// the two halves together form the 14-bit parameter number, and a completed
// null (both halves 127) ends the selection. While a selection is active the
// data controllers CC 6/38/96/97 operate on the selected parameter. Shared by
// the converter stage (conversion::State) and the nrpn/rpn parameter filters,
// each holding its own instance because the stages observe different message
// streams.
struct ParamSelection
{
    ParamSelection();

    // follows a parameter-select controller (CC 98-101); any other controller
    // is ignored, so every controller message can be handed to it
    void trackSelect(int channel, int cc, int value);

    // whether a selection is in progress: at least one real (non-127) select
    // byte, not closed by a completed null
    bool active(int channel) const;

    // the selected 14-bit parameter number, or -1 while the selection is
    // incomplete or has been ended by a completed null
    int param(int channel) const;

    // whether the current selection arrived via the NRPN selects (CC 98/99)
    bool isNrpn(int channel) const { return nrpn[channel - 1]; }

    int  msb[16], lsb[16];   // last select bytes seen, -1 = never
    bool nrpn[16];           // whether the selection came in via 98/99
};
