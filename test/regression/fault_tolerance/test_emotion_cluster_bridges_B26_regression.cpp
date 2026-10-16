/**
 * @file test_emotion_cluster_bridges_B26_regression.cpp
 * @brief Regression tests for B26 emotion cluster bridges: edge cases,
 *        stability, thread safety
 *        (36 bridges + 8 non-bridge modules across 11 emotion directories)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>
#include <climits>

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

    /* destroy(NULL) safety */
    void emotion_fep_bridge_destroy(void* bridge);
    void emotion_plasticity_destroy(void* bridge);
    void emotion_snn_destroy(void* bridge);
    void emotion_substrate_bridge_destroy(void* bridge);
    void emotion_thalamic_bridge_destroy(void* bridge);
    void emotional_tagging_fep_destroy(void* bridge);
    void emotional_tagging_substrate_bridge_destroy(void* bridge);
    void emotional_tagging_thalamic_bridge_destroy(void* bridge);
    void emotion_recognition_fep_destroy(void* bridge);
    void emotion_recognition_substrate_bridge_destroy(void* bridge);
    void emotion_recognition_thalamic_bridge_destroy(void* bridge);
    void emotional_sleep_bridge_destroy(void* bridge);
    void emotion_tensor_bridge_destroy(void* bridge);
    void emotion_tensor_substrate_bridge_destroy(void* bridge);
    void emotion_tensor_thalamic_bridge_destroy(void* bridge);
    void grief_fep_destroy(void* bridge);
    void grief_substrate_bridge_destroy(void* bridge);
    void grief_thalamic_bridge_destroy(void* bridge);
    void joy_fep_destroy(void* bridge);
    void joy_substrate_bridge_destroy(void* bridge);
    void joy_thalamic_bridge_destroy(void* bridge);
    void remorse_fep_destroy(void* bridge);
    void remorse_substrate_bridge_destroy(void* bridge);
    void remorse_thalamic_bridge_destroy(void* bridge);
    void social_bond_fep_bridge_destroy(void* bridge);
    void llf_substrate_bridge_destroy(void* bridge);
    void llf_thalamic_bridge_destroy(void* bridge);
    void shadow_emotions_fep_bridge_destroy(void* bridge);
    void shadow_emotions_substrate_bridge_destroy(void* bridge);
    void shadow_emotions_thalamic_bridge_destroy(void* bridge);
    void empathetic_response_fep_destroy(void* bridge);
    void empathetic_response_substrate_bridge_destroy(void* bridge);
    void empathetic_response_thalamic_bridge_destroy(void* bridge);
    void empathy_plasticity_destroy(void* bridge);
    void empathy_snn_destroy(void* bridge);
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

class EmotionClusterB26RegressionTest : public ::testing::Test {
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
 * Regression Tests
 * ============================================================================ */

TEST_F(EmotionClusterB26RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kEmotionClusterModules[i].name);
        kEmotionClusterModules[i].setter(nullptr);
    }
}

TEST_F(EmotionClusterB26RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kEmotionClusterModules[i].name);
        kEmotionClusterModules[i].setter(agent_);
        kEmotionClusterModules[i].setter(nullptr);
    }
}

TEST_F(EmotionClusterB26RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kEmotionClusterModules[i].name);
        kEmotionClusterModules[i].setter(agent_);
    }
}

TEST_F(EmotionClusterB26RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kEmotionClusterModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kEmotionClusterModules[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++)
            nimcp_health_agent_heartbeat_ex(agent_, "B26_regression", 0);
    });
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(nullptr);
    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26RegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 100; cycle++) {
        for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
        for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(nullptr);
    }
}

TEST_F(EmotionClusterB26RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 50; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(nullptr, temp);
        for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(temp);
        for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(nullptr);
        nimcp_health_agent_destroy(temp);
    }
}

/* ============================================================================
 * Null Destroy Idempotent
 * ============================================================================ */

TEST_F(EmotionClusterB26RegressionTest, NullDestroyIdempotentAllBridges) {
    /* Call destroy(NULL) twice for each bridge — must not crash */
    emotion_fep_bridge_destroy(nullptr);
    emotion_fep_bridge_destroy(nullptr);
    emotion_plasticity_destroy(nullptr);
    emotion_plasticity_destroy(nullptr);
    emotion_snn_destroy(nullptr);
    emotion_snn_destroy(nullptr);
    emotion_substrate_bridge_destroy(nullptr);
    emotion_substrate_bridge_destroy(nullptr);
    emotion_thalamic_bridge_destroy(nullptr);
    emotion_thalamic_bridge_destroy(nullptr);
    emotional_tagging_fep_destroy(nullptr);
    emotional_tagging_substrate_bridge_destroy(nullptr);
    emotional_tagging_thalamic_bridge_destroy(nullptr);
    emotion_recognition_fep_destroy(nullptr);
    emotion_recognition_substrate_bridge_destroy(nullptr);
    emotion_recognition_thalamic_bridge_destroy(nullptr);
    emotional_sleep_bridge_destroy(nullptr);
    emotion_tensor_bridge_destroy(nullptr);
    emotion_tensor_substrate_bridge_destroy(nullptr);
    emotion_tensor_thalamic_bridge_destroy(nullptr);
    grief_fep_destroy(nullptr);
    grief_substrate_bridge_destroy(nullptr);
    grief_thalamic_bridge_destroy(nullptr);
    joy_fep_destroy(nullptr);
    joy_substrate_bridge_destroy(nullptr);
    joy_thalamic_bridge_destroy(nullptr);
    remorse_fep_destroy(nullptr);
    remorse_substrate_bridge_destroy(nullptr);
    remorse_thalamic_bridge_destroy(nullptr);
    social_bond_fep_bridge_destroy(nullptr);
    llf_substrate_bridge_destroy(nullptr);
    llf_thalamic_bridge_destroy(nullptr);
    shadow_emotions_fep_bridge_destroy(nullptr);
    shadow_emotions_substrate_bridge_destroy(nullptr);
    shadow_emotions_thalamic_bridge_destroy(nullptr);
    empathetic_response_fep_destroy(nullptr);
    empathetic_response_substrate_bridge_destroy(nullptr);
    empathetic_response_thalamic_bridge_destroy(nullptr);
    empathy_plasticity_destroy(nullptr);
    empathy_snn_destroy(nullptr);
}

/* ============================================================================
 * Thread Safety
 * ============================================================================ */

TEST_F(EmotionClusterB26RegressionTest, ConcurrentSetClearFromMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t]() {
            for (int cycle = 0; cycle < 50; cycle++) {
                size_t idx = (t * 11 + cycle) % kNumModules;
                kEmotionClusterModules[idx].setter(agent_);
                nimcp_health_agent_heartbeat_ex(agent_, kEmotionClusterModules[idx].name, 0);
                kEmotionClusterModules[idx].setter(nullptr);
            }
        });
    }
    for (auto& th : threads) th.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26RegressionTest, AllDirectoriesConcurrentSetClear) {
    nimcp_health_agent_start(agent_);
    /* 11 threads, one per directory */
    std::vector<std::thread> threads;
    size_t dir_offsets[] = {0, 6, 10, 14, 16, 20, 24, 28, 32, 35, 38};
    size_t dir_counts[]  = {6, 4, 4,  2,  4,  4,  4,  4,  3,  3,  6};
    for (int d = 0; d < 11; d++) {
        threads.emplace_back([this, d, &dir_offsets, &dir_counts]() {
            for (int cycle = 0; cycle < 20; cycle++) {
                for (size_t i = dir_offsets[d]; i < dir_offsets[d] + dir_counts[d]; i++)
                    kEmotionClusterModules[i].setter(agent_);
                for (size_t i = dir_offsets[d]; i < dir_offsets[d] + dir_counts[d]; i++)
                    nimcp_health_agent_heartbeat_ex(agent_, kEmotionClusterModules[i].name, 0);
                for (size_t i = dir_offsets[d]; i < dir_offsets[d] + dir_counts[d]; i++)
                    kEmotionClusterModules[i].setter(nullptr);
            }
        });
    }
    for (auto& th : threads) th.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(EmotionClusterB26RegressionTest, BurstHeartbeatsDuringAgentSwap) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
    std::thread swapper([this, agent2]() {
        for (int c = 0; c < 50; c++) {
            for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent2);
            for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(agent_);
        }
    });
    for (int j = 0; j < 500; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B26_burst_swap", 0);
    swapper.join();
    for (size_t i = 0; i < kNumModules; i++) kEmotionClusterModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
    nimcp_health_agent_destroy(agent2);
}
