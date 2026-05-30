#include "synth.h"
#include "chords.h"
#include "amy.h"
#include <string.h>

static int ck_root_midi(int i) {
    return 48 + current_key + MAJOR_SCALE[CK[i].degree] + CK[i].oct_off + octave_shift * 12;
}
static int ck_bass_midi(int i) {
    return 36 + (current_key + MAJOR_SCALE[CK[i].degree]) % 12 + octave_shift * 12;
}
static int ck_lead_midi(int i) {
    return 72 + (current_key + MAJOR_SCALE[CK[i].degree]) % 12 + octave_shift * 12;
}

void amy_note_on_midi(int m) {
    amy_event e = amy_default_event();
    e.synth = 0; e.midi_note = (float)m; e.velocity = 0.75f;
    amy_add_event(&e);
}

void amy_note_off_midi(int m) {
    amy_event e = amy_default_event();
    e.synth = 0; e.midi_note = (float)m; e.velocity = 0.0f;
    amy_add_event(&e);
}

int notes_for(int ci, int *notes) {
    switch (row_mode_for(ci)) {
        case ROW_BASS: notes[0] = ck_bass_midi(ci); return 1;
        case ROW_LEAD: notes[0] = ck_lead_midi(ci); return 1;
        default: return build_chord_notes(CK[ci].degree, ck_root_midi(ci),
                                          voice_level, bass_on, quality_override, notes);
    }
}

void load_patch(int patch) {
    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active) continue;
        if (arp_on) arp_stop_note(i);
        else for (int n = 0; n < actv[i].num; n++) amy_note_off_midi(actv[i].notes[n]);
        actv[i].active = 0; actv[i].num = 0;
    }
    memset(note_count, 0, sizeof(note_count));
    for (int i = 0; i < NUM_CK; i++) arp_st[i].cur_note = -1;
    current_patch = patch;
    amy_event e = amy_default_event();
    e.synth = 0; e.num_voices = 32; e.patch_number = (uint16_t)patch;
    amy_add_event(&e);
}

void trigger_chord(int ci) {
    if (arp_on && arp_hold && actv[ci].active) {
        arp_stop_note(ci);
        actv[ci].active = 0; actv[ci].num = 0;
        return;
    }
    if (actv[ci].active) return;
    int notes[MAX_CHORD_NOTES];
    int nn = notes_for(ci, notes);
    if (nn == 0) return;
    actv[ci].active = 1; actv[ci].num = nn;
    memcpy(actv[ci].notes, notes, nn * sizeof(int));
    if (arp_on) {
        arp_init_key(ci);
    } else {
        for (int i = 0; i < nn; i++) {
            int m = notes[i];
            if (m < 0 || m > 127) continue;
            if (note_count[m] == 0) amy_note_on_midi(m);
            note_count[m]++;
        }
    }
}

void release_chord(int ci) {
    if (!actv[ci].active) return;
    if (arp_on) {
        if (arp_hold) { arp_st[ci].held = 0; return; }
        arp_stop_note(ci);
    } else {
        for (int i = 0; i < actv[ci].num; i++) {
            int m = actv[ci].notes[i];
            if (m < 0 || m > 127) continue;
            if (--note_count[m] <= 0) { note_count[m] = 0; amy_note_off_midi(m); }
        }
    }
    actv[ci].active = 0; actv[ci].num = 0;
}

void retrigger_all_active(void) {
    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active) continue;
        int notes[MAX_CHORD_NOTES];
        int nn = notes_for(i, notes);
        if (arp_on) {
            arp_stop_note(i);
            actv[i].num = nn;
            memcpy(actv[i].notes, notes, nn * sizeof(int));
            arp_init_key(i);
        } else {
            for (int n = 0; n < actv[i].num; n++) {
                int m = actv[i].notes[n];
                if (m < 0 || m > 127) continue;
                if (--note_count[m] <= 0) { note_count[m] = 0; amy_note_off_midi(m); }
            }
            actv[i].num = nn;
            memcpy(actv[i].notes, notes, nn * sizeof(int));
            for (int n = 0; n < nn; n++) {
                int m = notes[n];
                if (m < 0 || m > 127) continue;
                if (note_count[m] == 0) amy_note_on_midi(m);
                note_count[m]++;
            }
        }
    }
}

void toggle_arp(void) {
    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active) continue;
        if (arp_on) arp_stop_note(i);
        else for (int n = 0; n < actv[i].num; n++) {
            int m = actv[i].notes[n];
            if (m < 0 || m > 127) continue;
            if (--note_count[m] <= 0) { note_count[m] = 0; amy_note_off_midi(m); }
        }
        actv[i].active = 0; actv[i].num = 0;
    }
    memset(note_count, 0, sizeof(note_count));
    for (int i = 0; i < NUM_CK; i++) arp_st[i].cur_note = -1;
    arp_on ^= 1;
}
