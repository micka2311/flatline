// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Stateless Limited

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "flatline.h"

/* --- cross-platform high-res timer to nanoseconds --- */
#if defined(__APPLE__)
#include <mach/mach_time.h>
static uint64_t now_ns(void)
{
  static mach_timebase_info_data_t ti = {0, 0};
  if (ti.denom == 0)
    mach_timebase_info(&ti);
  return (uint64_t)((__uint128_t)mach_absolute_time() * ti.numer / ti.denom);
}
#else
#include <time.h>
static uint64_t now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
#endif

/* deterministic PRNG (splitmix64) just to fill buffers */
static uint64_t sm_state = 0x123456789ABCDEF0ull;
static uint64_t sm_next(void)
{
  uint64_t z = (sm_state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

/* run op for total_bytes ≈ target_bytes; return seconds */
static double bench_memxor(size_t sz, uint8_t *dst, uint8_t *src)
{
  const size_t TARGET = 64ull << 20; /* ~64 MB of total traffic */
  size_t iters = TARGET / (sz ? sz : 1);
  if (iters == 0)
    iters = 1;
  uint64_t t0 = now_ns();
  for (size_t i = 0; i < iters; i++)
    flat_memxor(dst, src, sz);
  uint64_t t1 = now_ns();
  return (double)(t1 - t0) / 1e9;
}

static double bench_memxor_auto(size_t sz, uint8_t *dst, uint8_t *src)
{
  const size_t TARGET = 64ull << 20;
  size_t iters = TARGET / (sz ? sz : 1);
  if (iters == 0)
    iters = 1;
  uint64_t t0 = now_ns();
  for (size_t i = 0; i < iters; i++)
    flat_memxor_auto(dst, src, sz);
  uint64_t t1 = now_ns();
  return (double)(t1 - t0) / 1e9;
}

static double bench_memcpy_when(size_t sz, uint8_t *dst, uint8_t *src, unsigned cond)
{
  const size_t TARGET = 64ull << 20;
  size_t iters = TARGET / (sz ? sz : 1);
  if (iters == 0)
    iters = 1;
  uint64_t t0 = now_ns();
  for (size_t i = 0; i < iters; i++)
    flat_memcpy_when(cond, dst, src, sz);
  uint64_t t1 = now_ns();
  return (double)(t1 - t0) / 1e9;
}

static double bench_memcpy_when_auto(size_t sz, uint8_t *dst, uint8_t *src, unsigned cond)
{
  const size_t TARGET = 64ull << 20;
  size_t iters = TARGET / (sz ? sz : 1);
  if (iters == 0)
    iters = 1;
  uint64_t t0 = now_ns();
  for (size_t i = 0; i < iters; i++)
    flat_memcpy_when_auto(cond, dst, src, sz);
  uint64_t t1 = now_ns();
  return (double)(t1 - t0) / 1e9;
}

static void fill_random(uint8_t *p, size_t n)
{
  for (size_t i = 0; i < n; i++)
    p[i] = (uint8_t)sm_next();
}

int main(void)
{
  static const size_t sizes[] = {
      1, 8, 16, 32, 64, 128, 256, 512,
      1024, 2048, 4096, 16384, 65536, 262144, 1048576};

  const size_t MAX = sizes[sizeof(sizes) / sizeof(sizes[0]) - 1];
  uint8_t *A = (uint8_t *)malloc(MAX);
  uint8_t *B = (uint8_t *)malloc(MAX);
  if (!A || !B)
  {
    fprintf(stderr, "OOM\n");
    return 1;
  }
  fill_random(A, MAX);
  fill_random(B, MAX);

  printf("   size | op           |   byte MB/s |    auto MB/s\n");
  printf("--------+--------------+-------------+-------------\n");

  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
  {
    size_t n = sizes[i];

    double s1 = bench_memxor(n, A, B);
    double mb1 = (n ? (double)n : 1) * (64.0 / 1.0) / s1 / (1024.0 * 1024.0) * (1 << 20); /* simplify: (n*iters)/s; but we fixed to 64MB total -> 64MB/s / (s/1s) */
    /* The above is messy; do a direct calc: total bytes = iters * n ~= 64MB; so MB/s = 64 / seconds */
    (void)mb1; /* re-calc properly below */

    /* Recompute with actual total bytes */
    const size_t TARGET = 64ull << 20;
    double t = s1;
    double mbps_byte = (double)TARGET / t / (1024.0 * 1024.0);

    double s2 = bench_memxor_auto(n, A, B);
    double mbps_auto = (double)TARGET / s2 / (1024.0 * 1024.0);

    printf("%7zu | %-12s | %11.1f MB/s | %11.1f MB/s\n", n, "memxor", mbps_byte, mbps_auto);
  }

  for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
  {
    size_t n = sizes[i];

    const size_t TARGET = 64ull << 20;

    double t1 = bench_memcpy_when(n, A, B, 1);
    double t2 = bench_memcpy_when_auto(n, A, B, 1);

    double mb1 = (double)TARGET / t1 / (1024.0 * 1024.0);
    double mb2 = (double)TARGET / t2 / (1024.0 * 1024.0);

    printf("%7zu | %-12s | %11.1f MB/s | %11.1f MB/s\n", n, "memcpy_when", mb1, mb2);
  }

  free(A);
  free(B);
  return 0;
}
