CC     = clang
AMY    = ../test-amy/src
# Reuse already-compiled AMY objects from the piano build
BUILD  = ../piano/build

CFLAGS = -O2 -Wall \
         -I$(AMY) \
         -I. \
         -I../piano \
         $(shell sdl2-config --cflags) \
         -DAMY_AUDIO_IS_MINIAUDIO=1 \
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

AMY_OBJS = \
    $(BUILD)/amy.o \
    $(BUILD)/api.o \
    $(BUILD)/oscillators.o \
    $(BUILD)/envelope.o \
    $(BUILD)/filters.o \
    $(BUILD)/algorithms.o \
    $(BUILD)/pcm.o \
    $(BUILD)/patches.o \
    $(BUILD)/sequencer.o \
    $(BUILD)/instrument.o \
    $(BUILD)/delay.o \
    $(BUILD)/parse.o \
    $(BUILD)/transfer.o \
    $(BUILD)/log2_exp2.o \
    $(BUILD)/interp_partials.o \
    $(BUILD)/midi_mappings.o \
    $(BUILD)/custom.o \
    $(BUILD)/libminiaudio-audio.o \
    $(BUILD)/amy_midi.o \
    $(BUILD)/macos_midi.o

all: chord_piano

chord_piano.o: chord_piano.c
	$(CC) $(CFLAGS) -c $< -o $@

chord_piano: chord_piano.o $(AMY_OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -f chord_piano.o chord_piano

.PHONY: all clean
