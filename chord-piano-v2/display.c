// display.c — SDL rendering, clean warm-light aesthetic, Helvetica Neue

#include "display.h"
#include "chords.h"
#include "fx.h"
#include "looper.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ── Fonts ─────────────────────────────────────────────────────────────────────

#define FONT_PATH  "/System/Library/Fonts/HelveticaNeue.ttc"
#define FONT_REG   0   // Regular
#define FONT_MED   10  // Medium

static TTF_Font *font_sm  = NULL;   // 11px regular  — sub-labels
static TTF_Font *font_md  = NULL;   // 13px medium   — button labels, values
static TTF_Font *font_lg  = NULL;   // 19px medium   — chord names on pads
static TTF_Font *font_xl  = NULL;   // 26px regular  — key name

void display_init(void) {
    TTF_Init();
    font_sm = TTF_OpenFontIndex(FONT_PATH, 11, FONT_REG);
    font_md = TTF_OpenFontIndex(FONT_PATH, 13, FONT_MED);
    font_lg = TTF_OpenFontIndex(FONT_PATH, 19, FONT_MED);
    font_xl = TTF_OpenFontIndex(FONT_PATH, 26, FONT_REG);
}

void display_cleanup(void) {
    TTF_CloseFont(font_sm); TTF_CloseFont(font_md);
    TTF_CloseFont(font_lg); TTF_CloseFont(font_xl);
    TTF_Quit();
}

// ── Text cache ────────────────────────────────────────────────────────────────

#define TCACHE_CAP 192

typedef struct {
    char         str[64];
    TTF_Font    *font;
    Uint32       color;
    SDL_Texture *tex;
    int          w, h;
    Uint32       tick;
} TCEntry;

static TCEntry       tce[TCACHE_CAP];
static Uint32        tce_tick = 0;
static SDL_Renderer *g_ren    = NULL;

static TCEntry *tc_get(const char *s, TTF_Font *font, SDL_Color col) {
    if (!font || !s || !*s || !g_ren) return NULL;
    Uint32 ck = ((Uint32)col.r<<24)|((Uint32)col.g<<16)|((Uint32)col.b<<8)|col.a;
    tce_tick++;
    for (int i = 0; i < TCACHE_CAP; i++) {
        if (tce[i].font==font && tce[i].color==ck && strcmp(tce[i].str, s)==0) {
            tce[i].tick = tce_tick; return &tce[i];
        }
    }
    int slot = 0; Uint32 oldest = tce[0].tick;
    for (int i = 1; i < TCACHE_CAP; i++) {
        if (!tce[i].font) { slot = i; break; }
        if (tce[i].tick < oldest) { oldest = tce[i].tick; slot = i; }
    }
    if (tce[slot].tex) SDL_DestroyTexture(tce[slot].tex);
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, s, col);
    if (!surf) { tce[slot].font = NULL; return NULL; }
    tce[slot].tex   = SDL_CreateTextureFromSurface(g_ren, surf);
    tce[slot].w     = surf->w;
    tce[slot].h     = surf->h;
    SDL_FreeSurface(surf);
    tce[slot].font  = font;
    tce[slot].color = ck;
    tce[slot].tick  = tce_tick;
    strncpy(tce[slot].str, s, 63);
    tce[slot].str[63] = '\0';
    return &tce[slot];
}

static void txt(SDL_Renderer *r, int x, int y, TTF_Font *f, const char *s, SDL_Color c) {
    TCEntry *e = tc_get(s, f, c);
    if (!e || !e->tex) return;
    SDL_RenderCopy(r, e->tex, NULL, &(SDL_Rect){x, y, e->w, e->h});
}

static void txt_c(SDL_Renderer *r, int cx, int cy, TTF_Font *f, const char *s, SDL_Color c) {
    TCEntry *e = tc_get(s, f, c);
    if (!e || !e->tex) return;
    SDL_RenderCopy(r, e->tex, NULL, &(SDL_Rect){cx-e->w/2, cy-e->h/2, e->w, e->h});
}

static int sw(TTF_Font *f, const char *s) {
    int w = 0; if (f && s) TTF_SizeUTF8(f, s, &w, NULL); return w;
}
static int sh(TTF_Font *f) { return f ? TTF_FontHeight(f) : 0; }

// ── Palette — warm cream light ────────────────────────────────────────────────

const SDL_Color C_BG     = {238,228,210, 255};  // warm cream
const SDL_Color C_SURF   = {252,248,240, 255};  // card surface
const SDL_Color C_BORDER = {206,192,168, 255};  // warm border
const SDL_Color C_SEP    = {222,212,194, 255};  // separator
const SDL_Color C_HI     = { 34, 24, 12, 255};  // dark warm text
const SDL_Color C_MID    = {112, 94, 64, 255};  // medium warm
const SDL_Color C_DIM    = {172,156,128, 255};  // dim warm
const SDL_Color C_ACCENT = {182,112, 14, 255};  // warm amber
const SDL_Color C_WHITE  = {252,248,240, 255};  // = C_SURF
const SDL_Color C_BLACK  = { 34, 24, 12, 255};  // = C_HI

const SDL_Color OVR_COL[7] = {
    { 96, 52,192, 255},  // violet
    {182,112, 14, 255},  // amber (accent)
    {186, 36, 48, 255},  // red
    {162,118,  0, 255},  // golden
    {  0,136,154, 255},  // teal
    { 14,144, 62, 255},  // green
    {168, 30,130, 255},  // magenta
};

const SDL_Color MODE_COL[3]      = {{206,192,168,255},{178,208,238,255},{238,212,162,255}};
const SDL_Color MODE_TEXT_COL[3] = {{148,134,110,255},{ 18, 96,182,255},{162, 88,  0,255}};

// ── Button rects ──────────────────────────────────────────────────────────────

const SDL_Rect BTN_KEY_PREV   = { 10,  10, 26, 28};
const SDL_Rect BTN_KEY_NEXT   = {208,  10, 26, 28};
const SDL_Rect BTN_PATCH_PREV = {648,  10, 26, 28};
const SDL_Rect BTN_PATCH_NEXT = {862,  10, 26, 28};
const SDL_Rect BTN_BASS       = {398, CTRL_Y + 7, 62, 28};
const SDL_Rect BTN_OCT_DOWN   = {478, CTRL_Y + 7, 24, 28};
const SDL_Rect BTN_OCT_UP     = {540, CTRL_Y + 7, 24, 28};

const SDL_Rect BTN_ARP_TOG     = { 10, ARP_BTN_Y, 42, ARP_BTN_H};
const SDL_Rect BTN_ARP_DIV     = { 84, ARP_BTN_Y, 44, ARP_BTN_H};
const SDL_Rect BTN_BPM_DOWN    = {158, ARP_BTN_Y, 20, ARP_BTN_H};
const SDL_Rect BTN_BPM_UP      = {214, ARP_BTN_Y, 20, ARP_BTN_H};
const SDL_Rect BTN_ARP_ORD     = {246, ARP_BTN_Y, 44, ARP_BTN_H};
const SDL_Rect BTN_ARPOCT_DOWN = {322, ARP_BTN_Y, 20, ARP_BTN_H};
const SDL_Rect BTN_ARPOCT_UP   = {364, ARP_BTN_Y, 20, ARP_BTN_H};
const SDL_Rect BTN_ARP_GATE    = {396, ARP_BTN_Y, 44, ARP_BTN_H};
const SDL_Rect BTN_ARP_HOLD    = {452, ARP_BTN_Y, 48, ARP_BTN_H};

const SDL_Rect BTN_FX_REV = { 62, FX_BTN_Y, 52, FX_BTN_H};
const SDL_Rect BTN_FX_CHO = {150, FX_BTN_Y, 52, FX_BTN_H};
const SDL_Rect BTN_FX_DLY = {238, FX_BTN_Y, 52, FX_BTN_H};

const SDL_Rect BTN_LOOP_TOG  = { 10, LOOP_BTN_Y,  46, LOOP_BTN_H};
const SDL_Rect BTN_LOOP_T[4] = {
    { 64, LOOP_BTN_Y, 82, LOOP_BTN_H},
    {150, LOOP_BTN_Y, 82, LOOP_BTN_H},
    {236, LOOP_BTN_Y, 82, LOOP_BTN_H},
    {322, LOOP_BTN_Y, 82, LOOP_BTN_H},
};
const SDL_Rect BTN_LOOP_REC = {840, LOOP_BTN_Y, 54, LOOP_BTN_H};

// ── Primitives ────────────────────────────────────────────────────────────────

static void set_color(SDL_Renderer *r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c) {
    set_color(r, c); SDL_RenderFillRect(r, &(SDL_Rect){x,y,w,h});
}
static void hline(SDL_Renderer *r, int y, SDL_Color c) {
    set_color(r, c); SDL_RenderDrawLine(r, 0, y, WIN_W, y);
}

// Rounded fill — scanline approach
static void fill_rr(SDL_Renderer *r, int x, int y, int w, int h, int rad, SDL_Color c) {
    set_color(r, c);
    if (rad <= 0 || 2*rad > w || 2*rad > h) {
        SDL_RenderFillRect(r, &(SDL_Rect){x, y, w, h}); return;
    }
    for (int dy = 0; dy < h; dy++) {
        int clip = 0;
        if (dy < rad) {
            double d = (double)(rad - 1 - dy);
            clip = rad - (int)sqrt((double)rad*rad - d*d);
        } else if (dy >= h - rad) {
            double d = (double)(dy - (h - rad));
            clip = rad - (int)sqrt((double)rad*rad - d*d);
        }
        SDL_RenderFillRect(r, &(SDL_Rect){x+clip, y+dy, w-2*clip, 1});
    }
}

// Rounded outline — 1px
static void draw_rr(SDL_Renderer *r, int x, int y, int w, int h, int rad, SDL_Color c) {
    set_color(r, c);
    if (rad <= 0) { SDL_RenderDrawRect(r, &(SDL_Rect){x,y,w,h}); return; }
    SDL_RenderDrawLine(r, x+rad, y,     x+w-1-rad, y);
    SDL_RenderDrawLine(r, x+rad, y+h-1, x+w-1-rad, y+h-1);
    SDL_RenderDrawLine(r, x,     y+rad, x,          y+h-1-rad);
    SDL_RenderDrawLine(r, x+w-1, y+rad, x+w-1,      y+h-1-rad);
    for (int a = 0; a < rad; a++) {
        double d = (double)(rad - 1 - a);
        int off = rad - (int)sqrt((double)rad*rad - d*d);
        SDL_RenderDrawPoint(r, x+off,     y+a);
        SDL_RenderDrawPoint(r, x+w-1-off, y+a);
        SDL_RenderDrawPoint(r, x+off,     y+h-1-a);
        SDL_RenderDrawPoint(r, x+w-1-off, y+h-1-a);
    }
}

// 2px rounded outline
static void draw_rr2(SDL_Renderer *r, int x, int y, int w, int h, int rad, SDL_Color c) {
    draw_rr(r, x,   y,   w,   h,   rad,         c);
    draw_rr(r, x+1, y+1, w-2, h-2, rad>1?rad-1:0, c);
}

// ── Geometry ──────────────────────────────────────────────────────────────────

int pt_in_rect(int px, int py, SDL_Rect re) {
    return px>=re.x && px<re.x+re.w && py>=re.y && py<re.y+re.h;
}

static SDL_Rect pad_rect(int i) {
    if (i < 7)  return (SDL_Rect){PAD_X0 + i*(PAD_W+PAD_GAP), LOWER_PAD_Y, PAD_W, PAD_H};
    if (i < 14) return (SDL_Rect){PAD_X0 + (i-7)*(PAD_W+PAD_GAP), UPPER_PAD_Y, PAD_W, PAD_H};
    return (SDL_Rect){K_PAD_X, LOWER_PAD_Y, K_W, PAD_H};
}
static SDL_Rect dot_rect(int v) {
    return (SDL_Rect){DOTS_X + v*(DOT_SZ+DOT_GAP), DOTS_Y, DOT_SZ, DOT_SZ};
}

int hit_pad(int mx, int my) {
    for (int i = 0; i < NUM_CK; i++) if (pt_in_rect(mx, my, pad_rect(i))) return i;
    return -1;
}
int hit_dot(int mx, int my) {
    for (int v = 0; v <= KNOB_MAX; v++) { SDL_Rect d = dot_rect(v); if (pt_in_rect(mx,my,d)) return v; }
    return -1;
}
int hit_row_mode_btn(int mx, int my) {
    SDL_Rect u = {MODE_BTN_X, UPPER_LABEL_Y-1, MODE_BTN_W, MODE_BTN_H};
    SDL_Rect l = {MODE_BTN_X, LOWER_LABEL_Y-1, MODE_BTN_W, MODE_BTN_H};
    if (pt_in_rect(mx, my, u)) return 0;
    if (pt_in_rect(mx, my, l)) return 1;
    return -1;
}

// ── Chip — the standard small button ─────────────────────────────────────────

static void chip(SDL_Renderer *r, SDL_Rect b, const char *lbl, int active, int hov) {
    SDL_Color fill, bord, tc;
    if (active) {
        fill = C_ACCENT; bord = C_ACCENT;
        tc = (SDL_Color){252,246,232,255};
    } else if (hov) {
        fill = C_SURF; bord = C_MID; tc = C_HI;
    } else {
        fill = C_BG; bord = C_BORDER; tc = C_MID;
    }
    fill_rr(r, b.x, b.y, b.w, b.h, 5, fill);
    if (active) draw_rr2(r, b.x, b.y, b.w, b.h, 5, bord);
    else        draw_rr(r,  b.x, b.y, b.w, b.h, 5, bord);
    txt_c(r, b.x+b.w/2, b.y+b.h/2, font_md, lbl, tc);
}

static void arrow_btn(SDL_Renderer *r, SDL_Rect b, const char *lbl, int hov) {
    fill_rr(r, b.x, b.y, b.w, b.h, 5, hov ? C_SURF : C_BG);
    draw_rr(r, b.x, b.y, b.w, b.h, 5, hov ? C_MID : C_BORDER);
    txt_c(r, b.x+b.w/2, b.y+b.h/2, font_md, lbl, hov ? C_HI : C_MID);
}

// ── Sections ──────────────────────────────────────────────────────────────────

static void render_voice_dots(SDL_Renderer *r) {
    txt(r, 12, DOTS_Y + (DOT_SZ - sh(font_sm))/2, font_sm, "VOICE", C_DIM);
    for (int v = 0; v <= KNOB_MAX; v++) {
        SDL_Rect d = dot_rect(v);
        if (v <= voice_level) {
            fill_rr(r, d.x, d.y, d.w, d.h, d.w/2, C_ACCENT);
        } else {
            fill_rr(r, d.x, d.y, d.w, d.h, d.w/2, C_SURF);
            draw_rr(r, d.x, d.y, d.w, d.h, d.w/2, C_BORDER);
        }
    }
    txt(r, DOTS_X+5*(DOT_SZ+DOT_GAP)+4, DOTS_Y+(DOT_SZ-sh(font_sm))/2,
        font_sm, VOICE_LABELS[voice_level], C_MID);
}

static void render_topbar(SDL_Renderer *r) {
    fill_rect(r, 0, 0, WIN_W, TOP_H, C_BG);
    fill_rect(r, 0, 0, WIN_W, 3, C_ACCENT);   // identity stripe
    hline(r, TOP_H, C_SEP);

    arrow_btn(r, BTN_KEY_PREV, "<", pt_in_rect(mouse_x, mouse_y, BTN_KEY_PREV));
    arrow_btn(r, BTN_KEY_NEXT, ">", pt_in_rect(mouse_x, mouse_y, BTN_KEY_NEXT));

    char key_lbl[24]; snprintf(key_lbl, sizeof(key_lbl), "%s MAJOR", KEY_NAMES[current_key]);
    int klw = sw(font_xl, key_lbl);
    int klx = BTN_KEY_PREV.x+BTN_KEY_PREV.w
            + (BTN_KEY_NEXT.x-BTN_KEY_PREV.x-BTN_KEY_PREV.w-klw)/2;
    txt(r, klx, TOP_H/2-sh(font_xl)/2+1, font_xl, key_lbl, C_HI);

    arrow_btn(r, BTN_PATCH_PREV, "<", pt_in_rect(mouse_x, mouse_y, BTN_PATCH_PREV));
    arrow_btn(r, BTN_PATCH_NEXT, ">", pt_in_rect(mouse_x, mouse_y, BTN_PATCH_NEXT));
    txt(r, BTN_PATCH_PREV.x-46, TOP_H/2-sh(font_sm)/2, font_sm, "PATCH", C_DIM);
    char ptch[8]; snprintf(ptch, sizeof(ptch), "#%03d", current_patch);
    int plw = sw(font_md, ptch);
    int plx = BTN_PATCH_PREV.x+BTN_PATCH_PREV.w
            + (BTN_PATCH_NEXT.x-BTN_PATCH_PREV.x-BTN_PATCH_PREV.w-plw)/2;
    txt(r, plx, TOP_H/2-sh(font_md)/2+1, font_md, ptch, C_MID);
}

static void render_controls(SDL_Renderer *r) {
    fill_rect(r, 0, CTRL_Y, WIN_W, CTRL_H, C_BG);
    hline(r, CTRL_Y+CTRL_H, C_SEP);
    render_voice_dots(r);

    chip(r, BTN_BASS, "BASS", bass_on, pt_in_rect(mouse_x, mouse_y, BTN_BASS));

    txt(r, BTN_OCT_DOWN.x, BTN_OCT_DOWN.y-13, font_sm, "OCT", C_DIM);
    arrow_btn(r, BTN_OCT_DOWN, "-", pt_in_rect(mouse_x, mouse_y, BTN_OCT_DOWN));
    arrow_btn(r, BTN_OCT_UP,   "+", pt_in_rect(mouse_x, mouse_y, BTN_OCT_UP));
    char oct_str[8]; snprintf(oct_str, sizeof(oct_str), "%+d", octave_shift);
    int otw = sw(font_md, oct_str);
    int ocx = BTN_OCT_DOWN.x+BTN_OCT_DOWN.w
            + (BTN_OCT_UP.x-BTN_OCT_DOWN.x-BTN_OCT_DOWN.w-otw)/2;
    txt(r, ocx, CTRL_Y+(CTRL_H-sh(font_md))/2, font_md, oct_str,
        octave_shift ? C_HI : C_MID);

    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active) continue;
        char cname[24];
        make_chord_name(current_key, CK[i].degree, voice_level, quality_override,
                        cname, sizeof(cname));
        SDL_Color cc = quality_override >= 0 ? OVR_COL[quality_override] : C_ACCENT;
        int cw = sw(font_xl, cname);
        txt(r, 528+(360-cw)/2, CTRL_Y+(CTRL_H-sh(font_xl))/2, font_xl, cname, cc);
        break;
    }
}

static void render_modifier_strip(SDL_Renderer *r) {
    fill_rect(r, 0, MOD_Y, WIN_W, MOD_H, C_BG);
    hline(r, MOD_Y+MOD_H, C_SEP);
    for (int i = 0; i < 7; i++) {
        SDL_Rect btn = {MOD_BTN_X0+i*(MOD_BTN_W+MOD_BTN_GAP), MOD_BTN_Y, MOD_BTN_W, MOD_BTN_H};
        int hov = pt_in_rect(mouse_x, mouse_y, btn);
        int act = (quality_override == i);
        fill_rr(r, btn.x, btn.y, btn.w, btn.h, 5,
                act ? OVR_COL[i] : (hov ? C_SURF : C_BG));
        if (act) draw_rr2(r, btn.x, btn.y, btn.w, btn.h, 5, OVR_COL[i]);
        else     draw_rr(r,  btn.x, btn.y, btn.w, btn.h, 5, hov ? C_BORDER : C_SEP);
        SDL_Color tc = act ? (SDL_Color){252,246,232,255} : (hov ? C_HI : C_DIM);
        txt_c(r, btn.x+btn.w/2, btn.y+btn.h/2, font_sm, OVR_LABELS[i], tc);
    }
}

static void render_arp_row(SDL_Renderer *r) {
    fill_rect(r, 0, ARP_Y, WIN_W, ARP_H, C_BG);
    hline(r, ARP_Y+ARP_H, C_SEP);
    int cy = ARP_BTN_Y + ARP_BTN_H/2;

    chip(r, BTN_ARP_TOG, "ARP", arp_on, pt_in_rect(mouse_x, mouse_y, BTN_ARP_TOG));

    if (!arp_on) {
        txt(r, 62, cy-sh(font_sm)/2, font_sm, "ARPEGGIATE OFF", C_DIM); return;
    }

    txt(r, 62,  cy-sh(font_sm)/2, font_sm, "DIV", C_DIM);
    chip(r, BTN_ARP_DIV, ARP_DIV_LBL[arp_div_idx], 0, pt_in_rect(mouse_x,mouse_y,BTN_ARP_DIV));

    txt(r, 136, cy-sh(font_sm)/2, font_sm, "BPM", C_DIM);
    arrow_btn(r, BTN_BPM_DOWN, "<", pt_in_rect(mouse_x, mouse_y, BTN_BPM_DOWN));
    arrow_btn(r, BTN_BPM_UP,   ">", pt_in_rect(mouse_x, mouse_y, BTN_BPM_UP));
    { char s[8]; snprintf(s, sizeof(s), "%d", arp_bpm);
      int bx = BTN_BPM_DOWN.x+BTN_BPM_DOWN.w
             + (BTN_BPM_UP.x-BTN_BPM_DOWN.x-BTN_BPM_DOWN.w-sw(font_md,s))/2;
      txt(r, bx, cy-sh(font_md)/2, font_md, s, C_HI); }

    chip(r, BTN_ARP_ORD, ARP_ORD_LBL[(int)arp_order], 0, pt_in_rect(mouse_x,mouse_y,BTN_ARP_ORD));

    txt(r, 298, cy-sh(font_sm)/2, font_sm, "OCT", C_DIM);
    arrow_btn(r, BTN_ARPOCT_DOWN, "<", pt_in_rect(mouse_x, mouse_y, BTN_ARPOCT_DOWN));
    arrow_btn(r, BTN_ARPOCT_UP,   ">", pt_in_rect(mouse_x, mouse_y, BTN_ARPOCT_UP));
    { char s[4]; snprintf(s, sizeof(s), "%d", arp_octaves);
      int bx = BTN_ARPOCT_DOWN.x+BTN_ARPOCT_DOWN.w
             + (BTN_ARPOCT_UP.x-BTN_ARPOCT_DOWN.x-BTN_ARPOCT_DOWN.w-sw(font_md,s))/2;
      txt(r, bx, cy-sh(font_md)/2, font_md, s, C_HI); }

    chip(r, BTN_ARP_GATE, ARP_GATE_LBL[arp_gate], 0, pt_in_rect(mouse_x,mouse_y,BTN_ARP_GATE));
    chip(r, BTN_ARP_HOLD, "HOLD", arp_hold, pt_in_rect(mouse_x,mouse_y,BTN_ARP_HOLD));
}

static void render_fx_row(SDL_Renderer *r) {
    fill_rect(r, 0, FX_Y, WIN_W, FX_H, C_BG);
    hline(r, FX_Y+FX_H, C_SEP);
    int cy = FX_BTN_Y + FX_BTN_H/2;
    txt(r, 12,  cy-sh(font_sm)/2, font_sm, "FX",  C_DIM);
    txt(r, 38,  cy-sh(font_sm)/2, font_sm, "REV", C_DIM);
    chip(r, BTN_FX_REV, REV_LABELS[rev_preset], rev_preset>0, pt_in_rect(mouse_x,mouse_y,BTN_FX_REV));
    txt(r, 126, cy-sh(font_sm)/2, font_sm, "CHO", C_DIM);
    chip(r, BTN_FX_CHO, CHO_LABELS[cho_preset], cho_preset>0, pt_in_rect(mouse_x,mouse_y,BTN_FX_CHO));
    txt(r, 214, cy-sh(font_sm)/2, font_sm, "DLY", C_DIM);
    chip(r, BTN_FX_DLY, DLY_LABELS[dly_preset], dly_preset>0, pt_in_rect(mouse_x,mouse_y,BTN_FX_DLY));
    if (dly_preset > 0) {
        char hint[16];
        float ms = (dly_preset==1)?60000.f/arp_bpm*.5f
                 : (dly_preset==2)?60000.f/arp_bpm
                                  :60000.f/arp_bpm*1.5f;
        snprintf(hint, sizeof(hint), "%.0fms", ms);
        txt(r, 298, cy-sh(font_sm)/2, font_sm, hint, C_DIM);
    }
}

static void render_loop_row(SDL_Renderer *r) {
    fill_rect(r, 0, LOOP_Y, WIN_W, LOOP_H, C_BG);
    hline(r, LOOP_Y+LOOP_H, C_SEP);
    int cy = LOOP_BTN_Y + LOOP_BTN_H/2;
    LoopState s = looper.state;
    int loop_active = (s != LOOP_IDLE);

    chip(r, BTN_LOOP_TOG, "LOOP", loop_active, pt_in_rect(mouse_x,mouse_y,BTN_LOOP_TOG));

    static const char *TN[4] = {"T1 BASS","T2 CHORD","T3 LEAD","T4 DRUM"};
    for (int i = 0; i < NUM_LOOP_TRACKS; i++) {
        SDL_Rect b = BTN_LOOP_T[i];
        int is_active_track = (i == looper.active_track);
        int is_rec = is_active_track && (s==LOOP_RECORDING||s==LOOP_OVERDUB);
        int has = (looper.tracks[i].count > 0);

        SDL_Color fill = is_rec ? C_ACCENT : (has ? C_SURF : C_BG);
        SDL_Color bord = is_active_track ? C_ACCENT : (has ? C_BORDER : C_SEP);
        SDL_Color tc   = is_rec ? (SDL_Color){252,246,232,255}
                       : (has  ? C_HI : C_DIM);

        fill_rr(r, b.x, b.y, b.w, b.h, 5, fill);
        if (is_active_track) draw_rr2(r, b.x, b.y, b.w, b.h, 5, bord);
        else                 draw_rr(r,  b.x, b.y, b.w, b.h, 5, bord);
        txt_c(r, b.x+b.w/2, cy, font_sm, TN[i], tc);
    }

    if (s == LOOP_ARMED) {
        char s_buf[4]; snprintf(s_buf, sizeof(s_buf), "%d", loop_beat_countdown());
        SDL_Color cc = ((SDL_GetTicks()/220)%2==0) ? C_ACCENT : C_MID;
        txt_c(r, 430, cy, font_lg, s_buf, cc);
        txt(r, 445, cy-sh(font_sm)/2, font_sm, "bars", C_DIM);
    } else if (s==LOOP_PLAYING || s==LOOP_OVERDUB) {
        float pos = loop_position();
        int bw=380, bx=420, by=LOOP_BTN_Y+(LOOP_BTN_H-6)/2;
        fill_rr(r, bx, by, bw, 6, 3, C_SURF);
        draw_rr(r, bx, by, bw, 6, 3, C_BORDER);
        if (pos > 0.001f)
            fill_rr(r, bx, by, (int)(pos*bw), 6, 3,
                    s==LOOP_OVERDUB ? C_ACCENT : C_HI);
    }

    // REC button
    { SDL_Rect b = BTN_LOOP_REC;
      int rec_lit = (s==LOOP_RECORDING||s==LOOP_OVERDUB||s==LOOP_ARMED);
      int pulse   = rec_lit && ((SDL_GetTicks()/320)%2==0);
      fill_rr(r, b.x, b.y, b.w, b.h, 5, pulse ? C_ACCENT : C_BG);
      if (rec_lit) draw_rr2(r, b.x, b.y, b.w, b.h, 5, C_ACCENT);
      else         draw_rr(r,  b.x, b.y, b.w, b.h, 5,
                           pt_in_rect(mouse_x,mouse_y,b) ? C_MID : C_BORDER);
      SDL_Color rtc = rec_lit ? (pulse?(SDL_Color){252,246,232,255}:C_ACCENT) : C_MID;
      txt_c(r, b.x+b.w/2, cy, font_md, "REC", rtc); }
}

static void render_mode_badge(SDL_Renderer *r, int row_y, RowMode mode) {
    SDL_Rect b = {MODE_BTN_X, row_y-1, MODE_BTN_W, MODE_BTN_H};
    int hov = pt_in_rect(mouse_x, mouse_y, b);
    fill_rr(r, b.x, b.y, b.w, b.h, 3,
            mode==ROW_CHORD ? C_BG : MODE_COL[(int)mode]);
    draw_rr(r, b.x, b.y, b.w, b.h, 3,
            mode==ROW_CHORD ? C_BORDER : MODE_COL[(int)mode]);
    txt_c(r, b.x+b.w/2, b.y+b.h/2, font_sm, row_mode_name(mode),
          hov ? C_HI : MODE_TEXT_COL[(int)mode]);
}

static void render_pads(SDL_Renderer *r) {
    txt(r, PAD_X0, UPPER_LABEL_Y, font_sm, "C4  Q W E R T Y U", C_DIM);
    render_mode_badge(r, UPPER_LABEL_Y, upper_mode);
    txt(r, PAD_X0, LOWER_LABEL_Y, font_sm, "C3  A S D F G H J", C_DIM);
    txt(r, K_PAD_X, LOWER_LABEL_Y, font_sm, "C5 K", C_DIM);
    render_mode_badge(r, LOWER_LABEL_Y, lower_mode);

    for (int i = 0; i < NUM_CK; i++) {
        SDL_Rect pad = pad_rect(i);
        int held = actv[i].active, hov = pt_in_rect(mouse_x, mouse_y, pad);
        RowMode mode = row_mode_for(i);

        SDL_Color fill, bord, text_col, sub_col;
        if (held && quality_override >= 0 && mode == ROW_CHORD) {
            fill = OVR_COL[quality_override];
            bord = OVR_COL[quality_override];
            text_col = (SDL_Color){252,246,232,255};
            sub_col  = (SDL_Color){252,246,232,180};
        } else if (held) {
            fill = C_ACCENT; bord = C_ACCENT;
            text_col = (SDL_Color){252,246,232,255};
            sub_col  = (SDL_Color){252,246,232,160};
        } else if (hov) {
            fill = C_SURF; bord = C_MID;
            text_col = C_HI; sub_col = C_DIM;
        } else {
            fill = C_SURF;
            bord = (mode==ROW_CHORD) ? C_BORDER : MODE_COL[(int)mode];
            text_col = C_MID; sub_col = C_DIM;
        }

        fill_rr(r, pad.x, pad.y, pad.w, pad.h, 7, fill);
        if (held) draw_rr2(r, pad.x, pad.y, pad.w, pad.h, 7, bord);
        else      draw_rr(r,  pad.x, pad.y, pad.w, pad.h, 7, bord);

        char cname[24];
        if (mode == ROW_CHORD)
            make_chord_name(current_key, CK[i].degree, voice_level, quality_override,
                            cname, sizeof(cname));
        else
            snprintf(cname, sizeof(cname), "%s",
                     KEY_NAMES[(current_key+MAJOR_SCALE[CK[i].degree])%12]);

        int cx = pad.x + pad.w/2;
        int cy = pad.y + pad.h/2 - 2;
        txt_c(r, cx, cy, font_lg, cname, text_col);

        if (mode != ROW_CHORD) {
            txt(r, pad.x+6, pad.y+6, font_sm,
                mode==ROW_BASS?"BAS":"LED",
                held?(SDL_Color){252,246,232,140}:MODE_TEXT_COL[(int)mode]);
        }
        if (held && arp_on)
            txt(r, pad.x+6, pad.y+pad.h-20, font_sm, "ARP", sub_col);

        txt(r, pad.x+6,                                pad.y+pad.h-17, font_sm, DEGREE_ROMAN[CK[i].degree], sub_col);
        txt(r, pad.x+pad.w-sw(font_sm,CK[i].label)-6, pad.y+pad.h-17, font_sm, CK[i].label, sub_col);
    }
}

// ── Public ────────────────────────────────────────────────────────────────────

void render_frame(SDL_Renderer *r) {
    g_ren = r;
    fill_rect(r, 0, 0, WIN_W, WIN_H, C_BG);
    render_topbar(r);
    render_controls(r);
    render_modifier_strip(r);
    render_arp_row(r);
    render_fx_row(r);
    render_loop_row(r);
    render_pads(r);
}
