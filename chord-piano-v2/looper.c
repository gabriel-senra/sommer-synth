#include "looper.h"
#include "state.h"
#include "amy.h"
#include <string.h>
#include <stdio.h>   // DEBUG: temporary diagnostics

Looper looper = { .state = LOOP_IDLE, .active_track = 0 };

#define MIN_LOOP_LEN_MS    200   // floor so an accidental tap can't make a 0ms loop
#define LOOP_TRACK_VOICES  12    // per-track polyphony (each track is its own AMY synth)

// Each loop track plays on its own AMY synth so it keeps the patch it was
// recorded with, independent of the live keyboard (synth 0).
static int track_synth(int track) { return track + 1; }

// ── AMY note fire ─────────────────────────────────────────────────────────────

static void fire_on(int synth, int note, float vel) {
    amy_event e = amy_default_event();
    e.synth = (uint8_t)synth; e.midi_note = (float)note; e.velocity = vel;
    amy_add_event(&e);
}
static void fire_off(int synth, int note) {
    amy_event e = amy_default_event();
    e.synth = (uint8_t)synth; e.midi_note = (float)note; e.velocity = 0.0f;
    amy_add_event(&e);
}

// Load a patch into a track's own synth slot (done once when a track is first
// recorded, so changing the live patch later doesn't alter the recorded loop).
static void load_track_patch(int track, int patch) {
    fprintf(stderr, "[loop] SETUP track=%d -> synth=%d patch=%d\n",
            track, track_synth(track), patch);  // DEBUG
    amy_event e = amy_default_event();
    e.synth        = (uint8_t)track_synth(track);
    e.num_voices   = LOOP_TRACK_VOICES;
    e.patch_number = (uint16_t)patch;
    amy_add_event(&e);
}

// ── Record ────────────────────────────────────────────────────────────────────

static Uint32 cur_loop_pos(Uint32 now) {
    if (looper.loop_len_ms == 0) return now - looper.anchor_ms;   // base pass
    return (now - looper.anchor_ms) % looper.loop_len_ms;          // overdub
}

static void record_event(int note, LoopEventType type, float vel) {
    LoopTrack *t = &looper.tracks[looper.rec_track];
    if (t->count >= MAX_LOOP_EVENTS) return;
    Uint32 now = SDL_GetTicks();

    // Base pass snaps its start to the first note played — no leading silence.
    if (looper.state == LOOP_RECORDING && !looper.rec_anchored) {
        looper.anchor_ms    = now;
        looper.rec_start_ms = now;
        looper.last_pos_ms  = 0;
        looper.rec_anchored = 1;
    }

    t->events[t->count++] = (LoopEvent){
        .time_ms  = cur_loop_pos(now),
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

// Close any notes left sounding within an event range [from, to): record a
// NOTE_OFF (at pos_ms) and fire it. Used when a record pass ends.
static void close_held_in_range(int track, int from, int to, Uint32 pos_ms) {
    LoopTrack *t = &looper.tracks[track];
    int on[128] = {0};
    for (int i = from; i < to; i++) {
        if (t->events[i].type == NOTE_ON)  on[t->events[i].note] = 1;
        if (t->events[i].type == NOTE_OFF) on[t->events[i].note] = 0;
    }
    for (int n = 0; n < 128; n++) {
        if (!on[n]) continue;
        if (t->count < MAX_LOOP_EVENTS)
            t->events[t->count++] = (LoopEvent){ .time_ms = pos_ms, .note = n,
                                                 .type = NOTE_OFF, .velocity = 0.0f };
        fire_off(track_synth(track), n);
    }
}

// ── Playback tick ─────────────────────────────────────────────────────────────

// During an overdub on rec_track, the notes being recorded this pass are already
// sounding live (fired by synth.c) — replaying them would double-trigger. So for
// the track currently recording, only play events from layers before the open one.
static int track_play_limit(int ti) {
    LoopTrack *t = &looper.tracks[ti];
    if (looper.state == LOOP_OVERDUB && ti == looper.rec_track && t->num_layers > 0)
        return t->layer_start[t->num_layers - 1];
    return t->count;
}

static void fire_event(int synth, const LoopEvent *e) {
    if (e->type == NOTE_ON) fire_on(synth, e->note, e->velocity);
    else                    fire_off(synth, e->note);
}

// Silence everything still sounding at the loop's end (notes with a NOTE_ON and
// no matching NOTE_OFF in the played range).
static void silence_loop_tail(LoopTrack *t, int limit, int synth) {
    int on[128] = {0};
    for (int i = 0; i < limit; i++) {
        if (t->events[i].type == NOTE_ON) on[t->events[i].note] = 1;
        else                              on[t->events[i].note] = 0;
    }
    for (int n = 0; n < 128; n++) if (on[n]) fire_off(synth, n);
}

static void play_track(int ti, Uint32 prev, Uint32 cur, Uint32 len) {
    (void)len;
    LoopTrack *t = &looper.tracks[ti];
    int limit = track_play_limit(ti);
    if (t->muted || limit == 0) return;
    int synth = track_synth(ti);

    if (cur >= prev) {
        // No seam this tick — fire events in (prev, cur].
        for (int i = 0; i < limit; i++) {
            Uint32 ts = t->events[i].time_ms;
            if (ts > prev && ts <= cur) fire_event(synth, &t->events[i]);
        }
        return;
    }

    // Seam crossed this tick. Order matters: finish the old loop and silence
    // anything still ringing BEFORE the new loop's opening notes, otherwise the
    // last note we played bleeds over the loop's first note.
    for (int i = 0; i < limit; i++) {                 // tail of old loop: (prev, len]
        Uint32 ts = t->events[i].time_ms;
        if (ts > prev) fire_event(synth, &t->events[i]);
    }
    silence_loop_tail(t, limit, synth);                // kill anything still held at loop end
    for (int i = 0; i < limit; i++) {                 // start of new loop: [0, cur]
        Uint32 ts = t->events[i].time_ms;
        if (ts <= cur) fire_event(synth, &t->events[i]);
    }
}

void loop_tick(void) {
    if (looper.state != LOOP_PLAYING && looper.state != LOOP_OVERDUB) return;
    if (looper.loop_len_ms == 0) return;

    Uint32 now  = SDL_GetTicks();
    Uint32 pos  = (now - looper.anchor_ms) % looper.loop_len_ms;
    Uint32 prev = looper.last_pos_ms;

    for (int i = 0; i < NUM_LOOP_TRACKS; i++)
        play_track(i, prev, pos, looper.loop_len_ms);

    looper.last_pos_ms = pos;
}

// ── Record trigger (momentary) ────────────────────────────────────────────────

void loop_rec_down(void) {
    if (looper.state == LOOP_RECORDING || looper.state == LOOP_OVERDUB) return;
    Uint32 now = SDL_GetTicks();

    looper.rec_track = looper.active_track;
    LoopTrack *t = &looper.tracks[looper.rec_track];

    // First take on an empty track captures the current patch into the track's
    // own synth, so later live-patch changes don't alter the recorded loop.
    if (t->count == 0) {
        t->patch = current_patch;
        load_track_patch(looper.rec_track, t->patch);
    }

    // Open a new layer (drop the oldest boundary if we ever overflow).
    if (t->num_layers < MAX_LOOP_LAYERS)
        t->layer_start[t->num_layers++] = t->count;

    if (looper.loop_len_ms == 0) {
        // Base pass: the loop start anchors to the first note (set in record_event),
        // so holding REC before playing adds no leading silence.
        looper.rec_anchored = 0;
        looper.state        = LOOP_RECORDING;
    } else {
        looper.rec_anchored = 1;
        looper.state        = LOOP_OVERDUB;
    }
    (void)now;
}

void loop_rec_up(void) {
    Uint32 now = SDL_GetTicks();
    LoopTrack *t = &looper.tracks[looper.rec_track];
    int layer_from = (t->num_layers > 0) ? t->layer_start[t->num_layers - 1] : 0;

    if (looper.state == LOOP_RECORDING) {
        if (!looper.rec_anchored) {
            // Held REC but never played a note — discard the empty take.
            if (t->num_layers > 0) t->num_layers--;
            looper.state = LOOP_IDLE;
            return;
        }
        Uint32 recorded = now - looper.rec_start_ms;
        looper.loop_len_ms = (recorded < MIN_LOOP_LEN_MS) ? MIN_LOOP_LEN_MS : recorded;
        // Close notes still held; stamp the NOTE_OFFs at the loop end.
        close_held_in_range(looper.rec_track, layer_from, t->count, looper.loop_len_ms);
        looper.last_pos_ms = 0;
        looper.state       = LOOP_PLAYING;
    } else if (looper.state == LOOP_OVERDUB) {
        Uint32 pos = cur_loop_pos(now);
        close_held_in_range(looper.rec_track, layer_from, t->count, pos);
        looper.state = LOOP_PLAYING;
    }
}

// ── Track ops ─────────────────────────────────────────────────────────────────

void loop_select_track(int track) {
    if (track < 0 || track >= NUM_LOOP_TRACKS) return;
    looper.active_track = track;
}

// Silence any note that events in [from, to) could be sounding right now. We
// can't use a net on/off ledger here: a note sustaining across the current
// playhead has its NOTE_OFF later in the range, so the ledger would think it's
// off and leave it hanging. Note-off every pitch that has a NOTE_ON in range.
static void silence_range(int track, int from, int to) {
    LoopTrack *t = &looper.tracks[track];
    int seen[128] = {0};
    for (int i = from; i < to; i++)
        if (t->events[i].type == NOTE_ON) seen[t->events[i].note] = 1;
    for (int n = 0; n < 128; n++) if (seen[n]) fire_off(track_synth(track), n);
}

static int all_tracks_empty(void) {
    for (int i = 0; i < NUM_LOOP_TRACKS; i++)
        if (looper.tracks[i].count > 0) return 0;
    return 1;
}

void loop_undo_layer(int track) {
    if (track < 0 || track >= NUM_LOOP_TRACKS) return;
    LoopTrack *t = &looper.tracks[track];
    if (t->num_layers == 0) return;

    int start = t->layer_start[--t->num_layers];
    silence_range(track, start, t->count);   // best-effort; other layers re-trigger next cycle
    t->count = start;

    if (all_tracks_empty()) {
        looper.loop_len_ms = 0;
        looper.last_pos_ms = 0;
        looper.state       = LOOP_IDLE;
    }
}

void loop_clear_track(int track) {
    if (track < 0 || track >= NUM_LOOP_TRACKS) return;
    LoopTrack *t = &looper.tracks[track];
    silence_range(track, 0, t->count);
    memset(t, 0, sizeof(LoopTrack));

    if (all_tracks_empty()) {
        looper.loop_len_ms = 0;
        looper.last_pos_ms = 0;
        looper.state       = LOOP_IDLE;
    }
}

// ── UI helpers ────────────────────────────────────────────────────────────────

int loop_track_layers(int track) {
    if (track < 0 || track >= NUM_LOOP_TRACKS) return 0;
    return looper.tracks[track].num_layers;
}

float loop_position(void) {
    if (looper.loop_len_ms == 0) return 0.0f;
    if (looper.state != LOOP_PLAYING && looper.state != LOOP_OVERDUB) return 0.0f;
    return (float)looper.last_pos_ms / (float)looper.loop_len_ms;
}
