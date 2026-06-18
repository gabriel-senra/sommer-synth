#pragma once
#include <SDL.h>

#define NUM_LOOP_TRACKS  4
#define MAX_LOOP_EVENTS  2048
#define MAX_LOOP_LAYERS  32      // overdub layers per track (undo depth)

typedef enum { NOTE_ON, NOTE_OFF } LoopEventType;

typedef struct {
    Uint32        time_ms;       // position within the loop (0..loop_len_ms)
    int           note;
    LoopEventType type;
    float         velocity;
} LoopEvent;

typedef struct {
    LoopEvent events[MAX_LOOP_EVENTS];
    int       count;
    int       muted;
    int       layer_start[MAX_LOOP_LAYERS]; // event index where each layer began
    int       num_layers;
    int       patch;                        // AMY patch this track was recorded with
} LoopTrack;

typedef enum {
    LOOP_IDLE,       // no loop established, nothing recording
    LOOP_RECORDING,  // base pass: defining the free master length
    LOOP_PLAYING,    // loop running
    LOOP_OVERDUB,    // loop running + recording a layer into rec_track
} LoopState;

typedef struct {
    LoopState  state;
    int        active_track;     // selected track (UI)
    int        rec_track;        // track being recorded this hold (latched at rec-down)
    int        rec_anchored;     // base pass: has the first note anchored the loop yet?

    Uint32     anchor_ms;        // loop phase origin; pos = (now - anchor_ms) % loop_len_ms
    Uint32     rec_start_ms;     // first-note time of the base pass (for length calc)
    Uint32     loop_len_ms;      // FREE master length, 0 until first base pass closes
    Uint32     last_pos_ms;      // previous tick's position (for seam detection)

    LoopTrack  tracks[NUM_LOOP_TRACKS];
} Looper;

extern Looper looper;

// Called from main loop
void loop_tick(void);

// Called from synth.c when a note fires
void loop_record_on(int note, float velocity);
void loop_record_off(int note);

// State transitions (called from event handler)
void loop_rec_down(void);                // REC button / hold key pressed
void loop_rec_up(void);                  // REC button / hold key released
void loop_select_track(int track);       // click a track button
void loop_undo_layer(int track);         // remove the most recent layer of a track
void loop_clear_track(int track);        // delete all track contents

// UI helpers
int   loop_track_layers(int track);      // number of layers on a track
float loop_position(void);               // 0.0–1.0 progress through loop
