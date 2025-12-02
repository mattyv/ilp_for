# TODO

## Macros
- [ ] Update ILP_FOR, ILP_FOR_RET, ILP_FOR_RANGE, ILP_FOR_RANGE_RET with same outer lambda pattern as ILP_REDUCE
- [ ] Update ILP_FIND, ILP_FIND_RANGE_IDX with same pattern

## Testing
- [x] Add UBSan (Undefined Behavior Sanitizer) to test builds
- [x] Add ASan (Address Sanitizer) to test builds

## Build
- [x] Add UBSan CMake option (`-DENABLE_UBSAN=ON`)
- [x] Add ASan CMake option (`-DENABLE_ASAN=ON`)

## CI
- [x] Add all ILP modes (ILP, SIMPLE, PRAGMA) testing to GitHub CI
