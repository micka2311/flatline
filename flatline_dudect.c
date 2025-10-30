// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Stateless Limited

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define FLATLINE_IMPLEMENTATION 1
#include "flatline.h"

/* -------------------- defaults -------------------- */
#ifndef DU_SAMPLES
#define DU_SAMPLES 20000
#endif
#ifndef DU_REPS
#define DU_REPS 8
#endif
#ifndef DU_BUFSZ
#define DU_BUFSZ 1024
#endif
#ifndef DU_T_THRESHOLD
#define DU_T_THRESHOLD 10.0
#endif
#ifndef DU_THRASH_BYTES
#define DU_THRASH_BYTES (16 * 1024 * 1024)
#endif
#ifndef DU_THRASH_STRIDE
#define DU_THRASH_STRIDE 64
#endif

/* ---------------- timer (ns) ---------------- */
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

/* ------------ PRNG: splitmix64 ------------ */
static uint64_t sm_state = 0x123456789ABCDEF0ull;
static uint64_t sm_next(void)
{
  uint64_t z = (sm_state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}
static void fill_random(uint8_t *p, size_t n)
{
  for (size_t i = 0; i < n; i++)
    p[i] = (uint8_t)sm_next();
}
static uint32_t rnd32(void) { return (uint32_t)sm_next(); }
static uint64_t rnd64(void) { return sm_next(); }

/* ------------ cache thrash -------------- */
static uint8_t *thrash_buf = NULL;
static void thrash_cache(void)
{
#if DU_THRASH_BYTES > 0
  if (!thrash_buf)
    return;
  volatile uint8_t sink = 0;
  for (size_t off = 0; off < (size_t)DU_THRASH_BYTES; off += (size_t)DU_THRASH_STRIDE)
  {
    sink ^= thrash_buf[off];
  }
  (void)sink;
#else
  (void)thrash_buf;
#endif
}

/* ============ target function signature ============ */
typedef void (*du_fn)(uint8_t *a, uint8_t *b, size_t n, int secret_class);

/* ============ negative controls (INTENTIONALLY LEAKY) ============ */

/* memcmp — early exit */
static volatile int v_acc_i = 0;
static void t_memcmp_leaky(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  memcpy(b, a, n);
  if (n)
  {
    if (secret == 0)
      b[0] ^= 1; /* early difference */
    else
      b[n - 1] ^= 1; /* late difference  */
  }
  int s = 0;
  for (int r = 0; r < DU_REPS; r++)
    s += memcmp(a, b, n);
  v_acc_i ^= s;
}

/* zeros padding — early return scan from end */
static size_t zeros_padding_leaky(const uint8_t *buf, size_t len)
{
  for (size_t i = len; i > 0; i--)
  {
    if (buf[i - 1] != 0)
      return i;
  }
  return 0;
}
static volatile size_t v_acc_sz = 0;
static void t_zeropad_leaky(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  (void)b;
  memset(a, 0, n);
  if (n)
  {
    /* last non-zero position differs by class */
    size_t pos = (secret == 0) ? (n / 16) : (n - n / 16 - 1);
    a[pos] = 1;
  }
  size_t sum = 0;
  for (int r = 0; r < DU_REPS; r++)
    sum += zeros_padding_leaky(a, n);
  v_acc_sz ^= sum;
}

/* secret-index direct lookup (leaky) */
static volatile uint32_t v_acc_u32 = 0;
static void t_lookup_leaky(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  (void)b;
  (void)n;
  /* build table in a; a[256] is enough */
  for (int i = 0; i < 256; i++)
    a[i] = (uint8_t)(i * 29u + 7u);
  uint32_t sum = 0;
  for (int r = 0; r < DU_REPS; r++)
  {
    uint8_t idx = (secret == 0) ? 0u : (uint8_t)rnd32();
    sum += a[idx]; /* direct, secret-indexed access */
  }
  v_acc_u32 ^= sum;
}

/* table apply (leaky) — direct sbox[in[i]] per byte */
static void t_table_apply_leaky(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  /* sbox in a[0..255], input in b[0..n-1] */
  for (int i = 0; i < 256; i++)
    a[i] = (uint8_t)((i * 53u + 11u) & 0xFF);
  for (size_t i = 0; i < n; i++)
    b[i] = (uint8_t)rnd32();

  /* Tilt the class distribution of indices to stress different cache lines. */
  if (secret == 0)
  {
    for (size_t i = 0; i < n; i++)
      b[i] &= 0x1F; /* low range 0..31  */
  }
  else
  {
    for (size_t i = 0; i < n; i++)
      b[i] = (b[i] & 0x1F) | 0xE0; /* high range 224..255 */
  }

  uint32_t sum = 0;
  for (int r = 0; r < DU_REPS; r++)
  {
    for (size_t i = 0; i < n; i++)
      sum += a[b[i]]; /* direct, secret-indexed */
  }
  v_acc_u32 ^= sum;
}

/* masked load (leaky) — branch on bounds (idx secret) */
static void t_masked_load_leaky(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  (void)b;
  for (size_t i = 0; i < n; i++)
    a[i] = (uint8_t)rnd32();
  uint32_t sum = 0;
  for (int r = 0; r < DU_REPS; r++)
  {
    size_t idx = (secret == 0) ? (size_t)(n / 2) : (size_t)(n + 5); /* out-of-bounds class */
    uint8_t v = 0;
    if (idx < n)
      v = a[idx]; /* branch on secret */
    sum += v;
  }
  v_acc_u32 ^= sum;
}

/* secret-conditioned memcpy (leaky) — branches on cond */
static void t_memcpy_when_leaky(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  /* a: dst, b: src */
  for (size_t i = 0; i < n; i++)
  {
    a[i] = (uint8_t)rnd32();
    b[i] = (uint8_t)rnd32();
  }
  for (int r = 0; r < DU_REPS; r++)
  {
    if (secret)
      memcpy(a, b, n); /* secret controls branch/work */
  }
  v_acc_u32 ^= a[0];
}

/* secret-conditioned memswap (leaky) — branches on cond */
static void t_memswap_when_leaky(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  for (size_t i = 0; i < n; i++)
  {
    a[i] = (uint8_t)rnd32();
    b[i] = (uint8_t)rnd32();
  }
  for (int r = 0; r < DU_REPS; r++)
  {
    if (secret)
    {
      for (size_t i = 0; i < n; i++)
      {
        uint8_t t = a[i];
        a[i] = b[i];
        b[i] = t;
      }
    }
  }
  v_acc_u32 ^= a[0];
}

/* leaky division/mod (% and /) — many CPUs have data-dependent latency */
static volatile uint64_t v_acc_u64 = 0;
static void t_divmod_leaky(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  (void)a;
  (void)b;
  (void)n;
  uint64_t sum = 0;
  for (int r = 0; r < DU_REPS; r++)
  {
    uint64_t num = rnd64();
    uint64_t den = (secret == 0) ? 3ull : ((rnd64() | 1ull) & 0x7fffffffffffffffull);
    /* NB: avoid div by zero; choose a fixed small vs random large */
    uint64_t q = num / den;
    uint64_t m = num % den;
    sum ^= (q + 31 * m);
  }
  v_acc_u64 ^= sum;
}

/* ============ CT counterparts (should be time-constant across classes) ============ */

/* flat_mem_cmp — constant-time */
static void t_memcmp_ct(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  memcpy(b, a, n);
  if (n)
  {
    if (secret == 0)
      b[0] ^= 1;
    else
      b[n - 1] ^= 1;
  }
  int s = 0;
  for (int r = 0; r < DU_REPS; r++)
    s += flat_mem_cmp(a, b, n);
  v_acc_i ^= s;
}

/* flat_zeropad_data_len — constant-time scan */
static void t_zeropad_ct(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  (void)b;
  memset(a, 0, n);
  if (n)
  {
    size_t pos = (secret == 0) ? (n / 16) : (n - n / 16 - 1);
    a[pos] = 1;
  }
  size_t sum = 0;
  for (int r = 0; r < DU_REPS; r++)
    sum += flat_zeropad_data_len(a, n);
  v_acc_sz ^= sum;
}

/* flat_lookup_u8 — table sweep per lookup (CT) */
static void t_lookup_ct(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  (void)b;
  (void)n;
  for (int i = 0; i < 256; i++)
    a[i] = (uint8_t)(i * 29u + 7u);
  uint32_t sum = 0;
  for (int r = 0; r < DU_REPS; r++)
  {
    size_t idx = (secret == 0) ? 0u : (size_t)(rnd32() & 0xFFu);
    sum += flat_lookup_u8(a, 256, idx); /* CT sweep */
  }
  v_acc_u32 ^= sum;
}

/* flat_table_apply_u8 — CT */
static void t_table_apply_ct(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  for (int i = 0; i < 256; i++)
    a[i] = (uint8_t)((i * 53u + 11u) & 0xFF);
  for (size_t i = 0; i < n; i++)
    b[i] = (uint8_t)rnd32();
  if (secret == 0)
  {
    for (size_t i = 0; i < n; i++)
      b[i] &= 0x1F;
  }
  else
  {
    for (size_t i = 0; i < n; i++)
      b[i] = (b[i] & 0x1F) | 0xE0;
  }
  uint8_t *out = (uint8_t *)alloca(n);
  for (int r = 0; r < DU_REPS; r++)
    flat_table_apply_u8(out, b, n, a, 256);
  v_acc_u32 ^= out[0];
}

/* flat_masked_load_u8 — CT */
static void t_masked_load_ct(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  (void)b;
  for (size_t i = 0; i < n; i++)
    a[i] = (uint8_t)rnd32();
  uint32_t sum = 0;
  for (int r = 0; r < DU_REPS; r++)
  {
    size_t idx = (secret == 0) ? (size_t)(n / 2) : (size_t)(n + 5);
    sum += flat_masked_load_u8(a, n, idx); /* scans safely */
  }
  v_acc_u32 ^= sum;
}

/* flat_memcpy_when — CT with secret cond */
static void t_memcpy_when_ct(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  /* a: dst, b: src */
  for (size_t i = 0; i < n; i++)
  {
    a[i] = (uint8_t)rnd32();
    b[i] = (uint8_t)rnd32();
  }
  for (int r = 0; r < DU_REPS; r++)
  {
    unsigned cond = (unsigned)(secret & 1);
    flat_memcpy_when(cond, a, b, n); /* CT select/blend */
  }
  v_acc_u32 ^= a[0];
}

/* flat_memswap_when — CT with secret cond */
static void t_memswap_when_ct(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  for (size_t i = 0; i < n; i++)
  {
    a[i] = (uint8_t)rnd32();
    b[i] = (uint8_t)rnd32();
  }
  for (int r = 0; r < DU_REPS; r++)
  {
    unsigned cond = (unsigned)(secret & 1);
    flat_memswap_when(cond, a, b, n); /* CT masked swap */
  }
  v_acc_u32 ^= a[0];
}

/* flat_div_mod_ct_u64 — CT integer division */
static void t_divmod_ct(uint8_t *a, uint8_t *b, size_t n, int secret)
{
  (void)a;
  (void)b;
  (void)n;
  uint64_t sum = 0;
  for (int r = 0; r < DU_REPS; r++)
  {
    uint64_t num = rnd64();
    uint64_t den = (secret == 0) ? 3ull : ((rnd64() | 1ull) & 0x7fffffffffffffffull);
    uint64_t q = 0, rem = 0;
    unsigned ok = flat_div_mod_ct_u64(num, den, &q, &rem);
    /* den non-zero -> ok must be 1 */
    sum ^= ((uint64_t)ok << 63) ^ (q + 31 * rem);
  }
  v_acc_u64 ^= sum;
}

/* ============ measurement harness ============ */

static double t_test_run(const char *label, du_fn fn, size_t n)
{
  uint8_t *A = (uint8_t *)malloc(n ? n : 1);
  uint8_t *B = (uint8_t *)malloc(n ? n : 1);
  if (!A || !B)
  {
    fprintf(stderr, "OOM\n");
    exit(1);
  }

  double *g0 = (double *)malloc(sizeof(double) * DU_SAMPLES);
  double *g1 = (double *)malloc(sizeof(double) * DU_SAMPLES);
  if (!g0 || !g1)
  {
    fprintf(stderr, "OOM\n");
    exit(1);
  }

  for (int s = 0; s < DU_SAMPLES; s++)
  {
    fill_random(A, n);
    fill_random(B, n);

    thrash_cache();
    uint64_t t0 = now_ns();
    fn(A, B, n, 0);
    uint64_t t1 = now_ns();
    g0[s] = (double)(t1 - t0);

    fill_random(A, n);
    fill_random(B, n);

    thrash_cache();
    uint64_t t2 = now_ns();
    fn(A, B, n, 1);
    uint64_t t3 = now_ns();
    g1[s] = (double)(t3 - t2);
  }

  double m0 = 0, m1 = 0;
  for (int s = 0; s < DU_SAMPLES; s++)
  {
    m0 += g0[s];
    m1 += g1[s];
  }
  m0 /= DU_SAMPLES;
  m1 /= DU_SAMPLES;
  double v0 = 0, v1 = 0;
  for (int s = 0; s < DU_SAMPLES; s++)
  {
    double d0 = g0[s] - m0;
    v0 += d0 * d0;
    double d1 = g1[s] - m1;
    v1 += d1 * d1;
  }
  v0 /= (DU_SAMPLES - 1);
  v1 /= (DU_SAMPLES - 1);

  double t = (m0 - m1) / sqrt(v0 / DU_SAMPLES + v1 / DU_SAMPLES);

  printf("[DU] %-18s | samples=%d*2 reps=%d | mean0=%.1fns mean1=%.1fns | t=%.2f | %s\n",
         label, DU_SAMPLES, DU_REPS, m0, m1, t, (fabs(t) > DU_T_THRESHOLD ? "LEAK" : "OK"));

  free(g0);
  free(g1);
  free(A);
  free(B);
  return t;
}

/* ------------------------------- main --------------------------------- */

int main(void)
{
#if DU_THRASH_BYTES > 0
  thrash_buf = (uint8_t *)malloc((size_t)DU_THRASH_BYTES);
  if (thrash_buf)
    memset(thrash_buf, 1, (size_t)DU_THRASH_BYTES);
#endif

  printf("DUDECT-like timing check (ns): DU_SAMPLES=%d, DU_REPS=%d, BUFSZ=%d, Tthr=%.1f, Thrash=%db\n",
         DU_SAMPLES, DU_REPS, DU_BUFSZ, DU_T_THRESHOLD, (int)DU_THRASH_BYTES);

  /* 1) Comparators */
  t_test_run("memcmp (leaky)     ", t_memcmp_leaky, DU_BUFSZ);
  t_test_run("flat_mem_cmp (CT)  ", t_memcmp_ct, DU_BUFSZ);

  /* 2) Zero padding scans */
  t_test_run("zeropad (leaky)    ", t_zeropad_leaky, DU_BUFSZ);
  t_test_run("flat_zeropad (CT)  ", t_zeropad_ct, DU_BUFSZ);

  /* 3) Secret-index memory */
  t_test_run("lookup (leaky)     ", t_lookup_leaky, 256);
  t_test_run("flat_lookup (CT)   ", t_lookup_ct, 256);

  t_test_run("tbl_apply (leaky)  ", t_table_apply_leaky, DU_BUFSZ);
  t_test_run("tbl_apply (CT)     ", t_table_apply_ct, DU_BUFSZ);

  t_test_run("masked_load (leaky)", t_masked_load_leaky, DU_BUFSZ);
  t_test_run("masked_load (CT)   ", t_masked_load_ct, DU_BUFSZ);

  /* 4) Secret-conditioned block ops (use CT scalar APIs, not *_auto fast-paths) */
  t_test_run("memcpy_when (leaky)", t_memcpy_when_leaky, DU_BUFSZ);
  t_test_run("memcpy_when (CT)   ", t_memcpy_when_ct, DU_BUFSZ);

  t_test_run("memswap_when(leaky)", t_memswap_when_leaky, DU_BUFSZ);
  t_test_run("memswap_when (CT)  ", t_memswap_when_ct, DU_BUFSZ);

  /* 5) Division / modulo */
  t_test_run("divmod (leaky /,%) ", t_divmod_leaky, DU_BUFSZ);
  t_test_run("divmod (CT)        ", t_divmod_ct, DU_BUFSZ);

#if DU_THRASH_BYTES > 0
  free(thrash_buf);
#endif
  return 0;
}
