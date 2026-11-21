#pragma once
#include <SDL.h>
#include "chords.h"  // for Active, NUM_CK

typedef enum { ARP_UP, ARP_DOWN, ARP_PINGPONG, ARP_RANDOM } ArpOrder;

typedef struct {
    int    pos, dir;
    Uint32 next_ms, note_off_ms;
    int    cur_note;
} ArpState;

extern const int   ARP_DIV_VAL[4];
extern const char *ARP_DIV_LBL[4];
extern const char *ARP_ORD_LBL[4];
extern const char *ARP_GATE_LBL[3];
extern const float ARP_GATE_RATIO[3];

int  arp_step_ms(void);
void arp_stop_note(int ci);
void arp_init_key(int ci);
void arp_tick(void);
