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

// Bit scaling methods from the MIDI 2.0 Bit Scaling and Resolution recommended
// practice [MA07]. Min-Center-Max (section 3) is the default for continuous
// 0..100% controllers; Zero-Extension with Rounding (section 4) is used for
// fixed-point/absolute values, namely RPNs whose parameter LSB is 0-31, to
// preserve exact backward compatibility with the classic MIDI 1.0 RPNs.
namespace bitscaling
{
    enum ScaleMethod { MinCenterMax, ZeroExtension };

    // section 3.3: Min-Center-Max upscaling. Preserves minimum, center and
    // maximum across resolutions and round-trips losslessly with the section
    // 3.4 downscale.
    int scaleUpMinCenterMax(int srcVal, int srcBits, int dstBits);

    // scales value between bit resolutions using the requested method
    int scaleValue(int value, int srcBits, int dstBits, ScaleMethod method);
}
