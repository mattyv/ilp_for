// COMPILE_FAIL: SmallStorage only supports trivially-copyable types
#include "../../ilp_for.hpp"

struct SelfAware {
    SelfAware* self = this;

    SelfAware() = default;
    SelfAware(const SelfAware&) = delete;
    SelfAware(SelfAware&&) noexcept : self(this) {}
};

int main() {
    auto result = ilp::for_loop<4>(0, 1, [](int, ilp::ForCtrl& ctrl) {
        ctrl.return_with(SelfAware{});
    });
    (void)result;
}
