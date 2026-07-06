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

#include "ParamSelection.h"

ParamSelection::ParamSelection()
{
    for (int ch = 0; ch < 16; ++ch)
    {
        msb[ch] = -1;
        lsb[ch] = -1;
        nrpn[ch] = false;
    }
}

void ParamSelection::trackSelect(int channel, int cc, int value)
{
    if (channel < 1 || channel > 16 || cc < 98 || cc > 101)
    {
        return;
    }
    const int ch = channel - 1;
    if (cc == 99 || cc == 101)
    {
        msb[ch] = value;
    }
    else
    {
        lsb[ch] = value;
    }
    nrpn[ch] = (cc == 98 || cc == 99);
}

bool ParamSelection::active(int channel) const
{
    // at least one real (non-127) select byte, not closed by a completed null
    const int ch = channel - 1;
    return (msb[ch] >= 0 && msb[ch] < 127) || (lsb[ch] >= 0 && lsb[ch] < 127);
}

int ParamSelection::param(int channel) const
{
    const int ch = channel - 1;
    if (msb[ch] < 0 || lsb[ch] < 0 || (msb[ch] == 127 && lsb[ch] == 127))
    {
        return -1;   // incomplete, or deselected by a completed (N)RPN null
    }
    return (msb[ch] << 7) | lsb[ch];
}
