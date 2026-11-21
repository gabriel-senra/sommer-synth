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

static void amy_note(int m, float velocity) {
    amy_event e = amy_default_event();
    e.synth = 0; e.midi_note = (float)m; e.velocity = velocity;
    amy_add_event(&e);
}
void amy_note_on_midi(int m)  { amy_note(m, 0.75f); }
void amy_note_off_midi(int m) { amy_note(m, 0.0f); }

// Reference-counted notes: when two held pads share a pitch, it keeps sounding
// until the last one lets go.
static void press_notes(const int *notes, int n) {
    for (int i = 0; i < n; i++) {
        int m = notes[i];
        if (m < 0 || m > 127) continue;
        if (note_count[m]++ == 0) amy_note_on_midi(m);
    }
}
static void release_notes(const int *notes, int n) {
    for (int i = 0; i < n; i++) {
        int m = notes[i];
        if (m < 0 || m > 127) continue;
        if (--note_count[m] <= 0) { note_count[m] = 0; amy_note_off_midi(m); }
    }
}

// Silence whatever pad ci is playing (arp note or held chord) and mark it idle.
static void stop_pad(int ci) {
    if (arp_on) arp_stop_note(ci);
    else        release_notes(actv[ci].notes, actv[ci].num);
    actv[ci].active = 0; actv[ci].num = 0;
}

int notes_for(int ci, int *notes) {
    if (piano_mode) {
        int m = piano_midi(ci);
        if (m < 0) return 0;          // gap key — no black note here
        notes[0] = m;
        return 1;
    }
    switch (row_mode_for(ci)) {
        case ROW_BASS: notes[0] = ck_bass_midi(ci); return 1;
        case ROW_LEAD: notes[0] = ck_lead_midi(ci); return 1;
        default: return build_chord_notes(CK[ci].degree, ck_root_midi(ci),
                                          voice_level, bass_on, quality_override, notes);
    }
}

void silence_all(void) {
    for (int i = 0; i < NUM_CK; i++) if (actv[i].active) stop_pad(i);
    memset(note_count, 0, sizeof(note_count));
    for (int i = 0; i < NUM_CK; i++) arp_st[i].cur_note = -1;
}

void load_patch(int patch) {
    silence_all();
    current_patch = patch;
    amy_event e = amy_default_event();
    e.synth = 0; e.num_voices = 32; e.patch_number = (uint16_t)patch;
    amy_add_event(&e);
}

void trigger_chord(int ci) {
    if (arp_on && arp_hold && actv[ci].active) { stop_pad(ci); return; }  // tap again = unlatch
    if (actv[ci].active) return;
    int notes[MAX_CHORD_NOTES];
    int nn = notes_for(ci, notes);
    if (nn == 0) return;
    actv[ci].active = 1; actv[ci].num = nn;
    memcpy(actv[ci].notes, notes, nn * sizeof(int));
    if (arp_on) arp_init_key(ci);
    else        press_notes(notes, nn);
}

void release_chord(int ci) {
    if (!actv[ci].active) return;
    if (arp_on && arp_hold) return;   // latched: keeps arpeggiating after release
    stop_pad(ci);
}

// Re-voice every held pad after a key/octave/voicing change.
void retrigger_all_active(void) {
    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active) continue;
        int notes[MAX_CHORD_NOTES];
        int nn = notes_for(i, notes);
        if (arp_on) arp_stop_note(i);
        else        release_notes(actv[i].notes, actv[i].num);
        actv[i].num = nn;
        memcpy(actv[i].notes, notes, nn * sizeof(int));
        if (arp_on) arp_init_key(i);
        else        press_notes(notes, nn);
    }
}

void toggle_arp(void) {
    silence_all();            // clean switch before flipping mode
    arp_on ^= 1;
}

void toggle_piano_mode(void) {
    silence_all();
    piano_mode ^= 1;
}
