# Makefile for flatline: unit tests, dudect-like timing, and micro-bench
# Usage examples:
#   make                # build all
#   make unit           # build flatline_unit
#   make bench          # build & run flatline_bench
#   make dudect         # build & run flatline_dudect (t-test)
#   make clean
#
# SIMD toggles:
#   make CFLAGS_EXTRA="-DFLATLINE_ENABLE_NEON"
#   make AVX2=1        # adds -DFLATLINE_ENABLE_AVX2 -mavx2
#   make SSE2=1        # adds -DFLATLINE_ENABLE_SSE2 -msse2
#
# DUDECT tunables (example):
#   make dudect CFLAGS_EXTRA="-DDU_SAMPLES=40000 -DDU_REPS=16 -DDU_BUFSZ=4096 -DDU_T_THRESHOLD=10.0 -DDU_THRASH_BYTES=16777216 -DDU_THRASH_STRIDE=64"

CC      ?= cc
CFLAGS  ?= -O2 -std=c11 -Wall -Wextra -pedantic -fno-builtin
LDFLAGS ?=
# Extra flags passed from command line if needed
CFLAGS += $(CFLAGS_EXTRA)

# SIMD shortcuts
ifeq ($(AVX2),1)
  CFLAGS += -DFLATLINE_ENABLE_AVX2 -mavx2
endif
ifeq ($(SSE2),1)
  CFLAGS += -DFLATLINE_ENABLE_SSE2 -msse2
endif
ifeq ($(NEON),1)
  CFLAGS += -DFLATLINE_ENABLE_NEON
endif

# Files
HDR = flatline.h
UNIT_SRC   = flatline_unit.c
BENCH_SRC  = flatline_bench.c
DUDECT_SRC = flatline_dudect.c

# Outputs
UNIT_BIN   = flatline_unit
BENCH_BIN  = flatline_bench
DUDECT_BIN = flatline_dudect

.PHONY: all unit bench dudect run-bench run-dudect clean

all: $(UNIT_BIN) $(BENCH_BIN) $(DUDECT_BIN)

$(UNIT_BIN): $(UNIT_SRC) $(HDR)
	$(CC) $(CFLAGS) $(UNIT_SRC) -o $(UNIT_BIN) $(LDFLAGS)

$(BENCH_BIN): $(BENCH_SRC) $(HDR)
	$(CC) $(CFLAGS) $(BENCH_SRC) -o $(BENCH_BIN) $(LDFLAGS)

$(DUDECT_BIN): $(DUDECT_SRC) $(HDR)
	$(CC) $(CFLAGS) $(DUDECT_SRC) -o $(DUDECT_BIN) $(LDFLAGS)

unit: $(UNIT_BIN)
	./$(UNIT_BIN)

bench: $(BENCH_BIN)
	./$(BENCH_BIN)

dudect: $(DUDECT_BIN)
	./$(DUDECT_BIN)

run-bench: bench
run-dudect: dudect

clean:
	rm -f $(UNIT_BIN) $(BENCH_BIN) $(DUDECT_BIN)
