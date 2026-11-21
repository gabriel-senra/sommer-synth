#pragma once
#include <stddef.h>

// ── Shared types ──────────────────────────────────────────────────────────────

#define MAX_CHORD_NOTES 9
#define NUM_CK          15

typedef enum { Q_MAJ, Q_MIN, Q_DOM, Q_DIM } Quality;
typedef enum { ROW_CHORD, ROW_BASS, ROW_LEAD } RowMode;
typedef struct { int n[6]; int c; } OvrVoicing;
typedef struct { int active; int notes[MAX_CHORD_NOTES]; int num; } Active;

// ── Constants (defined in chords.c) ──────────────────────────────────────────

extern const int        MAJOR_SCALE[7];
extern const char      *KEY_NAMES[12];
extern const char      *DEGREE_ROMAN[7];
extern const char      *VOICE_LABELS[5];
extern const Quality    DEGREE_QUALITY[7];
extern const OvrVoicing OVR[8][4];
extern const char      *OVR_LABELS[7];

// ── Pure functions (all state passed as parameters) ───────────────────────────

int         build_chord_notes(int degree, int root_midi, int voice_level,
                              int bass, int quality_override, int *out);
void        make_chord_name(int key_root, int degree, int voice_level,
                            int quality_override, char *buf, int bufsz);
