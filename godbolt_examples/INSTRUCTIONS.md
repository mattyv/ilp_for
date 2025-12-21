# Godbolt Examples - Maintenance Instructions

## Purpose

These examples are designed to be **self-contained, copy-paste ready** for [Godbolt Compiler Explorer](https://godbolt.org/). Each file must compile standalone without any external includes from this repository.

---

## CRITICAL: LINE-FOR-LINE EXACT COPIES

**The implementation code in godbolt examples MUST be LINE-FOR-LINE EXACT COPIES from the main library source files.**

This means:
- Copy-paste the actual text from the source files
- **Same whitespace, same comments, same formatting**
- **Character-for-character identical**
- Only omit entire functions/sections that are genuinely unused

DO NOT:
- Reformat code
- Rewrite comments
- Change variable names
- Simplify logic
- "Clean up" anything
- Add your own comments

### How to Update (Using Bash Tools)

Use `sed` to extract exact line ranges from source files:

```bash
# Extract ForCtrl from ctrl.hpp
sed -n '/^struct ForCtrl/,/^};/p' ilp_for/detail/ctrl.hpp

# Extract for_loop from loops_ilp.hpp
sed -n '/^template.*for_loop/,/^}/p' ilp_for/detail/loops_ilp.hpp
```

**NEVER manually rewrite or "clean up" the extracted code.**

### Why Line-for-Line?

1. **Identical codegen**: Godbolt output must match real library
2. **Easy diffing**: `diff` the godbolt file against source to verify
3. **No bugs**: Rewriting introduces bugs; copy-paste doesn't
4. **Maintainability**: When source changes, copy-paste the new version

---

## Structure

Each godbolt example contains:
1. **Extracted implementation** - LINE-FOR-LINE copy from library
2. **ILP version** - Example usage of library API
3. **Hand-rolled version** - Manual unrolling for comparison
4. **Simple version** - Baseline naive implementation
5. **Test main()** - Verifies all versions produce same results

---

## Test main() Volatile Pattern

Use `volatile` on inputs to prevent constant propagation, but NOT on loop counters:

```cpp
int main() {
    volatile size_t n = 1000;        // Volatile: unknown at compile time
    volatile int threshold = 50;     // Volatile: unknown at compile time

    std::vector<int> data(n);
    for (size_t i = 0; i < n; ++i) { // NOT volatile: avoids C++20 deprecation warning
        data[i] = i;
    }
    // ...
}
```

**Why this pattern:**
- `volatile` on `n`, `threshold`, etc. prevents compiler from constant-folding
- Loop counter doesn't need volatile: loop bound depends on volatile `n`, writes have side effects
- `++i` on volatile is deprecated in C++20, causes `-Wdeprecated-volatile` warning

---

## Source File Locations

Copy LINE-FOR-LINE from these files:

| Component | Source File | Lines |
|-----------|-------------|-------|
| `ForCtrl` | `ilp_for/detail/ctrl.hpp` | check file |
| `ForResult` | `ilp_for/detail/ctrl.hpp` | check file |
| `for_loop` | `ilp_for/detail/loops_ilp.hpp` | check file |
| Macros | `ilp_for.hpp` | check file |

---

## Which Files Need Which Components

| Example | Components Needed |
|---------|-------------------|
| `loop_with_break.cpp` | `ForCtrl`, `ForResult`, `for_loop`, `ILP_FOR` macros |
| `loop_with_return.cpp` | `ForCtrl`, `ForResult`, `for_loop`, `ILP_FOR` + `ILP_RETURN` macros |
| `loop_with_return_typed.cpp` | `ForCtrlTyped`, `ForResultTyped`, `for_loop_typed`, `ILP_FOR_T` macros |
| `pragma_vs_ilp.cpp` | Full `ILP_FOR` implementation for pragma comparison |

---

## Verification

Test compilation:
```bash
for f in godbolt_examples/*.cpp; do
    echo "Testing $f"
    clang++ -std=c++20 -O3 -Wall "$f" -o /tmp/test && /tmp/test && echo "PASS"
done
```

---

## Current API Reference

### `ILP_FOR(loop_var, start, end, N) { ... } ILP_END;`
- Basic loop, supports `ILP_BREAK`, `ILP_CONTINUE`

### `ILP_FOR_AUTO(loop_var, start, end, LoopType) { ... } ILP_END;`
- Auto-selects optimal N based on LoopType (Sum, DotProduct, Search, etc.)

### `ILP_FOR_T(type, loop_var, start, end, N) { ... } ILP_END_RETURN;`
- Typed loop with `ILP_RETURN(value)` support

### `ILP_FOR_RANGE(loop_var, range, N) { ... } ILP_END;`
- Range-based variant
