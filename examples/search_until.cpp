// Example: Search with ILP_FOR_UNTIL_RANGE_AUTO
// Demonstrates the recommended pattern for early-exit search

#include "../ilp_for.hpp"
#include <vector>
#include <string>
#include <iostream>

struct Record {
    int id;
    std::string name;
    bool active;
};

// Find first active record
std::optional<size_t> find_first_active(const std::vector<Record>& records) {
    return ILP_FOR_UNTIL_RANGE_AUTO(rec, records) {
        return rec.active;
    } ILP_END_UNTIL;
}

// Find record by name
std::optional<size_t> find_by_name(const std::vector<Record>& records,
                                    const std::string& target) {
    return ILP_FOR_UNTIL_RANGE_AUTO(rec, records) {
        return rec.name == target;
    } ILP_END_UNTIL;
}

int main() {
    std::vector<Record> records = {
        {1, "Alice", false},
        {2, "Bob", false},
        {3, "Charlie", true},
        {4, "Diana", false},
        {5, "Eve", true}
    };

    if (auto idx = find_first_active(records)) {
        std::cout << "First active: " << records[*idx].name
                  << " at index " << *idx << "\n";
    }

    if (auto idx = find_by_name(records, "Diana")) {
        std::cout << "Found Diana at index " << *idx << "\n";
    }

    if (auto idx = find_by_name(records, "Frank")) {
        std::cout << "Found Frank at index " << *idx << "\n";
    } else {
        std::cout << "Frank not found\n";
    }
}
