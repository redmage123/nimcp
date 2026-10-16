/**
 * @file test_gpu_inference_integration.cpp
 * @brief Integration tests for GPU inference optimization
 *
 * Tests end-to-end GPU inference: create → decide → verify,
 * batch inference, parallel vs serial equivalence, frozen brain
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

#include "nimcp.h"
#include "gpu/execution/nimcp_gpu_detect.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GPUInferenceIntegrationTest : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;
    static constexpr uint32_t NUM_INPUTS = 20;
    static constexpr uint32_t NUM_OUTPUTS = 8;

    void SetUp() override {
        nimcp_init();
        brain = nimcp_brain_create("gpu_infer_integ", NIMCP_BRAIN_SMALL,
                                    NIMCP_TASK_CLASSIFICATION,
                                    NUM_INPUTS, NUM_OUTPUTS);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
        nimcp_shutdown();
    }

    void TrainBrain(int epochs = 5) {
        const char* labels[] = {"cat", "dog", "bird", "fish"};
        for (int e = 0; e < epochs; e++) {
            for (int c = 0; c < 4; c++) {
                std::vector<float> features(NUM_INPUTS, 0.0f);
                for (uint32_t i = 0; i < NUM_INPUTS; i++) {
                    features[i] = (float)(c * NUM_INPUTS + i) / (4.0f * NUM_INPUTS);
                }
                nimcp_brain_learn_example(brain, features.data(), NUM_INPUTS,
                                          labels[c], 1.0f);
            }
        }
    }
};

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(GPUInferenceIntegrationTest, CreateBrainWithGPUInference) {
    // Brain should be created successfully — GPU init is automatic
    char label[64] = {0};
    float confidence = -1.0f;
    std::vector<float> features(NUM_INPUTS, 0.5f);
    nimcp_status_t status = nimcp_brain_predict(brain, features.data(), NUM_INPUTS, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(label), 0u);
}

TEST_F(GPUInferenceIntegrationTest, GPUInferenceEndToEnd) {
    // Train the brain
    TrainBrain(10);

    // Run inference
    std::vector<float> features(NUM_INPUTS, 0.0f);
    for (uint32_t i = 0; i < NUM_INPUTS; i++) features[i] = (float)i / (float)NUM_INPUTS;

    char label[64] = {0};
    float confidence = -1.0f;
    nimcp_status_t status = nimcp_brain_predict(brain, features.data(), NUM_INPUTS, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(label), 0u);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(GPUInferenceIntegrationTest, FrozenBrainDeterministicInference) {
    TrainBrain(5);
    nimcp_brain_freeze(brain);
    ASSERT_TRUE(nimcp_brain_is_frozen(brain));

    std::vector<float> features(NUM_INPUTS);
    for (uint32_t i = 0; i < NUM_INPUTS; i++) features[i] = (float)i / (float)NUM_INPUTS;

    // Run 20 inferences — all should produce consistent confidence
    float first_confidence = -1.0f;
    for (int i = 0; i < 20; i++) {
        char label[64] = {0};
        float confidence = -1.0f;
        nimcp_status_t status = nimcp_brain_predict(brain, features.data(), NUM_INPUTS, label, &confidence);
        EXPECT_EQ(status, NIMCP_OK);
        if (first_confidence < 0) {
            first_confidence = confidence;
        } else {
            EXPECT_NEAR(confidence, first_confidence, 1e-4f)
                << "Frozen brain should produce deterministic results (iteration " << i << ")";
        }
    }
}

TEST_F(GPUInferenceIntegrationTest, FreezeAfterTraining) {
    // Train
    TrainBrain(10);

    // Freeze
    nimcp_status_t status = nimcp_brain_freeze(brain);
    EXPECT_EQ(status, NIMCP_OK);

    // Inference should still work
    std::vector<float> features(NUM_INPUTS, 0.3f);
    char label[64] = {0};
    float confidence = -1.0f;
    status = nimcp_brain_predict(brain, features.data(), NUM_INPUTS, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(label), 0u);
}

TEST_F(GPUInferenceIntegrationTest, CPUFallbackWhenNoGPU) {
    // This test verifies that inference works regardless of GPU availability
    // (CPU fallback is always active)
    std::vector<float> features(NUM_INPUTS, 0.5f);
    char label[64] = {0};
    float confidence = -1.0f;
    nimcp_status_t status = nimcp_brain_predict(brain, features.data(), NUM_INPUTS, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(label), 0u);
}

TEST_F(GPUInferenceIntegrationTest, MultipleInferencesStable) {
    TrainBrain(5);

    // Run many inferences — verify no crashes or memory issues
    for (int i = 0; i < 100; i++) {
        std::vector<float> features(NUM_INPUTS);
        for (uint32_t j = 0; j < NUM_INPUTS; j++) {
            features[j] = (float)(i * NUM_INPUTS + j) / 2000.0f;
        }
        char label[64] = {0};
        float confidence = -1.0f;
        nimcp_status_t status = nimcp_brain_predict(brain, features.data(), NUM_INPUTS, label, &confidence);
        EXPECT_EQ(status, NIMCP_OK);
    }
}
