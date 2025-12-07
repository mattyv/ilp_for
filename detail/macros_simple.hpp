// ilp_for - ILP loop unrolling for C++23
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: MIT

#pragma once

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
