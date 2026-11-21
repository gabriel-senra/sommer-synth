#pragma once
#include "state.h"

void amy_note_on_midi(int m);
void amy_note_off_midi(int m);
int  notes_for(int ci, int *notes);
void load_patch(int patch);
void trigger_chord(int ci);
void release_chord(int ci);
void retrigger_all_active(void);
void silence_all(void);
void toggle_arp(void);
void toggle_piano_mode(void);
