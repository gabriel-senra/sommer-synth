#include "fx.h"
#include "state.h"
#include "amy.h"

const char *REV_LABELS[4] = {"OFF","ROOM","HALL","PLATE"};
const char *CHO_LABELS[4] = {"OFF","LGT","WARM","DEEP"};
const char *DLY_LABELS[4] = {"OFF","8TH","QTR","D8TH"};

void apply_reverb(void) {
    switch (rev_preset) {
        case 0: config_reverb(0.0f,  0.85f, 0.50f, 3000.0f); break;
        case 1: config_reverb(0.30f, 0.60f, 0.65f, 2000.0f); break;
        case 2: config_reverb(0.50f, 0.85f, 0.40f, 3000.0f); break;
        case 3: config_reverb(0.65f, 0.95f, 0.15f, 4500.0f); break;
    }
}

void apply_chorus(void) {
    switch (cho_preset) {
        case 0: config_chorus(0.0f, 320, 0.5f, 0.50f); break;
        case 1: config_chorus(0.3f, 320, 0.3f, 0.30f); break;
        case 2: config_chorus(0.5f, 320, 0.5f, 0.55f); break;
        case 3: config_chorus(0.7f, 320, 0.8f, 0.85f); break;
    }
}

void apply_delay(void) {
    if (dly_preset == 0) { config_echo(0.0f, 500.0f, 743.0f, 0.0f, 0.0f); return; }
    float beat = 60000.0f / arp_bpm;
    float ms, fb;
    switch (dly_preset) {
        case 1: ms = beat * 0.5f; fb = 0.25f; break;
        case 2: ms = beat;        fb = 0.30f; break;
        default:ms = beat * 1.5f; fb = 0.35f; break;
    }
    if (ms > 743.0f) ms = 743.0f;
    config_echo(0.38f, ms, 743.0f, fb, 0.25f);
}

void apply_all_fx(void) { apply_reverb(); apply_chorus(); apply_delay(); }
