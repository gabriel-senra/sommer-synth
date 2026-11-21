# 001 — One shared `core/` folder instead of a copy per target

Date: 2026-11-21

## Problem

The chord engine (`chords.c/.h`: scale degrees, voicings, chord names) is pure C
with no platform calls, so every target can use it unchanged. But each ESP32
sketch carried its own copy, as did the desktop app: **14 byte-identical copies**.
A fix to one copy would silently not reach the others, and the Daisy port was
about to add a 15th.

The root cause is how Arduino builds work. A sketch compiles only the files in
its own folder (plus installed libraries), so copying the file in was the
easiest way to make a sketch build.

## Options

1. **Keep copying.** Zero setup. Every change has to be repeated by hand, and the
   copies drift apart.
2. **Symlink `chords.c` into each sketch folder.** One source of truth, but
   symlinks are fragile across git, the Arduino IDE (which copies sketches to a
   temporary folder) and Windows.
3. **Treat `core/` as an Arduino library.** `arduino-cli compile --library ../../core`
   adds the folder to the include and source paths for that build only. Nothing
   is installed globally, and the sketch source doesn't change (`#include "chords.h"`).
   The desktop Makefile compiles `../core/*.c` the same way.

## Choice

Option 3: a top-level `core/` folder, linked by every target.

## Why

- One copy of the music logic, which is the point of the portable-core rule in CLAUDE.md.
- It works with the build command we already use (arduino-cli), so there's no
  new tooling.
- It scales: as arp, transport and patches move out of `chord-piano-v2/`, they
  land in `core/` and every target gets them for free.

## Trade-offs

- The compile command needs the extra `--library ../../core` flag. If it's
  missing, the build fails loudly with "chords.h: No such file", which is
  better than silently building a stale copy.
- Compiling from the Arduino IDE GUI needs `core/` installed as a library. We
  already avoid the IDE for builds (it compiles stale buffers).

## Numbers

- Duplicate files removed: 13 copies × 2 files (`chords.c` 103 lines + `chords.h` 33 lines).
- ESP32 stage 15 after the switch: flash 671,380 B (51%), globals 26,228 B (8%).
  That is 128 B less RAM than before, because helpers only used inside
  `chords.c` (`OVR_NAMES`, `resolve_ovr`, …) became `static`.
