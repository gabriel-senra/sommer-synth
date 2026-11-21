#pragma once
#include <SDL.h>

// Master metronome / transport clock — a click you can play along to. The click
// is a one-shot percussive note on its own AMY synth slot, so it never steals
// voices from the live keyboard (synth 0).

#define BEATS_PER_BAR  4
#define METRO_SYNTH    1     // AMY synth slot for the click (0 = live keyboard)

typedef struct {
    int    running;          // metronome click on/off
    Uint32 anchor_ms;        // grid origin — a downbeat sits exactly here
    Uint32 last_tick_ms;     // previous frame's time (for beat-cross detection)
    int    cur_beat;         // 0..BEATS_PER_BAR-1, for the UI beat indicator
} Transport;

extern Transport transport;

// Beat length, derived live from arp_bpm.
Uint32 transport_beat_ms(void);

// Lifecycle / control.
void transport_init(void);      // load the click patch into METRO_SYNTH (once)
void transport_toggle(void);    // PLAY / STOP button
void transport_tick(void);      // fire clicks + advance cur_beat; call each frame
