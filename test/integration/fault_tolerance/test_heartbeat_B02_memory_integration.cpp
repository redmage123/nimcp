/**
 * @file test_heartbeat_B02_memory_integration.cpp
 * @brief Integration tests for B02 (cognitive/memory non-core) heartbeat flow
 *
 * WHAT: Tests heartbeat integration across multiple memory (non-core) modules
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

/* B02: cognitive/memory (non-core) — 12 setter declarations */
void engram_set_health_agent(nimcp_health_agent_t* agent);
void hopfield_memory_set_health_agent(nimcp_health_agent_t* agent);
void memory_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void memory_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void memory_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void semantic_memory_set_health_agent(nimcp_health_agent_t* agent);
void systems_consolidation_set_health_agent(nimcp_health_agent_t* agent);
void systems_consolidation_pink_noise_bridge_set_health_agent(nimcp_health_agent_t* agent);
void temporal_replay_set_health_agent(nimcp_health_agent_t* agent);
void wm_transfer_set_health_agent(nimcp_health_agent_t* agent);
void working_memory_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void working_memory_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B02ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B02ModuleEntry B02_MODULES[] = {
    {"engram",                                  engram_set_health_agent},
    {"hopfield_memory",                         hopfield_memory_set_health_agent},
    {"memory_fep_bridge",                       memory_fep_bridge_set_health_agent},
    {"memory_sleep_bridge",                     memory_sleep_bridge_set_health_agent},
    {"memory_thalamic_bridge",                  memory_thalamic_bridge_set_health_agent},
    {"semantic_memory",                         semantic_memory_set_health_agent},
    {"systems_consolidation",                   systems_consolidation_set_health_agent},
    {"systems_consolidation_pink_noise_bridge", systems_consolidation_pink_noise_bridge_set_health_agent},
    {"temporal_replay",                         temporal_replay_set_health_agent},
    {"wm_transfer",                             wm_transfer_set_health_agent},
    {"working_memory_plasticity_bridge",        working_memory_plasticity_bridge_set_health_agent},
    {"working_memory_snn_bridge",               working_memory_snn_bridge_set_health_agent},
};

static constexpr size_t B02_MODULE_COUNT = sizeof(B02_MODULES) / sizeof(B02_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(nullptr);
    }
}

/* ========================================================================== */
/* Test Fixture                                                               */
/* ========================================================================== */

class HeartbeatB02IntegrationTest : public ::testing::Test {
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

/* ========================================================================== */
/* Module Connect + Heartbeat Flow                                            */
/* ========================================================================== */

TEST_F(HeartbeatB02IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(nimcp_health_agent_is_running(agent));

    /* Connect all 13 modules */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    /* Send heartbeats directly to simulate module activity */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_update", B02_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    /* Verify heartbeats received */
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B02_MODULE_COUNT));
}

/* ========================================================================== */
/* Multiple Modules Sharing Agent                                             */
/* ========================================================================== */

TEST_F(HeartbeatB02IntegrationTest, MultipleModulesShareSingleAgent) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect first half */
    for (size_t i = 0; i < B02_MODULE_COUNT / 2; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    /* Send heartbeats from first half */
    for (size_t i = 0; i < B02_MODULE_COUNT / 2; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "first_half", 0.5f);
    }

    health_agent_stats_t stats_mid;
    nimcp_health_agent_get_stats(agent, &stats_mid);
    uint64_t first_half_beats = stats_mid.heartbeats_received;
    EXPECT_GE(first_half_beats, static_cast<uint64_t>(B02_MODULE_COUNT / 2));

    /* Connect second half */
    for (size_t i = B02_MODULE_COUNT / 2; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    /* Send heartbeats from second half */
    for (size_t i = B02_MODULE_COUNT / 2; i < B02_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "second_half", 1.0f);
    }

    health_agent_stats_t stats_final;
    nimcp_health_agent_get_stats(agent, &stats_final);
    EXPECT_GT(stats_final.heartbeats_received, first_half_beats);
}

/* ========================================================================== */
/* Disconnect While Running                                                   */
/* ========================================================================== */

TEST_F(HeartbeatB02IntegrationTest, DisconnectModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect all */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    /* Send some heartbeats */
    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_disconnect", (float)j / 5.0f);
    }

    /* Disconnect half while agent is running */
    for (size_t i = 0; i < B02_MODULE_COUNT / 2; i++) {
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(nullptr));
    }

    /* Send more heartbeats - should still work for remaining modules */
    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "post_disconnect", (float)j / 5.0f);
    }

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 10u);
}

TEST_F(HeartbeatB02IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        EXPECT_NO_FATAL_FAILURE(B02_MODULES[i].set_fn(nullptr));
    }

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

/* ========================================================================== */
/* Agent Restart With Modules Connected                                       */
/* ========================================================================== */

TEST_F(HeartbeatB02IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (int j = 0; j < 10; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_restart", 0.5f);
    }

    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    for (int j = 0; j < 10; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "post_restart", 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 20u);
}

/* ========================================================================== */
/* Concurrent Module Connection During Active Heartbeats                      */
/* ========================================================================== */

TEST_F(HeartbeatB02IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
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
        for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
            B02_MODULES[i].set_fn(agent);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
            B02_MODULES[i].set_fn(nullptr);
        }
    }

    running.store(false);
    heartbeat_thread.join();

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

/* ========================================================================== */
/* Agent Replacement Flow                                                     */
/* ========================================================================== */

TEST_F(HeartbeatB02IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    config2.check_interval_ms = 50;
    config2.heartbeat_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);

    result = nimcp_health_agent_start(agent2);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent2);
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

/* ========================================================================== */
/* Progressive Connection                                                     */
/* ========================================================================== */

TEST_F(HeartbeatB02IntegrationTest, ProgressiveModuleConnection) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B02_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B02_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
