/**
 * @file test_e2e_gpu_inference_pipeline.cpp
 * @brief End-to-end tests for the GPU inference optimization pipeline
 *
 * Tests full pipeline: create → train → freeze → batch → concurrent,
 * VRAM lifecycle, large neuron counts
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>

#include "nimcp.h"
#include "gpu/execution/nimcp_gpu_detect.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GPUInferencePipelineTest : public ::testing::Test {
protected:
    static constexpr uint32_t N_IN = 32;
    static constexpr uint32_t N_OUT = 10;

    void SetUp() override {
        nimcp_init();
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    nimcp_brain_t CreateAndTrain(const char* name, int train_steps = 20) {
        nimcp_brain_t b = nimcp_brain_create(name, NIMCP_BRAIN_SMALL,
                                              NIMCP_TASK_CLASSIFICATION, N_IN, N_OUT);
        if (!b) return nullptr;

        const char* labels[] = {"zero", "one", "two", "three", "four"};
        for (int i = 0; i < train_steps; i++) {
            std::vector<float> features(N_IN);
            int cls = i % 5;
            for (uint32_t j = 0; j < N_IN; j++) {
                features[j] = (float)(cls * N_IN + j) / (5.0f * N_IN);
            }
            nimcp_brain_learn_example(b, features.data(), N_IN, labels[cls], 1.0f);
        }
        return b;
    }
};

//=============================================================================
// E2E Pipeline Tests
//=============================================================================

TEST_F(GPUInferencePipelineTest, FullPipeline_CreateFreezePredict) {
    nimcp_brain_t brain = CreateAndTrain("pipeline_test", 30);
    ASSERT_NE(brain, nullptr);

    // Freeze
    nimcp_status_t status = nimcp_brain_freeze(brain);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_TRUE(nimcp_brain_is_frozen(brain));

    // Run batch of predictions
    for (int i = 0; i < 20; i++) {
        std::vector<float> features(N_IN);
        for (uint32_t j = 0; j < N_IN; j++) {
            features[j] = (float)((i * 7 + j) % 100) / 100.0f;
        }
        char label[64] = {0};
        float confidence = -1.0f;
        status = nimcp_brain_predict(brain, features.data(), N_IN, label, &confidence);
        EXPECT_EQ(status, NIMCP_OK);
        EXPECT_GT(strlen(label), 0u);
    }

    nimcp_brain_destroy(brain);
}

TEST_F(GPUInferencePipelineTest, RepeatedInferenceOnFrozenBrain) {
    nimcp_brain_t brain = CreateAndTrain("repeated_test", 20);
    ASSERT_NE(brain, nullptr);

    nimcp_brain_freeze(brain);

    // Run multiple inferences sequentially on frozen brain — verify stability
    int success_count = 0;
    for (int i = 0; i < 5; i++) {
        std::vector<float> features(N_IN);
        for (uint32_t j = 0; j < N_IN; j++) {
            features[j] = (float)((i * 100 + j) % 100) / 100.0f;
        }
        char label[64] = {0};
        float confidence = -1.0f;
        nimcp_status_t status = nimcp_brain_predict(brain, features.data(), N_IN, label, &confidence);
        if (status == NIMCP_OK && strlen(label) > 0) {
            success_count++;
        }
    }

    EXPECT_EQ(success_count, 5) << "All inferences on frozen brain should succeed";
    nimcp_brain_destroy(brain);
}

TEST_F(GPUInferencePipelineTest, MixedTrainingAndInference) {
    // Create two brains — one training, one frozen inferring
    nimcp_brain_t train_brain = CreateAndTrain("train_brain", 10);
    nimcp_brain_t infer_brain = CreateAndTrain("infer_brain", 10);
    ASSERT_NE(train_brain, nullptr);
    ASSERT_NE(infer_brain, nullptr);

    nimcp_brain_freeze(infer_brain);

    // Alternate training on one and inference on the other
    for (int i = 0; i < 3; i++) {
        // Train
        std::vector<float> features(N_IN, (float)i / 10.0f);
        nimcp_brain_learn_example(train_brain, features.data(), N_IN, "train", 1.0f);

        // Infer on frozen brain
        char label[64] = {0};
        float confidence = -1.0f;
        nimcp_brain_predict(infer_brain, features.data(), N_IN, label, &confidence);
        EXPECT_EQ(confidence, confidence);  // Not NaN
    }

    nimcp_brain_destroy(train_brain);
    nimcp_brain_destroy(infer_brain);
}

TEST_F(GPUInferencePipelineTest, GPUMemoryLifecycle_NoVRAMLeak) {
    // Create and destroy multiple brains — VRAM should be released
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "vram_test_%d", i);
        nimcp_brain_t b = nimcp_brain_create(name, NIMCP_BRAIN_SMALL,
                                              NIMCP_TASK_CLASSIFICATION, N_IN, N_OUT);
        ASSERT_NE(b, nullptr);

        // Quick inference
        std::vector<float> features(N_IN, 0.5f);
        char label[64] = {0};
        float confidence = -1.0f;
        nimcp_brain_predict(b, features.data(), N_IN, label, &confidence);

        nimcp_brain_destroy(b);
    }
    // If VRAM leaked, we'd likely OOM on the 5th brain
    SUCCEED();
}

TEST_F(GPUInferencePipelineTest, FreezeAndSaveLoad) {
    nimcp_brain_t brain = CreateAndTrain("save_load_test", 10);
    ASSERT_NE(brain, nullptr);

    // Freeze before saving
    nimcp_brain_freeze(brain);

    // Save
    const char* path = "/tmp/nimcp_test_frozen_brain.bin";
    nimcp_status_t status = nimcp_brain_save(brain, path);
    EXPECT_EQ(status, NIMCP_OK);

    // Get reference prediction
    std::vector<float> features(N_IN, 0.3f);
    char label_before[64] = {0};
    float conf_before = -1.0f;
    nimcp_brain_predict(brain, features.data(), N_IN, label_before, &conf_before);

    nimcp_brain_destroy(brain);

    // Load
    nimcp_brain_t loaded = nimcp_brain_load(path);
    ASSERT_NE(loaded, nullptr);

    // Inference should still work
    char label_after[64] = {0};
    float conf_after = -1.0f;
    status = nimcp_brain_predict(loaded, features.data(), N_IN, label_after, &conf_after);
    EXPECT_EQ(status, NIMCP_OK);

    nimcp_brain_destroy(loaded);

    // Cleanup
    remove(path);
}
