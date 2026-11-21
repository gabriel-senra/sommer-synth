#include "chords.h"
#include <stdio.h>
#include <string.h>

// ── Constants ─────────────────────────────────────────────────────────────────

const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};

const char *KEY_NAMES[12] = {
    "C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"
};
const char *DEGREE_ROMAN[7] = { "I","ii","iii","IV","V","vi","vii" };
const char *VOICE_LABELS[5] = { "SINGLE","TRIAD","7TH","9TH","11TH" };

const Quality DEGREE_QUALITY[7] = {
    Q_MAJ, Q_MIN, Q_MIN, Q_MAJ, Q_DOM, Q_MIN, Q_DIM
};

const OvrVoicing OVR[8][4] = {
    /* 0 min  */ { {{0,3,7},3},      {{0,3,7,10},4},      {{0,3,7,10,14},5},     {{0,3,7,10,14,17},6} },
    /* 1 dom  */ { {{0,4,7,10},4},   {{0,4,7,10},4},      {{0,4,7,10,14},5},     {{0,4,7,10,14,17},6} },
    /* 2 dim  */ { {{0,3,6},3},      {{0,3,6,9},4},       {{0,3,6,9,13},5},      {{0,3,6,9,13,17},6}  },
    /* 3 dom9 */ { {{0,4,7,10,14},5},{{0,4,7,10,14},5},   {{0,4,7,10,14},5},     {{0,4,7,10,14,17},6} },
    /* 4 sus4 */ { {{0,5,7},3},      {{0,5,7,10},4},      {{0,5,7,10,14},5},     {{0,5,7,10,14,17},6} },
    /* 5 maj6 */ { {{0,4,7,9},4},    {{0,4,7,9},4},       {{0,4,7,9,14},5},      {{0,4,7,9,14,17},6}  },
    /* 6 aug  */ { {{0,4,8},3},      {{0,4,8,10},4},      {{0,4,8,10,14},5},     {{0,4,8,10,14,17},6} },
    /* 7 maj  */ { {{0,4,7},3},      {{0,4,7,11},4},      {{0,4,7,11,14},5},     {{0,4,7,11,14,17},6} },
};

const char *OVR_NAMES[8][4] = {
    {"m","m7","m9","m11"}, {"7","7","9","11"},
    {"dim","dim7","dim9","dim11"}, {"9","9","9","11"},
    {"sus4","7sus4","9sus4","11sus4"}, {"6","6","6/9","6/11"},
    {"aug","aug7","aug9","aug11"}, {"","maj7","maj9","maj11"},
};

const char *OVR_LABELS[7] = {
    "1 +-3RD","2 DOM","3 DIM","4 DOM9","5 SUS4","6 MAJ6","7 AUG"
};

// ── Pure functions ────────────────────────────────────────────────────────────

int resolve_ovr(int ovr, int degree) {
    if (ovr != 0) return ovr;
    Quality q = DEGREE_QUALITY[degree];
    return (q == Q_MIN || q == Q_DIM) ? 7 : 0;
}

const char *quality_suffix(int degree, int vl_idx) {
    static const char *maj[] = {"","maj7","maj9","maj11"};
    static const char *min[] = {"m","m7","m9","m11"};
    static const char *dom[] = {"","7","9","11"};
    static const char *dim[] = {"dim","m7b5","m9b5","m11b5"};
    switch (DEGREE_QUALITY[degree]) {
        case Q_MAJ: return maj[vl_idx];
        case Q_MIN: return min[vl_idx];
        case Q_DOM: return dom[vl_idx];
        case Q_DIM: return dim[vl_idx];
    }
    return "";
}

int compute_intervals(int degree, int voice_level, int *out) {
    int num = voice_level + 3;
    for (int i = 0; i < num; i++) {
        int pos = degree + i * 2;
        int idx = pos % 7, oct = (pos / 7) * 12;
        out[i] = MAJOR_SCALE[idx] - MAJOR_SCALE[degree] + oct;
        if (out[i] < 0) out[i] += 12;
    }
    return num;
}

// quality_override < 0 means diatonic; >= 0 means override index.
int build_chord_notes(int degree, int root_midi, int voice_level,
                      int bass, int quality_override, int *out) {
    int n = 0;
    if (bass) out[n++] = root_midi - 12;
    if (voice_level == 0) {
        out[n++] = root_midi;
    } else if (quality_override >= 0) {
        const OvrVoicing *v = &OVR[resolve_ovr(quality_override, degree)][voice_level-1];
        for (int i = 0; i < v->c; i++) out[n++] = root_midi + v->n[i];
    } else {
        int intervals[6];
        int nc = compute_intervals(degree, voice_level-1, intervals);
        for (int i = 0; i < nc; i++) out[n++] = root_midi + intervals[i];
    }
    return n;
}

void make_chord_name(int key_root, int degree, int voice_level,
                     int quality_override, char *buf, int bufsz) {
    int note = (key_root + MAJOR_SCALE[degree]) % 12;
    if (voice_level == 0)
        snprintf(buf, bufsz, "%s", KEY_NAMES[note]);
    else if (quality_override >= 0)
        snprintf(buf, bufsz, "%s%s", KEY_NAMES[note],
                 OVR_NAMES[resolve_ovr(quality_override, degree)][voice_level-1]);
    else
        snprintf(buf, bufsz, "%s%s", KEY_NAMES[note],
                 quality_suffix(degree, voice_level-1));
}
