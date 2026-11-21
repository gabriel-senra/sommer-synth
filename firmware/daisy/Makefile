# Project Name
TARGET = amy_daisy
# AMY is compiled from a TRIMMED copy (build/amy_src): AMY's sources overlaid
# with the smaller tables in amy_trim/ (no drums, no piano, Juno bank only).
# See amy_trim/README.md. Runs on every make; only changed files are touched.
AMY = build/amy_src
_ := $(shell ./amy_trim/sync.sh)
APP_TYPE=BOOT_SRAM
OPT = -O3


# Sources
CPP_SOURCES = amy_daisy.cpp
C_SOURCES = amy_daisy_platform.c ${AMY}/amy.c ${AMY}/oscillators.c ${AMY}/algorithms.c ${AMY}/envelope.c ${AMY}/examples.c ${AMY}/filters.c ${AMY}/pcm.c ${AMY}/custom.c ${AMY}/delay.c ${AMY}/log2_exp2.c ${AMY}/patches.c ${AMY}/api.c ${AMY}/sequencer.c ${AMY}/amy_midi.c ${AMY}/instrument.c ${AMY}/parse.c ${AMY}/transfer.c ${AMY}/interp_partials.c ${AMY}/midi_mappings.c


C_INCLUDES += -I${AMY} -DAMY_DAISY -DAMY_NO_MINIAUDIO -Wno-strict-aliasing -Wextra -Wno-unused-parameter -Wpointer-arith -Wno-float-conversion -Wno-missing-declarations

# Library Locations
LIBDAISY_DIR = ../../third_party/libDaisy
DAISYSP_DIR = ../../third_party/DaisySP

# Core location, and generic makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

