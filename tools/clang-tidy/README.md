# ilp-loop-analysis clang-tidy Check

Static analysis tool that detects ILP loop patterns and suggests optimal `LoopType` or `N` values.

## Building

Requires LLVM/Clang development packages.

### macOS (Homebrew)

```bash
brew install llvm

cd tools/clang-tidy
cmake -B build \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/llvm" \
  -DLLVM_DIR="/opt/homebrew/opt/llvm/lib/cmake/llvm" \
  -DClang_DIR="/opt/homebrew/opt/llvm/lib/cmake/clang"
cmake --build build
```

### Ubuntu

```bash
sudo apt-get install llvm-18-dev libclang-18-dev clang-18 clang-tidy-18

cd tools/clang-tidy
cmake -B build \
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm \
  -DClang_DIR=/usr/lib/llvm-18/lib/cmake/clang
cmake --build build
```

## Usage

### Command Line

```bash
# Analyze a file
clang-tidy \
  -load path/to/ILPTidyModule.so \
  -checks='-*,ilp-*' \
  your_file.cpp \
  -- -std=c++20

# On macOS with Homebrew LLVM
/opt/homebrew/opt/llvm/bin/clang-tidy \
  -load tools/clang-tidy/build/ILPTidyModule.so \
  -checks='-*,ilp-*' \
  your_file.cpp \
  -- -std=c++20
```

### Auto-Fix

Add `--fix` to automatically convert `ILP_FOR` to `ILP_FOR_AUTO` with the detected pattern:

```bash
clang-tidy \
  -load path/to/ILPTidyModule.so \
  -checks='-*,ilp-*' \
  --fix \
  your_file.cpp \
  -- -std=c++20
```

Before:
```cpp
ILP_FOR(auto i, 0uz, n, 4) {
    sum += data[i];
} ILP_END;
```

After:
```cpp
ILP_FOR_AUTO(auto i, 0uz, n, Sum) {
    sum += data[i];
} ILP_END;
```

### VSCode Integration

Copy the example tasks configuration:

```bash
cp tools/clang-tidy/vscode/tasks.json .vscode/
```

On macOS, edit `.vscode/tasks.json` to use the full path to Homebrew's clang-tidy:

```json
"command": "/opt/homebrew/opt/llvm/bin/clang-tidy",
```

Then run via `Cmd+Shift+P` (macOS) or `Ctrl+Shift+P` (Linux/Windows):
1. Select "Tasks: Run Task"
2. Choose "ILP Loop Analysis (current file)" or "ILP Loop Analysis (test patterns)"

Warnings will appear in the Problems panel.

Note: The `-checks` argument uses escaped asterisks (`-\\*,ilp-\\*`) to prevent shell glob expansion.

## Detected Patterns

| Pattern | Example | Detected As |
|---------|---------|-------------|
| Sum | `acc += val` | Sum |
| FMA | `acc += a * b` | DotProduct |
| Product | `acc *= val` | Multiply |
| Early Exit | `if (...) return` | Search |
| Copy | `dst[i] = src[i]` | Copy |
| Transform | `dst[i] = f(src[i])` | Transform |
| Division | `x / y` | Divide |
| Sqrt | `std::sqrt(x)` | Sqrt |
| Min/Max | `std::min(a, b)` | MinMax |
| Bitwise | `acc &= x`, `\|=`, `^=` | Bitwise |
| Shift | `x << n`, `x >> n` | Shift |

### Pattern Priority

When a loop contains multiple patterns, the highest-priority pattern determines the LoopType:

| Priority | Pattern | LoopType | Rationale |
|----------|---------|----------|-----------|
| 1 | Early exit | Search | Branch prediction dominates |
| 2 | Sqrt | Sqrt | Highest latency operation |
| 3 | Division | Divide | High latency operation |
| 4 | FMA (`acc += a * b`) | DotProduct | FMA unit throughput |
| 5 | Compound multiply | Multiply | Multiply throughput |
| 6 | Min/Max | MinMax | Comparison chain |
| 7 | Bitwise | Bitwise | ALU throughput |
| 8 | Shift | Shift | ALU throughput |
| 9 | Transform | Transform | Memory + compute |
| 10 | Copy | Copy | Memory bandwidth |
| 11 | Compound add | Sum | Default accumulator |

For example, a loop with both FMA (`sum += a[i] * b[i]`) and a transform (`dst[i] = f(src[i])`) is classified as DotProduct because FMA has higher priority.

## Output

The check emits warnings with a fix hint:

```
file.cpp:42:5: warning: Loop body contains DotProduct pattern [ilp-loop-analysis]
    ILP_FOR(auto i, 0uz, n, 4) {
    ^~~~~~~~~~~~~~~~~~~~~~~~~~~
    ILP_FOR_AUTO(auto i, 0uz, n, DotProduct)
file.cpp:42:5: note: Portable fix: use ILP_FOR_AUTO with LoopType::DotProduct
file.cpp:42:5: note: Architecture-specific fix for skylake: use ILP_FOR with N=8
```

Run with `--fix` to apply the suggested change automatically.

## Configuration

Options can be set via `.clang-tidy` or command line:

- `TargetCPU`: CPU profile for N values (default: `skylake`, also: `apple_m1`)
- `PreferPortableFix`: Use portable fix by default (default: `true`)
