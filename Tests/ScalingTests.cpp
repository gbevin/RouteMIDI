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

#include "JuceHeader.h"

#include "../Source/BitScaling.h"

using namespace bitscaling;

// Verifies the MIDI 2.0 Bit Scaling and Resolution [MA07] algorithms directly
// against the numeric examples in the specification's tables.
class ScalingTests : public UnitTest
{
public:
    ScalingTests() : UnitTest("Bit scaling (MA07)", "Scaling") {}

    void runTest() override
    {
        beginTest("Min-Center-Max upscale 7 -> 16 bits (Table 6)");
        {
            const int in[]  = {  0,    5,    30,    32,    64,    70,    96,   120,   127 };
            const int out[] = {  0, 2560, 15360, 16384, 32768, 35888, 49412, 61895, 65535 };
            for (int i = 0; i < numElementsInArray(in); ++i)
                expectEquals(scaleValue(in[i], 7, 16, MinCenterMax), out[i]);
        }

        beginTest("Min-Center-Max upscale 7 -> 14 bits (min/center/max preserved)");
        {
            expectEquals(scaleValue(0,   7, 14, MinCenterMax), 0);
            expectEquals(scaleValue(64,  7, 14, MinCenterMax), 8192);    // center
            expectEquals(scaleValue(127, 7, 14, MinCenterMax), 16383);   // max
            expectEquals(scaleValue(100, 7, 14, MinCenterMax), 12873);
        }

        beginTest("Min-Center-Max downscale 16 -> 7 bits (Table 9)");
        {
            expectEquals(scaleValue(5120,  16, 7, MinCenterMax), 10);
            expectEquals(scaleValue(32768, 16, 7, MinCenterMax), 64);
            expectEquals(scaleValue(44730, 16, 7, MinCenterMax), 87);
            expectEquals(scaleValue(65535, 16, 7, MinCenterMax), 127);
        }

        beginTest("Min-Center-Max round-trips losslessly for every 7-bit value");
        {
            for (int v = 0; v <= 127; ++v)
            {
                const int up = scaleValue(v, 7, 14, MinCenterMax);
                expectEquals(scaleValue(up, 14, 7, MinCenterMax), v);
            }
        }

        beginTest("Zero-Extension upscale 7 -> 16 bits (Table 10)");
        {
            expectEquals(scaleValue(10,  7, 16, ZeroExtension), 5120);
            expectEquals(scaleValue(64,  7, 16, ZeroExtension), 32768);
            expectEquals(scaleValue(87,  7, 16, ZeroExtension), 44544);
            expectEquals(scaleValue(127, 7, 16, ZeroExtension), 65024);   // not full scale
        }

        beginTest("Zero-Extension upscale 7 -> 14 bits");
        {
            expectEquals(scaleValue(0,   7, 14, ZeroExtension), 0);
            expectEquals(scaleValue(64,  7, 14, ZeroExtension), 8192);
            expectEquals(scaleValue(127, 7, 14, ZeroExtension), 16256);   // not 16383
        }

        beginTest("Zero-Extension downscale 16 -> 7 bits with rounding and clamp (Table 11)");
        {
            expectEquals(scaleValue(5120,  16, 7, ZeroExtension), 10);
            expectEquals(scaleValue(5631,  16, 7, ZeroExtension), 11);
            expectEquals(scaleValue(32768, 16, 7, ZeroExtension), 64);
            expectEquals(scaleValue(44544, 16, 7, ZeroExtension), 87);
            expectEquals(scaleValue(44730, 16, 7, ZeroExtension), 87);
            expectEquals(scaleValue(44800, 16, 7, ZeroExtension), 88);
            expectEquals(scaleValue(65024, 16, 7, ZeroExtension), 127);
            expectEquals(scaleValue(65535, 16, 7, ZeroExtension), 127);   // clamped, not 128
        }

        beginTest("Equal resolutions are identity");
        {
            for (int v = 0; v <= 127; ++v)
            {
                expectEquals(scaleValue(v, 7, 7, MinCenterMax), v);
                expectEquals(scaleValue(v, 7, 7, ZeroExtension), v);
            }
        }
    }
};

static ScalingTests scalingTests;
