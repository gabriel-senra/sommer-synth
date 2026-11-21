#pragma once

extern const char *REV_LABELS[4];
extern const char *CHO_LABELS[4];
extern const char *DLY_LABELS[4];

float fx_delay_ms(void);   // current echo time, as the engine plays it

void apply_reverb(void);
void apply_chorus(void);
void apply_delay(void);
void apply_all_fx(void);
