CC     = clang
AMY    = ../third_party/amy
STB    = ../third_party/stb
CORE   = ../core
BUILD  = build

CFLAGS = -O2 -Wall \
         -I$(AMY) \
         -I. \
         -I$(CORE) \
         -I$(STB) \
         $(shell sdl2-config --cflags) \
         -DMACOS

LDFLAGS = $(shell sdl2-config --libs) \
          -framework CoreAudio \
          -framework AudioUnit \
          -framework CoreFoundation \
          -framework CoreMIDI \
          -framework Foundation \
          -framework AudioToolbox \
          -framework CoreGraphics \
          -lpthread -lm

# AMY engine, compiled from the vendored source into build/amy/
AMY_SRCS = amy api oscillators envelope filters algorithms pcm patches \
           sequencer instrument delay parse transfer log2_exp2 \
           interp_partials midi_mappings custom libminiaudio-audio amy_midi
AMY_OBJS = $(AMY_SRCS:%=$(BUILD)/amy/%.o) $(BUILD)/amy/macos_midi.o

# Portable core (shared with firmware) + desktop sources
CORE_SRCS = chords
SRCS = main.c state.c arp.c synth.c fx.c display.c transport.c
OBJS = $(SRCS:%.c=$(BUILD)/%.o) $(CORE_SRCS:%=$(BUILD)/core/%.o)

all: chord_piano_v2

$(BUILD)/%.o: %.c | $(BUILD)/amy
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/core/%.o: $(CORE)/%.c | $(BUILD)/core
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/amy/%.o: $(AMY)/%.c | $(BUILD)/amy
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/amy/macos_midi.o: $(AMY)/macos_midi.m | $(BUILD)/amy
	$(CC) $(CFLAGS) -fobjc-arc -c $< -o $@

$(BUILD)/amy $(BUILD)/core:
	mkdir -p $@

chord_piano_v2: $(OBJS) $(AMY_OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD) chord_piano_v2

.PHONY: all clean
