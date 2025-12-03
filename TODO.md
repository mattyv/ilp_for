# TODO

## Macros
- [x] Consolidate ILP_FOR + ILP_FOR_RET into single ILP_FOR(ret_type, ...) macro
- [x] Consolidate ILP_FOR_RANGE + ILP_FOR_RANGE_RET into single ILP_FOR_RANGE(ret_type, ...) macro
- [x] Add ILP_FOR_AUTO and ILP_FOR_RANGE_AUTO macros
- [x] Update ILP_FIND, ILP_FIND_RANGE_IDX with same pattern (added AUTO versions)
- [x] Remove vestigial _ilp_ctrl from other macros (cleaner code)
- [ ] ILP_FIND uses neat return. for consistency should we also allow/encourage use of ILP_RETURN?
- [ ] ILP_REDUCE_RANGE_AUTO example uses neat return.
- [ ] Try super simple version (like SIMPLE mode): `#define ILP_FOR for(...) {...}` etc. For this to work current syntax for ILP DSL needs to be a lot more tidy and conformant.

## Testing
- [x] Add UBSan (Undefined Behavior Sanitizer) to test builds
- [x] Add ASan (Address Sanitizer) to test builds
- [ ] Update coverage report
- [ ] Regen godbolt links

## Build
- [x] Add UBSan CMake option (`-DENABLE_UBSAN=ON`)
- [x] Add ASan CMake option (`-DENABLE_ASAN=ON`)

## CI
- [x] Add all ILP modes (ILP, SIMPLE, PRAGMA) testing to GitHub CI

## Experimental
- [ ] Experiment with DSL for calculating ILP unroll value for N
