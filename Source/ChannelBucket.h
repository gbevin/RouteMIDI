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
 *
 * The available channels are added to the bucket at the beginning. When a note
 * needs a channel it just takes one; that channel moves to the bottom of the
 * bucket and is reused only after all the others have been taken too, so the
 * same channel can end up shared by several notes once the polyphony exceeds the
 * number of channels. The bucket also tracks which channels are taken and which
 * are released, dividing itself into an upper "released" section and a lower
 * "taken" section, so that channels are reused as late as possible. Postponing
 * reuse matters for sounds with long releases.
 */

#pragma once

class ChannelBucket
{
public:
    ChannelBucket() { clear(); }

    // adds a MIDI channel (1-16) to the bucket, just after the bottom-most
    // released channel; channels already present or out of range are ignored
    void add(int channel)
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

    // hands out the top (least recently used) channel as a 1-16 value, or 0
    // when the bucket is empty; the channel sinks to the bottom of the taken
    // section so it is reused as late as possible
    int take()
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

    // releases one use of a channel (1-16); if the channel is still in use by
    // another note it moves to the bottom of the taken section, otherwise it
    // moves to the bottom of the released section
    void release(int channel)
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

    void clear()
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

    bool isEmpty() const { return top_ == -1; }

private:
    void extract(int channel)
    {
        if (next_[channel] != -1)
        {
            next_[previous_[channel]] = next_[channel];
            previous_[next_[channel]] = previous_[channel];

            previous_[channel] = -1;
            next_[channel] = -1;
        }
    }

    void extremize(int channel)
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

    int top_;            // channel index of the top of the bucket, -1 when empty
    int previous_[16];   // previous channel index in the ring for each channel
    int next_[16];       // next channel index in the ring for each channel
    int taken_[16];      // how many times each channel is currently taken
    int bottomReleased_; // channel index of the bottom of the released section
};
