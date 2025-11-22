# ILP_FOR Design Document

A C++23 template metaprogramming library for compile-time loop unrolling with full control flow support.

## Motivation

Manual loop unrolling for ILP (Instruction Level Parallelism) is error-prone and verbose:

```cpp
// Hand-rolled unrolled loop
unsigned foo2(unsigned x, unsigned total) {
    unsigned ret = 0;
    unsigned i = 0;
    for (; (i + 4) < x; i += 4) {
        ret += i;
        if (ret > total) break;
        ret += i + 1;
        if (ret > total) break;
        ret += i + 2;
        if (ret > total) break;
        ret += i + 3;
        if (ret > total) break;
    }
    for (; i < x; ++i) {
        ret += i;
        if (ret > total) break;
    }
    return ret;
}
```

This library provides a DSL that generates identical assembly:

```cpp
unsigned foo3(unsigned x, unsigned total) {
    unsigned ret = 0;
    ILP_FOR_RET(unsigned, i, 0, x, 4) {
        ret += i;
        if (ret > total) ILP_RETURN(ret);
    } ILP_END_RET;
    return ret;
}
```

---

## API Reference

### Index-Based Loops

#### Basic Loop (no return value)
```cpp
ILP_FOR(var, start, end, N) {
    // body
} ILP_END;
```

#### Loop with Return Value
```cpp
ILP_FOR_RET(ret_type, var, start, end, N) {
    // body
    if (condition) ILP_RETURN(value);
} ILP_END_RET;
```

#### Loop with Custom Step
```cpp
ILP_FOR_STEP(var, start, end, step, N) {
    // body
} ILP_END;

ILP_FOR_STEP_RET(ret_type, var, start, end, step, N) {
    // body
} ILP_END_RET;
```

### Range-Based Loops

#### Basic Range Loop
```cpp
ILP_FOR_RANGE(var, range, N) {
    // body
} ILP_END;
```

#### Range Loop with Return
```cpp
ILP_FOR_RANGE_RET(ret_type, var, range, N) {
    // body
} ILP_END_RET;
```

#### Enumerate (Index + Value)
```cpp
ILP_FOR_ENUMERATE(idx, val, range, N) {
    // idx = 0, 1, 2, ...
    // val = range[0], range[1], ...
} ILP_END;

ILP_FOR_ENUMERATE_RET(ret_type, idx, val, range, N) {
    // body
} ILP_END_RET;
```

### Control Flow Macros

| Macro | Description |
|-------|-------------|
| `ILP_CONTINUE` | Skip to next iteration |
| `ILP_BREAK` | Exit the loop |
| `ILP_RETURN(x)` | Return value from enclosing function |
| `throw expr;` | Works naturally (no macro needed) |

---

## Control Flow Semantics

### Continue
Skips the rest of the current iteration, proceeds to the next index:

```cpp
ILP_FOR(i, 0, 10, 4) {
    if (i % 2 == 0) ILP_CONTINUE;  // Skip even numbers
    sum += i;
} ILP_END;
// sum = 1 + 3 + 5 + 7 + 9 = 25
```

### Break
Exits the entire loop immediately:

```cpp
ILP_FOR(i, 0, 100, 4) {
    sum += i;
    if (sum > 50) ILP_BREAK;
} ILP_END;
```

### Return
Returns a value from the enclosing function:

```cpp
int find_index(const std::vector<int>& vec, int target) {
    ILP_FOR_RET(int, i, 0, vec.size(), 4) {
        if (vec[i] == target) ILP_RETURN(i);
    } ILP_END_RET;
    return -1;  // Not found
}
```

### Throw
Regular exceptions work naturally:

```cpp
ILP_FOR(i, 0, n, 4) {
    if (data[i] < 0)
        throw std::invalid_argument("negative value");
    process(data[i]);
} ILP_END;
```

---

## Signed Numbers and Reverse Iteration

### Negative Ranges
```cpp
ILP_FOR(i, -10, 10, 4) {
    // i = -10, -9, ..., 9
} ILP_END;
```

### Backward Iteration
```cpp
ILP_FOR_STEP(i, 100, 0, -1, 4) {
    // i = 100, 99, ..., 1
} ILP_END;
```

### Custom Step Sizes
```cpp
// Even numbers
ILP_FOR_STEP(i, 0, 100, 2, 4) {
    // i = 0, 2, 4, ..., 98
} ILP_END;

// Descending by 2
ILP_FOR_STEP(i, 100, 0, -2, 4) {
    // i = 100, 98, 96, ..., 2
} ILP_END;
```

---

## Iterator and Range Support

### Random Access Ranges (Optimal)
Uses direct indexing for maximum efficiency:
- `std::vector`, `std::array`, `std::span`, raw arrays

```cpp
std::vector<int> vec = {1, 2, 3, 4, 5, 6, 7, 8};
ILP_FOR_RANGE(val, vec, 4) {
    sum += val;
} ILP_END;
```

### Forward/Bidirectional Ranges
Collects N iterators, still benefits from unrolling:
- `std::list`, `std::forward_list`, `std::set`, `std::map`

```cpp
std::list<int> lst = {1, 2, 3, 4, 5, 6, 7, 8};
ILP_FOR_RANGE(val, lst, 4) {
    sum += val;
} ILP_END;
```

### Views and Pipelines (C++23)
```cpp
ILP_FOR_RANGE(val, vec | std::views::filter(is_positive), 4) {
    sum += val;
} ILP_END;
```

---

## Usage Examples

### Simple Accumulation
```cpp
unsigned sum_range(unsigned n) {
    unsigned sum = 0;
    ILP_FOR(i, 0, n, 4) {
        sum += i;
    } ILP_END;
    return sum;
}
```

### Search with Early Exit
```cpp
std::optional<size_t> find_value(std::span<const int> data, int target) {
    ILP_FOR_RET(size_t, i, 0, data.size(), 4) {
        if (data[i] == target) ILP_RETURN(i);
    } ILP_END_RET;
    return std::nullopt;
}
```

### Conditional Processing
```cpp
unsigned sum_odd(unsigned n) {
    unsigned sum = 0;
    ILP_FOR(i, 0, n, 4) {
        if (i % 2 == 0) ILP_CONTINUE;
        sum += i;
    } ILP_END;
    return sum;
}
```

### Range with Break
```cpp
int sum_until_negative(std::span<const int> data) {
    int sum = 0;
    ILP_FOR_RANGE(val, data, 4) {
        if (val < 0) ILP_BREAK;
        sum += val;
    } ILP_END;
    return sum;
}
```

### Enumerate
```cpp
void print_indexed(const std::vector<std::string>& items) {
    ILP_FOR_ENUMERATE(idx, item, items, 4) {
        std::cout << idx << ": " << item << "\n";
    } ILP_END;
}
```

### External Function Calls
```cpp
void process_all(unsigned n) {
    ILP_FOR(i, 0, n, 4) {
        external_process(i);  // Function defined elsewhere
    } ILP_END;
}
```

---

## Implementation Details

### Core Components

#### LoopCtrl Control Object
Uses direct bool for minimal overhead (no enum comparison):

```cpp
template<typename R = void>
struct LoopCtrl {
    bool ok = true;  // Direct bool check - faster than enum comparison
    std::optional<R> return_value;

    void break_loop() { ok = false; }
    void return_with(R val) {
        ok = false;
        return_value = std::move(val);
    }
};

// Void specialization (no return value)
template<>
struct LoopCtrl<void> {
    bool ok = true;
    void break_loop() { ok = false; }
};
```

#### Compile-Time Unrolling
Uses C++17 fold expressions with `&&` for short-circuit evaluation:

```cpp
template<std::size_t N, std::integral T, typename F>
void for_loop(T start, T end, F&& body) {
    LoopCtrl<void> ctrl;

    T i = start;

    // Unrolled loop
    for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((body(i + static_cast<T>(Is), ctrl), ctrl.ok) && ...);
        }(std::make_index_sequence<N>{});
    }

    // Remainder
    for (; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }
}
```

#### Range Optimization
Uses `std::ranges::size()` directly for sized ranges (faster than `distance()`):

```cpp
template<std::size_t N, std::ranges::random_access_range Range, typename F>
void for_loop_range(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);  // Direct size, not distance()
    // ...
}
```

#### Macro Expansion
The `ILP_FOR_RET` / `ILP_END_RET` pattern uses C++17 if-with-initializer:

```cpp
#define ILP_FOR_RET(ret_type, var, start, end, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_ret<ret_type, N>(start, end, \
            [&](auto var, auto& _ilp_ctrl)

#define ILP_END_RET ); !_ilp_done_; _ilp_done_ = true) \
            if (_ilp_r_) return *_ilp_r_
```

---

## Test Plan

### Assembly Comparison Tests

Verify that ILP_FOR generates identical assembly to hand-rolled loops.

#### Test Categories

1. **Plain accumulation** - No control flow
2. **Break on condition** - Unknown exit point
3. **Continue (skip items)** - Conditional processing
4. **Early return** - Search patterns
5. **External function calls** - Prevents over-optimization
6. **Range-based** - Vector, list, span
7. **Negative numbers** - Signed index types
8. **Backward iteration** - Negative step
9. **Custom step sizes** - ±2, etc.

#### Unroll Factors
- N = 2, 4, 8, 16

#### Edge Cases
- `n = 0` (empty loop)
- `n < N` (only remainder)
- `n == N` (exactly one unrolled block)
- `n == N + 1` (one block + one remainder)

### Test Infrastructure

```
tests/
├── asm_compare/
│   ├── handrolled.cpp        # Hand-rolled implementations
│   ├── ilp_versions.cpp      # ILP_FOR implementations
│   ├── compare_asm.sh        # Compile & diff assembly
│   └── Makefile
├── correctness/
│   ├── catch.hpp             # Catch2 header-only
│   └── test_correctness.cpp  # Unit tests
└── CMakeLists.txt
```

### Unit Test Framework

Using [Catch2](https://github.com/catchorg/Catch2) (header-only) for correctness tests:

```cpp
#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "ilp_for.hpp"

TEST_CASE("Plain accumulation matches hand-rolled", "[basic]") {
    REQUIRE(sum_ilp(100) == sum_handrolled(100));
}

TEST_CASE("Break exits correctly", "[control]") {
    REQUIRE(break_ilp(100, 50) == break_handrolled(100, 50));
}

TEST_CASE("Edge case: empty range", "[edge]") {
    REQUIRE(sum_ilp(0) == 0);
}
```

### Compiler Flags
```bash
-std=c++23 -O3 -fno-exceptions -fno-rtti
```

Functions marked `__attribute__((noinline))` to prevent cross-function optimization.

---

## Gotchas and Warnings

### ⚠️ Don't Use Bare `return`

```cpp
int foo() {
    ILP_FOR(i, 0, n, 4) {
        if (found) return 42;  // WRONG: acts like ILP_CONTINUE!
    } ILP_END;
    return 0;
}
```

**Correct:**
```cpp
int foo() {
    ILP_FOR_RET(int, i, 0, n, 4) {
        if (found) ILP_RETURN(42);  // Correct
    } ILP_END_RET;
    return 0;
}
```

### ⚠️ Don't Use Bare `break`

```cpp
ILP_FOR(i, 0, n, 4) {
    if (done) break;  // COMPILE ERROR
} ILP_END;
```

**Correct:**
```cpp
ILP_FOR(i, 0, n, 4) {
    if (done) ILP_BREAK;  // Correct
} ILP_END;
```

### ⚠️ Don't Use Bare `continue`

```cpp
ILP_FOR(i, 0, n, 4) {
    if (skip) continue;  // COMPILE ERROR
} ILP_END;
```

**Correct:**
```cpp
ILP_FOR(i, 0, n, 4) {
    if (skip) ILP_CONTINUE;  // Correct
} ILP_END;
```

### ✓ Regular `throw` Works Fine

```cpp
ILP_FOR(i, 0, n, 4) {
    if (error) throw std::runtime_error("msg");  // OK
} ILP_END;
```

### ⚠️ Don't Forget `ILP_END`

```cpp
ILP_FOR(i, 0, n, 4) {
    work(i);
}  // COMPILE ERROR: missing ILP_END
```

---

## Future Enhancements (v2)

- **Loop else clause**: Execute code if loop completes without break
- **Labeled breaks**: Break out of nested loops
- **Parallel hints**: Compiler hints for vectorization
- **Coroutine support**: `co_yield` integration

---

## Requirements

- C++23 or later
- Compiler with fold expression support (GCC 10+, Clang 12+, MSVC 19.29+)

---

## License

[TBD]
