# ilp-loop-analysis clang-tidy Check

A best-effort static analysis tool that detects ILP loop patterns and suggests optimal `LoopType` or `N` values.

> **Note:** I'd really like this tool to be community-driven. If you encounter incorrect pattern detection or have improvements, please contribute back to help other users.

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

Once you've got it built, here's how to run it.

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

Add `--fix` to automatically convert `ILP_FOR` to `ILP_FOR_AUTO` with the detected pattern (pretty handy):

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

Note: The `-checks` argument uses escaped asterisks (`-\\*,ilp-\\*`) to prevent shell glob expansion. Yeah, I know its ugly.

## Detected Patterns

Here's what the tool can spot:

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

### Pattern Selection (max-N)

When a loop contains multiple patterns, the check selects the pattern requiring the **highest N value** (the bottleneck). This follows Agner Fog's guidance: *"You may have multiple carried dependency chains in a loop: the speed limit is set by the longest."*

The formula is pretty simple: **N = Latency × Throughput-per-cycle**

| Pattern | Example N (float) | Formula |
|---------|-------------------|---------|
| DotProduct | 8 | FMA: L=4, TPC=2 |
| Sum | 8 | FP Add: L=4, TPC=2 |
| MinMax (FP) | 8 | L=4, TPC=2 |
| Multiply | 8 | L=4, TPC=2 |
| Search | 4 | Branch + compare |
| Transform | 4 | Memory + compute |
| Bitwise | 3 | L=1, TPC=3 |
| Sqrt | 2 | L=12, TPC=0.17 |
| Divide | 2 | L=11, TPC=0.2 |
| Shift | 2 | L=1, TPC=2 |

For example, a loop with both `std::sqrt` and an indexed write (`result[i] = std::sqrt(...)`) is classified as Transform (N=4) because Transform requires more parallel chains than Sqrt (N=2).

## Output

The check spits out warnings with a fix hint:

```
file.cpp:42:5: warning: Loop body contains DotProduct pattern [ilp-loop-analysis]
    ILP_FOR(auto i, 0uz, n, 4) {
    ^~~~~~~~~~~~~~~~~~~~~~~~~~~
    ILP_FOR_AUTO(auto i, 0uz, n, DotProduct)
file.cpp:42:5: note: Portable fix: use ILP_FOR_AUTO with LoopType::DotProduct
file.cpp:42:5: note: Architecture-specific fix for skylake: use ILP_FOR with N=8
```

Run with `--fix` to apply the change automatically.

## Configuration

You can tweak these options via `.clang-tidy` or command line:

| Option | Default | Description |
|--------|---------|-------------|
| `TargetCPU` | `skylake` | CPU profile for N values (`skylake`, `alderlake`, `zen5`, `apple_m1`) |
| `PreferPortableFix` | `true` | Use `ILP_FOR_AUTO` fix instead of architecture-specific `ILP_FOR` |

Example `.clang-tidy`:

```yaml
Checks: '-*,ilp-*'
CheckOptions:
  - key: ilp-loop-analysis.TargetCPU
    value: apple_m1
  - key: ilp-loop-analysis.PreferPortableFix
    value: true
```
