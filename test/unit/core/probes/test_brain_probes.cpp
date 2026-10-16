/**
 * @file test_brain_probes.cpp
 * @brief Unit tests for the unified brain probe system
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

/* Include probes header directly — it has C++ compatibility via PROBE_ATOMIC */
#include "core/probes/nimcp_brain_probes.h"

extern "C" {
#include "utils/memory/nimcp_memory.h"
}

/* Include brain_internal after extern "C" block to avoid CUDA template issues */
#include "core/brain/nimcp_brain_internal.h"

/* ============================================================================
 * Test Fixture: creates a minimal brain_struct for probe testing
 * ============================================================================ */

class BrainProbeTest : public ::testing::Test {
protected:
    brain_t brain;
    probe_registry_t* reg;

    void SetUp() override {
        brain = (brain_t)calloc(1, sizeof(struct brain_struct));
        ASSERT_NE(brain, nullptr);
        /* Set up minimal fields probes might read */
        brain->stats.num_neurons = 1000;
        brain->stats.total_inferences = 42;
        brain->stats.avg_inference_time_us = 1234.5f;
        brain->network_metrics.ema_ann_loss = 0.5f;
        brain->network_metrics.ann_steps = 100;
        brain->network_metrics.ema_snn_loss = 0.3f;
        brain->network_metrics.snn_steps = 50;
        brain->cognitive_stats.jepa_steps = 25;
        brain->cognitive_stats.jepa_last_loss = 0.1f;

        reg = probe_registry_create(brain);
        ASSERT_NE(reg, nullptr);
    }

    void TearDown() override {
        if (reg) probe_registry_destroy(reg);
        if (brain) free(brain);
    }
};

/* ============================================================================
 * Registry Lifecycle
 * ============================================================================ */

TEST_F(BrainProbeTest, RegistryCreateDestroy) {
    /* reg is already created in SetUp */
    EXPECT_NE(reg, nullptr);
    EXPECT_EQ(reg->count, 0u);
    EXPECT_EQ(reg->brain, brain);
}

TEST(BrainProbeNullTest, RegistryCreateNull) {
    probe_registry_t* r = probe_registry_create(nullptr);
    EXPECT_EQ(r, nullptr);
}

TEST(BrainProbeNullTest, RegistryDestroyNull) {
    /* Should not crash */
    probe_registry_destroy(nullptr);
}

/* ============================================================================
 * Active Probe Creation
 * ============================================================================ */

static void dummy_sampler(void* mp, uint16_t mid, brain_t b,
                           probe_metric_t* m, uint32_t* c, void* ud) {
    (void)mp; (void)mid; (void)ud;
    if (!b || !m || !c) return;
    uint32_t max = *c;
    *c = 0;
    if (max > 0) {
        strncpy(m[0].key, "test_val", PROBE_KEY_LEN - 1);
        m[0].type = PROBE_METRIC_FLOAT;
        m[0].value.f = 42.0f;
        *c = 1;
    }
}

TEST_F(BrainProbeTest, CreateActiveProbe) {
    uint16_t modules[] = {0x0100};
    probe_handle_t h = probe_create_active(reg, "test", modules, 1,
                                            dummy_sampler, nullptr, 1000);
    EXPECT_NE(h, PROBE_INVALID_HANDLE);
    EXPECT_EQ(reg->count, 1u);
}

TEST_F(BrainProbeTest, CreateActiveNullSampleFn) {
    uint16_t modules[] = {0x0100};
    probe_handle_t h = probe_create_active(reg, "test", modules, 1,
                                            nullptr, nullptr, 1000);
    EXPECT_EQ(h, PROBE_INVALID_HANDLE);
}

TEST_F(BrainProbeTest, CreatePassiveProbe) {
    uint16_t modules[] = {0x0100};
    probe_handle_t h = probe_create_passive(reg, "passive", modules, 1,
                                             nullptr, 0);
    EXPECT_NE(h, PROBE_INVALID_HANDLE);
}

TEST_F(BrainProbeTest, DestroyProbe) {
    uint16_t modules[] = {0x0100};
    probe_handle_t h = probe_create_active(reg, "test", modules, 1,
                                            dummy_sampler, nullptr, 1000);
    EXPECT_NE(h, PROBE_INVALID_HANDLE);
    EXPECT_TRUE(probe_destroy(reg, h));
    EXPECT_FALSE(probe_destroy(reg, h));  /* Already destroyed */
}

TEST_F(BrainProbeTest, DestroyInvalidHandle) {
    EXPECT_FALSE(probe_destroy(reg, PROBE_INVALID_HANDLE));
    EXPECT_FALSE(probe_destroy(reg, 9999));
}

TEST_F(BrainProbeTest, MaxCapacity) {
    uint16_t modules[] = {0x0100};
    for (uint32_t i = 0; i < PROBE_REGISTRY_MAX; i++) {
        probe_handle_t h = probe_create_active(reg, "cap", modules, 1,
                                                dummy_sampler, nullptr, 1000);
        EXPECT_NE(h, PROBE_INVALID_HANDLE) << "Failed at probe " << i;
    }
    /* 33rd should fail */
    probe_handle_t h = probe_create_active(reg, "overflow", modules, 1,
                                            dummy_sampler, nullptr, 1000);
    EXPECT_EQ(h, PROBE_INVALID_HANDLE);
}

/* ============================================================================
 * Metrics Query
 * ============================================================================ */

TEST_F(BrainProbeTest, GetMetricsEmpty) {
    uint16_t modules[] = {0x0100};
    probe_handle_t h = probe_create_active(reg, "test", modules, 1,
                                            dummy_sampler, nullptr, 1000);
    probe_metric_t metrics[64];
    /* Before any sampling, metrics should be empty (count=0) */
    uint32_t count = probe_get_metrics(reg, h, metrics, 64);
    EXPECT_EQ(count, 0u);
}

TEST_F(BrainProbeTest, GetMetricsInvalidHandle) {
    probe_metric_t metrics[64];
    uint32_t count = probe_get_metrics(reg, PROBE_INVALID_HANDLE, metrics, 64);
    EXPECT_EQ(count, 0u);
}

TEST_F(BrainProbeTest, GetMetricsNullBuffer) {
    uint16_t modules[] = {0x0100};
    probe_handle_t h = probe_create_active(reg, "test", modules, 1,
                                            dummy_sampler, nullptr, 1000);
    uint32_t count = probe_get_metrics(reg, h, nullptr, 64);
    EXPECT_EQ(count, 0u);
}

/* ============================================================================
 * Built-in Samplers
 * ============================================================================ */

TEST_F(BrainProbeTest, AttachNetworkMetrics) {
    probe_handle_t h = probe_attach_network_metrics(reg, 1000);
    EXPECT_NE(h, PROBE_INVALID_HANDLE);
}

TEST_F(BrainProbeTest, AttachCognitiveStats) {
    probe_handle_t h = probe_attach_cognitive_stats(reg, 1000);
    EXPECT_NE(h, PROBE_INVALID_HANDLE);
}

TEST_F(BrainProbeTest, AttachTrainingDashboard) {
    probe_handle_t h = probe_attach_training_dashboard(reg, 1000);
    EXPECT_NE(h, PROBE_INVALID_HANDLE);
}

TEST_F(BrainProbeTest, AttachInference) {
    probe_handle_t h = probe_attach_inference(reg, 1000);
    EXPECT_NE(h, PROBE_INVALID_HANDLE);
}

/* ============================================================================
 * Module Resolution
 * ============================================================================ */

TEST_F(BrainProbeTest, ResolveModuleBrain) {
    void* ptr = probe_resolve_module(brain, 0x0100);
    EXPECT_EQ(ptr, brain);
}

TEST_F(BrainProbeTest, ResolveModuleNull) {
    void* ptr = probe_resolve_module(nullptr, 0x0100);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(BrainProbeTest, ResolveModuleUnknown) {
    void* ptr = probe_resolve_module(brain, 0xFFFF);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(BrainProbeTest, ModuleName) {
    EXPECT_STREQ(probe_module_name(0x0100), "brain");
    EXPECT_STREQ(probe_module_name(0x0210), "attention");
    EXPECT_EQ(probe_module_name(0xFFFF), nullptr);
}

/* ============================================================================
 * JSON Output
 * ============================================================================ */

TEST_F(BrainProbeTest, JsonEmpty) {
    char* json = probe_get_all_metrics_json(reg);
    ASSERT_NE(json, nullptr);
    EXPECT_STREQ(json, "{}");
    nimcp_free(json);
}

TEST_F(BrainProbeTest, JsonNull) {
    char* json = probe_get_all_metrics_json(nullptr);
    EXPECT_EQ(json, nullptr);
}

/* ============================================================================
 * Metric Types
 * ============================================================================ */

TEST_F(BrainProbeTest, MetricFloat) {
    probe_metric_t m = {};
    strncpy(m.key, "test", sizeof(m.key));
    m.type = PROBE_METRIC_FLOAT;
    m.value.f = 3.14f;
    EXPECT_FLOAT_EQ(m.value.f, 3.14f);
}

TEST_F(BrainProbeTest, MetricInt) {
    probe_metric_t m = {};
    strncpy(m.key, "test", sizeof(m.key));
    m.type = PROBE_METRIC_INT;
    m.value.i = 42;
    EXPECT_EQ(m.value.i, 42);
}

TEST_F(BrainProbeTest, MetricString) {
    probe_metric_t m = {};
    strncpy(m.key, "test", sizeof(m.key));
    m.type = PROBE_METRIC_STRING;
    strncpy(m.value.s, "hello", sizeof(m.value.s));
    EXPECT_STREQ(m.value.s, "hello");
}

/* ============================================================================
 * Pipeline Stage Names
 * ============================================================================ */

TEST_F(BrainProbeTest, StageNames) {
    EXPECT_STREQ(probe_stage_name(PROBE_INF_PRE_FORWARD), "inf_pre_forward");
    EXPECT_STREQ(probe_stage_name(PROBE_INF_ADAPTIVE_FWD), "inf_adaptive_fwd");
    EXPECT_STREQ(probe_stage_name(PROBE_INF_DECISION), "inf_decision");
    EXPECT_STREQ(probe_stage_name(PROBE_TRAIN_ADAPTIVE), "train_adaptive");
    EXPECT_STREQ(probe_stage_name(PROBE_TRAIN_COGNITIVE), "train_cognitive");
    EXPECT_STREQ(probe_stage_name(PROBE_STAGE_COUNT), "unknown");
}

TEST_F(BrainProbeTest, AllInferenceStageIds) {
    EXPECT_EQ(PROBE_INF_PRE_FORWARD, 0x00);
    EXPECT_EQ(PROBE_INF_DECISION, 0x0A);
    EXPECT_EQ(PROBE_INF_COGNITIVE, 0x0B);
}

TEST_F(BrainProbeTest, AllTrainingStageIds) {
    EXPECT_EQ(PROBE_TRAIN_INPUT, 0x10);
    EXPECT_EQ(PROBE_TRAIN_WM_CURRICULUM, 0x19);
}

/* ============================================================================
 * PROBE_STAGE Macro — No Registry
 * ============================================================================ */

TEST_F(BrainProbeTest, ProbeStageNoRegistry) {
    /* When probe_registry is NULL, PROBE_STAGE should be a no-op */
    brain->probe_registry = nullptr;
    int x = 0;
    PROBE_STAGE(brain, PROBE_INF_DECISION, {
        x = 1;  /* Should NOT execute */
    });
    EXPECT_EQ(x, 0);
}

TEST_F(BrainProbeTest, ProbeStageWithRegistry) {
    /* When registry exists, PROBE_STAGE should execute and record */
    brain->probe_registry = (struct probe_registry*)reg;
    /* Create a probe that subscribes to stages */
    probe_handle_t h = probe_attach_inference(reg, 1000);
    EXPECT_NE(h, PROBE_INVALID_HANDLE);

    int x = 0;
    PROBE_STAGE(brain, PROBE_INF_DECISION, {
        x = 1;  /* Should execute */
        PROBE_SET_FLOAT(&_ctx, "confidence", 0.85f);
    });
    EXPECT_EQ(x, 1);
}
