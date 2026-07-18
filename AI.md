# Driving RouteMIDI from AI tools and scripts

> **Experimental and fast-moving.** RouteMIDI's schema and MCP integration
> target the Model Context Protocol, a young and rapidly changing specification.
> The tool names, their arguments, the JSON shapes and the protocol version are
> all likely to change between releases, and this surface is not covered by the
> project's stability expectations the way the command-line interface is. If you
> build on it, pin a RouteMIDI version and expect to revisit it. Feedback is
> welcome.

RouteMIDI can be driven programmatically in three ways, from lightest to deepest:

1. **Generate command lines** and run them like any shell command.
2. **Verify without hardware** by piping text MIDI through the binary.
3. **Drive live sessions over MCP** with the built-in server: start routes,
   inspect them, and edit their commands while MIDI is flowing.

Two machine-readable surfaces support this: `routemidi --schema json` prints the
full command metadata, and `routemidi --mcp` runs a stdio [Model Context
Protocol](https://modelcontextprotocol.io) server. A condensed, always-current
copy of the generation contract below also lives in [llms.txt](llms.txt) at the
repository root.

## Security and trust model

The MCP server communicates only over the stdin and stdout of the process the
MCP client launches; there is no network listener and no authentication, which
is normal for a stdio MCP server. The transport is therefore private to the
local user who configured the client. **Do not expose `--mcp` over a socket or
network** (for example by wrapping it with a relay); it assumes a trusted local
client.

The tool calls themselves, however, are treated as untrusted. An AI client can
be steered by prompt injection from the content it is processing, so the server
grants capability by an explicit **allow-list**: routing (`in`, `out`, `vin`,
`vout`, `panic`), the number settings (`dec`, `hex`, `omc`) and the message
filters, transforms, MPE operations and conversions. Everything else is
rejected with a reason, and a rejected call is **atomic**: no route is started,
and session settings and the process exit code are rolled back.

In particular, the commands that reach outside MIDI are **not** available over
MCP:

* `js` and `jsf` (JavaScript) are blocked. The scripting engine can run shell
  commands (`Util.command`), open network connections (`OSC`) and write to
  standard output, so it is a code-execution and exfiltration surface that does
  not belong on an autonomously-driven endpoint. Use JavaScript from the
  command line instead, where the person running RouteMIDI is in control.
* `syf` (SysEx-to-file capture) and `file` (program files) are blocked because
  they read or write the local file system.
* `mon`, `src`, `list` and text (`-`) ports are blocked because they would write
  to standard output, which the JSON-RPC framing owns.

The allow-list is deny-by-default: a command added to RouteMIDI in the future is
unavailable over MCP until it is deliberately reviewed and permitted.

The `inject_midi` tool stays inside this model: its input is parsed strictly as
text MIDI data (never as commands, scripts or file paths) and the resulting
messages can only reach output ports that a route already opened, the same
ports live routing sends to. A line that is not a text MIDI message rejects the
whole call atomically.

## The MCP server

RouteMIDI includes a stdio MCP server in the same executable. MCP clients do
not discover arbitrary running command-line processes; configure the client to
launch RouteMIDI with `--mcp`. Once launched, the client will discover the
server through the MCP `initialize` and `tools/list` messages.

Example MCP server configuration:

```json
{
  "mcpServers": {
    "routemidi": {
      "command": "/path/to/routemidi",
      "args": ["--mcp"]
    }
  }
}
```

### Connecting a client without editing JSON

RouteMIDI can write this configuration for you, so `command` points at the
absolute path of the running binary (GUI clients such as Claude Desktop don't
inherit the shell `PATH`, so a bare name wouldn't resolve for them):

- `routemidi --print-mcp-config [client]` prints the entry in a client's own
  format: the JSON block above by default (and for `claude-desktop`, `cursor` and
  `claude-code`), or the TOML table for `codex`. Pipe or paste it wherever the
  client wants it.
- `routemidi --install-mcp <client>` sets a client up. For a JSON client
  (`claude-desktop`, `cursor`) it merges the entry into the configuration file in
  place, creating it if needed, preserving any servers already there, and refusing
  to touch a file it can't parse; it reports the path so you can review it, then
  restart the client. For a client that ships its own command (`codex`,
  `claude-code`) it prints that command to run instead of editing an unfamiliar
  file — `codex mcp add routemidi -- routemidi --mcp` and
  `claude mcp add routemidi -- routemidi --mcp` respectively. RouteMIDI prints
  Codex's TOML but doesn't rewrite it, since its config lives as TOML at
  `~/.codex/config.toml`.
- For **Claude Desktop**, a per-platform MCP Bundle (`.mcpb`) is attached to each
  GitHub release; the user installs RouteMIDI by double-clicking it, with no
  configuration file to edit at all. The `extension/` folder holds the manifest
  and `Scripts/build-mcpb.sh` builds a bundle from a local binary.

The server exposes these tools:

- `get_schema`: returns the same machine-readable command metadata as
  `routemidi --schema json`.
- `list_midi_ports`: returns the current MIDI input and output port names.
- `start_route`: starts live routing from explicit RouteMIDI command tokens and
  returns the new route's id, port connection state and staged commands.
- `list_routes`: returns every active route with its id, port connection state
  and the commands of each processing stage (filters, transforms, mpe,
  conversions, split) with their per-stage indexes.
- `inject_midi`: injects text MIDI messages into a running route as if they
  arrived on one of its inputs. They run through the route's full pipeline and
  go to its outputs, and the result echoes what the route emitted, each entry
  naming the output ports it was sent to, so a route can be exercised and
  verified in place: audition a note, send a program change, or check what a
  transform edit does to a specific message. Messages use the shared
  SendMIDI/ReceiveMIDI text format (one per array element) and send
  immediately, in order, without scheduling; a call takes at most 128 messages
  and live MIDI interleaves with the batch rather than waiting for it. The
  optional `input` index selects which input's per-input state (latch, pedals,
  MPE, conversions) the messages run through. When an injected message
  reconfigures an MPE zone on a `panic` route, the safety net's all-notes-off
  and reset-all-controllers go to the outputs outside the echo, reported by
  `zoneReset: true` in the result. Lines parse with the same permissive rules
  as the CLI text streams: a line that yields no message rejects the whole
  call, but unknown trailing tokens are ignored and a missing trailing value
  defaults to 0, so `note-on 60` without a velocity becomes an effective
  note-off. Generate every operand and check the echoed result against the
  intent.
- `read_route`: returns the MIDI messages that have recently flowed through a
  route, so a route can be listened to (the receiving counterpart to
  `inject_midi`). Each entry is a text MIDI message with the input port it
  arrived on and a per-route `seq`. To watch a route, poll repeatedly, passing
  the returned `cursor` as `after` to get only new messages; omit `after` to get
  everything currently buffered. The buffer holds the most recent 1024 messages,
  and `dropped` reports how many were missed when polling too slowly. Only
  messages that pass the route's filters and processing are captured (both live
  traffic and injected messages), exactly what the monitor would show.
- `add_commands`: appends processing commands (filters, transforms, MPE
  operations, conversions) to a running route; ports cannot be changed.
- `replace_command`: replaces one command of a running route in place, keeping
  its position in the stage, for example swapping `scale C major` for
  `scale D minor` or retuning a `cccurve` mid-performance.
- `remove_command`: removes one command from a running route by stage and index.
- `panic_route`: sends pedal-off and all-notes-off to a route's outputs and
  clears its latch, mono and pedal state; use it after editing stateful
  commands to release anything left sounding.
- `stop_route`: stops and removes a route, sending all-notes-off to its outputs
  first.

Route ids are stable: they are assigned when a route is created and survive the
removal of other routes.

Normal routing invocations such as `routemidi in Keyboard out Synth` do not
expose MCP. Use `--mcp` for AI-controlled routing sessions.

Troubleshooting: if the MCP client logs "invalid JSON" errors that quote the
version banner or the help text (for example `routemidi v...` or fragments of
the usage lines), the server was launched without the `--mcp` argument and
printed its usage to standard output instead of speaking MCP. Make sure the
configuration passes `--mcp` in `args`, as in the example above.

## Generation contract

RouteMIDI offers AI tools three integration surfaces, from lightest to deepest:

1. **Generate command lines** from the contract below and run them like any shell command.
2. **Verify without hardware** by piping text MIDI through the binary (see "Verifying generated commands" below).
3. **Drive live sessions over MCP** through the [built-in MCP server](#the-mcp-server): start routes, inspect them, and edit their commands while MIDI is flowing.

The three share one vocabulary: an MCP `start_route` or `add_commands` call takes exactly the command tokens described here, minus the `routemidi` executable name. The same metadata is available machine-readably from `routemidi --schema json` (or the `get_schema` MCP tool), and a condensed copy of this contract lives in [llms.txt](llms.txt) at the repository root.

This section condenses the rules an AI tool needs to generate correct RouteMIDI commands. Everything here is described in more detail elsewhere in this README; when generating commands, treat this section as the contract.

**Invocation shape**

```
routemidi [settings] in|vin <port> [commands ...] out|vout <port> [out ...] [in ... out ...]
```

* A route is one or more `in`/`vin` inputs, the route's filter/transform/MPE/convert commands (their position among each other is free, see the fixed stage order under [Routes](README.md#routes)), and one or more `out`/`vout` outputs. The first `in` after an `out` starts a new, independent route. Every input of a route is forwarded to every output of that route.
* Port names match case-insensitively by substring, so `in "Linn"` finds "LinnStrument MIDI". Ports that share the same name are listed with a number by `list` and `list_midi_ports`, like `KeyStep (2)`, and that numbered name selects that specific port. `-` as a port name is a text stream: `in -` reads text MIDI from standard input, `out -` writes it to standard output.
* Settings (`dec`, `hex`, `omc`, `mon`, `ts`, `nn`, `src`, `panic`) can appear anywhere.

**Value conventions**

* Numbers are decimal by default; `hex` switches the default, and a `H` or `M` suffix forces a single number to be hexadecimal or decimal. When mixing bases, prefer suffixes.
* Notes are numbers 0-127 or names from `C-2` to `G8` (middle C = `C3` = 60 by default; `omc` changes the octave numbering). Sharps use `#`, flats use `b`.
* Channels are 1-16. The selector of `ch`, `on`, `off`, `pp`, `cc`, `cc14` and `pc` accepts a single value or an inclusive `lo..hi` range (note names allowed).
* MPE zones are written `lower` or `upper` with an optional member count, e.g. `lower:7`.

**Semantics to respect**

* No filters means everything passes. One or more positive filters form a whitelist (a message must match at least one); `not <filter>` blocks matching messages and negates only the next filter; both combine.
* Transforms that would push a note out of 0-127 drop the message; value transforms clamp instead.
* `latch`, `mono`, the pedals (`sustain`, `sost`), the MPE operations and the conversions keep running per-input state (held notes, RPN selections, 14-bit pairings), so each input should carry one device's continuous stream.
* The stages always run filters, then transforms, then MPE operations, then conversions, regardless of command order (see [Routes](README.md#routes)).
* Every route must start with `in` or `vin`; filter/transform commands before the first `in` are ignored with a warning.

**Argument parsing traps**

* **Optional** arguments (the `on`/`off`/`pp`/`cc`/`cc14`/`nrpn`/`rpn`/`pc` selectors, `latch` mode, `mono` priority, `vin`/`vout` names, `mpesplit` channel) end at the first token that spells a RouteMIDI command name: `on clock` is a note-on filter with *no* note plus a `clock` filter, not "note-on for a note called clock". Never place a value or port name that spells a command word in an optional position.
* **Fixed** arguments are the opposite: they are taken literally even when they spell a command name, so `in cc` names a port "cc" and `convert`'s type words parse fine.
* `convert` is variable-length (`srctype [num] dsttype [num]`): `cc`, `cc14`, `rpn` and `nrpn` need a number, `pb`, `cp` and `pc` take none, and `pp` needs a note as a destination but may omit it as a source (meaning "any note").

**Safe live use**

* Add `panic` to live-performance routes: it sends all-notes-off when an input disconnects, when RouteMIDI exits, and when an MPE zone is reconfigured, preventing stuck notes.
* Positive filters are whitelists: `note` alone silently drops clock, transport and everything else. If the destination needs clock, include `sr` or `clock` in the whitelist; and when merging several inputs, keep clock from only one of them (route the others through `not sr`) or the tempo messages double up.
* `js` scripts run synchronously on the routing path for every message: keep them short, never call `Util.sleep` in a live route, and remember that `MIDI.send*()` emits immediately and script global state persists across messages.

**Verifying generated commands without MIDI hardware**

Any pipeline can be tested end-to-end by piping text MIDI through the binary. This example transposes an octave up and prints the transformed stream:

```
$ printf 'channel 1 note-on 60 100\nchannel 1 note-off 60 0\n' | routemidi in - transp 12 out -
channel  1   note-on           C4 100
channel  1   note-off          C4   0
```

One message per line: an optional `channel <1-16>` prefix, then one of `note-on <note> <velocity>`, `note-off <note> <velocity>`, `poly-pressure <note> <value>`, `control-change <cc> <value>`, `program-change <n>`, `channel-pressure <n>`, `pitch-bend <0-16383>`, `midi-clock`, `start`, `stop`, `continue`, `active-sensing`, `reset`, `song-position <n>`, `song-select <n>`, `tune-request`, `time-code <t> <v>`, `system-exclusive <bytes>` (with inline `hex`/`dec` toggles). The same format is read by SendMIDI and written by ReceiveMIDI.

Because all three tools speak this format, they chain into live pipelines: ReceiveMIDI captures a port as text, RouteMIDI processes it in the middle, and SendMIDI emits it on another port. This transposes everything a keyboard plays before it reaches the synth, without starting a route on hardware ports:

```
$ receivemidi dev "Keyboard" | routemidi in - transp 12 out - | sendmidi dev "Synth" --
```

The same text also records and replays: redirect ReceiveMIDI's output (with `ts` for timestamps) to a file, and SendMIDI's `file` command plays it back with the original timing.

**Working over MCP**

* The tool list, client configuration and troubleshooting live in the [MCP server section](#the-mcp-server). Everything below assumes the server was launched with `--mcp`.
* `start_route` and `add_commands` take the contract's command tokens as a JSON array, one token per element: `["in", "Keyboard", "transp", "12", "out", "Synth"]`. Quoting is not needed; a port name with spaces is simply one array element.
* MCP mode allows the routing commands (`in`, `out`, `vin`, `vout`, `panic`), the number settings (`dec`, `hex`, `omc`) and every processing command; anything else is rejected with a reason. In particular `-` ports and the monitoring commands are refused because MCP owns standard output for the protocol (use the `list_midi_ports` tool instead of `list`), `syf` because it captures to the local file system and could not be rolled back when a call fails, and the scripting commands (`js`, `jsf`) because they can run code and reach the shell, the network and local files. The check is per command, so a port merely *named* like a command is fine.
* A good workflow is to verify tokens first with a one-shot text pipe (previous section), start the identical tokens live with `start_route`, and then exercise the running route with `inject_midi`, checking the echoed result against the expectation. Tool calls are atomic: an invalid command (a bad MPE zone, a command before the first `in`) rejects the whole call, nothing is started, and session settings such as `hex` or `omc` that appeared in the rejected call are rolled back; an `inject_midi` call with a line that is not a text MIDI message injects nothing.
* Routes have stable ids. `list_routes` reports each route's ports (with their connection state) and the commands of every stage with per-stage indexes; a route whose port is not connected yet keeps retrying until the port appears.
* The `stage` field that `get_schema` reports for each command is the same `stage` argument the editing tools take: `filters`, `transforms`, `mpe`, `conversions` or `split`. Note two placements that differ from the help text's grouping: the 14-bit CC and RPN/NRPN value transforms (`cc14add`, `rpnadd`, `nrpnscale`, ...) edit in `conversions`, and `mpesplit` in `split`.
* `replace_command` swaps a command in place and keeps its position, which is the right tool for musical live edits: replacing `scale C major` with `scale D minor`, or retuning a `velcurve` or `cccurve` between songs.
* After removing or replacing stateful commands (`latch`, `mono`, the pedals, the MPE operations), call `panic_route` to release anything left sounding.

**Recipes**

Every recipe below also works as an MCP `start_route` payload: drop the `routemidi` executable name and pass the remaining tokens as the `commands` array.

| Intent | Command line |
| --- | --- |
| Forward a controller to a synth | `routemidi in "Keyboard" out "Synth"` |
| Merge two controllers | `routemidi in "A" in "B" out "Synth"` |
| Split one controller to two synths | `routemidi in "Keyboard" out "A" out "B"` |
| Key split at C3 | `routemidi in "K" noterange C-2 B2 out "Bass" in "K" noterange C3 G8 out "Lead"` |
| Velocity split | `routemidi in "K" velrange 1 63 out "Pad" in "K" velrange 64 127 out "Lead"` |
| Transpose an octave up | `routemidi in "K" transp 12 out "Synth"` |
| Keep a performance in C minor | `routemidi in "K" scale C minor out "Synth"` |
| Diatonic triads in a key | `routemidi in "K" chord 4 7 scale C major out "Synth"` |
| Drop clock and active sensing | `routemidi in "Seq" not clock not as out "Synth"` |
| Move channel 10 onto channel 1 | `routemidi in "Multi" chmap 10 1 out "Synth"` |
| Hold notes after release | `routemidi in "K" latch out "Synth"` |
| Sustain pedal for a synth that ignores CC 64 | `routemidi in "K" sustain out "Sampler"` |
| Calibrate an expression pedal's travel | `routemidi in "Pedal" ccrescale 11 20 108 0 127 out "Synth"` |
| Drive a mono synth (low-note priority) | `routemidi in "K" mono low out "Synth"` |
| MPE controller to a non-MPE synth | `routemidi in "Seaboard" mpemono lower 1 out "Mono Synth"` |
| Regular keyboard to an MPE synth | `routemidi in "K" mpexp 1 lower:15 out "MPE Synth"` |
| MPE zone across several mono synths | `routemidi in "Seaboard" mpesplit lower out "M1" out "M2" out "M3"` |
| NRPN to 14-bit CC | `routemidi in "Knobs" convert nrpn 245 cc14 1 out "Synth"` |
| CC 7 to pitch bend | `routemidi in "Fader" convert cc 7 pb out "Synth"` |
| Poly pressure to channel pressure | `routemidi in "MPE" convert pp cp out "Mono Synth"` |
| Soften poly aftertouch response | `routemidi in "Keyboard" ppcurve 2.0 out "Synth"` |
| Custom per-message logic | `routemidi in "K" js "if (MIDI.isNoteOn()) MIDI.setVelocity(100);" out "Synth"` |
| Bridge apps without hardware | `routemidi vin "RouteMIDI In" vout "RouteMIDI Out"` (Linux/macOS) |

For per-message scripting, the full JavaScript API (message accessors, `MIDI.send*`, `Util`, OSC output, persistent state across messages) is documented in [JAVASCRIPT.md](JAVASCRIPT.md).
