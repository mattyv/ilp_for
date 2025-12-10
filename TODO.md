# TODO

## Macros
- [x] Consolidate ILP_FOR + ILP_FOR_RET into single ILP_FOR(ret_type, ...) macro
- [x] Consolidate ILP_FOR_RANGE + ILP_FOR_RANGE_RET into single ILP_FOR_RANGE(ret_type, ...) macro
- [x] Add ILP_FOR_AUTO and ILP_FOR_RANGE_AUTO macros
- [x] Remove ILP_FIND* and ILP_REDUCE* macros (use ilp::find, ilp::reduce functions instead)
- [x] Try super simple version (like SIMPLE mode): `#define ILP_FOR for(...) {...}` etc.
- [x] Remove old SIMPLE/PRAGMA modes, rename SUPER_SIMPLE -> SIMPLE
- [ ] ~~ILP_WHILE/ILP_DO_WHILE~~ More research needed

## Testing
- [x] Add UBSan (Undefined Behavior Sanitizer) to test builds
- [x] Add ASan (Address Sanitizer) to test builds
- [x] Update coverage report
- [x] Regen godbolt links
- [ ] Add more extreme unit tests for ILP_FOR (edge cases, stress tests)
- [x] use clang-format
- [x] Get rid of all the silly LLM comments and formatting.
- [ ] code review with gemini 3
- [ ] are asm test serving any real purpose?
- [ ] might want to overhaul benchmarks

## Build
- [x] Add UBSan CMake option (`-DENABLE_UBSAN=ON`)
- [x] Add ASan CMake option (`-DENABLE_ASAN=ON`)

## CI
- [x] Add all ILP modes (ILP, SIMPLE) testing to GitHub CI

## Experimental
- [ ] Experiment with DSL for calculating ILP unroll value for N

## Guts
- [x] AnyStorage with fixed buffer seems odd.

## CPU Profiles
- [ ] Add compare/branch instruction info to cpu profile headers (FCMP, VUCOMISS latency/throughput)
- [ ] Document Search loop N derivation from compare instruction characteristics

## Misc
- [ ] Optionally compile in and out cpp 23 features
- [x] M1 and cortex don't work in ILP N analyser
- [ ] ARM64 MCA on Godbolt still showing llvm-mc errors (may be Godbolt infra issue)