// pcm_tiny.h — TRIMMED for the sommer-synth Daisy build (overlays AMY's version).
//
// AMY's original ships 11 Roland 808 drum samples (~102 KB of int16 audio).
// A chord synth doesn't need them, and on the Daisy every byte of constant data
// sits in the 480 KB SRAM next to the code. We keep the same names/shape so AMY
// compiles unchanged, but with a single silent 1-sample "drum".
// Effect: PCM waves (wave=PCM, the GM drum synth) play silence.
#ifndef __PCM_H
#define __PCM_H
#define PCM_AMY_SAMPLE_RATE 22050
#define PCM_BASE_SAMPLES 1
#define PCM_BASE_LENGTH 1
#define PCM_WAVETABLE_BASE PCM_BASE_SAMPLES
#define PCM_WAVETABLE_SAMPLES 0
#define PCM_WAVETABLE_LEN 0
#define PCM_LENGTH PCM_BASE_LENGTH
#define PCM_MAP_ENTRIES PCM_BASE_SAMPLES
const int16_t pcm[PCM_LENGTH] PROGMEM = { 0 };
const uint16_t pcm_samples = PCM_MAP_ENTRIES;
const uint16_t pcm_wavetable_base = PCM_WAVETABLE_BASE;
const uint16_t pcm_wavetable_samples = PCM_WAVETABLE_SAMPLES;
const uint32_t pcm_wavetable_len = PCM_WAVETABLE_LEN;
const pcm_map_t pcm_map[PCM_MAP_ENTRIES] PROGMEM = {
    {0, 1, 0, 1, 60},  // offset, length, loopstart, loopend, midinote
};
#endif  // __PCM_H
