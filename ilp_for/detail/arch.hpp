// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>
#include <cstdint>

namespace ilp::arch {

    /// Size of the largest integral type on this architecture.
    /// Typically 8 bytes on 64-bit platforms, 4 bytes on 32-bit.
    inline constexpr std::size_t max_integral_size = sizeof(std::intmax_t);

/// SBO buffer size for SmallStorage. Defaults to max integral size.
/// Override with -DILP_SBO_SIZE=N if needed.
#ifdef ILP_SBO_SIZE
    inline constexpr std::size_t sbo_size = ILP_SBO_SIZE;
#else
    inline constexpr std::size_t sbo_size = max_integral_size;
#endif

} // namespace ilp::arch
