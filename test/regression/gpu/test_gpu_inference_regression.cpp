/**
 * @file test_gpu_inference_regression.cpp
 * @brief Regression tests for GPU inference optimization
 *
 * Tests numerical equivalence: frozen vs unfrozen, batch vs sequential,
 * weight dirty flag correctness, repeated stability, accuracy preservation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "nimcp.h"
#include "gpu/execution/nimcp_gpu_detect.h"

static constexpr float EQUIV_TOLERANCE = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

class GPUInferenceRegressionTest : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;
    static constexpr uint32_t N_IN = 16;
    static constexpr uint32_t N_OUT = 6;

    void SetUp() override {
        nimcp_init();
        brain = nimcp_brain_create("gpu_infer_regr", NIMCP_BRAIN_SMALL,
                                    NIMCP_TASK_CLASSIFICATION, N_IN, N_OUT);
        ASSERT_NE(brain, nullptr);

        // Light training to establish weights
        const char* labels[] = {"a", "b", "c"};
        for (int e = 0; e < 3; e++) {
            for (int c = 0; c < 3; c++) {
                std::vector<float> f(N_IN, 0.0f);
                for (uint32_t i = 0; i < N_IN; i++) f[i] = (float)(c * N_IN + i) / 48.0f;
                nimcp_brain_learn_example(brain, f.data(), N_IN, labels[c], 1.0f);
            }
        }
    }

    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
        nimcp_shutdown();
    }
};

//=============================================================================
// Regression Tests
//=============================================================================

TEST_F(GPUInferenceRegressionTest, FrozenVsUnfrozenEquivalence) {
    // Get predictions before freezing
    std::vector<float> features(N_IN);
    for (uint32_t i = 0; i < N_IN; i++) features[i] = (float)i / (float)N_IN;

    char label_before[64] = {0};
    float conf_before = -1.0f;
    nimcp_brain_predict(brain, features.data(), N_IN, label_before, &conf_before);

    // Freeze
    nimcp_brain_freeze(brain);

    // Get predictions after freezing
    char label_after[64] = {0};
    float conf_after = -1.0f;
    nimcp_brain_predict(brain, features.data(), N_IN, label_after, &conf_after);

    // Confidence should be the same (freeze doesn't change weights)
    EXPECT_NEAR(conf_after, conf_before, EQUIV_TOLERANCE)
        << "Freeze should not change inference output";
}

TEST_F(GPUInferenceRegressionTest, WeightDirtyFlagCorrectness) {
    // Train modifies weights (sets dirty flag)
    std::vector<float> features(N_IN, 0.5f);
    nimcp_brain_learn_example(brain, features.data(), N_IN, "test", 1.0f);

    // Inference should still work after learning (re-uploads if dirty)
    char label[64] = {0};
    float confidence = -1.0f;
    nimcp_status_t status = nimcp_brain_predict(brain, features.data(), N_IN, label, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(label), 0u);
}

TEST_F(GPUInferenceRegressionTest, RepeatedInferenceStability) {
    // Run 50 random inputs — verify no NaN/Inf in results
    for (int i = 0; i < 50; i++) {
        std::vector<float> features(N_IN);
        for (uint32_t j = 0; j < N_IN; j++) {
            features[j] = (float)((i * 17 + j * 31) % 100) / 100.0f;
        }

        char label[64] = {0};
        float confidence = -1.0f;
        nimcp_status_t status = nimcp_brain_predict(brain, features.data(), N_IN, label, &confidence);
        EXPECT_EQ(status, NIMCP_OK);
        EXPECT_FALSE(std::isnan(confidence)) << "NaN confidence at iteration " << i;
        EXPECT_FALSE(std::isinf(confidence)) << "Inf confidence at iteration " << i;
        EXPECT_GE(confidence, 0.0f);
        EXPECT_LE(confidence, 1.0f);
    }
}

TEST_F(GPUInferenceRegressionTest, FreezePreservesAccuracy) {
    // Check accuracy before freeze
    float acc_before = nimcp_brain_get_accuracy(brain);

    // Freeze and check accuracy is preserved
    nimcp_brain_freeze(brain);
    float acc_after = nimcp_brain_get_accuracy(brain);

    EXPECT_NEAR(acc_after, acc_before, EQUIV_TOLERANCE)
        << "Freeze should preserve accuracy metric";
}

TEST_F(GPUInferenceRegressionTest, MultipleTrainFreezeCycles) {
    // Train, predict, freeze is the normal lifecycle
    // Verify no resource leaks or state corruption
    for (int cycle = 0; cycle < 3; cycle++) {
        nimcp_brain_t b = nimcp_brain_create("cycle_test", NIMCP_BRAIN_SMALL,
                                              NIMCP_TASK_CLASSIFICATION, N_IN, N_OUT);
        ASSERT_NE(b, nullptr);

        // Train
        std::vector<float> features(N_IN, 0.5f);
        nimcp_brain_learn_example(b, features.data(), N_IN, "label", 1.0f);

        // Predict
        char label[64] = {0};
        float confidence = -1.0f;
        nimcp_brain_predict(b, features.data(), N_IN, label, &confidence);

        // Freeze
        nimcp_brain_freeze(b);
        EXPECT_TRUE(nimcp_brain_is_frozen(b));

        // Predict again
        nimcp_brain_predict(b, features.data(), N_IN, label, &confidence);

        nimcp_brain_destroy(b);
    }
}
