/*
 * ChannelBucket: hands out MIDI channels from a bucket of allowed channel numbers.
 *
 * Adapted for RouteMIDI from the LinnStrument firmware (ls_channelbucket.h).
 * Copyright 2023 Roger Linn Design (https://www.rogerlinndesign.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ChannelBucket.h"

void ChannelBucket::add(int channel)
{
    if (channel < 1 || channel > 16) return;
    channel -= 1;                       // 0-based index

    if (next_[channel] != -1) return;   // already in the bucket

    if (top_ == -1)
    {
        top_ = channel;
        previous_[channel] = channel;
        next_[channel] = channel;
        taken_[channel] = 0;
        bottomReleased_ = channel;
    }
    else
    {
        previous_[channel] = bottomReleased_;
        previous_[next_[bottomReleased_]] = channel;

        next_[channel] = next_[bottomReleased_];
        next_[bottomReleased_] = channel;

        taken_[channel] = 0;
        bottomReleased_ = channel;
    }
}

int ChannelBucket::take()
{
    if (top_ == -1) return 0;

    int channel = top_;
    top_ = next_[channel];
    taken_[channel]++;
    if (channel == bottomReleased_)
    {
        bottomReleased_ = -1;
    }
    return channel + 1;
}

void ChannelBucket::release(int channel)
{
    if (channel < 1 || channel > 16 || top_ == -1 || next_[channel - 1] == -1) return;
    channel -= 1;

    taken_[channel]--;

    if (taken_[channel] > 0)
    {
        extremize(channel);
        top_ = next_[channel];
    }
    else
    {
        if (bottomReleased_ == -1)
        {
            extremize(channel);
            top_ = channel;
        }
        else if (next_[bottomReleased_] != channel)
        {
            const int releasedEdge = bottomReleased_;
            const int takenEdge = next_[bottomReleased_];

            extract(channel);

            previous_[channel] = releasedEdge;
            next_[releasedEdge] = channel;

            previous_[takenEdge] = channel;
            next_[channel] = takenEdge;
        }
        bottomReleased_ = channel;
    }
}

void ChannelBucket::clear()
{
    top_ = -1;
    bottomReleased_ = -1;
    for (int ch = 0; ch < 16; ++ch)
    {
        previous_[ch] = -1;
        next_[ch] = -1;
        taken_[ch] = 0;
    }
}

void ChannelBucket::extract(int channel)
{
    if (next_[channel] != -1)
    {
        next_[previous_[channel]] = next_[channel];
        previous_[next_[channel]] = previous_[channel];

        previous_[channel] = -1;
        next_[channel] = -1;
    }
}

void ChannelBucket::extremize(int channel)
{
    int bottom = previous_[top_];
    if (bottom == channel) bottom = previous_[channel];
    int top = top_;
    if (top == channel) top = next_[channel];

    extract(channel);

    previous_[channel] = bottom;
    next_[bottom] = channel;

    previous_[top] = channel;
    next_[channel] = top;
}
