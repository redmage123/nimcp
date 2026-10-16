/**
 * @file test_visual_cortex_integration.cpp
 * @brief Integration tests for visual cortex with other brain systems
 *
 * WHAT: Test visual cortex integration with curiosity, attention, memory, etc.
 * WHY:  Ensure visual cortex coordinates properly with other brain regions
 * HOW:  Multi-system integration scenarios
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 2.6
 */

#include <gtest/gtest.h>
#include "perception/nimcp_visual_cortex.h"
#include "utils/memory/nimcp_memory.h"
#include <vector>
#include <cstring>
#include <cmath>

// TODO: Add these includes when the systems are implemented
// #include "nimcp_curiosity.h"
// #include "nimcp_attention.h"
// #include "nimcp_knowledge.h"
// #include "nimcp_salience.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class VisualCortexIntegrationTest : public ::testing::Test {
protected:
    visual_cortex_t* cortex;
    visual_cortex_config_t config;

    void SetUp() override {
        // Initialize all fields to avoid undefined behavior
        memset(&config, 0, sizeof(config));
        config.input_width = 64;
        config.input_height = 64;
        config.num_v1_filters = 8;
        config.feature_dim = 128;
        config.enable_attention = true;
        config.enable_memory = true;
        config.enable_fractal_topology = false;
        config.hub_ratio = 0.15f;
        config.power_law_gamma = -2.1f;
        config.internal_neurons = 0;

        cortex = visual_cortex_create(&config);
        ASSERT_NE(cortex, nullptr);
    }

    void TearDown() override {
        if (cortex) {
            visual_cortex_destroy(cortex);
        }
    }

    // Helper: Create test image with pattern
    std::vector<uint8_t> create_test_image(uint32_t width, uint32_t height, const char* pattern) {
        std::vector<uint8_t> image(width * height);

        if (strcmp(pattern, "vertical_stripes") == 0) {
            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    image[y * width + x] = (x % 8 < 4) ? 255 : 0;
                }
            }
        } else if (strcmp(pattern, "horizontal_stripes") == 0) {
            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    image[y * width + x] = (y % 8 < 4) ? 255 : 0;
                }
            }
        } else if (strcmp(pattern, "checkerboard") == 0) {
            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    image[y * width + x] = ((x / 8 + y / 8) % 2 == 0) ? 255 : 0;
                }
            }
        } else if (strcmp(pattern, "gradient") == 0) {
            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    image[y * width + x] = (x * 255) / width;
                }
            }
        } else {  // uniform
            std::fill(image.begin(), image.end(), 128);
        }

        return image;
    }

    // Helper: Extract visual features
    std::vector<float> extract_features(const std::vector<uint8_t>& image) {
        std::vector<float> features(config.feature_dim);
        bool success = visual_cortex_process(
            cortex,
            image.data(),
            config.input_width,
            config.input_height,
            1,  // grayscale
            features.data()
        );
        EXPECT_TRUE(success);
        return features;
    }
};

//=============================================================================
// Test 1: Visual Cortex → Curiosity System Integration
//=============================================================================

TEST_F(VisualCortexIntegrationTest, CuriosityDrivenNoveltyDetection) {
    // SCENARIO: Robot explores environment, visual cortex detects novel objects
    // EXPECTED: High novelty for first-time objects, low novelty for familiar

    // 1. First image: vertical stripes (novel)
    auto stripes = create_test_image(64, 64, "vertical_stripes");
    auto stripes_features = extract_features(stripes);

    float novelty1 = visual_cortex_compute_novelty(cortex, stripes_features.data());
    EXPECT_GT(novelty1, 0.9f);  // Very novel (no memories yet)

    // 2. Store in memory
    visual_cortex_store_memory(cortex, stripes_features.data(), 0.8f);

    // 3. Same pattern again (familiar)
    auto stripes2 = create_test_image(64, 64, "vertical_stripes");
    auto stripes2_features = extract_features(stripes2);

    float novelty2 = visual_cortex_compute_novelty(cortex, stripes2_features.data());
    EXPECT_LT(novelty2, 0.3f);  // Low novelty (already in memory)

    // 4. Completely different pattern (novel)
    auto checkerboard = create_test_image(64, 64, "checkerboard");
    auto checker_features = extract_features(checkerboard);

    float novelty3 = visual_cortex_compute_novelty(cortex, checker_features.data());
    EXPECT_GT(novelty3, 0.05f);  // Moderate novelty (different from stored pattern)

    // Integration: Curiosity system would use novelty score to prioritize exploration
    // High novelty → explore more, Low novelty → move on
}

//=============================================================================
// Test 2: Visual Cortex → Attention System Integration
//=============================================================================

TEST_F(VisualCortexIntegrationTest, AttentionDrivenVisualFocus) {
    // SCENARIO: Visual attention highlights salient regions
    // EXPECTED: Attention peaks at high-contrast edges

    // Create image with single bright spot
    std::vector<uint8_t> image(64 * 64, 50);  // dim background
    // Add bright spot at (32, 32)
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            int x = 32 + dx;
            int y = 32 + dy;
            if (x >= 0 && x < 64 && y >= 0 && y < 64) {
                image[y * 64 + x] = 255;
            }
        }
    }

    // Compute attention map
    attention_map_t* attn_map = attention_map_create(64, 64);
    ASSERT_NE(attn_map, nullptr);

    bool success = visual_cortex_compute_attention(cortex, image.data(), 64, 64, attn_map);
    EXPECT_TRUE(success);

    // Find attention peak
    uint32_t max_x, max_y;
    float max_value;
    success = visual_cortex_get_attention_peak(attn_map, &max_x, &max_y, &max_value);
    EXPECT_TRUE(success);

    // Peak should be near the bright spot
    EXPECT_NEAR(max_x, 32, 5);  // Within 5 pixels
    EXPECT_NEAR(max_y, 32, 5);
    EXPECT_GT(max_value, 0.0f);  // Non-zero attention

    attention_map_destroy(attn_map);

    // Integration: Attention system uses peak location to:
    // - Guide eye movements (saccades)
    // - Prioritize processing resources
    // - Select relevant information for working memory
}

//=============================================================================
// Test 3: Visual Cortex → Memory System Integration
//=============================================================================

TEST_F(VisualCortexIntegrationTest, MemoryConsolidationPipeline) {
    // SCENARIO: Visual experiences consolidated into long-term memory
    // EXPECTED: Visual memories stored and retrievable

    // 1. Process diverse visual experiences
    auto stripes = create_test_image(64, 64, "vertical_stripes");
    auto checker = create_test_image(64, 64, "checkerboard");
    auto gradient = create_test_image(64, 64, "gradient");

    auto f1 = extract_features(stripes);
    auto f2 = extract_features(checker);
    auto f3 = extract_features(gradient);

    // 2. Consolidate memories
    bool success;
    success = visual_cortex_consolidate_memory(cortex, f1.data(), 0.9f, "vertical_pattern");
    EXPECT_TRUE(success);

    success = visual_cortex_consolidate_memory(cortex, f2.data(), 0.8f, "checkerboard_pattern");
    EXPECT_TRUE(success);

    success = visual_cortex_consolidate_memory(cortex, f3.data(), 0.7f, "gradient_pattern");
    EXPECT_TRUE(success);

    // 3. Verify memories stored
    visual_cortex_stats_t stats;
    visual_cortex_get_stats(cortex, &stats);
    EXPECT_EQ(stats.memories_stored, 3);

    // 4. Recall similar memory
    visual_memory_t** recalled = nullptr;
    int num_recalled = 0;

    success = visual_cortex_recall_memory(cortex, f1.data(), 1, &recalled, &num_recalled);
    EXPECT_TRUE(success);
    EXPECT_EQ(num_recalled, 1);
    EXPECT_NE(recalled, nullptr);

    if (recalled) {
        // Recalled memory should be most similar to query
        EXPECT_NEAR(recalled[0]->salience, 0.9f, 0.1f);
        nimcp_free(recalled);
    }

    // Integration: Memory consolidation flow:
    // Visual Cortex → Hippocampus (episodic storage)
    //              → Knowledge Graph (semantic integration)
    //              → Sleep consolidation (synaptic scaling)
}

//=============================================================================
// Test 4: Visual Cortex → Salience System Integration
//=============================================================================

TEST_F(VisualCortexIntegrationTest, SalienceBasedPrioritization) {
    // SCENARIO: Salience system prioritizes important visual information
    // EXPECTED: High-salience images stored with priority

    // Create images with varying salience
    auto boring = create_test_image(64, 64, "uniform");     // Low salience
    auto interesting = create_test_image(64, 64, "checkerboard");  // High salience

    auto f_boring = extract_features(boring);
    auto f_interesting = extract_features(interesting);

    // Store with salience values
    visual_cortex_consolidate_memory(cortex, f_boring.data(), 0.2f, "boring");
    visual_cortex_consolidate_memory(cortex, f_interesting.data(), 0.9f, "interesting");

    // Retrieve top-salience memory
    visual_memory_t** recalled = nullptr;
    int num_recalled = 0;

    // Recall based on interesting features (high salience should rank higher)
    visual_cortex_recall_memory(cortex, f_interesting.data(), 2, &recalled, &num_recalled);
    EXPECT_EQ(num_recalled, 2);

    if (recalled && num_recalled > 0) {
        // Top result should be high-salience memory
        EXPECT_GT(recalled[0]->salience, 0.5f);
        nimcp_free(recalled);
    }

    // Integration: Salience system uses visual features to:
    // - Prioritize attention allocation
    // - Determine memory consolidation strength
    // - Guide learning resource allocation
}

//=============================================================================
// Test 5: Multi-System Coordination
//=============================================================================

TEST_F(VisualCortexIntegrationTest, MultiSystemCoordination) {
    // SCENARIO: Complex task requiring all systems working together
    // EXAMPLE: Robot exploring novel environment
    //
    // Visual Cortex → extracts features
    //              → Curiosity (detect novelty)
    //              → Attention (focus on salient regions)
    //              → Memory (consolidate experiences)
    //              → Salience (prioritize important info)

    // 1. Exploration phase: see many different objects
    std::vector<std::vector<uint8_t>> scenes = {
        create_test_image(64, 64, "vertical_stripes"),
        create_test_image(64, 64, "horizontal_stripes"),
        create_test_image(64, 64, "checkerboard"),
        create_test_image(64, 64, "gradient")
    };

    std::vector<float> novelty_scores;
    std::vector<std::vector<float>> all_features;

    for (size_t i = 0; i < scenes.size(); i++) {
        // Extract features
        auto features = extract_features(scenes[i]);
        all_features.push_back(features);

        // Compute novelty (curiosity system)
        float novelty = visual_cortex_compute_novelty(cortex, features.data());
        novelty_scores.push_back(novelty);

        // Compute salience (combines novelty + visual complexity)
        float salience = novelty * 0.7f + 0.3f;  // Simple salience model

        // Consolidate memory (only high-salience items)
        if (salience > 0.5f) {
            visual_cortex_consolidate_memory(cortex, features.data(), salience, "scene");
        }
    }

    // 2. Verification: first scene should be most novel
    EXPECT_GT(novelty_scores[0], 0.9f);  // No prior memories

    // 3. Verification: later scenes less novel (familiarity increases)
    EXPECT_LT(novelty_scores[3], novelty_scores[0]);

    // 4. Verification: memories stored
    visual_cortex_stats_t stats;
    visual_cortex_get_stats(cortex, &stats);
    EXPECT_GT(stats.memories_stored, 0);

    // 5. Recall test: query with first scene
    visual_memory_t** recalled = nullptr;
    int num_recalled = 0;
    visual_cortex_recall_memory(cortex, all_features[0].data(), 1, &recalled, &num_recalled);

    EXPECT_GT(num_recalled, 0);
    if (recalled) {
        nimcp_free(recalled);
    }
}

//=============================================================================
// Test 6: Brain Integration Helper Functions
//=============================================================================

TEST_F(VisualCortexIntegrationTest, BrainIntegrationHelpers) {
    // Test novelty computation
    auto pattern = create_test_image(64, 64, "vertical_stripes");
    auto features = extract_features(pattern);

    // No memories → max novelty
    float novelty = visual_cortex_compute_novelty(cortex, features.data());
    EXPECT_FLOAT_EQ(novelty, 1.0f);

    // Add memory
    visual_cortex_store_memory(cortex, features.data(), 0.8f);

    // Same pattern → low novelty
    novelty = visual_cortex_compute_novelty(cortex, features.data());
    EXPECT_LT(novelty, 0.3f);

    // Test attention peak extraction
    attention_map_t* map = attention_map_create(64, 64);
    ASSERT_NE(map, nullptr);

    // Set a peak
    attention_map_set(map, 30, 40, 0.9f);
    attention_map_set(map, 10, 10, 0.3f);

    uint32_t max_x, max_y;
    float max_val;
    bool success = visual_cortex_get_attention_peak(map, &max_x, &max_y, &max_val);

    EXPECT_TRUE(success);
    EXPECT_EQ(max_x, 30);
    EXPECT_EQ(max_y, 40);
    EXPECT_FLOAT_EQ(max_val, 0.9f);

    attention_map_destroy(map);

    // Test memory consolidation
    auto new_pattern = create_test_image(64, 64, "checkerboard");
    auto new_features = extract_features(new_pattern);

    success = visual_cortex_consolidate_memory(cortex, new_features.data(), 0.85f, "test_context");
    EXPECT_TRUE(success);

    visual_cortex_stats_t stats;
    visual_cortex_get_stats(cortex, &stats);
    EXPECT_EQ(stats.memories_stored, 2);  // First memory + this one
}

//=============================================================================
// Test 7: Null Pointer Safety
//=============================================================================

TEST_F(VisualCortexIntegrationTest, NullPointerSafety) {
    // Test novelty computation with nulls
    EXPECT_FLOAT_EQ(visual_cortex_compute_novelty(nullptr, nullptr), 0.0f);

    auto pattern = create_test_image(64, 64, "vertical_stripes");
    auto features = extract_features(pattern);

    EXPECT_FLOAT_EQ(visual_cortex_compute_novelty(nullptr, features.data()), 0.0f);
    EXPECT_FLOAT_EQ(visual_cortex_compute_novelty(cortex, nullptr), 0.0f);

    // Test attention peak with nulls
    uint32_t x, y;
    float val;
    EXPECT_FALSE(visual_cortex_get_attention_peak(nullptr, &x, &y, &val));

    attention_map_t* map = attention_map_create(64, 64);
    EXPECT_FALSE(visual_cortex_get_attention_peak(map, nullptr, &y, &val));
    EXPECT_FALSE(visual_cortex_get_attention_peak(map, &x, nullptr, &val));
    EXPECT_FALSE(visual_cortex_get_attention_peak(map, &x, &y, nullptr));
    attention_map_destroy(map);

    // Test consolidation with nulls
    EXPECT_FALSE(visual_cortex_consolidate_memory(nullptr, features.data(), 0.5f, "test"));
    EXPECT_FALSE(visual_cortex_consolidate_memory(cortex, nullptr, 0.5f, "test"));
}

// Note: main() provided by GTest::Main from CMakeLists.txt
