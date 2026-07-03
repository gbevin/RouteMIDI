# RouteMIDI

[![License: GPL v3](https://img.shields.io/badge/license-GPLv3-blue.svg)](COPYING.md)
[![Release](https://img.shields.io/github/v/release/gbevin/RouteMIDI?sort=semver)](https://github.com/gbevin/RouteMIDI/releases/latest)
[![CI](https://github.com/gbevin/RouteMIDI/actions/workflows/ci.yml/badge.svg)](https://github.com/gbevin/RouteMIDI/actions/workflows/ci.yml)

RouteMIDI is a multi-platform command-line tool that makes it very easy to connect MIDI ports together, passing MIDI messages between them while filtering, transforming and converting them along the way.

All the heavy lifting is done by the wonderful JUCE library.

The project website is https://github.com/gbevin/RouteMIDI

RouteMIDI is the third member of a family of command-line MIDI tools, alongside [SendMIDI](https://github.com/gbevin/SendMIDI) (send MIDI messages) and [ReceiveMIDI](https://github.com/gbevin/ReceiveMIDI) (receive and monitor MIDI messages). It deliberately shares their concise command vocabulary, number and note-name parsing, and JavaScript scripting, so if you know one of them, you already know most of RouteMIDI.

## Purpose

RouteMIDI turns your computer into a flexible MIDI patchbay. It connects one or more MIDI input ports to one or more MIDI output ports, and lets you decide exactly which messages flow through and how they are modified on the way. Typical uses are merging and splitting ports, remapping channels, transposing a keyboard, scaling velocities, filtering out clock or aftertouch, or running arbitrary JavaScript on each message.

It can also be driven programmatically. RouteMIDI publishes a machine-readable command schema (`--schema json`) and includes a built-in Model Context Protocol server (`--mcp`), so scripts and AI assistants can discover the commands, start routes, and inspect or live-edit them while they run; see [AI.md](AI.md).

## ShowMIDI

If you're looking for a beautiful GUI to effortlessly visualize MIDI activity without having to wade through log files, please take a look at my other tool ShowMIDI:
https://github.com/gbevin/ShowMIDI

## Download

You can download pre-built binaries from the release section:
https://github.com/gbevin/RouteMIDI/releases

Since RouteMIDI is free and open-source, you can also easily build it yourself. Just take a look into the Builds directory when you download the sources.

If you're using the macOS Homebrew package manager, you can install RouteMIDI with:
```
brew install gbevin/tools/routemidi
```

## Usage

To use it, simply type "routemidi" or "routemidi.exe" on the command line and follow it with a series of commands. These commands have purposefully been chosen to be concise and easy to remember, so that it's extremely fast and intuitive to set up a MIDI routing.

These are all the supported commands:
```
Routing and ports:
  in        name       Add a MIDI input (- for stdin text); a new route starts
                       after outputs
  out       name       Add a MIDI output to the route (- for stdout text)
  vin       (name)     Add a virtual MIDI input to the route (Linux/macOS)
  vout      (name)     Add a virtual MIDI output to the route (Linux/macOS)
  list                 List the available MIDI input and output ports
  panic                Send all-notes-off on disconnect, exit and zone change

Configuration:
  file      path       Load commands from the specified program file
  dec                  Interpret the next numbers as decimals by default
  hex                  Interpret the next numbers as hexadecimals by default
  omc       number     Set octave for middle C, defaults to 3

Monitoring:
  nn                   Monitor notes as numbers instead of names
  ts                   Prefix monitored messages with a timestamp
  mon                  Print each routed message (quiet by default)
  src                  Prefix monitored messages with the input port name

Filters:
  not                  Negate the next filter, blocking matching messages
  ch        number     Restrict the route to a MIDI channel (1-16)
  voice                Pass all Channel Voice messages
  note                 Pass all Note messages
  on        (note)     Pass Note On, optionally for note (0-127)
  off       (note)     Pass Note Off, optionally for note (0-127)
  pp        (note)     Pass Poly Pressure, optionally for note (0-127)
  cc        (number)   Pass Control Change, optionally for controller (0-127)
  cc14      (number)   Pass 14-bit CC, optionally for MSB controller (0-31)
  nrpn                 Pass NRPN traffic (CC 6, 38, 98, 99)
  rpn                  Pass RPN traffic (CC 6, 38, 100, 101)
  pc        (number)   Pass Program Change, optionally for program (0-127)
  cp                   Pass Channel Pressure
  pb                   Pass Pitch Bend
  sr                   Pass all System Real-Time messages
  clock                Pass Timing Clock
  start                Pass Start
  stop                 Pass Stop
  cont                 Pass Continue
  as                   Pass Active Sensing
  rst                  Pass Reset
  sc                   Pass all System Common messages
  syx                  Pass System Exclusive
  syf       path       Capture routed System Exclusive to a .syx file
  tc                   Pass MIDI Time Code Quarter Frame
  spp                  Pass Song Position Pointer
  ss                   Pass Song Select
  tun                  Pass Tune Request
  noterange low high   Pass notes within a note range (key split)
  velrange  low high   Pass note-ons within a velocity range (vel split)
  ccrange   number low Pass a Control Change only when its value is in a range
            high
  inscale   root scale Pass notes that belong to a scale (root and name)
  mpemaster zone[:n]   Pass the master channel of an MPE zone (e.g. lower)
  mpemember zone[:n]   Pass the member channels of an MPE zone (e.g. upper:7)
  mpezone   zone[:n]   Pass a whole MPE zone (its master and member channels)

Transforms:
  chmap     from to    Remap channel-voice messages from one channel to another
  chset     number     Force all channel-voice messages onto a channel
  chadd     number     Add N to the channel, wrapping 1-16 (may be negative)
  transp    semitones  Transpose notes by N semitones (out-of-range dropped)
  dtransp   root scale Transpose within a scale by N scale steps (stays in key)
            steps
  notemap   from to    Remap a specific note number to another
  scale     root scale Snap notes to the nearest note of a scale (root, name)
  chord     intervals  Stack notes at the given semitone intervals (a chord)
  latch     (mode)     Keep notes on after release; toggle (default) or hold
  mono      (priority) Force monophony; priority last (default), low or high
  sustain              Apply the sustain pedal (CC 64) to the notes themselves
  sost                 Apply the sostenuto pedal (CC 66) to the notes themselves
  notecc    note cc    Turn a note into a Control Change (velocity as value)
  ccnote    cc note    Turn a Control Change into a note (64+ on, else off)
  notepc    note       Turn a note-on into a Program Change (note-off dropped)
            program
  velscale  factor     Scale note-on velocity by a factor (clamped 1-127)
  velset    number     Set a fixed note-on velocity (1-127)
  veladd    number     Add an offset to note-on velocity (clamped 1-127)
  velcurve  gamma      Apply a gamma curve to note-on velocity (1-127)
  velclip   min max    Clamp note-on velocity into a min-max range
  velcomp   amount     Squeeze note-on velocity toward the mid-range (0-1)
  velinvert            Invert note-on velocity (soft becomes loud)
  ccmap     from to    Remap a Control Change controller number
  ccadd     number     Add an offset to a controller's value (clamped 0-127)
            value
  ccscale   number     Scale a controller's value by a factor (clamped 0-127)
            factor
  cccurve   number     Apply a gamma curve to a controller's value
            gamma
  ccinvert  number     Invert a controller's value (0-127 mirrored)
  ccrescale number     Rescale a controller's value from one range onto another
            inlow      (a reversed output range inverts)
            inhigh
            outlow
            outhigh
  pcmap     from to    Remap a Program Change number
  pcadd     number     Add an offset to Program Change number (clamped 0-127)
  pbadd     number     Add an offset to Pitch Bend (clamped 0-16383)
  pbscale   factor     Scale Pitch Bend around center by a factor (0-16383)
  pbset     number     Set a fixed Pitch Bend value (0-16383)
  pbinvert             Invert Pitch Bend around the center (up becomes down)
  cpadd     number     Add an offset to Channel Pressure (clamped 0-127)
  cpscale   factor     Scale Channel Pressure by a factor (clamped 0-127)
  cpset     number     Set a fixed Channel Pressure value (0-127)
  cpcurve   gamma      Apply a gamma curve to Channel Pressure
  cpinvert             Invert Channel Pressure (0-127 mirrored)
  nrpnadd   param      Add an offset to an NRPN value (clamped to its
            number     resolution)
  nrpnscale param      Scale an NRPN value by a factor (clamped to its
            factor     resolution)
  nrpncurve param      Apply a gamma curve to an NRPN value
            gamma
  rpnadd    param      Add an offset to an RPN value (clamped to its resolution)
            number
  rpnscale  param      Scale an RPN value by a factor (clamped to its
            factor     resolution)
  rpncurve  param      Apply a gamma curve to an RPN value
            gamma
  js        code       Transform each message with this script
  jsf       path       Transform each message with the script in this file

Conversion:
  convert   srctype    Convert a value between cc, cc14, rpn, nrpn, pb, cp, pc &
            [number]   pp. Types cc, cc14, rpn and nrpn take a controller or
            dsttype    parameter number and pp a note (optional on a source,
            [number]   meaning any note), while pb, cp and pc take none; the
                       value is rescaled to the destination resolution

MPE routing:
  mpe       zone[:n]   Relocate an MPE stream between zones (e.g. lower upper),
            zone[:n]   remapping channels
  mpemono   zone[:n]   Collapse an MPE zone onto a single channel for non-MPE
            channel    gear (e.g. upper 1)
  mpexp     channel    Spread a channel's notes across an MPE zone's member
            zone[:n]   channels (e.g. 1 lower)
  mpesplit  zone[:n]   Distribute an MPE zone's voices over the output ports,
            (channel)  one per port, each rechanneled to channel (default 1)
  mpebend   zone[:n]   Rescale member-channel pitch bend from one semitone range
            from to    to another
  mpesens   zone[:n]   Declare a member-channel pitch bend range (RPN 0) for
            semitones  synths that honor it

  -h  or  --help       Print Help (this message) and exit
  --version            Print version information and exit
  --schema json        Print machine-readable command JSON and exit [experimental]
  --mcp                Run a stdio MCP server [experimental]
  --                   Read commands from standard input until it's closed
```

Use `--schema json` for command metadata for scripts, MCP servers and
AI agents. Use `--mcp` to let MCP clients control RouteMIDI over stdio.
These two features are experimental and fast-moving: their JSON and the MCP
tools may change between releases. See AI.md for details.

For driving RouteMIDI from scripts and AI assistants, see **[AI.md](AI.md)**: the `--schema json` command metadata and the `--mcp` MCP server, its tools, the generation contract and the security model. **This integration is experimental and fast-moving**, and unlike the command-line interface its JSON shapes, tool names and protocol version may change between releases.

Alternatively, you can use the following long versions of the commands:
```
  input output virtual-in virtual-out decimal hexadecimal octave-middle-c
  note-numbers timestamp verbose monitor-source channel note-on note-off
  poly-pressure control-change control-change-14 program-change
  channel-pressure pitch-bend system-realtime continue active-sensing reset
  system-common system-exclusive system-exclusive-file time-code song-position
  song-select tune-request note-range velocity-range control-change-range
  in-scale mpe-master mpe-member mpe-zone channel-map channel-set channel-add
  transpose diatonic-transpose note-map sustain-pedal sostenuto-pedal
  note-to-control-change control-change-to-note note-to-program-change
  velocity-scale velocity-set velocity-add velocity-curve velocity-clip
  velocity-compress velocity-invert control-change-map control-change-add
  control-change-scale control-change-curve control-change-invert
  control-change-rescale program-change-map program-change-add pitch-bend-add
  pitch-bend-scale pitch-bend-set pitch-bend-invert channel-pressure-add
  channel-pressure-scale channel-pressure-set channel-pressure-curve
  channel-pressure-invert nrpn-add nrpn-scale nrpn-curve rpn-add rpn-scale
  rpn-curve javascript javascript-file mpe-mono mpe-expand mpe-split mpe-bend
  mpe-sensitivity
```

## Routes

A *route* connects one or more MIDI inputs to one or more MIDI outputs. Every input on a route is forwarded to every output on that route, so a route can split one input to several outputs, merge several inputs into one output, or both. Any filter and transform commands also belong to the route.

A route starts with an `in` (or `vin`) command. Further `in` commands keep adding inputs to the same route until an `out` (or `vout`) command is given; the next `in` after that begins a new route. So this splits, merges, and starts a second independent route:

```
routemidi in "Keyboard" out "Synth A" out "Synth B" \
          in "Pads" in "Drums" out "Sampler"
```

On Linux and macOS, `vin` and `vout` create virtual ports that other applications can connect to, which is handy for bridging software that has no direct connection of its own:

```
routemidi vin "RouteMIDI In" out "USB MIDI"     # apps send to the virtual input -> hardware
routemidi in "USB MIDI" vout "RouteMIDI Out"    # hardware -> a virtual output apps can read
```

Filter and transform commands can be placed anywhere within a route's commands:

```
routemidi in "LinnStrument" ch 1 transp 12 out "Bidule 1" \
          in "Keystep" cc out "Bidule 1" out "Synth"
```

Each message a route receives flows through the route's processing stages in a **fixed order**, no matter where the commands appear between the route's inputs and outputs:

1. **Filters** decide whether the message passes at all.
2. **Transforms** (including `js` scripts) modify it, applied in the order they were written.
3. **MPE operations** (`mpe`, `mpemono`, `mpexp`, `mpebend`, `mpesens`) rechannel, fold or expand it, in the order they were written.
4. **Conversions** (`convert` and the RPN/NRPN value transforms) reassemble and convert controller values.
5. The result goes to every output of the route (or is distributed across them by `mpesplit`).

Only the relative order *within* the transform stage and *within* the MPE stage matters: a `transp` always runs before an `mpemono`, even when it is written after it.

By default RouteMIDI works silently. Add the `mon` command to print each routed message (after filtering and transformation) for debugging, optionally with `ts` for timestamps and `nn` for note numbers instead of names.

## Filters

Filters select which messages are allowed to pass through a route. As soon as one or more positive filters are present, only the messages matching at least one of them are forwarded (everything else is dropped). With no filters, everything passes.

Prefix any filter with `not` to make it block instead: matching messages are dropped and everything else passes. Positive and negative filters can be combined: a message must then match at least one positive filter and none of the negative ones.

The `ch` command narrows a route to a single MIDI channel; combined with type filters it restricts them to that channel.

The number that selects which message a filter matches (for `ch`, `on`, `off`, `pp`, `cc`, `cc14` and `pc`) may be a single value or an inclusive range written as `lo..hi`. The range form also accepts note names. The `..` separator is used (rather than `-`) so it doesn't clash with note names like `C-2`.

```
routemidi in "Keyboard" cc 1..10 out "Synth"        # only CC 1 through 10
routemidi in "Keyboard" on C3..C5 out "Synth"       # only note-ons in a range
routemidi in "Keyboard" ch 1..4 out "Synth"         # only channels 1-4
```

The `noterange` and `velrange` filters pass notes within a note or velocity range, which (combined with the multi-route model) makes keyboard and velocity splits easy. Unlike `on lo..hi` (which matches only note-ons), `noterange` matches note-ons, note-offs and poly pressure together, and `velrange` always passes note-offs so a velocity split can't leave notes stuck.

The `ccrange` filter is the same idea for a controller: it passes a Control Change only when its value falls within a `low high` window, so `ccrange 1 64 127` lets the modulation wheel through only in its upper half. As always, `not ccrange 1 64 127` inverts it (blocking that window and passing everything else).

The `cc14`, `nrpn` and `rpn` filters match the constituent Control Change messages of a 14-bit CC, an NRPN or an RPN. `cc14` without a number passes every 14-bit-capable controller (MSB 0-31 together with its LSB 32-63), or with a number just that MSB controller and its LSB. `nrpn` passes the NRPN traffic (CC 98, 99 plus the data-entry CC 6, 38) and `rpn` the RPN traffic (CC 100, 101 plus CC 6, 38).

The `inscale` filter passes only the notes that belong to a key, taking a root and a scale name from the [same list as the `scale` transform](#transforms). It's the filtering counterpart of `scale`: where `scale` bends stray notes onto the nearest scale note, `inscale` simply drops them (and `not inscale` keeps only the out-of-key notes). It matches note-ons, note-offs and poly pressure together, so a note that passes is always released.

```
routemidi in "Keyboard" note out "Synth"            # only note messages
routemidi in "Keyboard" not clock out "Synth"       # everything except clock
routemidi in "Keyboard" ch 1 cc 1 out "Synth"       # only CC 1 on channel 1
routemidi in "Knobs" rpn out "Synth"                # only RPN traffic
routemidi in "Keyboard" inscale C major out "Synth" # only notes in C major
routemidi in "Wheel" ccrange 1 64 127 out "Synth"   # mod wheel only in its top half
# key split: bottom half to a bass, top half to a lead
routemidi in "Keyboard" noterange C-2 B2 out "Bass" \
          in "Keyboard" noterange C3 G8 out "Lead"
# velocity split: soft notes to a pad, hard notes to a lead
routemidi in "Keyboard" velrange 1 63 out "Pad" \
          in "Keyboard" velrange 64 127 out "Lead"
```

## Transforms

Transforms modify the messages that pass the filters, and are applied in the order in which they appear in the route. A transform that would push a value out of range (for instance transposing a note past 0-127) drops that message.

```
routemidi in "Keyboard" transp 12 chset 2 out "Synth"   # octave up, on channel 2
routemidi in "Multi" chmap 10 1 out "Synth"             # move channel 10 onto channel 1
routemidi in "Pads" notemap 36 60 out "Drum"            # remap note 36 to 60
routemidi in "Fader" ccmap 7 11 out "Mixer"             # remap CC 7 to CC 11
routemidi in "Fader" ccscale 7 0.5 out "Mixer"          # halve the values of CC 7
routemidi in "Keyboard" velscale 0.5 out "Synth"        # halve note-on velocities
routemidi in "Keyboard" pbscale 0.5 out "Synth"         # halve the pitch-bend depth
routemidi in "Keyboard" cpscale 0.5 out "Synth"         # halve channel pressure
```

The `velcurve`, `cccurve` and `cpcurve` transforms apply a gamma curve to note-on velocity, a controller's value, or channel pressure. A gamma below 1 boosts low values (a concave curve), above 1 attenuates them (convex), and 1 is linear; the minimum, maximum and overall shape are preserved.

```
routemidi in "Keyboard" velcurve 0.5 out "Synth"        # easier to play loud
routemidi in "Fader" cccurve 7 2.0 out "Mixer"          # finer control at low end
```

Two more velocity transforms tame dynamics. `velclip` clamps note-on velocity into a `min max` window (the order of the two bounds doesn't matter), so nothing plays quieter or louder than you want. `velcomp` squeezes velocity toward the mid-range by an amount from 0 to 1: `1` leaves it untouched, `0.5` halves the distance from the centre, and `0` flattens everything to a single value, a quick way to even out an uneven playing hand.

```
routemidi in "Keyboard" velclip 40 100 out "Synth"      # never too soft or too loud
routemidi in "Keyboard" velcomp 0.5 out "Synth"         # tighten the dynamic range
```

The invert transforms mirror a value across its range: `velinvert` flips note-on velocity (soft becomes loud), `ccinvert` flips a controller's value (0 becomes 127 and vice versa, for instance to reverse a pedal or fader that works backwards), `cpinvert` flips Channel Pressure, and `pbinvert` mirrors Pitch Bend around its centre so a bend up becomes the same bend down.

```
routemidi in "Pedal" ccinvert 11 out "Synth"            # reverse an expression pedal
routemidi in "Keyboard" pbinvert out "Synth"            # bend up becomes bend down
```

`ccrescale` maps a controller's input range onto a different output range: it takes the controller number followed by `inlow inhigh outlow outhigh`, clamps the incoming value into the input range and rescales it linearly onto the output range. That calibrates a controller that never quite reaches its extremes, tames one that is too sensitive, or offsets its response; reversing the output bounds inverts the response at the same time.

```
routemidi in "Pedal" ccrescale 11 20 108 0 127 out "Synth"   # use the pedal's real travel
routemidi in "Fader" ccrescale 7 0 127 40 100 out "Mixer"    # keep the level in a window
routemidi in "Pedal" ccrescale 11 0 127 127 0 out "Synth"    # invert while rescaling
```

A few transforms change a message from one type to another. `notecc` turns a specific note into a Control Change (the note-on velocity becomes the value, and the note-off sends 0); `ccnote` turns a Control Change into a note (a value of 64 or more triggers a note-on with that value as the velocity, below 64 a note-off); and `notepc` turns a note-on into a Program Change (the note-off is dropped). `ccnote` is aimed at switch- or pedal-style controllers; a continuously changing CC would retrigger the note. Since `notecc` uses the velocity as the value, put a `velset` in front of it for a fixed value.

```
routemidi in "Pad" notecc 60 64 out "Synth"             # a pad key holds the sustain CC
routemidi in "Foot" ccnote 64 C1 out "Drums"            # a sustain pedal plays a kick
routemidi in "Pads" notepc 36 0 notepc 37 1 out "Synth" # two pads select two patches
```

The `scale` transform snaps every note to the nearest note of a musical key, so a performance always stays in tune. It takes a root (a note name such as `C`, `F#` or `Bb`, or a number) and a scale name. Notes that are already in the scale pass through unchanged, and a note exactly between two scale notes snaps to the lower one; note-offs snap the same way as note-ons so held notes are always released. Scale names are case-insensitive and any spaces, dashes and underscores are ignored, so `harmonicminor`, `harmonic-minor` and `Harmonic Minor` are all the same.

These scale names are supported (aliases in parentheses):

| Scale name | Semitones from the root |
| --- | --- |
| `chromatic` | 0 1 2 3 4 5 6 7 8 9 10 11 |
| `major` (`ionian`) | 0 2 4 5 7 9 11 |
| `minor` (`aeolian`, `naturalminor`) | 0 2 3 5 7 8 10 |
| `dorian` | 0 2 3 5 7 9 10 |
| `phrygian` | 0 1 3 5 7 8 10 |
| `lydian` | 0 2 4 6 7 9 11 |
| `mixolydian` | 0 2 4 5 7 9 10 |
| `locrian` | 0 1 3 5 6 8 10 |
| `harmonicminor` | 0 2 3 5 7 8 11 |
| `melodicminor` | 0 2 3 5 7 9 11 |
| `majorpentatonic` (`majpent`, `pentatonic`) | 0 2 4 7 9 |
| `minorpentatonic` (`minpent`) | 0 3 5 7 10 |
| `majorblues` (`majblues`) | 0 2 3 4 7 9 |
| `minorblues` (`minblues`, `blues`) | 0 3 5 6 7 10 |
| `diminished` (`dim`) | 0 2 3 5 6 8 9 11 |
| `wholetone` | 0 2 4 6 8 10 |
| `spanish` (`phrygiandominant`) | 0 1 4 5 7 8 10 |
| `romani` (`gypsy`, `hungarianminor`) | 0 2 3 6 7 8 11 |
| `arabian` | 0 2 4 5 6 8 10 |
| `egyptian` | 0 2 5 7 10 |
| `ryukyu` | 0 4 5 7 11 |
| `augmented` (`maj3rd`) | 0 4 8 |
| `diminished7` (`dim7`, `min3rd`) | 0 3 6 9 |
| `fifth` (`power`, `5th`) | 0 7 |

You can also give a custom scale as a comma-separated list of semitone degrees from the root, for instance `scale D 0,2,3,5,7,9,10` for a D minor scale.

The `dtransp` transform is a diatonic transpose: it shifts notes by a number of *scale steps* within a key rather than by a fixed number of semitones, so the result always stays in the scale. It takes a root, a scale name (the same list as `scale`) and a step count that may be negative. In C major, `dtransp C major 2` turns every note into the note a diatonic third above it (C becomes E, D becomes F, and so on), and seven steps of a seven-note scale is exactly one octave. Notes that aren't already in the key are snapped into it before the shift. This is different from `transp`, which moves everything by the same chromatic interval regardless of key.

```
routemidi in "Keyboard" dtransp C major 2 out "Synth"   # harmonize a diatonic third up
routemidi in "Keyboard" dtransp A minor -2 out "Synth"  # a third down, staying in A minor
```

The `chord` transform turns each note into a chord by stacking extra notes at fixed semitone intervals above (or below) the one that was played. `chord 4 7` adds a major third and a fifth, `chord 3 7` a minor third and a fifth, and negative intervals stack notes underneath (`chord -12` doubles an octave down). Each note-on emits the whole chord and each note-off releases it, and chord notes that fall outside 0-127 are dropped.

Because transforms run in order, `chord` and `scale` combine into a diatonic harmonizer: stack a plain chromatic chord and then snap the result into a key. For example `chord 4 7 scale C major` plays a triad on every note that always belongs to C major, so an E becomes E-G-B and an F becomes F-A-C.

```
routemidi in "Keyboard" scale C minor out "Synth"       # keep a solo in C minor
routemidi in "Keyboard" chord 4 7 out "Synth"           # play major triads
routemidi in "Keyboard" chord 4 7 scale C major out "Synth"  # diatonic triads in C major
```

The `latch` transform keeps notes sounding after their keys are released, so you can hold a drone or a chord without keeping your fingers down. It has two modes, given as an optional argument. In `toggle` mode (the default) each press of a note flips it on or off: press a key to start it, press the same key again to stop it. In `hold` mode the note or chord you play keeps sounding until you start a new gesture, and pressing a fresh key (after letting go of all the others) releases the previous notes and latches the new ones, like an arpeggiator's hold. Incoming note-offs are swallowed in both modes, and everything is released when the route hits `panic` (on disconnect, exit or an MPE zone change). Because transforms run in order, putting `latch` after `chord` latches whole generated chords at once.

```
routemidi in "Keyboard" latch out "Synth"               # tap notes to toggle a drone
routemidi in "Keyboard" latch hold out "Synth"          # hold the last chord you played
routemidi in "Keyboard" chord 4 7 latch hold out "Synth"  # hold whole triads hands-free
```

The `mono` transform forces monophony (only one note sounds at a time), which is handy for driving a mono synth from a polyphonic controller. When you hold several notes it picks the winner by an optional priority: `last` (the default) lets each new note take over, `low` keeps the lowest held note (classic bass-synth behaviour) and `high` the highest. Releasing the sounding note falls back to the next note still held, retriggered at its own velocity, and notes that lose priority simply wait silently until they win it. It works per channel, so to collapse a polyphonic MPE performance onto one mono synth, put a `chset` in front of it (or run it after an MPE collapse) to bring the notes onto a single channel first.

```
routemidi in "Keyboard" mono out "Synth"                # last-note mono
routemidi in "Bass" mono low out "Synth"                # lowest note wins
routemidi in "LinnStrument" chset 1 mono out "Mono"     # collapse MPE to a mono synth
```

The `sustain` and `sost` transforms apply a pedal to the note stream itself, for synths and samplers whose MIDI implementation ignores the pedal (or has none). `sustain` holds back every note-off that arrives while the sustain pedal (CC 64) is down and sends them when the pedal is lifted; `sost` does the same for the sostenuto pedal (CC 66), but only for the notes whose keys were down at the moment the pedal was pressed, so notes played afterwards are unaffected. The pedal message itself is consumed, since its effect is already applied, and re-striking a note that is sounding with its key released retriggers it cleanly (a note-off is sent first). Both pedals are tracked per channel, and everything held is released when the route hits `panic` (on disconnect, exit or an MPE zone change).

```
routemidi in "Keyboard" sustain out "Sampler"           # pedal works on any sampler
routemidi in "Keyboard" sost out "Synth"                # sostenuto for synths without it
```

For ultimate flexibility, the `js` and `jsf` commands run JavaScript on each message and can inspect it, rewrite it, drop it, or emit additional messages (so one note can become a chord, for instance). See the [JAVASCRIPT.md](JAVASCRIPT.md) documentation file for details.

## Conversions

The `convert` command translates a value from one controller type to another, reassembling and regenerating the underlying MIDI messages as needed:

```
convert <srctype> [number] <dsttype> [number]
```

where each type is one of:

| Type   | Meaning                              | `number` argument         | Value resolution |
|--------|--------------------------------------|---------------------------|------------------|
| `cc`   | 7-bit Control Change                 | controller number (0-127) | 7-bit            |
| `cc14` | 14-bit Control Change (MSB 0-31 + its LSB 32-63) | MSB controller (0-31) | 14-bit  |
| `rpn`  | Registered Parameter Number          | parameter (0-16383)       | 14-bit           |
| `nrpn` | Non-Registered Parameter Number      | parameter (0-16383)       | 14-bit           |
| `pb`   | Pitch Bend                           | none                      | 14-bit           |
| `cp`   | Channel Pressure                     | none                      | 7-bit            |
| `pc`   | Program Change                       | none                      | 7-bit            |
| `pp`   | Poly Pressure                        | note (optional as source) | 7-bit            |

The `cc`, `cc14`, `rpn` and `nrpn` types are followed by a number selecting which controller or parameter. The `pb`, `cp` and `pc` types have a single value per channel, so they take no number.

`pp` is the only per-*note* type, so its number is a note (a name like `C3` or a number). Because poly pressure carries a note that the other types don't, the note behaves differently on each side: as a **destination** it is required (poly pressure has to land on a specific note), while as a **source** it is optional: give one to convert just that note, or omit it to fold *every* note's poly pressure into the destination. That "any note" collapse (`convert pp cp`) is the usual way to feed a synth that has no poly aftertouch.

When several notes are held, the any-note collapse combines them with the **maximum**: the destination follows the hardest-pressed key, and drops back when it is released. This matches how MPE itself combines channel pressure (and what RouteMIDI's own `mpemono` does), so the loudest finger drives the value rather than whichever note happened to move last.

```
routemidi in "Knobs"  convert nrpn 245 cc14 1   out "Synth"
routemidi in "Fader"  convert cc14 1  nrpn 245  out "Module"
routemidi in "Wheel"  convert pb      nrpn 1000 out "Synth"
routemidi in "After"  convert cp      cc 11     out "Synth"
routemidi in "Knob"   convert cc 7    pb        out "Synth"
routemidi in "MPE"    convert pp      cp        out "Mono Synth"  # any note -> channel pressure
routemidi in "Pad"    convert cp      pp C3     out "Sampler"     # channel pressure -> note C3
routemidi in "Grid"   convert pp 60   cc 74     out "Synth"       # one pad's pressure -> CC 74
```

Several conversions can be configured on one route; only the matching messages are converted, everything else passes through untouched. When several rules share the same source, each one emits, so a single source can fan out to several destinations.

Notable details:

* **Bit scaling** by default uses a **Min-Center-Max** method, which preserves minimum, center and maximum across resolutions and round-trips losslessly: widening a 7-bit value to 14-bit and narrowing it back yields the original. So a 7-bit value of 127 becomes the 14-bit maximum 16383, 64 becomes the center 8192, and 0 stays 0.
* For **RPNs whose parameter LSB is 0-31** (the classic absolute MIDI 1.0 RPNs such as pitch-bend sensitivity, tuning and the MPE Configuration Message), the **Zero-Extension with Rounding** method is used instead, for exact backward compatibility. Upscaling pads with zeros (7-bit 127 becomes 16256, not the full 16383) and downscaling rounds to the nearest value. NRPNs and all CCs always use Min-Center-Max.
* Conversions to `rpn`/`nrpn` append the **RPN/NRPN null** (CC 101/100 = 127 or CC 99/98 = 127) to deselect the parameter afterwards.
* When a route converts any `rpn` or `nrpn`, the converter reassembles every (N)RPN from the RPN controller set (CC 6, 38, 96-101) on that route: targeted parameters are converted (their constituent CCs, data increments and closing null are consumed) and any other (N)RPN passes through untouched. Avoid filtering out those CCs on a converting route.
* **CC 6 and 38 do double duty** in MIDI: inside an (N)RPN parameter selection they are data entry, outside one they are the halves of a plain 14-bit CC. The converter tracks the selection, so a device's genuine (N)RPN work (a Pitch Bend Sensitivity declaration, an MPE Configuration Message) is never hijacked by a `cc14 6` rule, and bare CC 6/38 pairs still convert even when the same route also has `rpn`/`nrpn` rules.
* A device streaming **MSB+LSB pairs** (a 14-bit CC, or 14-bit (N)RPN data) converts to exactly **one destination value per pair** once its pairing has been observed; a sender that only ever transmits the MSB keeps converting on every MSB.

### Transforming RPN and NRPN values

You can also modify an RPN or NRPN value in place, the same way `ccadd`/`ccscale`/`cccurve` modify a Control Change. `rpnadd`/`nrpnadd`, `rpnscale`/`nrpnscale` and `rpncurve`/`nrpncurve` each take the parameter number followed by the offset, factor or gamma, reassemble the (N)RPN from its constituent CCs, apply the change to the assembled value, and regenerate it. The value is clamped to its own resolution (14-bit when a data-entry LSB is present, otherwise 7-bit). Parameter *remapping* doesn't need a dedicated command: `convert nrpn 245 nrpn 1000` re-emits NRPN 245 as NRPN 1000.

```
routemidi in "Synth" nrpnscale 1000 0.5 out "Synth"   # halve NRPN 1000's value
routemidi in "Synth" rpnadd 0 -2048 out "Synth"       # lower RPN 0 (bend range)
routemidi in "Synth" nrpncurve 74 2.0 out "Synth"     # finer control low down
```

Like the conversions, these transforms take over the whole RPN controller set on the route, so avoid filtering out CC 6, 38 and 96-101 on the same route.

## MPE

MIDI Polyphonic Expression (MPE) spreads the notes of a performance across several MIDI channels so that each note can have its own pitch bend, channel pressure and CC 74 (timbre). The channels form a *zone*: a **master channel** carrying zone-wide messages plus a contiguous block of **member channels**, one note each. The **Lower Zone** has master channel 1 with members counting up from channel 2; the **Upper Zone** has master channel 16 with members counting down from channel 15. The number of member channels is announced by an *MPE Configuration Message* (RPN 6) on the master channel.

Throughout the MPE commands a zone is written as a single token, `<side>[:<members>]`, where the side is `lower` or `upper` (or just `l`/`u`) and the optional member count defaults to 15. For example `lower`, `upper:7` or `l:5`.

RouteMIDI is *zone-aware*: it knows which channels belong to a zone, so it can relocate, collapse, build or split an MPE stream without scrambling its per-note channels. At a glance, the MPE commands cover these use cases:

| Use case | Command |
|----------|---------|
| Move a stream between zones, or resize it | [`mpe`](#routing-mpe-between-zones) |
| Play a non-MPE synth from an MPE controller | [`mpemono`](#collapsing-mpe-to-a-single-channel) |
| Turn a regular keyboard into MPE | [`mpexp`](#expanding-a-single-channel-to-mpe) |
| Drive a rack of mono synths from one MPE controller | [`mpesplit`](#splitting-mpe-across-separate-output-ports) |
| Make a controller and a synth agree on the bend range | [`mpebend`](#adapting-pitch-bend-range), [`mpesens`](#adapting-pitch-bend-range) |
| Edit or filter just part of a zone | [`mpemaster`, `mpemember`, `mpezone`](#zone-filters) |
| Handle a two-zone (split) controller | [zone filters + per-zone state](#two-zone-controllers) |
| Stop stuck notes when a zone is reconfigured | [`panic`](#panic) |

Wherever a command takes a value (a pitch bend value, a pressure, a sensitivity), RouteMIDI combines the zone's Manager (zone-wide) and Member (per-note) information the way an MPE receiver would, and applies the standard defaults (Manager pitch bend ±2 semitones, Members ±48) when no Pitch Bend Sensitivity has been declared.

### Routing MPE between zones

`mpe <from> <to>` relocates a whole MPE stream from one zone to another, remapping the master channel and each member channel by position (member *i* of the source becomes member *i* of the destination). This is the safe way to, for instance, feed a Lower-Zone controller into a synth configured for the Upper Zone, or to place two controllers on two different zones of one multitimbral synth.

If the destination has fewer member channels than the source uses, the extra members wrap around onto the available ones, so two notes can end up sharing a destination channel. While a destination channel holds a single note its pitch bend, channel pressure and CC 74 pass through as genuine per-note expression; once notes collide on it, those channel-wide dimensions follow a **last-note-wins** rule (only the most recently triggered note's expression is kept, falling back to the remaining note when it is released). Note-ons, note-offs and polyphonic aftertouch (which carries its own note number) always pass through unchanged. When the member count changes, the relocated MPE Configuration Message is rewritten to announce the destination's member count.

```
routemidi in "Seaboard" mpe lower upper out "Synth"      # Lower Zone -> Upper Zone
routemidi in "LinnStrument" mpe lower:15 lower:8 out "8-voice Synth"
# put two Lower-zone controllers onto the two zones of one multitimbral synth
routemidi in "Controller A" out "Synth" \
          in "Controller B" mpe lower upper out "Synth"
```

### Collapsing MPE to a single channel

`mpemono <zone> <channel>` folds every channel of a zone onto one ordinary channel, so an MPE controller can play a non-MPE synth. The notes all play polyphonically on the one channel, and RouteMIDI combines the zone's Manager (zone-wide) and Member (per-note) expression the way an MPE receiver would, keeping as much per-note expression as a single channel allows:

* **Channel pressure** is combined with the **maximum** of the Manager and Member values, and carried per-note as **polyphonic aftertouch**, so each note keeps its own pressure (the one MPE dimension a single channel *can* still carry per-note).
* **Pitch bend** is **summed in semitones** (so the per-note 48-semitone range and the 2-semitone zone range combine correctly), with the result expressed at the combined range; RouteMIDI emits an **RPN 0** on the target declaring that range so the synth reproduces it. Because pitch bend is channel-wide, only the most recently triggered (and still held) note's bend is applied, falling back to the previous held note on release.
* **Timbre (CC 74)** is combined with the **maximum** of the Manager and Member values, also last-note-wins.

```
routemidi in "Seaboard" mpemono lower 1 out "Mono Synth"
```

### Expanding a single channel to MPE

`mpexp <channel> <zone>` does the opposite: it voice-allocates the notes arriving on one channel across an MPE zone's member channels and emits the zone's MPE Configuration Message so the receiver is set up correctly. Each note is given a member channel, reusing a channel as late as possible (which matters for long releases) and sharing a channel with another note when the polyphony exceeds the member-channel count, rather than stealing an already sounding note. Zone-wide messages (pitch bend, channel pressure, CC) on the source channel are sent to the master channel. **Polyphonic aftertouch** is turned into **per-note channel pressure** on the note's member channel (the mirror image of what collapse does), so a keyboard with poly aftertouch drives each MPE note's pressure independently. Channel pressure is set to zero just before each Note On and Note Off. This "MPE-ifies" a regular keyboard so a downstream MPE synth gives each note its own channel.

```
routemidi in "Keyboard" mpexp 1 lower:15 out "MPE Synth"
```

### Splitting MPE across separate output ports

`mpesplit <zone> [channel]` fans an MPE zone out across the route's output ports, treating **each output port as one monophonic voice**. This connects a rack of ordinary mono synths to an MPE controller: a note-on is allocated to a free port (round-robin, stealing the oldest voice when every port is busy), and that note's expression follows it to the same port until it is released. Everything is rechanneled onto a single channel (channel 1 by default, or the optional `channel` argument) so each non-MPE device sees a plain monophonic stream. Because each port carries one voice, the Manager (zone-wide) and that note's Member (per-note) expression are combined per port exactly as in collapse: pitch bend summed in semitones (with an RPN 0 declaring the combined range on the port), channel pressure and CC 74 combined with the maximum. The MPE Configuration Message (RPN 6) is suppressed so the non-MPE devices aren't flooded with it (other RPNs still pass through).

```
# drive three mono synths polyphonically from one MPE controller
routemidi in "LinnStrument" mpesplit lower:15 out "Bass" out "Lead" out "Pad"
# combine with mpexp to spread a regular keyboard across a rack of mono synths
routemidi in "Keyboard" mpexp 1 lower:3 mpesplit lower:3 out "A" out "B" out "C"
```

There are as many simultaneous voices as there are output ports; with fewer ports than fingers, the oldest note is released to make room. In both collapse and split a note's initial combined expression is sent *before* its Note On, so the note starts in the right state.

### Adapting pitch bend range

`mpebend <zone> <from> <to>` rescales the per-note pitch bend on a zone's member channels from one Pitch Bend Sensitivity to another (the master channel and everything else are left untouched). MPE controllers default to a 48-semitone per-note range, but some synths have a smaller or fixed bend range that can't be changed; rescaling makes the two play in tune. For example, a 48-semitone controller feeding a synth whose member channels bend ±12 semitones:

```
routemidi in "Seaboard" mpebend lower:15 48 12 out "12-semitone Synth"
```

`mpesens <zone> <semitones>` takes the complementary approach: instead of rewriting the bend values, it *declares* the member-channel Pitch Bend Sensitivity by injecting an **RPN 0** on the zone's member channels, for a synth that honours RPN 0. Use it when the synth can adopt the range rather than being stuck at one, so no rescaling is needed and the original values play correctly. RouteMIDI sends the declaration once before the first note and again after each MPE Configuration Message (which resets the receiver's sensitivity to its 48-semitone default).

```
routemidi in "Seaboard" mpesens lower:15 48 out "Synth"   # tell the synth members bend +/-48
```

Use `mpebend` for synths with a fixed bend range, or `mpesens` for synths you can configure. Don't use both, since they would correct the same difference twice.

### Zone filters

The `mpemaster <zone>` and `mpemember <zone>` filters pass only the master channel or only the member channels of a zone, and `mpezone <zone>` passes a whole zone (its master *and* members). Combined with the normal transforms this gives zone-safe editing: filter to the part you want to touch, then transform it, without disturbing the rest of the zone. Like any filter, prefixing one with `not` blocks instead of passes, which is handy for keeping just one half of a split controller.

```
# nudge only the zone-wide bend, leaving the per-note channels alone
routemidi in "Seaboard" mpemaster lower pbscale 0.5 out "Synth"
# forward only the lower zone of a split controller (master and members)
routemidi in "Seaboard" mpezone lower:7 out "Synth"
# forward everything except the upper zone
routemidi in "Seaboard" not mpezone upper:7 out "Synth"
```

### Two-zone controllers

A split controller can run a **Lower** zone (master channel 1, members counting up) and an **Upper** zone (master channel 16, members counting down) at the same time, with non-overlapping member ranges, for example `lower:7` (channel 1 + channels 2-8) and `upper:7` (channel 16 + channels 9-15). Each zone is handled independently, so the simplest approach is one route per zone, isolating it with `mpezone`:

```
routemidi in "Seaboard" mpezone lower mpemono lower:7 1 out "Bass" \
          in "Seaboard" mpezone upper mpemono upper:7 2 out "Lead"
```

Both zones can also be processed in a single route. A Lower-zone operation and an Upper-zone operation keep separate per-zone state (each gets its own sensitivity defaults, voice allocation, and so on), so they don't interfere:

```
routemidi in "Linn" mpe lower:7 lower:7 mpe upper:7 upper:7 out "Synth"
```

## Monitoring

RouteMIDI is quiet by default: it just routes. Add `mon` to print every message that a route forwards, in the same text format that ReceiveMIDI uses (and SendMIDI reads). `src` prefixes each monitored line with the input port name, which is handy when several inputs are merged. `ts` adds a timestamp, and `nn` prints notes as numbers instead of names.

```
routemidi in "Keyboard" mon out "Synth"             # echo what is forwarded
routemidi in "A" in "B" mon src out "Synth"         # show which input each came from
```

## Panic

Add `panic` to a route to make it send an all-notes-off safety net (sustain off, sostenuto off and all-notes-off on every channel) to that route's outputs whenever one of its inputs disconnects, and once more when RouteMIDI exits. This prevents stuck notes when a controller is unplugged mid-performance. It also fires when an **MPE zone is reconfigured** mid-stream: when an MPE Configuration Message changes a zone's member count (or turns the zone off), it sends all-notes-off and reset-all-controllers downstream, since non-MPE devices won't reset themselves.

```
routemidi in "Keyboard" panic out "Synth"
```

## Text streams

An input or output port named `-` is a text stream instead of a real MIDI port. `out -` writes each forwarded message to standard output in the same text format as ReceiveMIDI, and `in -` reads that same format from standard input. This lets RouteMIDI sit in a pipeline with SendMIDI, ReceiveMIDI, or any program that speaks the format:

```
receivemidi dev "Keyboard" | routemidi in - transp 12 out - | sendmidi dev "Synth"
echo "channel 1 note-on 60 100" | routemidi in - chset 2 out -
```

## Capturing System Exclusive

The `syf` command adds a destination that appends every forwarded System Exclusive message to a `.syx` file (the standard raw format, including the F0/F7 framing). Combine it with the `syx` filter to capture only SysEx, for example to back up a synth's patch dump:

```
routemidi in "Synth" syx out "Synth" syf dump.syx
```

## Notes about names and numbers

By default, numbers are interpreted in the decimal system, this can be changed to hexadecimal by sending the `hex` command. Additionally, by suffixing a number with `M` or `H`, it will be interpreted as a decimal or hexadecimal respectively.

The MIDI port names don't have to be an exact match. If RouteMIDI can't find the exact name that was specified, it will pick the first MIDI port that contains the provided text, irrespective of case.

Where notes can be provided as arguments, they can also be written as note names, by default from C-2 to G8 which corresponds to note numbers 0 to 127. By setting the octave for middle C, the note name range can be changed. Sharps can be added by using the `#` symbol after the note letter, and flats by using the letter `b`.

## Examples

List all the available MIDI input and output ports on your system:

```
routemidi list
```

Forward everything from a controller to a software synth, unchanged:

```
routemidi in "LinnStrument MIDI" out "Bidule 1"
```

Merge two controllers into one destination:

```
routemidi in "LinnStrument" in "Keystep" out "Bidule 1"
```

Split a keyboard to two synths, transposing one of them down an octave:

```
routemidi in "Keyboard" out "Synth A" in "Keyboard" transp -12 out "Synth B"
```

Send only the notes on channel 1, remapped to channel 10, and print what is routed:

```
routemidi in "Keyboard" ch 1 note chset 10 out "Drum Machine" mon
```

Drop MIDI clock and active sensing while passing everything else:

```
routemidi in "Sequencer" not clock not as out "Synth"
```

Force every incoming note-on to velocity 100 with a script:

```
routemidi in "Keyboard" js "if (MIDI.isNoteOn()) MIDI.setVelocity(100);" out "Synth"
```

Translate a controller's NRPN 245 into a 14-bit CC 1 for a device that expects 14-bit CCs:

```
routemidi in "Knobs" convert nrpn 245 cc14 1 out "Synth"
```

Play a non-MPE synth from an MPE controller, and a regular keyboard from an MPE synth:

```
routemidi in "Seaboard" mpemono lower 1 out "Mono Synth"
routemidi in "Keyboard" mpexp 1 lower:15 out "MPE Synth"
```

## Text File Format

The text file that can be read through the `file` command can contain a list of commands and options, just like when you would have written them manually on the console (without the `routemidi` executable). You can insert new lines instead of spaces and any line that starts with a hash (#) character is a comment.

For instance, this is a text file for one of the examples above:
```
in "Keyboard"
# only notes on channel 1, remapped to channel 10
ch 1
note
chset 10
out "Drum Machine"
mon
```

## Building on Linux

To build RouteMIDI on Linux you need a minimal set of packages installed beforehand, on Ubuntu this can be done with:

```
sudo apt install build-essential pkg-config libasound2-dev
```

After that, go to the `LinuxMakefile` directory

```
cd Builds/LinuxMakefile
```

and build the binary by typing `make`

```
make
```

The resulting binary will be in the `Build/LinuxMakefile/build` directory and can be moved anywhere appropriate on your system, for instance into `/usr/local/bin`:

```
sudo mv build/routemidi /usr/local/bin
```

## Tests

RouteMIDI has a unit test suite living in the sibling `Tests` folder. It uses the
JUCE `UnitTest` framework, so it has no external dependencies, and it is built as
its own console application from the `Tests/RouteMIDITests.jucer` Projucer
project (which reuses the same JUCE modules as the main app). The tests cover the
bit-scaling math, every native transform, the filter logic (including range
selectors), the CC/CC14/RPN/NRPN conversions and value transforms, the MPE zone
routing, the text-MIDI codec, command-stream parsing, and the MCP server (its
command schema, the JSON-RPC tools and the live route lifecycle). A dedicated set
of tests also opens and reconnects real ports through virtual MIDI on macOS and
Linux.

If you change the project, regenerate the build files with the Projucer:

```
Projucer --resave Tests/RouteMIDITests.jucer
```

On macOS, build and run the tests with:

```
cd Tests
xcodebuild -project Builds/MacOSX/RouteMIDITests.xcodeproj -config Release \
           -scheme "RouteMIDITests - ConsoleApp" SYMROOT=build
./Builds/MacOSX/build/Release/RouteMIDITests
```

On Linux:

```
cd Tests/Builds/LinuxMakefile
make
./build/RouteMIDITests
```

The runner prints a summary and exits with a non-zero status if any test fails,
so it can be used directly in continuous integration. Pass a category name (for
instance `Conversions`) to run only part of the suite.

Two shell scripts under `Scripts` drive the built binary end to end, taking the
path to the executable as their argument. `pipe-test.sh` feeds text MIDI through
`in - ... out -` pipelines and checks the output byte for byte, and also
exercises the MCP handshake and route lifecycle over stdio; `use-case-test.sh`
runs a set of realistic routing scenarios (splits, cleanup filters, controller
remaps, musical transforms, MPE expansion and stateful JavaScript). Both run on
all three platforms in continuous integration and can be run locally:

```
./Scripts/pipe-test.sh ./Builds/MacOSX/build/Release/routemidi
./Scripts/use-case-test.sh ./Builds/MacOSX/build/Release/routemidi
```

## SendMIDI and ReceiveMIDI

RouteMIDI is designed to work alongside its sibling command-line tools:

* SendMIDI sends MIDI messages from the command line: https://github.com/gbevin/SendMIDI
* ReceiveMIDI receives and monitors MIDI messages from the command line: https://github.com/gbevin/ReceiveMIDI
