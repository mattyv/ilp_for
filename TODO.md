# TODO

## Macros
- [x] Consolidate ILP_FOR + ILP_FOR_RET into single ILP_FOR(ret_type, ...) macro
- [x] Consolidate ILP_FOR_RANGE + ILP_FOR_RANGE_RET into single ILP_FOR_RANGE(ret_type, ...) macro
- [x] Add ILP_FOR_AUTO and ILP_FOR_RANGE_AUTO macros
- [x] Remove ILP_FIND* and ILP_REDUCE* macros (use ilp::find, ilp::reduce functions instead)
- [x] Try super simple version (like SIMPLE mode): `#define ILP_FOR for(...) {...}` etc.
- [x] Remove old SIMPLE/PRAGMA modes, rename SUPER_SIMPLE -> SIMPLE

## Testing
- [x] Add UBSan (Undefined Behavior Sanitizer) to test builds
- [x] Add ASan (Address Sanitizer) to test builds
- [ ] Update coverage report
- [ ] Regen godbolt links
- [ ] Add more extreme unit tests for ILP_FOR (edge cases, stress tests)
- [x] use clang-format
- [ ] Get rid of all the silly LLM comments and formatting.
- [ ] code review with gemini 3

## Build
- [x] Add UBSan CMake option (`-DENABLE_UBSAN=ON`)
- [x] Add ASan CMake option (`-DENABLE_ASAN=ON`)

## CI
- [x] Add all ILP modes (ILP, SIMPLE) testing to GitHub CI

## Experimental
- [ ] Experiment with DSL for calculating ILP unroll value for N
