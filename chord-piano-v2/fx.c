#include "fx.h"
#include "state.h"
#include "amy.h"

// Presets are data: one row per button position (index 0 = OFF).
const char *REV_LABELS[4] = {"OFF","ROOM","HALL","PLATE"};
const char *CHO_LABELS[4] = {"OFF","LGT","WARM","DEEP"};
const char *DLY_LABELS[4] = {"OFF","8TH","QTR","D8TH"};

typedef struct { float level, liveness, damping, xover_hz; } ReverbPreset;
static const ReverbPreset REVERB[4] = {
    {0.00f, 0.85f, 0.50f, 3000.0f},
    {0.30f, 0.60f, 0.65f, 2000.0f},
    {0.50f, 0.85f, 0.40f, 3000.0f},
    {0.65f, 0.95f, 0.15f, 4500.0f},
};

typedef struct { float level, lfo_freq, depth; } ChorusPreset;
static const ChorusPreset CHORUS[4] = {
    {0.0f, 0.5f, 0.50f},
    {0.3f, 0.3f, 0.30f},
    {0.5f, 0.5f, 0.55f},
    {0.7f, 0.8f, 0.85f},
};
#define CHORUS_MAX_DELAY 320

// Delay time is a fraction of a beat, so echoes stay in time with arp_bpm.
typedef struct { float beats, feedback; } DelayPreset;
static const DelayPreset DELAY[4] = {
    {0.0f, 0.00f},
    {0.5f, 0.25f},   // 8th
    {1.0f, 0.30f},   // quarter
    {1.5f, 0.35f},   // dotted 8th
};
#define DELAY_MAX_MS 743.0f   // AMY's echo buffer length

float fx_delay_ms(void) {
    float ms = 60000.0f / arp_bpm * DELAY[dly_preset].beats;
    return ms > DELAY_MAX_MS ? DELAY_MAX_MS : ms;
}

void apply_reverb(void) {
    const ReverbPreset *p = &REVERB[rev_preset];
    config_reverb(p->level, p->liveness, p->damping, p->xover_hz);
}

void apply_chorus(void) {
    const ChorusPreset *p = &CHORUS[cho_preset];
    config_chorus(p->level, CHORUS_MAX_DELAY, p->lfo_freq, p->depth);
}

void apply_delay(void) {
    if (dly_preset == 0) { config_echo(0.0f, 500.0f, DELAY_MAX_MS, 0.0f, 0.0f); return; }
    config_echo(0.38f, fx_delay_ms(), DELAY_MAX_MS, DELAY[dly_preset].feedback, 0.25f);
}

void apply_all_fx(void) { apply_reverb(); apply_chorus(); apply_delay(); }
