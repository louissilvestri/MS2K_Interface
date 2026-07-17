<img src="resources/icon.png" width="104" align="right" alt="MS2K Interface icon"/>

# MS2K Interface — Korg MS2000 Editor / Librarian

A native editor for the **Korg MS2000 (gen‑1)** that *de‑multiplexes* the hardware. The physical
synth shares 5 LCD knobs and 2 EDIT‑SELECT dials across ~16 parameter pages and cycles waveforms/
types with buttons; this gives (almost) **every parameter its own permanent, labeled control** and
drives the real synth over MIDI — as a **standalone app** and a **VST3 plugin** for DAW automation.

![The MS2K Interface editor](docs/screenshots/editor-full.png)

## Features
- **De‑multiplexed editor** — every hardware section laid out with dedicated knobs and
  panel‑style red‑LED selectors (no LCD‑page hunting), in a signal‑flow layout you can rearrange
  with a drag‑and‑resize **UI Edit** mode.
- **Bank librarian** — pull all 128 patches from the synth, browse them, click to load/audition,
  and save/load `.syx` banks.
- **Mod‑Sequencer** — the MS2000's 3‑lane × 16‑step motion sequencer, fully editable (the feature
  this project was built for).
- **VST3 automation** — every synth parameter *and* the mod‑sequencer is an automatable plugin
  parameter; a **Listen to synth** toggle gives live, bidirectional CC/NRPN sync.
- **Faithful & verified** — byte‑exact SysEx (Korg 7→8 packing) plus the full CC/NRPN map, round‑
  trip tested against real hardware: **77/77** parameters and **60/60** mod‑seq values.

![The Mod-Sequencer panel](docs/screenshots/mod-sequencer.png)

### Two builds
- **Standalone app** — talks to the synth directly (via RtMidi); the full editor, librarian, and
  sequencer in one window.
- **VST3 plugin** (Reaper / any DAW) — a MIDI‑effect that emits on the host's output bus, so your
  edits and automation are recorded and played back by the DAW.

## Install (Windows)
Download the self‑contained installers from the
[**Releases**](https://github.com/louissilvestri/MS2K_Interface/releases) page and double‑click —
no build required:

- **`MS2K_Interface-x.y.z-Setup.exe`** — the standalone editor.
- **`MS2K-Editor-VST3-x.y.z-Setup.exe`** — the VST3 plugin.

The MS2000 gen‑1 has DIN MIDI only — connect through a USB‑MIDI interface. Step‑by‑step setup is
in the wiki.

## Documentation
Full guides and reference live in the **[Wiki »](https://github.com/louissilvestri/MS2K_Interface/wiki)**:

[Installation](https://github.com/louissilvestri/MS2K_Interface/wiki/Installation) ·
[Standalone App Guide](https://github.com/louissilvestri/MS2K_Interface/wiki/Standalone-App-Guide) ·
[VST3 Plugin in Reaper](https://github.com/louissilvestri/MS2K_Interface/wiki/VST3-Plugin-in-Reaper) ·
[Mod Sequencer](https://github.com/louissilvestri/MS2K_Interface/wiki/Mod-Sequencer) ·
[MIDI Implementation](https://github.com/louissilvestri/MS2K_Interface/wiki/MIDI-Implementation) ·
[Troubleshooting](https://github.com/louissilvestri/MS2K_Interface/wiki/Troubleshooting) ·
[Building from Source](https://github.com/louissilvestri/MS2K_Interface/wiki/Building-from-Source)

> **Note (VST3 in Reaper):** the plugin's in‑host *Get All / Get Current* can't **receive** SysEx,
> because Reaper doesn't pass real‑time input SysEx to a track's FX chain. Everything else works;
> use the simple `.syx` workaround (standalone **Save .syx** → plugin **Load .syx**). See
> [Troubleshooting](https://github.com/louissilvestri/MS2K_Interface/wiki/Troubleshooting).

## Build
Builds with CMake + JUCE (fetched automatically). Full instructions:
[Building from Source](https://github.com/louissilvestri/MS2K_Interface/wiki/Building-from-Source).
```sh
cmake -B build -G Ninja .
cmake --build build --target MS2K_Interface     # standalone app
cmake --build build --target MS2K_Plugin_VST3   # VST3 plugin
```

## Acknowledgments
This project stands on prior MS2000 reverse‑engineering and tooling:

- **[mlazarev/midi](https://github.com/mlazarev/midi)** — a comprehensive MS2000 MIDI/SysEx
  resource; its documentation was used to cross‑check the SysEx layout and **confirm the
  Mod‑Sequencer byte offsets** (then re‑verified against a real bank dump).
- **[ReMS2000](https://github.com/inteyes/ReMS2000)** (inteyes) — the open‑source Ctrlr‑based
  editor that inspired the data‑driven, single‑table parameter model used here.
- The **Korg MS2000/2000R MIDI Implementation** (Korg Inc.) — the authoritative spec the byte
  maps are transcribed from.
This project also **builds on third‑party code**, each under its own license:
- **[RtMidi](https://github.com/thestk/rtmidi)** (Gary P. Scavone) — **vendored** (copied) into
  `Source/midi/rtmidi/`, under its MIT‑style license; provides the direct hardware MIDI I/O.
- **[JUCE](https://juce.com)** — the GUI/audio framework the app is built on (fetched at build
  time; under the JUCE license).

The **first‑party source** in this repo — the parameter model, SysEx codec, MIDI‑message
builders, UI, standalone app, and VST3 plugin — is original work. RtMidi and JUCE are
third‑party components under their own licenses (see **License** below). Only the
**mlazarev/midi**, **ReMS2000**, and **Korg MIDI Implementation** entries above are
references/spec sources rather than copied code.

## License
This project's source is released under the **MIT License** — see [LICENSE](LICENSE).
Dependencies carry their own terms: **JUCE** (under the JUCE license) and **RtMidi** (MIT,
vendored in `Source/midi/rtmidi/`). MS2000 and Korg are trademarks of Korg Inc.; this is an
unofficial, independent editor with no affiliation.
