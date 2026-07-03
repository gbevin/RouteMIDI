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

#include "TestHelpers.h"
#include "../Source/ApplicationState.h"

// Exercises every native transform through ApplicationCommand::transform.
class TransformTests : public UnitTest
{
public:
    TransformTests() : UnitTest("Transforms", "Transforms") {}

    void runTest() override
    {
        ApplicationState state;

        beginTest("Channel transforms");
        {
            MidiMessage m = MidiMessage::noteOn(3, 60, (uint8)100);
            expect(makeCommand(CHANNEL_MAP, {"3", "5"}).transform(state, m));
            expectEquals(m.getChannel(), 5);

            // a non-matching channel is left alone
            m = MidiMessage::noteOn(4, 60, (uint8)100);
            expect(makeCommand(CHANNEL_MAP, {"3", "5"}).transform(state, m));
            expectEquals(m.getChannel(), 4);

            m = MidiMessage::controllerEvent(7, 10, 20);
            expect(makeCommand(CHANNEL_SET, {"2"}).transform(state, m));
            expectEquals(m.getChannel(), 2);

            m = MidiMessage::noteOn(16, 60, (uint8)100);
            expect(makeCommand(CHANNEL_ADD, {"2"}).transform(state, m));
            expectEquals(m.getChannel(), 2);   // 16 + 2 wraps to 2

            m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(CHANNEL_ADD, {"-1"}).transform(state, m));
            expectEquals(m.getChannel(), 16);  // 1 - 1 wraps to 16
        }

        beginTest("Note transforms");
        {
            MidiMessage m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(TRANSPOSE, {"12"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 72);

            // out of range is dropped
            m = MidiMessage::noteOn(1, 120, (uint8)100);
            expect(! makeCommand(TRANSPOSE, {"12"}).transform(state, m));

            m = MidiMessage::noteOn(1, 5, (uint8)100);
            expect(! makeCommand(TRANSPOSE, {"-10"}).transform(state, m));

            m = MidiMessage::noteOn(1, 36, (uint8)100);
            expect(makeCommand(NOTE_MAP, {"36", "60"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 60);

            m = MidiMessage::noteOn(1, 37, (uint8)100);
            expect(makeCommand(NOTE_MAP, {"36", "60"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 37);   // unchanged
        }

        beginTest("Diatonic transpose (shift by scale steps)");
        {
            // C major, +2 scale steps: C4 -> E4, D4 -> F4 (a diatonic third)
            MidiMessage m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(DIATONIC_TRANSPOSE, {"C", "major", "2"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 64);

            m = MidiMessage::noteOn(1, 62, (uint8)100);
            expect(makeCommand(DIATONIC_TRANSPOSE, {"C", "major", "2"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 65);

            // negative steps wrap down through the octave: C4 -1 step -> B3
            m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(DIATONIC_TRANSPOSE, {"C", "major", "-1"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 59);

            // a full seven steps in a heptatonic scale is exactly one octave
            m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(DIATONIC_TRANSPOSE, {"C", "major", "7"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 72);

            // an out-of-scale note is snapped into the key before shifting: C#4
            // snaps to C4, then +1 step -> D4
            m = MidiMessage::noteOn(1, 61, (uint8)100);
            expect(makeCommand(DIATONIC_TRANSPOSE, {"C", "major", "1"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 62);

            // an unknown scale leaves the note untouched
            m = MidiMessage::noteOn(1, 61, (uint8)100);
            expect(makeCommand(DIATONIC_TRANSPOSE, {"C", "bogus", "2"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 61);
        }

        beginTest("Message-type conversions (note <-> CC / program change)");
        {
            // notecc: a note becomes a Control Change, velocity as value
            MidiMessage m = MidiMessage::noteOn(3, 60, (uint8)100);
            expect(makeCommand(NOTE_TO_CC, {"60", "64"}).transform(state, m));
            expect(m.isController());
            expectEquals(m.getChannel(), 3);          // channel preserved
            expectEquals(m.getControllerNumber(), 64);
            expectEquals(m.getControllerValue(), 100);

            m = MidiMessage::noteOff(3, 60, (uint8)0);
            expect(makeCommand(NOTE_TO_CC, {"60", "64"}).transform(state, m));
            expect(m.isController());
            expectEquals(m.getControllerValue(), 0);  // note-off sends 0

            m = MidiMessage::noteOn(1, 61, (uint8)100);
            expect(makeCommand(NOTE_TO_CC, {"60", "64"}).transform(state, m));
            expect(m.isNoteOn());                     // a different note is untouched

            // ccnote: a Control Change becomes a note, threshold at 64
            m = MidiMessage::controllerEvent(2, 64, 127);
            expect(makeCommand(CC_TO_NOTE, {"64", "C3"}).transform(state, m));
            expect(m.isNoteOn());
            expectEquals(m.getChannel(), 2);
            expectEquals(m.getNoteNumber(), 60);      // C3
            expectEquals((int)m.getVelocity(), 127);

            m = MidiMessage::controllerEvent(2, 64, 0);
            expect(makeCommand(CC_TO_NOTE, {"64", "C3"}).transform(state, m));
            expect(m.isNoteOff());
            expectEquals(m.getNoteNumber(), 60);

            m = MidiMessage::controllerEvent(2, 7, 100);
            expect(makeCommand(CC_TO_NOTE, {"64", "C3"}).transform(state, m));
            expect(m.isController());                 // a different CC is untouched

            // notepc: a note-on becomes a Program Change, note-off dropped
            m = MidiMessage::noteOn(5, 48, (uint8)100);
            expect(makeCommand(NOTE_TO_PROGRAM, {"48", "5"}).transform(state, m));
            expect(m.isProgramChange());
            expectEquals(m.getChannel(), 5);
            expectEquals(m.getProgramChangeNumber(), 5);

            m = MidiMessage::noteOff(5, 48, (uint8)0);
            expect(! makeCommand(NOTE_TO_PROGRAM, {"48", "5"}).transform(state, m));  // dropped
        }

        beginTest("Scale quantize (snap notes to a key)");
        {
            // C major: chromatic notes fall to the nearest scale note, ties down
            MidiMessage m = MidiMessage::noteOn(1, 61, (uint8)100);   // C#4
            expect(makeCommand(SCALE, {"C", "major"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 60);                      // -> C4

            m = MidiMessage::noteOn(1, 66, (uint8)100);               // F#4
            expect(makeCommand(SCALE, {"C", "major"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 65);                      // -> F4

            // a note already in the scale is left alone
            m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(SCALE, {"C", "major"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 60);

            // note-offs snap too, so on/off pairs stay matched
            m = MidiMessage::noteOff(1, 61, (uint8)0);
            expect(makeCommand(SCALE, {"C", "major"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 60);

            // the root and scale matter: F#4 belongs to G major and is kept
            m = MidiMessage::noteOn(1, 66, (uint8)100);
            expect(makeCommand(SCALE, {"G", "major"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 66);

            // pentatonic scales skip more notes
            m = MidiMessage::noteOn(1, 65, (uint8)100);               // F4
            expect(makeCommand(SCALE, {"C", "majpent"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 64);                      // -> E4

            // a custom comma-separated scale (semitone degrees from the root)
            m = MidiMessage::noteOn(1, 64, (uint8)100);               // E4
            expect(makeCommand(SCALE, {"C", "0,3,7"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 63);                      // -> Eb4

            // additional scales snap as expected
            m = MidiMessage::noteOn(1, 64, (uint8)100);               // E4
            expect(makeCommand(SCALE, {"C", "diminished"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 63);                      // -> Eb4

            m = MidiMessage::noteOn(1, 64, (uint8)100);               // E4
            expect(makeCommand(SCALE, {"C", "fifth"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 67);                      // -> G4 (root/fifth)

            // major blues keeps the natural third, minor blues snaps it away
            m = MidiMessage::noteOn(1, 64, (uint8)100);
            expect(makeCommand(SCALE, {"C", "majblues"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 64);
            m = MidiMessage::noteOn(1, 64, (uint8)100);
            expect(makeCommand(SCALE, {"C", "minblues"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 63);

            // names are case-insensitive and ignore spaces, dashes and underscores
            m = MidiMessage::noteOn(1, 64, (uint8)100);
            expect(makeCommand(SCALE, {"C", "Harmonic-Minor"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 63);                      // harmonic minor has Eb

            // an unknown scale name leaves the note untouched
            m = MidiMessage::noteOn(1, 61, (uint8)100);
            expect(makeCommand(SCALE, {"C", "bogus"}).transform(state, m));
            expectEquals(m.getNoteNumber(), 61);
        }

        beginTest("Chord stacking and diatonic composition");
        {
            // chord adds notes at fixed semitone intervals above each played note
            Route route;
            route.inputs.add(new RouteInput());
            auto& input = *route.inputs[0];
            route.transforms.add(makeCommand(CHORD, {"4", "7"}));     // major triad
            auto out = state.applyTransforms(route, input, MidiMessage::noteOn(1, 60, (uint8)100));
            expectEquals(out.size(), 3);
            expectEquals(out[0].getNoteNumber(), 60);
            expectEquals(out[1].getNoteNumber(), 64);
            expectEquals(out[2].getNoteNumber(), 67);
            for (auto& n : out)
            {
                expect(n.isNoteOn());
                expectEquals(n.getChannel(), 1);
                expectEquals((int)n.getVelocity(), 100);
            }

            // note-offs expand the same way, so the whole chord is released
            out = state.applyTransforms(route, input, MidiMessage::noteOff(1, 60, (uint8)0));
            expectEquals(out.size(), 3);
            for (auto& n : out) expect(n.isNoteOff());

            // chord notes out of range are dropped, the played note still passes
            Route high;
            high.inputs.add(new RouteInput());
            high.transforms.add(makeCommand(CHORD, {"7"}));
            out = state.applyTransforms(high, *high.inputs[0], MidiMessage::noteOn(1, 125, (uint8)100));
            expectEquals(out.size(), 1);
            expectEquals(out[0].getNoteNumber(), 125);

            // non-note messages pass through a chord untouched
            out = state.applyTransforms(route, input, MidiMessage::controllerEvent(1, 7, 100));
            expectEquals(out.size(), 1);
            expect(out[0].isController());

            // chord then scale yields diatonic chords: a triad on E in C major
            // becomes E-G-B (the raw G# is snapped down to G)
            Route diatonic;
            diatonic.inputs.add(new RouteInput());
            diatonic.transforms.add(makeCommand(CHORD, {"4", "7"}));
            diatonic.transforms.add(makeCommand(SCALE, {"C", "major"}));
            out = state.applyTransforms(diatonic, *diatonic.inputs[0], MidiMessage::noteOn(1, 64, (uint8)100));
            expectEquals(out.size(), 3);
            expectEquals(out[0].getNoteNumber(), 64);   // E
            expectEquals(out[1].getNoteNumber(), 67);   // G# -> G
            expectEquals(out[2].getNoteNumber(), 71);   // B
        }

        beginTest("Note latch (toggle and hold modes)");
        {
            // toggle: the first press latches the note on, the second releases it
            Route route;
            route.inputs.add(new RouteInput());
            auto& input = *route.inputs[0];
            route.transforms.add(makeCommand(LATCH, {"toggle"}));

            auto out = state.applyTransforms(route, input, MidiMessage::noteOn(1, 60, (uint8)100));
            expectEquals(out.size(), 1);
            expect(out[0].isNoteOn());                   // latched on

            out = state.applyTransforms(route, input, MidiMessage::noteOff(1, 60, (uint8)0));
            expectEquals(out.size(), 0);                 // note-off is swallowed

            out = state.applyTransforms(route, input, MidiMessage::noteOn(1, 60, (uint8)100));
            expectEquals(out.size(), 1);
            expect(out[0].isNoteOff());                  // second press toggles it off
            expectEquals(out[0].getNoteNumber(), 60);

            // a second note-off after toggling off is swallowed too
            out = state.applyTransforms(route, input, MidiMessage::noteOff(1, 60, (uint8)0));
            expectEquals(out.size(), 0);

            // non-note messages pass through untouched
            out = state.applyTransforms(route, input, MidiMessage::controllerEvent(1, 7, 100));
            expectEquals(out.size(), 1);
            expect(out[0].isController());

            // hold: a new gesture replaces the previously held chord
            Route hold;
            hold.inputs.add(new RouteInput());
            auto& hin = *hold.inputs[0];
            hold.transforms.add(makeCommand(LATCH, {"hold"}));

            // play and release C and E together: both keep sounding
            expectEquals(state.applyTransforms(hold, hin, MidiMessage::noteOn(1, 60, (uint8)100)).size(), 1);
            expectEquals(state.applyTransforms(hold, hin, MidiMessage::noteOn(1, 64, (uint8)100)).size(), 1);
            expectEquals(state.applyTransforms(hold, hin, MidiMessage::noteOff(1, 60, (uint8)0)).size(), 0);
            expectEquals(state.applyTransforms(hold, hin, MidiMessage::noteOff(1, 64, (uint8)0)).size(), 0);

            // pressing a new note releases the held chord, then sounds the new note
            out = state.applyTransforms(hold, hin, MidiMessage::noteOn(1, 67, (uint8)100));
            expectEquals(out.size(), 3);                 // off C, off E, on G (order may vary)
            int noteOns = 0, noteOffs = 0;
            for (auto& n : out)
            {
                if (n.isNoteOn())  ++noteOns;
                if (n.isNoteOff()) ++noteOffs;
            }
            expectEquals(noteOns, 1);
            expectEquals(noteOffs, 2);
        }

        beginTest("Mono forces one note at a time by priority");
        {
            // a small helper: apply a mono transform over a sequence, returning
            // the (on?,note) pairs it produces
            auto runMono = [&state](const StringArray& opts, const Array<MidiMessage>& in)
            {
                Route route;
                route.inputs.add(new RouteInput());
                route.transforms.add(makeCommand(MONO, opts));
                Array<MidiMessage> out;
                for (const auto& m : in)
                {
                    out.addArray(state.applyTransforms(route, *route.inputs[0], m));
                }
                return out;
            };

            auto describe = [](const MidiMessage& m)
            {
                return (m.isNoteOn() ? "on " : m.isNoteOff() ? "off " : "?") + String(m.getNoteNumber());
            };

            // last-note priority: a new note steals, releasing it falls back to
            // the previously held note (retriggered at its own velocity)
            auto out = runMono({}, {
                MidiMessage::noteOn(1, 60, (uint8)100),   // C sounds
                MidiMessage::noteOn(1, 64, (uint8)90),    // E steals: off C, on E
                MidiMessage::noteOff(1, 64, (uint8)0),    // E off, fall back: on C
                MidiMessage::noteOff(1, 60, (uint8)0) });  // C off
            StringArray got;
            for (const auto& m : out) got.add(describe(m));
            expect(got == StringArray({ "on 60", "off 60", "on 64", "off 64", "on 60", "off 60" }));
            // the fallback note keeps its own velocity
            for (const auto& m : out)
            {
                if (m.isNoteOn() && m.getNoteNumber() == 60)
                {
                    expectEquals((int)m.getVelocity(), 100);
                }
            }

            // low-note priority: a higher note waits under a held lower one
            out = runMono({"low"}, {
                MidiMessage::noteOn(1, 60, (uint8)100),   // C sounds
                MidiMessage::noteOn(1, 67, (uint8)90),    // G higher: waits, no output
                MidiMessage::noteOff(1, 60, (uint8)0),    // C off, fall back to G
                MidiMessage::noteOff(1, 67, (uint8)0) });
            got.clear();
            for (const auto& m : out) got.add(describe(m));
            expect(got == StringArray({ "on 60", "off 60", "on 67", "off 67" }));

            // high-note priority: a lower note waits under a held higher one
            out = runMono({"high"}, {
                MidiMessage::noteOn(1, 67, (uint8)100),   // G sounds
                MidiMessage::noteOn(1, 60, (uint8)90),    // C lower: waits
                MidiMessage::noteOff(1, 67, (uint8)0),    // G off, fall back to C
                MidiMessage::noteOff(1, 60, (uint8)0) });
            got.clear();
            for (const auto& m : out) got.add(describe(m));
            expect(got == StringArray({ "on 67", "off 67", "on 60", "off 60" }));

            // non-note messages pass through untouched
            out = runMono({}, { MidiMessage::controllerEvent(1, 7, 100) });
            expectEquals(out.size(), 1);
            expect(out[0].isController());
        }

        beginTest("Sustain pedal applied to the note stream");
        {
            Route route;
            route.inputs.add(new RouteInput());
            auto& input = *route.inputs[0];
            route.transforms.add(makeCommand(SUSTAIN));

            // the pedal itself is consumed
            auto out = state.applyTransforms(route, input, MidiMessage::controllerEvent(1, 64, 127));
            expectEquals(out.size(), 0);

            // notes sound normally while the pedal is down
            out = state.applyTransforms(route, input, MidiMessage::noteOn(1, 60, (uint8)100));
            expectEquals(out.size(), 1);
            expect(out[0].isNoteOn());

            // their note-offs are held back
            out = state.applyTransforms(route, input, MidiMessage::noteOff(1, 60, (uint8)0));
            expectEquals(out.size(), 0);

            // re-striking a sustained note retriggers it: off first, then on
            out = state.applyTransforms(route, input, MidiMessage::noteOn(1, 60, (uint8)90));
            expectEquals(out.size(), 2);
            expect(out[0].isNoteOff());
            expect(out[1].isNoteOn());
            expectEquals((int)out[1].getVelocity(), 90);

            // release the key again, then lift the pedal: the held note-off is sent
            out = state.applyTransforms(route, input, MidiMessage::noteOff(1, 60, (uint8)0));
            expectEquals(out.size(), 0);
            out = state.applyTransforms(route, input, MidiMessage::controllerEvent(1, 64, 0));
            expectEquals(out.size(), 1);
            expect(out[0].isNoteOff());
            expectEquals(out[0].getNoteNumber(), 60);

            // with the pedal up, note-offs pass straight through
            out = state.applyTransforms(route, input, MidiMessage::noteOn(1, 62, (uint8)100));
            expectEquals(out.size(), 1);
            out = state.applyTransforms(route, input, MidiMessage::noteOff(1, 62, (uint8)0));
            expectEquals(out.size(), 1);
            expect(out[0].isNoteOff());

            // the pedal is tracked per channel: channel 2's pedal is up
            out = state.applyTransforms(route, input, MidiMessage::controllerEvent(1, 64, 127));
            expectEquals(out.size(), 0);
            out = state.applyTransforms(route, input, MidiMessage::noteOn(2, 60, (uint8)100));
            expectEquals(out.size(), 1);
            out = state.applyTransforms(route, input, MidiMessage::noteOff(2, 60, (uint8)0));
            expectEquals(out.size(), 1);

            // other messages pass through untouched
            out = state.applyTransforms(route, input, MidiMessage::controllerEvent(1, 7, 100));
            expectEquals(out.size(), 1);
            expect(out[0].isController());
        }

        beginTest("Sostenuto pedal holds only the notes down at press");
        {
            Route route;
            route.inputs.add(new RouteInput());
            auto& input = *route.inputs[0];
            route.transforms.add(makeCommand(SOSTENUTO));

            // hold C, press the pedal (consumed), then play E
            auto out = state.applyTransforms(route, input, MidiMessage::noteOn(1, 60, (uint8)100));
            expectEquals(out.size(), 1);
            out = state.applyTransforms(route, input, MidiMessage::controllerEvent(1, 66, 127));
            expectEquals(out.size(), 0);
            out = state.applyTransforms(route, input, MidiMessage::noteOn(1, 64, (uint8)100));
            expectEquals(out.size(), 1);

            // C was down at the press, so its note-off is held; E's passes
            out = state.applyTransforms(route, input, MidiMessage::noteOff(1, 60, (uint8)0));
            expectEquals(out.size(), 0);
            out = state.applyTransforms(route, input, MidiMessage::noteOff(1, 64, (uint8)0));
            expectEquals(out.size(), 1);
            expect(out[0].isNoteOff());
            expectEquals(out[0].getNoteNumber(), 64);

            // lifting the pedal releases the captured note
            out = state.applyTransforms(route, input, MidiMessage::controllerEvent(1, 66, 0));
            expectEquals(out.size(), 1);
            expect(out[0].isNoteOff());
            expectEquals(out[0].getNoteNumber(), 60);
        }

        beginTest("Velocity transforms (note-on only)");
        {
            MidiMessage m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(VELOCITY_SCALE, {"0.5"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 50);

            m = MidiMessage::noteOn(1, 60, (uint8)10);
            expect(makeCommand(VELOCITY_SCALE, {"0.01"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 1);   // clamped to minimum 1

            m = MidiMessage::noteOn(1, 60, (uint8)10);
            expect(makeCommand(VELOCITY_SET, {"64"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 64);

            m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(VELOCITY_ADD, {"50"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 127);  // clamped to maximum

            // velclip clamps into a window (order of bounds does not matter)
            m = MidiMessage::noteOn(1, 60, (uint8)10);
            expect(makeCommand(VELOCITY_CLIP, {"40", "100"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 40);
            m = MidiMessage::noteOn(1, 60, (uint8)127);
            expect(makeCommand(VELOCITY_CLIP, {"40", "100"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 100);
            m = MidiMessage::noteOn(1, 60, (uint8)64);
            expect(makeCommand(VELOCITY_CLIP, {"100", "40"}).transform(state, m));  // reversed bounds
            expectEquals((int)m.getVelocity(), 64);

            // velcomp squeezes toward the mid-range (64): amount 0.5 halves the
            // distance, amount 1 leaves it unchanged
            m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(VELOCITY_COMPRESS, {"0.5"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 82);   // 64 + (100-64)*0.5
            m = MidiMessage::noteOn(1, 60, (uint8)20);
            expect(makeCommand(VELOCITY_COMPRESS, {"0.5"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 42);   // 64 + (20-64)*0.5

            // velinvert mirrors the 1-127 range: soft becomes loud, 64 stays
            m = MidiMessage::noteOn(1, 60, (uint8)100);
            expect(makeCommand(VELOCITY_INVERT).transform(state, m));
            expectEquals((int)m.getVelocity(), 28);   // 128 - 100
            m = MidiMessage::noteOn(1, 60, (uint8)1);
            expect(makeCommand(VELOCITY_INVERT).transform(state, m));
            expectEquals((int)m.getVelocity(), 127);
            m = MidiMessage::noteOn(1, 60, (uint8)64);
            expect(makeCommand(VELOCITY_INVERT).transform(state, m));
            expectEquals((int)m.getVelocity(), 64);

            // note-off velocity is left untouched
            m = MidiMessage::noteOff(1, 60, (uint8)80);
            expect(makeCommand(VELOCITY_SET, {"10"}).transform(state, m));
            expect(m.isNoteOff());
            expectEquals((int)m.getVelocity(), 80);
        }

        beginTest("Control Change transforms");
        {
            MidiMessage m = MidiMessage::controllerEvent(1, 7, 100);
            expect(makeCommand(CONTROL_CHANGE_MAP, {"7", "11"}).transform(state, m));
            expectEquals(m.getControllerNumber(), 11);
            expectEquals(m.getControllerValue(), 100);

            m = MidiMessage::controllerEvent(1, 7, 100);
            expect(makeCommand(CONTROL_CHANGE_ADD, {"7", "10"}).transform(state, m));
            expectEquals(m.getControllerValue(), 110);

            m = MidiMessage::controllerEvent(1, 7, 100);
            expect(makeCommand(CONTROL_CHANGE_SCALE, {"7", "0.5"}).transform(state, m));
            expectEquals(m.getControllerValue(), 50);

            // ccinvert mirrors the 0-127 value range
            m = MidiMessage::controllerEvent(1, 7, 100);
            expect(makeCommand(CONTROL_CHANGE_INVERT, {"7"}).transform(state, m));
            expectEquals(m.getControllerValue(), 27);
            m = MidiMessage::controllerEvent(1, 7, 0);
            expect(makeCommand(CONTROL_CHANGE_INVERT, {"7"}).transform(state, m));
            expectEquals(m.getControllerValue(), 127);

            // a different controller is left alone
            m = MidiMessage::controllerEvent(1, 8, 100);
            expect(makeCommand(CONTROL_CHANGE_SCALE, {"7", "0.5"}).transform(state, m));
            expectEquals(m.getControllerValue(), 100);
            m = MidiMessage::controllerEvent(1, 8, 100);
            expect(makeCommand(CONTROL_CHANGE_INVERT, {"7"}).transform(state, m));
            expectEquals(m.getControllerValue(), 100);
        }

        beginTest("Control Change range rescaling");
        {
            // the input range maps linearly onto the output range
            MidiMessage m = MidiMessage::controllerEvent(1, 11, 0);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "0", "127", "20", "100"}).transform(state, m));
            expectEquals(m.getControllerValue(), 20);
            m = MidiMessage::controllerEvent(1, 11, 127);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "0", "127", "20", "100"}).transform(state, m));
            expectEquals(m.getControllerValue(), 100);
            m = MidiMessage::controllerEvent(1, 11, 64);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "0", "127", "20", "100"}).transform(state, m));
            expectEquals(m.getControllerValue(), 60);

            // values outside the input range clamp to the nearest endpoint,
            // which calibrates a pedal that never reaches its extremes
            m = MidiMessage::controllerEvent(1, 11, 10);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "20", "100", "0", "127"}).transform(state, m));
            expectEquals(m.getControllerValue(), 0);
            m = MidiMessage::controllerEvent(1, 11, 110);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "20", "100", "0", "127"}).transform(state, m));
            expectEquals(m.getControllerValue(), 127);

            // a reversed output range inverts the response
            m = MidiMessage::controllerEvent(1, 11, 0);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "0", "127", "127", "0"}).transform(state, m));
            expectEquals(m.getControllerValue(), 127);
            m = MidiMessage::controllerEvent(1, 11, 100);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "0", "127", "127", "0"}).transform(state, m));
            expectEquals(m.getControllerValue(), 27);

            // a reversed input range keeps its stated endpoint mapping
            m = MidiMessage::controllerEvent(1, 11, 0);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "127", "0", "0", "127"}).transform(state, m));
            expectEquals(m.getControllerValue(), 127);

            // a collapsed input range pins the output to its low endpoint
            m = MidiMessage::controllerEvent(1, 11, 30);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "64", "64", "0", "127"}).transform(state, m));
            expectEquals(m.getControllerValue(), 0);

            // a different controller is left alone
            m = MidiMessage::controllerEvent(1, 12, 64);
            expect(makeCommand(CONTROL_CHANGE_RESCALE, {"11", "0", "127", "20", "100"}).transform(state, m));
            expectEquals(m.getControllerValue(), 64);
        }

        beginTest("Program Change transforms");
        {
            MidiMessage m = MidiMessage::programChange(1, 5);
            expect(makeCommand(PROGRAM_CHANGE_MAP, {"5", "10"}).transform(state, m));
            expectEquals(m.getProgramChangeNumber(), 10);

            m = MidiMessage::programChange(1, 5);
            expect(makeCommand(PROGRAM_CHANGE_ADD, {"1"}).transform(state, m));
            expectEquals(m.getProgramChangeNumber(), 6);
        }

        beginTest("Pitch Bend transforms");
        {
            MidiMessage m = MidiMessage::pitchWheel(1, 8192);
            expect(makeCommand(PITCH_BEND_ADD, {"100"}).transform(state, m));
            expectEquals(m.getPitchWheelValue(), 8292);

            m = MidiMessage::pitchWheel(1, 12288);
            expect(makeCommand(PITCH_BEND_SCALE, {"0.5"}).transform(state, m));
            expectEquals(m.getPitchWheelValue(), 10240);   // scaled around centre 8192

            m = MidiMessage::pitchWheel(1, 12288);
            expect(makeCommand(PITCH_BEND_SET, {"0"}).transform(state, m));
            expectEquals(m.getPitchWheelValue(), 0);

            // pbinvert mirrors the bend around the centre; the centre stays put
            m = MidiMessage::pitchWheel(1, 12288);
            expect(makeCommand(PITCH_BEND_INVERT).transform(state, m));
            expectEquals(m.getPitchWheelValue(), 4096);
            m = MidiMessage::pitchWheel(1, 8192);
            expect(makeCommand(PITCH_BEND_INVERT).transform(state, m));
            expectEquals(m.getPitchWheelValue(), 8192);
            m = MidiMessage::pitchWheel(1, 0);
            expect(makeCommand(PITCH_BEND_INVERT).transform(state, m));
            expectEquals(m.getPitchWheelValue(), 16383);   // clamped mirror
        }

        beginTest("Channel Pressure transforms");
        {
            MidiMessage m = MidiMessage::channelPressureChange(1, 50);
            expect(makeCommand(CHANNEL_PRESSURE_ADD, {"10"}).transform(state, m));
            expectEquals(m.getChannelPressureValue(), 60);

            m = MidiMessage::channelPressureChange(1, 100);
            expect(makeCommand(CHANNEL_PRESSURE_SCALE, {"0.5"}).transform(state, m));
            expectEquals(m.getChannelPressureValue(), 50);

            m = MidiMessage::channelPressureChange(1, 10);
            expect(makeCommand(CHANNEL_PRESSURE_SET, {"64"}).transform(state, m));
            expectEquals(m.getChannelPressureValue(), 64);

            // cpinvert mirrors the 0-127 pressure range
            m = MidiMessage::channelPressureChange(1, 100);
            expect(makeCommand(CHANNEL_PRESSURE_INVERT).transform(state, m));
            expectEquals(m.getChannelPressureValue(), 27);
        }

        beginTest("Gamma curves");
        {
            // gamma 1.0 is linear (identity)
            MidiMessage m = MidiMessage::noteOn(1, 60, (uint8)64);
            expect(makeCommand(VELOCITY_CURVE, {"1.0"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 64);

            // gamma 2.0 attenuates: 64/127 -> ^2 -> ~32
            m = MidiMessage::noteOn(1, 60, (uint8)64);
            expect(makeCommand(VELOCITY_CURVE, {"2.0"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 32);

            // endpoints are preserved (max stays max)
            m = MidiMessage::noteOn(1, 60, (uint8)127);
            expect(makeCommand(VELOCITY_CURVE, {"2.0"}).transform(state, m));
            expectEquals((int)m.getVelocity(), 127);

            // CC value curve
            m = MidiMessage::controllerEvent(1, 7, 64);
            expect(makeCommand(CONTROL_CHANGE_CURVE, {"7", "2.0"}).transform(state, m));
            expectEquals(m.getControllerValue(), 32);
            m = MidiMessage::controllerEvent(1, 7, 0);
            expect(makeCommand(CONTROL_CHANGE_CURVE, {"7", "2.0"}).transform(state, m));
            expectEquals(m.getControllerValue(), 0);

            // channel pressure curve
            m = MidiMessage::channelPressureChange(1, 64);
            expect(makeCommand(CHANNEL_PRESSURE_CURVE, {"2.0"}).transform(state, m));
            expectEquals(m.getChannelPressureValue(), 32);
        }

        beginTest("Transforms ignore unrelated message types");
        {
            MidiMessage m = MidiMessage::noteOn(1, 60, (uint8)100);
            const MidiMessage original = m;
            expect(makeCommand(CONTROL_CHANGE_MAP, {"7", "11"}).transform(state, m));
            expectEquals(m.getNoteNumber(), original.getNoteNumber());
            expect(m.isNoteOn());
        }
    }
};

static TransformTests transformTests;
