# RouteMIDI JavaScript support

RouteMIDI can run JavaScript code as a transform stage for each message that flows through a route. The script inspects the current message and may rewrite it, drop it, or trigger side effects (OSC, shell commands). Whatever the script leaves in the `MIDI` object is what gets forwarded to the route's output ports.

Note that this is not fully standards-compliant, and is not as fast as the fancy JIT-compiled engines that you get in browsers, but this is an extremely compact, low-overhead JavaScript interpreter that comes with JUCE.

JavaScript code can be provided directly on the command line by using `js` or `javascript`, but the code can also be read from a file by using the `jsf` and `javascript-file` commands. A `js`/`jsf` command applies to the current route, and runs in the order it appears among that route's transforms, so you can combine it with the native transforms.

The engine that is running the code is stateful and processes all the MIDI messages with the same code. This can be used to make more complex decisions by looking at multiple MIDI messages and keeping track of the state in global variables. Any global variable you set survives from one message to the next, so you can keep counters, remember earlier notes, or count clock ticks.

For example, this accents every fourth note-on by keeping a running count in a global variable:

```
routemidi in "Keyboard" \
  js "if (MIDI.isNoteOn()) { if (typeof count == 'undefined') count = 0; if (++count % 4 == 0) MIDI.setVelocity(127); }" \
  out "Synth"
```

For instance, this route transposes every note up an octave, forces a fixed velocity, and drops the sustain pedal, all in one script:

```
routemidi vin "RM In" \
  js "if (MIDI.isNoteOnOrOff()) { MIDI.setNote(MIDI.getNoteNumber() + 12); if (MIDI.isNoteOn()) MIDI.setVelocity(100); } if (MIDI.isSustainPedalOn() || MIDI.isSustainPedalOff()) MIDI.block();" \
  out "Bidule 1"
```

Or, using a file `remap.js`:

```
routemidi in "LinnStrument" jsf /path/to/remap.js out "Bidule 1"
```

## Rewriting and dropping the forwarded message

These mutators change the message that RouteMIDI forwards. They only take effect for the relevant message type (for instance `setNote` only changes note and poly-pressure messages).

```javascript
MIDI.setChannel(n);            // 1-16
MIDI.setNote(n);               // 0-127, alias setNoteNumber
MIDI.setVelocity(n);           // 0-127, for note-on/note-off
MIDI.setControllerNumber(n);   // 0-127, for control-change
MIDI.setControllerValue(n);    // 0-127, for control-change
MIDI.setProgram(n);            // 0-127, alias setProgramChangeNumber
MIDI.setPitchBend(n);          // 0-16383, alias setPitchWheel
MIDI.setChannelPressure(n);    // 0-127
MIDI.setPolyPressure(n);       // 0-127, alias setAfterTouch

MIDI.block();                  // drop this message, alias drop()
```

## Emitting extra messages

A script can emit additional MIDI messages, which are forwarded to the route's output ports alongside the current message (or instead of it, if you also call `block()`). This lets one input message turn into several, for instance a chord from a single note, or a CC alongside a note.

```javascript
MIDI.sendNoteOn(channel, note, velocity);
MIDI.sendNoteOff(channel, note, velocity);
MIDI.sendController(channel, number, value);      // alias sendControlChange
MIDI.sendProgramChange(channel, program);
MIDI.sendPitchBend(channel, value);               // alias sendPitchWheel
MIDI.sendChannelPressure(channel, value);
MIDI.sendPolyPressure(channel, note, value);      // alias sendAfterTouch
MIDI.sendSysEx([byte, byte, ...]);                // data bytes, no F0/F7
MIDI.send([byte, byte, ...]);                     // a raw MIDI message
```

For example, turning every note into a major triad:

```
routemidi in "Keyboard" js "if (MIDI.isNoteOn()) { var n = MIDI.getNoteNumber(), c = MIDI.getChannel(), v = MIDI.getVelocity(); MIDI.sendNoteOn(c, n + 4, v); MIDI.sendNoteOn(c, n + 7, v); }" out "Synth"
```

## General Utilities

```javascript
Util.command('/full/path/to/executable arguments');
Util.print('some text');
Util.println('some text with newline');
Util.sleep(<milliseconds>);
```

Because the script runs synchronously as each message passes through, `Util.sleep` blocks the whole route for its duration: the current message and every message queued behind it are held up. It's fine for pacing a generated or offline stream, but avoid it when routing a live instrument.

## Sending OSC messages

```javascript
osc_sender = OSC.connect("hostname", port);
osc_sender.send("/osc/path", arg1, arg2, ...);
```

## Checking and retrieving data from the current MIDI message

```javascript
MIDI.getRawData();        // array
MIDI.getRawDataSize();
MIDI.rawData();           // array
MIDI.rawDataSize();

MIDI.getDescription();
MIDI.description();

MIDI.getTimeStamp();
MIDI.getChannel();
MIDI.timeStamp();
MIDI.channel();

MIDI.isSysEx();
MIDI.getSysExData();      // array
MIDI.getSysExDataSize();
MIDI.sysExData();         // array
MIDI.sysExDataSize();

MIDI.isNoteOn();
MIDI.isNoteOff();
MIDI.isNoteOnOrOff();
MIDI.getNoteNumber();
MIDI.getVelocity();
MIDI.getFloatVelocity();
MIDI.noteNumber();
MIDI.velocity();
MIDI.floatVelocity();

MIDI.isSustainPedalOn();
MIDI.isSustainPedalOff();
MIDI.isSostenutoPedalOn();
MIDI.isSostenutoPedalOff();
MIDI.isSoftPedalOn();
MIDI.isSoftPedalOff();

MIDI.isProgramChange();
MIDI.getProgramChangeNumber();
MIDI.programChange();

MIDI.isPitchWheel();
MIDI.isPitchBend();
MIDI.getPitchWheelValue();
MIDI.getPitchBendValue();
MIDI.pitchWheel();
MIDI.pitchBend();

MIDI.isAftertouch();
MIDI.isPolyPressure();
MIDI.getAfterTouchValue();
MIDI.getPolyPressureValue();
MIDI.afterTouch();
MIDI.polyPressure();

MIDI.isChannelPressure();
MIDI.getChannelPressureValue();
MIDI.channelPressure();

MIDI.isController();
MIDI.getControllerNumber();
MIDI.getControllerValue();
MIDI.controllerNumber();
MIDI.controllerValue();

MIDI.isAllNotesOff();
MIDI.isAllSoundOff();
MIDI.isResetAllControllers();

MIDI.isActiveSense();
MIDI.isMidiStart();
MIDI.isMidiContinue();
MIDI.isMidiStop();
MIDI.isMidiClock();
MIDI.isSongPositionPointer();
MIDI.getSongPositionPointerMidiBeat();
MIDI.songPositionPointerMidiBeat();
```
