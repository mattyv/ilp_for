# TODO

## Macro Improvements

- [ ] **Fix `ILP_FOR_RANGE_IDX_RET_SIMPLE` macro pattern** - The current macro doesn't work well when you need to capture the result as a variable (e.g., in benchmarks). The `ILP_END_RET` pattern tries to return from the enclosing function which doesn't fit all use cases. Consider adding an alternative pattern or macro that allows capturing the result directly, e.g.:
  ```cpp
  // Current (doesn't work in all contexts):
  ILP_FOR_RANGE_IDX_RET_SIMPLE(std::size_t, val, idx, arr, 4) {
      if (val == target) return idx;
  } ILP_END_RET else { /* not found */ }

  // Desired (captures result):
  auto result = ILP_FOR_RANGE_IDX_RET_SIMPLE_EXPR(std::size_t, val, idx, arr, 4) {
      if (val == target) return idx;
  } ILP_END_EXPR;
  ```


## Get benchmarks to use auto unrulling based on cpu type
 - if the macro or function doesn't exist create it. 