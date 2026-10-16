#include <gtest/gtest.h>
#include "include/perception/nimcp_visual_cortex.h"
#include <vector>
#include <cstring>
#include <cmath>
#include <iostream>

// Helper: Create test image with pattern
std::vector<uint8_t> create_test_image(uint32_t width, uint32_t height, const char* pattern) {
    std::vector<uint8_t> image(width * height);

    if (strcmp(pattern, "vertical_stripes") == 0) {
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = (x % 8 < 4) ? 255 : 0;
            }
        }
    } else if (strcmp(pattern, "checkerboard") == 0) {
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = ((x / 8 + y / 8) % 2 == 0) ? 255 : 0;
            }
        }
    }

    return image;
}

int main() {
    visual_cortex_config_t config;
    config.input_width = 64;
    config.input_height = 64;
    config.num_v1_filters = 8;
    config.feature_dim = 128;
    config.enable_attention = true;
    config.enable_memory = true;

    visual_cortex_t* cortex = visual_cortex_create(&config);

    // Create vertical stripes
    auto stripes = create_test_image(64, 64, "vertical_stripes");
    std::vector<float> stripes_features(128);
    visual_cortex_process(cortex, stripes.data(), 64, 64, 1, stripes_features.data());

    std::cout << "Vertical stripes features (first 10): ";
    for (int i = 0; i < 10; i++) {
        std::cout << stripes_features[i] << " ";
    }
    std::cout << std::endl;

    // Store in memory
    visual_cortex_store_memory(cortex, stripes_features.data(), 0.8f);

    // Create checkerboard
    auto checker = create_test_image(64, 64, "checkerboard");
    std::vector<float> checker_features(128);
    visual_cortex_process(cortex, checker.data(), 64, 64, 1, checker_features.data());

    std::cout << "Checkerboard features (first 10): ";
    for (int i = 0; i < 10; i++) {
        std::cout << checker_features[i] << " ";
    }
    std::cout << std::endl;

    // Compute cosine similarity manually
    float dot = 0.0f;
    for (int i = 0; i < 128; i++) {
        dot += stripes_features[i] * checker_features[i];
    }
    std::cout << "Cosine similarity: " << dot << std::endl;
    std::cout << "Novelty (1 - similarity): " << (1.0f - dot) << std::endl;

    float novelty = visual_cortex_compute_novelty(cortex, checker_features.data());
    std::cout << "Computed novelty: " << novelty << std::endl;

    visual_cortex_destroy(cortex);
    return 0;
}
