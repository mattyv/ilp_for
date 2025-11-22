#pragma once

// Compiler barrier to prevent optimization of inputs/outputs
// Similar to Google Benchmark's DoNotOptimize

template<typename T>
inline void escape(T& val) {
    asm volatile("" : "+r,m"(val) : : "memory");
}

template<typename T>
inline void escape(T* val) {
    asm volatile("" : "+r"(val) : : "memory");
}
