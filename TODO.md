# TODO

## Macros
- [ ] Update ILP_FOR, ILP_FOR_RET, ILP_FOR_RANGE, ILP_FOR_RANGE_RET with same outer lambda pattern as ILP_REDUCE
- [ ] Update ILP_FIND, ILP_FIND_RANGE_IDX with same pattern

## Testing
- [ ] Add UBSan (Undefined Behavior Sanitizer) to test builds
- [ ] Update copy count tests to expect moves instead of copies (0 copies is correct now)

## Build
- [ ] Add UBSan CMake option (`-fsanitize=undefined`)
- [ ] Consider ASan (Address Sanitizer) integration

## CI
- [ ] Add all ILP modes (ILP, SIMPLE, PRAGMA) testing to GitHub CI
