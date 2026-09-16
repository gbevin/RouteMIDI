# Vendored JUCE patches

The JUCE modules vendored under `JuceLibraryCode/modules` are based on JUCE
9.0.2 with one local patch, kept as the patch file in this folder:

- **`juce_alsa-midi-1-bytestream.patch`**: JUCE 9 registers every ALSA client as
  MIDI 2.0, which makes the sequencer carry UMP. Sending then combines the
  controllers of an RPN or NRPN into one MIDI 2.0 message, and receiving decodes
  that message to nothing, so RPN and NRPN traffic disappears between MIDI 1.0
  applications on Linux. The fix registers as a MIDI 1.0 client and always sends
  a bytestream.

The same patch is applied in SendMIDI and ReceiveMIDI, which carry two further
patches to `juce_midi_ci` for their MPE Profile negotiation.

## Re-applying

`Projucer --resave` re-copies the modules from the external JUCE and **silently
overwrites this patch**. After a resave, restore the vendored code before
committing:

```
git checkout -- JuceLibraryCode/modules JuceLibraryCode/AppConfig.h
```

To apply the patch onto a fresh stock module copy instead (for example after
deliberately updating the vendored JUCE), from the repository root:

```
git apply Patches/juce_alsa-midi-1-bytestream.patch
```

Note that JUCE ships these sources with CRLF line endings while the vendored
copies are LF; if a fresh copy still has CRLF, apply with
`git apply --ignore-whitespace` (or normalize to LF first) and verify with
`git apply --reverse --check <patch>`, which succeeds when a tree contains
exactly what a patch describes.
