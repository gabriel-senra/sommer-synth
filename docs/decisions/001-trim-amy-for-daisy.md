# 001: Trim AMY's built-in sound data for the Daisy build

**Date:** 2026-11-21 · **Status:** Done · **Decided by:** Gabriel

## Problem
The Daisy Seed's STM32H750 has only **128 KB of internal flash**, too small for AMY's ~460 KB build. So the app is built as `BOOT_SRAM`: a bootloader copies the whole program into the **480 KB internal SRAM** at boot and runs it from there. Stock AMY alone filled **94%** of that SRAM, leaving about 18 KB for our own code (chord engine, arp, transport, UI). We would have run out of room as soon as we ported the app.

Runtime memory was not the issue. The heap, where AMY creates voices and oscillators, lives in a separate 256 KB region (`RAM_D2`) that was nearly empty.

## What was using the space
Measured with `arm-none-eabi-size` and `nm --size-sort`:

| Contents | Size | Needed? |
|---|---|---|
| Built-in patches: Juno bank (34 KB) + DX7 bank (77 KB) | ~111 KB | We're building our own patch format |
| Roland 808 drum samples | ~102 KB | No; it's a chord synth |
| Piano harmonic tables | ~80 KB | No |
| AMY engine + libDaisy + HAL | ~150 KB | Yes |

## Options considered
1. **Trim AMY: leave out the data we don't use.** Code stays in fast SRAM. We lose the built-in sounds.
2. **Move the large tables to the 8 MB QSPI flash.** Keeps everything, but reads are slower and the linker setup is fiddly.
3. **Run the whole program from QSPI (`BOOT_QSPI`).** Plenty of room, but every instruction is fetched from slower external flash, which costs audio performance.

## Decision
**Option 1.** For now we keep the Juno bank (34 KB) so there are real sounds to test with until our own patch format exists.

## Why
- On a synth, the audio code's speed matters most. Options 2 and 3 trade speed for data we don't use.
- The built-in banks were always temporary: patches are moving to our own format (small, data-driven, checked against the voice budget).
- It's reversible. The keep-list is one line in `make_patches.py`, and option 2 is still available if we ever want sampled sounds back.

## How
An **overlay** build (`firmware/daisy/amy_trim/`): copy AMY's sources into `build/amy_src/`, swap in three smaller headers, and compile the copy. We don't edit the vendored AMY, so updating AMY never conflicts with our changes. Patch numbers never shift; a removed patch plays a 1-oscillator sine, so selecting one never gives silence or a crash.

## Result
SRAM: **462 KB (94%) → 243 KB (49.5%)**, so about 237 KB is now free for our code. The heap and voices were unaffected.

## Trade-offs we accepted
- No drums, no piano, and no DX7 sounds on the Daisy.
- The Daisy and the desktop app no longer have the same patch bank. The desktop still has all 258. Once our own patch format exists, both will use it.

## How to explain it in one breath
"The Daisy's chip has tiny internal flash, so the program runs from 480 KB of on-chip SRAM, and AMY's built-in sound libraries filled 94% of it. I measured what was in there, saw that most of it was drum samples, piano data and preset banks we'd never use, and stripped them at build time with an overlay, without forking the library. That halved the footprint and kept the audio code in the fastest memory."
