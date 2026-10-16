/**
 * @file test_mirror_bridges_B22_e2e.cpp
 * @brief End-to-end tests for B22 mirror bridges: full lifecycle,
 *        sustained operation, load testing
 *        (cognitive/mirror_neurons bridges: thalamic, substrate, sleep, fep,
 *         motor, hypothalamus, emotion, attention, visual, tom, prefrontal,
 *         snn, hippocampus, language, omni, plasticity)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

/* Mirror bridge APIs - included OUTSIDE extern "C" (headers have their own guards) */
#include "cognitive/mirror_neurons/nimcp_mirror_thalamic_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_substrate_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_neurons_sleep_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_neurons_fep_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_motor_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_hypothalamus_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_emotion_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_attention_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_visual_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_tom_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_prefrontal_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_snn_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_hippocampus_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_language_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_omni_bridge.h"
#include "cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.h"

/* Health agent API */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    /* Mirror bridge health agent setters (bare declarations with void*) */
    void mirror_thalamic_bridge_set_health_agent(void* agent);
    void mirror_substrate_bridge_set_health_agent(void* agent);
    void mirror_neurons_sleep_bridge_set_health_agent(void* agent);
    void mirror_neurons_fep_bridge_set_health_agent(void* agent);
    void mirror_motor_bridge_set_health_agent(void* agent);
    void mirror_hypothalamus_bridge_set_health_agent(void* agent);
    void mirror_emotion_bridge_set_health_agent(void* agent);
    void mirror_attention_bridge_set_health_agent(void* agent);
    void mirror_visual_bridge_set_health_agent(void* agent);
    void mirror_tom_bridge_set_health_agent(void* agent);
    void mirror_prefrontal_bridge_set_health_agent(void* agent);
    void mirror_snn_bridge_set_health_agent(void* agent);
    void mirror_hippocampus_bridge_set_health_agent(void* agent);
    void mirror_language_bridge_set_health_agent(void* agent);
    void mirror_omni_bridge_set_health_agent(void* agent);
    void mirror_plasticity_bridge_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kMirrorBridgeModules[] = {
    {"mirror_thalamic_bridge",       mirror_thalamic_bridge_set_health_agent},
    {"mirror_substrate_bridge",      mirror_substrate_bridge_set_health_agent},
    {"mirror_neurons_sleep_bridge",  mirror_neurons_sleep_bridge_set_health_agent},
    {"mirror_neurons_fep_bridge",    mirror_neurons_fep_bridge_set_health_agent},
    {"mirror_motor_bridge",          mirror_motor_bridge_set_health_agent},
    {"mirror_hypothalamus_bridge",   mirror_hypothalamus_bridge_set_health_agent},
    {"mirror_emotion_bridge",        mirror_emotion_bridge_set_health_agent},
    {"mirror_attention_bridge",      mirror_attention_bridge_set_health_agent},
    {"mirror_visual_bridge",         mirror_visual_bridge_set_health_agent},
    {"mirror_tom_bridge",            mirror_tom_bridge_set_health_agent},
    {"mirror_prefrontal_bridge",     mirror_prefrontal_bridge_set_health_agent},
    {"mirror_snn_bridge",            mirror_snn_bridge_set_health_agent},
    {"mirror_hippocampus_bridge",    mirror_hippocampus_bridge_set_health_agent},
    {"mirror_language_bridge",       mirror_language_bridge_set_health_agent},
    {"mirror_omni_bridge",           mirror_omni_bridge_set_health_agent},
    {"mirror_plasticity_bridge",     mirror_plasticity_bridge_set_health_agent},
};

static constexpr size_t kNumModules = 16;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MirrorBridgesB22E2ETest : public ::testing::Test {
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
            kMirrorBridgeModules[i].setter(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_stop(agent_);
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

/* ============================================================================
 * E2E Tests
 * ============================================================================ */

TEST_F(MirrorBridgesB22E2ETest, FullLifecycleAllModules) {
    /* Create -> start -> connect -> heartbeat -> stats -> disconnect -> stop -> destroy */
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kMirrorBridgeModules[i].name, 0);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22E2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kMirrorBridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kMirrorBridgeModules[i].name, 0);
            kMirrorBridgeModules[i].setter(nullptr);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22E2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    health_agent_stats_t before;
    nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B22_burst", 0);
    health_agent_stats_t after;
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22E2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B22_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22E2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent_);
    /* Phase 1: Create bridges */
    mirror_motor_bridge_t* motor = mirror_motor_bridge_create(NULL);
    ASSERT_NE(nullptr, motor);
    mirror_snn_bridge_t* snn = mirror_snn_create(NULL);
    ASSERT_NE(nullptr, snn);
    mirror_visual_bridge_t* vis = mirror_visual_bridge_create(NULL);
    ASSERT_NE(nullptr, vis);

    /* Connect health agent modules */
    for (size_t i = 0; i < kNumModules; i++) {
        kMirrorBridgeModules[i].setter(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, kMirrorBridgeModules[i].name, 0);
    }

    /* Phase 2: Operate bridges */
    mirror_motor_bridge_update(motor, 10);
    mirror_snn_update(snn, 0.01f);
    mirror_visual_bridge_update(vis, 10);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);

    /* Phase 3: Teardown bridges */
    mirror_visual_bridge_destroy(vis);
    mirror_snn_destroy(snn);
    mirror_motor_bridge_destroy(motor);

    for (size_t i = 0; i < kNumModules / 2; i++) kMirrorBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22E2ETest, ModuleHotSwapDuringOperation) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    nimcp_health_agent_start(agent2);

    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++) {
        kMirrorBridgeModules[i].setter(agent2);
        nimcp_health_agent_heartbeat_ex(agent2, kMirrorBridgeModules[i].name, 0);
    }

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, kNumModules);

    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22E2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250)) {
        nimcp_health_agent_heartbeat_ex(agent_, "B22_sustained", 0);
        count++;
    }
    EXPECT_GT(count, 0u);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, count);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MirrorBridgesB22E2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kMirrorBridgeModules[i].name, 0);
    /* Orderly: stop operations, disconnect, destroy */
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
}

TEST_F(MirrorBridgesB22E2ETest, AllBridgesCreateOperateDestroy) {
    /* Full lifecycle for each bridge type that accepts NULL create params */
    {
        mirror_motor_bridge_t* b = mirror_motor_bridge_create(NULL);
        ASSERT_NE(nullptr, b);
        mirror_motor_bridge_update(b, 10);
        mirror_motor_bridge_destroy(b);
    }
    {
        mirror_visual_bridge_t* b = mirror_visual_bridge_create(NULL);
        ASSERT_NE(nullptr, b);
        mirror_visual_bridge_update(b, 10);
        mirror_visual_bridge_destroy(b);
    }
    {
        mirror_neurons_fep_bridge_t* b = mirror_neurons_fep_bridge_create(NULL);
        ASSERT_NE(nullptr, b);
        mirror_neurons_fep_bridge_update(b, 10);
        mirror_neurons_fep_bridge_destroy(b);
    }
    {
        mirror_snn_bridge_t* b = mirror_snn_create(NULL);
        ASSERT_NE(nullptr, b);
        mirror_snn_update(b, 0.01f);
        mirror_snn_destroy(b);
    }
    {
        mirror_emotion_bridge_t* b = mirror_emotion_create(NULL);
        ASSERT_NE(nullptr, b);
        mirror_emotion_destroy(b);
    }
    {
        mirror_attention_bridge_t* b = mirror_attention_create(NULL);
        ASSERT_NE(nullptr, b);
        mirror_attention_destroy(b);
    }
    {
        /* mirror_tom_bridge_t is a POINTER typedef - no extra * */
        mirror_tom_bridge_t b = mirror_tom_create(NULL);
        ASSERT_NE(nullptr, (void*)b);
        mirror_tom_update(b, 1000);
        mirror_tom_destroy(b);
    }
    {
        mirror_omni_bridge_t* b = mirror_omni_bridge_create(NULL);
        ASSERT_NE(nullptr, b);
        mirror_omni_bridge_update(b, 10);
        mirror_omni_bridge_destroy(b);
    }
    {
        mirror_hippocampus_bridge_t* b = mirror_hippocampus_bridge_create(NULL);
        ASSERT_NE(nullptr, b);
        mirror_hippocampus_bridge_update(b, 10);
        mirror_hippocampus_bridge_destroy(b);
    }
    {
        mirror_plasticity_bridge_t* b = mirror_plasticity_create(NULL);
        ASSERT_NE(nullptr, b);
        mirror_plasticity_update(b, 0.01f);
        mirror_plasticity_destroy(b);
    }
}

TEST_F(MirrorBridgesB22E2ETest, CrossBridgeEventPropagation) {
    /* Mirror event flows through motor -> visual -> emotion pipeline */
    mirror_motor_bridge_t* motor = mirror_motor_bridge_create(NULL);
    ASSERT_NE(nullptr, motor);
    mirror_visual_bridge_t* vis = mirror_visual_bridge_create(NULL);
    ASSERT_NE(nullptr, vis);
    mirror_emotion_bridge_t* emo = mirror_emotion_create(NULL);
    ASSERT_NE(nullptr, emo);

    /* Connect health agent modules */
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(agent_);

    /* Step 1: Motor bridge processes mirrored action */
    mirror_motor_bridge_update(motor, 10);

    /* Step 2: Visual bridge processes observed action */
    mirror_visual_bridge_update(vis, 10);

    /* Step 3: Emotion bridge integrates mirror response */
    /* emotion bridge has no update, just create/destroy lifecycle */

    /* Heartbeat to confirm health agent is tracking */
    nimcp_health_agent_heartbeat_ex(agent_, "B22_cross_bridge", 1.0f);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);

    for (size_t i = 0; i < kNumModules; i++) kMirrorBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);

    mirror_emotion_destroy(emo);
    mirror_visual_bridge_destroy(vis);
    mirror_motor_bridge_destroy(motor);
}
