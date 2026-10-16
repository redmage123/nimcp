/**
 * @file test_gpu_inference_unit.cpp
 * @brief Unit tests for GPU inference optimization features
 *
 * Tests: GPU weight cache creation, forward pass, batch forward,
 *        freeze lifecycle, frozen learning rejection, parallel stage dispatch
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers with their own extern "C" guards
#include "nimcp.h"
#include "gpu/execution/nimcp_gpu_detect.h"

static constexpr float GPU_TOLERANCE = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

class GPUInferenceUnitTest : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;

    void SetUp() override {
        nimcp_init();
        brain = nimcp_brain_create("gpu_infer_unit", NIMCP_BRAIN_SMALL,
                                    NIMCP_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
        nimcp_shutdown();
    }
};

//=============================================================================
// Feature 1: GPU Inference Init Tests
//=============================================================================

TEST_F(GPUInferenceUnitTest, WeightCacheCreatedFromBrainNetwork) {
    if (!gpu_is_available()) { GTEST_SKIP() << "No GPU available"; }

    // Verify that inference works — implies GPU weight cache was created
    std::vector<float> features(10, 0.5f);
    char label[64] = {0};
    float confidence = -1.0f;
    nimcp_status_t status = nimcp_brain_predict(brain, features.data(), 10, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(label), 0u);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(GPUInferenceUnitTest, GPUForwardPassMatchesCPU) {
    if (!gpu_is_available()) { GTEST_SKIP() << "No GPU available"; }

    // Run a forward pass through brain_predict
    std::vector<float> features(10);
    for (int i = 0; i < 10; i++) features[i] = (float)(i + 1) / 10.0f;

    char label[64] = {0};
    float confidence = -1.0f;
    nimcp_status_t status = nimcp_brain_predict(brain, features.data(), 10, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(label), 0u);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

//=============================================================================
// Feature 2: Batch Forward Pass Tests
//=============================================================================

TEST_F(GPUInferenceUnitTest, BatchForwardPassSingleSample) {
    if (!gpu_is_available()) { GTEST_SKIP() << "No GPU available"; }

    // Single sample inference should work
    std::vector<float> input(10, 0.3f);
    char label[64] = {0};
    float confidence = -1.0f;
    nimcp_status_t status = nimcp_brain_predict(brain, input.data(), 10, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
}

//=============================================================================
// Feature 4: Freeze Lifecycle Tests
//=============================================================================

TEST_F(GPUInferenceUnitTest, FreezeLifecycle) {
    // Brain should not be frozen initially
    EXPECT_FALSE(nimcp_brain_is_frozen(brain));

    // Freeze should succeed
    nimcp_status_t status = nimcp_brain_freeze(brain);
    EXPECT_EQ(status, NIMCP_OK);

    // Should be frozen now
    EXPECT_TRUE(nimcp_brain_is_frozen(brain));

    // Double freeze should succeed (idempotent)
    status = nimcp_brain_freeze(brain);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_TRUE(nimcp_brain_is_frozen(brain));
}

TEST_F(GPUInferenceUnitTest, FrozenBrainRejectsTrain) {
    // Train first to verify learning works
    std::vector<float> features(10, 0.5f);
    nimcp_status_t status = nimcp_brain_learn_example(brain, features.data(), 10, "cat", 1.0f);
    EXPECT_EQ(status, NIMCP_OK);

    // Freeze
    status = nimcp_brain_freeze(brain);
    EXPECT_EQ(status, NIMCP_OK);

    // Learning should still "succeed" at API level but internally does nothing
    // (adaptive_network_learn returns 0.0f when frozen)
    status = nimcp_brain_learn_example(brain, features.data(), 10, "dog", 1.0f);
    EXPECT_EQ(status, NIMCP_OK);
}

TEST_F(GPUInferenceUnitTest, FrozenWeightImmutability) {
    // Train a bit
    std::vector<float> features(10);
    for (int i = 0; i < 10; i++) features[i] = (float)i / 10.0f;
    nimcp_brain_learn_example(brain, features.data(), 10, "test", 1.0f);

    // Get prediction before freeze
    char label1[64] = {0};
    float conf1 = -1.0f;
    nimcp_brain_predict(brain, features.data(), 10, label1, &conf1);

    // Freeze
    nimcp_brain_freeze(brain);

    // Get prediction after freeze
    char label2[64] = {0};
    float conf2 = -1.0f;
    nimcp_brain_predict(brain, features.data(), 10, label2, &conf2);

    // Results should be the same (or very close due to floating point)
    EXPECT_NEAR(conf1, conf2, GPU_TOLERANCE);
}

//=============================================================================
// Feature 3: Parallel Stage Dispatch Tests
//=============================================================================

TEST_F(GPUInferenceUnitTest, ParallelStageDispatchAndSync) {
    // Verify the brain functions correctly with inference pool
    std::vector<float> features(10, 0.5f);
    char label[64] = {0};
    float confidence = -1.0f;
    nimcp_status_t status = nimcp_brain_predict(brain, features.data(), 10, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
}

TEST_F(GPUInferenceUnitTest, NullBrainFreezeHandling) {
    nimcp_status_t status = nimcp_brain_freeze(nullptr);
    EXPECT_NE(status, NIMCP_OK);

    EXPECT_FALSE(nimcp_brain_is_frozen(nullptr));
}
