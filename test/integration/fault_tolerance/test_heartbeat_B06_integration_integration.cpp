/**
 * @file test_heartbeat_B06_integration_integration.cpp
 * @brief Integration tests for B06 (cognitive/integration) heartbeat flow
 *
 * WHAT: Tests heartbeat integration across 24 cognitive integration modules
 * WHY:  Phase 8 requires modules to work together with shared health agents
 * HOW:  Multi-module agent sharing, disconnect-while-running, agent restart
 *
 * NOTE: cognitive_bio_async_bridge now included (mutex bug fixed, added to build)
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

/* B06: cognitive/integration — 24 setter declarations */
void attention_wm_bridge_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_bio_async_bridge_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_integration_fep_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_integration_hub_set_health_agent(nimcp_health_agent_t* agent);
void collective_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_reasoning_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_executive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_memory_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_executive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_executive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_executive_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void gw_cognitive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_reasoning_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_reasoning_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_empathy_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_empathy_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_attention_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_attention_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void security_cognitive_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_introspection_bridge_set_health_agent(nimcp_health_agent_t* agent);
void tom_social_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B06ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B06ModuleEntry B06_MODULES[] = {
    {"attention_wm_bridge",                  attention_wm_bridge_set_health_agent},
    {"cognitive_bio_async_bridge",           cognitive_bio_async_bridge_set_health_agent},
    {"cognitive_integration_fep",            cognitive_integration_fep_set_health_agent},
    {"cognitive_integration_hub",            cognitive_integration_hub_set_health_agent},
    {"collective_hub_bridge",                collective_hub_bridge_set_health_agent},
    {"curiosity_reasoning_bridge",           curiosity_reasoning_bridge_set_health_agent},
    {"emotion_executive_bridge",             emotion_executive_bridge_set_health_agent},
    {"emotion_memory_bridge",                emotion_memory_bridge_set_health_agent},
    {"ethics_executive_bridge",              ethics_executive_bridge_set_health_agent},
    {"game_theory_executive_bridge",         game_theory_executive_bridge_set_health_agent},
    {"game_theory_executive_fep_bridge",     game_theory_executive_fep_bridge_set_health_agent},
    {"gw_cognitive_bridge",                  gw_cognitive_bridge_set_health_agent},
    {"imagination_reasoning_bridge",         imagination_reasoning_bridge_set_health_agent},
    {"imagination_reasoning_fep_bridge",     imagination_reasoning_fep_bridge_set_health_agent},
    {"mirror_empathy_bridge",                mirror_empathy_bridge_set_health_agent},
    {"mirror_empathy_fep_bridge",            mirror_empathy_fep_bridge_set_health_agent},
    {"predictive_attention_bridge",          predictive_attention_bridge_set_health_agent},
    {"predictive_attention_fep_bridge",      predictive_attention_fep_bridge_set_health_agent},
    {"rcog_hub_bridge",                      rcog_hub_bridge_set_health_agent},
    {"salience_attention_bridge",            salience_attention_bridge_set_health_agent},
    {"salience_attention_fep_bridge",        salience_attention_fep_bridge_set_health_agent},
    {"security_cognitive_hub_bridge",        security_cognitive_hub_bridge_set_health_agent},
    {"self_introspection_bridge",            self_introspection_bridge_set_health_agent},
    {"tom_social_bridge",                    tom_social_bridge_set_health_agent},
};

static constexpr size_t B06_MODULE_COUNT = sizeof(B06_MODULES) / sizeof(B06_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB06IntegrationTest : public ::testing::Test {
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

TEST_F(HeartbeatB06IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(nimcp_health_agent_is_running(agent));

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_update", B06_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B06_MODULE_COUNT));
}

TEST_F(HeartbeatB06IntegrationTest, MultipleModulesShareSingleAgent) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT / 2; i++) {
        B06_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B06_MODULE_COUNT / 2; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "first_half", 0.5f);
    }

    health_agent_stats_t stats_mid;
    nimcp_health_agent_get_stats(agent, &stats_mid);
    uint64_t first_half_beats = stats_mid.heartbeats_received;
    EXPECT_GE(first_half_beats, static_cast<uint64_t>(B06_MODULE_COUNT / 2));

    for (size_t i = B06_MODULE_COUNT / 2; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }
    for (size_t i = B06_MODULE_COUNT / 2; i < B06_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "second_half", 1.0f);
    }

    health_agent_stats_t stats_final;
    nimcp_health_agent_get_stats(agent, &stats_final);
    EXPECT_GT(stats_final.heartbeats_received, first_half_beats);
}

TEST_F(HeartbeatB06IntegrationTest, DisconnectModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_disconnect", (float)j / 5.0f);
    }

    for (size_t i = 0; i < B06_MODULE_COUNT / 2; i++) {
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(nullptr));
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "post_disconnect", (float)j / 5.0f);
    }

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 10u);
}

TEST_F(HeartbeatB06IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        EXPECT_NO_FATAL_FAILURE(B06_MODULES[i].set_fn(nullptr));
    }
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB06IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
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

TEST_F(HeartbeatB06IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
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
        for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
            B06_MODULES[i].set_fn(agent);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
            B06_MODULES[i].set_fn(nullptr);
        }
    }

    running.store(false);
    heartbeat_thread.join();
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB06IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    config2.check_interval_ms = 50;
    config2.heartbeat_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);
    result = nimcp_health_agent_start(agent2);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent2);
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

TEST_F(HeartbeatB06IntegrationTest, ProgressiveModuleConnection) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B06_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B06_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
