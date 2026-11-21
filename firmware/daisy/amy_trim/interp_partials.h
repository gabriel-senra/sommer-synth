// interp_partials.h — TRIMMED for the sommer-synth Daisy build (overlays AMY's version).
//
// AMY's original holds the "dpwe piano": per-note, per-velocity harmonic
// envelopes measured from a real piano (~80 KB). We don't use it. This keeps
// the table shape valid (2 notes x 2 velocities, so interpolation never reads
// out of bounds) but with zero harmonics, so a piano note renders silence.
#ifndef __INTERP_PARTIALS_H
#define __INTERP_PARTIALS_H

#define NUM_INTERP_PARTIALS_PRESETS 1
#define NUM_PIANO_SAMPLE_TIMES_MS 1
const uint16_t piano_sample_times_ms[NUM_PIANO_SAMPLE_TIMES_MS] PROGMEM = { 100 };
#define NUM_PIANO_VELOCITIES 2
const uint8_t piano_velocities[NUM_PIANO_VELOCITIES] PROGMEM = { 1, 127 };
#define NUM_PIANO_NOTES 2
const uint8_t piano_notes[NUM_PIANO_NOTES] PROGMEM = { 21, 108 };
const uint8_t piano_num_harmonics[NUM_PIANO_NOTES * NUM_PIANO_VELOCITIES] PROGMEM = { 0, 0, 0, 0 };
const uint16_t piano_harmonics_freq[1] PROGMEM = { 0 };
const uint8_t piano_harmonics_mags[1] PROGMEM = { 0 };

const interp_partials_voice_t interp_partials_map[NUM_INTERP_PARTIALS_PRESETS] PROGMEM = {
    {
        NUM_PIANO_SAMPLE_TIMES_MS, piano_sample_times_ms,
        NUM_PIANO_VELOCITIES, piano_velocities,
        NUM_PIANO_NOTES, piano_notes,
        piano_num_harmonics, piano_harmonics_freq, piano_harmonics_mags,
    },
};
#endif // __INTERP_PARTIALS_H
