/**
 * @file test_heartbeat_B04_parietal_integration.cpp
 * @brief Integration tests for B04 (cognitive/parietal) heartbeat flow
 *
 * WHAT: Tests heartbeat integration across multiple parietal modules
 * WHY:  Phase 8 requires modules to work together with shared health agents
 * HOW:  Multi-module agent sharing, disconnect-while-running, agent restart
 *
 * NOTE: physics_nn excluded — not yet compiled (NOT YET IMPLEMENTED in CMakeLists.txt)
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

/* B04: cognitive/parietal — 34 setter declarations */
void analogical_reasoning_set_health_agent(nimcp_health_agent_t* agent);
void biology_set_health_agent(nimcp_health_agent_t* agent);
void chemistry_set_health_agent(nimcp_health_agent_t* agent);
void civil_engineering_set_health_agent(nimcp_health_agent_t* agent);
void code_generation_set_health_agent(nimcp_health_agent_t* agent);
void conceptual_blending_set_health_agent(nimcp_health_agent_t* agent);
void electrical_engineering_set_health_agent(nimcp_health_agent_t* agent);
void equation_manipulation_set_health_agent(nimcp_health_agent_t* agent);
void fep_parietal_bridge_set_health_agent(nimcp_health_agent_t* agent);
void genius_erdos_set_health_agent(nimcp_health_agent_t* agent);
void genius_gauss_set_health_agent(nimcp_health_agent_t* agent);
void genius_newton_set_health_agent(nimcp_health_agent_t* agent);
void genius_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void genius_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void genius_training_bridge_set_health_agent(nimcp_health_agent_t* agent);
void hypothesis_generation_set_health_agent(nimcp_health_agent_t* agent);
void insight_discovery_set_health_agent(nimcp_health_agent_t* agent);
void intuition_integrations_set_health_agent(nimcp_health_agent_t* agent);
void intuition_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void intuition_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void intuitive_reasoning_set_health_agent(nimcp_health_agent_t* agent);
void mathematical_genius_set_health_agent(nimcp_health_agent_t* agent);
void mathematical_intuition_set_health_agent(nimcp_health_agent_t* agent);
void mechanical_engineering_set_health_agent(nimcp_health_agent_t* agent);
void meta_reasoning_set_health_agent(nimcp_health_agent_t* agent);
void number_sense_set_health_agent(nimcp_health_agent_t* agent);
void parietal_set_health_agent(nimcp_health_agent_t* agent);
void parietal_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void parietal_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void parietal_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void parietal_training_bridge_set_health_agent(nimcp_health_agent_t* agent);
void scientific_reasoning_set_health_agent(nimcp_health_agent_t* agent);
void software_engineering_set_health_agent(nimcp_health_agent_t* agent);
void spatial_reasoning_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B04ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B04ModuleEntry B04_MODULES[] = {
    {"analogical_reasoning",       analogical_reasoning_set_health_agent},
    {"biology",                    biology_set_health_agent},
    {"chemistry",                  chemistry_set_health_agent},
    {"civil_engineering",          civil_engineering_set_health_agent},
    {"code_generation",            code_generation_set_health_agent},
    {"conceptual_blending",        conceptual_blending_set_health_agent},
    {"electrical_engineering",     electrical_engineering_set_health_agent},
    {"equation_manipulation",      equation_manipulation_set_health_agent},
    {"fep_parietal_bridge",        fep_parietal_bridge_set_health_agent},
    {"genius_erdos",               genius_erdos_set_health_agent},
    {"genius_gauss",               genius_gauss_set_health_agent},
    {"genius_newton",              genius_newton_set_health_agent},
    {"genius_plasticity_bridge",   genius_plasticity_bridge_set_health_agent},
    {"genius_snn_bridge",          genius_snn_bridge_set_health_agent},
    {"genius_training_bridge",     genius_training_bridge_set_health_agent},
    {"hypothesis_generation",      hypothesis_generation_set_health_agent},
    {"insight_discovery",          insight_discovery_set_health_agent},
    {"intuition_integrations",     intuition_integrations_set_health_agent},
    {"intuition_substrate_bridge", intuition_substrate_bridge_set_health_agent},
    {"intuition_thalamic_bridge",  intuition_thalamic_bridge_set_health_agent},
    {"intuitive_reasoning",        intuitive_reasoning_set_health_agent},
    {"mathematical_genius",        mathematical_genius_set_health_agent},
    {"mathematical_intuition",     mathematical_intuition_set_health_agent},
    {"mechanical_engineering",     mechanical_engineering_set_health_agent},
    {"meta_reasoning",             meta_reasoning_set_health_agent},
    {"number_sense",               number_sense_set_health_agent},
    {"parietal",                   parietal_set_health_agent},
    {"parietal_fep_bridge",        parietal_fep_bridge_set_health_agent},
    {"parietal_plasticity_bridge", parietal_plasticity_bridge_set_health_agent},
    {"parietal_snn_bridge",        parietal_snn_bridge_set_health_agent},
    {"parietal_training_bridge",   parietal_training_bridge_set_health_agent},
    {"scientific_reasoning",       scientific_reasoning_set_health_agent},
    {"software_engineering",       software_engineering_set_health_agent},
    {"spatial_reasoning",          spatial_reasoning_set_health_agent},
};

static constexpr size_t B04_MODULE_COUNT = sizeof(B04_MODULES) / sizeof(B04_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB04IntegrationTest : public ::testing::Test {
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

TEST_F(HeartbeatB04IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(nimcp_health_agent_is_running(agent));

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_update", B04_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B04_MODULE_COUNT));
}

TEST_F(HeartbeatB04IntegrationTest, MultipleModulesShareSingleAgent) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B04_MODULE_COUNT / 2; i++) {
        B04_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B04_MODULE_COUNT / 2; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "first_half", 0.5f);
    }

    health_agent_stats_t stats_mid;
    nimcp_health_agent_get_stats(agent, &stats_mid);
    uint64_t first_half_beats = stats_mid.heartbeats_received;
    EXPECT_GE(first_half_beats, static_cast<uint64_t>(B04_MODULE_COUNT / 2));

    for (size_t i = B04_MODULE_COUNT / 2; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
    }
    for (size_t i = B04_MODULE_COUNT / 2; i < B04_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "second_half", 1.0f);
    }

    health_agent_stats_t stats_final;
    nimcp_health_agent_get_stats(agent, &stats_final);
    EXPECT_GT(stats_final.heartbeats_received, first_half_beats);
}

TEST_F(HeartbeatB04IntegrationTest, DisconnectModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_disconnect", (float)j / 5.0f);
    }

    for (size_t i = 0; i < B04_MODULE_COUNT / 2; i++) {
        EXPECT_NO_FATAL_FAILURE(B04_MODULES[i].set_fn(nullptr));
    }

    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "post_disconnect", (float)j / 5.0f);
    }

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 10u);
}

TEST_F(HeartbeatB04IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        EXPECT_NO_FATAL_FAILURE(B04_MODULES[i].set_fn(nullptr));
    }
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB04IntegrationTest, AgentRestartWithModulesConnected) {
    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
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

TEST_F(HeartbeatB04IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
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
        for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
            B04_MODULES[i].set_fn(agent);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
            B04_MODULES[i].set_fn(nullptr);
        }
    }

    running.store(false);
    heartbeat_thread.join();
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

TEST_F(HeartbeatB04IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
    }

    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    config2.check_interval_ms = 50;
    config2.heartbeat_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);
    result = nimcp_health_agent_start(agent2);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent2);
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

TEST_F(HeartbeatB04IntegrationTest, ProgressiveModuleConnection) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B04_MODULE_COUNT; i++) {
        B04_MODULES[i].set_fn(agent);
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B04_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B04_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
