# Why Not Just `#pragma unroll`?

Compilers *can* unroll loops with `break`/`continue`/`return` using `#pragma unroll`. The difference is not in whether it unrolls, but in **how many instructions it generates per iteration**.

---

## The Real Issue: Per-Iteration Bounds Checks

When you add `#pragma unroll` to a loop with early exit, the compiler must preserve exact break semantics. Since it cannot determine the trip count (SCEV fails for loops with `break`), it inserts bounds checks after **each unrolled element**.

### Assembly Evidence (x86-64 Clang)

**Pragma Unroll:**
```asm
.LBB0_2:                                ; main loop
    cmp  dword ptr [rdi + 4*rax], edx   ; load & compare element 0
    ja   .LBB0_18                       ; exit if > threshold
    cmp  r9, rax                        ; <-- bounds check (i < n-1?)
    je   .LBB0_12                       ; <-- exit if at end
    cmp  dword ptr [rdi + 4*rax + 4], edx ; element 1
    ja   .LBB0_13
    cmp  r8, rax                        ; <-- bounds check (again!)
    je   .LBB0_12
    cmp  dword ptr [rdi + 4*rax + 8], edx ; element 2
    ja   .LBB0_15
    cmp  rcx, rax                       ; <-- bounds check (again!)
    je   .LBB0_12
    cmp  dword ptr [rdi + 4*rax + 12], edx ; element 3
    ja   .LBB0_17
    add  rax, 4
    cmp  rsi, rax
    jne  .LBB0_2
```

**ILP_FOR:**
```asm
.LBB1_9:                                ; main loop
    cmp  dword ptr [rdi + 4*rax - 8], edx  ; element 0
    ja   .LBB1_17
    cmp  dword ptr [rdi + 4*rax - 4], edx  ; element 1
    ja   .LBB1_15
    cmp  dword ptr [rdi + 4*rax], edx      ; element 2
    ja   .LBB1_18
    cmp  dword ptr [rdi + 4*rax + 4], edx  ; element 3
    ja   .LBB1_16
    lea  rcx, [rax + 4]
    add  rax, 6
    cmp  rax, rsi                       ; <-- bounds check only HERE
    mov  rax, rcx
    jbe  .LBB1_9
```

### Assembly Evidence (ARM64 Clang)

**Pragma Unroll:**
```asm
LBB2_2:                                 ; main loop
    ldur w12, [x8, #-8]    ; load 0
    cmp  w12, w2
    b.hi LBB2_12           ; exit if > threshold
    cmp  x11, x0           ; <-- bounds check (i < n?)
    b.eq LBB2_10           ; <-- exit if at end
    ldur w12, [x8, #-4]    ; load 1
    cmp  w12, w2
    b.hi LBB2_13
    cmp  x10, x0           ; <-- another bounds check
    b.eq LBB2_10
    ... (repeated for each element)
```

**ILP_FOR:**
```asm
LBB0_8:                                 ; main loop
    ldur w9, [x10, #-8]    ; load 0
    cmp  w9, w2
    b.hi LBB0_6            ; exit if > threshold
    ldur w9, [x10, #-4]    ; load 1
    cmp  w9, w2
    b.hi LBB0_14
    ldr  w9, [x10]         ; load 2
    cmp  w9, w2
    b.hi LBB0_15
    ldr  w9, [x10, #4]     ; load 3
    cmp  w9, w2
    b.hi LBB0_16
    ; bounds check only HERE, after all 4 elements
    cmp  x11, x1
    b.ls LBB0_8
```

**Result:** Pragma has ~6 instructions per element, ILP_FOR has ~4. This accounts for the ~1.29x speedup.

[View on Godbolt](https://godbolt.org/z/Mh4aTP5j7)

---

## Why Can't the Compiler Optimize This?

The compiler uses **Scalar Evolution (SCEV)** to analyze loop trip counts. For a loop like:

```cpp
for (size_t i = 0; i < n; ++i) {
    if (data[i] > threshold) break;  // Early exit
    ++count;
}
```

SCEV cannot determine how many iterations will execute - it depends on runtime data. The compiler must conservatively assume the loop could exit at any element.

**Without break:** SCEV knows exactly `n` iterations will run. The compiler can:
- Unroll cleanly into a main loop + tail loop
- Vectorize with SIMD (both Clang and GCC do this)
- Bounds check only at the end of each unrolled block

**With break:** The trip count is unknown. The compiler must:
- Check bounds after each element to preserve exact semantics
- Cannot speculatively execute past a potential exit point
- Falls back to "interleaved" unrolling instead of "strip-mining"

---

## Verification: Loops Without Break

For loops **without** early exit, all approaches produce identical SIMD code:

```cpp
// All three compile to the same SIMD (ld4.4s, cmhs.4s, etc.)
size_t count_simple(const uint32_t* data, size_t n, uint32_t threshold);
size_t count_pragma(const uint32_t* data, size_t n, uint32_t threshold);
size_t count_ilp(const uint32_t* data, size_t n, uint32_t threshold);
```

**Conclusion:** ILP_FOR only helps for loops with early exit. For simple loops without `break`/`return`, just use a regular loop - the compiler optimizes it perfectly.

---

## Portability

Pragma syntax varies by compiler:

| Compiler | Syntax | Notes |
|----------|--------|-------|
| Clang | `#pragma clang loop unroll_count(N)` | Reliable |
| GCC | `#pragma GCC unroll N` | Reliable |
| MSVC | None | No equivalent pragma |
| Intel | `#pragma unroll(N)` | ICC/ICX only |

For portable code:
```cpp
#if defined(__clang__)
    #pragma clang loop unroll_count(4)
#elif defined(__GNUC__)
    #pragma GCC unroll 4
#endif
for (size_t i = 0; i < n; ++i) { ... }
```

Or just use `ILP_FOR` which works everywhere.

---

## When Does It Matter?

| Pattern | `#pragma unroll` | ILP_FOR | Winner |
|---------|------------------|---------|--------|
| Simple sum (no break) | SIMD | SIMD | Either |
| Early exit (`break`/`return`) | Bounds check per element | Bounds check per block | **ILP_FOR (~1.29x)** |
| Min/max reduction | Keeps dependency chain | Breaks chain | **ILP_FOR (5.8x)** |
| Loops without early exit | SIMD | SIMD | Either |

---

## Multi-Accumulator Reductions

For reductions like min/max, the benefit is different. `#pragma unroll` keeps one accumulator:

```cpp
// Pragma unroll - serial dependency chain:
min_val = std::min(min_val, data[i]);      // depends on min_val
min_val = std::min(min_val, data[i+1]);    // waits for previous
min_val = std::min(min_val, data[i+2]);    // waits for previous
min_val = std::min(min_val, data[i+3]);    // waits for previous
```

`ilp::reduce` uses multiple independent accumulators:
```cpp
// Multiple accumulators - parallel execution:
min0 = std::min(min0, data[i+0]);  // independent
min1 = std::min(min1, data[i+1]);  // independent
min2 = std::min(min2, data[i+2]);  // independent
min3 = std::min(min3, data[i+3]);  // independent
// combine at end: min(min0, min1, min2, min3)
```

This breaks the dependency chain. **5.8x speedup** on min/max (see [PERFORMANCE.md](PERFORMANCE.md)).

---

## Summary

| Issue | Pragma Unroll | ILP_FOR |
|-------|---------------|---------|
| Bounds checks | Per element | Per block |
| Trip count needed? | No (but costly) | No |
| Dependency chains | Preserved | Broken |
| Portability | Compiler-specific | Universal |
| SIMD for simple loops | Yes | Yes |

**Use ILP_FOR for:**
- Loops with `break`, `continue`, or `return`
- Reductions where you want parallel accumulators

**Skip ILP_FOR for:**
- Simple loops without early exit (compilers handle these well)

---

## References

- [GCC Loop-Specific Pragmas](https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html)
- [LLVM Loop Metadata](https://llvm.org/docs/TransformMetadata.html)
- [Godbolt Example: Pragma vs ILP](https://godbolt.org/z/Mh4aTP5j7)
