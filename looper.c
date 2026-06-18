#include "looper.h"
#include "state.h"
#include "amy.h"
#include <string.h>

Looper looper = { .state = LOOP_IDLE, .active_track = 0 };

// ── Timing helpers ────────────────────────────────────────────────────────────

static Uint32 beat_ms(void)  { return (Uint32)(60000 / arp_bpm); }
static Uint32 bar_ms(void)   { return beat_ms() * 4; }

static Uint32 next_bar_boundary(Uint32 from_ms) {
    Uint32 b = bar_ms();
    // round up to next full bar from armed_at_ms as the phase origin
    Uint32 elapsed = from_ms - looper.armed_at_ms;
    Uint32 bars_done = elapsed / b;
    return looper.armed_at_ms + (bars_done + 1) * b;
}

static Uint32 nearest_bar(Uint32 duration_ms) {
    Uint32 b = bar_ms();
    Uint32 n = (duration_ms + b / 2) / b;
    return (n < 1 ? 1 : n) * b;
}

// ── AMY note fire ─────────────────────────────────────────────────────────────

static void fire_on(int note, float vel) {
    amy_event e = amy_default_event();
    e.synth = 0; e.midi_note = (float)note; e.velocity = vel;
    amy_add_event(&e);
}
static void fire_off(int note) {
    amy_event e = amy_default_event();
    e.synth = 0; e.midi_note = (float)note; e.velocity = 0.0f;
    amy_add_event(&e);
}

// ── Record ────────────────────────────────────────────────────────────────────

static void record_event(int note, LoopEventType type, float vel) {
    LoopTrack *t = &looper.tracks[looper.active_track];
    if (t->count >= MAX_LOOP_EVENTS) return;
    Uint32 now = SDL_GetTicks();
    t->events[t->count++] = (LoopEvent){
        .time_ms  = now - looper.rec_start_ms,
        .note     = note,
        .type     = type,
        .velocity = vel,
    };
}

void loop_record_on(int note, float velocity) {
    if (looper.state == LOOP_RECORDING || looper.state == LOOP_OVERDUB)
        record_event(note, NOTE_ON, velocity);
}

void loop_record_off(int note) {
    if (looper.state == LOOP_RECORDING || looper.state == LOOP_OVERDUB)
        record_event(note, NOTE_OFF, 0.0f);
}

// ── Playback tick ─────────────────────────────────────────────────────────────

static void play_track(int ti, Uint32 prev, Uint32 cur, Uint32 len) {
    LoopTrack *t = &looper.tracks[ti];
    if (t->muted || t->count == 0) return;

    int wrapped = (cur < prev);  // loop seam crossed this tick

    for (int i = 0; i < t->count; i++) {
        Uint32 ts = t->events[i].time_ms;
        int fire = 0;
        if (!wrapped)
            fire = (ts > prev && ts <= cur);
        else
            // events in [prev, len) ∪ [0, cur]
            fire = (ts > prev && ts < len) || (ts <= cur);
        if (!fire) continue;

        if (t->events[i].type == NOTE_ON)
            fire_on(t->events[i].note, t->events[i].velocity);
        else
            fire_off(t->events[i].note);
    }

    // At the seam, silence any notes that have a NOTE_ON but no NOTE_OFF
    // within the loop (they'd hang forever). Scan all notes that are 'on'
    // at the seam and force a note_off.
    if (wrapped) {
        // Build a simple bitmask of notes that ended up 'on' at loop end
        // by replaying the on/off ledger — cheap for 2048 events.
        int on[128] = {0};
        for (int i = 0; i < t->count; i++) {
            if (t->events[i].type == NOTE_ON)  on[t->events[i].note] = 1;
            if (t->events[i].type == NOTE_OFF) on[t->events[i].note] = 0;
        }
        for (int n = 0; n < 128; n++) if (on[n]) fire_off(n);
    }
}

void loop_tick(void) {
    Uint32 now = SDL_GetTicks();

    // ARMED: wait for pre-computed bar boundary (rec_start_ms set on ARMED entry)
    if (looper.state == LOOP_ARMED) {
        if (now >= looper.rec_start_ms) {
            looper.last_pos_ms = 0;
            looper.state       = LOOP_RECORDING;
        }
        return;
    }

    if (looper.state != LOOP_PLAYING && looper.state != LOOP_OVERDUB) return;
    if (looper.loop_len_ms == 0) return;

    Uint32 pos  = (now - looper.rec_start_ms) % looper.loop_len_ms;
    Uint32 prev = looper.last_pos_ms;

    for (int i = 0; i < NUM_LOOP_TRACKS; i++)
        play_track(i, prev, pos, looper.loop_len_ms);

    looper.last_pos_ms = pos;
}

// ── State transitions ─────────────────────────────────────────────────────────

void loop_press_rec(void) {
    Uint32 now = SDL_GetTicks();
    switch (looper.state) {

    case LOOP_IDLE:
        looper.armed_at_ms  = now;
        looper.rec_start_ms = next_bar_boundary(now);  // pre-compute target
        looper.state        = LOOP_ARMED;
        break;

    case LOOP_ARMED:
        // Cancel arm
        looper.state = LOOP_IDLE;
        break;

    case LOOP_RECORDING:
        // First pass done — quantise length and start playing
        {
            Uint32 recorded = now - looper.rec_start_ms;
            looper.loop_len_ms  = nearest_bar(recorded);
            looper.last_pos_ms  = 0;
            // Force note-offs for anything still held at stop time
            LoopTrack *t = &looper.tracks[looper.active_track];
            int on[128] = {0};
            for (int i = 0; i < t->count; i++) {
                if (t->events[i].type == NOTE_ON)  on[t->events[i].note] = 1;
                if (t->events[i].type == NOTE_OFF) on[t->events[i].note] = 0;
            }
            for (int n = 0; n < 128; n++) {
                if (on[n]) {
                    record_event(n, NOTE_OFF, 0.0f);
                    fire_off(n);
                }
            }
            looper.state = LOOP_PLAYING;
        }
        break;

    case LOOP_PLAYING:
        // Start overdub on active track
        looper.state = LOOP_OVERDUB;
        break;

    case LOOP_OVERDUB:
        // End overdub, back to playing
        {
            // Force note-offs for held notes in overdub layer
            LoopTrack *t = &looper.tracks[looper.active_track];
            Uint32 pos = (now - looper.rec_start_ms) % looper.loop_len_ms;
            int on[128] = {0};
            for (int i = 0; i < t->count; i++) {
                if (t->events[i].time_ms > looper.last_pos_ms) break; // overdub events are at end
                if (t->events[i].type == NOTE_ON)  on[t->events[i].note] = 1;
                if (t->events[i].type == NOTE_OFF) on[t->events[i].note] = 0;
            }
            for (int n = 0; n < 128; n++) {
                if (on[n]) { record_event(n, NOTE_OFF, 0.0f); fire_off(n); }
            }
            looper.last_pos_ms = pos;
            looper.state       = LOOP_PLAYING;
        }
        break;
    }
}

void loop_select_track(int track) {
    if (track < 0 || track >= NUM_LOOP_TRACKS) return;
    looper.active_track = track;
}

void loop_clear_track(int track) {
    if (track < 0 || track >= NUM_LOOP_TRACKS) return;
    // Silence any playing notes from this track first
    LoopTrack *t = &looper.tracks[track];
    int on[128] = {0};
    for (int i = 0; i < t->count; i++) {
        if (t->events[i].type == NOTE_ON)  on[t->events[i].note] = 1;
        if (t->events[i].type == NOTE_OFF) on[t->events[i].note] = 0;
    }
    for (int n = 0; n < 128; n++) if (on[n]) fire_off(n);
    memset(t, 0, sizeof(LoopTrack));
}

// ── UI helpers ────────────────────────────────────────────────────────────────

int loop_beat_countdown(void) {
    if (looper.state != LOOP_ARMED) return 0;
    Uint32 now = SDL_GetTicks();
    Uint32 b = beat_ms();
    Uint32 remaining_ms = (looper.rec_start_ms > now) ? (looper.rec_start_ms - now) : 0;
    int beats = (int)((remaining_ms + b - 1) / b);  // round up
    return beats > 4 ? 4 : beats;
}

float loop_position(void) {
    if (looper.loop_len_ms == 0) return 0.0f;
    if (looper.state != LOOP_PLAYING && looper.state != LOOP_OVERDUB) return 0.0f;
    return (float)looper.last_pos_ms / (float)looper.loop_len_ms;
}
