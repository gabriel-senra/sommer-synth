// piano.c — Interactive piano with AMY Juno presets and SDL2 UI

#include "amy.h"
#include <SDL.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

// ─────────────────────────────────────────────
// Patch names (0-127 = Juno, 128-255 = DX7)
// ─────────────────────────────────────────────
#define NUM_PATCHES 256
static const char *patch_names[NUM_PATCHES] = {
    "Brass Set 1",              //  0
    "Brass Swell",              //  1
    "Trumpet",                  //  2
    "Flutes",                   //  3
    "Moving Strings",           //  4
    "Brass & Strings",          //  5
    "Choir",                    //  6
    "Piano I",                  //  7
    "Organ I",                  //  8
    "Organ II",                 //  9
    "Combo Organ",              // 10
    "Calliope",                 // 11
    "Donald Pluck",             // 12
    "Celeste (1 oct up)",       // 13
    "Elect. Piano I",           // 14
    "Elect. Piano II",          // 15
    "Clock Chimes (1 oct dn)",  // 16
    "Steel Drums",              // 17
    "Xylophone",                // 18
    "Brass III",                // 19
    "Fanfare",                  // 20
    "String III",               // 21
    "Pizzicato",                // 22
    "High Strings",             // 23
    "Bass Clarinet",            // 24
    "English Horn",             // 25
    "Brass Ensemble",           // 26
    "Guitar",                   // 27
    "Koto",                     // 28
    "Dark Pluck",               // 29
    "Funky I",                  // 30
    "Synth Bass I (unison)",    // 31
    "Lead I",                   // 32
    "Lead II",                  // 33
    "Lead III",                 // 34
    "Funky II",                 // 35
    "Synth Bass II",            // 36
    "Funky III",                // 37
    "Thud Wah",                 // 38
    "Going Up",                 // 39
    "Piano II",                 // 40
    "Clav",                     // 41
    "Frontier Organ",           // 42
    "Snare Drum (unison)",      // 43
    "Tom Toms (unison)",        // 44
    "Timpani (unison)",         // 45
    "Shaker",                   // 46
    "Synth Pad",                // 47
    "Sweep I",                  // 48
    "Pluck Sweep",              // 49
    "Repeater",                 // 50
    "Sweep II",                 // 51
    "Pluck Bell",               // 52
    "Dark Synth Piano",         // 53
    "Sustainer",                // 54
    "Wah Release",              // 55
    "Gong",                     // 56
    "Resonance Funk",           // 57
    "Drum Booms (1 oct dn)",    // 58
    "Dust Storm",               // 59
    "Rocket Men",               // 60
    "Hand Claps",               // 61
    "FX Sweep",                 // 62
    "Caverns",                  // 63
    "Strings",                  // 64
    "Violin",                   // 65
    "Chorus Vibes",             // 66
    "Organ 1",                  // 67
    "Harpsichord 1",            // 68
    "Recorder",                 // 69
    "Perc. Pluck",              // 70
    "Noise Sweep",              // 71
    "Space Chimes",             // 72
    "Nylon Guitar",             // 73
    "Orchestral Pad",           // 74
    "Bright Pluck",             // 75
    "Organ Bell",               // 76
    "Accordion",                // 77
    "FX Rise 1",                // 78
    "FX Rise 2",                // 79
    "Brass",                    // 80
    "Helicopter",               // 81
    "Lute",                     // 82
    "Chorus Funk",              // 83
    "Tomita",                   // 84
    "FX Sweep 1",               // 85
    "Sharp Reed",               // 86
    "Bass Pluck",               // 87
    "Resonant Rise",            // 88
    "Harpsichord 2",            // 89
    "Dark Ensemble",            // 90
    "Contact Wah",              // 91
    "Noise Sweep 2",            // 92
    "Glassy Wah",               // 93
    "Phase Ensemble",           // 94
    "Chorused Bell",            // 95
    "Clav 2",                   // 96
    "Organ 2",                  // 97
    "Bassoon",                  // 98
    "Auto Release Noise Sweep", // 99
    "Brass Ensemble 2",         // 100
    "Ethereal",                 // 101
    "Chorus Bell 2",            // 102
    "Blizzard",                 // 103
    "E. Piano w/ Tremolo",      // 104
    "Clarinet",                 // 105
    "Thunder",                  // 106
    "Reedy Organ",              // 107
    "Flute / Horn",             // 108
    "Toy Rhodes",               // 109
    "Surf's Up",                // 110
    "OW Bass",                  // 111
    "Piccolo",                  // 112
    "Melodic Taps",             // 113
    "Meow Brass",               // 114
    "Violin (high)",            // 115
    "High Bells",               // 116
    "Rolling Wah",              // 117
    "Ping Bell",                // 118
    "Brassy Organ",             // 119
    "Low Dark Strings",         // 120
    "Piccolo Trumpet",          // 121
    "Cello",                    // 122
    "High Strings",             // 123
    "Rocket Men 2",             // 124
    "Forbidden Planet",         // 125
    "Froggy",                   // 126
    "Owgan",                    // 127
    // DX7 (128-255)
    "DX7 Brass 1",   "DX7 Brass 2",   "DX7 Brass 3",
    "DX7 Strings 1", "DX7 Strings 2", "DX7 Strings 3",
    "DX7 Piano 1",   "DX7 Piano 2",   "DX7 Piano 3",
    "DX7 E.Piano 1", "DX7 E.Piano 2", "DX7 E.Piano 3",
    "DX7 Organ 1",   "DX7 Organ 2",   "DX7 Organ 3",
    "DX7 Voice 1",   "DX7 Voice 2",   "DX7 Voice 3",
    "DX7 Flute 1",   "DX7 Flute 2",   "DX7 Flute 3",
    "DX7 Clarinet 1","DX7 Clarinet 2","DX7 Clarinet 3",
    "DX7 Guitar 1",  "DX7 Guitar 2",  "DX7 Guitar 3",
    "DX7 Bass 1",    "DX7 Bass 2",    "DX7 Bass 3",
    "DX7 Perc 1",    "DX7 Perc 2",    "DX7 Perc 3",
    "DX7 SFX 1",     "DX7 SFX 2",     "DX7 SFX 3",
    "DX7 Synth 1",   "DX7 Synth 2",   "DX7 Synth 3",
    "DX7 Bell 1",    "DX7 Bell 2",    "DX7 Bell 3",
    "DX7 Pad 1",     "DX7 Pad 2",     "DX7 Pad 3",
    "DX7 Lead 1",    "DX7 Lead 2",    "DX7 Lead 3",
    "DX7 Patch 48",  "DX7 Patch 49",  "DX7 Patch 50",
    "DX7 Patch 51",  "DX7 Patch 52",  "DX7 Patch 53",
    "DX7 Patch 54",  "DX7 Patch 55",  "DX7 Patch 56",
    "DX7 Patch 57",  "DX7 Patch 58",  "DX7 Patch 59",
    "DX7 Patch 60",  "DX7 Patch 61",  "DX7 Patch 62",
    "DX7 Patch 63",  "DX7 Patch 64",  "DX7 Patch 65",
    "DX7 Patch 66",  "DX7 Patch 67",  "DX7 Patch 68",
    "DX7 Patch 69",  "DX7 Patch 70",  "DX7 Patch 71",
    "DX7 Patch 72",  "DX7 Patch 73",  "DX7 Patch 74",
    "DX7 Patch 75",  "DX7 Patch 76",  "DX7 Patch 77",
    "DX7 Patch 78",  "DX7 Patch 79",  "DX7 Patch 80",
    "DX7 Patch 81",  "DX7 Patch 82",  "DX7 Patch 83",
    "DX7 Patch 84",  "DX7 Patch 85",  "DX7 Patch 86",
    "DX7 Patch 87",  "DX7 Patch 88",  "DX7 Patch 89",
    "DX7 Patch 90",  "DX7 Patch 91",  "DX7 Patch 92",
    "DX7 Patch 93",  "DX7 Patch 94",  "DX7 Patch 95",
    "DX7 Patch 96",  "DX7 Patch 97",  "DX7 Patch 98",
    "DX7 Patch 99",  "DX7 Patch 100", "DX7 Patch 101",
    "DX7 Patch 102", "DX7 Patch 103", "DX7 Patch 104",
    "DX7 Patch 105", "DX7 Patch 106", "DX7 Patch 107",
    "DX7 Patch 108", "DX7 Patch 109", "DX7 Patch 110",
    "DX7 Patch 111", "DX7 Patch 112", "DX7 Patch 113",
    "DX7 Patch 114", "DX7 Patch 115", "DX7 Patch 116",
    "DX7 Patch 117", "DX7 Patch 118", "DX7 Patch 119",
    "DX7 Patch 120", "DX7 Patch 121", "DX7 Patch 122",
    "DX7 Patch 123", "DX7 Patch 124", "DX7 Patch 125",
    "DX7 Patch 126", "DX7 Patch 127",
};

// ─────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────
#define WIN_W        900
#define WIN_H        430
#define HEADER_H     120
#define KB_X         30        // keyboard left edge
#define KB_Y         (HEADER_H + 8)
#define WK_W         40        // white key width
#define WK_H         (WIN_H - KB_Y - 8)   // white key height fills to bottom
#define BK_W         26        // black key width
#define BK_H         (WK_H * 60 / 100)    // 60% of white key height
#define OCTAVES      3
#define START_NOTE   48        // C3

// Semitone -> white key index within octave  (C=0..B=6 positions)
static const int WHITE_SEMI[7]  = {0, 2, 4, 5, 7, 9, 11};
// Black key semitones within an octave
static const int BLACK_SEMI[5]  = {1, 3, 6, 8, 10};
// Black key x offset from octave left edge (WK_W=40 assumed)
static const int BLACK_OFF[5]   = {28, 68, 148, 188, 228};

// Prev / Next button rects
static const SDL_Rect BTN_PREV = {20,  44, 100, 38};
static const SDL_Rect BTN_NEXT = {780, 44, 100, 38};

// ─────────────────────────────────────────────
// Keyboard → MIDI mapping  (standard piano layout)
//   white keys: a s d f g h j k l ;
//   black keys: w e   t y u   o p
// ─────────────────────────────────────────────
typedef struct { SDL_Keycode key; int midi; const char *label; } KeyMap;
static const KeyMap KEY_MAP[] = {
    { SDLK_a,         48, "a"  }, // C3
    { SDLK_w,         49, "w"  }, // C#3
    { SDLK_s,         50, "s"  }, // D3
    { SDLK_e,         51, "e"  }, // D#3
    { SDLK_d,         52, "d"  }, // E3
    { SDLK_f,         53, "f"  }, // F3
    { SDLK_t,         54, "t"  }, // F#3
    { SDLK_g,         55, "g"  }, // G3
    { SDLK_y,         56, "y"  }, // G#3
    { SDLK_h,         57, "h"  }, // A3
    { SDLK_u,         58, "u"  }, // A#3
    { SDLK_j,         59, "j"  }, // B3
    { SDLK_k,         60, "k"  }, // C4
    { SDLK_o,         61, "o"  }, // C#4
    { SDLK_l,         62, "l"  }, // D4
    { SDLK_p,         63, "p"  }, // D#4
    { SDLK_SEMICOLON, 64, ";"  }, // E4
};
#define KEY_MAP_SIZE ((int)(sizeof(KEY_MAP) / sizeof(KEY_MAP[0])))

static int key_to_midi(SDL_Keycode k) {
    for (int i = 0; i < KEY_MAP_SIZE; i++)
        if (KEY_MAP[i].key == k) return KEY_MAP[i].midi;
    return -1;
}

static const char *midi_to_keylabel(int midi) {
    for (int i = 0; i < KEY_MAP_SIZE; i++)
        if (KEY_MAP[i].midi == midi) return KEY_MAP[i].label;
    return NULL;
}

// ─────────────────────────────────────────────
// App state
// ─────────────────────────────────────────────
static int  current_patch = 0;
static int  held_note     = -1;   // mouse-held note
static int  key_down[128] = {0};  // which MIDI notes are currently sounding
static int  mouse_x       = 0;
static int  mouse_y       = 0;

// ─────────────────────────────────────────────
// Text rendering via stb_easy_font
// ─────────────────────────────────────────────
static void draw_text(SDL_Renderer *r, int x, int y, float scale,
                      const char *text, SDL_Color col)
{
    static char buf[1 << 17];
    int num_quads = stb_easy_font_print(0, 0, (char *)text, NULL,
                                        buf, (int)sizeof(buf));
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    for (int q = 0; q < num_quads; q++) {
        float *v = (float *)(buf + q * 64);
        float x0 = v[0], y0 = v[1];
        float x1 = v[0], y1 = v[1];
        for (int i = 1; i < 4; i++) {
            float vx = v[i * 4], vy = v[i * 4 + 1];
            if (vx < x0) x0 = vx;
            if (vx > x1) x1 = vx;
            if (vy < y0) y0 = vy;
            if (vy > y1) y1 = vy;
        }
        SDL_Rect rc = {
            x + (int)(x0 * scale),
            y + (int)(y0 * scale),
            (int)((x1 - x0 + 1) * scale),
            (int)((y1 - y0 + 1) * scale)
        };
        SDL_RenderFillRect(r, &rc);
    }
}

static int text_width(const char *s, float scale) {
    return (int)(stb_easy_font_width((char *)s) * scale);
}

// ─────────────────────────────────────────────
// Piano geometry helpers
// ─────────────────────────────────────────────
static int note_to_white_key_x(int midi_note) {
    int rel    = midi_note - START_NOTE;
    int octave = rel / 12;
    int semi   = rel % 12;
    for (int i = 0; i < 7; i++) {
        if (WHITE_SEMI[i] == semi)
            return KB_X + (octave * 7 + i) * WK_W;
    }
    return -1;
}

static int note_to_black_key_x(int midi_note) {
    int rel    = midi_note - START_NOTE;
    int octave = rel / 12;
    int semi   = rel % 12;
    for (int i = 0; i < 5; i++) {
        if (BLACK_SEMI[i] == semi)
            return KB_X + octave * 7 * WK_W + BLACK_OFF[i];
    }
    return -1;
}

static int is_black_note(int midi_note) {
    int semi = (midi_note - START_NOTE) % 12;
    for (int i = 0; i < 5; i++)
        if (BLACK_SEMI[i] == semi) return 1;
    return 0;
}

// Returns MIDI note at pixel (mx, my), or -1
static int hit_test(int mx, int my) {
    if (my < KB_Y || my >= KB_Y + WK_H) return -1;
    int in_black = (my < KB_Y + BK_H);

    if (in_black) {
        for (int oct = 0; oct < OCTAVES; oct++) {
            for (int i = 0; i < 5; i++) {
                int midi = START_NOTE + oct * 12 + BLACK_SEMI[i];
                int bx   = KB_X + oct * 7 * WK_W + BLACK_OFF[i];
                if (mx >= bx && mx < bx + BK_W)
                    return midi;
            }
        }
    }

    for (int oct = 0; oct < OCTAVES; oct++) {
        for (int i = 0; i < 7; i++) {
            int midi = START_NOTE + oct * 12 + WHITE_SEMI[i];
            int wx   = KB_X + (oct * 7 + i) * WK_W;
            if (mx >= wx && mx < wx + WK_W)
                return midi;
        }
    }
    return -1;
}

// ─────────────────────────────────────────────
// AMY helpers
// ─────────────────────────────────────────────
static void amy_note_on(int midi_note) {
    amy_event e = amy_default_event();
    e.synth     = 0;
    e.midi_note = (float)midi_note;
    e.velocity  = 0.8f;
    amy_add_event(&e);
}

static void amy_note_off(int midi_note) {
    amy_event e = amy_default_event();
    e.synth     = 0;
    e.midi_note = (float)midi_note;
    e.velocity  = 0.0f;
    amy_add_event(&e);
}

static void load_patch(int patch_num) {
    // Release all sounding notes
    for (int n = 0; n < 128; n++) {
        if (key_down[n]) { amy_note_off(n); key_down[n] = 0; }
    }
    held_note = -1;
    current_patch = patch_num;

    // Load the patch into synth 0 with 8-voice polyphony
    amy_event e = amy_default_event();
    e.synth        = 0;
    e.num_voices   = 8;
    e.patch_number = (uint16_t)patch_num;
    amy_add_event(&e);
}

// ─────────────────────────────────────────────
// Rendering
// ─────────────────────────────────────────────
static int pt_in_rect(int px, int py, SDL_Rect r) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

static void draw_button(SDL_Renderer *r, SDL_Rect btn, const char *label,
                        int hovered, int pressed_vis)
{
    Uint8 base = hovered ? 80 : 58;
    Uint8 top  = pressed_vis ? 42 : base + 20;
    SDL_SetRenderDrawColor(r, top, top, top + 10, 255);
    SDL_RenderFillRect(r, &btn);
    SDL_SetRenderDrawColor(r, 110, 110, 130, 255);
    SDL_RenderDrawRect(r, &btn);

    float sc  = 2.0f;
    int   tw  = text_width(label, sc);
    int   th  = (int)(13 * sc);
    draw_text(r,
              btn.x + (btn.w - tw) / 2,
              btn.y + (btn.h - th) / 2,
              sc, label,
              (SDL_Color){220, 220, 230, 255});
}

static void render_header(SDL_Renderer *r) {
    // Background gradient effect (two rects)
    SDL_SetRenderDrawColor(r, 28, 28, 34, 255);
    SDL_Rect bg = {0, 0, WIN_W, HEADER_H};
    SDL_RenderFillRect(r, &bg);

    // Separator line
    SDL_SetRenderDrawColor(r, 60, 60, 75, 255);
    SDL_RenderDrawLine(r, 0, HEADER_H, WIN_W, HEADER_H);

    // "AMY Piano" title
    draw_text(r, WIN_W / 2 - text_width("AMY Piano", 1.8f) / 2, 10,
              1.8f, "AMY Piano", (SDL_Color){130, 130, 160, 255});

    // Prev / Next buttons
    int prev_hov = pt_in_rect(mouse_x, mouse_y, BTN_PREV);
    int next_hov = pt_in_rect(mouse_x, mouse_y, BTN_NEXT);
    draw_button(r, BTN_PREV, "< Prev", prev_hov, 0);
    draw_button(r, BTN_NEXT, "Next >", next_hov, 0);

    // Bank tag
    const char *bank = (current_patch < 128) ? "Juno 106" : "DX7";
    char tag[32];
    snprintf(tag, sizeof(tag), "[%s]", bank);
    SDL_Color bank_col = (current_patch < 128)
        ? (SDL_Color){80, 200, 120, 255}
        : (SDL_Color){80, 160, 255, 255};
    draw_text(r, WIN_W / 2 - text_width(tag, 1.5f) / 2, 32,
              1.5f, tag, bank_col);

    // Patch name
    char label[128];
    snprintf(label, sizeof(label), "#%03d  %s",
             current_patch, patch_names[current_patch]);
    float sc = 2.5f;
    int   lw = text_width(label, sc);
    // Clamp if too wide
    if (lw > WIN_W - 240) sc = 2.0f, lw = text_width(label, sc);
    draw_text(r, (WIN_W - lw) / 2, 56, sc, label,
              (SDL_Color){255, 230, 90, 255});

    // Hint
    draw_text(r, WIN_W / 2 - text_width("Arrow keys / scroll: change preset  |  a-; to play (polyphonic)", 1.3f) / 2,
              96, 1.3f, "Arrow keys / scroll: change preset  |  a-; to play (polyphonic)",
              (SDL_Color){90, 90, 110, 255});
}

static void render_keyboard(SDL_Renderer *r) {
    // White keys
    for (int oct = 0; oct < OCTAVES; oct++) {
        for (int i = 0; i < 7; i++) {
            int midi = START_NOTE + oct * 12 + WHITE_SEMI[i];
            int wx   = KB_X + (oct * 7 + i) * WK_W;
            SDL_Rect key = {wx, KB_Y, WK_W - 1, WK_H};

            if (key_down[midi]) {
                SDL_SetRenderDrawColor(r, 90, 150, 255, 255);
            } else {
                SDL_SetRenderDrawColor(r, 242, 242, 242, 255);
            }
            SDL_RenderFillRect(r, &key);

            // Bottom shadow / border
            SDL_SetRenderDrawColor(r, 80, 80, 90, 255);
            SDL_RenderDrawRect(r, &key);

            // C note label
            if (WHITE_SEMI[i] == 0) {
                char lbl[4];
                int  octave_num = (midi / 12) - 1;
                snprintf(lbl, sizeof(lbl), "C%d", octave_num);
                SDL_Color lcol = key_down[midi]
                    ? (SDL_Color){200, 230, 255, 255}
                    : (SDL_Color){150, 150, 170, 255};
                draw_text(r, wx + 4, KB_Y + WK_H - 18, 1.3f, lbl, lcol);
            }

            // Keyboard shortcut label
            const char *klbl = midi_to_keylabel(midi);
            if (klbl) {
                SDL_Color kcol = key_down[midi]
                    ? (SDL_Color){220, 240, 255, 255}
                    : (SDL_Color){100, 100, 140, 255};
                int kw = text_width(klbl, 1.5f);
                draw_text(r, wx + (WK_W - 1 - kw) / 2,
                          KB_Y + WK_H - 34, 1.5f, klbl, kcol);
            }
        }
    }

    // Black keys (drawn on top)
    for (int oct = 0; oct < OCTAVES; oct++) {
        for (int i = 0; i < 5; i++) {
            int midi = START_NOTE + oct * 12 + BLACK_SEMI[i];
            int bx   = KB_X + oct * 7 * WK_W + BLACK_OFF[i];
            SDL_Rect key = {bx, KB_Y, BK_W, BK_H};

            if (key_down[midi]) {
                SDL_SetRenderDrawColor(r, 50, 100, 210, 255);
            } else {
                SDL_SetRenderDrawColor(r, 22, 22, 26, 255);
            }
            SDL_RenderFillRect(r, &key);

            // Subtle highlight on top edge
            SDL_SetRenderDrawColor(r, 70, 70, 80, 255);
            SDL_RenderDrawLine(r, bx, KB_Y, bx + BK_W - 1, KB_Y);

            // Keyboard shortcut label on black key
            const char *klbl = midi_to_keylabel(midi);
            if (klbl) {
                SDL_Color kcol = key_down[midi]
                    ? (SDL_Color){200, 230, 255, 255}
                    : (SDL_Color){120, 120, 150, 255};
                int kw = text_width(klbl, 1.3f);
                draw_text(r, bx + (BK_W - kw) / 2,
                          KB_Y + BK_H - 16, 1.3f, klbl, kcol);
            }
        }
    }
}

// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────
int main(int argc, char **argv) {
    (void)argc; (void)argv;

    // Init AMY
    amy_config_t cfg  = amy_default_config();
    cfg.audio         = AMY_AUDIO_IS_MINIAUDIO;
    cfg.features.default_synths = 0;
    amy_start(cfg);

    // Load first patch
    load_patch(0);

    // Init SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window   *win = SDL_CreateWindow("AMY Piano",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "SDL create: %s\n", SDL_GetError());
        return 1;
    }

    int running    = 1;
    int mouse_held = 0;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_MOUSEMOTION:
                mouse_x = ev.motion.x;
                mouse_y = ev.motion.y;
                if (mouse_held) {
                    int note = hit_test(mouse_x, mouse_y);
                    if (note >= 0 && note != held_note) {
                        // Slide to adjacent key
                        if (held_note >= 0) {
                            amy_note_off(held_note);
                            key_down[held_note] = 0;
                        }
                        held_note         = note;
                        key_down[note]    = 1;
                        amy_note_on(note);
                    }
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_held = 1;
                    mouse_x    = ev.button.x;
                    mouse_y    = ev.button.y;

                    if (pt_in_rect(mouse_x, mouse_y, BTN_PREV)) {
                        if (current_patch > 0)
                            load_patch(current_patch - 1);
                    } else if (pt_in_rect(mouse_x, mouse_y, BTN_NEXT)) {
                        if (current_patch < NUM_PATCHES - 1)
                            load_patch(current_patch + 1);
                    } else {
                        int note = hit_test(mouse_x, mouse_y);
                        if (note >= 0) {
                            if (held_note >= 0 && held_note != note) {
                                amy_note_off(held_note);
                                key_down[held_note] = 0;
                            }
                            held_note      = note;
                            key_down[note] = 1;
                            amy_note_on(note);
                        }
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_held = 0;
                    if (held_note >= 0) {
                        amy_note_off(held_note);
                        key_down[held_note] = 0;
                        held_note = -1;
                    }
                }
                break;

            case SDL_MOUSEWHEEL:
                if (ev.wheel.y > 0 && current_patch > 0)
                    load_patch(current_patch - 1);
                else if (ev.wheel.y < 0 && current_patch < NUM_PATCHES - 1)
                    load_patch(current_patch + 1);
                break;

            case SDL_KEYDOWN:
                if (ev.key.repeat) break;
                switch (ev.key.keysym.sym) {
                case SDLK_LEFT:
                    if (current_patch > 0)
                        load_patch(current_patch - 1);
                    break;
                case SDLK_RIGHT:
                    if (current_patch < NUM_PATCHES - 1)
                        load_patch(current_patch + 1);
                    break;
                case SDLK_ESCAPE:
                    running = 0;
                    break;
                default: {
                    int note = key_to_midi(ev.key.keysym.sym);
                    if (note >= 0 && !key_down[note]) {
                        key_down[note] = 1;
                        amy_note_on(note);
                    }
                    break;
                }
                }
                break;

            case SDL_KEYUP: {
                int note = key_to_midi(ev.key.keysym.sym);
                if (note >= 0 && key_down[note]) {
                    key_down[note] = 0;
                    amy_note_off(note);
                }
                break;
            }
            }
        }

        // Draw
        SDL_SetRenderDrawColor(ren, 20, 20, 24, 255);
        SDL_RenderClear(ren);
        render_header(ren);
        render_keyboard(ren);
        SDL_RenderPresent(ren);
    }

    // Cleanup — release all sounding notes
    for (int n = 0; n < 128; n++)
        if (key_down[n]) amy_note_off(n);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    amy_stop();
    return 0;
}
