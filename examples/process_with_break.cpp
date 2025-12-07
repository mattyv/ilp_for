// ILP_BREAK and ILP_CONTINUE

#include "../ilp_for.hpp"
#include <iostream>
#include <vector>

void process_until_error(const std::vector<int>& data) {
    int sum = 0;
    int count = 0;

    ILP_FOR(auto i, 0uz, data.size(), 4) {
        if (data[i] < 0) {
            std::cout << "Error at index " << i << ", stopping\n";
            ILP_BREAK;
        }
        if (data[i] == 0) {
            ILP_CONTINUE;
        }
        sum += data[i];
        ++count;
    }
    ILP_END;

    std::cout << "Processed " << count << " items, sum = " << sum << "\n";
}

bool validate_entries(const std::vector<int>& data, int min_val, int max_val) {
    bool valid = true;

    ILP_FOR(auto i, 0uz, data.size(), 4) {
        if (data[i] < min_val || data[i] > max_val) {
            std::cout << "Invalid value " << data[i] << " at index " << i << "\n";
            valid = false;
            ILP_BREAK;
        }
    }
    ILP_END;

    return valid;
}

int main() {
    std::vector<int> data1 = {5, 0, 10, 0, 15, -1, 20, 25};
    process_until_error(data1);

    std::cout << "\n";

    std::vector<int> data2 = {1, 5, 3, 8, 2, 9};
    if (validate_entries(data2, 0, 10)) {
        std::cout << "All entries valid\n";
    }

    std::vector<int> data3 = {1, 5, 15, 8};
    validate_entries(data3, 0, 10);
}
