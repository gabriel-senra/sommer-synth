#pragma once
#include <SDL.h>

#define NUM_LOOP_TRACKS  4
#define MAX_LOOP_EVENTS  2048

typedef enum { NOTE_ON, NOTE_OFF } LoopEventType;

typedef struct {
    Uint32        time_ms;
    int           note;
    LoopEventType type;
    float         velocity;
} LoopEvent;

typedef struct {
    LoopEvent events[MAX_LOOP_EVENTS];
    int       count;
    int       muted;
} LoopTrack;

typedef enum {
    LOOP_IDLE,
    LOOP_ARMED,      // countdown running, waiting for bar boundary
    LOOP_RECORDING,
    LOOP_PLAYING,
    LOOP_OVERDUB,
} LoopState;

typedef struct {
    LoopState  state;
    int        active_track;    // 0–3

    Uint32     armed_at_ms;     // when REC was pressed (ARMED entry)
    Uint32     rec_start_ms;    // bar boundary where recording began
    Uint32     loop_len_ms;     // quantised loop length (set on first stop)
    Uint32     last_pos_ms;     // previous tick's position (for seam detection)

    LoopTrack  tracks[NUM_LOOP_TRACKS];
} Looper;

extern Looper looper;

// Called from main loop
void loop_tick(void);

// Called from synth.c when a note fires
void loop_record_on(int note, float velocity);
void loop_record_off(int note);

// State transitions (called from event handler)
void loop_press_rec(void);               // REC button
void loop_select_track(int track);       // click a track button
void loop_clear_track(int track);        // delete track contents

// UI helpers
int  loop_beat_countdown(void);          // beats remaining before record starts (0 = now)
float loop_position(void);              // 0.0–1.0 progress through loop
