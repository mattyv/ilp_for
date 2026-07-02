// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

namespace ilp {

    // Selects whether the function API unrolls (Unrolled) or degrades to a plain,
    // single bounds-check-per-iteration loop (Simple). Mirrors the ILP_MODE_SIMPLE
    // macro switch, but is expressible per-call-site via an explicit template
    // argument in addition to the global default below.
    enum class Mode { Unrolled, Simple };

#ifdef ILP_MODE_SIMPLE
    inline constexpr Mode default_mode = Mode::Simple;
#else
    inline constexpr Mode default_mode = Mode::Unrolled;
#endif

} // namespace ilp
