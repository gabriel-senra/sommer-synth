// main.c — AMY Chord Piano · entry point, event loop, platform init

#include "amy.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

#include "state.h"
#include "synth.h"
#include "fx.h"
#include "looper.h"
#include "display.h"
#include "arp.h"

static int key_to_ovr(SDL_Keycode k) {
    if (k >= SDLK_1 && k <= SDLK_7) return (int)(k - SDLK_1);
    return -1;
}
static int find_ck(SDL_Keycode k) {
    for (int i = 0; i < NUM_CK; i++) if (CK[i].key == k) return i;
    return -1;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    srand((unsigned)SDL_GetTicks());
    for (int i = 0; i < NUM_CK; i++) arp_st[i].cur_note = -1;

    amy_config_t cfg = amy_default_config();
    cfg.audio                   = AMY_AUDIO_IS_MINIAUDIO;
    cfg.features.default_synths = 0;
    cfg.features.reverb         = 1;
    cfg.features.chorus         = 1;
    cfg.features.echo           = 1;
    amy_start(cfg);
    load_patch(0);
    apply_all_fx();

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1;
    }
    display_init();
    SDL_Window *win = SDL_CreateWindow("AMY Chord Piano",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }

    int running = 1;
    while (running) {
        arp_tick();
        loop_tick();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT: running = 0; break;
            case SDL_MOUSEMOTION: mouse_x = ev.motion.x; mouse_y = ev.motion.y; break;

            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_x = ev.button.x; mouse_y = ev.button.y;
                    if      (pt_in_rect(mouse_x, mouse_y, BTN_KEY_PREV))               current_key = (current_key+11)%12;
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_KEY_NEXT))               current_key = (current_key+1)%12;
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_PATCH_PREV) && current_patch >   0) load_patch(current_patch-1);
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_PATCH_NEXT) && current_patch < 255) load_patch(current_patch+1);
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_BASS))                   bass_on ^= 1;
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_OCT_DOWN) && octave_shift > -2) { octave_shift--; retrigger_all_active(); }
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_OCT_UP)   && octave_shift <  2) { octave_shift++; retrigger_all_active(); }
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_ARP_TOG))                toggle_arp();
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_ARP_DIV))                arp_div_idx = (arp_div_idx+1)%4;
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_BPM_DOWN) && arp_bpm >  40) { arp_bpm -= 5; apply_delay(); }
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_BPM_UP)   && arp_bpm < 240) { arp_bpm += 5; apply_delay(); }
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_ARP_ORD))                arp_order = (ArpOrder)((arp_order+1)%4);
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_ARPOCT_DOWN) && arp_octaves > 1) arp_octaves--;
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_ARPOCT_UP)   && arp_octaves < 3) arp_octaves++;
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_ARP_GATE))               arp_gate = (arp_gate+1)%3;
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_ARP_HOLD))               arp_hold ^= 1;
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_FX_REV)) { rev_preset = (rev_preset+1)%4; apply_reverb(); }
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_FX_CHO)) { cho_preset = (cho_preset+1)%4; apply_chorus(); }
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_FX_DLY)) { dly_preset = (dly_preset+1)%4; apply_delay();  }
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_LOOP_REC)) loop_press_rec();
                    else if (pt_in_rect(mouse_x, mouse_y, BTN_LOOP_TOG)) {
                        if (looper.state != LOOP_IDLE) {
                            // stop all playing notes and reset
                            for (int i = 0; i < NUM_LOOP_TRACKS; i++) loop_clear_track(i);
                            looper.state = LOOP_IDLE; looper.loop_len_ms = 0;
                        } else {
                            loop_press_rec();
                        }
                    }
                    else {
                        int lt = -1;
                        for (int i = 0; i < NUM_LOOP_TRACKS; i++)
                            if (pt_in_rect(mouse_x, mouse_y, BTN_LOOP_T[i])) { lt = i; break; }
                        if (lt >= 0) { loop_select_track(lt); break; }
                        int dv = hit_dot(mouse_x, mouse_y);
                        if (dv >= 0) { voice_level = dv; break; }
                        int rm = hit_row_mode_btn(mouse_x, mouse_y);
                        if (rm == 0) { upper_mode = (RowMode)((upper_mode+1)%3); retrigger_all_active(); break; }
                        if (rm == 1) { lower_mode = (RowMode)((lower_mode+1)%3); retrigger_all_active(); break; }
                        int pi = hit_pad(mouse_x, mouse_y);
                        if (pi >= 0) { trigger_chord(pi); mouse_pad = pi; }
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT && mouse_pad >= 0) {
                    release_chord(mouse_pad); mouse_pad = -1;
                }
                break;

            case SDL_MOUSEWHEEL: {
                int delta = (ev.wheel.y > 0) ? 1 : -1;
                int np = current_patch - delta;
                if (np >= 0 && np <= 255) load_patch(np);
                break;
            }

            case SDL_KEYDOWN:
                if (ev.key.repeat) break;
                switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE:        running = 0; break;
                case SDLK_LEFT:          current_key = (current_key+11)%12; break;
                case SDLK_RIGHT:         current_key = (current_key+1)%12; break;
                case SDLK_UP:            if (voice_level < KNOB_MAX) voice_level++; break;
                case SDLK_DOWN:          if (voice_level > 0) voice_level--; break;
                case SDLK_b:             bass_on ^= 1; break;
                case SDLK_LEFTBRACKET:   if (octave_shift > -2) { octave_shift--; retrigger_all_active(); } break;
                case SDLK_RIGHTBRACKET:  if (octave_shift <  2) { octave_shift++; retrigger_all_active(); } break;
                case SDLK_BACKSLASH:     toggle_arp(); break;
                default: {
                    int oi = key_to_ovr(ev.key.keysym.sym);
                    if (oi >= 0) { quality_override = oi; retrigger_all_active(); break; }
                    int ci = find_ck(ev.key.keysym.sym);
                    if (ci >= 0) trigger_chord(ci);
                } break;
                }
                break;

            case SDL_KEYUP: {
                int oi = key_to_ovr(ev.key.keysym.sym);
                if (oi >= 0 && quality_override == oi) { quality_override = -1; retrigger_all_active(); break; }
                int ci = find_ck(ev.key.keysym.sym);
                if (ci >= 0) release_chord(ci);
                break;
            }
            }
        }

        render_frame(ren);
        SDL_RenderPresent(ren);
    }

    for (int i = 0; i < NUM_CK; i++) { arp_stop_note(i); release_chord(i); }
    display_cleanup();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    amy_stop();
    return 0;
}
