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
    void add(int channel);

    // hands out the top (least recently used) channel as a 1-16 value, or 0
    // when the bucket is empty; the channel sinks to the bottom of the taken
    // section so it is reused as late as possible
    int take();

    // releases one use of a channel (1-16); if the channel is still in use by
    // another note it moves to the bottom of the taken section, otherwise it
    // moves to the bottom of the released section
    void release(int channel);

    void clear();

    bool isEmpty() const { return top_ == -1; }

private:
    void extract(int channel);
    void extremize(int channel);

    int top_;            // channel index of the top of the bucket, -1 when empty
    int previous_[16];   // previous channel index in the ring for each channel
    int next_[16];       // next channel index in the ring for each channel
    int taken_[16];      // how many times each channel is currently taken
    int bottomReleased_; // channel index of the bottom of the released section
};
