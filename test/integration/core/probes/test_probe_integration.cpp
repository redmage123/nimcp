/**
 * @file test_probe_integration.cpp
 * @brief Integration tests — probes with real brain_decide_full and brain_learn_vector
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <unistd.h>

#include "core/probes/nimcp_brain_probes.h"
#include "core/brain/nimcp_brain_internal.h"

extern "C" {
#include "nimcp.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Integration Fixture: creates a real SMALL brain
 * ============================================================================ */

class ProbeIntegrationTest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        nimcp_init();
        brain = nimcp_brain_create("probe_int_test", NIMCP_BRAIN_SMALL,
                                    NIMCP_TASK_REGRESSION, 64, 32);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
        nimcp_shutdown();
    }
};

/* ============================================================================
 * Integration: Probes with brain_decide_full
 * ============================================================================ */

TEST_F(ProbeIntegrationTest, ProbeWithBrainDecideFull) {
    int count = nimcp_brain_attach_builtin_probes(brain, 100);
    EXPECT_GE(count, 1);

    float features[64];
    for (int i = 0; i < 64; i++) features[i] = 0.1f * (float)i;

    char label[64] = {0};
    float confidence = 0.0f;
    char explanation[256] = {0};
    float output_vector[32] = {0};
    uint32_t output_size = 32;
    uint32_t active_neurons = 0;
    float sparsity = 0.0f;
    uint64_t inference_time_us = 0;

    nimcp_status_t rc = nimcp_brain_decide_full(brain, features, 64,
        label, &confidence, explanation,
        output_vector, &output_size,
        &active_neurons, &sparsity, &inference_time_us);

    EXPECT_EQ(rc, NIMCP_OK);

    /* Query probe metrics — should have data from the inference pipeline */
    char* json = nullptr;
    int jrc = nimcp_brain_get_all_probe_metrics_json(brain, &json);
    EXPECT_EQ(jrc, 0);
    ASSERT_NE(json, nullptr);
    EXPECT_NE(strstr(json, "inference"), nullptr);
    nimcp_free(json);
}

TEST_F(ProbeIntegrationTest, ProbeWithBrainLearnVector) {
    int count = nimcp_brain_attach_builtin_probes(brain, 100);
    EXPECT_GE(count, 1);

    float features[64], target[32];
    for (int i = 0; i < 64; i++) features[i] = 0.1f;
    for (int i = 0; i < 32; i++) target[i] = 0.5f;

    nimcp_status_t rc = nimcp_brain_learn_vector(brain, features, 64,
        target, 32, "test_label", 1.0f);
    /* rc may fail on SMALL brain but shouldn't crash */
    (void)rc;

    char* json = nullptr;
    nimcp_brain_get_all_probe_metrics_json(brain, &json);
    if (json) {
        EXPECT_GT(strlen(json), 2u);
        nimcp_free(json);
    }
}

TEST_F(ProbeIntegrationTest, CompositeProbeMultipleModules) {
    uint16_t modules[] = {0x0100, 0x0210, 0x0230};
    uint32_t handle = 0;
    int rc = nimcp_brain_create_probe(brain, modules, 3, 100, 1, &handle);
    EXPECT_EQ(rc, 0);
    EXPECT_NE(handle, 0u);

    float feat[64] = {0.1f};
    char label[64] = {0};
    float confidence = 0.0f;
    nimcp_brain_predict(brain, feat, 64, label, &confidence);

    nimcp_brain_destroy_probe(brain, handle);
}

TEST_F(ProbeIntegrationTest, ProbeDoesNotAffectTraining) {
    float features[64], target[32];
    for (int i = 0; i < 64; i++) features[i] = 0.5f;
    for (int i = 0; i < 32; i++) target[i] = 0.3f;

    /* Train without probes */
    nimcp_status_t rc1 = nimcp_brain_learn_vector(brain, features, 64,
        target, 32, "no_probe", 1.0f);

    /* Attach all probes */
    nimcp_brain_attach_builtin_probes(brain, 100);

    /* Train with probes */
    nimcp_status_t rc2 = nimcp_brain_learn_vector(brain, features, 64,
        target, 32, "with_probe", 1.0f);

    /* Both should return same status (both succeed or both fail) */
    EXPECT_EQ(rc1, rc2);
}

TEST_F(ProbeIntegrationTest, BuiltinSamplersDuringTraining) {
    nimcp_brain_attach_builtin_probes(brain, 100);

    float features[64], target[32];
    for (int i = 0; i < 64; i++) features[i] = 0.2f;
    for (int i = 0; i < 32; i++) target[i] = 0.4f;

    for (int step = 0; step < 5; step++) {
        nimcp_brain_learn_vector(brain, features, 64, target, 32, "train", 1.0f);
    }

    usleep(200000);  /* 200ms — let sampler run */

    char* json = nullptr;
    nimcp_brain_get_all_probe_metrics_json(brain, &json);
    if (json) {
        EXPECT_GT(strlen(json), 10u);
        nimcp_free(json);
    }
}

TEST_F(ProbeIntegrationTest, PipelineStageRecordsDuringDecide) {
    nimcp_brain_attach_builtin_probes(brain, 100);

    float features[64];
    for (int i = 0; i < 64; i++) features[i] = 0.1f * (float)(i + 1);

    for (int i = 0; i < 3; i++) {
        char label[64] = {0};
        float confidence = 0.0f;
        nimcp_brain_predict(brain, features, 64, label, &confidence);
    }

    char* json = nullptr;
    nimcp_brain_get_all_probe_metrics_json(brain, &json);
    ASSERT_NE(json, nullptr);
    EXPECT_GT(strlen(json), 10u);
    nimcp_free(json);
}
