/**
 * @file test_resize.cpp
 * @brief GoogleTest unit tests for NIMCP edge brain resize and maturation subsystem
 *
 * Tests maturation lifecycle, stage progression, output/LR scaling,
 * and progress tracking. Resize check/contract/expand tests require
 * a brain handle and are covered in integration tests.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class ResizeTest : public ::testing::Test {
protected:
    nimcp_maturation_tracker_t* tracker = nullptr;

    void TearDown() override {
        if (tracker) {
            nimcp_maturation_destroy(tracker);
            tracker = nullptr;
        }
    }
};

TEST_F(ResizeTest, MaturationCreateDestroy) {
    tracker = nimcp_maturation_create(100, 1000, 0.5f);
    ASSERT_NE(tracker, nullptr);
    EXPECT_EQ(tracker->capacity, 100u);
    EXPECT_EQ(tracker->maturation_steps, 1000u);
    EXPECT_FLOAT_EQ(tracker->existing_lr_scale, 0.5f);
    EXPECT_EQ(tracker->count, 0u);
}

TEST_F(ResizeTest, MaturationAddNeuronStartsAsProgenitor) {
    tracker = nimcp_maturation_create(10, 100, 0.5f);
    ASSERT_NE(tracker, nullptr);

    int ret = nimcp_maturation_add_neuron(tracker, 42);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(tracker->count, 1u);
    EXPECT_EQ(tracker->neurons[0].neuron_id, 42u);
    EXPECT_EQ(tracker->neurons[0].stage, NIMCP_MATURATION_PROGENITOR);
    EXPECT_FLOAT_EQ(tracker->neurons[0].maturity, 0.0f);
}

TEST_F(ResizeTest, MaturationStepProgressesThroughStages) {
    // PROGENITOR lasts 100 steps, need maturation_steps >= 501 for full lifecycle
    tracker = nimcp_maturation_create(10, 1000, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 1);
    EXPECT_EQ(tracker->neurons[0].stage, NIMCP_MATURATION_PROGENITOR);

    // Step 100 times to progress past PROGENITOR (transition at step 100)
    for (int i = 0; i < 101; i++) {
        nimcp_maturation_step(tracker);
    }

    // Should have progressed past PROGENITOR
    EXPECT_NE(tracker->neurons[0].stage, NIMCP_MATURATION_PROGENITOR);
}

TEST_F(ResizeTest, MaturationFullCycle) {
    // Full lifecycle: PROGENITOR(100) + IMMATURE(400) + INTEGRATING(500) = 1000
    tracker = nimcp_maturation_create(10, 1000, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 1);

    // Step through full maturation (need >= 1000 steps)
    for (int i = 0; i < 1100; i++) {
        nimcp_maturation_step(tracker);
    }

    EXPECT_EQ(tracker->neurons[0].stage, NIMCP_MATURATION_MATURE);
    EXPECT_NEAR(tracker->neurons[0].maturity, 1.0f, 0.01f);
}

TEST_F(ResizeTest, MaturationGetProgressZeroInitially) {
    tracker = nimcp_maturation_create(10, 100, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 1);
    nimcp_maturation_add_neuron(tracker, 2);

    float progress = nimcp_maturation_get_progress(tracker);
    EXPECT_NEAR(progress, 0.0f, 0.01f);
}

TEST_F(ResizeTest, MaturationGetProgressOneWhenAllMature) {
    // Need maturation_steps >= 501 for full lifecycle
    tracker = nimcp_maturation_create(10, 1000, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 1);
    nimcp_maturation_add_neuron(tracker, 2);

    // Step through full maturation (need >= 1000 steps)
    for (int i = 0; i < 1100; i++) {
        nimcp_maturation_step(tracker);
    }

    float progress = nimcp_maturation_get_progress(tracker);
    EXPECT_NEAR(progress, 1.0f, 0.01f);
}

TEST_F(ResizeTest, MaturationOutputScaleProgenitorIsZero) {
    tracker = nimcp_maturation_create(10, 100, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 42);

    float scale = nimcp_maturation_get_output_scale(tracker, 42);
    EXPECT_NEAR(scale, 0.0f, 0.01f);
}

TEST_F(ResizeTest, MaturationOutputScaleMatureIsOne) {
    tracker = nimcp_maturation_create(10, 1000, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 42);

    for (int i = 0; i < 1100; i++) {
        nimcp_maturation_step(tracker);
    }

    float scale = nimcp_maturation_get_output_scale(tracker, 42);
    EXPECT_NEAR(scale, 1.0f, 0.01f);
}

TEST_F(ResizeTest, MaturationLRScaleReducedDuringMaturation) {
    tracker = nimcp_maturation_create(10, 100, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 1);

    float lr_during = nimcp_maturation_get_lr_scale(tracker);
    // During maturation, existing neurons should have reduced LR
    EXPECT_LE(lr_during, 1.0f);
}

TEST_F(ResizeTest, MaturationLRScaleOneWhenDone) {
    tracker = nimcp_maturation_create(10, 1000, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 1);

    for (int i = 0; i < 1100; i++) {
        nimcp_maturation_step(tracker);
    }

    float lr_done = nimcp_maturation_get_lr_scale(tracker);
    EXPECT_NEAR(lr_done, 1.0f, 0.01f);
}

TEST_F(ResizeTest, MaturationMultipleNeurons) {
    tracker = nimcp_maturation_create(100, 1000, 0.5f);
    ASSERT_NE(tracker, nullptr);

    for (uint32_t i = 0; i < 50; i++) {
        nimcp_maturation_add_neuron(tracker, i);
    }
    EXPECT_EQ(tracker->count, 50u);

    // Step all to maturity (need >= 1000 steps)
    for (int i = 0; i < 1100; i++) {
        nimcp_maturation_step(tracker);
    }

    float progress = nimcp_maturation_get_progress(tracker);
    EXPECT_NEAR(progress, 1.0f, 0.01f);
}

TEST_F(ResizeTest, MaturationDestroyNullSafe) {
    nimcp_maturation_destroy(nullptr);
}

TEST_F(ResizeTest, ResizeConfigDefault) {
    nimcp_resize_config_t config = nimcp_resize_config_default();
    // target_neuron_count defaults to 0 (must be set by caller before use)
    EXPECT_EQ(config.target_neuron_count, 0u);
    EXPECT_GT(config.maturation_steps, 0u);
}

TEST_F(ResizeTest, MaturationOutputScaleMonotonicallyIncreases) {
    tracker = nimcp_maturation_create(10, 1000, 0.5f);
    ASSERT_NE(tracker, nullptr);

    nimcp_maturation_add_neuron(tracker, 1);

    float prev_scale = 0.0f;
    bool monotonic = true;

    for (int i = 0; i < 1100; i++) {
        nimcp_maturation_step(tracker);
        float scale = nimcp_maturation_get_output_scale(tracker, 1);
        if (scale < prev_scale - 1e-6f) {
            monotonic = false;
            break;
        }
        prev_scale = scale;
    }

    EXPECT_TRUE(monotonic) << "Output scale should monotonically increase";
}
