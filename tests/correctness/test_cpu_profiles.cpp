#include "../../ilp_for/cpu_profiles/ilp_cpu_profiles.hpp"
#include "catch.hpp"

TEST_CASE("cpu::get returns correct profiles for known names") {
    SECTION("Apple M1 variants") {
        CHECK(&ilp::cpu::get("apple_m1") == &ilp::cpu::apple_m1);
        CHECK(&ilp::cpu::get("m1") == &ilp::cpu::apple_m1);
    }
    SECTION("Alderlake variants") {
        CHECK(&ilp::cpu::get("alderlake") == &ilp::cpu::alderlake);
        CHECK(&ilp::cpu::get("alder_lake") == &ilp::cpu::alderlake);
    }
    SECTION("Zen variants") {
        CHECK(&ilp::cpu::get("zen5") == &ilp::cpu::zen5);
        CHECK(&ilp::cpu::get("zen4") == &ilp::cpu::zen5);
        CHECK(&ilp::cpu::get("zen") == &ilp::cpu::zen5);
    }
    SECTION("Default profile") {
        CHECK(&ilp::cpu::get("default") == &ilp::cpu::default_profile);
    }
}

TEST_CASE("cpu::get falls back to skylake for unknown names") {
    CHECK(&ilp::cpu::get("unknown") == &ilp::cpu::skylake);
    CHECK(&ilp::cpu::get("") == &ilp::cpu::skylake);
    CHECK(&ilp::cpu::get("skylake") == &ilp::cpu::skylake);
}
