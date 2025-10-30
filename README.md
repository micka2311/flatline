# Flatline — Constant‑Time C Primitives (single‑header)

> **If time varies, secrets leak. Flatline it.**  

`flatline.h` is a tiny, dependency‑free, allocation‑free, **constant‑time** toolbox for C (C99/C11).
It helps you write code that **does not branch on secrets**, **does not index memory with secrets**, and **avoids variable‑time ops** — while staying portable and fast (optional SIMD).

---

## Why Flatline?

Side‑channel bugs often hide in:
- early‑exit compares (`memcmp`)
- secret‑dependent branches (`if (secret) ...`)
- secret‑indexed loads (`sbox[secret]`)
- variable‑time instructions (`/`, `%`, FP)

Flatline provides **branchless** selectors, **masked** memory ops, **table sweeps** for secret indices, and **constant‑time arithmetic** to eliminate those leaks — without pulling in a bignum or crypto framework.

---

## Highlights

- **Single header**: drop `flatline.h` into your project.
- **Zero deps / zero heap**: no libc tricks, no malloc.
- **Constant‑time by default**: helpers that avoid branches on secrets and secret‑indexed memory.
- **SIMD “auto” fast paths** (optional): NEON / SSE2 / AVX2 for throughput — still constant‑time with respect to `cond`.
- **Tested**: unit tests, micro‑benchmarks, and a DUDECT‑like timing harness included.

---

## Install

1) Copy `flatline.h` to your project.  
2) Include it where needed:

```c
#include "flatline.h"
```

3) (Optional) Enable SIMD fast paths:
- **x86 AVX2**: `-DFLATLINE_ENABLE_AVX2 -mavx2`
- **x86 SSE2**: `-DFLATLINE_ENABLE_SSE2 -msse2`
- **ARM NEON** (Apple Silicon, aarch64): `-DFLATLINE_ENABLE_NEON`

Or rely on auto‑detect in the header (can be disabled with `-DFLATLINE_DISABLE_SIMD`).

> For strict builds you can add `-fno-builtin` to keep the compiler from replacing functions with potentially non‑CT intrinsics.

---

## Build & Run (repo layout)

```bash
# unit tests (quick, deterministic)
make unit            # builds and runs ./flatline_unit

# micro‑benchmarks (throughput only)
make bench           # builds and runs ./flatline_bench

# DUDECT‑like timing checks (finds leaks)
make dudect          # builds and runs ./flatline_dudect

# clean
make clean
```

Examples for stronger DUDECT signals:

```bash
make dudect CFLAGS_EXTRA="-DDU_SAMPLES=40000 -DDU_REPS=16 -DDU_BUFSZ=4096 -DDU_T_THRESHOLD=10.0 -DDU_THRASH_BYTES=16777216 -DDU_THRASH_STRIDE=64"
```

Enable SIMD for benches on your CPU:

```bash
# Apple Silicon (NEON)
make bench NEON=1 CFLAGS_EXTRA="-O3 -march=native -mtune=native"

# x86 AVX2
make bench AVX2=1 CFLAGS_EXTRA="-O3 -march=native -mtune=native"
```

---

## Quick start (safe patterns)

### 1) Safe constant‑time compare

```c
/* returns -1, 0, +1 without early exit */
int sgn = flat_mem_cmp(a, b, len);

/* equality (1/0) in constant time */
int eq = flat_mem_eq(a, b, len);
```

### 2) Secret‑conditioned copy (no branching)

```c
/* cond may be secret; copies src->dst if cond==1, else keeps dst */
flat_memcpy_when(cond, dst, src, len);

/* idem for XOR or swap: */
flat_memxor_when(cond, dst, src, len);
flat_memswap_when(cond, a, b, len);
```

> In this build, the **auto** variants remain constant‑time with respect to `cond` (no branching):  
> `flat_memcpy_when_auto(cond, dst, src, len)` / `flat_memswap_when_auto(...)` / `flat_memxor_auto(...)`.

### 3) Secret‑indexed lookup (no cache leak)

```c
/* Instead of x = table[secret];  // leaky
   do a full sweep and mask: */
uint8_t x = flat_lookup_u8(table, 256, secret);

/* Apply S‑box to a buffer with secret indices: */
flat_table_apply_u8(out, in, n, table, 256);
```

### 4) Scan with no early exit (zero‑padding length)

```c
size_t data_len = flat_zeropad_data_len(buf, len);
```

### 5) Select without branches

```c
/* x = cond ? yes : no;      // branchy, leaky if cond is secret */
uint32_t x = flat_select_u32(cond, yes, no);
```

### 6) Constant‑time division (no `/, %`)

```c
uint64_t q, r;
unsigned ok = flat_div_mod_ct_u64(numerator, denominator, &q, &r);
/* ok==0 if denominator==0 (q=r=0) */
```

---

## API Overview (not exhaustive)

### Masks & Predicates
- `flat_mask_from_bit_u{8,16,32,64,sz}(bit)` → full mask
- `flat_mask_is_zero_u{8,16,32,64,sz}(x)`
- `flat_mask_eq_u{8,16,32,64,sz}(a,b)`
- `flat_mask_lt_u{8,16,32,64,sz}(a,b)`
- `flat_mask_to_bit_u32(m)`

### Selectors
- `flat_select_u{8,16,32,64,sz}(cond, yes, no)`
- `flat_select_u*_mask(yes, no, mask)`

### Memory (byte & typed)
- `flat_memxor(dst, src, len)`
- `flat_memxor_when(cond, dst, src, len)`
- `flat_memcpy_when(cond, dst, src, len)`
- `flat_memswap_when(cond, a, b, len)`
- Typed: `flat_memxor_u{16,32,64}`, `flat_memcpy_when_u{16,32,64}`, `flat_memswap_when_u{16,32,64}`
- **Auto** fast paths: `flat_memxor_auto`, `flat_memcpy_when_auto`, `flat_memswap_when_auto` (still constant‑time w.r.t `cond`).

### Compare
- `flat_mem_eq(a,b,len)` → 1/0
- `flat_mem_cmp(a,b,len)` → -1/0/+1

### Secret‑Index Table Ops
- `flat_lookup_u{8,16,32,64}(arr, len, idx)`
- `flat_store_at_u{8,16,32,64}(arr, len, idx, val)`
- `flat_table_apply_u8(out, in, n, table, table_len)`
- `flat_masked_load_u8(buf, len, idx)`
- `flat_index_clamp(idx, len)`

### Zero‑Padding
- `flat_zeropad_data_len(buf, len)`

### Endianness / Bit Ops
- `flat_load_{be,le}{16,32,64}(p)`
- `flat_store_{be,le}{16,32,64}(p, v)`
- `flat_rol{32,64}(x, r)`, `flat_ror{32,64}(x, r)`
- `flat_bswap{32,64}(x)`

### Arithmetic (constant‑time)
- Masked add/sub: `flat_add_when_u{32,64}`, `flat_adc_u{32,64}`, `flat_sbc_u{32,64}`
- Optional bignum‑style: `flat_add_carry_u64(a,b,cin,&cout)`, `flat_sub_borrow_u64(a,b,bin,&bout)` (if enabled)
- `flat_min_u{32,64,sz}`, `flat_max_u{32,64,sz}`, `flat_clamp_u{32,64,sz}`

### Constant‑Time Division
- `flat_div_mod_ct_u{32,64}(n, d, &q, &r)`

### Convenience / Misc
- `flat_zero_when_u{32,64}(cond, &x)`
- `flat_mov_when_u{32,64}(cond, &dst, src)`
- `flat_select_block(out, blocks, nblocks, block_size, idx)`
- `flat_erracc_init`, `flat_erracc_or`, `flat_erracc_ok`, `flat_commit_if_ok(cond, dst, tmp, len)`
- `flat_memeq_mask(a,b,len)` → all‑equal mask
- `flat_cswap32(cond, &a, &b)` / `flat_sort4_u32(v)`

> Exact surface may evolve; check the header for authoritative signatures.

---

## Constant‑Time Discipline (what Flatline guarantees / expects)

- **No branches on secrets.** Prefer `flat_select_*` over `if (secret)`.
- **No secret‑indexed memory.** Use `flat_lookup_*` / `flat_table_apply_u8`.
- **No variable‑time ops.** Avoid `/, %`, and FP; use `flat_div_mod_ct_*` and integer primitives.
- **Loops over public bounds only.** All loops take the same trip count independent of secret data.
- ***_auto** functions:** in this build, they do **not** branch on `cond` and stay constant‑time with respect to it.

Flatline reduces timing variability but cannot address **all** side‑channels (e.g., power/EM, system interrupts, OS page faults). Evaluate your **threat model**.

---

## Performance Notes

- For **large buffers**, prefer `*_auto` (NEON/SSE2/AVX2).
- Keep buffers **aligned** when possible (the bench allocates 64‑byte aligned blocks).
- For **tiny sizes** (<64B), scalar paths are often faster (SIMD setup overhead).
- Consider `-O3 -march=native -mtune=native` for bench builds.

---

## Testing

- **Unit tests:** `make unit` (covers all primitives, scalar vs auto equivalence in a small, deterministic set; no timing).
- **Benches:** `make bench` (reports MB/s for scalar vs auto).
- **DUDECT‑like timing:** `make dudect` (flags **LEAK** on known leaky references; Flatline CT counterparts should show **OK**).

---

## C++ / Toolchains

- Header is C99/C11; safe to include from C++.
- Tested with Clang/AppleClang, GCC, MSVC (use `/O2 /std:c11`‑equivalent and enable intrinsics if you want SIMD).
- Optional compiler barrier is provided to keep the compiler honest around memory ops.

---

## Security & Responsible Disclosure

If you spot a side‑channel or constant‑time violation, please file a **private issue** or contact the maintainer directly. We’ll triage quickly and publish a fix and a note.

---

## License
MIT, Copyright (c) 2025 Stateless Limited — see the header for SPDX identifier.

---

## FAQ

**Q: Do I need the auto SIMD paths?**  
A: No. They’re optional. In this build, they remain constant‑time with respect to `cond` (no branching).

**Q: Is constant‑time the same as constant‑everything?**  
A: No. Flatline addresses timing variability and cache‑based leakage patterns. It doesn’t eliminate power/EM channels or OS noise.

**Q: Can I replace all `/` and `%` with `flat_div_mod_ct_*`?**  
A: Use them where operands (especially the divisor) can be secret. If operands are public, normal division is fine.

**Q: Can I use `memcmp` safely anywhere?**  
A: Only if the compared bytes are **public**. Otherwise use `flat_mem_cmp` / `flat_mem_eq`.

---

## Example: constant‑time S‑box application

```c
/* Unsafe (leaky): out[i] = sbox[in[i]]; */

/* Safe with Flatline: */
void sbox_apply_ct(uint8_t *out, const uint8_t *in, size_t n,
                   const uint8_t *sbox, size_t sbox_len) {
  flat_table_apply_u8(out, in, n, sbox, sbox_len);
}
```

## Example: constant‑time “copy if equal”

```c
/* If tag matches, copy secret to dst — without timing on the compare or the copy. */
int secure_copy_if_tag_matches(uint8_t *dst, const uint8_t *src, size_t len,
                               const uint8_t *tag_a, const uint8_t *tag_b, size_t tag_len) {
  int eq = flat_mem_eq(tag_a, tag_b, tag_len);     /* CT equality */
  flat_memcpy_when((unsigned)eq, dst, src, len);   /* CT conditional copy */
  return eq;
}
```

---

## Roadmap

- More bignum‑friendly add/sub carry/borrow helpers.
- Constant‑time modular reduction patterns.
- Optional compile‑time lint (warn on `/`, `%`, FP, and secret branches with macros).
- Integrate with LLVM by introducing a new `secret` keyword to tag sensitive memory and enable automated pattern recognition for replacement with constant-time APIs.

---

If **time varies**, secrets leak. **Flatline it.**
