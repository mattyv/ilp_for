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
# Extract is_optional trait from ctrl.hpp (lines 15-17)
sed -n '15,17p' detail/ctrl.hpp

# Extract validate_unroll_factor from loops_common.hpp (lines 51-63)
sed -n '51,63p' detail/loops_common.hpp

# Extract find_impl from loops_ilp.hpp (lines 122-182)
sed -n '122,182p' detail/loops_ilp.hpp

# Extract reduce_impl from loops_ilp.hpp (lines 425-488)
sed -n '425,488p' detail/loops_ilp.hpp
```

To build a complete godbolt example:

```bash
# Example: build find_first_match.cpp implementation section
{
    echo "namespace ilp {"
    echo "namespace detail {"
    echo ""
    sed -n '15,17p' detail/ctrl.hpp                    # is_optional
    echo ""
    sed -n '51,63p' detail/loops_common.hpp            # validate_unroll_factor
    echo ""
    sed -n '138p' detail/loops_common.hpp              # FindBody concept
    echo ""
    sed -n '122,182p' detail/loops_ilp.hpp             # find_impl
    echo ""
    echo "} // namespace detail"
    echo ""
    sed -n '676,680p' detail/loops_ilp.hpp             # public ilp::find
    echo ""
    echo "} // namespace ilp"
}
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
| `is_optional` trait | `detail/ctrl.hpp` | ~15-17 |
| `validate_unroll_factor` | `detail/loops_common.hpp` | ~51-63 |
| `ReduceBody` concept | `detail/loops_common.hpp` | ~131 |
| `FindBody` concept | `detail/loops_common.hpp` | ~138 |
| `has_known_identity_v` | `detail/loops_ilp.hpp` | ~26-36 |
| `make_identity` | `detail/loops_ilp.hpp` | ~39-59 |
| `make_accumulators` | `detail/loops_ilp.hpp` | ~74-90 |
| `find_impl` | `detail/loops_ilp.hpp` | ~122-182 |
| `reduce_impl` | `detail/loops_ilp.hpp` | ~425-488 |
| Public `ilp::find` | `detail/loops_ilp.hpp` | ~676-680 |
| Public `ilp::reduce` | `detail/loops_ilp.hpp` | ~739-743 |

---

## Which Files Need Which Components

| Example | Components Needed |
|---------|-------------------|
| `find_first_match.cpp` | `is_optional`, `validate_unroll_factor`, `find_impl`, `ilp::find` |
| `sum_with_break.cpp` | `is_optional`, `validate_unroll_factor`, `has_known_identity_v`, `make_identity`, `make_accumulators`, `ReduceBody`, `reduce_impl`, `ilp::reduce` |
| `parallel_min.cpp` | Same as `sum_with_break.cpp` |
| `transform_simple.cpp` | `ForCtrl`, `ForResult`, `validate_unroll_factor`, `for_loop_untyped_impl`, `ilp::for_loop`, macros from `ilp_for.hpp` |

---

## Verification

After updating, verify with diff:

```bash
# Example: verify find_impl is exact copy
diff <(sed -n '/^template.*find_impl/,/^}$/p' detail/loops_ilp.hpp) \
     <(sed -n '/^template.*find_impl/,/^}$/p' godbolt_examples/find_first_match.cpp)
```

Test compilation:
```bash
for f in godbolt_examples/*.cpp; do
    echo "Testing $f"
    clang++ -std=C++20 -O3 -Wall "$f" -o /tmp/test && /tmp/test && echo "PASS"
done
```

---

## Current API Reference

### `ilp::find<N>(start, end, body)`
- `body(i, end)` returns `bool` or value with sentinel comparison
- Returns index of first match, or `end` if not found

### `ilp::reduce<N>(start, end, init, op, body)`
- `body(i)` returns value OR `std::optional<T>` (nullopt = break)
- Returns reduced result

### `ILP_FOR(loop_var, start, end, N) { ... } ILP_END`
- Void loops, supports `ILP_BREAK`, `ILP_CONTINUE`

### `ILP_FOR(...) { ... } ILP_END_RETURN`
- Loops with `ILP_RETURN(value)`
