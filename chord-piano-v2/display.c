#include "display.h"
#include "chords.h"
#include "fx.h"
#include "looper.h"
#include <stdio.h>
#include <string.h>

#define STB_EASY_FONT_IMPLEMENTATION
#include "../piano/stb_easy_font.h"

// ── Colours — warm amber-on-dark ─────────────────────────────────────────────

const SDL_Color C_BG     = { 14, 11,  9, 255};  // warm near-black
const SDL_Color C_SURF   = { 24, 20, 15, 255};  // pad surface (warm dark)
const SDL_Color C_BORDER = { 48, 40, 30, 255};  // warm border
const SDL_Color C_SEP    = { 28, 24, 18, 255};  // separator
const SDL_Color C_HI     = {232,210,160, 255};  // warm amber-cream
const SDL_Color C_MID    = {118, 98, 68, 255};  // warm mid-brown
const SDL_Color C_DIM    = { 52, 44, 32, 255};  // warm dim
const SDL_Color C_ACCENT = {255, 88,  0, 255};  // TE orange
const SDL_Color C_WHITE  = {232,210,160, 255};  // warm cream (= C_HI on pads)
const SDL_Color C_BLACK  = { 14, 11,  9, 255};  // dark text on cream pads

const SDL_Color OVR_COL[7] = {
    {118, 72,220, 255},  // violet
    {255, 88,  0, 255},  // orange
    {215, 48, 55, 255},  // red
    {205,148,  0, 255},  // amber
    {  0,172,185, 255},  // teal
    { 28,170, 82, 255},  // green
    {200, 44,162, 255},  // magenta
};

const SDL_Color MODE_COL[3]      = {{ 48, 40, 30, 255}, { 18, 55,105, 255}, { 82, 50, 10, 255}};
const SDL_Color MODE_TEXT_COL[3] = {{ 52, 44, 32, 255}, { 38,138,225, 255}, {215,140,  0, 255}};

// ── Button rects ──────────────────────────────────────────────────────────────

const SDL_Rect BTN_KEY_PREV   = { 12,  9, 24, 26};
const SDL_Rect BTN_KEY_NEXT   = {208,  9, 24, 26};
const SDL_Rect BTN_PATCH_PREV = {648,  9, 24, 26};
const SDL_Rect BTN_PATCH_NEXT = {864,  9, 24, 26};
const SDL_Rect BTN_BASS       = {398, CTRL_Y + 6, 62, 28};
const SDL_Rect BTN_OCT_DOWN   = {478, CTRL_Y + 6, 24, 28};
const SDL_Rect BTN_OCT_UP     = {540, CTRL_Y + 6, 24, 28};

const SDL_Rect BTN_ARP_TOG     = { 12, ARP_BTN_Y, 40, ARP_BTN_H};
const SDL_Rect BTN_ARP_DIV     = { 84, ARP_BTN_Y, 42, ARP_BTN_H};
const SDL_Rect BTN_BPM_DOWN    = {158, ARP_BTN_Y, 20, ARP_BTN_H};
const SDL_Rect BTN_BPM_UP      = {212, ARP_BTN_Y, 20, ARP_BTN_H};
const SDL_Rect BTN_ARP_ORD     = {244, ARP_BTN_Y, 42, ARP_BTN_H};
const SDL_Rect BTN_ARPOCT_DOWN = {320, ARP_BTN_Y, 20, ARP_BTN_H};
const SDL_Rect BTN_ARPOCT_UP   = {360, ARP_BTN_Y, 20, ARP_BTN_H};
const SDL_Rect BTN_ARP_GATE    = {392, ARP_BTN_Y, 42, ARP_BTN_H};
const SDL_Rect BTN_ARP_HOLD    = {446, ARP_BTN_Y, 46, ARP_BTN_H};

const SDL_Rect BTN_FX_REV = { 62, FX_BTN_Y, 52, FX_BTN_H};
const SDL_Rect BTN_FX_CHO = {150, FX_BTN_Y, 52, FX_BTN_H};
const SDL_Rect BTN_FX_DLY = {238, FX_BTN_Y, 52, FX_BTN_H};

// Loop row: [LOOP]  [T1][T2][T3][T4]  ····  [REC]
const SDL_Rect BTN_LOOP_TOG    = { 12, LOOP_BTN_Y,  44, LOOP_BTN_H};
const SDL_Rect BTN_LOOP_T[4]   = {
    { 68, LOOP_BTN_Y, 80, LOOP_BTN_H},
    {152, LOOP_BTN_Y, 80, LOOP_BTN_H},
    {236, LOOP_BTN_Y, 80, LOOP_BTN_H},
    {320, LOOP_BTN_Y, 80, LOOP_BTN_H},
};
const SDL_Rect BTN_LOOP_REC    = {840, LOOP_BTN_Y,  52, LOOP_BTN_H};

// ── Low-level draw helpers ────────────────────────────────────────────────────

static void set_color(SDL_Renderer *r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}
static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c) {
    set_color(r, c); SDL_RenderFillRect(r, &(SDL_Rect){x,y,w,h});
}
static void draw_rect_outline(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c) {
    set_color(r, c); SDL_RenderDrawRect(r, &(SDL_Rect){x,y,w,h});
}
static void draw_thick_outline(SDL_Renderer *r, int x, int y, int w, int h, SDL_Color c) {
    set_color(r, c);
    SDL_RenderDrawRect(r, &(SDL_Rect){x,   y,   w,   h  });
    SDL_RenderDrawRect(r, &(SDL_Rect){x+1, y+1, w-2, h-2});
}
static void hline(SDL_Renderer *r, int y, SDL_Color c) {
    set_color(r, c); SDL_RenderDrawLine(r, 0, y, WIN_W, y);
}
static void draw_text(SDL_Renderer *r, int x, int y, float sc,
                      const char *text, SDL_Color col) {
    static char buf[1<<17];
    int nq = stb_easy_font_print(0, 0, (char*)text, NULL, buf, (int)sizeof(buf));
    set_color(r, col);
    for (int q = 0; q < nq; q++) {
        float *v = (float*)(buf + q*64);
        float x0=v[0],y0=v[1],x1=v[0],y1=v[1];
        for (int i=1;i<4;i++){float vx=v[i*4],vy=v[i*4+1];
            if(vx<x0)x0=vx; if(vx>x1)x1=vx;
            if(vy<y0)y0=vy; if(vy>y1)y1=vy;}
        SDL_Rect rc={x+(int)(x0*sc),y+(int)(y0*sc),
                     (int)((x1-x0+1)*sc),(int)((y1-y0+1)*sc)};
        SDL_RenderFillRect(r, &rc);
    }
}

int text_width(const char *s, float sc) {
    return (int)(stb_easy_font_width((char*)s) * sc);
}
int pt_in_rect(int px, int py, SDL_Rect re) {
    return px >= re.x && px < re.x+re.w && py >= re.y && py < re.y+re.h;
}

static void draw_arrow_btn(SDL_Renderer *r, SDL_Rect btn, const char *lbl, int hov) {
    fill_rect(r, btn.x, btn.y, btn.w, btn.h, hov ? C_SURF : C_BG);
    draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, hov ? C_MID : C_BORDER);
    draw_text(r, btn.x+(btn.w-text_width(lbl,1.5f))/2,
                 btn.y+(btn.h-(int)(13*1.5f))/2, 1.5f, lbl, hov ? C_HI : C_MID);
}

// ── Pad + dot geometry ────────────────────────────────────────────────────────

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
    for (int v = 0; v <= KNOB_MAX; v++) {
        SDL_Rect d = dot_rect(v);
        if (pt_in_rect(mx, my, d)) return v;
    }
    return -1;
}
int hit_row_mode_btn(int mx, int my) {
    SDL_Rect u = {MODE_BTN_X, UPPER_LABEL_Y-1, MODE_BTN_W, MODE_BTN_H};
    SDL_Rect l = {MODE_BTN_X, LOWER_LABEL_Y-1, MODE_BTN_W, MODE_BTN_H};
    if (pt_in_rect(mx, my, u)) return 0;
    if (pt_in_rect(mx, my, l)) return 1;
    return -1;
}

// ── Render sections ───────────────────────────────────────────────────────────

static void render_voice_dots(SDL_Renderer *r) {
    draw_text(r, 14, DOTS_Y, 1.3f, "VOICE", C_DIM);
    for (int v = 0; v <= KNOB_MAX; v++) {
        SDL_Rect d = dot_rect(v);
        SDL_Color fill = (v <= voice_level) ? C_ACCENT : C_SURF;
        SDL_Color bord = (v == voice_level) ? C_HI    : C_BORDER;
        fill_rect(r, d.x, d.y, d.w, d.h, fill);
        draw_rect_outline(r, d.x, d.y, d.w, d.h, bord);
    }
    draw_text(r, DOTS_X+5*(DOT_SZ+DOT_GAP)+4, DOTS_Y, 1.3f, VOICE_LABELS[voice_level], C_MID);
}

static void render_topbar(SDL_Renderer *r) {
    fill_rect(r, 0, 0, WIN_W, TOP_H, C_BG);
    // 2px accent line at the very top — instrument identity strip
    fill_rect(r, 0, 0, WIN_W, 2, C_ACCENT);
    hline(r, TOP_H, C_SEP);
    int kp = pt_in_rect(mouse_x, mouse_y, BTN_KEY_PREV);
    int kn = pt_in_rect(mouse_x, mouse_y, BTN_KEY_NEXT);
    draw_arrow_btn(r, BTN_KEY_PREV, "<", kp);
    draw_arrow_btn(r, BTN_KEY_NEXT, ">", kn);
    char key_lbl[20]; snprintf(key_lbl, sizeof(key_lbl), "%s MAJOR", KEY_NAMES[current_key]);
    float ksc = 2.5f; int klw = text_width(key_lbl, ksc);
    int klx = BTN_KEY_PREV.x+BTN_KEY_PREV.w+(BTN_KEY_NEXT.x-BTN_KEY_PREV.x-BTN_KEY_PREV.w-klw)/2;
    draw_text(r, klx, TOP_H/2-(int)(13*ksc)/2+1, ksc, key_lbl, C_HI);
    int pp = pt_in_rect(mouse_x, mouse_y, BTN_PATCH_PREV);
    int pn = pt_in_rect(mouse_x, mouse_y, BTN_PATCH_NEXT);
    draw_arrow_btn(r, BTN_PATCH_PREV, "<", pp);
    draw_arrow_btn(r, BTN_PATCH_NEXT, ">", pn);
    char ptch[16]; snprintf(ptch, sizeof(ptch), "#%03d", current_patch);
    draw_text(r, BTN_PATCH_PREV.x-58, TOP_H/2-(int)(13*1.3f)/2+1, 1.3f, "PATCH", C_DIM);
    float psc = 2.0f; int plw = text_width(ptch, psc);
    int plx = BTN_PATCH_PREV.x+BTN_PATCH_PREV.w+(BTN_PATCH_NEXT.x-BTN_PATCH_PREV.x-BTN_PATCH_PREV.w-plw)/2;
    draw_text(r, plx, TOP_H/2-(int)(13*psc)/2+1, psc, ptch, C_MID);
}

static void render_controls(SDL_Renderer *r) {
    fill_rect(r, 0, CTRL_Y, WIN_W, CTRL_H, C_BG);
    hline(r, CTRL_Y+CTRL_H, C_SEP);
    render_voice_dots(r);
    SDL_Rect bb = BTN_BASS; int bh = pt_in_rect(mouse_x, mouse_y, bb);
    SDL_Color bf = bass_on ? C_ACCENT : (bh ? C_SURF : C_BG);
    fill_rect(r, bb.x, bb.y, bb.w, bb.h, bf);
    draw_rect_outline(r, bb.x, bb.y, bb.w, bb.h, bass_on ? C_ACCENT : C_BORDER);
    draw_text(r, bb.x+(bb.w-text_width("BASS",1.5f))/2,
                 bb.y+(bb.h-(int)(13*1.5f))/2, 1.5f, "BASS", bass_on ? C_WHITE : C_MID);
    draw_text(r, BTN_OCT_DOWN.x, BTN_OCT_DOWN.y-12, 1.2f, "OCT", C_DIM);
    draw_arrow_btn(r, BTN_OCT_DOWN, "-", pt_in_rect(mouse_x, mouse_y, BTN_OCT_DOWN));
    draw_arrow_btn(r, BTN_OCT_UP,   "+", pt_in_rect(mouse_x, mouse_y, BTN_OCT_UP));
    char oct_str[8]; snprintf(oct_str, sizeof(oct_str), "%+d", octave_shift);
    int otw = text_width(oct_str, 1.8f);
    int ocx = BTN_OCT_DOWN.x+BTN_OCT_DOWN.w+(BTN_OCT_UP.x-BTN_OCT_DOWN.x-BTN_OCT_DOWN.w-otw)/2;
    draw_text(r, ocx, CTRL_Y+(CTRL_H-(int)(13*1.8f))/2, 1.8f, oct_str,
              octave_shift ? C_HI : C_MID);
    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active) continue;
        char cname[24];
        make_chord_name(current_key, CK[i].degree, voice_level, quality_override,
                        cname, sizeof(cname));
        SDL_Color cc = quality_override >= 0 ? OVR_COL[quality_override] : C_ACCENT;
        draw_text(r, 525, CTRL_Y+(CTRL_H-(int)(13*3.2f))/2, 3.2f, cname, cc);
        break;
    }
}

static void render_modifier_strip(SDL_Renderer *r) {
    fill_rect(r, 0, MOD_Y, WIN_W, MOD_H, C_BG);
    hline(r, MOD_Y+MOD_H, C_SEP);
    for (int i = 0; i < 7; i++) {
        SDL_Rect btn = {MOD_BTN_X0+i*(MOD_BTN_W+MOD_BTN_GAP), MOD_BTN_Y, MOD_BTN_W, MOD_BTN_H};
        int hov = pt_in_rect(mouse_x, mouse_y, btn), act = (quality_override == i);
        fill_rect(r, btn.x, btn.y, btn.w, btn.h,
                  act ? OVR_COL[i] : (hov ? C_SURF : C_BG));
        if (act) draw_thick_outline(r, btn.x, btn.y, btn.w, btn.h, C_HI);
        else     draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, hov ? C_BORDER : C_DIM);
        draw_text(r, btn.x+(btn.w-text_width(OVR_LABELS[i],1.3f))/2,
                     btn.y+(btn.h-(int)(13*1.3f))/2, 1.3f, OVR_LABELS[i],
                     act ? C_HI : (hov ? C_MID : C_DIM));
    }
}

static void render_arp_row(SDL_Renderer *r) {
    fill_rect(r, 0, ARP_Y, WIN_W, ARP_H, C_BG);
    hline(r, ARP_Y+ARP_H, C_SEP);
    int ty = ARP_BTN_Y + (ARP_BTN_H-(int)(13*1.3f))/2;

    { SDL_Rect btn = BTN_ARP_TOG;
      SDL_Color fill = arp_on ? C_ACCENT : (pt_in_rect(mouse_x,mouse_y,btn) ? C_SURF : C_BG);
      fill_rect(r, btn.x, btn.y, btn.w, btn.h, fill);
      draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, arp_on ? C_ACCENT : C_BORDER);
      draw_text(r, btn.x+(btn.w-text_width("ARP",1.3f))/2, ty, 1.3f, "ARP",
                arp_on ? C_WHITE : C_MID); }

    if (!arp_on) { draw_text(r, 62, ty, 1.2f, "ARPEGGIATE OFF", C_DIM); return; }

    draw_text(r, 62, ty, 1.2f, "DIV", C_DIM);
    { SDL_Rect btn = BTN_ARP_DIV; int hov = pt_in_rect(mouse_x, mouse_y, btn);
      fill_rect(r, btn.x, btn.y, btn.w, btn.h, hov ? C_SURF : C_BG);
      draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, C_BORDER);
      draw_text(r, btn.x+(btn.w-text_width(ARP_DIV_LBL[arp_div_idx],1.3f))/2,
                ty, 1.3f, ARP_DIV_LBL[arp_div_idx], C_HI); }

    draw_text(r, 136, ty, 1.2f, "BPM", C_DIM);
    draw_arrow_btn(r, BTN_BPM_DOWN, "<", pt_in_rect(mouse_x, mouse_y, BTN_BPM_DOWN));
    draw_arrow_btn(r, BTN_BPM_UP,   ">", pt_in_rect(mouse_x, mouse_y, BTN_BPM_UP));
    { char s[8]; snprintf(s, sizeof(s), "%d", arp_bpm);
      int tw = text_width(s, 1.5f);
      int cx = BTN_BPM_DOWN.x+BTN_BPM_DOWN.w+(BTN_BPM_UP.x-BTN_BPM_DOWN.x-BTN_BPM_DOWN.w-tw)/2;
      draw_text(r, cx, ty, 1.5f, s, C_WHITE); }

    { SDL_Rect btn = BTN_ARP_ORD; int hov = pt_in_rect(mouse_x, mouse_y, btn);
      fill_rect(r, btn.x, btn.y, btn.w, btn.h, hov ? C_SURF : C_BG);
      draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, C_BORDER);
      draw_text(r, btn.x+(btn.w-text_width(ARP_ORD_LBL[(int)arp_order],1.3f))/2,
                ty, 1.3f, ARP_ORD_LBL[(int)arp_order], C_HI); }

    draw_text(r, 298, ty, 1.2f, "OCT", C_DIM);
    draw_arrow_btn(r, BTN_ARPOCT_DOWN, "<", pt_in_rect(mouse_x, mouse_y, BTN_ARPOCT_DOWN));
    draw_arrow_btn(r, BTN_ARPOCT_UP,   ">", pt_in_rect(mouse_x, mouse_y, BTN_ARPOCT_UP));
    { char s[4]; snprintf(s, sizeof(s), "%d", arp_octaves);
      int tw = text_width(s, 1.5f);
      int cx = BTN_ARPOCT_DOWN.x+BTN_ARPOCT_DOWN.w+(BTN_ARPOCT_UP.x-BTN_ARPOCT_DOWN.x-BTN_ARPOCT_DOWN.w-tw)/2;
      draw_text(r, cx, ty, 1.5f, s, C_WHITE); }

    { SDL_Rect btn = BTN_ARP_GATE; int hov = pt_in_rect(mouse_x, mouse_y, btn);
      fill_rect(r, btn.x, btn.y, btn.w, btn.h, hov ? C_SURF : C_BG);
      draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, C_BORDER);
      draw_text(r, btn.x+(btn.w-text_width(ARP_GATE_LBL[arp_gate],1.3f))/2,
                ty, 1.3f, ARP_GATE_LBL[arp_gate], C_HI); }

    { SDL_Rect btn = BTN_ARP_HOLD;
      SDL_Color fill = arp_hold ? C_WHITE : (pt_in_rect(mouse_x,mouse_y,btn) ? C_SURF : C_BG);
      fill_rect(r, btn.x, btn.y, btn.w, btn.h, fill);
      draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, arp_hold ? C_WHITE : C_BORDER);
      draw_text(r, btn.x+(btn.w-text_width("HOLD",1.3f))/2, ty, 1.3f, "HOLD",
                arp_hold ? C_BLACK : C_MID); }
}

static void render_fx_cycle_btn(SDL_Renderer *r, SDL_Rect btn,
                                 const char *const *labels, int preset) {
    int hov = pt_in_rect(mouse_x, mouse_y, btn);
    int act = preset > 0;
    fill_rect(r, btn.x, btn.y, btn.w, btn.h,
              act ? C_WHITE : (hov ? C_SURF : C_BG));
    draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, act ? C_WHITE : C_BORDER);
    draw_text(r, btn.x+(btn.w-text_width(labels[preset],1.3f))/2,
                 btn.y+(btn.h-(int)(13*1.3f))/2,
              1.3f, labels[preset], act ? C_BLACK : (hov ? C_HI : C_MID));
}

static void render_fx_row(SDL_Renderer *r) {
    fill_rect(r, 0, FX_Y, WIN_W, FX_H, C_BG);
    hline(r, FX_Y+FX_H, C_SEP);
    int ty = FX_BTN_Y + (FX_BTN_H-(int)(13*1.2f))/2;
    draw_text(r, 14,  ty, 1.2f, "FX",  C_DIM);
    draw_text(r, 40,  ty, 1.2f, "REV", C_DIM);
    render_fx_cycle_btn(r, BTN_FX_REV, REV_LABELS, rev_preset);
    draw_text(r, 128, ty, 1.2f, "CHO", C_DIM);
    render_fx_cycle_btn(r, BTN_FX_CHO, CHO_LABELS, cho_preset);
    draw_text(r, 216, ty, 1.2f, "DLY", C_DIM);
    render_fx_cycle_btn(r, BTN_FX_DLY, DLY_LABELS, dly_preset);
    if (dly_preset > 0) {
        char hint[24];
        float ms = (dly_preset == 1) ? 60000.0f/arp_bpm*0.5f
                 : (dly_preset == 2) ? 60000.0f/arp_bpm
                                     : 60000.0f/arp_bpm*1.5f;
        snprintf(hint, sizeof(hint), "%.0fms", ms);
        draw_text(r, 298, ty, 1.2f, hint, C_DIM);
    }
}

static void render_mode_badge(SDL_Renderer *r, int row_y, RowMode mode) {
    SDL_Rect btn = {MODE_BTN_X, row_y-1, MODE_BTN_W, MODE_BTN_H};
    int hov = pt_in_rect(mouse_x, mouse_y, btn);
    fill_rect(r, btn.x, btn.y, btn.w, btn.h, hov ? C_SURF : (SDL_Color){22,22,22,255});
    draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h,
                      mode == ROW_CHORD ? C_BORDER : MODE_COL[(int)mode]);
    SDL_Color tc = hov ? C_HI : MODE_TEXT_COL[(int)mode];
    const char *lbl = row_mode_name(mode);
    draw_text(r, btn.x+(btn.w-text_width(lbl,1.2f))/2,
                 btn.y+(btn.h-(int)(13*1.2f))/2+1, 1.2f, lbl, tc);
}

static void render_pads(SDL_Renderer *r) {
    draw_text(r, PAD_X0, UPPER_LABEL_Y, 1.2f, "C4   Q W E R T Y U", C_DIM);
    render_mode_badge(r, UPPER_LABEL_Y, upper_mode);
    draw_text(r, PAD_X0, LOWER_LABEL_Y, 1.2f, "C3   A S D F G H J", C_DIM);
    draw_text(r, K_PAD_X, LOWER_LABEL_Y, 1.2f, "C5 K", C_DIM);
    render_mode_badge(r, LOWER_LABEL_Y, lower_mode);

    for (int i = 0; i < NUM_CK; i++) {
        SDL_Rect pad = pad_rect(i);
        int held = actv[i].active, hov = pt_in_rect(mouse_x, mouse_y, pad);
        RowMode mode = row_mode_for(i);

        SDL_Color fill, bord, text_col, sub_col;
        int thick = 0;
        if (held && quality_override >= 0 && mode == ROW_CHORD) {
            SDL_Color oc = OVR_COL[quality_override];
            fill = oc; bord = C_HI; text_col = C_HI; thick = 1;
            sub_col = (SDL_Color){(Uint8)(oc.r/2+80),(Uint8)(oc.g/2+80),(Uint8)(oc.b/2+80),255};
        } else if (held) {
            fill = C_WHITE; bord = C_ACCENT; text_col = C_BLACK; thick = 1;
            sub_col = (SDL_Color){80, 65, 42, 255};
        } else if (hov) {
            fill = C_SURF; bord = C_MID; text_col = C_HI; sub_col = C_DIM;
        } else {
            fill = C_SURF;
            bord = (mode == ROW_CHORD) ? C_BORDER : MODE_COL[(int)mode];
            text_col = C_MID; sub_col = C_DIM;
        }
        fill_rect(r, pad.x, pad.y, pad.w, pad.h, fill);
        if (thick) draw_thick_outline(r, pad.x, pad.y, pad.w, pad.h, bord);
        else        draw_rect_outline(r, pad.x, pad.y, pad.w, pad.h, bord);

        char cname[24];
        if (mode == ROW_CHORD)
            make_chord_name(current_key, CK[i].degree, voice_level, quality_override,
                            cname, sizeof(cname));
        else
            snprintf(cname, sizeof(cname), "%s",
                     KEY_NAMES[(current_key + MAJOR_SCALE[CK[i].degree]) % 12]);
        float csc = ((int)strlen(cname) > 5) ? 1.8f : 2.4f;
        draw_text(r, pad.x+(pad.w-text_width(cname,csc))/2,
                     pad.y+(pad.h-(int)(13*csc))/2-4, csc, cname, text_col);

        if (mode != ROW_CHORD) {
            SDL_Color mc = held ? (SDL_Color){100,100,100,255} : MODE_TEXT_COL[(int)mode];
            draw_text(r, pad.x+4, pad.y+4, 1.1f, mode == ROW_BASS ? "BAS" : "LED", mc);
        }
        if (held && arp_on) {
            SDL_Color ac = text_col.r < 100 ? (SDL_Color){200,200,200,255}
                                             : (SDL_Color){80,80,80,255};
            draw_text(r, pad.x+4, pad.y+pad.h-28, 1.1f, "ARP", ac);
        }
        draw_text(r, pad.x+5, pad.y+pad.h-16, 1.2f, DEGREE_ROMAN[CK[i].degree], sub_col);
        draw_text(r, pad.x+pad.w-text_width(CK[i].label,1.2f)-5,
                     pad.y+pad.h-16, 1.2f, CK[i].label, sub_col);
    }
}

// ── Loop row ──────────────────────────────────────────────────────────────────

// Track state colour: empty=dim border, has content=mid, playing=white, rec=accent
static SDL_Color loop_track_color(int ti) {
    LoopState s = looper.state;
    int is_active = (ti == looper.active_track);
    int has_content = (looper.tracks[ti].count > 0);
    if (is_active && (s == LOOP_RECORDING || s == LOOP_OVERDUB))
        return C_ACCENT;
    if (has_content && !looper.tracks[ti].muted)
        return C_WHITE;
    if (has_content && looper.tracks[ti].muted)
        return C_DIM;
    return (SDL_Color){28,28,28,255};
}

static void render_loop_row(SDL_Renderer *r) {
    fill_rect(r, 0, LOOP_Y, WIN_W, LOOP_H, C_BG);
    hline(r, LOOP_Y + LOOP_H, C_SEP);

    int ty = LOOP_BTN_Y + (LOOP_BTN_H - (int)(13*1.3f)) / 2;
    LoopState s = looper.state;
    int loop_active = (s != LOOP_IDLE);

    // [LOOP] toggle
    { SDL_Rect btn = BTN_LOOP_TOG;
      SDL_Color fill = loop_active ? C_ACCENT : (pt_in_rect(mouse_x,mouse_y,btn) ? C_SURF : C_BG);
      fill_rect(r, btn.x, btn.y, btn.w, btn.h, fill);
      if (loop_active) draw_thick_outline(r, btn.x, btn.y, btn.w, btn.h, C_HI);
      else             draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, C_BORDER);
      draw_text(r, btn.x+(btn.w-text_width("LOOP",1.3f))/2, ty, 1.3f, "LOOP",
                loop_active ? C_HI : C_MID); }

    // Track buttons
    static const char *TRACK_NAMES[4] = {"T1 BASS","T2 CHORD","T3 LEAD","T4 DRUM"};
    for (int i = 0; i < NUM_LOOP_TRACKS; i++) {
        SDL_Rect btn = BTN_LOOP_T[i];
        int is_active = (i == looper.active_track);
        int hov = pt_in_rect(mouse_x, mouse_y, btn);
        SDL_Color tc = loop_track_color(i);
        SDL_Color fill = (tc.r == C_ACCENT.r && tc.g == C_ACCENT.g) ? C_ACCENT
                       : (looper.tracks[i].count > 0 && !looper.tracks[i].muted) ? (SDL_Color){30,30,30,255}
                       : (hov ? C_SURF : C_BG);
        SDL_Color bord = is_active ? C_HI : (looper.tracks[i].count > 0 ? C_MID : C_DIM);
        fill_rect(r, btn.x, btn.y, btn.w, btn.h, fill);
        draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, bord);
        draw_text(r, btn.x+(btn.w-text_width(TRACK_NAMES[i],1.2f))/2, ty, 1.2f,
                  TRACK_NAMES[i], tc.r==0&&tc.g==0&&tc.b==0 ? C_DIM : tc);
    }

    // Position bar (when playing) or beat countdown (when armed)
    if (s == LOOP_ARMED) {
        int beats = loop_beat_countdown();
        char s_buf[8]; snprintf(s_buf, sizeof(s_buf), "%d", beats);
        // Flash the countdown number
        Uint32 now = SDL_GetTicks();
        SDL_Color cc = (now / 250) % 2 == 0 ? C_ACCENT : C_MID;
        draw_text(r, 420, ty, 2.0f, s_buf, cc);
        draw_text(r, 445, ty, 1.2f, "bars", C_DIM);
    } else if (s == LOOP_PLAYING || s == LOOP_OVERDUB) {
        float pos = loop_position();
        int bar_w = 380;
        int bar_x = 420;
        int bar_y = LOOP_BTN_Y + (LOOP_BTN_H - 6) / 2;
        fill_rect(r, bar_x, bar_y, bar_w, 6, (SDL_Color){30,30,30,255});
        fill_rect(r, bar_x, bar_y, (int)(pos * bar_w), 6,
                  s == LOOP_OVERDUB ? C_ACCENT : C_WHITE);
    }

    // [REC] button
    { SDL_Rect btn = BTN_LOOP_REC;
      int hov = pt_in_rect(mouse_x, mouse_y, btn);
      int rec_lit = (s == LOOP_RECORDING || s == LOOP_OVERDUB || s == LOOP_ARMED);
      Uint32 now = SDL_GetTicks();
      int pulse = rec_lit && ((now / 350) % 2 == 0);
      SDL_Color fill = pulse ? C_ACCENT : (hov ? C_SURF : C_BG);
      fill_rect(r, btn.x, btn.y, btn.w, btn.h, fill);
      if (rec_lit) draw_thick_outline(r, btn.x, btn.y, btn.w, btn.h, C_ACCENT);
      else         draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, hov ? C_MID : C_BORDER);
      draw_text(r, btn.x+(btn.w-text_width("REC",1.3f))/2, ty, 1.3f, "REC",
                rec_lit ? C_HI : C_MID); }
}

// ── Public entry point ────────────────────────────────────────────────────────

void render_frame(SDL_Renderer *r) {
    fill_rect(r, 0, 0, WIN_W, WIN_H, C_BG);
    render_topbar(r);
    render_controls(r);
    render_modifier_strip(r);
    render_arp_row(r);
    render_fx_row(r);
    render_loop_row(r);
    render_pads(r);
}
