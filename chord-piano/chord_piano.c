// chord_piano.c — Diatonic chord player · TE brutalist UI · Arpeggiator

#include "amy.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define STB_EASY_FONT_IMPLEMENTATION
#include "../piano/stb_easy_font.h"

// ── Music theory ──────────────────────────────────────────────────────────────

static const int   MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
static const char *KEY_NAMES[12]  = {
    "C","Db","D","Eb","E","F","F#","G","Ab","A","Bb","B"
};
static const char *DEGREE_ROMAN[7] = { "I","ii","iii","IV","V","vi","vii" };
static const char *VOICE_LABELS[5] = { "SINGLE","TRIAD","7TH","9TH","11TH" };

typedef enum { Q_MAJ, Q_MIN, Q_DOM, Q_DIM } Quality;
static const Quality DEGREE_QUALITY[7] = {
    Q_MAJ, Q_MIN, Q_MIN, Q_MAJ, Q_DOM, Q_MIN, Q_DIM
};

// ── Override tables ───────────────────────────────────────────────────────────

typedef struct { int n[6]; int c; } OvrVoicing;

static const OvrVoicing OVR[8][4] = {
    /* 0 min  */ { {{0,3,7},3},      {{0,3,7,10},4},      {{0,3,7,10,14},5},     {{0,3,7,10,14,17},6} },
    /* 1 dom  */ { {{0,4,7,10},4},   {{0,4,7,10},4},      {{0,4,7,10,14},5},     {{0,4,7,10,14,17},6} },
    /* 2 dim  */ { {{0,3,6},3},      {{0,3,6,9},4},       {{0,3,6,9,13},5},      {{0,3,6,9,13,17},6}  },
    /* 3 dom9 */ { {{0,4,7,10,14},5},{{0,4,7,10,14},5},   {{0,4,7,10,14},5},     {{0,4,7,10,14,17},6} },
    /* 4 sus4 */ { {{0,5,7},3},      {{0,5,7,10},4},      {{0,5,7,10,14},5},     {{0,5,7,10,14,17},6} },
    /* 5 maj6 */ { {{0,4,7,9},4},    {{0,4,7,9},4},       {{0,4,7,9,14},5},      {{0,4,7,9,14,17},6}  },
    /* 6 aug  */ { {{0,4,8},3},      {{0,4,8,10},4},      {{0,4,8,10,14},5},     {{0,4,8,10,14,17},6} },
    /* 7 maj  */ { {{0,4,7},3},      {{0,4,7,11},4},      {{0,4,7,11,14},5},     {{0,4,7,11,14,17},6} },
};
static const char *OVR_NAMES[8][4] = {
    {"m","m7","m9","m11"}, {"7","7","9","11"},
    {"dim","dim7","dim9","dim11"}, {"9","9","9","11"},
    {"sus4","7sus4","9sus4","11sus4"}, {"6","6","6/9","6/11"},
    {"aug","aug7","aug9","aug11"}, {"","maj7","maj9","maj11"},
};
static const char *OVR_LABELS[7] = {
    "1 +-3RD","2 DOM","3 DIM","4 DOM9","5 SUS4","6 MAJ6","7 AUG"
};
static int quality_override = -1;

typedef enum { ROW_CHORD, ROW_BASS, ROW_LEAD } RowMode;
static RowMode lower_mode = ROW_CHORD;
static RowMode upper_mode = ROW_CHORD;

static int key_to_ovr(SDL_Keycode k) {
    if (k >= SDLK_1 && k <= SDLK_7) return (int)(k - SDLK_1);
    return -1;
}
static int resolve_ovr(int ovr, int degree) {
    if (ovr != 0) return ovr;
    Quality q = DEGREE_QUALITY[degree];
    return (q == Q_MIN || q == Q_DIM) ? 7 : 0;
}

// ── Quality helpers ───────────────────────────────────────────────────────────

static const char *quality_suffix(int degree, int vl_idx) {
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

static int compute_intervals(int degree, int voice_level, int *out) {
    int num = voice_level + 3;
    for (int i = 0; i < num; i++) {
        int pos = degree + i * 2;
        int idx = pos % 7, oct = (pos / 7) * 12;
        out[i] = MAJOR_SCALE[idx] - MAJOR_SCALE[degree] + oct;
        if (out[i] < 0) out[i] += 12;
    }
    return num;
}

static int build_chord_notes(int degree, int root_midi, int voice_level,
                              int bass, int *out) {
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

static void make_chord_name(int key_root, int degree, int voice_level,
                             char *buf, int bufsz) {
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

// ── Colour palette ────────────────────────────────────────────────────────────

static const SDL_Color C_BG     = { 11, 11, 11,255};
static const SDL_Color C_SURF   = { 20, 20, 20,255};
static const SDL_Color C_BORDER = { 40, 40, 40,255};
static const SDL_Color C_SEP    = { 30, 30, 30,255};
static const SDL_Color C_HI     = {228,228,228,255};
static const SDL_Color C_MID    = {110,110,110,255};
static const SDL_Color C_DIM    = { 50, 50, 50,255};
static const SDL_Color C_ACCENT = {255, 82,  0,255};
static const SDL_Color C_WHITE  = {255,255,255,255};
static const SDL_Color C_BLACK  = { 11, 11, 11,255};

static const SDL_Color OVR_COL[7] = {
    {110, 68,210,255},{255, 82,  0,255},{190, 38, 38,255},
    {195,140,  0,255},{  0,158,180,255},{  0,158, 74,255},{180, 38,138,255},
};

// ── Layout ────────────────────────────────────────────────────────────────────

#define WIN_W  900
#define WIN_H  408

#define TOP_Y  0
#define TOP_H  44
#define CTRL_Y (TOP_H + 1)
#define CTRL_H 40
#define MOD_Y  (CTRL_Y + CTRL_H + 1)
#define MOD_H  30
#define ARP_Y  (MOD_Y + MOD_H + 1)
#define ARP_H  28
#define ARP_BTN_H 20
#define ARP_BTN_Y (ARP_Y + (ARP_H - ARP_BTN_H) / 2)

#define PAD_W   116
#define PAD_H    92
#define PAD_GAP   2
#define PAD_X0   12
#define K_W      40
#define K_PAD_X  (PAD_X0 + 7*(PAD_W+PAD_GAP) + 14)

#define UPPER_LABEL_Y (ARP_Y + ARP_H + 3)
#define UPPER_PAD_Y   (UPPER_LABEL_Y + 14 + 2)
#define LOWER_LABEL_Y (UPPER_PAD_Y + PAD_H + 8)
#define LOWER_PAD_Y   (LOWER_LABEL_Y + 14 + 2)

#define MOD_BTN_W   PAD_W
#define MOD_BTN_GAP PAD_GAP
#define MOD_BTN_X0  PAD_X0
#define MOD_BTN_Y   (MOD_Y + (MOD_H - 22) / 2)
#define MOD_BTN_H   22

#define DOT_SZ   9
#define DOT_GAP  4
#define DOTS_X   90
#define DOTS_Y   (CTRL_Y + (CTRL_H - DOT_SZ) / 2)
#define KNOB_MAX 4

#define MODE_BTN_W  48
#define MODE_BTN_H  12
#define MODE_BTN_X  (WIN_W - MODE_BTN_W - 4)

// Buttons — controls row
static const SDL_Rect BTN_KEY_PREV   = { 12,  9, 24, 26};
static const SDL_Rect BTN_KEY_NEXT   = {208,  9, 24, 26};
static const SDL_Rect BTN_PATCH_PREV = {648,  9, 24, 26};
static const SDL_Rect BTN_PATCH_NEXT = {864,  9, 24, 26};
static const SDL_Rect BTN_BASS       = {398, CTRL_Y + 6, 62, 28};
static const SDL_Rect BTN_OCT_DOWN   = {478, CTRL_Y + 6, 24, 28};
static const SDL_Rect BTN_OCT_UP     = {540, CTRL_Y + 6, 24, 28};

// Buttons — arp row
static const SDL_Rect BTN_ARP_TOG     = { 12, ARP_BTN_Y, 40, ARP_BTN_H};
static const SDL_Rect BTN_ARP_DIV     = { 84, ARP_BTN_Y, 42, ARP_BTN_H};
static const SDL_Rect BTN_BPM_DOWN    = {158, ARP_BTN_Y, 20, ARP_BTN_H};
static const SDL_Rect BTN_BPM_UP      = {212, ARP_BTN_Y, 20, ARP_BTN_H};
static const SDL_Rect BTN_ARP_ORD     = {244, ARP_BTN_Y, 42, ARP_BTN_H};
static const SDL_Rect BTN_ARPOCT_DOWN = {320, ARP_BTN_Y, 20, ARP_BTN_H};
static const SDL_Rect BTN_ARPOCT_UP   = {360, ARP_BTN_Y, 20, ARP_BTN_H};
static const SDL_Rect BTN_ARP_GATE    = {392, ARP_BTN_Y, 42, ARP_BTN_H};
static const SDL_Rect BTN_ARP_HOLD    = {446, ARP_BTN_Y, 46, ARP_BTN_H};

// ── App state ─────────────────────────────────────────────────────────────────

static int current_key   = 0;
static int voice_level   = 0;
static int bass_on       = 0;
static int octave_shift  = 0;
static int current_patch = 0;
static int mouse_x       = 0;
static int mouse_y       = 0;

// ── Arpeggiator state ─────────────────────────────────────────────────────────

typedef enum { ARP_UP, ARP_DOWN, ARP_PINGPONG, ARP_RANDOM } ArpOrder;

static int      arp_on      = 0;
static int      arp_bpm     = 120;
static int      arp_div_idx = 2;      // 0=Q 1=8 2=16 3=32
static ArpOrder arp_order   = ARP_UP;
static int      arp_octaves = 1;
static int      arp_gate    = 1;      // 0=SHT 1=MED 2=LNG
static int      arp_hold    = 0;

static const int   ARP_DIV_VAL[4]   = {1, 2, 4, 8};
static const char *ARP_DIV_LBL[4]   = {"Q","8TH","16TH","32ND"};
static const char *ARP_ORD_LBL[4]   = {"UP","DWN","PING","RND"};
static const char *ARP_GATE_LBL[3]  = {"SHT","MED","LNG"};
static const float ARP_GATE_RATIO[3]= {0.2f, 0.5f, 0.9f};

typedef struct {
    int    pos;
    int    dir;
    Uint32 next_ms;
    Uint32 note_off_ms;
    int    cur_note;
    int    held;
} ArpState;

// ── Keyboard → chord map ──────────────────────────────────────────────────────

typedef struct { SDL_Keycode key; int degree; int oct_off; const char *label; } ChordKey;

static const ChordKey CK[] = {
    {SDLK_a,0,0,"A"},{SDLK_s,1,0,"S"},{SDLK_d,2,0,"D"},
    {SDLK_f,3,0,"F"},{SDLK_g,4,0,"G"},{SDLK_h,5,0,"H"},{SDLK_j,6,0,"J"},
    {SDLK_q,0,12,"Q"},{SDLK_w,1,12,"W"},{SDLK_e,2,12,"E"},
    {SDLK_r,3,12,"R"},{SDLK_t,4,12,"T"},{SDLK_y,5,12,"Y"},{SDLK_u,6,12,"U"},
    {SDLK_k,0,24,"K"},
};
#define NUM_CK ((int)(sizeof(CK)/sizeof(CK[0])))

static int ck_root_midi(int i) {
    return 48 + current_key + MAJOR_SCALE[CK[i].degree] + CK[i].oct_off
               + octave_shift * 12;
}
static int ck_bass_midi(int i) {
    return 36 + (current_key + MAJOR_SCALE[CK[i].degree]) % 12 + octave_shift * 12;
}
static int ck_lead_midi(int i) {
    return 72 + (current_key + MAJOR_SCALE[CK[i].degree]) % 12 + octave_shift * 12;
}
static int find_ck(SDL_Keycode k) {
    for (int i = 0; i < NUM_CK; i++) if (CK[i].key == k) return i;
    return -1;
}
static RowMode row_mode_for(int ci) {
    return (ci >= 7 && ci < 14) ? upper_mode : lower_mode;
}
static const char *row_mode_name(RowMode m) {
    switch (m) { case ROW_CHORD: return "CHORD"; case ROW_BASS: return "BASS"; case ROW_LEAD: return "LEAD"; }
    return "CHORD";
}

// ── Active chord tracking ─────────────────────────────────────────────────────

#define MAX_CHORD_NOTES 9
typedef struct { int active; int notes[MAX_CHORD_NOTES]; int num; } Active;
static Active    actv[NUM_CK];
static int       note_count[128];
static int       mouse_pad = -1;
static ArpState  arp_st[NUM_CK];

// ── Drawing helpers ───────────────────────────────────────────────────────────

static void set_color(SDL_Renderer *r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c) {
    set_color(r, c); SDL_RenderFillRect(r, &(SDL_Rect){x,y,w,h});
}
static void draw_rect_outline(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c) {
    set_color(r, c); SDL_RenderDrawRect(r, &(SDL_Rect){x,y,w,h});
}
static void hline(SDL_Renderer *r, int y, SDL_Color c) {
    set_color(r, c); SDL_RenderDrawLine(r, 0, y, WIN_W, y);
}
static void draw_text(SDL_Renderer *r, int x, int y, float sc,
                      const char *text, SDL_Color col) {
    static char buf[1<<17];
    int nq = stb_easy_font_print(0,0,(char*)text,NULL,buf,(int)sizeof(buf));
    set_color(r, col);
    for (int q = 0; q < nq; q++) {
        float *v = (float*)(buf + q*64);
        float x0=v[0],y0=v[1],x1=v[0],y1=v[1];
        for (int i=1;i<4;i++){float vx=v[i*4],vy=v[i*4+1];
            if(vx<x0)x0=vx; if(vx>x1)x1=vx;
            if(vy<y0)y0=vy; if(vy>y1)y1=vy;}
        SDL_Rect rc={x+(int)(x0*sc),y+(int)(y0*sc),
                     (int)((x1-x0+1)*sc),(int)((y1-y0+1)*sc)};
        SDL_RenderFillRect(r,&rc);
    }
}
static int text_width(const char *s, float sc) {
    return (int)(stb_easy_font_width((char*)s)*sc);
}
static int pt_in_rect(int px, int py, SDL_Rect re) {
    return px>=re.x&&px<re.x+re.w&&py>=re.y&&py<re.y+re.h;
}
static void draw_arrow_btn(SDL_Renderer *r, SDL_Rect btn, const char *lbl, int hov) {
    SDL_Color bg = hov ? C_SURF : C_BG;
    fill_rect(r,btn.x,btn.y,btn.w,btn.h,bg);
    draw_rect_outline(r,btn.x,btn.y,btn.w,btn.h,C_BORDER);
    draw_text(r, btn.x+(btn.w-text_width(lbl,1.5f))/2,
                 btn.y+(btn.h-(int)(13*1.5f))/2, 1.5f, lbl,
                 hov ? C_HI : C_MID);
}

// ── Voice dots ────────────────────────────────────────────────────────────────

static SDL_Rect dot_rect(int v) {
    return (SDL_Rect){DOTS_X + v*(DOT_SZ+DOT_GAP), DOTS_Y, DOT_SZ, DOT_SZ};
}
static void render_voice_dots(SDL_Renderer *r) {
    draw_text(r, 14, DOTS_Y, 1.3f, "VOICE", C_DIM);
    for (int v = 0; v <= KNOB_MAX; v++) {
        SDL_Rect d = dot_rect(v);
        SDL_Color fill = (v <= voice_level) ? C_WHITE : (SDL_Color){28,28,28,255};
        SDL_Color bord = (v == voice_level) ? C_HI    : C_BORDER;
        fill_rect(r,d.x,d.y,d.w,d.h,fill);
        draw_rect_outline(r,d.x,d.y,d.w,d.h,bord);
    }
    draw_text(r, DOTS_X+5*(DOT_SZ+DOT_GAP)+4, DOTS_Y,
              1.3f, VOICE_LABELS[voice_level], C_MID);
}

// ── AMY helpers ───────────────────────────────────────────────────────────────

static void amy_note_on_midi(int m) {
    amy_event e=amy_default_event(); e.synth=0; e.midi_note=(float)m; e.velocity=0.75f;
    amy_add_event(&e);
}
static void amy_note_off_midi(int m) {
    amy_event e=amy_default_event(); e.synth=0; e.midi_note=(float)m; e.velocity=0.0f;
    amy_add_event(&e);
}

// ── Arp helpers ───────────────────────────────────────────────────────────────

static int arp_step_ms(void) { return 60000 / arp_bpm / ARP_DIV_VAL[arp_div_idx]; }
static int arp_gate_ms(void) { return (int)(arp_step_ms() * ARP_GATE_RATIO[arp_gate]); }

static void arp_stop_note(int ci) {
    if (arp_st[ci].cur_note >= 0) {
        amy_note_off_midi(arp_st[ci].cur_note);
        arp_st[ci].cur_note = -1;
    }
}

static void arp_init_key(int ci) {
    int total = actv[ci].num * arp_octaves;
    if (total == 0) return;
    arp_st[ci].pos         = (arp_order == ARP_DOWN) ? total-1 : 0;
    arp_st[ci].dir         = 1;
    arp_st[ci].next_ms     = SDL_GetTicks();
    arp_st[ci].note_off_ms = 0;
    arp_st[ci].held        = 1;
}

static void arp_advance(int ci, int total) {
    switch (arp_order) {
        case ARP_UP:
            arp_st[ci].pos = (arp_st[ci].pos + 1) % total; break;
        case ARP_DOWN:
            arp_st[ci].pos = (arp_st[ci].pos - 1 + total) % total; break;
        case ARP_PINGPONG:
            arp_st[ci].pos += arp_st[ci].dir;
            if (arp_st[ci].pos >= total-1) { arp_st[ci].pos = total-1; arp_st[ci].dir = -1; }
            if (arp_st[ci].pos <= 0)       { arp_st[ci].pos = 0;       arp_st[ci].dir =  1; }
            break;
        case ARP_RANDOM:
            arp_st[ci].pos = rand() % total; break;
    }
}

static void arp_tick(void) {
    if (!arp_on) return;
    Uint32 now     = SDL_GetTicks();
    int    step_ms = arp_step_ms();
    int    gate_ms = arp_gate_ms();
    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active || actv[i].num == 0) continue;
        // Gate off
        if (arp_st[i].cur_note >= 0 && now >= arp_st[i].note_off_ms)
            arp_stop_note(i);
        // Step
        if (now >= arp_st[i].next_ms) {
            int total = actv[i].num * arp_octaves;
            if (total == 0) continue;
            int base = actv[i].notes[arp_st[i].pos % actv[i].num];
            int oct  = (arp_st[i].pos / actv[i].num) * 12;
            int midi = base + oct;
            if (midi >= 0 && midi <= 127) {
                arp_stop_note(i);
                amy_note_on_midi(midi);
                arp_st[i].cur_note    = midi;
                arp_st[i].note_off_ms = now + gate_ms;
            }
            arp_st[i].next_ms += step_ms;
            if (arp_st[i].next_ms + step_ms < now) arp_st[i].next_ms = now;
            arp_advance(i, total);
        }
    }
}

// ── Notes-for helper ─────────────────────────────────────────────────────────

static int notes_for(int ci, int *notes) {
    switch (row_mode_for(ci)) {
        case ROW_BASS: notes[0] = ck_bass_midi(ci); return 1;
        case ROW_LEAD: notes[0] = ck_lead_midi(ci); return 1;
        default:       return build_chord_notes(CK[ci].degree, ck_root_midi(ci),
                                                voice_level, bass_on, notes);
    }
}

// ── Load patch ────────────────────────────────────────────────────────────────

static void load_patch(int patch) {
    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active) continue;
        if (arp_on) arp_stop_note(i);
        else for (int n = 0; n < actv[i].num; n++) amy_note_off_midi(actv[i].notes[n]);
        actv[i].active = 0; actv[i].num = 0;
    }
    memset(note_count, 0, sizeof(note_count));
    for (int i = 0; i < NUM_CK; i++) arp_st[i].cur_note = -1;
    current_patch      = patch;
    amy_event e        = amy_default_event();
    e.synth = 0; e.num_voices = 32; e.patch_number = (uint16_t)patch;
    amy_add_event(&e);
}

// ── Chord trigger / release ───────────────────────────────────────────────────

static void trigger_chord(int ci) {
    // Hold mode: second press stops the arp
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

static void release_chord(int ci) {
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

static void retrigger_all_active(void) {
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
                if (m<0||m>127) continue;
                if (--note_count[m]<=0){note_count[m]=0;amy_note_off_midi(m);}
            }
            actv[i].num = nn;
            memcpy(actv[i].notes, notes, nn * sizeof(int));
            for (int n = 0; n < nn; n++) {
                int m = notes[n];
                if (m<0||m>127) continue;
                if (note_count[m]==0) amy_note_on_midi(m);
                note_count[m]++;
            }
        }
    }
}

static void toggle_arp(void) {
    // Silence everything cleanly before switching modes
    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active) continue;
        if (arp_on) arp_stop_note(i);
        else for (int n=0;n<actv[i].num;n++) {
            int m=actv[i].notes[n];
            if(m<0||m>127) continue;
            if(--note_count[m]<=0){note_count[m]=0;amy_note_off_midi(m);}
        }
        actv[i].active=0; actv[i].num=0;
    }
    memset(note_count,0,sizeof(note_count));
    for (int i=0;i<NUM_CK;i++) arp_st[i].cur_note=-1;
    arp_on ^= 1;
}

// ── Pad geometry ──────────────────────────────────────────────────────────────

static SDL_Rect pad_rect(int i) {
    if (i < 7)  return (SDL_Rect){PAD_X0 + i*(PAD_W+PAD_GAP), LOWER_PAD_Y, PAD_W, PAD_H};
    if (i < 14) return (SDL_Rect){PAD_X0 + (i-7)*(PAD_W+PAD_GAP), UPPER_PAD_Y, PAD_W, PAD_H};
    return (SDL_Rect){K_PAD_X, LOWER_PAD_Y, K_W, PAD_H};
}
static int hit_pad(int mx, int my) {
    for (int i=0;i<NUM_CK;i++) if (pt_in_rect(mx,my,pad_rect(i))) return i;
    return -1;
}
static int hit_dot(int mx, int my) {
    for (int v=0;v<=KNOB_MAX;v++) { SDL_Rect d=dot_rect(v); if(pt_in_rect(mx,my,d)) return v; }
    return -1;
}
static int hit_row_mode_btn(int mx, int my) {
    SDL_Rect u={MODE_BTN_X,UPPER_LABEL_Y-1,MODE_BTN_W,MODE_BTN_H};
    SDL_Rect l={MODE_BTN_X,LOWER_LABEL_Y-1,MODE_BTN_W,MODE_BTN_H};
    if (pt_in_rect(mx,my,u)) return 0;
    if (pt_in_rect(mx,my,l)) return 1;
    return -1;
}

// ── Rendering ─────────────────────────────────────────────────────────────────

static void render_topbar(SDL_Renderer *r) {
    fill_rect(r,0,0,WIN_W,TOP_H,C_BG);
    hline(r,TOP_H,C_SEP);
    int kp=pt_in_rect(mouse_x,mouse_y,BTN_KEY_PREV);
    int kn=pt_in_rect(mouse_x,mouse_y,BTN_KEY_NEXT);
    draw_arrow_btn(r,BTN_KEY_PREV,"<",kp);
    draw_arrow_btn(r,BTN_KEY_NEXT,">",kn);
    char key_lbl[20]; snprintf(key_lbl,sizeof(key_lbl),"%s MAJOR",KEY_NAMES[current_key]);
    float ksc=2.2f; int klw=text_width(key_lbl,ksc);
    int klx=BTN_KEY_PREV.x+BTN_KEY_PREV.w+(BTN_KEY_NEXT.x-BTN_KEY_PREV.x-BTN_KEY_PREV.w-klw)/2;
    draw_text(r,klx,TOP_H/2-(int)(13*ksc)/2+1,ksc,key_lbl,C_WHITE);
    int pp=pt_in_rect(mouse_x,mouse_y,BTN_PATCH_PREV);
    int pn=pt_in_rect(mouse_x,mouse_y,BTN_PATCH_NEXT);
    draw_arrow_btn(r,BTN_PATCH_PREV,"<",pp);
    draw_arrow_btn(r,BTN_PATCH_NEXT,">",pn);
    char ptch[16]; snprintf(ptch,sizeof(ptch),"#%03d",current_patch);
    draw_text(r,BTN_PATCH_PREV.x-58,TOP_H/2-(int)(13*1.3f)/2+1,1.3f,"PATCH",C_DIM);
    float psc=1.8f; int plw=text_width(ptch,psc);
    int plx=BTN_PATCH_PREV.x+BTN_PATCH_PREV.w+(BTN_PATCH_NEXT.x-BTN_PATCH_PREV.x-BTN_PATCH_PREV.w-plw)/2;
    draw_text(r,plx,TOP_H/2-(int)(13*psc)/2+1,psc,ptch,C_MID);
}

static void render_controls(SDL_Renderer *r) {
    fill_rect(r,0,CTRL_Y,WIN_W,CTRL_H,C_BG);
    hline(r,CTRL_Y+CTRL_H,C_SEP);
    render_voice_dots(r);
    // BASS
    SDL_Rect bb=BTN_BASS; int bh=pt_in_rect(mouse_x,mouse_y,bb);
    SDL_Color bf=bass_on?C_ACCENT:(bh?C_SURF:C_BG);
    SDL_Color bbord=bass_on?C_ACCENT:C_BORDER;
    SDL_Color bt=bass_on?C_WHITE:C_MID;
    fill_rect(r,bb.x,bb.y,bb.w,bb.h,bf);
    draw_rect_outline(r,bb.x,bb.y,bb.w,bb.h,bbord);
    draw_text(r,bb.x+(bb.w-text_width("BASS",1.5f))/2,
                bb.y+(bb.h-(int)(13*1.5f))/2,1.5f,"BASS",bt);
    // OCT
    draw_text(r,BTN_OCT_DOWN.x,BTN_OCT_DOWN.y-12,1.2f,"OCT",C_DIM);
    draw_arrow_btn(r,BTN_OCT_DOWN,"-",pt_in_rect(mouse_x,mouse_y,BTN_OCT_DOWN));
    draw_arrow_btn(r,BTN_OCT_UP,  "+",pt_in_rect(mouse_x,mouse_y,BTN_OCT_UP));
    char oct_str[8]; snprintf(oct_str,sizeof(oct_str),"%+d",octave_shift);
    SDL_Color oc=octave_shift?C_HI:C_MID;
    int otw=text_width(oct_str,1.8f);
    int ocx=BTN_OCT_DOWN.x+BTN_OCT_DOWN.w+(BTN_OCT_UP.x-BTN_OCT_DOWN.x-BTN_OCT_DOWN.w-otw)/2;
    draw_text(r,ocx,CTRL_Y+(CTRL_H-(int)(13*1.8f))/2,1.8f,oct_str,oc);
    // Active chord name
    for (int i=0;i<NUM_CK;i++) {
        if (!actv[i].active) continue;
        char cname[24]; make_chord_name(current_key,CK[i].degree,voice_level,cname,sizeof(cname));
        float csc=2.8f;
        SDL_Color cc=quality_override>=0?OVR_COL[quality_override]:C_ACCENT;
        draw_text(r,530,CTRL_Y+(CTRL_H-(int)(13*csc))/2,csc,cname,cc);
        break;
    }
}

static void render_modifier_strip(SDL_Renderer *r) {
    fill_rect(r,0,MOD_Y,WIN_W,MOD_H,C_BG);
    hline(r,MOD_Y+MOD_H,C_SEP);
    for (int i=0;i<7;i++) {
        SDL_Rect btn={MOD_BTN_X0+i*(MOD_BTN_W+MOD_BTN_GAP),MOD_BTN_Y,MOD_BTN_W,MOD_BTN_H};
        int hov=pt_in_rect(mouse_x,mouse_y,btn), act=(quality_override==i);
        SDL_Color fill=act?OVR_COL[i]:(hov?C_SURF:C_BG);
        SDL_Color bord=act?OVR_COL[i]:(hov?C_BORDER:C_DIM);
        SDL_Color text=act?C_WHITE:(hov?C_MID:C_DIM);
        fill_rect(r,btn.x,btn.y,btn.w,btn.h,fill);
        draw_rect_outline(r,btn.x,btn.y,btn.w,btn.h,bord);
        draw_text(r,btn.x+(btn.w-text_width(OVR_LABELS[i],1.3f))/2,
                    btn.y+(btn.h-(int)(13*1.3f))/2,1.3f,OVR_LABELS[i],text);
    }
}

static void render_arp_row(SDL_Renderer *r) {
    fill_rect(r,0,ARP_Y,WIN_W,ARP_H,C_BG);
    hline(r,ARP_Y+ARP_H,C_SEP);
    int ty = ARP_BTN_Y + (ARP_BTN_H-(int)(13*1.3f))/2;

    // [ARP] toggle
    {
        SDL_Rect btn=BTN_ARP_TOG;
        SDL_Color fill=arp_on?C_ACCENT:(pt_in_rect(mouse_x,mouse_y,btn)?C_SURF:C_BG);
        SDL_Color bord=arp_on?C_ACCENT:C_BORDER;
        SDL_Color tc=arp_on?C_WHITE:C_MID;
        fill_rect(r,btn.x,btn.y,btn.w,btn.h,fill);
        draw_rect_outline(r,btn.x,btn.y,btn.w,btn.h,bord);
        draw_text(r,btn.x+(btn.w-text_width("ARP",1.3f))/2,ty,1.3f,"ARP",tc);
    }

    if (!arp_on) {
        draw_text(r,62,ty,1.2f,"ARPEGGIATE OFF",C_DIM);
        return;
    }

    // DIV
    draw_text(r,62,ty,1.2f,"DIV",C_DIM);
    { SDL_Rect btn=BTN_ARP_DIV; int hov=pt_in_rect(mouse_x,mouse_y,btn);
      fill_rect(r,btn.x,btn.y,btn.w,btn.h,hov?C_SURF:C_BG);
      draw_rect_outline(r,btn.x,btn.y,btn.w,btn.h,C_BORDER);
      const char *lbl=ARP_DIV_LBL[arp_div_idx];
      draw_text(r,btn.x+(btn.w-text_width(lbl,1.3f))/2,ty,1.3f,lbl,C_HI); }

    // BPM
    draw_text(r,136,ty,1.2f,"BPM",C_DIM);
    draw_arrow_btn(r,BTN_BPM_DOWN,"<",pt_in_rect(mouse_x,mouse_y,BTN_BPM_DOWN));
    draw_arrow_btn(r,BTN_BPM_UP,  ">",pt_in_rect(mouse_x,mouse_y,BTN_BPM_UP));
    { char s[8]; snprintf(s,sizeof(s),"%d",arp_bpm);
      int tw=text_width(s,1.5f);
      int cx=BTN_BPM_DOWN.x+BTN_BPM_DOWN.w+(BTN_BPM_UP.x-BTN_BPM_DOWN.x-BTN_BPM_DOWN.w-tw)/2;
      draw_text(r,cx,ty,1.5f,s,C_WHITE); }

    // ORDER
    { SDL_Rect btn=BTN_ARP_ORD; int hov=pt_in_rect(mouse_x,mouse_y,btn);
      fill_rect(r,btn.x,btn.y,btn.w,btn.h,hov?C_SURF:C_BG);
      draw_rect_outline(r,btn.x,btn.y,btn.w,btn.h,C_BORDER);
      const char *lbl=ARP_ORD_LBL[(int)arp_order];
      draw_text(r,btn.x+(btn.w-text_width(lbl,1.3f))/2,ty,1.3f,lbl,C_HI); }

    // ARP OCT
    draw_text(r,298,ty,1.2f,"OCT",C_DIM);
    draw_arrow_btn(r,BTN_ARPOCT_DOWN,"<",pt_in_rect(mouse_x,mouse_y,BTN_ARPOCT_DOWN));
    draw_arrow_btn(r,BTN_ARPOCT_UP,  ">",pt_in_rect(mouse_x,mouse_y,BTN_ARPOCT_UP));
    { char s[4]; snprintf(s,sizeof(s),"%d",arp_octaves);
      int tw=text_width(s,1.5f);
      int cx=BTN_ARPOCT_DOWN.x+BTN_ARPOCT_DOWN.w+(BTN_ARPOCT_UP.x-BTN_ARPOCT_DOWN.x-BTN_ARPOCT_DOWN.w-tw)/2;
      draw_text(r,cx,ty,1.5f,s,C_WHITE); }

    // GATE
    { SDL_Rect btn=BTN_ARP_GATE; int hov=pt_in_rect(mouse_x,mouse_y,btn);
      fill_rect(r,btn.x,btn.y,btn.w,btn.h,hov?C_SURF:C_BG);
      draw_rect_outline(r,btn.x,btn.y,btn.w,btn.h,C_BORDER);
      const char *lbl=ARP_GATE_LBL[arp_gate];
      draw_text(r,btn.x+(btn.w-text_width(lbl,1.3f))/2,ty,1.3f,lbl,C_HI); }

    // [HOLD]
    { SDL_Rect btn=BTN_ARP_HOLD;
      SDL_Color fill=arp_hold?C_WHITE:(pt_in_rect(mouse_x,mouse_y,btn)?C_SURF:C_BG);
      SDL_Color bord=arp_hold?C_WHITE:C_BORDER;
      SDL_Color tc=arp_hold?C_BLACK:C_MID;
      fill_rect(r,btn.x,btn.y,btn.w,btn.h,fill);
      draw_rect_outline(r,btn.x,btn.y,btn.w,btn.h,bord);
      draw_text(r,btn.x+(btn.w-text_width("HOLD",1.3f))/2,ty,1.3f,"HOLD",tc); }
}

static const SDL_Color MODE_COL[3]      = {{40,40,40,255},{20,50,90,255},{80,50,10,255}};
static const SDL_Color MODE_TEXT_COL[3] = {{50,50,50,255},{0,120,210,255},{200,120,0,255}};

static void render_mode_badge(SDL_Renderer *r, int row_y, RowMode mode) {
    SDL_Rect btn={MODE_BTN_X,row_y-1,MODE_BTN_W,MODE_BTN_H};
    int hov=pt_in_rect(mouse_x,mouse_y,btn);
    fill_rect(r,btn.x,btn.y,btn.w,btn.h,hov?C_SURF:(SDL_Color){22,22,22,255});
    draw_rect_outline(r,btn.x,btn.y,btn.w,btn.h,
                      mode==ROW_CHORD?C_BORDER:MODE_COL[(int)mode]);
    const char *lbl=row_mode_name(mode);
    SDL_Color tc=hov?C_HI:MODE_TEXT_COL[(int)mode];
    draw_text(r,btn.x+(btn.w-text_width(lbl,1.2f))/2,
                btn.y+(btn.h-(int)(13*1.2f))/2+1,1.2f,lbl,tc);
}

static void render_pads(SDL_Renderer *r) {
    draw_text(r,PAD_X0,UPPER_LABEL_Y,1.2f,"C4   Q W E R T Y U",C_DIM);
    render_mode_badge(r,UPPER_LABEL_Y,upper_mode);
    draw_text(r,PAD_X0,LOWER_LABEL_Y,1.2f,"C3   A S D F G H J",C_DIM);
    draw_text(r,K_PAD_X,LOWER_LABEL_Y,1.2f,"C5 K",C_DIM);
    render_mode_badge(r,LOWER_LABEL_Y,lower_mode);

    for (int i=0;i<NUM_CK;i++) {
        SDL_Rect pad=pad_rect(i);
        int held=actv[i].active, hov=pt_in_rect(mouse_x,mouse_y,pad);
        RowMode mode=row_mode_for(i);

        SDL_Color fill,bord,text_col,sub_col;
        if (held && quality_override>=0 && mode==ROW_CHORD) {
            SDL_Color oc=OVR_COL[quality_override];
            fill=oc; bord=C_WHITE; text_col=C_WHITE;
            sub_col=(SDL_Color){oc.r/2+127,oc.g/2+127,oc.b/2+127,255};
        } else if (held) {
            fill=C_WHITE; bord=C_WHITE; text_col=C_BLACK;
            sub_col=(SDL_Color){80,80,80,255};
        } else if (hov) {
            fill=C_SURF; bord=C_BORDER; text_col=C_HI; sub_col=C_DIM;
        } else {
            fill=C_BG;
            bord=(mode==ROW_CHORD)?C_BORDER:MODE_COL[(int)mode];
            text_col=C_MID; sub_col=C_DIM;
        }
        fill_rect(r,pad.x,pad.y,pad.w,pad.h,fill);
        draw_rect_outline(r,pad.x,pad.y,pad.w,pad.h,bord);

        char cname[24];
        if (mode==ROW_CHORD)
            make_chord_name(current_key,CK[i].degree,voice_level,cname,sizeof(cname));
        else
            snprintf(cname,sizeof(cname),"%s",KEY_NAMES[(current_key+MAJOR_SCALE[CK[i].degree])%12]);
        float csc=((int)strlen(cname)>5)?1.6f:2.0f;
        draw_text(r,pad.x+(pad.w-text_width(cname,csc))/2,
                    pad.y+(pad.h-(int)(13*csc))/2-6,csc,cname,text_col);

        if (mode!=ROW_CHORD) {
            SDL_Color mc=held?(SDL_Color){100,100,100,255}:MODE_TEXT_COL[(int)mode];
            draw_text(r,pad.x+4,pad.y+4,1.1f,mode==ROW_BASS?"BAS":"LED",mc);
        }
        // Arp indicator on active pad
        if (held && arp_on) {
            SDL_Color ac=text_col.r<100?(SDL_Color){200,200,200,255}:(SDL_Color){80,80,80,255};
            draw_text(r,pad.x+4,pad.y+pad.h-28,1.1f,"ARP",ac);
        }
        draw_text(r,pad.x+5,pad.y+pad.h-16,1.2f,DEGREE_ROMAN[CK[i].degree],sub_col);
        draw_text(r,pad.x+pad.w-text_width(CK[i].label,1.2f)-5,
                    pad.y+pad.h-16,1.2f,CK[i].label,sub_col);
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    srand((unsigned)SDL_GetTicks());
    for (int i=0;i<NUM_CK;i++) arp_st[i].cur_note=-1;

    amy_config_t cfg=amy_default_config();
    cfg.audio=AMY_AUDIO_IS_MINIAUDIO; cfg.features.default_synths=0;
    amy_start(cfg);
    load_patch(0);

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)<0){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
    SDL_Window   *win=SDL_CreateWindow("AMY Chord Piano",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIN_W,WIN_H,SDL_WINDOW_SHOWN);
    SDL_Renderer *ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!win||!ren){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}

    int running=1;
    while (running) {
        arp_tick();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: running=0; break;
            case SDL_MOUSEMOTION: mouse_x=ev.motion.x; mouse_y=ev.motion.y; break;

            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button==SDL_BUTTON_LEFT) {
                    mouse_x=ev.button.x; mouse_y=ev.button.y;
                    if      (pt_in_rect(mouse_x,mouse_y,BTN_KEY_PREV))  current_key=(current_key+11)%12;
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_KEY_NEXT))  current_key=(current_key+1)%12;
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_PATCH_PREV)&&current_patch>0)   load_patch(current_patch-1);
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_PATCH_NEXT)&&current_patch<255) load_patch(current_patch+1);
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_BASS))      bass_on^=1;
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_OCT_DOWN)&&octave_shift>-2){octave_shift--;retrigger_all_active();}
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_OCT_UP)  &&octave_shift< 2){octave_shift++;retrigger_all_active();}
                    // Arp row
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_ARP_TOG))   toggle_arp();
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_ARP_DIV))   arp_div_idx=(arp_div_idx+1)%4;
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_BPM_DOWN)&&arp_bpm>40)  arp_bpm-=5;
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_BPM_UP)  &&arp_bpm<240) arp_bpm+=5;
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_ARP_ORD))   arp_order=(ArpOrder)((arp_order+1)%4);
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_ARPOCT_DOWN)&&arp_octaves>1) arp_octaves--;
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_ARPOCT_UP)  &&arp_octaves<3) arp_octaves++;
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_ARP_GATE))  arp_gate=(arp_gate+1)%3;
                    else if (pt_in_rect(mouse_x,mouse_y,BTN_ARP_HOLD))  arp_hold^=1;
                    else {
                        int dv=hit_dot(mouse_x,mouse_y);
                        if (dv>=0) { voice_level=dv; break; }
                        int rm=hit_row_mode_btn(mouse_x,mouse_y);
                        if (rm==0){upper_mode=(RowMode)((upper_mode+1)%3);retrigger_all_active();break;}
                        if (rm==1){lower_mode=(RowMode)((lower_mode+1)%3);retrigger_all_active();break;}
                        int pi=hit_pad(mouse_x,mouse_y);
                        if (pi>=0){trigger_chord(pi);mouse_pad=pi;}
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (ev.button.button==SDL_BUTTON_LEFT&&mouse_pad>=0){
                    release_chord(mouse_pad); mouse_pad=-1;
                }
                break;

            case SDL_MOUSEWHEEL: {
                int delta=(ev.wheel.y>0)?1:-1;
                int np=current_patch-delta;
                if (np>=0&&np<=255) load_patch(np);
                break;
            }

            case SDL_KEYDOWN:
                if (ev.key.repeat) break;
                switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: running=0; break;
                case SDLK_LEFT:   current_key=(current_key+11)%12; break;
                case SDLK_RIGHT:  current_key=(current_key+1)%12; break;
                case SDLK_UP:     if(voice_level<KNOB_MAX)voice_level++; break;
                case SDLK_DOWN:   if(voice_level>0)voice_level--; break;
                case SDLK_b:      bass_on^=1; break;
                case SDLK_LEFTBRACKET:  if(octave_shift>-2){octave_shift--;retrigger_all_active();}break;
                case SDLK_RIGHTBRACKET: if(octave_shift< 2){octave_shift++;retrigger_all_active();}break;
                case SDLK_BACKSLASH:    toggle_arp(); break;
                default: {
                    int oi=key_to_ovr(ev.key.keysym.sym);
                    if (oi>=0){quality_override=oi;retrigger_all_active();break;}
                    int ci=find_ck(ev.key.keysym.sym);
                    if (ci>=0) trigger_chord(ci);
                } break;
                }
                break;

            case SDL_KEYUP: {
                int oi=key_to_ovr(ev.key.keysym.sym);
                if (oi>=0&&quality_override==oi){quality_override=-1;retrigger_all_active();break;}
                int ci=find_ck(ev.key.keysym.sym);
                if (ci>=0) release_chord(ci);
                break;
            }
            }
        }

        fill_rect(ren,0,0,WIN_W,WIN_H,C_BG);
        render_topbar(ren);
        render_controls(ren);
        render_modifier_strip(ren);
        render_arp_row(ren);
        render_pads(ren);
        SDL_RenderPresent(ren);
    }

    for (int i=0;i<NUM_CK;i++){arp_stop_note(i);release_chord(i);}
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win);
    SDL_Quit(); amy_stop();
    return 0;
}
