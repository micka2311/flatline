// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Stateless Limited

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define FLATLINE_IMPLEMENTATION 1
#include "flatline.h"

/* ---------------------------- test harness ---------------------------- */
static int g_fail = 0, g_pass = 0, g_total = 0;

static void test_failf(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  fputs("[ FAIL ] ", stderr);
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
}

#define TNAME(name) static void name(void)

#define EXPECT_TRUE(cond, ...) \
  do                           \
  {                            \
    g_total++;                 \
    if (!(cond))               \
    {                          \
      g_fail++;                \
      test_failf(__VA_ARGS__); \
    }                          \
    else                       \
    {                          \
      g_pass++;                \
    }                          \
  } while (0)

#define EXPECT_EQ_U64(a, b, ctx) EXPECT_TRUE((uint64_t)(a) == (uint64_t)(b), "%s: expected 0x%016llx == 0x%016llx", ctx, (unsigned long long)(uint64_t)(a), (unsigned long long)(uint64_t)(b))
#define EXPECT_EQ_U32(a, b, ctx) EXPECT_TRUE((uint32_t)(a) == (uint32_t)(b), "%s: expected 0x%08x == 0x%08x", ctx, (unsigned)(uint32_t)(a), (unsigned)(uint32_t)(b))
#define EXPECT_EQ_U16(a, b, ctx) EXPECT_TRUE((uint16_t)(a) == (uint16_t)(b), "%s: expected 0x%04x == 0x%04x", ctx, (unsigned)(uint16_t)(a), (unsigned)(uint16_t)(b))
#define EXPECT_EQ_SZ(a, b, ctx) EXPECT_TRUE((size_t)(a) == (size_t)(b), "%s: expected %zu == %zu", ctx, (size_t)(a), (size_t)(b))
#define EXPECT_EQ_INT(a, b, ctx) EXPECT_TRUE((int)(a) == (int)(b), "%s: expected %d == %d", ctx, (int)(a), (int)(b))
#define EXPECT_MEMEQ(a, b, n, ctx) EXPECT_TRUE(memcmp((a), (b), (n)) == 0, "%s: memory mismatch (n=%zu)", ctx, (size_t)(n))

/* PRNG (splitmix64) for deterministic fuzz */
static uint64_t sm_state = 0x123456789ABCDEF0ull;
static uint64_t sm_next(void)
{
  uint64_t z = (sm_state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}
static uint32_t rnd32(void) { return (uint32_t)sm_next(); }
static uint64_t rnd64(void) { return sm_next(); }

/* ----------------------------- test cases ----------------------------- */

TNAME(test_masks_select)
{
  for (unsigned b = 0; b < 4; b++)
  {
    uint32_t m = flat_mask_from_bit_u32(b);
    EXPECT_EQ_U32(m, (b & 1) ? ~0u : 0u, "mask_from_bit_u32");
    uint32_t z0 = flat_mask_is_zero_u32(0);
    uint32_t z1 = flat_mask_is_zero_u32(123);
    EXPECT_EQ_U32(z0, ~0u, "mask_is_zero_u32(0)");
    EXPECT_EQ_U32(z1, 0u, "mask_is_zero_u32(nz)");
  }
  for (int i = 0; i < 200; i++)
  {
    uint32_t a = rnd32(), b = rnd32();
    uint32_t lt = flat_mask_lt_u32(a, b);
    unsigned lt_bit = flat_mask_to_bit_u32(lt);
    EXPECT_EQ_INT(lt_bit, a < b, "mask_lt_u32");
    uint32_t eq = flat_mask_eq_u32(a, b);
    unsigned eq_bit = flat_mask_to_bit_u32(eq);
    EXPECT_EQ_INT(eq_bit, a == b, "mask_eq_u32");
    uint32_t sel = flat_select_u32(a < b, 111, 222);
    EXPECT_EQ_U32(sel, (a < b) ? 111u : 222u, "select_u32");
  }
}

TNAME(test_rot_bswap)
{
  EXPECT_EQ_U32(flat_rol32(0x11223344u, 8), 0x22334411u, "rol32");
  EXPECT_EQ_U32(flat_ror32(0x11223344u, 8), 0x44112233u, "ror32");
  EXPECT_EQ_U64(flat_rol64(0x1122334455667788ull, 16), 0x3344556677881122ull, "rol64");
  EXPECT_EQ_U64(flat_ror64(0x1122334455667788ull, 16), 0x7788112233445566ull, "ror64");
  EXPECT_EQ_U32(flat_bswap32(0xA1B2C3D4u), 0xD4C3B2A1u, "bswap32");
  EXPECT_EQ_U64(flat_bswap64(0x0011223344556677ull), 0x7766554433221100ull, "bswap64");
}

TNAME(test_endian)
{
  uint8_t b16[2];
  flat_store_be16(b16, 0xABCDu);
  EXPECT_EQ_U16(flat_load_be16(b16), 0xABCDu, "be16");
  flat_store_le16(b16, 0xABCDu);
  EXPECT_EQ_U16(flat_load_le16(b16), 0xABCDu, "le16");
  uint8_t b32[4];
  flat_store_be32(b32, 0x89ABCDEFu);
  EXPECT_EQ_U32(flat_load_be32(b32), 0x89ABCDEFu, "be32");
  flat_store_le32(b32, 0x89ABCDEFu);
  EXPECT_EQ_U32(flat_load_le32(b32), 0x89ABCDEFu, "le32");
  uint8_t b64[8];
  flat_store_be64(b64, 0x0123456789ABCDEFull);
  EXPECT_EQ_U64(flat_load_be64(b64), 0x0123456789ABCDEFull, "be64");
  flat_store_le64(b64, 0x0123456789ABCDEFull);
  EXPECT_EQ_U64(flat_load_le64(b64), 0x0123456789ABCDEFull, "le64");
}

TNAME(test_mem_byte_ops)
{
  const size_t Ns[] = {0, 1, 2, 3, 8, 16, 31, 32, 33, 64, 128, 1024};
  uint8_t A[2048], B[2048], R1[2048], R2[2048];
  for (size_t t = 0; t < sizeof(Ns) / sizeof(Ns[0]); t++)
  {
    size_t n = Ns[t];
    for (size_t i = 0; i < n; i++)
    {
      A[i] = (uint8_t)rnd32();
      B[i] = (uint8_t)rnd32();
      R1[i] = A[i];
      R2[i] = A[i];
    }
    /* memxor vs reference */
    for (size_t i = 0; i < n; i++)
      R1[i] ^= B[i];
    flat_memxor(R2, B, n);
    EXPECT_MEMEQ(R1, R2, n, "memxor");

    /* memcpy_when(cond) equivalence cond=0/1 and mixed */
    memcpy(R2, A, n);
    flat_memcpy_when(0u, R2, B, n);
    EXPECT_MEMEQ(R2, A, n, "memcpy_when cond=0");
    flat_memcpy_when(1u, R2, B, n);
    EXPECT_MEMEQ(R2, B, n, "memcpy_when cond=1");

    /* memswap_when: when=1 swap back to A */
    memcpy(R2, A, n);
    memcpy(R1, B, n);
    flat_memswap_when(1u, R2, R1, n);
    EXPECT_MEMEQ(R2, B, n, "memswap_when cond=1 a");
    EXPECT_MEMEQ(R1, A, n, "memswap_when cond=1 b");

    /* mem_eq & mem_cmp */
    EXPECT_TRUE(flat_mem_eq(A, A, n) == 1, "mem_eq self");
    EXPECT_TRUE(flat_mem_eq(A, B, n) == (memcmp(A, B, n) == 0), "mem_eq AB");
    int c1 = flat_mem_cmp(A, B, n);
    int c2 = memcmp(A, B, n);
    if (c2 < 0)
      c2 = -1;
    else if (c2 > 0)
      c2 = +1;
    EXPECT_EQ_INT(c1, c2, "mem_cmp sign");
  }
}

TNAME(test_typed_ops)
{
  const size_t words = 257;
  uint32_t u32a[words], u32b[words], u32r1[words], u32r2[words];
  for (size_t i = 0; i < words; i++)
  {
    u32a[i] = rnd32();
    u32b[i] = rnd32();
    u32r1[i] = u32a[i];
    u32r2[i] = u32a[i];
  }
  for (size_t i = 0; i < words; i++)
    u32r1[i] ^= u32b[i];
  flat_memxor_u32(u32r2, u32b, words);
  EXPECT_MEMEQ(u32r1, u32r2, words * sizeof(uint32_t), "memxor_u32");

  memcpy(u32r2, u32a, sizeof(u32a));
  flat_memcpy_when_u32(0, u32r2, u32b, words);
  EXPECT_MEMEQ(u32r2, u32a, sizeof(u32a), "memcpy_when_u32 cond=0");
  flat_memcpy_when_u32(1, u32r2, u32b, words);
  EXPECT_MEMEQ(u32r2, u32b, sizeof(u32b), "memcpy_when_u32 cond=1");

  memcpy(u32r2, u32a, sizeof(u32a));
  uint32_t tmp[words];
  memcpy(tmp, u32b, sizeof(u32b));
  flat_memswap_when_u32(1, u32r2, tmp, words);
  EXPECT_MEMEQ(u32r2, u32b, sizeof(u32b), "memswap_when_u32");
}

TNAME(test_lookup_store_zeropad)
{
  uint8_t T[17];
  for (size_t i = 0; i < 17; i++)
    T[i] = (uint8_t)(i * 7u + 3u);
  for (size_t i = 0; i < 17; i++)
  {
    uint8_t v = flat_lookup_u8(T, 17, i);
    EXPECT_EQ_U32(v, T[i], "lookup_u8");
  }
  uint8_t S[17];
  memcpy(S, T, sizeof(T));
  flat_store_at_u8(S, 17, 9, 0xEEu);
  EXPECT_EQ_U32(S[9], 0xEEu, "store_at_u8(9)");

  /* zeropad scan */
  uint8_t Z[16] = {1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  EXPECT_EQ_SZ(flat_zeropad_data_len(Z, 16), 3, "zeropad_data_len=3");
  uint8_t Z2[16] = {0};
  EXPECT_EQ_SZ(flat_zeropad_data_len(Z2, 16), 0, "zeropad_data_len=0");
}

TNAME(test_arith_adc_sbc)
{
  for (int i = 0; i < 2000; i++)
  {
    uint32_t a = rnd32(), b = rnd32();
    unsigned cin = (unsigned)(rnd32() & 1);
    unsigned cout = 0;
    uint32_t s = flat_adc_u32(a, b, cin, &cout);
    uint64_t ref = (uint64_t)a + (uint64_t)b + (uint64_t)cin;
    EXPECT_EQ_U32(s, (uint32_t)ref, "adc_u32 sum");
    EXPECT_EQ_INT(cout, (ref >> 32) != 0, "adc_u32 carry");

    unsigned bout = 0;
    uint32_t r = flat_sbc_u32(a, b, cin, &bout);
    int64_t ref2 = (int64_t)(uint64_t)a - (int64_t)(uint64_t)b - (int64_t)(cin & 1);
    uint32_t rref = (uint32_t)ref2;
    unsigned bref = (ref2 < 0);
    EXPECT_EQ_U32(r, rref, "sbc_u32 diff");
    EXPECT_EQ_INT(bout, bref, "sbc_u32 borrow");
  }
  /* masked variants no-op/op */
  unsigned co = 0;
  EXPECT_EQ_U32(flat_adc_when_u32(0, 10, 20, 1, &co), 10u, "adc_when_u32 cond=0");
  unsigned bo = 0;
  EXPECT_EQ_U32(flat_sbc_when_u32(0, 10, 20, 1, &bo), 10u, "sbc_when_u32 cond=0");
}

TNAME(test_minmaxclamp)
{
  EXPECT_EQ_U32(flat_min_u32(5, 9), 5, "min_u32");
  EXPECT_EQ_U32(flat_max_u32(5, 9), 9, "max_u32");
  EXPECT_EQ_U32(flat_clamp_u32(3, 5, 9), 5, "clamp low");
  EXPECT_EQ_U32(flat_clamp_u32(13, 5, 9), 9, "clamp high");
  EXPECT_EQ_U32(flat_clamp_u32(7, 5, 9), 7, "clamp mid");
}

TNAME(test_table_apply)
{
  uint8_t sbox[256];
  for (int i = 0; i < 256; i++)
    sbox[i] = (uint8_t)((i * 29 + 7) & 0xFF);
  uint8_t in[257], out1[257], out2[257];
  for (size_t i = 0; i < 257; i++)
    in[i] = (uint8_t)rnd32();
  /* reference via lookup */
  for (size_t i = 0; i < 257; i++)
    out1[i] = flat_lookup_u8(sbox, 256, in[i]);
  flat_table_apply_u8(out2, in, 257, sbox, 256);
  EXPECT_MEMEQ(out1, out2, 257, "table_apply_u8");
}

TNAME(test_auto_vs_scalar)
{
  static const size_t SIZES[] = {0, 1, 15, 16, 31, 32, 63, 64, 65, 256, 4096};
  const int TRIALS = 8; /* bump to 64 in EXTENDED mode */

  uint8_t bufA[8192], bufB[8192], r1[8192], r2[8192];

  for (int trial = 0; trial < TRIALS; trial++)
  {
    for (size_t si = 0; si < sizeof(SIZES) / sizeof(SIZES[0]); si++)
    {
      size_t n = SIZES[si];

      /* try aligned and misaligned (+1) */
      for (int mis = 0; mis < 2; mis++)
      {
        size_t off = mis ? 1 : 0;
        uint8_t *a = bufA + off, *b = bufB + off, *o1 = r1 + off, *o2 = r2 + off;

        for (size_t i = 0; i < n + off; i++)
        {
          bufA[i] = (uint8_t)rnd32();
          bufB[i] = (uint8_t)rnd32();
          r1[i] = bufA[i];
          r2[i] = bufA[i];
        }

        /* memxor */
        memcpy(o1, a, n);
        memcpy(o2, a, n);
        flat_memxor(o1, b, n);
        flat_memxor_auto(o2, b, n);
        EXPECT_MEMEQ(o1, o2, n, "auto memxor == scalar");

        /* memcpy_when(cond=0): should be no-op for both */
        memcpy(o1, a, n);
        memcpy(o2, a, n);
        flat_memcpy_when(0, o1, b, n);
        flat_memcpy_when_auto(0, o2, b, n);
        EXPECT_MEMEQ(o1, o2, n, "auto memcpy_when cond=0");

        /* memcpy_when(cond=1): full copy for both */
        memcpy(o1, a, n);
        memcpy(o2, a, n);
        flat_memcpy_when(1, o1, b, n);
        flat_memcpy_when_auto(1, o2, b, n);
        EXPECT_MEMEQ(o1, o2, n, "auto memcpy_when cond=1");

        /* memswap_when(cond=1): swap both sides */
        memcpy(o1, a, n);
        memcpy(o2, a, n);
        uint8_t t1[8192], t2[8192];
        memcpy(t1, b, n);
        memcpy(t2, b, n);
        flat_memswap_when(1, o1, t1, n);
        flat_memswap_when_auto(1, o2, t2, n);
        EXPECT_MEMEQ(o1, o2, n, "auto memswap_when cond=1");
      }
    }
  }
}

TNAME(test_division_ct)
{
  /* Random checks for 64-bit and 32-bit */
  for (int i = 0; i < 500; i++)
  {
    uint64_t n = rnd64();
    uint64_t d = rnd64() | 1ull; /* non-zero */
    uint64_t q = 0, r = 0;
    unsigned ok = flat_div_mod_ct_u64(n, d, &q, &r);
    EXPECT_EQ_INT(ok, 1, "div64 ok==1");
    EXPECT_EQ_U64(n, q * d + r, "div64 identity");
    EXPECT_TRUE(r < d, "div64 remainder<d");
  }
  /* zero divisor */
  {
    uint64_t q = 1, r = 2;
    unsigned ok = flat_div_mod_ct_u64(123, 0, &q, &r);
    EXPECT_EQ_INT(ok, 0, "div64 by zero ok==0");
    EXPECT_EQ_U64(q, 0, "div64 by zero q=0");
    EXPECT_EQ_U64(r, 0, "div64 by zero r=0");
  }
  /* 32-bit */
  for (int i = 0; i < 500; i++)
  {
    uint32_t n = (uint32_t)rnd32();
    uint32_t d = (uint32_t)rnd32() | 1u;
    uint32_t q = 0, r = 0;
    unsigned ok = flat_div_mod_ct_u32(n, d, &q, &r);
    EXPECT_EQ_INT(ok, 1, "div32 ok==1");
    EXPECT_EQ_U32(n, q * d + r, "div32 identity");
    EXPECT_TRUE(r < d, "div32 remainder<d");
  }
}

TNAME(test_zero_move_and_misc)
{
  uint32_t x = 0xA5A5A5A5u;
  flat_zero_when_u32(0, &x);
  EXPECT_EQ_U32(x, 0xA5A5A5A5u, "zero_when_u32 cond=0");
  flat_zero_when_u32(1, &x);
  EXPECT_EQ_U32(x, 0u, "zero_when_u32 cond=1");

  uint64_t y = 0x1122334455667788ull;
  flat_mov_when_u64(0, &y, 0x0ull);
  EXPECT_EQ_U64(y, 0x1122334455667788ull, "mov_when_u64 cond=0");
  flat_mov_when_u64(1, &y, 0xCAFEBABEDEADBEEFull);
  EXPECT_EQ_U64(y, 0xCAFEBABEDEADBEEFull, "mov_when_u64 cond=1");

  /* select_block */
  uint8_t blocks[3][7];
  for (int i = 0; i < 3; i++)
    for (int k = 0; k < 7; k++)
      blocks[i][k] = (uint8_t)(10 * i + k);
  uint8_t out[7];
  flat_select_block(out, blocks, 3, 7, 2);
  for (int k = 0; k < 7; k++)
    EXPECT_EQ_U32(out[k], blocks[2][k], "select_block idx=2");

  /* erracc / commit */
  uint8_t dst[8] = {1, 1, 1, 1, 1, 1, 1, 1}, tmp[8] = {9, 9, 9, 9, 9, 9, 9, 9};
  flat_erracc_t e;
  flat_erracc_init(&e);
  flat_erracc_or(&e, 0); /* still ok */
  flat_commit_if_ok(flat_erracc_ok(&e), dst, tmp, 8);
  EXPECT_MEMEQ(dst, tmp, 8, "commit_if_ok ok");

  flat_erracc_init(&e);
  flat_erracc_or(&e, 1); /* fail */
  memset(dst, 1, 8);
  flat_commit_if_ok(flat_erracc_ok(&e), dst, tmp, 8);
  for (int i = 0; i < 8; i++)
    EXPECT_EQ_U32(dst[i], 1, "commit_if_ok fail keeps dst");
}

TNAME(test_sort_and_masks)
{
  uint32_t a = 9, b = 3;
  flat_cswap32(1, &a, &b);
  EXPECT_EQ_U32(a, 3, "cswap32 a");
  EXPECT_EQ_U32(b, 9, "cswap32 b");
  uint32_t v[4] = {7, 4, 9, 1};
  flat_sort4_u32(v);
  EXPECT_TRUE(v[0] <= v[1] && v[1] <= v[2] && v[2] <= v[3], "sort4_u32 non-decreasing");

  /* memeq_mask */
  uint8_t s1[5] = {1, 2, 3, 4, 5}, s2[5] = {1, 2, 3, 4, 5}, s3[5] = {1, 2, 4, 4, 5};
  uint32_t m1 = flat_memeq_mask(s1, s2, 5);
  uint32_t m2 = flat_memeq_mask(s1, s3, 5);
  EXPECT_EQ_INT(flat_mask_to_bit_u32(m1), 1, "memeq_mask equal");
  EXPECT_EQ_INT(flat_mask_to_bit_u32(m2), 0, "memeq_mask diff");
}

TNAME(test_index_clamp_masked_load)
{
  uint8_t buf[16];
  for (int i = 0; i < 16; i++)
    buf[i] = (uint8_t)i;
  for (int i = -5; i < 21; i++)
  {
    size_t idx = (size_t)(i < 0 ? 0 : i);
    size_t cl = flat_index_clamp(idx, 16);
    EXPECT_TRUE(cl == ((idx < 16) ? idx : 0), "index_clamp within bounds or 0");
    uint8_t v = flat_masked_load_u8(buf, 16, idx);
    EXPECT_EQ_U32(v, buf[cl], "masked_load_u8");
  }
}

/* ------------------------------- main --------------------------------- */

int main(void)
{
  printf("flatline_unit: running tests…\n");

  test_masks_select();
  test_rot_bswap();
  test_endian();
  test_mem_byte_ops();
  test_typed_ops();
  test_lookup_store_zeropad();
  test_arith_adc_sbc();
  test_minmaxclamp();
  test_table_apply();
  test_auto_vs_scalar();
  test_division_ct();
  test_zero_move_and_misc();
  test_sort_and_masks();
  test_index_clamp_masked_load();

  printf("\n==== SUMMARY ====\n");
  printf("Total: %d  |  Pass: %d  |  Fail: %d\n", g_total, g_pass, g_fail);
  printf("Overall: %s\n", (g_fail == 0) ? "PASS" : "FAIL");
  return (g_fail == 0) ? 0 : 1;
}
