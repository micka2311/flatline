// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Stateless Limited

#ifndef FLATLINE_H
#define FLATLINE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h> /* for memset_s when available */

#ifdef __cplusplus
extern "C"
{
#endif

/*------------------------------------------------------------------------------
  Version
------------------------------------------------------------------------------*/
#define FLATLINE_VERSION_MAJOR 0
#define FLATLINE_VERSION_MINOR 1
#define FLATLINE_VERSION_PATCH 0
#define FLATLINE_VERSION_HEX 0x000100

/*------------------------------------------------------------------------------
  Config / inline / restrict
------------------------------------------------------------------------------*/
#ifndef FLAT_NO_BARRIER
#if defined(_MSC_VER)
#include <intrin.h>
#define FLAT_COMPILER_BARRIER() _ReadWriteBarrier()
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define FLAT_COMPILER_BARRIER() atomic_signal_fence(memory_order_seq_cst)
#elif defined(__GNUC__) || defined(__clang__)
#define FLAT_COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
#else
#define FLAT_COMPILER_BARRIER() \
  do                            \
  {                             \
  } while (0)
#endif
#else
#define FLAT_COMPILER_BARRIER() \
  do                            \
  {                             \
  } while (0)
#endif

#if defined(_MSC_VER)
#define FLAT_INLINE static __inline
#else
#define FLAT_INLINE static inline
#endif

#if defined(__cplusplus)
#define FLAT_RESTRICT
#else
#define FLAT_RESTRICT restrict
#endif

/* Auto-enable NEON on aarch64 unless explicitly disabled */
#if defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__)) && !defined(FLATLINE_DISABLE_NEON)
#ifndef FLATLINE_ENABLE_NEON
#define FLATLINE_ENABLE_NEON
#endif
#endif

  /*------------------------------------------------------------------------------
    Masks & predicates (branchless)
  ------------------------------------------------------------------------------*/
  FLAT_INLINE uint8_t flat_mask_from_bit_u8(unsigned bit) { return (uint8_t)((uint8_t)0 - (uint8_t)(bit & 1u)); }
  FLAT_INLINE uint16_t flat_mask_from_bit_u16(unsigned bit) { return (uint16_t)((uint16_t)0 - (uint16_t)(bit & 1u)); }
  FLAT_INLINE uint32_t flat_mask_from_bit_u32(unsigned bit) { return (uint32_t)((uint32_t)0 - (uint32_t)(bit & 1u)); }
  FLAT_INLINE uint64_t flat_mask_from_bit_u64(unsigned bit) { return (uint64_t)((uint64_t)0 - (uint64_t)(bit & 1u)); }
  FLAT_INLINE size_t flat_mask_from_bit_sz(unsigned bit) { return (size_t)((size_t)0 - (size_t)(bit & 1u)); }

  FLAT_INLINE uint8_t flat_mask_is_zero_u8(uint8_t x) { return (uint8_t)((((uint8_t)(x | (uint8_t)(0u - x))) >> 7) - 1u); }
  FLAT_INLINE uint16_t flat_mask_is_zero_u16(uint16_t x) { return (uint16_t)((((uint16_t)(x | (uint16_t)(0u - x))) >> 15) - 1u); }
  FLAT_INLINE uint32_t flat_mask_is_zero_u32(uint32_t x) { return ((((x | (0u - x)) >> 31) - 1u)); }
  FLAT_INLINE uint64_t flat_mask_is_zero_u64(uint64_t x) { return ((((x | (0ull - x)) >> 63) - 1ull)); }
  FLAT_INLINE size_t flat_mask_is_zero_sz(size_t x)
  {
    return (((x | ((size_t)0 - x)) >> (sizeof(size_t) * 8 - 1)) - 1);
  }

  FLAT_INLINE uint8_t flat_mask_eq_u8(uint8_t a, uint8_t b) { return flat_mask_is_zero_u8((uint8_t)(a ^ b)); }
  FLAT_INLINE uint16_t flat_mask_eq_u16(uint16_t a, uint16_t b) { return flat_mask_is_zero_u16((uint16_t)(a ^ b)); }
  FLAT_INLINE uint32_t flat_mask_eq_u32(uint32_t a, uint32_t b) { return flat_mask_is_zero_u32((uint32_t)(a ^ b)); }
  FLAT_INLINE uint64_t flat_mask_eq_u64(uint64_t a, uint64_t b) { return flat_mask_is_zero_u64((uint64_t)(a ^ b)); }
  FLAT_INLINE size_t flat_mask_eq_sz(size_t a, size_t b) { return flat_mask_is_zero_sz((size_t)(a ^ b)); }

  /* a < b ? 0xFF..FF : 0 — robust unsigned */
  FLAT_INLINE uint8_t flat_mask_lt_u8(uint8_t a, uint8_t b)
  {
    uint8_t t = (uint8_t)((a ^ ((uint8_t)(a ^ b) | (uint8_t)((a - b) ^ b))) >> 7);
    return (uint8_t)(0u - (t & 1u));
  }
  FLAT_INLINE uint16_t flat_mask_lt_u16(uint16_t a, uint16_t b)
  {
    uint16_t t = (uint16_t)((a ^ ((uint16_t)(a ^ b) | (uint16_t)((a - b) ^ b))) >> 15);
    return (uint16_t)(0u - (t & 1u));
  }
  FLAT_INLINE uint32_t flat_mask_lt_u32(uint32_t a, uint32_t b)
  {
    uint32_t t = (uint32_t)((a ^ ((a ^ b) | ((a - b) ^ b))) >> 31);
    return (uint32_t)(0u - (t & 1u));
  }
  FLAT_INLINE uint64_t flat_mask_lt_u64(uint64_t a, uint64_t b)
  {
    uint64_t t = (uint64_t)((a ^ ((a ^ b) | ((a - b) ^ b))) >> 63);
    return (uint64_t)(0u - (t & 1u));
  }
  FLAT_INLINE size_t flat_mask_lt_sz(size_t a, size_t b)
  {
    const unsigned W = (unsigned)(sizeof(size_t) * 8 - 1);
    size_t t = (size_t)((a ^ ((a ^ b) | ((a - b) ^ b))) >> W);
    return (size_t)((size_t)0 - (t & (size_t)1));
  }
  FLAT_INLINE unsigned flat_mask_to_bit_u32(uint32_t m) { return (unsigned)(m & 1u); }

  /*------------------------------------------------------------------------------
    Selectors (cond ? yes : no), branchless
  ------------------------------------------------------------------------------*/
  FLAT_INLINE uint8_t flat_select_u8_mask(uint8_t yes, uint8_t no, uint8_t mask) { return (uint8_t)((yes & mask) | (no & (uint8_t)~mask)); }
  FLAT_INLINE uint16_t flat_select_u16_mask(uint16_t yes, uint16_t no, uint16_t mask) { return (uint16_t)((yes & mask) | (no & (uint16_t)~mask)); }
  FLAT_INLINE uint32_t flat_select_u32_mask(uint32_t yes, uint32_t no, uint32_t mask) { return (yes & mask) | (no & ~mask); }
  FLAT_INLINE uint64_t flat_select_u64_mask(uint64_t yes, uint64_t no, uint64_t mask) { return (yes & mask) | (no & ~mask); }
  FLAT_INLINE size_t flat_select_sz_mask(size_t yes, size_t no, size_t mask) { return (yes & mask) | (no & ~mask); }

  FLAT_INLINE uint8_t flat_select_u8(unsigned cond, uint8_t yes, uint8_t no) { return flat_select_u8_mask(yes, no, flat_mask_from_bit_u8(cond)); }
  FLAT_INLINE uint16_t flat_select_u16(unsigned cond, uint16_t yes, uint16_t no) { return flat_select_u16_mask(yes, no, flat_mask_from_bit_u16(cond)); }
  FLAT_INLINE uint32_t flat_select_u32(unsigned cond, uint32_t yes, uint32_t no) { return flat_select_u32_mask(yes, no, flat_mask_from_bit_u32(cond)); }
  FLAT_INLINE uint64_t flat_select_u64(unsigned cond, uint64_t yes, uint64_t no) { return flat_select_u64_mask(yes, no, flat_mask_from_bit_u64(cond)); }
  FLAT_INLINE size_t flat_select_sz(unsigned cond, size_t yes, size_t no) { return flat_select_sz_mask(yes, no, flat_mask_from_bit_sz(cond)); }

  /*------------------------------------------------------------------------------
    Constant-time memory (byte)
  ------------------------------------------------------------------------------*/
  FLAT_INLINE void flat_memxor(uint8_t *FLAT_RESTRICT dst, const uint8_t *FLAT_RESTRICT src, size_t len)
  {
    for (size_t i = 0; i < len; i++)
      dst[i] ^= src[i];
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memxor_when(unsigned cond, uint8_t *FLAT_RESTRICT dst, const uint8_t *FLAT_RESTRICT src, size_t len)
  {
    uint8_t m = flat_mask_from_bit_u8(cond);
    for (size_t i = 0; i < len; i++)
      dst[i] ^= (uint8_t)(src[i] & m);
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memcpy_when(unsigned cond, uint8_t *FLAT_RESTRICT dst, const uint8_t *FLAT_RESTRICT src, size_t len)
  {
    uint8_t m = flat_mask_from_bit_u8(cond);
    for (size_t i = 0; i < len; i++)
    {
      uint8_t v = (uint8_t)((src[i] & m) | (dst[i] & (uint8_t)~m));
      dst[i] = v;
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memswap_when(unsigned cond, uint8_t *FLAT_RESTRICT a, uint8_t *FLAT_RESTRICT b, size_t len)
  {
    uint8_t m = flat_mask_from_bit_u8(cond);
    for (size_t i = 0; i < len; i++)
    {
      uint8_t t = (uint8_t)((a[i] ^ b[i]) & m);
      a[i] ^= t;
      b[i] ^= t;
    }
    FLAT_COMPILER_BARRIER();
  }

  FLAT_INLINE int flat_mem_eq(const void *pa, const void *pb, size_t len)
  {
    const uint8_t *a = (const uint8_t *)pa, *b = (const uint8_t *)pb;
    uint32_t diff = 0u;
    for (size_t i = 0; i < len; i++)
      diff |= (uint32_t)(a[i] ^ b[i]);
    return (int)(flat_mask_is_zero_u32(diff) & 1u);
  }

  FLAT_INLINE int flat_mem_cmp(const void *pa, const void *pb, size_t len)
  {
    const uint8_t *a = (const uint8_t *)pa, *b = (const uint8_t *)pb;
    uint32_t seen = 0u, lt = 0u, gt = 0u;
    for (size_t i = 0; i < len; i++)
    {
      uint32_t ai = a[i], bi = b[i];
      uint32_t m_lt = flat_mask_lt_u32(ai, bi);
      uint32_t m_gt = flat_mask_lt_u32(bi, ai);
      uint32_t m_ne = (uint32_t)~flat_mask_eq_u32(ai, bi);
      uint32_t take = (uint32_t)~seen;
      lt |= (take & m_lt);
      gt |= (take & m_gt);
      seen |= (take & m_ne);
    }
    return (int)flat_mask_to_bit_u32(gt) - (int)flat_mask_to_bit_u32(lt);
  }

  /*------------------------------------------------------------------------------
    Lookup / Store (u8/u16/u32/u64) — CT scan with public bounds
  ------------------------------------------------------------------------------*/
  FLAT_INLINE uint8_t flat_lookup_u8(const uint8_t *arr, size_t len, size_t idx)
  {
    uint8_t out = 0u;
    for (size_t i = 0; i < len; i++)
    {
      uint8_t m = (uint8_t)flat_mask_eq_sz(i, idx);
      out = (uint8_t)((arr[i] & m) | (out & (uint8_t)~m));
    }
    return out;
  }
  FLAT_INLINE uint16_t flat_lookup_u16(const uint16_t *arr, size_t len, size_t idx)
  {
    uint16_t out = 0u;
    for (size_t i = 0; i < len; i++)
    {
      uint16_t m = (uint16_t)flat_mask_eq_sz(i, idx);
      out = (uint16_t)((arr[i] & m) | (out & (uint16_t)~m));
    }
    return out;
  }
  FLAT_INLINE uint32_t flat_lookup_u32(const uint32_t *arr, size_t len, size_t idx)
  {
    uint32_t out = 0u;
    for (size_t i = 0; i < len; i++)
    {
      uint32_t m = flat_mask_eq_sz(i, idx);
      out = ((arr[i] & m) | (out & ~m));
    }
    return out;
  }
  FLAT_INLINE uint64_t flat_lookup_u64(const uint64_t *arr, size_t len, size_t idx)
  {
    uint64_t out = 0u;
    for (size_t i = 0; i < len; i++)
    {
      uint64_t m = flat_mask_eq_sz(i, idx);
      out = ((arr[i] & m) | (out & ~m));
    }
    return out;
  }

  FLAT_INLINE void flat_store_at_u8(uint8_t *arr, size_t len, size_t idx, uint8_t value)
  {
    for (size_t i = 0; i < len; i++)
    {
      uint8_t m = (uint8_t)flat_mask_eq_sz(i, idx);
      arr[i] = (uint8_t)((value & m) | (arr[i] & (uint8_t)~m));
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_store_at_u16(uint16_t *arr, size_t len, size_t idx, uint16_t value)
  {
    for (size_t i = 0; i < len; i++)
    {
      uint16_t m = (uint16_t)flat_mask_eq_sz(i, idx);
      arr[i] = (uint16_t)((value & m) | (arr[i] & (uint16_t)~m));
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_store_at_u32(uint32_t *arr, size_t len, size_t idx, uint32_t value)
  {
    for (size_t i = 0; i < len; i++)
    {
      uint32_t m = flat_mask_eq_sz(i, idx);
      arr[i] = ((value & m) | (arr[i] & ~m));
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_store_at_u64(uint64_t *arr, size_t len, size_t idx, uint64_t value)
  {
    for (size_t i = 0; i < len; i++)
    {
      uint64_t m = flat_mask_eq_sz(i, idx);
      arr[i] = ((value & m) | (arr[i] & ~m));
    }
    FLAT_COMPILER_BARRIER();
  }

  /*------------------------------------------------------------------------------
    Zero-padding scan (index of last non-zero + 1) — CT
  ------------------------------------------------------------------------------*/
  FLAT_INLINE size_t flat_zeropad_data_len(const uint8_t *buf, size_t len)
  {
    size_t data_len = 0, seen = 0;
    for (size_t i = len; i > 0; i--)
    {
      uint8_t x = buf[i - 1];
      uint8_t nz_mask = (uint8_t)~flat_mask_is_zero_u8(x);
      size_t nz_bit = (size_t)(nz_mask & 1u);
      size_t trigger = nz_bit & (size_t)~seen;
      data_len = flat_select_sz(trigger, i, data_len);
      seen |= nz_bit;
    }
    return data_len;
  }

  /*------------------------------------------------------------------------------
    Endianness helpers
  ------------------------------------------------------------------------------*/
  FLAT_INLINE uint16_t flat_load_be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]); }
  FLAT_INLINE uint16_t flat_load_le16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[1] << 8) | (uint16_t)p[0]); }
  FLAT_INLINE void flat_store_be16(uint8_t *p, uint16_t v)
  {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
  }
  FLAT_INLINE void flat_store_le16(uint8_t *p, uint16_t v)
  {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
  }

  FLAT_INLINE uint32_t flat_load_be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3]; }
  FLAT_INLINE uint32_t flat_load_le32(const uint8_t *p) { return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[0]; }
  FLAT_INLINE void flat_store_be32(uint8_t *p, uint32_t v)
  {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
  }
  FLAT_INLINE void flat_store_le32(uint8_t *p, uint32_t v)
  {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
  }

  FLAT_INLINE uint64_t flat_load_be64(const uint8_t *p)
  {
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
           ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] << 8) | (uint64_t)p[7];
  }
  FLAT_INLINE uint64_t flat_load_le64(const uint8_t *p)
  {
    return ((uint64_t)p[7] << 56) | ((uint64_t)p[6] << 48) | ((uint64_t)p[5] << 40) | ((uint64_t)p[4] << 32) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[2] << 16) | ((uint64_t)p[1] << 8) | (uint64_t)p[0];
  }
  FLAT_INLINE void flat_store_be64(uint8_t *p, uint64_t v)
  {
    p[0] = (uint8_t)(v >> 56);
    p[1] = (uint8_t)(v >> 48);
    p[2] = (uint8_t)(v >> 40);
    p[3] = (uint8_t)(v >> 32);
    p[4] = (uint8_t)(v >> 24);
    p[5] = (uint8_t)(v >> 16);
    p[6] = (uint8_t)(v >> 8);
    p[7] = (uint8_t)v;
  }
  FLAT_INLINE void flat_store_le64(uint8_t *p, uint64_t v)
  {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
    p[4] = (uint8_t)(v >> 32);
    p[5] = (uint8_t)(v >> 40);
    p[6] = (uint8_t)(v >> 48);
    p[7] = (uint8_t)(v >> 56);
  }

  /*------------------------------------------------------------------------------
    Bitwise rotations & byte-swap (portable, CT)
  ------------------------------------------------------------------------------*/
  FLAT_INLINE uint32_t flat_rol32(uint32_t x, unsigned c)
  {
    unsigned n = c & 31u;
    return (x << n) | (x >> ((32u - n) & 31u));
  }
  FLAT_INLINE uint32_t flat_ror32(uint32_t x, unsigned c)
  {
    unsigned n = c & 31u;
    return (x >> n) | (x << ((32u - n) & 31u));
  }
  FLAT_INLINE uint64_t flat_rol64(uint64_t x, unsigned c)
  {
    unsigned n = c & 63u;
    return (x << n) | (x >> ((64u - n) & 63u));
  }
  FLAT_INLINE uint64_t flat_ror64(uint64_t x, unsigned c)
  {
    unsigned n = c & 63u;
    return (x >> n) | (x << ((64u - n) & 63u));
  }

  FLAT_INLINE uint32_t flat_bswap32(uint32_t x)
  {
#if defined(__has_builtin)
#if __has_builtin(__builtin_bswap32)
    return __builtin_bswap32(x);
#endif
#endif
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) << 8) |
           ((x & 0x00FF0000u) >> 8) |
           ((x & 0xFF000000u) >> 24);
  }
  FLAT_INLINE uint64_t flat_bswap64(uint64_t x)
  {
#if defined(__has_builtin)
#if __has_builtin(__builtin_bswap64)
    return __builtin_bswap64(x);
#endif
#endif
    return ((x & 0x00000000000000FFull) << 56) |
           ((x & 0x000000000000FF00ull) << 40) |
           ((x & 0x0000000000FF0000ull) << 24) |
           ((x & 0x00000000FF000000ull) << 8) |
           ((x & 0x000000FF00000000ull) >> 8) |
           ((x & 0x0000FF0000000000ull) >> 24) |
           ((x & 0x00FF000000000000ull) >> 40) |
           ((x & 0xFF00000000000000ull) >> 56);
  }

  /*------------------------------------------------------------------------------
    Masked arithmetic (incl. ADC/SBC + classic add_carry/sub_borrow API)
  ------------------------------------------------------------------------------*/
  FLAT_INLINE uint32_t flat_add_when_u32(unsigned cond, uint32_t x, uint32_t y)
  {
    uint32_t m = flat_mask_from_bit_u32(cond);
    return x + (y & m);
  }
  FLAT_INLINE uint64_t flat_add_when_u64(unsigned cond, uint64_t x, uint64_t y)
  {
    uint64_t m = flat_mask_from_bit_u64(cond);
    return x + (y & m);
  }

  FLAT_INLINE uint32_t flat_adc_u32(uint32_t x, uint32_t y, unsigned cin, unsigned *cout)
  {
    uint32_t s1 = x + y;
    uint32_t c1 = flat_mask_to_bit_u32(flat_mask_lt_u32(s1, x));
    uint32_t s2 = s1 + (cin & 1u);
    uint32_t c2 = flat_mask_to_bit_u32(flat_mask_lt_u32(s2, s1));
    *cout = (unsigned)(c1 | c2);
    return s2;
  }
  FLAT_INLINE uint64_t flat_adc_u64(uint64_t x, uint64_t y, unsigned cin, unsigned *cout)
  {
    uint64_t s1 = x + y;
    unsigned c1 = (unsigned)flat_mask_to_bit_u32((uint32_t)flat_mask_lt_u64(s1, x));
    uint64_t s2 = s1 + (uint64_t)(cin & 1u);
    unsigned c2 = (unsigned)flat_mask_to_bit_u32((uint32_t)flat_mask_lt_u64(s2, s1));
    *cout = (unsigned)(c1 | c2);
    return s2;
  }
  FLAT_INLINE uint32_t flat_sbc_u32(uint32_t x, uint32_t y, unsigned bin, unsigned *bout)
  {
    uint32_t t = x - y;
    unsigned b1 = (unsigned)flat_mask_to_bit_u32(flat_mask_lt_u32(x, y));
    uint32_t r = t - (bin & 1u);
    unsigned b2 = (unsigned)flat_mask_to_bit_u32(flat_mask_lt_u32(t, (bin & 1u)));
    *bout = (unsigned)(b1 | b2);
    return r;
  }
  FLAT_INLINE uint64_t flat_sbc_u64(uint64_t x, uint64_t y, unsigned bin, unsigned *bout)
  {
    uint64_t t = x - y;
    unsigned b1 = (unsigned)flat_mask_to_bit_u32((uint32_t)flat_mask_lt_u64(x, y));
    uint64_t r = t - (uint64_t)(bin & 1u);
    unsigned b2 = (unsigned)flat_mask_to_bit_u32((uint32_t)flat_mask_lt_u64(t, (uint64_t)(bin & 1u)));
    *bout = (unsigned)(b1 | b2);
    return r;
  }

  /* Conventional names (wrappers) */
  FLAT_INLINE uint32_t flat_add_carry_u32(uint32_t a, uint32_t b, unsigned cin, unsigned *cout) { return flat_adc_u32(a, b, cin, cout); }
  FLAT_INLINE uint64_t flat_add_carry_u64(uint64_t a, uint64_t b, unsigned cin, unsigned *cout) { return flat_adc_u64(a, b, cin, cout); }
  FLAT_INLINE uint32_t flat_sub_borrow_u32(uint32_t a, uint32_t b, unsigned bin, unsigned *bout) { return flat_sbc_u32(a, b, bin, bout); }
  FLAT_INLINE uint64_t flat_sub_borrow_u64(uint64_t a, uint64_t b, unsigned bin, unsigned *bout) { return flat_sbc_u64(a, b, bin, bout); }

  /* Masked ADC/SBC */
  FLAT_INLINE uint32_t flat_adc_when_u32(unsigned cond, uint32_t x, uint32_t y, unsigned cin, unsigned *cout)
  {
    uint32_t m = flat_mask_from_bit_u32(cond);
    uint32_t ym = y & m;
    unsigned cim = cin & (unsigned)(m & 1u);
    return flat_adc_u32(x, ym, cim, cout);
  }
  FLAT_INLINE uint64_t flat_adc_when_u64(unsigned cond, uint64_t x, uint64_t y, unsigned cin, unsigned *cout)
  {
    uint64_t m = flat_mask_from_bit_u64(cond);
    uint64_t ym = y & m;
    unsigned cim = cin & (unsigned)(m & 1u);
    return flat_adc_u64(x, ym, cim, cout);
  }
  FLAT_INLINE uint32_t flat_sbc_when_u32(unsigned cond, uint32_t x, uint32_t y, unsigned bin, unsigned *bout)
  {
    uint32_t m = flat_mask_from_bit_u32(cond);
    uint32_t ym = y & m;
    unsigned binm = bin & (unsigned)(m & 1u);
    return flat_sbc_u32(x, ym, binm, bout);
  }
  FLAT_INLINE uint64_t flat_sbc_when_u64(unsigned cond, uint64_t x, uint64_t y, unsigned bin, unsigned *bout)
  {
    uint64_t m = flat_mask_from_bit_u64(cond);
    uint64_t ym = y & m;
    unsigned binm = bin & (unsigned)(m & 1u);
    return flat_sbc_u64(x, ym, binm, bout);
  }

  /*------------------------------------------------------------------------------
    Min/Max/Clamp
  ------------------------------------------------------------------------------*/
  FLAT_INLINE uint32_t flat_min_u32(uint32_t a, uint32_t b)
  {
    uint32_t m = flat_mask_lt_u32(a, b);
    return b ^ ((a ^ b) & m);
  }
  FLAT_INLINE uint32_t flat_max_u32(uint32_t a, uint32_t b)
  {
    uint32_t m = flat_mask_lt_u32(a, b);
    return a ^ ((a ^ b) & m);
  }
  FLAT_INLINE uint64_t flat_min_u64(uint64_t a, uint64_t b)
  {
    uint64_t m = flat_mask_lt_u64(a, b);
    return b ^ ((a ^ b) & m);
  }
  FLAT_INLINE uint64_t flat_max_u64(uint64_t a, uint64_t b)
  {
    uint64_t m = flat_mask_lt_u64(a, b);
    return a ^ ((a ^ b) & m);
  }
  FLAT_INLINE size_t flat_min_sz(size_t a, size_t b)
  {
    size_t m = flat_mask_lt_sz(a, b);
    return b ^ ((a ^ b) & m);
  }
  FLAT_INLINE size_t flat_max_sz(size_t a, size_t b)
  {
    size_t m = flat_mask_lt_sz(a, b);
    return a ^ ((a ^ b) & m);
  }
  FLAT_INLINE uint32_t flat_clamp_u32(uint32_t x, uint32_t lo, uint32_t hi) { return flat_min_u32(flat_max_u32(x, lo), hi); }
  FLAT_INLINE uint64_t flat_clamp_u64(uint64_t x, uint64_t lo, uint64_t hi) { return flat_min_u64(flat_max_u64(x, lo), hi); }
  FLAT_INLINE size_t flat_clamp_sz(size_t x, size_t lo, size_t hi) { return flat_min_sz(flat_max_sz(x, lo), hi); }

  /*------------------------------------------------------------------------------
    S-box / Table transform (CT)
  ------------------------------------------------------------------------------*/
  FLAT_INLINE void flat_table_apply_u8(uint8_t *FLAT_RESTRICT out,
                                       const uint8_t *FLAT_RESTRICT in,
                                       size_t n,
                                       const uint8_t *table,
                                       size_t table_len)
  {
    for (size_t i = 0; i < n; i++)
    {
      size_t idx = (size_t)in[i]; /* secret; don't index directly */
      out[i] = flat_lookup_u8(table, table_len, idx);
    }
    FLAT_COMPILER_BARRIER();
  }

  /*------------------------------------------------------------------------------
    Typed memory (u16/u32/u64) & conditionals
  ------------------------------------------------------------------------------*/
  FLAT_INLINE void flat_memxor_u16(uint16_t *FLAT_RESTRICT dst, const uint16_t *FLAT_RESTRICT src, size_t words)
  {
    for (size_t i = 0; i < words; i++)
      dst[i] ^= src[i];
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memxor_u32(uint32_t *FLAT_RESTRICT dst, const uint32_t *FLAT_RESTRICT src, size_t words)
  {
    for (size_t i = 0; i < words; i++)
      dst[i] ^= src[i];
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memxor_u64(uint64_t *FLAT_RESTRICT dst, const uint64_t *FLAT_RESTRICT src, size_t words)
  {
    for (size_t i = 0; i < words; i++)
      dst[i] ^= src[i];
    FLAT_COMPILER_BARRIER();
  }

  FLAT_INLINE void flat_memxor_when_u16(unsigned cond, uint16_t *FLAT_RESTRICT dst, const uint16_t *FLAT_RESTRICT src, size_t words)
  {
    uint16_t m = flat_mask_from_bit_u16(cond);
    for (size_t i = 0; i < words; i++)
      dst[i] ^= (uint16_t)(src[i] & m);
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memxor_when_u32(unsigned cond, uint32_t *FLAT_RESTRICT dst, const uint32_t *FLAT_RESTRICT src, size_t words)
  {
    uint32_t m = flat_mask_from_bit_u32(cond);
    for (size_t i = 0; i < words; i++)
      dst[i] ^= (src[i] & m);
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memxor_when_u64(unsigned cond, uint64_t *FLAT_RESTRICT dst, const uint64_t *FLAT_RESTRICT src, size_t words)
  {
    uint64_t m = flat_mask_from_bit_u64(cond);
    for (size_t i = 0; i < words; i++)
      dst[i] ^= (src[i] & m);
    FLAT_COMPILER_BARRIER();
  }

  FLAT_INLINE void flat_memcpy_when_u16(unsigned cond, uint16_t *FLAT_RESTRICT dst, const uint16_t *FLAT_RESTRICT src, size_t words)
  {
    uint16_t m = flat_mask_from_bit_u16(cond);
    for (size_t i = 0; i < words; i++)
    {
      uint16_t v = (uint16_t)((src[i] & m) | (dst[i] & (uint16_t)~m));
      dst[i] = v;
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memcpy_when_u32(unsigned cond, uint32_t *FLAT_RESTRICT dst, const uint32_t *FLAT_RESTRICT src, size_t words)
  {
    uint32_t m = flat_mask_from_bit_u32(cond);
    for (size_t i = 0; i < words; i++)
    {
      uint32_t v = (src[i] & m) | (dst[i] & ~m);
      dst[i] = v;
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memcpy_when_u64(unsigned cond, uint64_t *FLAT_RESTRICT dst, const uint64_t *FLAT_RESTRICT src, size_t words)
  {
    uint64_t m = flat_mask_from_bit_u64(cond);
    for (size_t i = 0; i < words; i++)
    {
      uint64_t v = (src[i] & m) | (dst[i] & ~m);
      dst[i] = v;
    }
    FLAT_COMPILER_BARRIER();
  }

  FLAT_INLINE void flat_memswap_when_u16(unsigned cond, uint16_t *FLAT_RESTRICT a, uint16_t *FLAT_RESTRICT b, size_t words)
  {
    uint16_t m = flat_mask_from_bit_u16(cond);
    for (size_t i = 0; i < words; i++)
    {
      uint16_t t = (uint16_t)((a[i] ^ b[i]) & m);
      a[i] ^= t;
      b[i] ^= t;
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memswap_when_u32(unsigned cond, uint32_t *FLAT_RESTRICT a, uint32_t *FLAT_RESTRICT b, size_t words)
  {
    uint32_t m = flat_mask_from_bit_u32(cond);
    for (size_t i = 0; i < words; i++)
    {
      uint32_t t = (a[i] ^ b[i]) & m;
      a[i] ^= t;
      b[i] ^= t;
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memswap_when_u64(unsigned cond, uint64_t *FLAT_RESTRICT a, uint64_t *FLAT_RESTRICT b, size_t words)
  {
    uint64_t m = flat_mask_from_bit_u64(cond);
    for (size_t i = 0; i < words; i++)
    {
      uint64_t t = (a[i] ^ b[i]) & m;
      a[i] ^= t;
      b[i] ^= t;
    }
    FLAT_COMPILER_BARRIER();
  }

/*------------------------------------------------------------------------------
  Auto-dispatch SIMD (NEON/SSE2/AVX2) or portable fallback
------------------------------------------------------------------------------*/
#if defined(FLATLINE_ENABLE_AVX2) && defined(__AVX2__)
#include <immintrin.h>
  FLAT_INLINE void flat_memxor_auto(void *dst_, const void *src_, size_t len)
  {
    uint8_t *dst = (uint8_t *)dst_;
    const uint8_t *src = (const uint8_t *)src_;
    while (len >= 64)
    {
      __m256i a0 = _mm256_loadu_si256((const __m256i *)(dst + 0));
      __m256i b0 = _mm256_loadu_si256((const __m256i *)(src + 0));
      __m256i a1 = _mm256_loadu_si256((const __m256i *)(dst + 32));
      __m256i b1 = _mm256_loadu_si256((const __m256i *)(src + 32));
      _mm256_storeu_si256((__m256i *)(dst + 0), _mm256_xor_si256(a0, b0));
      _mm256_storeu_si256((__m256i *)(dst + 32), _mm256_xor_si256(a1, b1));
      dst += 64;
      src += 64;
      len -= 64;
    }
    while (len >= 32)
    {
      __m256i a = _mm256_loadu_si256((const __m256i *)dst);
      __m256i b = _mm256_loadu_si256((const __m256i *)src);
      _mm256_storeu_si256((__m256i *)dst, _mm256_xor_si256(a, b));
      dst += 32;
      src += 32;
      len -= 32;
    }
    while (len)
    {
      *dst++ ^= *src++;
      len--;
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memcpy_when_auto(unsigned cond, void *dst_, const void *src_, size_t len)
  {
    uint8_t *dst = (uint8_t *)dst_;
    const uint8_t *src = (const uint8_t *)src_;
    const __m256i m = _mm256_set1_epi8((char)flat_mask_from_bit_u8(cond));
    while (len >= 64)
    {
      __m256i d0 = _mm256_loadu_si256((const __m256i *)(dst + 0));
      __m256i s0 = _mm256_loadu_si256((const __m256i *)(src + 0));
      __m256i d1 = _mm256_loadu_si256((const __m256i *)(dst + 32));
      __m256i s1 = _mm256_loadu_si256((const __m256i *)(src + 32));
      _mm256_storeu_si256((__m256i *)(dst + 0), _mm256_or_si256(_mm256_and_si256(s0, m), _mm256_andnot_si256(m, d0)));
      _mm256_storeu_si256((__m256i *)(dst + 32), _mm256_or_si256(_mm256_and_si256(s1, m), _mm256_andnot_si256(m, d1)));
      dst += 64;
      src += 64;
      len -= 64;
    }
    while (len >= 32)
    {
      __m256i d = _mm256_loadu_si256((const __m256i *)dst);
      __m256i s = _mm256_loadu_si256((const __m256i *)src);
      _mm256_storeu_si256((__m256i *)dst, _mm256_or_si256(_mm256_and_si256(s, m), _mm256_andnot_si256(m, d)));
      dst += 32;
      src += 32;
      len -= 32;
    }
    while (len)
    {
      uint8_t d = *dst, s = *src, m8 = (uint8_t)flat_mask_from_bit_u8(cond);
      *dst = (uint8_t)((s & m8) | (d & (uint8_t)~m8));
      dst++;
      src++;
      len--;
    }
    FLAT_COMPILER_BARRIER();
  }
  FLAT_INLINE void flat_memswap_when_auto(unsigned cond, void *a_, void *b_, size_t len)
  {
    uint8_t *a = (uint8_t *)a_, *b = (uint8_t *)b_;
    const __m256i m = _mm256_set1_epi8((char)flat_mask_from_bit_u8(cond));
    while (len >= 64)
    {
      __m256i av0 = _mm256_loadu_si256((const __m256i *)(a + 0));
      __m256i bv0 = _mm256_loadu_si256((const __m256i *)(b + 0));
      __m256i av1 = _mm256_loadu_si256((const __m256i *)(a + 32));
      __m256i bv1 = _mm256_loadu_si256((const __m256i *)(b + 32));
      __m256i t0 = _mm256_and_si256(_mm256_xor_si256(av0, bv0), m);
      __m256i t1 = _mm256_and_si256(_mm256_xor_si256(av1, bv1), m);
      _mm256_storeu_si256((__m256i *)(a + 0), _mm256_xor_si256(av0, t0));
      _mm256_storeu_si256((__m256i *)(b + 0), _mm256_xor_si256(bv0, t0));
      _mm256_storeu_si256((__m256i *)(a + 32), _mm256_xor_si256(av1, t1));
      _mm256_storeu_si256((__m256i *)(b + 32), _mm256_xor_si256(bv1, t1));
      a += 64;
      b += 64;
      len -= 64;
    }
    while (len >= 32)
    {
      __m256i av = _mm256_loadu_si256((const __m256i *)a);
      __m256i bv = _mm256_loadu_si256((const __m256i *)b);
      __m256i t = _mm256_and_si256(_mm256_xor_si256(av, bv), m);
      _mm256_storeu_si256((__m256i *)a, _mm256_xor_si256(av, t));
      _mm256_storeu_si256((__m256i *)b, _mm256_xor_si256(bv, t));
      a += 32;
      b += 32;
      len -= 32;
    }
    while (len)
    {
      uint8_t m8 = (uint8_t)flat_mask_from_bit_u8(cond);
      uint8_t t = (uint8_t)((*a ^ *b) & m8);
      *a ^= t;
      *b ^= t;
      a++;
      b++;
      len--;
    }
    FLAT_COMPILER_BARRIER();
  }

#elif defined(FLATLINE_ENABLE_SSE2) && defined(__SSE2__)
#include <emmintrin.h>
FLAT_INLINE void flat_memxor_auto(void *dst_, const void *src_, size_t len)
{
  uint8_t *dst = (uint8_t *)dst_;
  const uint8_t *src = (const uint8_t *)src_;
  while (len >= 32)
  {
    __m128i a0 = _mm_loadu_si128((const __m128i *)(dst + 0));
    __m128i b0 = _mm_loadu_si128((const __m128i *)(src + 0));
    __m128i a1 = _mm_loadu_si128((const __m128i *)(dst + 16));
    __m128i b1 = _mm_loadu_si128((const __m128i *)(src + 16));
    _mm_storeu_si128((__m128i *)(dst + 0), _mm_xor_si128(a0, b0));
    _mm_storeu_si128((__m128i *)(dst + 16), _mm_xor_si128(a1, b1));
    dst += 32;
    src += 32;
    len -= 32;
  }
  while (len >= 16)
  {
    __m128i a = _mm_loadu_si128((const __m128i *)dst);
    __m128i b = _mm_loadu_si128((const __m128i *)src);
    _mm_storeu_si128((__m128i *)dst, _mm_xor_si128(a, b));
    dst += 16;
    src += 16;
    len -= 16;
  }
  while (len)
  {
    *dst++ ^= *src++;
    len--;
  }
  FLAT_COMPILER_BARRIER();
}
FLAT_INLINE void flat_memcpy_when_auto(unsigned cond, void *dst_, const void *src_, size_t len)
{
  uint8_t *dst = (uint8_t *)dst_;
  const uint8_t *src = (const uint8_t *)src_;
  const __m128i m = _mm_set1_epi8((char)flat_mask_from_bit_u8(cond));
  while (len >= 32)
  {
    __m128i d0 = _mm_loadu_si128((const __m128i *)(dst + 0));
    __m128i s0 = _mm_loadu_si128((const __m128i *)(src + 0));
    __m128i d1 = _mm_loadu_si128((const __m128i *)(dst + 16));
    __m128i s1 = _mm_loadu_si128((const __m128i *)(src + 16));
    _mm_storeu_si128((__m128i *)(dst + 0), _mm_or_si128(_mm_and_si128(s0, m), _mm_andnot_si128(m, d0)));
    _mm_storeu_si128((__m128i *)(dst + 16), _mm_or_si128(_mm_and_si128(s1, m), _mm_andnot_si128(m, d1)));
    dst += 32;
    src += 32;
    len -= 32;
  }
  while (len >= 16)
  {
    __m128i d = _mm_loadu_si128((const __m128i *)dst);
    __m128i s = _mm_loadu_si128((const __m128i *)src);
    _mm_storeu_si128((__m128i *)dst, _mm_or_si128(_mm_and_si128(s, m), _mm_andnot_si128(m, d)));
    dst += 16;
    src += 16;
    len -= 16;
  }
  while (len)
  {
    uint8_t d = *dst, s = *src, m8 = (uint8_t)flat_mask_from_bit_u8(cond);
    *dst = (uint8_t)((s & m8) | (d & (uint8_t)~m8));
    dst++;
    src++;
    len--;
  }
  FLAT_COMPILER_BARRIER();
}
FLAT_INLINE void flat_memswap_when_auto(unsigned cond, void *a_, void *b_, size_t len)
{
  uint8_t *a = (uint8_t *)a_, *b = (uint8_t *)b_;
  const __m128i m = _mm_set1_epi8((char)flat_mask_from_bit_u8(cond));
  while (len >= 32)
  {
    __m128i av0 = _mm_loadu_si128((const __m128i *)(a + 0));
    __m128i bv0 = _mm_loadu_si128((const __m128i *)(b + 0));
    __m128i av1 = _mm_loadu_si128((const __m128i *)(a + 16));
    __m128i bv1 = _mm_loadu_si128((const __m128i *)(b + 16));
    __m128i t0 = _mm_and_si128(_mm_xor_si128(av0, bv0), m);
    __m128i t1 = _mm_and_si128(_mm_xor_si128(av1, bv1), m);
    _mm_storeu_si128((__m128i *)(a + 0), _mm_xor_si128(av0, t0));
    _mm_storeu_si128((__m128i *)(b + 0), _mm_xor_si128(bv0, t0));
    _mm_storeu_si128((__m128i *)(a + 16), _mm_xor_si128(av1, t1));
    _mm_storeu_si128((__m128i *)(b + 16), _mm_xor_si128(bv1, t1));
    a += 32;
    b += 32;
    len -= 32;
  }
  while (len >= 16)
  {
    __m128i av = _mm_loadu_si128((const __m128i *)a);
    __m128i bv = _mm_loadu_si128((const __m128i *)b);
    __m128i t = _mm_and_si128(_mm_xor_si128(av, bv), m);
    _mm_storeu_si128((__m128i *)a, _mm_xor_si128(av, t));
    _mm_storeu_si128((__m128i *)b, _mm_xor_si128(bv, t));
    a += 16;
    b += 16;
    len -= 16;
  }
  while (len)
  {
    uint8_t m8 = (uint8_t)flat_mask_from_bit_u8(cond);
    uint8_t t = (uint8_t)((*a ^ *b) & m8);
    *a ^= t;
    *b ^= t;
    a++;
    b++;
    len--;
  }
  FLAT_COMPILER_BARRIER();
}

#elif defined(FLATLINE_ENABLE_NEON) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#include <arm_neon.h>
FLAT_INLINE void flat_memxor_auto(void *dst_, const void *src_, size_t len)
{
  uint8_t *dst = (uint8_t *)dst_;
  const uint8_t *src = (const uint8_t *)src_;
  while (len >= 64)
  {
    uint8x16_t a0 = vld1q_u8(dst + 0), b0 = vld1q_u8(src + 0);
    uint8x16_t a1 = vld1q_u8(dst + 16), b1 = vld1q_u8(src + 16);
    uint8x16_t a2 = vld1q_u8(dst + 32), b2 = vld1q_u8(src + 32);
    uint8x16_t a3 = vld1q_u8(dst + 48), b3 = vld1q_u8(src + 48);
    vst1q_u8(dst + 0, veorq_u8(a0, b0));
    vst1q_u8(dst + 16, veorq_u8(a1, b1));
    vst1q_u8(dst + 32, veorq_u8(a2, b2));
    vst1q_u8(dst + 48, veorq_u8(a3, b3));
    dst += 64;
    src += 64;
    len -= 64;
  }
  while (len >= 16)
  {
    uint8x16_t a = vld1q_u8(dst), b = vld1q_u8(src);
    vst1q_u8(dst, veorq_u8(a, b));
    dst += 16;
    src += 16;
    len -= 16;
  }
  while (len)
  {
    *dst++ ^= *src++;
    len--;
  }
  FLAT_COMPILER_BARRIER();
}
FLAT_INLINE void flat_memcpy_when_auto(unsigned cond, void *dst_, const void *src_, size_t len)
{
  uint8_t *dst = (uint8_t *)dst_;
  const uint8_t *src = (const uint8_t *)src_;
  const uint8_t m8 = (uint8_t)flat_mask_from_bit_u8(cond);
  const uint8x16_t m = vdupq_n_u8(m8);
  while (len >= 64)
  {
    uint8x16_t d0 = vld1q_u8(dst + 0), s0 = vld1q_u8(src + 0);
    uint8x16_t d1 = vld1q_u8(dst + 16), s1 = vld1q_u8(src + 16);
    uint8x16_t d2 = vld1q_u8(dst + 32), s2 = vld1q_u8(src + 32);
    uint8x16_t d3 = vld1q_u8(dst + 48), s3 = vld1q_u8(src + 48);
    vst1q_u8(dst + 0, vorrq_u8(vandq_u8(s0, m), vbicq_u8(d0, m)));
    vst1q_u8(dst + 16, vorrq_u8(vandq_u8(s1, m), vbicq_u8(d1, m)));
    vst1q_u8(dst + 32, vorrq_u8(vandq_u8(s2, m), vbicq_u8(d2, m)));
    vst1q_u8(dst + 48, vorrq_u8(vandq_u8(s3, m), vbicq_u8(d3, m)));
    dst += 64;
    src += 64;
    len -= 64;
  }
  while (len >= 16)
  {
    uint8x16_t d = vld1q_u8(dst), s = vld1q_u8(src);
    vst1q_u8(dst, vorrq_u8(vandq_u8(s, m), vbicq_u8(d, m)));
    dst += 16;
    src += 16;
    len -= 16;
  }
  while (len)
  {
    uint8_t d = *dst, s = *src;
    *dst = (uint8_t)((s & m8) | (d & (uint8_t)~m8));
    dst++;
    src++;
    len--;
  }
  FLAT_COMPILER_BARRIER();
}
FLAT_INLINE void flat_memswap_when_auto(unsigned cond, void *a_, void *b_, size_t len)
{
  uint8_t *a = (uint8_t *)a_, *b = (uint8_t *)b_;
  const uint8_t m8 = (uint8_t)flat_mask_from_bit_u8(cond);
  const uint8x16_t m = vdupq_n_u8(m8);
  while (len >= 64)
  {
    uint8x16_t a0 = vld1q_u8(a + 0), b0 = vld1q_u8(b + 0);
    uint8x16_t a1 = vld1q_u8(a + 16), b1 = vld1q_u8(b + 16);
    uint8x16_t a2 = vld1q_u8(a + 32), b2 = vld1q_u8(b + 32);
    uint8x16_t a3 = vld1q_u8(a + 48), b3 = vld1q_u8(b + 48);
    uint8x16_t t0 = vandq_u8(veorq_u8(a0, b0), m);
    uint8x16_t t1 = vandq_u8(veorq_u8(a1, b1), m);
    uint8x16_t t2 = vandq_u8(veorq_u8(a2, b2), m);
    uint8x16_t t3 = vandq_u8(veorq_u8(a3, b3), m);
    vst1q_u8(a + 0, veorq_u8(a0, t0));
    vst1q_u8(b + 0, veorq_u8(b0, t0));
    vst1q_u8(a + 16, veorq_u8(a1, t1));
    vst1q_u8(b + 16, veorq_u8(b1, t1));
    vst1q_u8(a + 32, veorq_u8(a2, t2));
    vst1q_u8(b + 32, veorq_u8(b2, t2));
    vst1q_u8(a + 48, veorq_u8(a3, t3));
    vst1q_u8(b + 48, veorq_u8(b3, t3));
    a += 64;
    b += 64;
    len -= 64;
  }
  while (len >= 16)
  {
    uint8x16_t av = vld1q_u8(a), bv = vld1q_u8(b);
    uint8x16_t t = vandq_u8(veorq_u8(av, bv), m);
    vst1q_u8(a, veorq_u8(av, t));
    vst1q_u8(b, veorq_u8(bv, t));
    a += 16;
    b += 16;
    len -= 16;
  }
  while (len)
  {
    uint8_t t = (uint8_t)((*a ^ *b) & m8);
    *a ^= t;
    *b ^= t;
    a++;
    b++;
    len--;
  }
  FLAT_COMPILER_BARRIER();
}

#else /* Portable fallback */
FLAT_INLINE void flat_memxor_auto(void *dst_, const void *src_, size_t len)
{
  uint8_t *dst = (uint8_t *)dst_;
  const uint8_t *src = (const uint8_t *)src_;
  for (size_t i = 0; i < len; i++)
    dst[i] ^= src[i];
  FLAT_COMPILER_BARRIER();
}
FLAT_INLINE void flat_memcpy_when_auto(unsigned cond, void *dst_, const void *src_, size_t len)
{
  uint8_t *dst = (uint8_t *)dst_;
  const uint8_t *src = (const uint8_t *)src_;
  const uint8_t m = (uint8_t)flat_mask_from_bit_u8(cond);
  for (size_t i = 0; i < len; i++)
  {
    uint8_t d = dst[i], s = src[i];
    dst[i] = (uint8_t)((s & m) | (d & (uint8_t)~m));
  }
  FLAT_COMPILER_BARRIER();
}
FLAT_INLINE void flat_memswap_when_auto(unsigned cond, void *a_, void *b_, size_t len)
{
  uint8_t *a = (uint8_t *)a_, *b = (uint8_t *)b_;
  const uint8_t m = (uint8_t)flat_mask_from_bit_u8(cond);
  for (size_t i = 0; i < len; i++)
  {
    uint8_t t = (uint8_t)((a[i] ^ b[i]) & m);
    a[i] ^= t;
    b[i] ^= t;
  }
  FLAT_COMPILER_BARRIER();
}
#endif

  /*------------------------------------------------------------------------------
    Boolean reductions
  ------------------------------------------------------------------------------*/
  FLAT_INLINE uint8_t flat_reduce_or_u8(const uint8_t *buf, size_t len)
  {
    uint32_t acc = 0u;
    for (size_t i = 0; i < len; i++)
      acc |= (uint32_t)buf[i];
    return (uint8_t)acc;
  }
  FLAT_INLINE uint8_t flat_reduce_and_u8(const uint8_t *buf, size_t len)
  {
    uint32_t acc = 0xFFu;
    for (size_t i = 0; i < len; i++)
      acc &= (uint32_t)buf[i];
    return (uint8_t)acc;
  }
  FLAT_INLINE unsigned flat_any_nonzero_u8(const uint8_t *buf, size_t len)
  {
    uint32_t acc = 0u;
    for (size_t i = 0; i < len; i++)
      acc |= (uint32_t)buf[i];
    return (unsigned)((~flat_mask_is_zero_u32(acc)) & 1u);
  }
  FLAT_INLINE unsigned flat_all_zero_u8(const uint8_t *buf, size_t len)
  {
    uint32_t acc = 0u;
    for (size_t i = 0; i < len; i++)
      acc |= (uint32_t)buf[i];
    return (unsigned)(flat_mask_is_zero_u32(acc) & 1u);
  }

  FLAT_INLINE uint32_t flat_reduce_or_u32w(const uint32_t *buf, size_t words)
  {
    uint32_t acc = 0u;
    for (size_t i = 0; i < words; i++)
      acc |= buf[i];
    return acc;
  }
  FLAT_INLINE uint32_t flat_reduce_and_u32w(const uint32_t *buf, size_t words)
  {
    uint32_t acc = ~0u;
    for (size_t i = 0; i < words; i++)
      acc &= buf[i];
    return acc;
  }
  FLAT_INLINE unsigned flat_any_nonzero_u32w(const uint32_t *buf, size_t words)
  {
    uint32_t acc = 0u;
    for (size_t i = 0; i < words; i++)
      acc |= buf[i];
    return (unsigned)((~flat_mask_is_zero_u32(acc)) & 1u);
  }
  FLAT_INLINE unsigned flat_all_zero_u32w(const uint32_t *buf, size_t words)
  {
    uint32_t acc = 0u;
    for (size_t i = 0; i < words; i++)
      acc |= buf[i];
    return (unsigned)(flat_mask_is_zero_u32(acc) & 1u);
  }

  FLAT_INLINE uint64_t flat_reduce_or_u64w(const uint64_t *buf, size_t words)
  {
    uint64_t acc = 0u;
    for (size_t i = 0; i < words; i++)
      acc |= buf[i];
    return acc;
  }
  FLAT_INLINE uint64_t flat_reduce_and_u64w(const uint64_t *buf, size_t words)
  {
    uint64_t acc = ~0ull;
    for (size_t i = 0; i < words; i++)
      acc &= buf[i];
    return acc;
  }
  FLAT_INLINE unsigned flat_any_nonzero_u64w(const uint64_t *buf, size_t words)
  {
    uint64_t acc = 0u;
    for (size_t i = 0; i < words; i++)
      acc |= buf[i];
    uint32_t comb = (uint32_t)(acc >> 32) | (uint32_t)acc;
    return (unsigned)((~flat_mask_is_zero_u32(comb)) & 1u);
  }
  FLAT_INLINE unsigned flat_all_zero_u64w(const uint64_t *buf, size_t words)
  {
    uint64_t acc = 0u;
    for (size_t i = 0; i < words; i++)
      acc |= buf[i];
    uint32_t comb = (uint32_t)(acc >> 32) | (uint32_t)acc;
    return (unsigned)(flat_mask_is_zero_u32(comb) & 1u);
  }

  /*==============================================================================
    EXTRA HELPERS against side channels
  ==============================================================================*/

  /* Secure wipe / conditional wipe */
  FLAT_INLINE void flat_explicit_bzero(void *p, size_t n)
  {
#if defined(__STDC_LIB_EXT1__)
    (void)memset_s(p, n, 0, n);
#else
  volatile uint8_t *vp = (volatile uint8_t *)p;
  for (size_t i = 0; i < n; i++)
    vp[i] = 0;
  FLAT_COMPILER_BARRIER();
#endif
  }
  FLAT_INLINE void flat_memwipe_when(unsigned cond, void *p, size_t n)
  {
    uint8_t *b = (uint8_t *)p;
    uint8_t m = flat_mask_from_bit_u8(cond);
    for (size_t i = 0; i < n; i++)
      b[i] &= (uint8_t)~m; /* if cond==1 => zero */
    FLAT_COMPILER_BARRIER();
  }

  /* Speculative-execution hardening + masked index clamp */
  FLAT_INLINE void flat_spec_fence(void)
  {
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("lfence" ::: "memory");
#elif defined(__aarch64__)
  __asm__ __volatile__("dsb sy\nisb" ::: "memory");
#else
  FLAT_COMPILER_BARRIER();
#endif
  }
  FLAT_INLINE size_t flat_index_clamp(size_t idx, size_t len)
  {
    size_t m = flat_mask_lt_sz(idx, len); /* 0x..FF if idx < len, else 0 */
    return idx & m;                       /* becomes 0 if out-of-bounds */
  }
  FLAT_INLINE uint8_t flat_masked_load_u8(const uint8_t *base, size_t len, size_t idx)
  {
    size_t i = flat_index_clamp(idx, len);
    flat_spec_fence();
    return base[i];
  }

  /* Branchless CMOVs (pointers, words) */
  FLAT_INLINE void *flat_ptr_select(unsigned cond, void *yes, void *no)
  {
    uintptr_t ym = (uintptr_t)yes, nm = (uintptr_t)no;
    uintptr_t m = (uintptr_t)flat_mask_from_bit_sz(cond);
    return (void *)((ym & m) | (nm & ~m));
  }
  FLAT_INLINE void flat_cswap32(unsigned cond, uint32_t *a, uint32_t *b)
  {
    uint32_t m = flat_mask_from_bit_u32(cond);
    uint32_t t = (*a ^ *b) & m;
    *a ^= t;
    *b ^= t;
  }
  FLAT_INLINE void flat_cswap64(unsigned cond, uint64_t *a, uint64_t *b)
  {
    uint64_t m = flat_mask_from_bit_u64(cond);
    uint64_t t = (*a ^ *b) & m;
    *a ^= t;
    *b ^= t;
  }

  /* CT error accumulator & commit */
  typedef struct
  {
    uint32_t acc;
  } flat_erracc_t;
  FLAT_INLINE void flat_erracc_init(flat_erracc_t *e) { e->acc = 0; }
  FLAT_INLINE void flat_erracc_or(flat_erracc_t *e, unsigned cond) { e->acc |= (cond & 1u); }
  FLAT_INLINE unsigned flat_erracc_ok(const flat_erracc_t *e) { return (unsigned)(flat_mask_is_zero_u32(e->acc) & 1u); }
  FLAT_INLINE void flat_commit_if_ok(unsigned ok, void *dst, const void *tmp, size_t n)
  {
    flat_memcpy_when(ok, (uint8_t *)dst, (const uint8_t *)tmp, n);
  }

  /* Constant-time PKCS#7 unpad */
  FLAT_INLINE unsigned flat_pkcs7_unpad_ct(const uint8_t *buf, size_t len,
                                           size_t block, size_t *data_len_out)
  {
    if (len == 0 || block == 0)
    {
      *data_len_out = 0;
      return 0;
    }
    uint8_t pad = buf[len - 1];
    unsigned ok1 = (pad >= 1u) & (pad <= (uint8_t)block) & (pad <= (uint8_t)len);

    uint32_t diff = 0u;
    size_t maxchk = flat_min_sz(len, block);
    for (size_t i = 0; i < maxchk; i++)
    {
      uint8_t in_window = (uint8_t)(flat_mask_lt_sz(i, (size_t)pad) & 0xFFu);
      uint8_t b = buf[len - 1 - i];
      diff |= (uint32_t)((b ^ pad) & in_window);
    }
    unsigned ok2 = (unsigned)(flat_mask_is_zero_u32(diff) & 1u);
    unsigned ok = ok1 & ok2;
    size_t data_len = len - (size_t)pad;
    *data_len_out = flat_select_sz(ok, data_len, (size_t)0);
    return ok;
  }

  /* Oblivious block select (scan-based) */
  FLAT_INLINE void flat_select_block(void *FLAT_RESTRICT out,
                                     const void *FLAT_RESTRICT blocks,
                                     size_t count, size_t stride,
                                     size_t secret_idx)
  {
    uint8_t *o = (uint8_t *)out;
    const uint8_t *base = (const uint8_t *)blocks;
    for (size_t k = 0; k < stride; k++)
      o[k] = 0;
    for (size_t i = 0; i < count; i++)
    {
      size_t msz = flat_mask_eq_sz(i, secret_idx);
      const uint8_t *b = base + i * stride;
      for (size_t k = 0; k < stride; k++)
      {
        uint8_t m = (uint8_t)msz;
        o[k] = (uint8_t)((b[k] & m) | (o[k] & (uint8_t)~m));
      }
    }
    FLAT_COMPILER_BARRIER();
  }

  /* Small constant-time sorting primitives */
  FLAT_INLINE void flat_sort2_u32(uint32_t *a, uint32_t *b)
  {
    uint32_t m = flat_mask_lt_u32(*b, *a);
    uint32_t t = (*a ^ *b) & m;
    *a ^= t;
    *b ^= t;
  }
  FLAT_INLINE void flat_sort4_u32(uint32_t v[4])
  {
    flat_sort2_u32(&v[0], &v[1]);
    flat_sort2_u32(&v[2], &v[3]);
    flat_sort2_u32(&v[0], &v[2]);
    flat_sort2_u32(&v[1], &v[3]);
    flat_sort2_u32(&v[1], &v[2]);
  }

  /* Mask-returning equality (composable) */
  FLAT_INLINE uint32_t flat_memeq_mask(const void *a, const void *b, size_t n)
  {
    const uint8_t *x = (const uint8_t *)a, *y = (const uint8_t *)b;
    uint32_t diff = 0;
    for (size_t i = 0; i < n; i++)
      diff |= (uint32_t)(x[i] ^ y[i]);
    return flat_mask_is_zero_u32(diff); /* 0xFFFF.. or 0 */
  }

  /* Constant-time division (bit-serial long division, fixed 32/64 iters) */
  FLAT_INLINE unsigned flat_div_mod_ct_u64(uint64_t n, uint64_t d, uint64_t *q, uint64_t *r)
  {
    uint64_t qv = 0, rv = 0;
    for (int i = 63; i >= 0; i--)
    {
      rv = (rv << 1) | ((n >> (unsigned)i) & 1ull);
      /* ge = (rv >= d) ? 0xFF.. : 0x00.. */
      uint64_t ge = (uint64_t)~flat_mask_lt_u64(rv, d);
      rv = rv - (d & ge);
      qv |= ((1ull << (unsigned)i) & ge);
    }
    /* ok=1 if d!=0 */
    unsigned ok = (unsigned)(~flat_mask_is_zero_u64(d) & 1ull);
    *q = flat_select_u64(ok, qv, 0ull);
    *r = flat_select_u64(ok, rv, 0ull);
    return ok;
  }
  FLAT_INLINE unsigned flat_div_mod_ct_u32(uint32_t n, uint32_t d, uint32_t *q, uint32_t *r)
  {
    uint32_t qv = 0, rv = 0;
    for (int i = 31; i >= 0; i--)
    {
      rv = (rv << 1) | ((n >> (unsigned)i) & 1u);
      uint32_t ge = (uint32_t)~flat_mask_lt_u32(rv, d);
      rv = rv - (d & ge);
      qv |= ((1u << (unsigned)i) & ge);
    }
    unsigned ok = (unsigned)(~flat_mask_is_zero_u32(d) & 1u);
    *q = flat_select_u32(ok, qv, 0u);
    *r = flat_select_u32(ok, rv, 0u);
    return ok;
  }

  /* Conditional zero / move (word-sized) */
  FLAT_INLINE void flat_zero_when_u32(unsigned cond, uint32_t *x)
  {
    uint32_t m = flat_mask_from_bit_u32(cond);
    *x &= ~m;
  }
  FLAT_INLINE void flat_zero_when_u64(unsigned cond, uint64_t *x)
  {
    uint64_t m = flat_mask_from_bit_u64(cond);
    *x &= ~m;
  }
  FLAT_INLINE void flat_zero_when_sz(unsigned cond, size_t *x)
  {
    size_t m = flat_mask_from_bit_sz(cond);
    *x &= ~m;
  }

  FLAT_INLINE void flat_mov_when_u32(unsigned cond, uint32_t *dst, uint32_t src)
  {
    uint32_t m = flat_mask_from_bit_u32(cond);
    *dst = (*dst & ~m) | (src & m);
  }
  FLAT_INLINE void flat_mov_when_u64(unsigned cond, uint64_t *dst, uint64_t src)
  {
    uint64_t m = flat_mask_from_bit_u64(cond);
    *dst = (*dst & ~m) | (src & m);
  }
  FLAT_INLINE void flat_mov_when_sz(unsigned cond, size_t *dst, size_t src)
  {
    size_t m = flat_mask_from_bit_sz(cond);
    *dst = (*dst & ~m) | (src & m);
  }
#ifdef __cplusplus
}
#endif
#endif /* FLATLINE_H */
