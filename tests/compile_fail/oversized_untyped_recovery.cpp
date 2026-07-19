// COMPILE_FAIL: Recovered type exceeds SBO size
#include "../../ilp_for.hpp"

struct Oversized {
    unsigned char bytes[ilp::arch::sbo_size + 1];
};

int main() {
    ilp::SmallStorage storage;
    storage.set(1);
    (void)storage.extract<Oversized>();
}
