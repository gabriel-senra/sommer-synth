#pragma once
#include <SDL.h>

// Master metronome / transport clock. Owns the bar grid the looper phase-locks
// to. The click is a one-shot percussive note on its own AMY synth slot, so it
// never steals voices from the live keyboard (synth 0) or the loop tracks (1-4).

#define BEATS_PER_BAR  4
#define METRO_SYNTH    5     // AMY synth slot for the click

typedef struct {
    int    running;          // metronome click on/off
    int    anchor_set;       // has the grid origin ever been established
    Uint32 anchor_ms;        // grid origin — a downbeat sits exactly here
    Uint32 last_tick_ms;     // previous frame's time (for beat-cross detection)
    int    cur_beat;         // 0..BEATS_PER_BAR-1, for the UI beat indicator
} Transport;

extern Transport transport;

// Grid maths (derived live from arp_bpm).
Uint32 transport_beat_ms(void);
Uint32 transport_bar_ms(void);

// Lifecycle / control.
void transport_init(void);      // load the click patch into METRO_SYNTH (once)
void transport_toggle(void);    // PLAY / STOP button
void transport_tick(void);      // fire clicks + advance cur_beat; call each frame
int  transport_active(void);    // metronome currently running?

// Grid helpers used by the looper to lock recordings to bars.
Uint32 transport_grid_origin(Uint32 t);  // downbeat at or before t
Uint32 transport_snap_len(Uint32 span);   // span rounded to nearest whole bar (min 1)
