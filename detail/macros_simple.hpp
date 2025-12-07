#pragma once

// SUPER_SIMPLE mode: Direct for-loop expansion with native break/continue/return
// Uses range-based for with ilp::iota to sidestep preprocessor limitations

#include "iota.hpp"

#define ILP_FOR(loop_var_decl, start, end, N) for (loop_var_decl : ::ilp::iota((start), (end)))

#define ILP_FOR_RANGE(loop_var_decl, range, N) for (loop_var_decl : (range))

#define ILP_FOR_AUTO(loop_var_decl, start, end) for (loop_var_decl : ::ilp::iota((start), (end)))

#define ILP_FOR_RANGE_AUTO(loop_var_decl, range) for (loop_var_decl : (range))

#define ILP_END
#define ILP_END_RETURN

#define ILP_CONTINUE continue

#define ILP_BREAK break

#define ILP_RETURN(x) return x
