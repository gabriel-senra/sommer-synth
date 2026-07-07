#include "display.h"
#include "chords.h"
#include "fx.h"
#include "transport.h"
#include <stdio.h>
#include <string.h>

#define STB_EASY_FONT_IMPLEMENTATION
#include "../piano/stb_easy_font.h"

// ── Colours ───────────────────────────────────────────────────────────────────

const SDL_Color C_BG     = { 11, 11, 11,255};
const SDL_Color C_SURF   = { 20, 20, 20,255};
const SDL_Color C_BORDER = { 40, 40, 40,255};
const SDL_Color C_SEP    = { 30, 30, 30,255};
const SDL_Color C_HI     = {228,228,228,255};
const SDL_Color C_MID    = {110,110,110,255};
const SDL_Color C_DIM    = { 50, 50, 50,255};
const SDL_Color C_ACCENT = {255, 82,  0,255};
const SDL_Color C_WHITE  = {255,255,255,255};
const SDL_Color C_BLACK  = { 11, 11, 11,255};

const SDL_Color OVR_COL[7] = {
    {110, 68,210,255},{255, 82,  0,255},{190, 38, 38,255},
    {195,140,  0,255},{  0,158,180,255},{  0,158, 74,255},{180, 38,138,255},
};

const SDL_Color MODE_COL[3]      = {{40,40,40,255},{20,50,90,255},{80,50,10,255}};
const SDL_Color MODE_TEXT_COL[3] = {{50,50,50,255},{0,120,210,255},{200,120,0,255}};

// ── Button rects ──────────────────────────────────────────────────────────────

const SDL_Rect BTN_KEY_PREV   = { 12,  9, 24, 26};
const SDL_Rect BTN_KEY_NEXT   = {208,  9, 24, 26};
const SDL_Rect BTN_PATCH_PREV = {648,  9, 24, 26};
const SDL_Rect BTN_PATCH_NEXT = {864,  9, 24, 26};
const SDL_Rect BTN_BASS       = {398, CTRL_Y + 6, 62, 28};
const SDL_Rect BTN_OCT_DOWN   = {478, CTRL_Y + 6, 24, 28};
const SDL_Rect BTN_OCT_UP     = {540, CTRL_Y + 6, 24, 28};
const SDL_Rect BTN_MODE       = {726, CTRL_Y + 6, 78, 28};

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

// Metronome row: [PLAY] ●●●●
const SDL_Rect BTN_METRO       = { 12, LOOP_BTN_Y,  46, LOOP_BTN_H};

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
    draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, C_BORDER);
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
        SDL_Color fill = (v <= voice_level) ? C_WHITE : (SDL_Color){28,28,28,255};
        SDL_Color bord = (v == voice_level) ? C_HI    : C_BORDER;
        fill_rect(r, d.x, d.y, d.w, d.h, fill);
        draw_rect_outline(r, d.x, d.y, d.w, d.h, bord);
    }
    draw_text(r, DOTS_X+5*(DOT_SZ+DOT_GAP)+4, DOTS_Y, 1.3f, VOICE_LABELS[voice_level], C_MID);
}

static void render_topbar(SDL_Renderer *r) {
    fill_rect(r, 0, 0, WIN_W, TOP_H, C_BG);
    hline(r, TOP_H, C_SEP);
    int kp = pt_in_rect(mouse_x, mouse_y, BTN_KEY_PREV);
    int kn = pt_in_rect(mouse_x, mouse_y, BTN_KEY_NEXT);
    draw_arrow_btn(r, BTN_KEY_PREV, "<", kp);
    draw_arrow_btn(r, BTN_KEY_NEXT, ">", kn);
    char key_lbl[20];
    snprintf(key_lbl, sizeof(key_lbl), "%s %s", KEY_NAMES[current_key],
             piano_mode ? "PIANO" : "MAJOR");
    float ksc = 2.2f; int klw = text_width(key_lbl, ksc);
    int klx = BTN_KEY_PREV.x+BTN_KEY_PREV.w+(BTN_KEY_NEXT.x-BTN_KEY_PREV.x-BTN_KEY_PREV.w-klw)/2;
    draw_text(r, klx, TOP_H/2-(int)(13*ksc)/2+1, ksc, key_lbl, C_WHITE);
    int pp = pt_in_rect(mouse_x, mouse_y, BTN_PATCH_PREV);
    int pn = pt_in_rect(mouse_x, mouse_y, BTN_PATCH_NEXT);
    draw_arrow_btn(r, BTN_PATCH_PREV, "<", pp);
    draw_arrow_btn(r, BTN_PATCH_NEXT, ">", pn);
    char ptch[16]; snprintf(ptch, sizeof(ptch), "#%03d", current_patch);
    draw_text(r, BTN_PATCH_PREV.x-58, TOP_H/2-(int)(13*1.3f)/2+1, 1.3f, "PATCH", C_DIM);
    float psc = 1.8f; int plw = text_width(ptch, psc);
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
    // MODE toggle — SCALE (diatonic chords) / PIANO (chromatic keys)
    { SDL_Rect mb = BTN_MODE; int mh = pt_in_rect(mouse_x, mouse_y, mb);
      const char *mlbl = piano_mode ? "PIANO" : "SCALE";
      fill_rect(r, mb.x, mb.y, mb.w, mb.h, piano_mode ? C_ACCENT : (mh ? C_SURF : C_BG));
      draw_rect_outline(r, mb.x, mb.y, mb.w, mb.h, piano_mode ? C_ACCENT : C_BORDER);
      draw_text(r, mb.x+(mb.w-text_width(mlbl,1.5f))/2,
                   mb.y+(mb.h-(int)(13*1.5f))/2, 1.5f, mlbl,
                piano_mode ? C_WHITE : C_MID); }
    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active) continue;
        char cname[24];
        SDL_Color cc;
        if (piano_mode) {
            snprintf(cname, sizeof(cname), "%s", KEY_NAMES[actv[i].notes[0] % 12]);
            cc = C_ACCENT;
        } else {
            make_chord_name(current_key, CK[i].degree, voice_level, quality_override,
                            cname, sizeof(cname));
            cc = quality_override >= 0 ? OVR_COL[quality_override] : C_ACCENT;
        }
        draw_text(r, 530, CTRL_Y+(CTRL_H-(int)(13*2.8f))/2, 2.8f, cname, cc);
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
        draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h,
                          act ? OVR_COL[i] : (hov ? C_BORDER : C_DIM));
        draw_text(r, btn.x+(btn.w-text_width(OVR_LABELS[i],1.3f))/2,
                     btn.y+(btn.h-(int)(13*1.3f))/2, 1.3f, OVR_LABELS[i],
                     act ? C_WHITE : (hov ? C_MID : C_DIM));
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
    if (piano_mode) {
        draw_text(r, PAD_X0, UPPER_LABEL_Y, 1.2f, "BLACK   Q W E R T Y U", C_DIM);
        draw_text(r, PAD_X0, LOWER_LABEL_Y, 1.2f, "WHITE   A S D F G H J", C_DIM);
        draw_text(r, K_PAD_X, LOWER_LABEL_Y, 1.2f, "C  K", C_DIM);
    } else {
        draw_text(r, PAD_X0, UPPER_LABEL_Y, 1.2f, "C4   Q W E R T Y U", C_DIM);
        render_mode_badge(r, UPPER_LABEL_Y, upper_mode);
        draw_text(r, PAD_X0, LOWER_LABEL_Y, 1.2f, "C3   A S D F G H J", C_DIM);
        draw_text(r, K_PAD_X, LOWER_LABEL_Y, 1.2f, "C5 K", C_DIM);
        render_mode_badge(r, LOWER_LABEL_Y, lower_mode);
    }

    for (int i = 0; i < NUM_CK; i++) {
        SDL_Rect pad = pad_rect(i);
        int held = actv[i].active, hov = pt_in_rect(mouse_x, mouse_y, pad);

        if (piano_mode) {
            int m = piano_midi(i);
            int is_black = piano_is_black(i);
            if (m < 0) {                       // gap — no key here
                fill_rect(r, pad.x, pad.y, pad.w, pad.h, C_BG);
                draw_rect_outline(r, pad.x, pad.y, pad.w, pad.h, C_SEP);
                continue;
            }
            SDL_Color fill, bord, text_col, sub_col;
            if (held) {
                fill = C_ACCENT; bord = C_WHITE; text_col = C_WHITE;
                sub_col = (SDL_Color){255,180,140,255};
            } else if (hov) {
                fill = C_SURF; bord = C_BORDER; text_col = C_HI; sub_col = C_DIM;
            } else if (is_black) {
                fill = (SDL_Color){16,16,16,255}; bord = C_BORDER;
                text_col = C_MID; sub_col = C_DIM;
            } else {
                fill = (SDL_Color){26,26,26,255}; bord = C_BORDER;
                text_col = C_HI;  sub_col = C_MID;
            }
            fill_rect(r, pad.x, pad.y, pad.w, pad.h, fill);
            draw_rect_outline(r, pad.x, pad.y, pad.w, pad.h, bord);
            char nm[8]; snprintf(nm, sizeof(nm), "%s", KEY_NAMES[m % 12]);
            float nsc = 2.0f;
            draw_text(r, pad.x+(pad.w-text_width(nm,nsc))/2,
                         pad.y+(pad.h-(int)(13*nsc))/2-6, nsc, nm, text_col);
            if (held && arp_on)
                draw_text(r, pad.x+4, pad.y+pad.h-28, 1.1f, "ARP",
                          (SDL_Color){255,220,200,255});
            draw_text(r, pad.x+5, pad.y+pad.h-16, 1.2f, is_black ? "BLK" : "WHT", sub_col);
            draw_text(r, pad.x+pad.w-text_width(CK[i].label,1.2f)-5,
                         pad.y+pad.h-16, 1.2f, CK[i].label, sub_col);
            continue;
        }

        RowMode mode = row_mode_for(i);

        SDL_Color fill, bord, text_col, sub_col;
        if (held && quality_override >= 0 && mode == ROW_CHORD) {
            SDL_Color oc = OVR_COL[quality_override];
            fill = oc; bord = C_WHITE; text_col = C_WHITE;
            sub_col = (SDL_Color){oc.r/2+127, oc.g/2+127, oc.b/2+127, 255};
        } else if (held) {
            fill = C_WHITE; bord = C_WHITE; text_col = C_BLACK;
            sub_col = (SDL_Color){80,80,80,255};
        } else if (hov) {
            fill = C_SURF; bord = C_BORDER; text_col = C_HI; sub_col = C_DIM;
        } else {
            fill = C_BG;
            bord = (mode == ROW_CHORD) ? C_BORDER : MODE_COL[(int)mode];
            text_col = C_MID; sub_col = C_DIM;
        }
        fill_rect(r, pad.x, pad.y, pad.w, pad.h, fill);
        draw_rect_outline(r, pad.x, pad.y, pad.w, pad.h, bord);

        char cname[24];
        if (mode == ROW_CHORD)
            make_chord_name(current_key, CK[i].degree, voice_level, quality_override,
                            cname, sizeof(cname));
        else
            snprintf(cname, sizeof(cname), "%s",
                     KEY_NAMES[(current_key + MAJOR_SCALE[CK[i].degree]) % 12]);
        float csc = ((int)strlen(cname) > 5) ? 1.6f : 2.0f;
        draw_text(r, pad.x+(pad.w-text_width(cname,csc))/2,
                     pad.y+(pad.h-(int)(13*csc))/2-6, csc, cname, text_col);

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

// ── Metronome row ─────────────────────────────────────────────────────────────

static void render_metro_row(SDL_Renderer *r) {
    fill_rect(r, 0, LOOP_Y, WIN_W, LOOP_H, C_BG);
    hline(r, LOOP_Y + LOOP_H, C_SEP);

    int ty = LOOP_BTN_Y + (LOOP_BTN_H - (int)(13*1.3f)) / 2;

    // [PLAY/STOP] metronome toggle + 4-beat indicator
    { SDL_Rect btn = BTN_METRO;
      int hov = pt_in_rect(mouse_x, mouse_y, btn);
      int on  = transport_active();
      fill_rect(r, btn.x, btn.y, btn.w, btn.h, on ? C_HI : (hov ? C_SURF : C_BG));
      draw_rect_outline(r, btn.x, btn.y, btn.w, btn.h, on ? C_HI : C_BORDER);
      const char *lbl = on ? "STOP" : "PLAY";
      draw_text(r, btn.x+(btn.w-text_width(lbl,1.2f))/2, ty, 1.2f, lbl,
                on ? C_BLACK : C_MID);
      int dx = btn.x + btn.w + 12;
      int dy = LOOP_BTN_Y + (LOOP_BTN_H - 8) / 2;
      for (int b = 0; b < BEATS_PER_BAR; b++) {
          int lit = on && (transport.cur_beat == b);
          SDL_Color dc = lit ? (b == 0 ? C_ACCENT : C_WHITE) : (SDL_Color){40,40,40,255};
          fill_rect(r, dx + b*14, dy, 8, 8, dc);
      }
      draw_text(r, dx + BEATS_PER_BAR*14 + 8, ty, 1.2f, "METRONOME", C_DIM); }
}

// ── Public entry point ────────────────────────────────────────────────────────

void render_frame(SDL_Renderer *r) {
    fill_rect(r, 0, 0, WIN_W, WIN_H, C_BG);
    render_topbar(r);
    render_controls(r);
    render_modifier_strip(r);
    render_arp_row(r);
    render_fx_row(r);
    render_metro_row(r);
    render_pads(r);
}
