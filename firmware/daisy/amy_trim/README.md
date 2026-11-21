# amy_trim: a smaller AMY for the Daisy

The Daisy build compiles AMY from `build/amy_src/`, a copy of `third_party/amy/` with a few large tables replaced. `sync.sh` creates that copy, and the Makefile runs it on every `make`. **The vendored AMY is never edited**, so it can be updated freely.

| AMY file | Original | Trimmed | Effect |
|---|---|---|---|
| `pcm_tiny.h` | 11 Roland 808 drum samples (~102 KB) | 1 silent sample | PCM/drum waves are silent |
| `interp_partials.h` | Measured piano harmonics (~80 KB) | Empty table | Piano (patch 256) is silent |
| `patches.h` | 258 patches (~111 KB) | Only the patches in `KEEP` | Removed patches play a 1-osc sine |

**To change which patches ship:** edit `KEEP` in `make_patches.py`, then run `make`. Patch numbers never shift, so patch 18 is always Juno patch 18.

**Result (2026-11-21):** SRAM went from 462 KB (94%) to 243 KB (49.5%). Why we did this: [docs/decisions/001-trim-amy-for-daisy.md](../../../docs/decisions/001-trim-amy-for-daisy.md).
