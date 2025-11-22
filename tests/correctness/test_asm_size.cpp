#include "catch.hpp"
#include <fstream>
#include <string>
#include <cctype>

// Count instruction lines in assembly file
static int count_instructions(const std::string& path) {
    std::ifstream file(path);
    if (!file) return -1;

    int count = 0;
    std::string line;
    while (std::getline(file, line)) {
        // Lines starting with whitespace followed by lowercase letter are instructions
        size_t i = 0;
        while (i < line.size() && std::isspace(line[i])) i++;
        if (i < line.size() && std::islower(line[i])) {
            count++;
        }
    }
    return count;
}

TEST_CASE("Assembly instruction count comparison", "[asm]") {
    // Max overhead for simple functions
    const int MAX_OVERHEAD = 10;
    // Higher tolerance for SIMD-vectorized functions (setup/teardown complexity)
    const int MAX_OVERHEAD_SIMD = 50;

    auto check = [&](const std::string& name, int max_overhead) {
        std::string hand_path = "asm_compare/handrolled/" + name + ".s";
        std::string ilp_path = "asm_compare/ilp/" + name + ".s";

        int hand = count_instructions(hand_path);
        int ilp = count_instructions(ilp_path);

        // Skip if assembly files don't exist (run 'make asm' first)
        if (hand == -1 || ilp == -1) {
            WARN("Skipping " << name << " - run 'make asm' first");
            return;
        }

        INFO("Function: " << name);
        INFO("Handrolled: " << hand << " instructions");
        INFO("ILP: " << ilp << " instructions");
        INFO("Difference: " << (ilp - hand));

        // ILP should not exceed handrolled by more than max_overhead
        CHECK(ilp - hand <= max_overhead);
    };

    SECTION("sum_plain") { check("sum_plain", MAX_OVERHEAD); }
    SECTION("sum_with_break") { check("sum_with_break", MAX_OVERHEAD); }
    SECTION("find_value") { check("find_value", MAX_OVERHEAD); }
    SECTION("find_value_no_ctrl") { check("find_value_no_ctrl", MAX_OVERHEAD); }
    SECTION("sum_range") { check("sum_range", MAX_OVERHEAD_SIMD); }
    SECTION("sum_odd") { check("sum_odd", MAX_OVERHEAD); }
    SECTION("sum_step2") { check("sum_step2", MAX_OVERHEAD); }
    SECTION("sum_negative") { check("sum_negative", MAX_OVERHEAD); }
    SECTION("sum_backward") { check("sum_backward", MAX_OVERHEAD); }
}
