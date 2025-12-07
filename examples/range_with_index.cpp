// Example: Range iteration with index tracking
// Demonstrates ilp::find_range_idx

#include "../ilp_for.hpp"
#include <iostream>
#include <string>
#include <vector>

struct Item {
    std::string name;
    int priority;
};

// Find item by name, return iterator
auto find_item(std::vector<Item>& items, const std::string& target) {
    return ilp::find_range_idx<4>(items, [&](auto&& item, auto idx, auto end) {
        if (item.name == target)
            return std::ranges::begin(items) + idx;
        return end;
    });
}

// Find highest priority item
auto find_highest_priority(std::vector<Item>& items) {
    return ilp::find_range_idx<4>(items, [&](auto&& item, auto idx, auto end) {
        if (item.priority >= 10)
            return std::ranges::begin(items) + idx;
        return end;
    });
}

// Find first item matching predicate, also get its index
void find_and_report(const std::vector<Item>& items, int min_priority) {
    size_t found_idx = items.size();

    auto it = ilp::find_range_idx<4>(items, [&](auto&& item, auto idx, auto end) {
        if (item.priority >= min_priority) {
            found_idx = idx;
            return std::ranges::begin(items) + idx;
        }
        return end;
    });

    if (it != items.end()) {
        std::cout << "Found '" << it->name << "' at index " << found_idx << " with priority " << it->priority << "\n";
    } else {
        std::cout << "No item with priority >= " << min_priority << "\n";
    }
}

int main() {
    std::vector<Item> items = {{"task-a", 3}, {"task-b", 7}, {"task-c", 12}, {"task-d", 5}, {"task-e", 15}};

    // Find by name
    if (auto it = find_item(items, "task-c"); it != items.end()) {
        std::cout << "Found: " << it->name << " (priority " << it->priority << ")\n";
    }

    // Find high priority
    if (auto it = find_highest_priority(items); it != items.end()) {
        std::cout << "High priority: " << it->name << "\n";
    }

    // Find with index reporting
    find_and_report(items, 10);
    find_and_report(items, 20);
}
