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

#include "Route.h"

void PressureCollapse::reset()
{
    for (int c = 0; c < 16; ++c)
    {
        lastMax[c] = -1;
        for (int n = 0; n < 128; ++n) pressure[c][n] = -1;
    }
}

int PressureCollapse::maxPressure(int channel) const
{
    int m = -1;
    for (int n = 0; n < 128; ++n) m = jmax(m, pressure[channel - 1][n]);
    return jmax(0, m);
}

bool PressureCollapse::changed(int channel, int value)
{
    if (lastMax[channel - 1] == value) return false;
    lastMax[channel - 1] = value;
    return true;
}

RouteInput::RouteInput()
{
    for (int c = 0; c < 16; ++c) { rpnSelMSB[c] = -1; rpnSelLSB[c] = -1; }
}
