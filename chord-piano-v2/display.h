#pragma once
#include <SDL.h>
#include "state.h"

// ── Layout constants ──────────────────────────────────────────────────────────

#define WIN_W  900
#define WIN_H  466

#define TOP_H  44
#define CTRL_Y (TOP_H + 1)
#define CTRL_H 40
#define MOD_Y  (CTRL_Y + CTRL_H + 1)
#define MOD_H  30
#define ARP_Y  (MOD_Y + MOD_H + 1)
#define ARP_H  28
#define ARP_BTN_H 20
#define ARP_BTN_Y (ARP_Y + (ARP_H - ARP_BTN_H) / 2)
#define FX_Y   (ARP_Y + ARP_H + 1)
#define FX_H   26
#define FX_BTN_H 18
#define FX_BTN_Y (FX_Y + (FX_H - FX_BTN_H) / 2)

#define PAD_W   116
#define PAD_H    92
#define PAD_GAP   2
#define PAD_X0   12
#define K_W      40
#define K_PAD_X  (PAD_X0 + 7*(PAD_W+PAD_GAP) + 14)

#define METRO_Y       (FX_Y + FX_H + 1)
#define METRO_H       28
#define METRO_BTN_H   20
#define METRO_BTN_Y   (METRO_Y + (METRO_H - METRO_BTN_H) / 2)

#define UPPER_LABEL_Y (METRO_Y + METRO_H + 3)
#define UPPER_PAD_Y   (UPPER_LABEL_Y + 14 + 2)
#define LOWER_LABEL_Y (UPPER_PAD_Y + PAD_H + 8)
#define LOWER_PAD_Y   (LOWER_LABEL_Y + 14 + 2)

#define MOD_BTN_W   PAD_W
#define MOD_BTN_GAP PAD_GAP
#define MOD_BTN_X0  PAD_X0
#define MOD_BTN_Y   (MOD_Y + (MOD_H - 22) / 2)
#define MOD_BTN_H   22

#define DOT_SZ   9
#define DOT_GAP  4
#define DOTS_X   90
#define DOTS_Y   (CTRL_Y + (CTRL_H - DOT_SZ) / 2)
#define KNOB_MAX 4

#define MODE_BTN_W  48
#define MODE_BTN_H  12
#define MODE_BTN_X  (WIN_W - MODE_BTN_W - 4)

// ── Colours ───────────────────────────────────────────────────────────────────

extern const SDL_Color C_BG, C_SURF, C_BORDER, C_SEP;
extern const SDL_Color C_HI, C_MID, C_DIM;
extern const SDL_Color C_ACCENT, C_WHITE, C_BLACK;
extern const SDL_Color OVR_COL[7];
extern const SDL_Color MODE_COL[3], MODE_TEXT_COL[3];

// ── Button rects — controls row ───────────────────────────────────────────────

extern const SDL_Rect BTN_KEY_PREV, BTN_KEY_NEXT;
extern const SDL_Rect BTN_PATCH_PREV, BTN_PATCH_NEXT;
extern const SDL_Rect BTN_BASS, BTN_OCT_DOWN, BTN_OCT_UP;
extern const SDL_Rect BTN_MODE;   // SCALE / PIANO toggle

// ── Button rects — arp row ────────────────────────────────────────────────────

extern const SDL_Rect BTN_ARP_TOG, BTN_ARP_DIV;
extern const SDL_Rect BTN_BPM_DOWN, BTN_BPM_UP;
extern const SDL_Rect BTN_ARP_ORD;
extern const SDL_Rect BTN_ARPOCT_DOWN, BTN_ARPOCT_UP;
extern const SDL_Rect BTN_ARP_GATE, BTN_ARP_HOLD;

// ── Button rects — fx row ─────────────────────────────────────────────────────

extern const SDL_Rect BTN_FX_REV, BTN_FX_CHO, BTN_FX_DLY;

// ── Button rects — metronome row ──────────────────────────────────────────────

extern const SDL_Rect BTN_METRO;       // metronome play/stop

// ── Hit-testing (used by main event loop) ────────────────────────────────────

int pt_in_rect(int px, int py, SDL_Rect re);
int hit_pad(int mx, int my);
int hit_dot(int mx, int my);
int hit_row_mode_btn(int mx, int my);

// ── Render ────────────────────────────────────────────────────────────────────

void render_frame(SDL_Renderer *r);
