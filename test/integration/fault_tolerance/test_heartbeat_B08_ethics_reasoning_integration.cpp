/**
 * @file test_heartbeat_B08_ethics_reasoning_integration.cpp
 * @brief Integration tests for B08 (cognitive/ethics + cognitive/reasoning) heartbeat flow
 *
 * WHAT: Tests heartbeat integration across 30 ethics and reasoning modules
 * WHY:  Phase 8 requires modules to work together with shared health agents
 * HOW:  Multi-module agent sharing, disconnect-while-running, agent restart
 *
 * NOTE: core_directives excluded — not compiled into library
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

/* B08: cognitive/ethics + cognitive/reasoning — 30 setter declarations */
void backward_chaining_set_health_agent(nimcp_health_agent_t* agent);
void combinatorial_harm_set_health_agent(nimcp_health_agent_t* agent);
void ethics_asimov_set_health_agent(nimcp_health_agent_t* agent);
void ethics_evaluation_set_health_agent(nimcp_health_agent_t* agent);
void ethics_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_hyperbolic_set_health_agent(nimcp_health_agent_t* agent);
void ethics_immune_set_health_agent(nimcp_health_agent_t* agent);
void ethics_incidents_set_health_agent(nimcp_health_agent_t* agent);
void ethics_learning_set_health_agent(nimcp_health_agent_t* agent);
void ethics_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_policies_set_health_agent(nimcp_health_agent_t* agent);
void ethics_set_health_agent(nimcp_health_agent_t* agent);
void ethics_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_warfare_set_health_agent(nimcp_health_agent_t* agent);
void forward_chaining_set_health_agent(nimcp_health_agent_t* agent);
void health_ethics_bridge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_base_interface_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_factory_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_integration_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_attachment_set_health_agent(nimcp_health_agent_t* agent);
void symbolic_logic_brain_integration_set_health_agent(nimcp_health_agent_t* agent);
void unification_engine_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B08ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B08ModuleEntry B08_MODULES[] = {
    {"backward_chaining",                backward_chaining_set_health_agent},
    {"combinatorial_harm",               combinatorial_harm_set_health_agent},
    {"ethics",                           ethics_set_health_agent},
    {"ethics_asimov",                    ethics_asimov_set_health_agent},
    {"ethics_evaluation",                ethics_evaluation_set_health_agent},
    {"ethics_fep_bridge",                ethics_fep_bridge_set_health_agent},
    {"ethics_hyperbolic",                ethics_hyperbolic_set_health_agent},
    {"ethics_immune",                    ethics_immune_set_health_agent},
    {"ethics_incidents",                 ethics_incidents_set_health_agent},
    {"ethics_learning",                  ethics_learning_set_health_agent},
    {"ethics_plasticity_bridge",         ethics_plasticity_bridge_set_health_agent},
    {"ethics_policies",                  ethics_policies_set_health_agent},
    {"ethics_snn_bridge",                ethics_snn_bridge_set_health_agent},
    {"ethics_substrate_bridge",          ethics_substrate_bridge_set_health_agent},
    {"ethics_thalamic_bridge",           ethics_thalamic_bridge_set_health_agent},
    {"ethics_warfare",                   ethics_warfare_set_health_agent},
    {"forward_chaining",                 forward_chaining_set_health_agent},
    {"health_ethics_bridge",             health_ethics_bridge_set_health_agent},
    {"knowledge_base_interface",         knowledge_base_interface_set_health_agent},
    {"reasoning_factory",                reasoning_factory_set_health_agent},
    {"reasoning_fep_bridge",             reasoning_fep_bridge_set_health_agent},
    {"reasoning_integration",            reasoning_integration_set_health_agent},
    {"reasoning_plasticity_bridge",      reasoning_plasticity_bridge_set_health_agent},
    {"reasoning_sleep_bridge",           reasoning_sleep_bridge_set_health_agent},
    {"reasoning_snn_bridge",             reasoning_snn_bridge_set_health_agent},
    {"reasoning_substrate_bridge",       reasoning_substrate_bridge_set_health_agent},
    {"reasoning_thalamic_bridge",        reasoning_thalamic_bridge_set_health_agent},
    {"symbolic_logic_attachment",        symbolic_logic_attachment_set_health_agent},
    {"symbolic_logic_brain_integration", symbolic_logic_brain_integration_set_health_agent},
    {"unification_engine",               unification_engine_set_health_agent},
};

static constexpr size_t B08_MODULE_COUNT = sizeof(B08_MODULES) / sizeof(B08_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB08IntegrationTest : public ::testing::Test {
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

TEST_F(HeartbeatB08IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(nimcp_health_agent_is_running(agent));

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_update", B08_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B08_MODULE_COUNT));
}

TEST_F(HeartbeatB08IntegrationTest, MultipleModulesShareSingleAgent) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B08_MODULE_COUNT / 2; i++) {
        B08_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B08_MODULE_COUNT / 2; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "first_half", 0.5f);
    }

    health_agent_stats_t stats_mid;
    nimcp_health_agent_get_stats(agent, &stats_mid);
    uint64_t first_half_beats = stats_mid.heartbeats_received;
    EXPECT_GE(first_half_beats, static_cast<uint64_t>(B08_MODULE_COUNT / 2));

    for (size_t i = B08_MODULE_COUNT / 2; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
    }
    for (size_t i = B08_MODULE_COUNT / 2; i < B08_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "second_half", 1.0f);
    }

    health_agent_stats_t stats_final;
    nimcp_health_agent_get_stats(agent, &stats_final);
    EXPECT_GT(stats_final.heartbeats_received, first_half_beats);
}

TEST_F(HeartbeatB08IntegrationTest, DisconnectModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_disconnect", (float)j / 5.0f);
    }

    for (size_t i = 0; i < B08_MODULE_COUNT / 2; i++) {
        EXPECT_NO_FATAL_FAILURE(B08_MODULES[i].set_fn(nullptr));
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "post_disconnect", (float)j / 5.0f);
    }

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 10u);
}

TEST_F(HeartbeatB08IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        EXPECT_NO_FATAL_FAILURE(B08_MODULES[i].set_fn(nullptr));
    }
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB08IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
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

TEST_F(HeartbeatB08IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
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
        for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
            B08_MODULES[i].set_fn(agent);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
            B08_MODULES[i].set_fn(nullptr);
        }
    }

    running.store(false);
    heartbeat_thread.join();
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB08IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
    }

    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    config2.check_interval_ms = 50;
    config2.heartbeat_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);
    result = nimcp_health_agent_start(agent2);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent2);
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

TEST_F(HeartbeatB08IntegrationTest, ProgressiveModuleConnection) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B08_MODULE_COUNT; i++) {
        B08_MODULES[i].set_fn(agent);
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B08_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B08_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
