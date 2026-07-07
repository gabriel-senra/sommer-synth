#include "state.h"

const ChordKey CK[NUM_CK] = {
    {SDLK_a,0, 0,"A"},{SDLK_s,1, 0,"S"},{SDLK_d,2, 0,"D"},
    {SDLK_f,3, 0,"F"},{SDLK_g,4, 0,"G"},{SDLK_h,5, 0,"H"},{SDLK_j,6, 0,"J"},
    {SDLK_q,0,12,"Q"},{SDLK_w,1,12,"W"},{SDLK_e,2,12,"E"},
    {SDLK_r,3,12,"R"},{SDLK_t,4,12,"T"},{SDLK_y,5,12,"Y"},{SDLK_u,6,12,"U"},
    {SDLK_k,0,24,"K"},
};

// Piano-mode semitone offsets (relative to PIANO_BASE + current_key):
//   bottom row A S D F G H J = white keys C D E F G A B
//   top row    Q W E R T Y U = black keys C# D# _ F# G# A# _  (-1 = gap)
//   K = white C an octave above the bottom row's C
const int PIANO_SEMI[NUM_CK] = {
    0, 2, 4, 5, 7, 9, 11,
    1, 3, -1, 6, 8, 10, -1,
    12,
};
int     piano_mode       = 0;

int piano_midi(int ci) {
    if (PIANO_SEMI[ci] < 0) return -1;
    return PIANO_BASE + current_key + PIANO_SEMI[ci] + octave_shift * 12;
}
int piano_is_black(int ci) {
    int s = PIANO_SEMI[ci];
    return s == 1 || s == 3 || s == 6 || s == 8 || s == 10;
}

int     current_key      = 0;
int     voice_level      = 0;
int     bass_on          = 0;
int     octave_shift     = 0;
int     current_patch    = 0;
int     mouse_x          = 0;
int     mouse_y          = 0;
int     mouse_pad        = -1;
int     quality_override = -1;
RowMode lower_mode       = ROW_CHORD;
RowMode upper_mode       = ROW_CHORD;

int rev_preset = 0;
int cho_preset = 0;
int dly_preset = 0;

int      arp_on      = 0;
int      arp_bpm     = 120;
int      arp_div_idx = 2;
int      arp_octaves = 1;
int      arp_gate    = 1;
int      arp_hold    = 0;
ArpOrder arp_order   = ARP_UP;

Active   actv[NUM_CK];
int      note_count[128];
ArpState arp_st[NUM_CK];

RowMode row_mode_for(int ci) {
    return (ci >= 7 && ci < 14) ? upper_mode : lower_mode;
}

const char *row_mode_name(RowMode m) {
    switch (m) {
        case ROW_CHORD: return "CHORD";
        case ROW_BASS:  return "BASS";
        case ROW_LEAD:  return "LEAD";
    }
    return "CHORD";
}
