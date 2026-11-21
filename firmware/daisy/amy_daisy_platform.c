// amy_daisy_platform.c
// Platform hooks AMY expects every target to provide.
//
// On ESP32 and RP2040, AMY's i2s.c implements these: it starts its own I2S
// driver and render tasks. On the Daisy, libDaisy owns the audio codec and
// calls our AudioCallback, which pulls a block from AMY. So AMY has no
// platform work to do here, and these hooks are intentionally empty.
//
// These live here rather than in third_party/amy so that AMY can be updated
// without losing our changes.

#include "amy.h"

void amy_platform_init(void)   {}
void amy_platform_deinit(void) {}

// AMY's amy_midi.c defines run_midi() for AMY_DAISY but not stop_midi().
// MIDI is handled by libDaisy's MidiUartHandler in main().
void stop_midi(void) {}
