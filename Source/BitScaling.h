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
    inline int scaleUpMinCenterMax(int srcVal, int srcBits, int dstBits)
    {
        const int scaleBits = dstBits - srcBits;
        int bitShiftedValue = srcVal << scaleBits;

        const int srcCenter = 1 << (srcBits - 1);
        if (srcVal <= srcCenter)
        {
            return bitShiftedValue;
        }

        // expanded bit-repeat scheme for the range from center to maximum
        const int repeatBits = srcBits - 1;
        const int repeatMask = (1 << repeatBits) - 1;
        int repeatValue = srcVal & repeatMask;
        if (scaleBits > repeatBits)
        {
            repeatValue <<= (scaleBits - repeatBits);
        }
        else
        {
            repeatValue >>= (repeatBits - scaleBits);
        }
        while (repeatValue != 0)
        {
            bitShiftedValue |= repeatValue;
            repeatValue >>= repeatBits;
        }
        return bitShiftedValue;
    }

    // scales value between bit resolutions using the requested method
    inline int scaleValue(int value, int srcBits, int dstBits, ScaleMethod method)
    {
        if (srcBits == dstBits)
        {
            return value;
        }

        const int scaleBits = (dstBits > srcBits) ? (dstBits - srcBits) : (srcBits - dstBits);

        if (dstBits > srcBits)
        {
            // section 4.3: Zero-Extension upscaling is a plain left shift (zero fill)
            if (method == ZeroExtension)
            {
                return value << scaleBits;
            }
            return scaleUpMinCenterMax(value, srcBits, dstBits);
        }

        // section 4.4: Zero-Extension downscaling rounds and clamps
        if (method == ZeroExtension)
        {
            const int halfScaleRange = 1 << (scaleBits - 1);
            const int shifted = (value + halfScaleRange) >> scaleBits;
            const int maxValue = (1 << dstBits) - 1;
            return shifted < maxValue ? shifted : maxValue;
        }
        // section 3.4: Min-Center-Max downscaling is a simple bit shift
        return value >> scaleBits;
    }
}
