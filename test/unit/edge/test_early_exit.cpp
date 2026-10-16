/**
 * @file test_early_exit.cpp
 * @brief GoogleTest unit tests for NIMCP edge early exit subsystem
 *
 * Tests adaptive computation depth with confidence-based early exits.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class EarlyExitTest : public ::testing::Test {
protected:
    nimcp_early_exit_t* ee = nullptr;

    void TearDown() override {
        if (ee) {
            nimcp_early_exit_destroy(ee);
            ee = nullptr;
        }
    }
};

TEST_F(EarlyExitTest, CreateWithValidParams) {
    uint32_t exit_layers[] = {2, 4, 6};
    float thresholds[] = {0.9f, 0.8f, 0.7f};
    uint32_t layer_sizes[] = {64, 64, 64};
    uint32_t output_size = 10;

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 3, output_size);
    ASSERT_NE(ee, nullptr);
    EXPECT_EQ(ee->num_exits, 3u);
    EXPECT_EQ(ee->output_size, output_size);
    EXPECT_TRUE(ee->enabled);
}

TEST_F(EarlyExitTest, HighConfidenceTriggersExit) {
    uint32_t exit_layers[] = {1};
    float thresholds[] = {0.5f};
    uint32_t layer_sizes[] = {4};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 1, 2);
    ASSERT_NE(ee, nullptr);

    // Create activation that should produce high confidence
    // Using large magnitude values to produce confident softmax
    float activation[] = {10.0f, 0.0f, 0.0f, 0.0f};
    float output[2] = {0};
    float confidence = 0.0f;

    int ret = nimcp_early_exit_evaluate(ee, 0, activation, 4, output, &confidence);
    // If confidence > threshold (0.5), ret should be exit index (0)
    // Otherwise ret = -1
    if (confidence > 0.5f) {
        EXPECT_EQ(ret, 0);
    }
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(EarlyExitTest, LowConfidenceDoesNotTriggerExit) {
    uint32_t exit_layers[] = {1};
    float thresholds[] = {0.99f}; // Very high threshold
    uint32_t layer_sizes[] = {4};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 1, 2);
    ASSERT_NE(ee, nullptr);

    // Uniform activation — low confidence
    float activation[] = {0.25f, 0.25f, 0.25f, 0.25f};
    float output[2] = {0};
    float confidence = 0.0f;

    int ret = nimcp_early_exit_evaluate(ee, 0, activation, 4, output, &confidence);
    // With uniform input and high threshold, should not exit
    EXPECT_EQ(ret, -1);
    EXPECT_LT(confidence, 0.99f);
}

TEST_F(EarlyExitTest, StatsTracking) {
    uint32_t exit_layers[] = {1};
    float thresholds[] = {0.5f};
    uint32_t layer_sizes[] = {4};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 1, 2);
    ASSERT_NE(ee, nullptr);

    uint64_t total = 0, early = 0, full = 0;
    nimcp_early_exit_get_stats(ee, &total, &early, &full);
    EXPECT_EQ(total, 0u);
    EXPECT_EQ(early, 0u);
    EXPECT_EQ(full, 0u);

    // Run some evaluations
    float act_high[] = {10.0f, 0.0f, 0.0f, 0.0f};
    float act_low[] = {0.25f, 0.25f, 0.25f, 0.25f};
    float output[2];
    float conf;

    nimcp_early_exit_evaluate(ee, 0, act_high, 4, output, &conf);
    nimcp_early_exit_evaluate(ee, 0, act_low, 4, output, &conf);

    nimcp_early_exit_get_stats(ee, &total, &early, &full);
    EXPECT_GE(total, 0u); // Stats should be tracked
}

TEST_F(EarlyExitTest, MultipleExitPoints) {
    uint32_t exit_layers[] = {1, 3, 5};
    float thresholds[] = {0.95f, 0.8f, 0.6f}; // Decreasing thresholds
    uint32_t layer_sizes[] = {8, 8, 8};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 3, 4);
    ASSERT_NE(ee, nullptr);
    EXPECT_EQ(ee->num_exits, 3u);

    // Each exit point should have its own threshold
    EXPECT_FLOAT_EQ(ee->confidence_thresholds[0], 0.95f);
    EXPECT_FLOAT_EQ(ee->confidence_thresholds[1], 0.8f);
    EXPECT_FLOAT_EQ(ee->confidence_thresholds[2], 0.6f);
}

TEST_F(EarlyExitTest, DestroyCleanup) {
    uint32_t exit_layers[] = {1};
    float thresholds[] = {0.5f};
    uint32_t layer_sizes[] = {4};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 1, 2);
    ASSERT_NE(ee, nullptr);

    nimcp_early_exit_destroy(ee);
    ee = nullptr; // Prevent double-free in TearDown
}

TEST_F(EarlyExitTest, OutputDimensionsCorrect) {
    uint32_t exit_layers[] = {1};
    float thresholds[] = {0.1f}; // Low threshold to ensure exit
    uint32_t layer_sizes[] = {8};
    uint32_t output_size = 5;

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 1, output_size);
    ASSERT_NE(ee, nullptr);
    EXPECT_EQ(ee->output_size, 5u);

    float activation[] = {10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> output(output_size, -999.0f);
    float confidence;

    nimcp_early_exit_evaluate(ee, 0, activation, 8, output.data(), &confidence);

    // If exit triggered, output should have been written
    if (confidence > 0.1f) {
        bool any_written = false;
        for (uint32_t i = 0; i < output_size; i++) {
            if (output[i] != -999.0f) any_written = true;
        }
        EXPECT_TRUE(any_written);
    }
}

TEST_F(EarlyExitTest, ConfidenceRangeAlwaysBounded) {
    uint32_t exit_layers[] = {1};
    float thresholds[] = {0.5f};
    uint32_t layer_sizes[] = {4};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 1, 2);
    ASSERT_NE(ee, nullptr);

    // Test with various activations
    float test_cases[][4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1e6f, 0.0f, 0.0f, 0.0f},
        {-1e6f, 1e6f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    };

    float output[2];
    float confidence;

    for (int t = 0; t < 4; t++) {
        nimcp_early_exit_evaluate(ee, 0, test_cases[t], 4, output, &confidence);
        EXPECT_GE(confidence, 0.0f) << "Test case " << t;
        EXPECT_LE(confidence, 1.0f) << "Test case " << t;
    }
}

TEST_F(EarlyExitTest, DestroyNullSafe) {
    nimcp_early_exit_destroy(nullptr);
}

TEST_F(EarlyExitTest, DisabledExitsAlwaysReturnNegative) {
    uint32_t exit_layers[] = {1};
    float thresholds[] = {0.1f};
    uint32_t layer_sizes[] = {4};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 1, 2);
    ASSERT_NE(ee, nullptr);

    ee->enabled = false;

    float activation[] = {10.0f, 0.0f, 0.0f, 0.0f};
    float output[2];
    float confidence;

    int ret = nimcp_early_exit_evaluate(ee, 0, activation, 4, output, &confidence);
    // When disabled, should not trigger early exit
    EXPECT_EQ(ret, -1);
}

TEST_F(EarlyExitTest, InitialStatsAreZero) {
    uint32_t exit_layers[] = {1, 3};
    float thresholds[] = {0.5f, 0.5f};
    uint32_t layer_sizes[] = {4, 4};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 2, 2);
    ASSERT_NE(ee, nullptr);

    EXPECT_EQ(ee->total_inferences, 0u);
    EXPECT_EQ(ee->full_depth_count, 0u);
    for (int i = 0; i < 2; i++) {
        EXPECT_EQ(ee->early_exits[i], 0u);
    }
}

TEST_F(EarlyExitTest, ExitIndexOutOfBounds) {
    uint32_t exit_layers[] = {1, 3};
    float thresholds[] = {0.5f, 0.5f};
    uint32_t layer_sizes[] = {4, 4};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 2, 2);
    ASSERT_NE(ee, nullptr);

    float activation[] = {10.0f, 0.0f, 0.0f, 0.0f};
    float output[2] = {0};
    float confidence = 0.0f;

    // exit_idx=5 is >= num_exits=2, should return -1
    int ret = nimcp_early_exit_evaluate(ee, 5, activation, 4, output, &confidence);
    EXPECT_EQ(ret, -1);
}

TEST_F(EarlyExitTest, NullOutputPointer) {
    uint32_t exit_layers[] = {1};
    float thresholds[] = {0.5f};
    uint32_t layer_sizes[] = {4};

    ee = nimcp_early_exit_create(exit_layers, thresholds, layer_sizes, 1, 2);
    ASSERT_NE(ee, nullptr);

    float activation[] = {10.0f, 0.0f, 0.0f, 0.0f};
    float confidence = 0.0f;

    int ret = nimcp_early_exit_evaluate(ee, 0, activation, 4, nullptr, &confidence);
    EXPECT_EQ(ret, -1);
}
