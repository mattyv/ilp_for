#pragma once

#include <optional>
#include <utility>

namespace ilp {

template<typename R = void>
struct LoopCtrl {
    bool ok = true;
    std::optional<R> return_value;

    void break_loop() { ok = false; }
    void return_with(R val) {
        ok = false;
        return_value = std::move(val);
    }
};

template<>
struct LoopCtrl<void> {
    bool ok = true;
    void break_loop() { ok = false; }
};

} // namespace ilp
