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

| Component | Source File |
|-----------|-------------|
| `arch::sbo_size`, `arch::max_integral_size` | `ilp_for/detail/arch.hpp` |
| `SmallStorage`, `TypedStorage`, `EachCtrl`, `ForCtrl`, `ForCtrlTyped`, `ForResult`, `ForResultTyped` (+ their `Proxy`s), `propagate_return`, the debug-typecheck machinery (`TypeTag`/`type_mismatch_abort`/`swallowed_proxy_abort`, gated on `ILP_TYPECHECK_ENABLED`) | `ilp_for/detail/ctrl.hpp` |
| `Mode`, `default_mode` | `ilp_for/detail/mode.hpp` |
| `validate_unroll_factor`/`warn_large_unroll_factor`, the `ForEachBody`/`ForUntypedCtrlBody`/`ForTypedCtrlBody` concepts | `ilp_for/detail/loops_common.hpp` |
| `unrolled_block_end`, `index_loop_core`, `for_each_impl`, `for_loop_untyped_impl`, `for_loop_typed_impl`, `macro_for` (both tag overloads) | `ilp_for/detail/loops_ilp.hpp` |
| Macros, `ilp_detail_ctrl()` sentinel | `ilp_for.hpp` |
| `find_if_default_block`-strategy constants, `find_if_resolved_block`, `validate_find_block`/`warn_gcc_find_block_cliff`, `find_if_impl`, `find_if` | `ilp_for/detail/find.hpp` |

None of the four `ILP_FOR`-based examples use `ILP_FOR_RANGE`/`ILP_FOR_AUTO`, so the range-based
(`range_loop_core`, `for_each_range_impl`, `for_loop_range_*_impl`,
`macro_for_range*`) and auto-N (`optimal_N`, `LoopType`, `cpu_profiles/`)
machinery is genuinely unused across all five files and stays out.

---

## Which Files Need Which Components

The dependency graph is less modular than it looks, because `propagate_return`
(used by every `ILP_END_RETURN`) branches on `if constexpr (std::is_same_v<C,
EachCtrl>)` / `ForCtrl` / `ctrl_typed_return<C>::is_typed` - so **every one of
those ctrl types must be a declared name**, even in a file that never
instantiates most of them. Concretely:

- **`ILP_END`-only files** (no `ILP_RETURN` anywhere): need only the lean
  break-only path - `EachCtrl`, `NoResult`, `end_tag_t`,
  `validate_unroll_factor`/`warn_large_unroll_factor`, `unrolled_block_end`,
  `index_loop_core`, `for_each_impl`, the `macro_for(..., end_tag_t)` overload,
  `Mode`/`default_mode`. No `SmallStorage`/`ForResult`/`ForCtrl`/debug-typecheck
  machinery, no `ilp_detail_ctrl()` sentinel (nothing calls `propagate_return`).
  `loop_with_break.cpp` and `pragma_vs_ilp.cpp` are this shape.
- **`ILP_END_RETURN` files** (untyped `ILP_RETURN`, or `ILP_FOR_T`/typed
  `ILP_RETURN`): need the FULL zoo regardless of which path the demo actually
  exercises - `arch::sbo_size`, `SmallStorage`, `TypedStorage`, `EachCtrl`,
  `ForCtrl`, `ForCtrlTyped<R>`, `ForResult`+`Proxy`, `ForResultTyped<R>`+`Proxy`,
  the debug-typecheck machinery, `ctrl_typed_return`, `propagate_return`, both
  tags, `for_loop_untyped_impl` AND `for_loop_typed_impl` (macro_for's generic
  body names both unconditionally - see the comment at its definition), the
  `macro_for(..., end_return_tag_t)` overload, and the `ilp_detail_ctrl()`
  sentinel. `loop_with_return.cpp` and `loop_with_return_typed.cpp` are this
  shape and are near-identical except for their demo functions/macros - don't
  try to trim one below the other's dependencies without re-verifying the
  interdependency above.
- **`find_if.cpp`** is a different shape entirely: it uses the function API
  only (no `ILP_FOR`/`ILP_END` macro at all), so none of the ctrl-type zoo
  above is reachable - `find_if_impl` doesn't take a `Ctrl&` and never calls
  `propagate_return`. It needs only `Mode`/`default_mode` (from `mode.hpp`)
  and `ILP_ALWAYS_INLINE` (the macro definition from `ctrl.hpp`, not the rest
  of that file) plus everything in `find.hpp` itself. This is the leanest of
  the five examples.

| Example | Path |
|---------|------|
| `loop_with_break.cpp` | `ILP_END`-only (lean) |
| `pragma_vs_ilp.cpp` | `ILP_END`-only (lean) |
| `loop_with_return.cpp` | `ILP_END_RETURN` (full zoo) |
| `loop_with_return_typed.cpp` | `ILP_END_RETURN` (full zoo) |
| `find_if.cpp` | function-API-only (leanest - no ctrl-type zoo) |

---

## Verification

Test compilation on both compilers, both with and without `-DNDEBUG` (the
debug-typecheck machinery in `loop_with_return*.cpp` only compiles under one
of the two, so both must be checked):

```bash
for f in godbolt_examples/*.cpp; do
    for cxx in clang++ g++; do
        for flags in "" "-DNDEBUG"; do
            echo "Testing $f ($cxx $flags)"
            $cxx -std=c++20 -O3 -Wall -Wextra $flags "$f" -o /tmp/test && /tmp/test && echo "PASS"
        done
    done
done
```

Then spot-check extraction fidelity by diffing individual structs/functions
against the real source (adjust the `sed` range per component):

```bash
diff <(sed -n '/struct SmallStorage {/,/^    };$/p' ilp_for/detail/ctrl.hpp) \
     <(sed -n '/struct SmallStorage {/,/^    };$/p' godbolt_examples/loop_with_return.cpp)
```

For `find_if.cpp`, the whole `namespace ilp { namespace detail { ... } ... }` body
of `find.hpp` (Mode-definition line aside, folded in from `mode.hpp` since both
headers reopen `namespace ilp`) is copied verbatim, so a single range diff
covers it:

```bash
diff <(sed -n '/^namespace ilp {$/,/^} \/\/ namespace ilp$/p' ilp_for/detail/find.hpp | sed -n '3,$p') \
     <(sed -n '/^    namespace detail {$/,/^} \/\/ namespace ilp$/p' godbolt_examples/find_if.cpp)
```

---

## Current API Reference

### `ILP_FOR(loop_var, start, end, N) { ... } ILP_END;`
- Basic loop, supports `ILP_BREAK`, `ILP_CONTINUE`

### `ILP_FOR_AUTO(loop_var, start, end, LoopType, element_type) { ... } ILP_END;`
- Auto-selects optimal N based on LoopType and element type (Sum, DotProduct, Search, etc.)

### `ILP_FOR_T(type, loop_var, start, end, N) { ... } ILP_END_RETURN;`
- Typed loop with `ILP_RETURN(value)` support

### `ILP_FOR_RANGE(loop_var, range, N) { ... } ILP_END;`
- Range-based variant
