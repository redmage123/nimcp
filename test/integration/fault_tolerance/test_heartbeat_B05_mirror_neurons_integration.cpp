/**
 * @file test_heartbeat_B05_mirror_neurons_integration.cpp
 * @brief Integration tests for B05 (cognitive/mirror_neurons) heartbeat flow
 *
 * WHAT: Tests heartbeat integration across multiple mirror neuron modules
 * WHY:  Phase 8 requires modules to work together with shared health agents
 * HOW:  Multi-module agent sharing, disconnect-while-running, agent restart
 *
 * @author NIMCP Team
 * @date 2026-01-26
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"

/* B05: cognitive/mirror_neurons — 27 setter declarations */
void mirror_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_emotion_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_habituation_set_health_agent(nimcp_health_agent_t* agent);
void mirror_hierarchy_set_health_agent(nimcp_health_agent_t* agent);
void mirror_hippocampus_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_hypothalamus_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_immune_integration_set_health_agent(nimcp_health_agent_t* agent);
void mirror_language_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_motor_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_multimodal_set_health_agent(nimcp_health_agent_t* agent);
void mirror_neurons_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_neurons_set_health_agent(nimcp_health_agent_t* agent);
void mirror_neurons_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_omni_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_prefrontal_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_resonance_set_health_agent(nimcp_health_agent_t* agent);
void mirror_self_other_set_health_agent(nimcp_health_agent_t* agent);
void mirror_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_social_context_set_health_agent(nimcp_health_agent_t* agent);
void mirror_stdp_set_health_agent(nimcp_health_agent_t* agent);
void mirror_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_substrate_set_health_agent(nimcp_health_agent_t* agent);
void mirror_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_tom_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_vicarious_reward_set_health_agent(nimcp_health_agent_t* agent);
void mirror_visual_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B05ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B05ModuleEntry B05_MODULES[] = {
    {"mirror_attention_bridge",     mirror_attention_bridge_set_health_agent},
    {"mirror_emotion_bridge",       mirror_emotion_bridge_set_health_agent},
    {"mirror_habituation",          mirror_habituation_set_health_agent},
    {"mirror_hierarchy",            mirror_hierarchy_set_health_agent},
    {"mirror_hippocampus_bridge",   mirror_hippocampus_bridge_set_health_agent},
    {"mirror_hypothalamus_bridge",  mirror_hypothalamus_bridge_set_health_agent},
    {"mirror_immune_integration",   mirror_immune_integration_set_health_agent},
    {"mirror_language_bridge",      mirror_language_bridge_set_health_agent},
    {"mirror_motor_bridge",         mirror_motor_bridge_set_health_agent},
    {"mirror_multimodal",           mirror_multimodal_set_health_agent},
    {"mirror_neurons_fep_bridge",   mirror_neurons_fep_bridge_set_health_agent},
    {"mirror_neurons",              mirror_neurons_set_health_agent},
    {"mirror_neurons_sleep_bridge", mirror_neurons_sleep_bridge_set_health_agent},
    {"mirror_omni_bridge",          mirror_omni_bridge_set_health_agent},
    {"mirror_plasticity_bridge",    mirror_plasticity_bridge_set_health_agent},
    {"mirror_prefrontal_bridge",    mirror_prefrontal_bridge_set_health_agent},
    {"mirror_resonance",            mirror_resonance_set_health_agent},
    {"mirror_self_other",           mirror_self_other_set_health_agent},
    {"mirror_snn_bridge",           mirror_snn_bridge_set_health_agent},
    {"mirror_social_context",       mirror_social_context_set_health_agent},
    {"mirror_stdp",                 mirror_stdp_set_health_agent},
    {"mirror_substrate_bridge",     mirror_substrate_bridge_set_health_agent},
    {"mirror_substrate",            mirror_substrate_set_health_agent},
    {"mirror_thalamic_bridge",      mirror_thalamic_bridge_set_health_agent},
    {"mirror_tom_bridge",           mirror_tom_bridge_set_health_agent},
    {"mirror_vicarious_reward",     mirror_vicarious_reward_set_health_agent},
    {"mirror_visual_bridge",        mirror_visual_bridge_set_health_agent},
};

static constexpr size_t B05_MODULE_COUNT = sizeof(B05_MODULES) / sizeof(B05_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB05IntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;
        config.heartbeat_interval_ms = 100;
        config.watchdog_timeout_ms = 500;
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        clear_all_modules();
        if (agent) {
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

TEST_F(HeartbeatB05IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(nimcp_health_agent_is_running(agent));

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_update", B05_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B05_MODULE_COUNT));
}

TEST_F(HeartbeatB05IntegrationTest, MultipleModulesShareSingleAgent) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT / 2; i++) {
        B05_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B05_MODULE_COUNT / 2; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "first_half", 0.5f);
    }

    health_agent_stats_t stats_mid;
    nimcp_health_agent_get_stats(agent, &stats_mid);
    uint64_t first_half_beats = stats_mid.heartbeats_received;
    EXPECT_GE(first_half_beats, static_cast<uint64_t>(B05_MODULE_COUNT / 2));

    for (size_t i = B05_MODULE_COUNT / 2; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }
    for (size_t i = B05_MODULE_COUNT / 2; i < B05_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "second_half", 1.0f);
    }

    health_agent_stats_t stats_final;
    nimcp_health_agent_get_stats(agent, &stats_final);
    EXPECT_GT(stats_final.heartbeats_received, first_half_beats);
}

TEST_F(HeartbeatB05IntegrationTest, DisconnectModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_disconnect", (float)j / 5.0f);
    }

    for (size_t i = 0; i < B05_MODULE_COUNT / 2; i++) {
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(nullptr));
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "post_disconnect", (float)j / 5.0f);
    }

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 10u);
}

TEST_F(HeartbeatB05IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        EXPECT_NO_FATAL_FAILURE(B05_MODULES[i].set_fn(nullptr));
    }
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB05IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (int j = 0; j < 10; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_restart", 0.5f);
    }

    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);

    result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (int j = 0; j < 10; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "post_restart", 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 20u);
}

TEST_F(HeartbeatB05IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    std::atomic<bool> running{true};
    std::thread heartbeat_thread([this, &running]() {
        while (running.load()) {
            nimcp_health_agent_heartbeat_ex(agent, "background", 0.5f);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    for (int round = 0; round < 3; round++) {
        for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
            B05_MODULES[i].set_fn(agent);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
            B05_MODULES[i].set_fn(nullptr);
        }
    }

    running.store(false);
    heartbeat_thread.join();
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB05IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    config2.check_interval_ms = 50;
    config2.heartbeat_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);
    result = nimcp_health_agent_start(agent2);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent2);
    }

    for (int j = 0; j < 10; j++) {
        nimcp_health_agent_heartbeat_ex(agent2, "new_agent", 1.0f);
    }

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, 10u);

    clear_all_modules();
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(HeartbeatB05IntegrationTest, ProgressiveModuleConnection) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B05_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B05_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
