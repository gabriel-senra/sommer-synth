#pragma once
#include "arp.h"   // ArpState, ArpOrder; pulls in chords.h (RowMode, Active, NUM_CK) and SDL.h

// ── Keyboard → chord mapping ──────────────────────────────────────────────────

typedef struct { SDL_Keycode key; int degree; int oct_off; const char *label; } ChordKey;
extern const ChordKey CK[NUM_CK];

// ── Piano mode ────────────────────────────────────────────────────────────────
// Reinterpret the pads as a chromatic keyboard: bottom row = white keys, top row
// = black keys (with gaps where no black key exists), K = white C an octave up.
// current_key acts as a transpose; octave_shift still applies.
#define PIANO_BASE 48
extern const int PIANO_SEMI[NUM_CK];   // semitone offset per pad; -1 = gap (no key)
extern int       piano_mode;
int  piano_midi(int ci);               // MIDI note for pad ci, or -1 if a gap
int  piano_is_black(int ci);           // 1 if pad ci is a black key

// ── App state ─────────────────────────────────────────────────────────────────

extern int     current_key;
extern int     voice_level;
extern int     bass_on;
extern int     octave_shift;
extern int     current_patch;
extern int     mouse_x, mouse_y, mouse_pad;
extern int     quality_override;
extern RowMode lower_mode, upper_mode;

// ── FX state ──────────────────────────────────────────────────────────────────

extern int rev_preset;   // 0=OFF  1=ROOM  2=HALL  3=PLATE
extern int cho_preset;   // 0=OFF  1=LGT   2=WARM  3=DEEP
extern int dly_preset;   // 0=OFF  1=8TH   2=QTR   3=D8TH

// ── Arp state ─────────────────────────────────────────────────────────────────

extern int      arp_on, arp_bpm, arp_div_idx, arp_octaves, arp_gate, arp_hold;
extern ArpOrder arp_order;

// ── Per-key playback state ────────────────────────────────────────────────────

extern Active   actv[NUM_CK];
extern int      note_count[128];
extern ArpState arp_st[NUM_CK];

// ── Helpers (used by display + synth) ────────────────────────────────────────

RowMode     row_mode_for(int ci);
const char *row_mode_name(RowMode m);
