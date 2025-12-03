# TODO

## Macros
- [x] Consolidate ILP_FOR + ILP_FOR_RET into single ILP_FOR(ret_type, ...) macro
- [x] Consolidate ILP_FOR_RANGE + ILP_FOR_RANGE_RET into single ILP_FOR_RANGE(ret_type, ...) macro
- [x] Add ILP_FOR_AUTO and ILP_FOR_RANGE_AUTO macros
- [ ] Update ILP_FIND, ILP_FIND_RANGE_IDX with same pattern
- [ ] Remove vestigial _ilp_ctrl from other macros (cleaner code)
- [ ] ILP_FIND uses neat return. for consistency should we also allow/encourage use of ILP_RETURN?

## Testing
- [x] Add UBSan (Undefined Behavior Sanitizer) to test builds
- [x] Add ASan (Address Sanitizer) to test builds

## Build
- [x] Add UBSan CMake option (`-DENABLE_UBSAN=ON`)
- [x] Add ASan CMake option (`-DENABLE_ASAN=ON`)

## CI
- [x] Add all ILP modes (ILP, SIMPLE, PRAGMA) testing to GitHub CI
