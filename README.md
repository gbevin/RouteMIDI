# RouteMIDI

RouteMIDI is a multi-platform command-line tool that makes it very easy to connect MIDI ports together, passing MIDI messages between them while filtering and transforming them along the way.

All the heavy lifting is done by the wonderful JUCE library.

The project website is https://github.com/gbevin/RouteMIDI

RouteMIDI is the third member of a family of command-line MIDI tools, alongside [SendMIDI](https://github.com/gbevin/SendMIDI) (send MIDI messages) and [ReceiveMIDI](https://github.com/gbevin/ReceiveMIDI) (receive and monitor MIDI messages). It deliberately shares their concise command vocabulary, number and note-name parsing, and JavaScript scripting, so if you know one of them, you already know most of RouteMIDI.

## Community

Join the Forums: https://forum.uwyn.com

## Purpose

RouteMIDI turns your computer into a flexible MIDI patchbay. It connects one or more MIDI input ports to one or more MIDI output ports, and lets you decide exactly which messages flow through and how they are modified on the way. Typical uses are merging and splitting ports, remapping channels, transposing a keyboard, scaling velocities, filtering out clock or aftertouch, or running arbitrary JavaScript on each message.

## ShowMIDI

If you're looking for a beautiful GUI to effortlessly visualize MIDI activity without having to wade through log files, please take a look at my other tool ShowMIDI:
https://github.com/gbevin/ShowMIDI

## Download

Since RouteMIDI is free and open-source, you can easily build it yourself. Just take a look into the Builds directory when you download the sources.

## Usage

To use it, simply type "routemidi" or "routemidi.exe" on the command line and follow it with a series of commands. These commands have purposefully been chosen to be concise and easy to remember, so that it's extremely fast and intuitive to set up a MIDI routing.

These are all the supported commands:
```
  in        name       Add a MIDI input (- for stdin text); a new route starts
                       after outputs
  out       name       Add a MIDI output to the route (- for stdout text)
  vin       (name)     Add a virtual MIDI input to the route (Linux/macOS)
  vout      (name)     Add a virtual MIDI output to the route (Linux/macOS)
  list                 List the available MIDI input and output ports
  panic                Send all-notes-off on disconnect, exit and zone change
  file      path       Load commands from the specified program file
  dec                  Interpret the next numbers as decimals by default
  hex                  Interpret the next numbers as hexadecimals by default
  omc       number     Set octave for middle C, defaults to 3
  nn                   Monitor notes as numbers instead of names
  ts                   Prefix monitored messages with a timestamp
  mon                  Print each routed message (quiet by default)
  src                  Prefix monitored messages with the input port name
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
  mpemaster zone[:n]   Pass the master channel of an MPE zone (e.g. lower)
  mpemember zone[:n]   Pass the member channels of an MPE zone (e.g. upper:7)
  mpezone   zone[:n]   Pass a whole MPE zone (its master and member channels)
  chmap     from to    Remap channel-voice messages from one channel to another
  chset     number     Force all channel-voice messages onto a channel
  chadd     number     Add N to the channel, wrapping 1-16 (may be negative)
  transp    semitones  Transpose notes by N semitones (out-of-range dropped)
  notemap   from to    Remap a specific note number to another
  notecc    note cc    Turn a note into a Control Change (velocity as value)
  ccnote    cc note    Turn a Control Change into a note (64+ on, else off)
  notepc    note       Turn a note-on into a Program Change (note-off dropped)
            program
  velscale  factor     Scale note-on velocity by a factor (clamped 1-127)
  velset    number     Set a fixed note-on velocity (1-127)
  veladd    number     Add an offset to note-on velocity (clamped 1-127)
  velcurve  gamma      Apply a gamma curve to note-on velocity (1-127)
  ccmap     from to    Remap a Control Change controller number
  ccadd     number     Add an offset to a controller's value (clamped 0-127)
            value
  ccscale   number     Scale a controller's value by a factor (clamped 0-127)
            factor
  cccurve   number     Apply a gamma curve to a controller's value
            gamma
  pcmap     from to    Remap a Program Change number
  pcadd     number     Add an offset to Program Change number (clamped 0-127)
  pbadd     number     Add an offset to Pitch Bend (clamped 0-16383)
  pbscale   factor     Scale Pitch Bend around center by a factor (0-16383)
  pbset     number     Set a fixed Pitch Bend value (0-16383)
  cpadd     number     Add an offset to Channel Pressure (clamped 0-127)
  cpscale   factor     Scale Channel Pressure by a factor (clamped 0-127)
  cpset     number     Set a fixed Channel Pressure value (0-127)
  cpcurve   gamma      Apply a gamma curve to Channel Pressure
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
  convert   srctype    Convert a value between cc, cc14, rpn, nrpn, pb, cp & pc.
            [number]   Types cc, cc14, rpn and nrpn take a number selecting the
            dsttype    controller or parameter, pb, cp and pc do not, and the
            [number]   value is rescaled to the destination resolution
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
  --                   Read commands from standard input until it's closed
```

Alternatively, you can use the following long versions of the commands:
```
  input output virtual-in virtual-out decimal hexadecimal octave-middle-c
  note-numbers timestamp verbose monitor-source channel note-on note-off
  poly-pressure control-change control-change-14 program-change
  channel-pressure pitch-bend system-realtime continue active-sensing reset
  system-common system-exclusive system-exclusive-file time-code song-position
  song-select tune-request note-range velocity-range mpe-master mpe-member
  mpe-zone channel-map channel-set channel-add transpose note-map
  note-to-control-change control-change-to-note note-to-program-change
  velocity-scale velocity-set velocity-add velocity-curve control-change-map
  control-change-add control-change-scale control-change-curve
  program-change-map program-change-add pitch-bend-add pitch-bend-scale
  pitch-bend-set channel-pressure-add channel-pressure-scale
  channel-pressure-set channel-pressure-curve nrpn-add nrpn-scale nrpn-curve
  rpn-add rpn-scale rpn-curve javascript javascript-file mpe-mono mpe-expand
  mpe-split mpe-bend mpe-sensitivity
```

## Routes

A *route* connects one or more MIDI inputs to one or more MIDI outputs. Every input on a route is forwarded to every output on that route, so a route can split one input to several outputs, merge several inputs into one output, or both. Any filter and transform commands also belong to the route.

A route starts with an `in` (or `vin`) command. Further `in` commands keep adding inputs to the same route until an `out` (or `vout`) command is given; the next `in` after that begins a new route. So this splits, merges, and starts a second independent route:

```
routemidi in "Keyboard" out "Synth A" out "Synth B" \
          in "Pads" in "Drums" out "Sampler"
```

Filter and transform commands can be placed anywhere within a route's commands:

```
routemidi in "LinnStrument" ch 1 transp 12 out "Bidule 1" \
          in "Keystep" cc out "Bidule 1" out "Synth"
```

By default RouteMIDI works silently. Add the `mon` command to print each routed message (after filtering and transformation) for debugging, optionally with `ts` for timestamps and `nn` for note numbers instead of names.

## Filters

Filters select which messages are allowed to pass through a route. As soon as one or more positive filters are present, only the messages matching at least one of them are forwarded (everything else is dropped). With no filters, everything passes.

Prefix any filter with `not` to make it block instead: matching messages are dropped and everything else passes. Positive and negative filters can be combined: a message must then match at least one positive filter and none of the negative ones.

The `ch` command narrows a route to a single MIDI channel; combined with type filters it restricts them to that channel.

The number that selects which message a filter matches — for `ch`, `on`, `off`, `pp`, `cc`, `cc14` and `pc` — may be a single value or an inclusive range written as `lo..hi`. The range form also accepts note names. The `..` separator is used (rather than `-`) so it doesn't clash with note names like `C-2`.

```
routemidi in "Keyboard" cc 1..10 out "Synth"        # only CC 1 through 10
routemidi in "Keyboard" on C3..C5 out "Synth"       # only note-ons in a range
routemidi in "Keyboard" ch 1..4 out "Synth"         # only channels 1-4
```

The `noterange` and `velrange` filters pass notes within a note or velocity range, which (combined with the multi-route model) makes keyboard and velocity splits easy. Unlike `on lo..hi` (which matches only note-ons), `noterange` matches note-ons, note-offs and poly pressure together, and `velrange` always passes note-offs so a velocity split can't leave notes stuck.

The `cc14`, `nrpn` and `rpn` filters match the constituent Control Change messages of a 14-bit CC, an NRPN or an RPN. `cc14` without a number passes every 14-bit-capable controller (MSB 0-31 together with its LSB 32-63), or with a number just that MSB controller and its LSB. `nrpn` passes the NRPN traffic (CC 98, 99 plus the data-entry CC 6, 38) and `rpn` the RPN traffic (CC 100, 101 plus CC 6, 38).

```
routemidi in "Keyboard" note out "Synth"            # only note messages
routemidi in "Keyboard" not clock out "Synth"       # everything except clock
routemidi in "Keyboard" ch 1 cc 1 out "Synth"       # only CC 1 on channel 1
routemidi in "Knobs" rpn out "Synth"                # only RPN traffic
# key split: bottom half to a bass, top half to a lead
routemidi in "Keyboard" noterange C-2 B2 out "Bass" \
          in "Keyboard" noterange C3 G8 out "Lead"
```

## Transforms

Transforms modify the messages that pass the filters, and are applied in the order in which they appear in the route. A transform that would push a value out of range (for instance transposing a note past 0-127) drops that message.

```
routemidi in "Keyboard" transp 12 chset 2 out "Synth"   # octave up, on channel 2
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

A few transforms change a message from one type to another. `notecc` turns a specific note into a Control Change (the note-on velocity becomes the value, and the note-off sends 0); `ccnote` turns a Control Change into a note (a value of 64 or more triggers a note-on with that value as the velocity, below 64 a note-off); and `notepc` turns a note-on into a Program Change (the note-off is dropped). `ccnote` is aimed at switch- or pedal-style controllers; a continuously changing CC would retrigger the note. Since `notecc` uses the velocity as the value, put a `velset` in front of it for a fixed value.

```
routemidi in "Pad" notecc 60 64 out "Synth"             # a pad key holds the sustain CC
routemidi in "Foot" ccnote 64 C1 out "Drums"            # a sustain pedal plays a kick
routemidi in "Pads" notepc 36 0 notepc 37 1 out "Synth" # two pads select two patches
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

The `cc`, `cc14`, `rpn` and `nrpn` types are followed by a number selecting which controller or parameter. The `pb`, `cp` and `pc` types have a single value per channel, so they take no number.

```
routemidi in "Knobs"  convert nrpn 245 cc14 1   out "Synth"
routemidi in "Fader"  convert cc14 1  nrpn 245  out "Module"
routemidi in "Wheel"  convert pb      nrpn 1000 out "Synth"
routemidi in "After"  convert cp      cc 11     out "Synth"
routemidi in "Knob"   convert cc 7    pb        out "Synth"
```

Several conversions can be configured on one route; only the matching messages are converted, everything else passes through untouched.

Notable details:

* **Bit scaling** by default uses a **Min-Center-Max** method, which preserves minimum, center and maximum across resolutions and round-trips losslessly: widening a 7-bit value to 14-bit and narrowing it back yields the original. So a 7-bit value of 127 becomes the 14-bit maximum 16383, 64 becomes the center 8192, and 0 stays 0.
* For **RPNs whose parameter LSB is 0-31** (the classic absolute MIDI 1.0 RPNs such as pitch-bend sensitivity, tuning and the MPE Configuration Message), the **Zero-Extension with Rounding** method is used instead, for exact backward compatibility. Upscaling pads with zeros (7-bit 127 becomes 16256, not the full 16383) and downscaling rounds to the nearest value. NRPNs and all CCs always use Min-Center-Max.
* Conversions to `rpn`/`nrpn` append the **RPN/NRPN null** (CC 101/100 = 127 or CC 99/98 = 127) to deselect the parameter afterwards.
* When a route converts any `rpn` or `nrpn`, the converter takes over the whole RPN controller set (CC 6, 38, 98, 99, 100, 101) on that route to reassemble every (N)RPN: targeted parameters are converted and any other (N)RPN is regenerated and passed through. Avoid filtering out those CCs on a converting route.

### Transforming RPN and NRPN values

You can also modify an RPN or NRPN value in place, the same way `ccadd`/`ccscale`/`cccurve` modify a Control Change. `rpnadd`/`nrpnadd`, `rpnscale`/`nrpnscale` and `rpncurve`/`nrpncurve` each take the parameter number followed by the offset, factor or gamma, reassemble the (N)RPN from its constituent CCs, apply the change to the assembled value, and regenerate it. The value is clamped to its own resolution (14-bit when a data-entry LSB is present, otherwise 7-bit). Parameter *remapping* doesn't need a dedicated command — `convert nrpn 245 nrpn 1000` re-emits NRPN 245 as NRPN 1000.

```
routemidi in "Synth" nrpnscale 1000 0.5 out "Synth"   # halve NRPN 1000's value
routemidi in "Synth" rpnadd 0 -2048 out "Synth"       # lower RPN 0 (bend range)
routemidi in "Synth" nrpncurve 74 2.0 out "Synth"     # finer control low down
```

Like the conversions, these transforms take over the whole RPN controller set on the route, so avoid filtering out CC 6, 38 and 98-101 on the same route.

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
* **Pitch bend** is **summed in semitones** (so the per-note 48-semitone range and the 2-semitone zone range combine correctly), with the result expressed at the combined range — RouteMIDI emits an **RPN 0** on the target declaring that range so the synth reproduces it. Because pitch bend is channel-wide, only the most recently triggered (and still held) note's bend is applied, falling back to the previous held note on release.
* **Timbre (CC 74)** is combined with the **maximum** of the Manager and Member values, also last-note-wins.

```
routemidi in "Seaboard" mpemono lower 1 out "Mono Synth"
```

### Expanding a single channel to MPE

`mpexp <channel> <zone>` does the opposite: it voice-allocates the notes arriving on one channel across an MPE zone's member channels and emits the zone's MPE Configuration Message so the receiver is set up correctly. Each note is given a member channel, reusing a channel as late as possible — which matters for long releases — and sharing a channel with another note when the polyphony exceeds the member-channel count, rather than stealing an already sounding note. Zone-wide messages (pitch bend, channel pressure, CC) on the source channel are sent to the master channel. **Polyphonic aftertouch** is turned into **per-note channel pressure** on the note's member channel — the mirror image of what collapse does — so a keyboard with poly aftertouch drives each MPE note's pressure independently. Channel pressure is set to zero just before each Note On and Note Off. This "MPE-ifies" a regular keyboard so a downstream MPE synth gives each note its own channel.

```
routemidi in "Keyboard" mpexp 1 lower:15 out "MPE Synth"
```

### Splitting MPE across separate output ports

`mpesplit <zone> [channel]` fans an MPE zone out across the route's output ports, treating **each output port as one monophonic voice**. This connects a rack of ordinary mono synths to an MPE controller: a note-on is allocated to a free port (round-robin, stealing the oldest voice when every port is busy), and that note's expression follows it to the same port until it is released. Everything is rechanneled onto a single channel (channel 1 by default, or the optional `channel` argument) so each non-MPE device sees a plain monophonic stream. Because each port carries one voice, the Manager (zone-wide) and that note's Member (per-note) expression are combined per port exactly as in collapse — pitch bend summed in semitones (with an RPN 0 declaring the combined range on the port), channel pressure and CC 74 combined with the maximum. The MPE Configuration Message (RPN 6) is suppressed so the non-MPE devices aren't flooded with it (other RPNs still pass through).

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

`mpesens <zone> <semitones>` takes the complementary approach: instead of rewriting the bend values, it *declares* the member-channel Pitch Bend Sensitivity by injecting an **RPN 0** on the zone's member channels, for a synth that honours RPN 0. Use it when the synth can adopt the range rather than being stuck at one — then no rescaling is needed and the original values play correctly. RouteMIDI sends the declaration once before the first note and again after each MPE Configuration Message (which resets the receiver's sensitivity to its 48-semitone default).

```
routemidi in "Seaboard" mpesens lower:15 48 out "Synth"   # tell the synth members bend +/-48
```

Use `mpebend` for synths with a fixed bend range, or `mpesens` for synths you can configure — not both, since they would correct the same difference twice.

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

A split controller can run a **Lower** zone (master channel 1, members counting up) and an **Upper** zone (master channel 16, members counting down) at the same time, with non-overlapping member ranges — for example `lower:7` (channel 1 + channels 2-8) and `upper:7` (channel 16 + channels 9-15). Each zone is handled independently, so the simplest approach is one route per zone, isolating it with `mpezone`:

```
routemidi in "Seaboard" mpezone lower mpemono lower:7 1 out "Bass" \
          in "Seaboard" mpezone upper mpemono upper:7 2 out "Lead"
```

Both zones can also be processed in a single route — a Lower-zone operation and an Upper-zone operation keep separate per-zone state (each gets its own sensitivity defaults, voice allocation, and so on), so they don't interfere:

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

Add `panic` to a route to make it send an all-notes-off safety net (sustain off and all-notes-off on every channel) to that route's outputs whenever one of its inputs disconnects, and once more when RouteMIDI exits. This prevents stuck notes when a controller is unplugged mid-performance. It also fires when an **MPE zone is reconfigured** mid-stream — when an MPE Configuration Message changes a zone's member count (or turns the zone off), it sends all-notes-off and reset-all-controllers downstream, since non-MPE devices won't reset themselves.

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
the bit-scaling math, every native transform, the filter logic (including range
selectors), the CC/CC14/RPN/NRPN conversions and value transforms, the MPE zone
routing, the text-MIDI codec, and command-stream parsing.

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

## SendMIDI and ReceiveMIDI

RouteMIDI is designed to work alongside its sibling command-line tools:

* SendMIDI sends MIDI messages from the command line: https://github.com/gbevin/SendMIDI
* ReceiveMIDI receives and monitors MIDI messages from the command line: https://github.com/gbevin/ReceiveMIDI
