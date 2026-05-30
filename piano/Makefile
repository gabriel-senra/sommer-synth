CC      = clang
OBJC    = clang
AMY     = ../test-amy/src
BUILD   = build

CFLAGS  = -O2 -Wall \
          -I$(AMY) \
          -I. \
          $(shell sdl2-config --cflags) \
          -DAMY_AUDIO_IS_MINIAUDIO=1 \
          -DMACOS

OBJCFLAGS = $(CFLAGS) -fobjc-arc

LDFLAGS = $(shell sdl2-config --libs) \
          -framework CoreAudio \
          -framework AudioUnit \
          -framework CoreFoundation \
          -framework CoreMIDI \
          -framework Foundation \
          -framework AudioToolbox \
          -framework CoreGraphics \
          -lpthread -lm

AMY_SRCS = \
    $(AMY)/amy.c \
    $(AMY)/api.c \
    $(AMY)/oscillators.c \
    $(AMY)/envelope.c \
    $(AMY)/filters.c \
    $(AMY)/algorithms.c \
    $(AMY)/pcm.c \
    $(AMY)/patches.c \
    $(AMY)/sequencer.c \
    $(AMY)/instrument.c \
    $(AMY)/delay.c \
    $(AMY)/parse.c \
    $(AMY)/transfer.c \
    $(AMY)/log2_exp2.c \
    $(AMY)/interp_partials.c \
    $(AMY)/midi_mappings.c \
    $(AMY)/custom.c \
    $(AMY)/libminiaudio-audio.c \
    $(AMY)/amy_midi.c

AMY_OBJS  = $(patsubst $(AMY)/%.c,   $(BUILD)/%.o,  $(AMY_SRCS))
MIDI_OBJ  = $(BUILD)/macos_midi.o
PIANO_OBJ = $(BUILD)/piano.o

all: $(BUILD) piano

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: $(AMY)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(MIDI_OBJ): $(AMY)/macos_midi.m | $(BUILD)
	$(OBJC) $(OBJCFLAGS) -c $< -o $@

$(PIANO_OBJ): piano.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

piano: $(AMY_OBJS) $(MIDI_OBJ) $(PIANO_OBJ)
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD) piano

.PHONY: all clean
