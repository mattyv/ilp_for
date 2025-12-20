// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

// Test file for ilp-loop-analysis clang-tidy check
// Each function demonstrates a different LoopType pattern
//
// KNOWN LIMITATIONS (v0.5):
// - Sqrt: std::sqrt in generic lambdas detected as Transform (template instantiation issue)
// - MinMax: std::min/max in generic lambdas not detected (template instantiation issue)
// These will be addressed in a future version by matching instantiated templates.

#include "ilp_for.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>

// 1. Sum pattern: acc += val
// Expected: DetectedLoopType::Sum, N varies by type
void test_sum_float(const float* data, std::size_t n) {
    float sum = 0;
    ILP_FOR(auto i, 0uz, n, 8) {
        // CHECK: warning: Loop body contains Sum pattern
        sum += data[i];
    }
    ILP_END;
}

void test_sum_double(const double* data, std::size_t n) {
    double sum = 0;
    ILP_FOR(auto i, 0uz, n, 8) {
        // CHECK: warning: Loop body contains Sum pattern
        sum += data[i];
    }
    ILP_END;
}

void test_sum_int(const int* data, std::size_t n) {
    int sum = 0;
    ILP_FOR(auto i, 0uz, n, 3) {
        // CHECK: warning: Loop body contains Sum pattern
        sum += data[i];
    }
    ILP_END;
}

// 2. DotProduct pattern: acc += a * b (FMA)
// Expected: DetectedLoopType::DotProduct, N=8 for float/double
void test_dotproduct_float(const float* a, const float* b, std::size_t n) {
    float dot = 0;
    ILP_FOR(auto i, 0uz, n, 8) {
        // CHECK: warning: Loop body contains DotProduct pattern
        dot += a[i] * b[i];
    }
    ILP_END;
}

void test_dotproduct_double(const double* a, const double* b, std::size_t n) {
    double dot = 0;
    ILP_FOR(auto i, 0uz, n, 8) {
        // CHECK: warning: Loop body contains DotProduct pattern
        dot += a[i] * b[i];
    }
    ILP_END;
}

// 3. Search pattern: early exit
// Expected: DetectedLoopType::Search, N=4
void test_search(const int* data, std::size_t n, int target) {
    ILP_FOR(auto i, 0uz, n, 4) {
        // CHECK: warning: Loop body contains Search pattern
        if (data[i] == target)
            return; // Early exit
    }
    ILP_END;
}

// 4. Copy pattern: dst[i] = src[i]
// Expected: DetectedLoopType::Copy
void test_copy(const double* src, double* dst, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 4) {
        // CHECK: warning: Loop body contains Copy pattern
        dst[i] = src[i];
    }
    ILP_END;
}

// 5. Transform pattern: dst[i] = f(src[i])
// Expected: DetectedLoopType::Transform
void test_transform(const double* src, double* dst, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 4) {
        // CHECK: warning: Loop body contains Transform pattern
        dst[i] = std::sqrt(src[i]); // Transform with function
    }
    ILP_END;
}

// 6. Multiply pattern: acc *= val
// Expected: DetectedLoopType::Multiply
void test_multiply_float(const float* data, std::size_t n) {
    float product = 1;
    ILP_FOR(auto i, 0uz, n, 8) {
        // CHECK: warning: Loop body contains Multiply pattern
        product *= data[i];
    }
    ILP_END;
}

void test_multiply_int(const int* data, std::size_t n) {
    int product = 1;
    ILP_FOR(auto i, 0uz, n, 10) {
        // CHECK: warning: Loop body contains Multiply pattern
        product *= data[i];
    }
    ILP_END;
}

// 7. Divide pattern: x / y
// Expected: DetectedLoopType::Divide, N=2
void test_divide(const float* data, float* result, float divisor, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 2) {
        // CHECK: warning: Loop body contains Divide pattern
        result[i] = data[i] / divisor;
    }
    ILP_END;
}

// 8. Sqrt pattern: sqrt(x)
// Expected: DetectedLoopType::Sqrt, N=2
void test_sqrt(const float* data, float* result, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 2) {
        // CHECK: warning: Loop body contains Sqrt pattern
        result[i] = std::sqrt(data[i]);
    }
    ILP_END;
}

// 9. MinMax pattern: std::min/max
// Expected: DetectedLoopType::MinMax
void test_minmax_float(const float* data, std::size_t n) {
    float min_val = data[0];
    ILP_FOR(auto i, 1uz, n, 8) {
        // CHECK: warning: Loop body contains MinMax pattern
        min_val = std::min(min_val, data[i]);
    }
    ILP_END;
}

void test_minmax_int(const int* data, std::size_t n) {
    int max_val = data[0];
    ILP_FOR(auto i, 1uz, n, 2) {
        // CHECK: warning: Loop body contains MinMax pattern
        max_val = std::max(max_val, data[i]);
    }
    ILP_END;
}

// 10. Bitwise pattern: &=, |=, ^=
// Expected: DetectedLoopType::Bitwise, N=3
void test_bitwise_and(const unsigned* data, std::size_t n) {
    unsigned result = ~0u;
    ILP_FOR(auto i, 0uz, n, 3) {
        // CHECK: warning: Loop body contains Bitwise pattern
        result &= data[i];
    }
    ILP_END;
}

void test_bitwise_or(const unsigned* data, std::size_t n) {
    unsigned result = 0;
    ILP_FOR(auto i, 0uz, n, 3) {
        // CHECK: warning: Loop body contains Bitwise pattern
        result |= data[i];
    }
    ILP_END;
}

void test_bitwise_xor(const unsigned* data, std::size_t n) {
    unsigned result = 0;
    ILP_FOR(auto i, 0uz, n, 3) {
        // CHECK: warning: Loop body contains Bitwise pattern
        result ^= data[i];
    }
    ILP_END;
}

// 11. Shift pattern: <<, >>
// Expected: DetectedLoopType::Shift, N=2
void test_shift_left(const unsigned* data, unsigned* result, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 2) {
        // CHECK: warning: Loop body contains Shift pattern
        result[i] = data[i] << 2;
    }
    ILP_END;
}

void test_shift_right(const unsigned* data, unsigned* result, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 2) {
        // CHECK: warning: Loop body contains Shift pattern
        result[i] = data[i] >> 2;
    }
    ILP_END;
}
