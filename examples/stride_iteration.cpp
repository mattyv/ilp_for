// Example: Non-unit stride iteration
// Demonstrates ILP_FOR_STEP for processing every Nth element

#include "../ilp_for.hpp"
#include <vector>
#include <iostream>
#include <cstdint>

// Process interleaved stereo audio (left channel only)
void process_left_channel(std::vector<int16_t>& stereo_data) {
    ILP_FOR_STEP(auto i, 0uz, stereo_data.size(), 2uz, 4) {
        stereo_data[i] = static_cast<int16_t>(stereo_data[i] * 0.8);  // Reduce volume
    } ILP_END;
}

// Sum every 4th element (e.g., RGBA alpha channel)
int sum_alpha_channel(const std::vector<uint8_t>& rgba_data) {
    int sum = 0;
    ILP_FOR_STEP(auto i, 3uz, rgba_data.size(), 4uz, 4) {  // Start at index 3 (alpha)
        sum += rgba_data[i];
    } ILP_END;
    return sum;
}

// Downsample by factor of N
std::vector<int> downsample(const std::vector<int>& data, size_t factor) {
    std::vector<int> result;
    result.reserve(data.size() / factor + 1);

    ILP_FOR_STEP(auto i, 0uz, data.size(), factor, 4) {
        result.push_back(data[i]);
    } ILP_END;

    return result;
}

// Process matrix diagonal (stride = width + 1)
void scale_diagonal(std::vector<double>& matrix, size_t width, double scale) {
    size_t stride = width + 1;
    size_t max_idx = width * width;

    ILP_FOR_STEP(auto i, 0uz, max_idx, stride, 4) {
        matrix[i] *= scale;
    } ILP_END;
}

int main() {
    // Stereo audio processing
    std::vector<int16_t> audio = {100, 50, 200, 75, 150, 60, 180, 90};
    std::cout << "Before: L=" << audio[0] << " R=" << audio[1] << "\n";
    process_left_channel(audio);
    std::cout << "After:  L=" << audio[0] << " R=" << audio[1] << "\n\n";

    // RGBA alpha sum
    std::vector<uint8_t> rgba = {
        255, 0, 0, 128,    // Red, 50% alpha
        0, 255, 0, 255,    // Green, full alpha
        0, 0, 255, 64      // Blue, 25% alpha
    };
    std::cout << "Alpha sum: " << sum_alpha_channel(rgba) << "\n\n";

    // Downsampling
    std::vector<int> signal = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    auto downsampled = downsample(signal, 3);
    std::cout << "Downsampled (3x): ";
    for (int v : downsampled) std::cout << v << " ";
    std::cout << "\n\n";

    // Matrix diagonal
    std::vector<double> mat = {
        1, 0, 0,
        0, 2, 0,
        0, 0, 3
    };
    scale_diagonal(mat, 3, 10.0);
    std::cout << "Scaled diagonal: " << mat[0] << ", " << mat[4] << ", " << mat[8] << "\n";
}
