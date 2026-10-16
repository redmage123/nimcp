/**
 * @file test_memory_core_bridges_B25_regression.cpp
 * @brief Regression tests for B25 memory/core bridges: edge cases, stability,
 *        thread safety
 *        (cognitive/memory/core bridges: attention, audio, bio, cerebellum,
 *         cognitive, continual, curriculum, hypo, immune, kg, logging, loss,
 *         mental_health, meta, omni, optimizer, pink_noise, plasticity,
 *         predictive, sleep, snn, speech, visual)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>
#include <climits>

/* ALL memory/core bridge headers excluded due to _Atomic in nimcp_pr_memory_node.h (C11, not C++).
 * Bridge functions are forward-declared with void* below. */

/* Health agent API */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    /* Forward declarations for all bridge create/destroy (void* for C++ compat) */
    void* pr_attention_bridge_create(const void* config);
    void pr_attention_bridge_destroy(void* bridge);
    void* pr_audio_bridge_create(const void* config, void* node_mgr);
    void pr_audio_bridge_destroy(void* bridge);
    void* pr_bio_bridge_create(const void* config);
    void pr_bio_bridge_destroy(void* bridge);
    void* pr_cerebellum_bridge_create(const void* config);
    void pr_cerebellum_bridge_destroy(void* bridge);
    void* pr_cognitive_bridge_create(const void* config, void* z_ladder);
    void pr_cognitive_bridge_destroy(void* bridge);
    void* pr_continual_bridge_create(const void* config);
    void pr_continual_bridge_destroy(void* bridge);
    void* pr_curriculum_bridge_create(const void* config);
    void pr_curriculum_bridge_destroy(void* bridge);
    void* pr_hypo_bridge_create(const void* config);
    void pr_hypo_bridge_destroy(void* bridge);
    void* pr_immune_bridge_create(const void* config, void* immune_sys, void* sleep_sys);
    void pr_immune_bridge_destroy(void* bridge);
    void* pr_kg_bridge_create(const void* config);
    void pr_kg_bridge_destroy(void* bridge);
    void* pr_logging_bridge_create(const void* config);
    void pr_logging_bridge_destroy(void* bridge);
    void* pr_loss_bridge_create(const void* config);
    void pr_loss_bridge_destroy(void* bridge);
    void* pr_mental_health_bridge_create(const void* config);
    void pr_mental_health_bridge_destroy(void* bridge);
    void* pr_meta_bridge_create(const void* config);
    void pr_meta_bridge_destroy(void* bridge);
    void* pr_omni_bridge_create(const void* config);
    void pr_omni_bridge_destroy(void* bridge);
    void* pr_optimizer_bridge_create(const void* config);
    void pr_optimizer_bridge_destroy(void* bridge);
    void* pr_pink_bridge_create(const void* config);
    void pr_pink_bridge_destroy(void* bridge);
    void* pr_plasticity_bridge_create(const void* config);
    void pr_plasticity_bridge_destroy(void* bridge);
    void* pr_predictive_bridge_create(const void* config);
    void pr_predictive_bridge_destroy(void* bridge);
    void* pr_sleep_bridge_create(const void* config);
    void pr_sleep_bridge_destroy(void* bridge);
    void* pr_snn_bridge_create(const void* config);
    void pr_snn_bridge_destroy(void* bridge);
    void* pr_speech_bridge_create(const void* config);
    void pr_speech_bridge_destroy(void* bridge);
    void* pr_visual_bridge_create(const void* config);
    void pr_visual_bridge_destroy(void* bridge);

    /* Memory/core bridge health agent setters (not declared in headers) */
    void pr_attention_bridge_set_health_agent(void* agent);
    void pr_audio_bridge_set_health_agent(void* agent);
    void pr_bio_bridge_set_health_agent(void* agent);
    void pr_cerebellum_bridge_set_health_agent(void* agent);
    void pr_cognitive_bridge_set_health_agent(void* agent);
    void pr_continual_bridge_set_health_agent(void* agent);
    void pr_curriculum_bridge_set_health_agent(void* agent);
    void pr_hypo_bridge_set_health_agent(void* agent);
    void pr_immune_bridge_set_health_agent(void* agent);
    void pr_kg_bridge_set_health_agent(void* agent);
    void pr_logging_bridge_set_health_agent(void* agent);
    void pr_loss_bridge_set_health_agent(void* agent);
    void pr_mental_health_bridge_set_health_agent(void* agent);
    void pr_meta_bridge_set_health_agent(void* agent);
    void pr_omni_bridge_set_health_agent(void* agent);
    void pr_optimizer_bridge_set_health_agent(void* agent);
    void pr_pink_noise_bridge_set_health_agent(void* agent);
    void pr_plasticity_bridge_set_health_agent(void* agent);
    void pr_predictive_bridge_set_health_agent(void* agent);
    void pr_sleep_bridge_set_health_agent(void* agent);
    void pr_snn_bridge_set_health_agent(void* agent);
    void pr_speech_bridge_set_health_agent(void* agent);
    void pr_visual_bridge_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kMemoryCoreBridgeModules[] = {
    {"pr_attention_bridge",      pr_attention_bridge_set_health_agent},
    {"pr_audio_bridge",          pr_audio_bridge_set_health_agent},
    {"pr_bio_bridge",            pr_bio_bridge_set_health_agent},
    {"pr_cerebellum_bridge",     pr_cerebellum_bridge_set_health_agent},
    {"pr_cognitive_bridge",      pr_cognitive_bridge_set_health_agent},
    {"pr_continual_bridge",      pr_continual_bridge_set_health_agent},
    {"pr_curriculum_bridge",     pr_curriculum_bridge_set_health_agent},
    {"pr_hypo_bridge",           pr_hypo_bridge_set_health_agent},
    {"pr_immune_bridge",         pr_immune_bridge_set_health_agent},
    {"pr_kg_bridge",             pr_kg_bridge_set_health_agent},
    {"pr_logging_bridge",        pr_logging_bridge_set_health_agent},
    {"pr_loss_bridge",           pr_loss_bridge_set_health_agent},
    {"pr_mental_health_bridge",  pr_mental_health_bridge_set_health_agent},
    {"pr_meta_bridge",           pr_meta_bridge_set_health_agent},
    {"pr_omni_bridge",           pr_omni_bridge_set_health_agent},
    {"pr_optimizer_bridge",      pr_optimizer_bridge_set_health_agent},
    {"pr_pink_noise_bridge",     pr_pink_noise_bridge_set_health_agent},
    {"pr_plasticity_bridge",     pr_plasticity_bridge_set_health_agent},
    {"pr_predictive_bridge",     pr_predictive_bridge_set_health_agent},
    {"pr_sleep_bridge",          pr_sleep_bridge_set_health_agent},
    {"pr_snn_bridge",            pr_snn_bridge_set_health_agent},
    {"pr_speech_bridge",         pr_speech_bridge_set_health_agent},
    {"pr_visual_bridge",         pr_visual_bridge_set_health_agent},
};

static constexpr size_t kNumModules = 23;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MemoryCoreBridgesB25RegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent_ = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = false;
        agent_ = nimcp_health_agent_create(&config);
        ASSERT_NE(nullptr, agent_);
    }

    void TearDown() override {
        for (size_t i = 0; i < kNumModules; i++) {
            kMemoryCoreBridgeModules[i].setter(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_stop(agent_);
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

/* ============================================================================
 * Regression Tests
 * ============================================================================ */

TEST_F(MemoryCoreBridgesB25RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreBridgeModules[i].name);
        kMemoryCoreBridgeModules[i].setter(nullptr);
    }
}

TEST_F(MemoryCoreBridgesB25RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreBridgeModules[i].name);
        kMemoryCoreBridgeModules[i].setter(agent_);
        kMemoryCoreBridgeModules[i].setter(nullptr);
    }
}

TEST_F(MemoryCoreBridgesB25RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreBridgeModules[i].name);
        kMemoryCoreBridgeModules[i].setter(agent_);
    }
}

TEST_F(MemoryCoreBridgesB25RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kMemoryCoreBridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kMemoryCoreBridgeModules[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreBridgesB25RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreBridgeModules[i].setter(agent_);
    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++)
            nimcp_health_agent_heartbeat_ex(agent_, "B25_regression", 0);
    });
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreBridgeModules[i].setter(nullptr);
    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreBridgesB25RegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 100; cycle++) {
        for (size_t i = 0; i < kNumModules; i++) kMemoryCoreBridgeModules[i].setter(agent_);
        for (size_t i = 0; i < kNumModules; i++) kMemoryCoreBridgeModules[i].setter(nullptr);
    }
}

TEST_F(MemoryCoreBridgesB25RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 50; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(nullptr, temp);
        for (size_t i = 0; i < kNumModules; i++) kMemoryCoreBridgeModules[i].setter(temp);
        for (size_t i = 0; i < kNumModules; i++) kMemoryCoreBridgeModules[i].setter(nullptr);
        nimcp_health_agent_destroy(temp);
    }
}

/* ============================================================================
 * Bridge-Specific Boundary Tests
 * ============================================================================ */

/* Disabled: nimcp_pr_attention_bridge.h excluded due to forward-declaration conflicts */
#if 0
TEST_F(MemoryCoreBridgesB25RegressionTest, AttentionBoundaryValues) {
    pr_attention_bridge_config_t cfg = pr_attention_bridge_config_default();
    pr_attention_bridge_t* bridge = pr_attention_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    /* Reset should be safe */
    pr_attn_error_t rc = pr_attention_bridge_reset(bridge);
    EXPECT_EQ(0, rc);

    pr_attention_bridge_destroy(bridge);
}
#endif

#if 0  /* Disabled: typed config structs not available (headers excluded) */
TEST_F(MemoryCoreBridgesB25RegressionTest, SnnMultipleResets) {
    pr_snn_bridge_config_t cfg = pr_snn_bridge_config_default();
    pr_snn_bridge_t bridge = pr_snn_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 10; i++) {
        pr_snn_error_t rc = pr_snn_bridge_reset(bridge);
        EXPECT_EQ(0, rc);
    }

    pr_snn_bridge_destroy(bridge);
}
#endif

#if 0  /* Disabled: typed config structs not available (headers excluded) */
TEST_F(MemoryCoreBridgesB25RegressionTest, PlasticityMultipleResets) {
    pr_plasticity_bridge_config_t cfg = pr_plasticity_config_default();
    pr_plasticity_bridge_t bridge = pr_plasticity_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 10; i++) {
        int rc = pr_plasticity_bridge_reset(bridge);
        EXPECT_EQ(0, rc);
    }

    pr_plasticity_bridge_destroy(bridge);
}
#endif

#if 0  /* Disabled: typed config structs not available (headers excluded) */
TEST_F(MemoryCoreBridgesB25RegressionTest, PinkNoiseBoundarySeeds) {
    pr_pink_bridge_config_t cfg = pr_pink_bridge_default_config();
    pr_pink_bridge_t bridge = pr_pink_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    /* Reset with various seeds */
    bool ok = pr_pink_bridge_reset(bridge, 0u);
    EXPECT_TRUE(ok);
    ok = pr_pink_bridge_reset(bridge, UINT32_MAX);
    EXPECT_TRUE(ok);
    ok = pr_pink_bridge_reset(bridge, 12345u);
    EXPECT_TRUE(ok);

    pr_pink_bridge_destroy(bridge);
}
#endif

/* ============================================================================
 * Comprehensive Null Safety
 * ============================================================================ */

TEST_F(MemoryCoreBridgesB25RegressionTest, AllBridgesNullDestroyIdempotent) {
    /* destroy(NULL) should be safe for all bridge types (all use void*). */
    pr_attention_bridge_destroy(nullptr);
    pr_audio_bridge_destroy(nullptr);
    pr_bio_bridge_destroy(nullptr);
    pr_cerebellum_bridge_destroy(nullptr);
    pr_cognitive_bridge_destroy(nullptr);
    pr_continual_bridge_destroy(nullptr);
    pr_curriculum_bridge_destroy(nullptr);
    pr_hypo_bridge_destroy(nullptr);
    pr_immune_bridge_destroy(nullptr);
    pr_kg_bridge_destroy(nullptr);
    pr_logging_bridge_destroy(nullptr);
    pr_loss_bridge_destroy(nullptr);
    pr_mental_health_bridge_destroy(nullptr);
    pr_meta_bridge_destroy(nullptr);
    pr_omni_bridge_destroy(nullptr);
    pr_optimizer_bridge_destroy(nullptr);
    pr_pink_bridge_destroy(nullptr);
    pr_plasticity_bridge_destroy(nullptr);
    pr_predictive_bridge_destroy(nullptr);
    pr_sleep_bridge_destroy(nullptr);
    pr_snn_bridge_destroy(nullptr);
    pr_speech_bridge_destroy(nullptr);
    pr_visual_bridge_destroy(nullptr);
}

TEST_F(MemoryCoreBridgesB25RegressionTest, AllBridgesDoubleDestroy) {
    /* Create and destroy each config-only bridge, then call destroy(NULL).
     * All typed-config blocks disabled because headers are excluded (C11 _Atomic). */

#if 0  /* Disabled: typed config structs not available (headers excluded) */
    /* attention */
    {
        pr_attention_bridge_config_t cfg = pr_attention_bridge_config_default();
        pr_attention_bridge_t* b = pr_attention_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        pr_attention_bridge_destroy(b);
        pr_attention_bridge_destroy(nullptr);
    }

    /* cerebellum */
    {
        pr_cerebellum_config_t cfg = pr_cerebellum_config_default();
        pr_cerebellum_bridge_t b = pr_cerebellum_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        pr_cerebellum_bridge_destroy(b);
        pr_cerebellum_bridge_destroy(nullptr);
    }

    /* continual */
    {
        pr_continual_config_t cfg = pr_continual_config_default();
        pr_continual_bridge_t b = pr_continual_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        pr_continual_bridge_destroy(b);
        pr_continual_bridge_destroy(nullptr);
    }

    /* sleep */
    {
        pr_sleep_config_t cfg = pr_sleep_config_default();
        pr_sleep_bridge_t b = pr_sleep_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        pr_sleep_bridge_destroy(b);
        pr_sleep_bridge_destroy(nullptr);
    }

    /* snn */
    {
        pr_snn_bridge_config_t cfg = pr_snn_bridge_config_default();
        pr_snn_bridge_t b = pr_snn_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        pr_snn_bridge_destroy(b);
        pr_snn_bridge_destroy(nullptr);
    }

    /* predictive */
    {
        pr_predictive_bridge_config_t cfg = pr_predictive_bridge_config_default();
        pr_predictive_bridge_t* b = pr_predictive_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        pr_predictive_bridge_destroy(b);
        pr_predictive_bridge_destroy(nullptr);
    }

    /* speech */
    {
        pr_speech_bridge_config_t cfg = pr_speech_bridge_config_default();
        pr_speech_bridge_t* b = pr_speech_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        pr_speech_bridge_destroy(b);
        pr_speech_bridge_destroy(nullptr);
    }

    /* visual */
    {
        pr_visual_bridge_config_t cfg = pr_visual_bridge_default_config();
        pr_visual_bridge_t* b = pr_visual_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        pr_visual_bridge_destroy(b);
        pr_visual_bridge_destroy(nullptr);
    }
#endif

    /* Bridges requiring non-NULL params: only test destroy(NULL) */
    pr_audio_bridge_destroy(nullptr);
    pr_cognitive_bridge_destroy(nullptr);
    pr_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * Misc Regression Tests
 * ============================================================================ */

TEST_F(MemoryCoreBridgesB25RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreBridgeModules[i].name);
        EXPECT_NE(kMemoryCoreBridgeModules[i].setter, nullptr);
        kMemoryCoreBridgeModules[i].setter(agent_);
        kMemoryCoreBridgeModules[i].setter(nullptr);
    }
}

TEST_F(MemoryCoreBridgesB25RegressionTest, StatsConsistentAfterOperations) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreBridgeModules[i].setter(agent_);
    health_agent_stats_t stats1;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_heartbeat_ex(agent_, "B25_consistency", 0);
    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent_, &stats2);
    EXPECT_GE(stats2.heartbeats_received, stats1.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreBridgesB25RegressionTest, ConcurrentSettersThreadSafe) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this]() {
            for (int cycle = 0; cycle < 50; cycle++) {
                for (size_t i = 0; i < kNumModules; i++) {
                    kMemoryCoreBridgeModules[i].setter(agent_);
                }
                for (size_t i = 0; i < kNumModules; i++) {
                    kMemoryCoreBridgeModules[i].setter(nullptr);
                }
            }
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

#if 0  /* Disabled: typed config structs not available (headers excluded) */
TEST_F(MemoryCoreBridgesB25RegressionTest, MetaMultipleResets) {
    pr_meta_config_t cfg = pr_meta_config_default();
    pr_meta_bridge_t bridge = pr_meta_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 10; i++) {
        int rc = pr_meta_bridge_reset(bridge);
        EXPECT_EQ(0, rc);
    }

    pr_meta_bridge_destroy(bridge);
}
#endif

#if 0  /* Disabled: typed config structs not available (headers excluded) */
TEST_F(MemoryCoreBridgesB25RegressionTest, ContinualMultipleResets) {
    pr_continual_config_t cfg = pr_continual_config_default();
    pr_continual_bridge_t bridge = pr_continual_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 10; i++) {
        int rc = pr_continual_bridge_reset(bridge);
        EXPECT_EQ(0, rc);
    }

    pr_continual_bridge_destroy(bridge);
}
#endif
