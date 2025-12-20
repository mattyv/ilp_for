// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

// Unit tests for ilp-loop-analysis clang-tidy check
// Tests pattern detection and auto-fix functionality

#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// Helper to run clang-tidy and capture output
struct ClangTidyResult {
    int exitCode;
    std::string output;
};

// Get clang-tidy path from environment or use defaults
std::string getClangTidyPath() {
    // Check CLANG_TIDY environment variable first
    if (const char* env = std::getenv("CLANG_TIDY")) {
        return env;
    }
    // Try common paths
    const char* paths[] = {
        "clang-tidy-18",                         // Ubuntu with versioned binary
        "clang-tidy",                            // Generic
        "/opt/homebrew/opt/llvm/bin/clang-tidy", // macOS Homebrew
        "/usr/local/opt/llvm/bin/clang-tidy",    // macOS Homebrew (Intel)
    };
    for (const char* path : paths) {
        std::string cmd = std::string("which ") + path + " >/dev/null 2>&1";
        if (std::system(cmd.c_str()) == 0) {
            return path;
        }
    }
    return "clang-tidy"; // Fallback
}

// Get project root directory (contains ilp_for.hpp)
std::string getProjectRoot() {
    // From tools/clang-tidy, go up two levels to project root
    return fs::current_path().parent_path().parent_path().string();
}

ClangTidyResult runClangTidy(const std::string& inputFile, bool fix = false) {
    static std::string clangTidyPath = getClangTidyPath();
    static std::string projectRoot = getProjectRoot();

    // Build command
    std::string cmd = clangTidyPath + " -load ";
    cmd += fs::current_path().string() + "/build/ILPTidyModule.so";
    cmd += " -checks='-*,ilp-*'";
    if (fix) {
        cmd += " --fix";
    }
    cmd += " " + inputFile;
    cmd += " -- -std=c++20 -I" + projectRoot + " 2>&1";

    // Run and capture output
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return {-1, "Failed to run clang-tidy"};
    }

    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }

    int exitCode = pclose(pipe);
    return {exitCode, output};
}

// Helper to read file contents
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Helper to write file contents
void writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
}

// Test input: ILP_FOR patterns to detect
const char* TEST_INPUT_SUM = R"(
#include "ilp_for.hpp"

void test_sum(const int* arr, std::size_t n) {
    int total = 0;
    ILP_FOR(auto i, 0uz, n, 4) {
        total += arr[i];
    } ILP_END;
}
)";

const char* TEST_INPUT_DOTPRODUCT = R"(
#include "ilp_for.hpp"

void test_dotproduct(const double* a, const double* b, std::size_t n) {
    double sum = 0.0;
    ILP_FOR(auto i, 0uz, n, 4) {
        sum += a[i] * b[i];
    } ILP_END;
}
)";

const char* TEST_INPUT_SEARCH = R"(
#include "ilp_for.hpp"

int test_search(const int* arr, std::size_t n, int target) {
    int found = -1;
    ILP_FOR(auto i, 0uz, n, 4) {
        if (arr[i] == target) {
            found = i;
            return found;
        }
    } ILP_END;
    return found;
}
)";

const char* TEST_INPUT_COPY = R"(
#include "ilp_for.hpp"

void test_copy(const int* src, int* dst, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 4) {
        dst[i] = src[i];
    } ILP_END;
}
)";

const char* TEST_INPUT_TRANSFORM = R"(
#include "ilp_for.hpp"

int square(int x) { return x * x; }

void test_transform(const int* src, int* dst, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 4) {
        dst[i] = square(src[i]);
    } ILP_END;
}
)";

const char* TEST_INPUT_MULTIPLY = R"(
#include "ilp_for.hpp"

void test_multiply(const double* arr, std::size_t n) {
    double product = 1.0;
    ILP_FOR(auto i, 0uz, n, 4) {
        product *= arr[i];
    } ILP_END;
}
)";

const char* TEST_INPUT_DIVIDE = R"(
#include "ilp_for.hpp"

void test_divide(const double* a, const double* b, double* c, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 2) {
        c[i] = a[i] / b[i];
    } ILP_END;
}
)";

const char* TEST_INPUT_BITWISE = R"(
#include "ilp_for.hpp"

void test_bitwise(const unsigned* arr, std::size_t n) {
    unsigned result = 0xFFFFFFFF;
    ILP_FOR(auto i, 0uz, n, 3) {
        result &= arr[i];
    } ILP_END;
}
)";

const char* TEST_INPUT_SHIFT = R"(
#include "ilp_for.hpp"

void test_shift(unsigned* arr, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 2) {
        arr[i] = arr[i] << 1;
    } ILP_END;
}
)";

// Test pattern detection
TEST_CASE("Pattern detection", "[clang-tidy][detection]") {
    std::string tmpFile = "/tmp/ilp_clang_tidy_test.cpp";

    SECTION("Sum pattern") {
        writeFile(tmpFile, TEST_INPUT_SUM);
        auto result = runClangTidy(tmpFile);
        REQUIRE(result.output.find("Sum pattern") != std::string::npos);
    }

    SECTION("DotProduct pattern") {
        writeFile(tmpFile, TEST_INPUT_DOTPRODUCT);
        auto result = runClangTidy(tmpFile);
        REQUIRE(result.output.find("DotProduct pattern") != std::string::npos);
    }

    SECTION("Search pattern") {
        writeFile(tmpFile, TEST_INPUT_SEARCH);
        auto result = runClangTidy(tmpFile);
        REQUIRE(result.output.find("Search pattern") != std::string::npos);
    }

    SECTION("Copy pattern") {
        writeFile(tmpFile, TEST_INPUT_COPY);
        auto result = runClangTidy(tmpFile);
        REQUIRE(result.output.find("Copy pattern") != std::string::npos);
    }

    SECTION("Transform pattern") {
        writeFile(tmpFile, TEST_INPUT_TRANSFORM);
        auto result = runClangTidy(tmpFile);
        REQUIRE(result.output.find("Transform pattern") != std::string::npos);
    }

    SECTION("Multiply pattern") {
        writeFile(tmpFile, TEST_INPUT_MULTIPLY);
        auto result = runClangTidy(tmpFile);
        REQUIRE(result.output.find("Multiply pattern") != std::string::npos);
    }

    SECTION("Divide pattern") {
        writeFile(tmpFile, TEST_INPUT_DIVIDE);
        auto result = runClangTidy(tmpFile);
        REQUIRE(result.output.find("Divide pattern") != std::string::npos);
    }

    SECTION("Bitwise pattern") {
        writeFile(tmpFile, TEST_INPUT_BITWISE);
        auto result = runClangTidy(tmpFile);
        REQUIRE(result.output.find("Bitwise pattern") != std::string::npos);
    }

    SECTION("Shift pattern") {
        writeFile(tmpFile, TEST_INPUT_SHIFT);
        auto result = runClangTidy(tmpFile);
        REQUIRE(result.output.find("Shift pattern") != std::string::npos);
    }

    // Cleanup
    fs::remove(tmpFile);
}

// Test that regular for loops are not detected
TEST_CASE("Regular for loops not detected", "[clang-tidy][negative]") {
    std::string tmpFile = "/tmp/ilp_clang_tidy_test.cpp";

    const char* regularForLoop = R"(
#include <cstddef>

void test_regular_for(const int* arr, std::size_t n) {
    int sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += arr[i];
    }
}
)";

    writeFile(tmpFile, regularForLoop);
    auto result = runClangTidy(tmpFile);

    // Should not find any ILP patterns
    REQUIRE(result.output.find("ilp-loop-analysis") == std::string::npos);

    fs::remove(tmpFile);
}

// Test fix suggestions are present
TEST_CASE("Fix suggestions", "[clang-tidy][fix]") {
    std::string tmpFile = "/tmp/ilp_clang_tidy_test.cpp";

    writeFile(tmpFile, TEST_INPUT_DOTPRODUCT);
    auto result = runClangTidy(tmpFile);

    SECTION("Portable fix suggestion") {
        REQUIRE(result.output.find("Portable fix: use ILP_FOR_AUTO with LoopType::DotProduct") != std::string::npos);
    }

    SECTION("Architecture-specific fix suggestion") {
        REQUIRE(result.output.find("Architecture-specific fix for skylake") != std::string::npos);
    }

    fs::remove(tmpFile);
}

// Test fix application with ILP_FOR macro
const char* TEST_FIX_INPUT = R"(#include "ilp_for.hpp"

void test_fix(const double* a, const double* b, std::size_t n) {
    double sum = 0.0;
    ILP_FOR(auto i, 0uz, n, 4) {
        sum += a[i] * b[i];
    } ILP_END;
}
)";

const char* TEST_FIX_EXPECTED = R"(#include "ilp_for.hpp"

void test_fix(const double* a, const double* b, std::size_t n) {
    double sum = 0.0;
    ILP_FOR_AUTO(auto i, 0uz, n, DotProduct) {
        sum += a[i] * b[i];
    } ILP_END;
}
)";

TEST_CASE("Auto-fix application", "[clang-tidy][autofix]") {
    std::string tmpFile = "/tmp/ilp_clang_tidy_fix_test.cpp";
    std::string ilpForDir = fs::current_path().parent_path().parent_path().string();

    // Write test input
    writeFile(tmpFile, TEST_FIX_INPUT);

    // Run clang-tidy with --fix
    static std::string clangTidyPath = getClangTidyPath();
    std::string cmd = clangTidyPath + " -load ";
    cmd += fs::current_path().string() + "/build/ILPTidyModule.so";
    cmd += " -checks='-*,ilp-*' --fix ";
    cmd += tmpFile;
    cmd += " -- -std=c++20 -I" + ilpForDir + " 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    REQUIRE(pipe != nullptr);

    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);

    // Check fix was applied
    REQUIRE(output.find("applied") != std::string::npos);

    // Verify the file was modified correctly
    std::string fixedContent = readFile(tmpFile);
    REQUIRE(fixedContent.find("ILP_FOR_AUTO(auto i, 0uz, n, DotProduct)") != std::string::npos);
    REQUIRE(fixedContent.find("ILP_FOR(auto i, 0uz, n, 4)") == std::string::npos);

    fs::remove(tmpFile);
}

// Test that already-fixed code is not re-detected
TEST_CASE("Already fixed code not detected", "[clang-tidy][idempotent]") {
    std::string tmpFile = "/tmp/ilp_clang_tidy_test.cpp";

    const char* alreadyFixed = R"(
#include "ilp_for.hpp"

void test_already_fixed(const double* a, const double* b, std::size_t n) {
    double sum = 0.0;
    ILP_FOR_AUTO(auto i, 0uz, n, DotProduct) {
        sum += a[i] * b[i];
    } ILP_END;
}
)";

    writeFile(tmpFile, alreadyFixed);
    auto result = runClangTidy(tmpFile);

    // ILP_FOR_AUTO uses for_loop_auto which should not be detected
    // (the check only matches for_loop<N>, not for_loop_auto<LoopType>)
    REQUIRE(result.output.find("ilp-loop-analysis") == std::string::npos);

    fs::remove(tmpFile);
}

// Test patterns that should NOT be misclassified
TEST_CASE("Pattern classification edge cases", "[clang-tidy][negative][classification]") {
    std::string tmpFile = "/tmp/ilp_negative_test.cpp";

    SECTION("Scaled sum should be Sum, not DotProduct") {
        // sum += data[i] * 2.0 is Sum with scaling, NOT DotProduct
        // DotProduct requires TWO indexed array accesses in the multiply
        const char* scaledSum = R"(
#include "ilp_for.hpp"

void test_scaled_sum(const double* data, std::size_t n) {
    double sum = 0.0;
    ILP_FOR(auto i, 0uz, n, 8) {
        sum += data[i] * 2.0;
    } ILP_END;
}
)";
        writeFile(tmpFile, scaledSum);
        auto result = runClangTidy(tmpFile);
        // Should detect as Sum, NOT DotProduct
        REQUIRE(result.output.find("Sum pattern") != std::string::npos);
        REQUIRE(result.output.find("DotProduct pattern") == std::string::npos);
        fs::remove(tmpFile);
    }

    SECTION("Sum with variable multiplier should be Sum, not DotProduct") {
        // sum += data[i] * factor is Sum with scaling, NOT DotProduct
        const char* variableScaledSum = R"(
#include "ilp_for.hpp"

void test_variable_scaled_sum(const double* data, double factor, std::size_t n) {
    double sum = 0.0;
    ILP_FOR(auto i, 0uz, n, 8) {
        sum += data[i] * factor;
    } ILP_END;
}
)";
        writeFile(tmpFile, variableScaledSum);
        auto result = runClangTidy(tmpFile);
        // Should detect as Sum, NOT DotProduct
        REQUIRE(result.output.find("Sum pattern") != std::string::npos);
        REQUIRE(result.output.find("DotProduct pattern") == std::string::npos);
        fs::remove(tmpFile);
    }
}

// Test loops with unrecognizable patterns (should not trigger)
TEST_CASE("Unrecognizable patterns not detected", "[clang-tidy][negative]") {
    std::string tmpFile = "/tmp/ilp_negative_test.cpp";

    SECTION("Loop with only side-effect function call") {
        // Just calling a function with no accumulation pattern
        const char* sideEffectOnly = R"(
#include "ilp_for.hpp"

void process(int x);

void test_side_effect(const int* data, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 4) {
        process(data[i]);
    } ILP_END;
}
)";
        writeFile(tmpFile, sideEffectOnly);
        auto result = runClangTidy(tmpFile);
        // Should not detect any pattern (Unknown type is skipped)
        REQUIRE(result.output.find("ilp-loop-analysis") == std::string::npos);
        fs::remove(tmpFile);
    }

    SECTION("Empty loop body") {
        const char* emptyLoop = R"(
#include "ilp_for.hpp"

void test_empty(std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 4) {
        // Empty body
    } ILP_END;
}
)";
        writeFile(tmpFile, emptyLoop);
        auto result = runClangTidy(tmpFile);
        // Should not detect any pattern
        REQUIRE(result.output.find("ilp-loop-analysis") == std::string::npos);
        fs::remove(tmpFile);
    }
}

// Test pointer arithmetic access patterns
TEST_CASE("Pointer arithmetic patterns", "[clang-tidy][detection][pointer]") {
    std::string tmpFile = "/tmp/ilp_pointer_test.cpp";

    SECTION("Pointer dereference with addition: *(arr + i)") {
        const char* ptrArith = R"(
#include "ilp_for.hpp"

void test_ptr_arith(const double* src, double* dst, std::size_t n) {
    ILP_FOR(auto i, 0uz, n, 4) {
        *(dst + i) = *(src + i);
    } ILP_END;
}
)";
        writeFile(tmpFile, ptrArith);
        auto result = runClangTidy(tmpFile);
        // Should detect Copy pattern
        REQUIRE(result.output.find("Copy pattern") != std::string::npos);
        fs::remove(tmpFile);
    }

    SECTION("Reversed subscript: i[arr] syntax") {
        // C/C++ allows i[arr] which is equivalent to arr[i]
        const char* reversedSubscript = R"(
#include "ilp_for.hpp"

void test_reversed_subscript(const int* data, std::size_t n) {
    int sum = 0;
    ILP_FOR(auto i, 0uz, n, 4) {
        sum += i[data];
    } ILP_END;
}
)";
        writeFile(tmpFile, reversedSubscript);
        auto result = runClangTidy(tmpFile);
        // Should detect Sum pattern (i[arr] is valid C++)
        REQUIRE(result.output.find("Sum pattern") != std::string::npos);
        fs::remove(tmpFile);
    }
}
