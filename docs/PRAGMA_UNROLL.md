# Why Not Just `#pragma unroll`?

Compilers *can* unroll loops with `break`/`continue`/`return` using `#pragma unroll`. I tested it. The difference is in how the unrolled code is structured.

---

## I Tested It

[Godbolt](https://godbolt.org), GCC 15.2 and Clang 19-20, x86-64 and ARM64.

| Compiler | Target | `#pragma unroll 4` | `-funroll-loops` |
|----------|--------|-------------------|------------------|
| Clang 20 | x86-64 | Unrolls 4x | - |
| Clang 19 | ARM64  | Unrolls 4x | - |
| GCC 15.2 | x86-64 | Unrolls 4x | Unrolls 8x |
| GCC 15.2 | ARM64  | Unrolls + auto-vectorizes | - |

So yes, compilers will unroll your early-exit loops if you ask nicely.

---

## What You Get

Given:
```cpp
#pragma unroll 4
for (size_t i = 0; i < n; ++i) {
    if (data[i] == target) return i;
}
```

GCC 15.2 x86-64 produces:
```asm
.L3:
  cmp DWORD PTR [rdi+rdx*4], esi    ; load+compare 0
  je .L4                             ; branch
  lea rcx, [rdx+1]
  cmp DWORD PTR [rdi+rcx*4], esi    ; load+compare 1
  je .L4                             ; branch
  cmp DWORD PTR [rdi+rdx*4+8], esi  ; load+compare 2
  je .L4
  cmp DWORD PTR [rdi+8+rcx*4], esi  ; load+compare 3
  je .L4
  lea rdx, [rcx+3]
  jmp .L3
```

Each compare is immediately followed by its branch. The CPU must predict each branch before the next compare can retire.

---

## What ILP_FOR Produces

The macro generates code that evaluates conditions first:
```cpp
// These can all execute simultaneously
bool found0 = data[i+0] == target;
bool found1 = data[i+1] == target;
bool found2 = data[i+2] == target;
bool found3 = data[i+3] == target;

// Then check sequentially
if (found0) return i+0;
if (found1) return i+1;
if (found2) return i+2;
if (found3) return i+3;
```

---

## The Nuance

Modern compilers are smart. In our tests, GCC sometimes produced identical assembly for both patterns - it saw through the "parallel evaluation" and merged loads with compares anyway.

So why bother with ilp_for?

**1. Guaranteed unrolling**

`#pragma unroll` is a hint. Compilers may ignore it. They may unroll sequentially. ilp_for generates the unrolled code directly.

**2. Multi-accumulator reductions**

This is where ilp_for really shines. For min/max:

```cpp
// #pragma unroll keeps one accumulator (serial dependency):
min_val = std::min(min_val, data[i]);      // depends on min_val
min_val = std::min(min_val, data[i+1]);    // waits for previous
min_val = std::min(min_val, data[i+2]);    // waits for previous
min_val = std::min(min_val, data[i+3]);    // waits for previous
```

ilp_for uses multiple independent accumulators:
```cpp
min0 = std::min(min0, data[i+0]);  // independent
min1 = std::min(min1, data[i+1]);  // independent
min2 = std::min(min2, data[i+2]);  // independent
min3 = std::min(min3, data[i+3]);  // independent
// combine at end
```

This breaks the dependency chain. **5.8x speedup** on min/max (see [PERFORMANCE.md](PERFORMANCE.md)).

**3. Consistent API**

Same interface across GCC, Clang, MSVC, x86-64, ARM64, whatever.

---

## When Does It Matter?

| Pattern | `#pragma unroll` | ilp_for | Winner |
|---------|------------------|---------|--------|
| Simple sum | Fine | Fine | Either |
| Early exit (`break`/`return`) | Unrolls, sequential | Structured for ILP | ilp_for (marginal) |
| Min/max reduction | Keeps dependency chain | Breaks chain | **ilp_for (5.8x)** |

---

## References

- [GCC Loop-Specific Pragmas](https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html)
- [LLVM Code Transformation Metadata](https://llvm.org/docs/TransformMetadata.html)
- [Intel Unroll Pragma Guide](https://www.intel.com/content/www/us/en/docs/oneapi-fpga-add-on/optimization-guide/2023-1/unroll-pragma.html)
- [Loop Unrolling - Wikipedia](https://en.wikipedia.org/wiki/Loop_unrolling)
