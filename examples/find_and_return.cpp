// ILP_RETURN

#include "../ilp_for.hpp"
#include <iostream>
#include <vector>

int find_index(const std::vector<int>& data, int target) {
    ILP_FOR(auto i, 0, static_cast<int>(data.size()), 4) {
        if (data[i] == target)
            ILP_RETURN(i);
    }
    ILP_END_RETURN;
    return -1;
}

int find_first_above_threshold(const std::vector<int>& data, int threshold) {
    ILP_FOR(auto i, 0, static_cast<int>(data.size()), 4) {
        if (data[i] > threshold)
            ILP_RETURN(data[i]);
    }
    ILP_END_RETURN;
    return -1;
}

struct Point {
    int x, y;
};

Point find_point_in_quadrant(const std::vector<Point>& points, int quadrant) {
    ILP_FOR(auto i, 0, static_cast<int>(points.size()), 4) {
        bool match = false;
        switch (quadrant) {
        case 1:
            match = points[i].x > 0 && points[i].y > 0;
            break;
        case 2:
            match = points[i].x < 0 && points[i].y > 0;
            break;
        case 3:
            match = points[i].x < 0 && points[i].y < 0;
            break;
        case 4:
            match = points[i].x > 0 && points[i].y < 0;
            break;
        }
        if (match)
            ILP_RETURN(points[i]);
    }
    ILP_END_RETURN;
    return Point{0, 0};
}

int main() {
    std::vector<int> data = {10, 20, 30, 40, 50};

    int idx = find_index(data, 30);
    std::cout << "Index of 30: " << idx << "\n";

    int val = find_first_above_threshold(data, 35);
    std::cout << "First above 35: " << val << "\n";

    std::vector<Point> points = {{-1, 2}, {3, -4}, {5, 6}, {-7, -8}};
    auto p = find_point_in_quadrant(points, 1);
    std::cout << "Point in Q1: (" << p.x << ", " << p.y << ")\n";
}
