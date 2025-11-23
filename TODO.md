# TODO

## Macro Improvements

- [x] **Fix `ILP_FOR_RANGE_IDX_RET_SIMPLE` macro pattern** - The current macro doesn't work well when you need to capture the result as a variable (e.g., in benchmarks). The `ILP_END_RET` pattern tries to return from the enclosing function which doesn't fit all use cases. Consider adding an alternative pattern or macro that allows capturing the result directly, e.g.:
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


## Get benchmarks to use auto unrolling based on cpu type
 - [x] Updated benchmarks to use `*_AUTO` variants 

 ## add a #define mechanims to disble unrolling to allow develoopers to quickly a:b simple vs ILP loops.

 ## update benchmarks to be in nanoseconds