#include "transport.h"
#include "state.h"
#include "amy.h"

Transport transport = { 0 };

// Xylophone patch — bright, short, percussive; a natural metronome click.
#define METRO_PATCH    18
#define METRO_VOICES    3     // enough that back-to-back clicks don't cut each other
#define METRO_NOTE_HI  84     // downbeat (accent)
#define METRO_NOTE_LO  79     // beats 2..N

Uint32 transport_beat_ms(void) { return (Uint32)(60000 / arp_bpm); }
Uint32 transport_bar_ms(void)  { return transport_beat_ms() * BEATS_PER_BAR; }

int transport_active(void) { return transport.running; }

// The click is a mallet sound with a natural decay, so no note-off is needed —
// firing note-on each beat and letting the voices ring/steal is the whole sound.
static void fire_click(int accent) {
    amy_event e = amy_default_event();
    e.synth     = METRO_SYNTH;
    e.midi_note = accent ? METRO_NOTE_HI : METRO_NOTE_LO;
    e.velocity  = accent ? 1.0f : 0.5f;
    amy_add_event(&e);
}

void transport_init(void) {
    amy_event e = amy_default_event();
    e.synth        = METRO_SYNTH;
    e.num_voices   = METRO_VOICES;
    e.patch_number = METRO_PATCH;
    amy_add_event(&e);
}

void transport_toggle(void) {
    if (transport.running) { transport.running = 0; return; }
    Uint32 now = SDL_GetTicks();
    transport.running      = 1;
    transport.anchor_ms    = now;   // start the grid now, click the downbeat at once
    transport.anchor_set   = 1;
    transport.cur_beat     = 0;
    transport.last_tick_ms = now;
    fire_click(1);
}

void transport_tick(void) {
    if (!transport.running || !transport.anchor_set) return;
    Uint32 beat = transport_beat_ms();
    if (beat == 0) return;

    Uint32 now = SDL_GetTicks();
    long cur  = (long)(now - transport.anchor_ms)             / (long)beat;
    long prev = (long)(transport.last_tick_ms - transport.anchor_ms) / (long)beat;

    if (cur > prev) {                       // crossed at least one beat boundary
        int bib = (int)(((cur % BEATS_PER_BAR) + BEATS_PER_BAR) % BEATS_PER_BAR);
        transport.cur_beat = bib;
        fire_click(bib == 0);               // accent the downbeat
    }
    transport.last_tick_ms = now;
}
