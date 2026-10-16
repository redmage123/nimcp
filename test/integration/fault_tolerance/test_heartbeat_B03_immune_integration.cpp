/**
 * @file test_heartbeat_B03_immune_integration.cpp
 * @brief Integration tests for B03 (cognitive/immune) heartbeat flow
 *
 * WHAT: Tests heartbeat integration across multiple immune modules
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

/* B03: cognitive/immune — 39 setter declarations */
void attention_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void autobiographical_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_integration_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_plasticity_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void brain_immune_tick_set_health_agent(nimcp_health_agent_t* agent);
void claude_healer_set_health_agent(nimcp_health_agent_t* agent);
void code_immune_set_health_agent(nimcp_health_agent_t* agent);
void code_immune_self_repair_set_health_agent(nimcp_health_agent_t* agent);
void complement_system_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void executive_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void heal_bridge_set_health_agent(nimcp_health_agent_t* agent);
void immune_bridge_coordinator_set_health_agent(nimcp_health_agent_t* agent);
void immune_exhaustion_set_health_agent(nimcp_health_agent_t* agent);
void immune_metrics_set_health_agent(nimcp_health_agent_t* agent);
void immune_persistence_set_health_agent(nimcp_health_agent_t* agent);
void immune_tolerance_set_health_agent(nimcp_health_agent_t* agent);
void immune_vaccine_set_health_agent(nimcp_health_agent_t* agent);
void introspection_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void knowledge_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void memory_immune_integration_set_health_agent(nimcp_health_agent_t* agent);
void mental_health_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mucosal_immunity_set_health_agent(nimcp_health_agent_t* agent);
void omni_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void perception_immune_set_health_agent(nimcp_health_agent_t* agent);
void reasoning_immune_set_health_agent(nimcp_health_agent_t* agent);
void regulatory_tcells_set_health_agent(nimcp_health_agent_t* agent);
void self_heal_set_health_agent(nimcp_health_agent_t* agent);
void self_model_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void sleep_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void surface_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void tom_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void trained_immunity_set_health_agent(nimcp_health_agent_t* agent);
void wellbeing_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B03ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B03ModuleEntry B03_MODULES[] = {
    {"attention_immune_bridge",          attention_immune_bridge_set_health_agent},
    {"autobiographical_immune_bridge",   autobiographical_immune_bridge_set_health_agent},
    {"brain_immune",                     brain_immune_set_health_agent},
    {"brain_immune_fep_bridge",          brain_immune_fep_bridge_set_health_agent},
    {"brain_immune_integration",         brain_immune_integration_set_health_agent},
    {"brain_immune_plasticity",          brain_immune_plasticity_set_health_agent},
    {"brain_immune_substrate_bridge",    brain_immune_substrate_bridge_set_health_agent},
    {"brain_immune_thalamic_bridge",     brain_immune_thalamic_bridge_set_health_agent},
    {"brain_immune_tick",                brain_immune_tick_set_health_agent},
    {"claude_healer",                    claude_healer_set_health_agent},
    {"code_immune",                      code_immune_set_health_agent},
    {"code_immune_self_repair",          code_immune_self_repair_set_health_agent},
    {"complement_system",                complement_system_set_health_agent},
    {"curiosity_immune_bridge",          curiosity_immune_bridge_set_health_agent},
    {"emotion_immune_bridge",            emotion_immune_bridge_set_health_agent},
    {"executive_immune_bridge",          executive_immune_bridge_set_health_agent},
    {"heal_bridge",                      heal_bridge_set_health_agent},
    {"immune_bridge_coordinator",        immune_bridge_coordinator_set_health_agent},
    {"immune_exhaustion",                immune_exhaustion_set_health_agent},
    {"immune_metrics",                   immune_metrics_set_health_agent},
    {"immune_persistence",               immune_persistence_set_health_agent},
    {"immune_tolerance",                 immune_tolerance_set_health_agent},
    {"immune_vaccine",                   immune_vaccine_set_health_agent},
    {"introspection_immune_bridge",      introspection_immune_bridge_set_health_agent},
    {"knowledge_immune_bridge",          knowledge_immune_bridge_set_health_agent},
    {"memory_immune_integration",        memory_immune_integration_set_health_agent},
    {"mental_health_immune_bridge",      mental_health_immune_bridge_set_health_agent},
    {"mucosal_immunity",                 mucosal_immunity_set_health_agent},
    {"omni_immune_bridge",               omni_immune_bridge_set_health_agent},
    {"perception_immune",                perception_immune_set_health_agent},
    {"reasoning_immune",                 reasoning_immune_set_health_agent},
    {"regulatory_tcells",                regulatory_tcells_set_health_agent},
    {"self_heal",                        self_heal_set_health_agent},
    {"self_model_immune_bridge",         self_model_immune_bridge_set_health_agent},
    {"sleep_immune_bridge",              sleep_immune_bridge_set_health_agent},
    {"surface_immune_bridge",            surface_immune_bridge_set_health_agent},
    {"tom_immune_bridge",                tom_immune_bridge_set_health_agent},
    {"trained_immunity",                 trained_immunity_set_health_agent},
    {"wellbeing_immune_bridge",          wellbeing_immune_bridge_set_health_agent},
};

static constexpr size_t B03_MODULE_COUNT = sizeof(B03_MODULES) / sizeof(B03_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB03IntegrationTest : public ::testing::Test {
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

TEST_F(HeartbeatB03IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(nimcp_health_agent_is_running(agent));

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_update", B03_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B03_MODULE_COUNT));
}

TEST_F(HeartbeatB03IntegrationTest, MultipleModulesShareSingleAgent) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT / 2; i++) {
        B03_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B03_MODULE_COUNT / 2; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "first_half", 0.5f);
    }

    health_agent_stats_t stats_mid;
    nimcp_health_agent_get_stats(agent, &stats_mid);
    uint64_t first_half_beats = stats_mid.heartbeats_received;
    EXPECT_GE(first_half_beats, static_cast<uint64_t>(B03_MODULE_COUNT / 2));

    for (size_t i = B03_MODULE_COUNT / 2; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }
    for (size_t i = B03_MODULE_COUNT / 2; i < B03_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "second_half", 1.0f);
    }

    health_agent_stats_t stats_final;
    nimcp_health_agent_get_stats(agent, &stats_final);
    EXPECT_GT(stats_final.heartbeats_received, first_half_beats);
}

TEST_F(HeartbeatB03IntegrationTest, DisconnectModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_disconnect", (float)j / 5.0f);
    }

    for (size_t i = 0; i < B03_MODULE_COUNT / 2; i++) {
        EXPECT_NO_FATAL_FAILURE(B03_MODULES[i].set_fn(nullptr));
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "post_disconnect", (float)j / 5.0f);
    }

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 10u);
}

TEST_F(HeartbeatB03IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        EXPECT_NO_FATAL_FAILURE(B03_MODULES[i].set_fn(nullptr));
    }
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB03IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
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

TEST_F(HeartbeatB03IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
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
        for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
            B03_MODULES[i].set_fn(agent);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
            B03_MODULES[i].set_fn(nullptr);
        }
    }

    running.store(false);
    heartbeat_thread.join();
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB03IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    config2.check_interval_ms = 50;
    config2.heartbeat_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);
    result = nimcp_health_agent_start(agent2);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent2);
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

TEST_F(HeartbeatB03IntegrationTest, ProgressiveModuleConnection) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B03_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B03_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
