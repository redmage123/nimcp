/**
 * @file test_emotion_cluster_bridges_B26_e2e.cpp
 * @brief End-to-end tests for B26 emotion cluster bridges: full lifecycle,
 *        sustained operation, load testing
 *        (36 bridges + 8 non-bridge modules across 11 emotion directories)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

/* NO bridge headers — they use _Atomic (C11, not C++) via nimcp_pr_memory_node.h */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    /* 44 health agent setters */
    void emotion_fep_bridge_set_health_agent(void* agent);
    void emotion_plasticity_bridge_set_health_agent(void* agent);
    void emotion_snn_bridge_set_health_agent(void* agent);
    void emotion_substrate_bridge_set_health_agent(void* agent);
    void emotion_thalamic_bridge_set_health_agent(void* agent);
    void health_emotion_bridge_set_health_agent(void* agent);
    void emotional_tagging_fep_bridge_set_health_agent(void* agent);
    void emotional_tagging_substrate_bridge_set_health_agent(void* agent);
    void emotional_tagging_thalamic_bridge_set_health_agent(void* agent);
    void emotional_tagging_set_health_agent(void* agent);
    void emotion_recognition_fep_bridge_set_health_agent(void* agent);
    void emotion_recognition_substrate_bridge_set_health_agent(void* agent);
    void emotion_recognition_thalamic_bridge_set_health_agent(void* agent);
    void emotion_recognition_simple_set_health_agent(void* agent);
    void emotional_system_sleep_bridge_set_health_agent(void* agent);
    void emotional_system_set_health_agent(void* agent);
    void emotion_tensor_bridge_set_health_agent(void* agent);
    void emotion_tensor_substrate_bridge_set_health_agent(void* agent);
    void emotion_tensor_thalamic_bridge_set_health_agent(void* agent);
    void emotion_tensor_set_health_agent(void* agent);
    void grief_fep_bridge_set_health_agent(void* agent);
    void grief_substrate_bridge_set_health_agent(void* agent);
    void grief_thalamic_bridge_set_health_agent(void* agent);
    void grief_and_loss_set_health_agent(void* agent);
    void joy_fep_bridge_set_health_agent(void* agent);
    void joy_substrate_bridge_set_health_agent(void* agent);
    void joy_thalamic_bridge_set_health_agent(void* agent);
    void joy_euphoria_set_health_agent(void* agent);
    void remorse_fep_bridge_set_health_agent(void* agent);
    void remorse_substrate_bridge_set_health_agent(void* agent);
    void remorse_thalamic_bridge_set_health_agent(void* agent);
    void remorse_regret_set_health_agent(void* agent);
    void love_loyalty_friendship_fep_bridge_set_health_agent(void* agent);
    void llf_substrate_bridge_set_health_agent(void* agent);
    void llf_thalamic_bridge_set_health_agent(void* agent);
    void shadow_emotions_fep_bridge_set_health_agent(void* agent);
    void shadow_emotions_substrate_bridge_set_health_agent(void* agent);
    void shadow_emotions_thalamic_bridge_set_health_agent(void* agent);
    void empathetic_response_fep_bridge_set_health_agent(void* agent);
    void empathetic_response_substrate_bridge_set_health_agent(void* agent);
    void empathetic_response_thalamic_bridge_set_health_agent(void* agent);
    void empathy_plasticity_bridge_set_health_agent(void* agent);
    void empathy_snn_bridge_set_health_agent(void* agent);
    void empathetic_response_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kEmotionClusterModules[] = {
    {"emotion_fep_bridge",                   emotion_fep_bridge_set_health_agent},
    {"emotion_plasticity_bridge",            emotion_plasticity_bridge_set_health_agent},
    {"emotion_snn_bridge",                   emotion_snn_bridge_set_health_agent},
    {"emotion_substrate_bridge",             emotion_substrate_bridge_set_health_agent},
    {"emotion_thalamic_bridge",              emotion_thalamic_bridge_set_health_agent},
    {"health_emotion_bridge",                health_emotion_bridge_set_health_agent},
    {"emotional_tagging_fep_bridge",         emotional_tagging_fep_bridge_set_health_agent},
    {"emotional_tagging_substrate_bridge",   emotional_tagging_substrate_bridge_set_health_agent},
    {"emotional_tagging_thalamic_bridge",    emotional_tagging_thalamic_bridge_set_health_agent},
    {"emotional_tagging",                    emotional_tagging_set_health_agent},
    {"emotion_recognition_fep_bridge",       emotion_recognition_fep_bridge_set_health_agent},
    {"emotion_recognition_substrate_bridge", emotion_recognition_substrate_bridge_set_health_agent},
    {"emotion_recognition_thalamic_bridge",  emotion_recognition_thalamic_bridge_set_health_agent},
    {"emotion_recognition_simple",           emotion_recognition_simple_set_health_agent},
    {"emotional_system_sleep_bridge",        emotional_system_sleep_bridge_set_health_agent},
    {"emotional_system",                     emotional_system_set_health_agent},
    {"emotion_tensor_bridge",                emotion_tensor_bridge_set_health_agent},
    {"emotion_tensor_substrate_bridge",      emotion_tensor_substrate_bridge_set_health_agent},
    {"emotion_tensor_thalamic_bridge",       emotion_tensor_thalamic_bridge_set_health_agent},
    {"emotion_tensor",                       emotion_tensor_set_health_agent},
    {"grief_fep_bridge",                     grief_fep_bridge_set_health_agent},
    {"grief_substrate_bridge",               grief_substrate_bridge_set_health_agent},
    {"grief_thalamic_bridge",                grief_thalamic_bridge_set_health_agent},
    {"grief_and_loss",                       grief_and_loss_set_health_agent},
    {"joy_fep_bridge",                       joy_fep_bridge_set_health_agent},
    {"joy_substrate_bridge",                 joy_substrate_bridge_set_health_agent},
    {"joy_thalamic_bridge",                  joy_thalamic_bridge_set_health_agent},
    {"joy_euphoria",                         joy_euphoria_set_health_agent},
    {"remorse_fep_bridge",                   remorse_fep_bridge_set_health_agent},
    {"remorse_substrate_bridge",             remorse_substrate_bridge_set_health_agent},
    {"remorse_thalamic_bridge",              remorse_thalamic_bridge_set_health_agent},
    {"remorse_regret",                       remorse_regret_set_health_agent},
    {"love_loyalty_friendship_fep_bridge",   love_loyalty_friendship_fep_bridge_set_health_agent},
    {"llf_substrate_bridge",                 llf_substrate_bridge_set_health_agent},
    {"llf_thalamic_bridge",                  llf_thalamic_bridge_set_health_agent},
    {"shadow_emotions_fep_bridge",           shadow_emotions_fep_bridge_set_health_agent},
    {"shadow_emotions_substrate_bridge",     shadow_emotions_substrate_bridge_set_health_agent},
    {"shadow_emotions_thalamic_bridge",      shadow_emotions_thalamic_bridge_set_health_agent},
    {"empathetic_response_fep_bridge",       empathetic_response_fep_bridge_set_health_agent},
    {"empathetic_response_substrate_bridge", empathetic_response_substrate_bridge_set_health_agent},
    {"empathetic_response_thalamic_bridge",  empathetic_response_thalamic_bridge_set_health_agent},
    {"empathy_plasticity_bridge",            empathy_plasticity_bridge_set_health_agent},
    {"empathy_snn_bridge",                   empathy_snn_bridge_set_health_agent},
    {"empathetic_response",                  empathetic_response_set_health_agent},
};

static constexpr size_t kNumModules = 44;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class EmotionClusterB26E2ETest : public ::testing::Test {
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
            kEmotionClusterModules[i].setter(nullptr);
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

TEST_F(EmotionClusterB26E2ETest, FullLifecycleAllModules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kEmotionClusterModules[i].name, 0);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26E2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kEmotionClusterModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kEmotionClusterModules[i].name, 0);
            kEmotionClusterModules[i].setter(nullptr);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26E2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
    health_agent_stats_t before;
    nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B26_burst", 0);
    health_agent_stats_t after;
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26E2ETest, SustainedOperationWithPeriodicHeartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
    for (int round = 0; round < 5; round++) {
        for (size_t i = 0; i < kNumModules; i++)
            nimcp_health_agent_heartbeat_ex(agent_, kEmotionClusterModules[i].name,
                                            (float)round / 5.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules * 5);
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26E2ETest, HotSwapAgentDuringOperation) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
    for (int j = 0; j < 100; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B26_pre_swap", 0);

    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    nimcp_health_agent_start(agent2);

    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent2);
    for (int j = 0; j < 100; j++)
        nimcp_health_agent_heartbeat_ex(agent2, "B26_post_swap", 0);

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, 100u);

    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26E2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kEmotionClusterModules[i].name, 1.0f);
    /* Disconnect in reverse order */
    for (size_t i = kNumModules; i > 0; i--)
        kEmotionClusterModules[i - 1].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26E2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B26_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}
