/**
 * @file test_srp_split_api_regression.cpp
 * @brief Regression tests for SRP-split API module (nimcp.c)
 *
 * Verifies that the #include-based SRP split of nimcp.c into 7 part files
 * (core, helpers, accessors, lifecycle, io, stats, processing) preserves
 * correct behavior across all responsibility areas.
 *
 * Each test targets functions from different part files to catch issues
 * with cross-file function visibility, forward declarations, and shared
 * static state.
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>
#include <cmath>
#include <cstdio>
#include <unistd.h>

/**
 * @brief Test fixture for API SRP split regression tests
 */
class SRPSplitAPIRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_status_t status = nimcp_init();
        ASSERT_EQ(status, NIMCP_SUCCESS) << "nimcp_init() failed during SetUp";
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// Lifecycle functions (nimcp_part_lifecycle.c)
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, BrainCreateDestroyCycle) {
    nimcp_brain_t brain = nimcp_brain_create(
        "srp_test_brain", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr) << "Brain creation failed after SRP split";
    nimcp_brain_destroy(brain);
}

TEST_F(SRPSplitAPIRegressionTest, EthicsCreateDestroyCycle) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr) << "Ethics creation failed after SRP split";
    nimcp_ethics_destroy(ethics);
}

TEST_F(SRPSplitAPIRegressionTest, KnowledgeCreateDestroyCycle) {
    nimcp_knowledge_t knowledge = nimcp_knowledge_create();
    ASSERT_NE(knowledge, nullptr) << "Knowledge creation failed after SRP split";
    nimcp_knowledge_destroy(knowledge);
}

TEST_F(SRPSplitAPIRegressionTest, NetworkCreateDestroyCycle) {
    nimcp_network_t network = nimcp_network_create(10, 5, 8, 0.01f);
    ASSERT_NE(network, nullptr) << "Network creation failed after SRP split";
    nimcp_network_destroy(network);
}

//=============================================================================
// Core functions (nimcp_part_core.c)
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, BrainLearnAndPredict) {
    nimcp_brain_t brain = nimcp_brain_create(
        "srp_learn_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    float features[10] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                          0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    nimcp_status_t status = nimcp_brain_learn_example(
        brain, features, 10, "class_a", 0.9f
    );
    EXPECT_EQ(status, NIMCP_SUCCESS) << "Learn example failed after SRP split";

    char label[NIMCP_MAX_LABEL_SIZE] = {0};
    float confidence = 0.0f;
    status = nimcp_brain_predict(brain, features, 10, label, &confidence);
    EXPECT_EQ(status, NIMCP_SUCCESS) << "Predict failed after SRP split";

    nimcp_brain_destroy(brain);
}

TEST_F(SRPSplitAPIRegressionTest, BrainInfer) {
    nimcp_brain_t brain = nimcp_brain_create(
        "srp_infer_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    float features[10] = {0.5f};
    float outputs[3] = {0};
    nimcp_status_t status = nimcp_brain_infer(brain, features, 10, outputs, 3);
    EXPECT_EQ(status, NIMCP_SUCCESS) << "Infer failed after SRP split";

    nimcp_brain_destroy(brain);
}

TEST_F(SRPSplitAPIRegressionTest, EthicsCheck) {
    nimcp_ethics_t ethics = nimcp_ethics_create();
    ASSERT_NE(ethics, nullptr);

    float situation[10] = {0.5f, 0.3f, 0.7f, 0.2f, 0.8f,
                           0.1f, 0.9f, 0.4f, 0.6f, 0.5f};
    float score = -1.0f;
    nimcp_status_t status = nimcp_ethics_check(ethics, situation, 10, &score);
    EXPECT_EQ(status, NIMCP_SUCCESS) << "Ethics check failed after SRP split";
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);

    nimcp_ethics_destroy(ethics);
}

//=============================================================================
// Accessor functions (nimcp_part_accessors.c)
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, BrainGetNeuronCount) {
    nimcp_brain_t brain = nimcp_brain_create(
        "srp_accessor_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    uint32_t count = nimcp_brain_get_neuron_count(brain);
    EXPECT_GT(count, 0u) << "Neuron count should be positive after creation";

    nimcp_brain_destroy(brain);
}

TEST_F(SRPSplitAPIRegressionTest, BrainGetUtilizationMetrics) {
    nimcp_brain_t brain = nimcp_brain_create(
        "srp_util_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    float utilization = -1.0f, saturation = -1.0f;
    bool success = nimcp_brain_get_utilization_metrics(brain, &utilization, &saturation);
    EXPECT_TRUE(success) << "Get utilization metrics failed after SRP split";
    EXPECT_GE(utilization, 0.0f);
    EXPECT_GE(saturation, 0.0f);

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Helper functions (nimcp_part_helpers.c)
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, ErrorStringAfterInvalidOperation) {
    nimcp_status_t status = nimcp_brain_learn_example(nullptr, nullptr, 0, nullptr, 0.0f);
    EXPECT_NE(status, NIMCP_SUCCESS) << "NULL brain should return error";

    const char* error = nimcp_get_error();
    EXPECT_NE(error, nullptr) << "Error string should not be NULL";
}

TEST_F(SRPSplitAPIRegressionTest, NullSafetyAcrossPartFiles) {
    nimcp_brain_destroy(nullptr);
    nimcp_ethics_destroy(nullptr);
    nimcp_knowledge_destroy(nullptr);
    nimcp_network_destroy(nullptr);

    uint32_t count = nimcp_brain_get_neuron_count(nullptr);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Version functions (nimcp_part_stats.c / nimcp_part_core.c)
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, VersionFunctions) {
    const char* version = nimcp_version();
    EXPECT_NE(version, nullptr);
    EXPECT_STREQ(version, NIMCP_VERSION_STRING);

    int version_int = nimcp_version_int();
    EXPECT_EQ(version_int, NIMCP_VERSION_MAJOR * 10000 +
                            NIMCP_VERSION_MINOR * 100 +
                            NIMCP_VERSION_PATCH);
}

//=============================================================================
// IO functions (nimcp_part_io.c)
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, BrainSaveLoadCycle) {
    nimcp_brain_t brain = nimcp_brain_create(
        "srp_io_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    float features[10] = {1.0f};
    nimcp_brain_learn_example(brain, features, 10, "test_class", 0.9f);

    const char* filepath = "/tmp/nimcp_srp_test_brain.bin";
    nimcp_status_t status = nimcp_brain_save(brain, filepath);
    EXPECT_EQ(status, NIMCP_SUCCESS) << "Brain save failed after SRP split";

    nimcp_brain_t loaded = nimcp_brain_load(filepath);
    EXPECT_NE(loaded, nullptr) << "Brain load failed after SRP split";

    if (loaded) {
        char label[NIMCP_MAX_LABEL_SIZE] = {0};
        float confidence = 0.0f;
        status = nimcp_brain_predict(loaded, features, 10, label, &confidence);
        EXPECT_EQ(status, NIMCP_SUCCESS);

        uint32_t count = nimcp_brain_get_neuron_count(loaded);
        EXPECT_GT(count, 0u);

        nimcp_brain_destroy(loaded);
    }

    nimcp_brain_destroy(brain);
    unlink(filepath);
}

//=============================================================================
// Processing functions (nimcp_part_processing.c)
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, BrainResizePreservesFunction) {
    nimcp_brain_t brain = nimcp_brain_create(
        "srp_resize_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    uint32_t original_count = nimcp_brain_get_neuron_count(brain);
    EXPECT_GT(original_count, 0u);

    bool resized = nimcp_brain_resize(brain, original_count * 2);
    EXPECT_TRUE(resized) << "Brain resize failed after SRP split";

    uint32_t new_count = nimcp_brain_get_neuron_count(brain);
    EXPECT_GE(new_count, original_count);

    float features[10] = {0.5f};
    float outputs[3] = {0};
    nimcp_status_t status = nimcp_brain_infer(brain, features, 10, outputs, 3);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Cross-part interaction: Probe
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, BrainProbeCollectsMetrics) {
    nimcp_brain_t brain = nimcp_brain_create(
        "srp_probe_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    ASSERT_NE(brain, nullptr);

    nimcp_brain_probe_t probe;
    memset(&probe, 0, sizeof(probe));
    nimcp_status_t status = nimcp_brain_probe(brain, &probe);
    EXPECT_EQ(status, NIMCP_SUCCESS) << "Brain probe failed after SRP split";
    EXPECT_GT(probe.num_neurons, 0u);

    nimcp_brain_destroy(brain);
}

//=============================================================================
// Multiple instances (shared static state across parts)
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, MultipleInstancesIndependent) {
    nimcp_brain_t brain1 = nimcp_brain_create(
        "srp_multi_1", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    nimcp_brain_t brain2 = nimcp_brain_create(
        "srp_multi_2", NIMCP_BRAIN_TINY, NIMCP_TASK_REGRESSION, 5, 1
    );

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);
    EXPECT_NE(brain1, brain2);

    float f1[10] = {1.0f};
    nimcp_brain_learn_example(brain1, f1, 10, "class_a", 0.9f);

    float f2[5] = {0.5f};
    char label[NIMCP_MAX_LABEL_SIZE] = {0};
    float confidence = 0.0f;
    nimcp_status_t status = nimcp_brain_predict(brain2, f2, 5, label, &confidence);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_brain_destroy(brain2);
    nimcp_brain_destroy(brain1);
}

//=============================================================================
// Init/Shutdown idempotency
//=============================================================================

TEST_F(SRPSplitAPIRegressionTest, ReInitAfterShutdown) {
    nimcp_shutdown();

    nimcp_status_t status = nimcp_init();
    EXPECT_EQ(status, NIMCP_SUCCESS) << "Re-init failed after SRP split";

    nimcp_brain_t brain = nimcp_brain_create(
        "srp_reinit_test", NIMCP_BRAIN_TINY, NIMCP_TASK_CLASSIFICATION, 10, 3
    );
    EXPECT_NE(brain, nullptr);

    if (brain) {
        nimcp_brain_destroy(brain);
    }
}
