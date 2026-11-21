# sommer-synth

A hardware chord-synth groovebox. It's prototyped as a desktop app (C + SDL2 + the AMY synth engine), runs as working firmware on a classic ESP32, and is being **ported to the Daisy Seed next**.

## Layout

```
core/                  Portable C99 shared by every target (so far: chords.c/.h).
chord-piano-v2/        Desktop simulator (macOS, SDL2 + AMY via miniaudio). The reference implementation.
firmware/esp32/        ESP32 Arduino sketches: stage15_surface (current) + bench test sketches.
                       Stages 1-14 live in git under the `esp32-stages-archive` tag.
firmware/daisy/        Daisy Seed port (libDaisy + AMY). Starter only; port is the next milestone.
third_party/amy/       Vendored AMY engine source (shared by desktop and Daisy builds).
third_party/amy-arduino-examples/   AMY reference sketches (ESP32 I2S, MIDI, etc.).
third_party/stb/       stb_easy_font (desktop UI text).
docs/diagrams/         Mermaid diagrams of every system (*.md). Keep in sync with the code.
docs/decisions/        Decision records (NNN-title.md), one per real design choice.
```

## Build & run

- Desktop: `cd chord-piano-v2 && make && ./chord_piano_v2` (AMY compiles into `build/amy/`; `make clean` wipes it).
- ESP32: compile/upload with the arduino-cli bundled in Arduino IDE, never via the IDE buffer (it compiles stale copies):
  ```
  CLI="/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli"
  cd firmware/esp32 && "$CLI" compile --fqbn esp32:esp32:esp32 --library ../../core esp32_stageNN_name
  pkill -f serial-monitor; "$CLI" upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 esp32_stageNN_name
  ```
  `--library ../../core` is required: the sketch links the shared `core/` instead of carrying its own copy (see docs/decisions/001).
- `arduino-cli` is on PATH (symlink to the IDE's bundled copy), so the `$CLI` above can just be `arduino-cli`.
- Daisy: libDaisy and DaisySP are git submodules in `third_party/`. After a fresh clone: `git submodule update --init`, then `make` in each of `third_party/libDaisy` and `third_party/DaisySP` once. Firmware: `cd firmware/daisy && make`. Toolchain: Daisy Toolchain 0.2.0 (arm-none-eabi-gcc 10.3), `dfu-util`.
  - The app is `APP_TYPE=BOOT_SRAM`, so it needs the Daisy bootloader. **Once per board:** hold BOOT, tap RESET (system DFU), `make program-boot`. **Each flash:** tap RESET (the LED pulses for about 2.5 s while the bootloader waits), then `make program-dfu`.
  - AMY's Daisy support is stale. `firmware/daisy/amy_daisy_platform.c` supplies the missing platform hooks, and the build uses `-DAMY_NO_MINIAUDIO` and `config.audio = AMY_AUDIO_IS_NONE`. Don't patch `third_party/amy`.
  - The Daisy build compiles a **trimmed AMY** from `build/amy_src` (overlay made by `amy_trim/sync.sh`): no drum samples, no piano, Juno bank only. Edit `KEEP` in `amy_trim/make_patches.py` to change which patches ship.

## Hardware budgets (hard limits, design to these)

- **ESP32 (classic WROOM-32):** 6 voices / 32 oscillators max. 8/48 crashes from RAM exhaustion. A patch that doesn't fit fails *silently* (dead key). 9th chord (5 notes) + bass = exactly 6 voices.
- **Daisy Seed (STM32H750, 480 MHz M7, 64 MB SDRAM):** voice budget not yet measured. Measure it before designing patches for it, and record the number here. Runs at 48 kHz with 128-sample blocks (AMY_DAISY). Memory: code + constants run from the 480 KB SRAM (BOOT_SRAM). With the trimmed AMY it's **49.5% (243 KB)**; stock AMY was 94%. The heap (AMY voices) is a separate 256 KB in RAM_D2. See `docs/decisions/001`.

## Architecture rules (modularity)

The goal is a **portable core** that every platform shares unchanged:

- `core` = music + engine logic in plain C99: chords, arp, transport, state, patches, synth control. **No SDL, Arduino, libDaisy, or OS headers.** Time, input, display, and audio come in through small platform interfaces (e.g. `platform_millis()`), not direct calls.
- `platform/<sdl|esp32|daisy>` = adapters that implement those interfaces plus the main loop.
- `ui` = rendering, per platform (SDL window, SSD1306 OLED, ...).
- New features go in as a new `.h/.c` module in the right layer. `main.c` only wires things together.
- Patches are **data** (structs/tables), not code, so one patch definition plays on every target and can be checked against the voice/osc budget.

Known violations to fix during the core/platform split: `arp.c` and `transport.c` call `SDL_GetTicks()`, `arp.h`/`transport.h` include `<SDL.h>`, and `state.h`'s `ChordKey` uses `SDL_Keycode`. Only `chords.c` has moved into `core/` so far; arp, transport, state and fx still live in `chord-piano-v2/`.

## How we work

- Gabriel makes the design calls. For any real choice, present 2–3 options with trade-offs and a recommendation, then wait.
- Every real decision gets a record in `docs/decisions/`: problem, options, choice, why, trade-offs, measured numbers. Written so Gabriel can explain it himself in an interview.
- When a system's structure changes, update its Mermaid diagram in `docs/diagrams/` in the same change.
- Explain from fundamentals up (DSP or embedded concept → how our code does it → why it fits the whole system).

## AMY gotchas

- Master volume is 0..10, not 0..1.
- On ESP32, `amy_start()` runs its own FreeRTOS render + I2S tasks, so `loop()` stays free for input.
- C sources included from `.ino`/`.cpp` need `extern "C" { }`.
