#pragma once

// Loop implementation dispatcher
// Includes the appropriate implementation based on mode selection

#if defined(ILP_MODE_SIMPLE)
    // Plain sequential loops for testing/debugging
    #include "loops_simple.hpp"
#elif defined(ILP_MODE_PRAGMA)
    // Pragma-unrolled loops for GCC auto-vectorization
    #include "loops_pragma.hpp"
#else
    // Full ILP pattern (default) - best for Clang ccmp optimization
    #include "loops_ilp.hpp"
#endif
