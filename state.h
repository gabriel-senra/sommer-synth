#pragma once
#include "arp.h"   // ArpState, ArpOrder; pulls in chords.h (RowMode, Active, NUM_CK) and SDL.h

// ── Keyboard → chord mapping ──────────────────────────────────────────────────

typedef struct { SDL_Keycode key; int degree; int oct_off; const char *label; } ChordKey;
extern const ChordKey CK[NUM_CK];

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
