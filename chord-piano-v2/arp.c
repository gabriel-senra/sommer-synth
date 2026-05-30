#include "arp.h"
#include "state.h"
#include "amy.h"
#include <stdlib.h>

const int   ARP_DIV_VAL[4]    = {1, 2, 4, 8};
const char *ARP_DIV_LBL[4]    = {"Q","8TH","16TH","32ND"};
const char *ARP_ORD_LBL[4]    = {"UP","DWN","PING","RND"};
const char *ARP_GATE_LBL[3]   = {"SHT","MED","LNG"};
const float ARP_GATE_RATIO[3] = {0.2f, 0.5f, 0.9f};

int arp_step_ms(void) { return 60000 / arp_bpm / ARP_DIV_VAL[arp_div_idx]; }
int arp_gate_ms(void) { return (int)(arp_step_ms() * ARP_GATE_RATIO[arp_gate]); }

void arp_stop_note(int ci) {
    if (arp_st[ci].cur_note >= 0) {
        amy_event e = amy_default_event();
        e.synth = 0; e.midi_note = (float)arp_st[ci].cur_note; e.velocity = 0.0f;
        amy_add_event(&e);
        arp_st[ci].cur_note = -1;
    }
}

void arp_init_key(int ci) {
    int total = actv[ci].num * arp_octaves;
    if (total == 0) return;
    arp_st[ci].pos         = (arp_order == ARP_DOWN) ? total - 1 : 0;
    arp_st[ci].dir         = 1;
    arp_st[ci].next_ms     = SDL_GetTicks();
    arp_st[ci].note_off_ms = 0;
    arp_st[ci].held        = 1;
}

void arp_advance(int ci, int total) {
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

void arp_tick(void) {
    if (!arp_on) return;
    Uint32 now     = SDL_GetTicks();
    int    step_ms = arp_step_ms();
    int    gate_ms = arp_gate_ms();

    for (int i = 0; i < NUM_CK; i++) {
        if (!actv[i].active || actv[i].num == 0) continue;

        if (arp_st[i].cur_note >= 0 && now >= arp_st[i].note_off_ms)
            arp_stop_note(i);

        if (now >= arp_st[i].next_ms) {
            int total = actv[i].num * arp_octaves;
            if (total == 0) continue;
            int base = actv[i].notes[arp_st[i].pos % actv[i].num];
            int oct  = (arp_st[i].pos / actv[i].num) * 12;
            int midi = base + oct;
            if (midi >= 0 && midi <= 127) {
                arp_stop_note(i);
                amy_event e = amy_default_event();
                e.synth = 0; e.midi_note = (float)midi; e.velocity = 0.75f;
                amy_add_event(&e);
                arp_st[i].cur_note    = midi;
                arp_st[i].note_off_ms = now + gate_ms;
            }
            arp_st[i].next_ms += step_ms;
            if (arp_st[i].next_ms + step_ms < now) arp_st[i].next_ms = now;
            arp_advance(i, total);
        }
    }
}
