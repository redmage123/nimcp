/**
 * @file test_heartbeat_B01_memory_core_integration.cpp
 * @brief Integration tests for B01 (cognitive/memory/core) heartbeat flow
 *
 * WHAT: Tests heartbeat integration across multiple memory core modules
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

/* B01: cognitive/memory/core — 51 setter declarations */
void collective_memory_set_health_agent(nimcp_health_agent_t* agent);
void counterfactual_set_health_agent(nimcp_health_agent_t* agent);
void entanglement_set_health_agent(nimcp_health_agent_t* agent);
void flashbulb_set_health_agent(nimcp_health_agent_t* agent);
void fractal_set_health_agent(nimcp_health_agent_t* agent);
void future_thinking_set_health_agent(nimcp_health_agent_t* agent);
void gist_set_health_agent(nimcp_health_agent_t* agent);
void kuramoto_set_health_agent(nimcp_health_agent_t* agent);
void metamemory_set_health_agent(nimcp_health_agent_t* agent);
void metamemory_monitor_set_health_agent(nimcp_health_agent_t* agent);
void pr_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_audio_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_bio_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_cerebellum_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_cognitive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_continual_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_curriculum_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_hypo_bridge_set_health_agent(nimcp_health_agent_t* agent);
void prime_signature_set_health_agent(nimcp_health_agent_t* agent);
void pr_immune_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_kg_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_logging_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_loss_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_memory_node_set_health_agent(nimcp_health_agent_t* agent);
void pr_mental_health_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_meta_bridge_set_health_agent(nimcp_health_agent_t* agent);
void procedural_set_health_agent(nimcp_health_agent_t* agent);
void pr_omni_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_optimizer_bridge_set_health_agent(nimcp_health_agent_t* agent);
void prospective_set_health_agent(nimcp_health_agent_t* agent);
void prospective_scheduler_set_health_agent(nimcp_health_agent_t* agent);
void pr_pink_noise_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_pink_noise_set_health_agent(nimcp_health_agent_t* agent);
void pr_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_predictive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_speech_bridge_set_health_agent(nimcp_health_agent_t* agent);
void pr_training_plasticity_set_health_agent(nimcp_health_agent_t* agent);
void pr_visual_bridge_set_health_agent(nimcp_health_agent_t* agent);
void quaternion_set_health_agent(nimcp_health_agent_t* agent);
void reconsolidation_set_health_agent(nimcp_health_agent_t* agent);
void resonance_set_health_agent(nimcp_health_agent_t* agent);
void schemas_set_health_agent(nimcp_health_agent_t* agent);
void skill_acquisition_set_health_agent(nimcp_health_agent_t* agent);
void social_memory_set_health_agent(nimcp_health_agent_t* agent);
void source_memory_set_health_agent(nimcp_health_agent_t* agent);
void spaced_repetition_set_health_agent(nimcp_health_agent_t* agent);
void theta_gamma_set_health_agent(nimcp_health_agent_t* agent);
void transactive_set_health_agent(nimcp_health_agent_t* agent);
void z_ladder_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B01ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B01ModuleEntry B01_MODULES[] = {
    {"collective_memory",       collective_memory_set_health_agent},
    {"counterfactual",          counterfactual_set_health_agent},
    {"entanglement",            entanglement_set_health_agent},
    {"flashbulb",               flashbulb_set_health_agent},
    {"fractal",                 fractal_set_health_agent},
    {"future_thinking",         future_thinking_set_health_agent},
    {"gist",                    gist_set_health_agent},
    {"kuramoto",                kuramoto_set_health_agent},
    {"metamemory",              metamemory_set_health_agent},
    {"metamemory_monitor",      metamemory_monitor_set_health_agent},
    {"pr_attention_bridge",     pr_attention_bridge_set_health_agent},
    {"pr_audio_bridge",         pr_audio_bridge_set_health_agent},
    {"pr_bio_bridge",           pr_bio_bridge_set_health_agent},
    {"pr_cerebellum_bridge",    pr_cerebellum_bridge_set_health_agent},
    {"pr_cognitive_bridge",     pr_cognitive_bridge_set_health_agent},
    {"pr_continual_bridge",     pr_continual_bridge_set_health_agent},
    {"pr_curriculum_bridge",    pr_curriculum_bridge_set_health_agent},
    {"pr_hypo_bridge",          pr_hypo_bridge_set_health_agent},
    {"prime_signature",         prime_signature_set_health_agent},
    {"pr_immune_bridge",        pr_immune_bridge_set_health_agent},
    {"pr_kg_bridge",            pr_kg_bridge_set_health_agent},
    {"pr_logging_bridge",       pr_logging_bridge_set_health_agent},
    {"pr_loss_bridge",          pr_loss_bridge_set_health_agent},
    {"pr_memory_node",          pr_memory_node_set_health_agent},
    {"pr_mental_health_bridge", pr_mental_health_bridge_set_health_agent},
    {"pr_meta_bridge",          pr_meta_bridge_set_health_agent},
    {"procedural",              procedural_set_health_agent},
    {"pr_omni_bridge",          pr_omni_bridge_set_health_agent},
    {"pr_optimizer_bridge",     pr_optimizer_bridge_set_health_agent},
    {"prospective",             prospective_set_health_agent},
    {"prospective_scheduler",   prospective_scheduler_set_health_agent},
    {"pr_pink_noise_bridge",    pr_pink_noise_bridge_set_health_agent},
    {"pr_pink_noise",           pr_pink_noise_set_health_agent},
    {"pr_plasticity_bridge",    pr_plasticity_bridge_set_health_agent},
    {"pr_predictive_bridge",    pr_predictive_bridge_set_health_agent},
    {"pr_sleep_bridge",         pr_sleep_bridge_set_health_agent},
    {"pr_snn_bridge",           pr_snn_bridge_set_health_agent},
    {"pr_speech_bridge",        pr_speech_bridge_set_health_agent},
    {"pr_training_plasticity",  pr_training_plasticity_set_health_agent},
    {"pr_visual_bridge",        pr_visual_bridge_set_health_agent},
    {"quaternion",              quaternion_set_health_agent},
    {"reconsolidation",         reconsolidation_set_health_agent},
    {"resonance",               resonance_set_health_agent},
    {"schemas",                 schemas_set_health_agent},
    {"skill_acquisition",       skill_acquisition_set_health_agent},
    {"social_memory",           social_memory_set_health_agent},
    {"source_memory",           source_memory_set_health_agent},
    {"spaced_repetition",       spaced_repetition_set_health_agent},
    {"theta_gamma",             theta_gamma_set_health_agent},
    {"transactive",             transactive_set_health_agent},
    {"z_ladder",                z_ladder_set_health_agent},
};

static constexpr size_t B01_MODULE_COUNT = sizeof(B01_MODULES) / sizeof(B01_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(nullptr);
    }
}

/* ========================================================================== */
/* Test Fixture                                                               */
/* ========================================================================== */

class HeartbeatB01IntegrationTest : public ::testing::Test {
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

TEST_F(HeartbeatB01IntegrationTest, ConnectAllModulesAndVerifyHeartbeatFlow) {
    /* Start the agent */
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    ASSERT_TRUE(nimcp_health_agent_is_running(agent));

    /* Connect all 51 modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Send heartbeats directly to simulate module activity */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_update", B01_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    /* Verify heartbeats received */
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B01_MODULE_COUNT));
}

/* ========================================================================== */
/* Multiple Modules Sharing Agent                                             */
/* ========================================================================== */

TEST_F(HeartbeatB01IntegrationTest, MultipleModulesShareSingleAgent) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect first half */
    for (size_t i = 0; i < B01_MODULE_COUNT / 2; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Send heartbeats from first half */
    for (size_t i = 0; i < B01_MODULE_COUNT / 2; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "first_half", 0.5f);
    }

    health_agent_stats_t stats_mid;
    nimcp_health_agent_get_stats(agent, &stats_mid);
    uint64_t first_half_beats = stats_mid.heartbeats_received;
    EXPECT_GE(first_half_beats, static_cast<uint64_t>(B01_MODULE_COUNT / 2));

    /* Connect second half */
    for (size_t i = B01_MODULE_COUNT / 2; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Send heartbeats from second half */
    for (size_t i = B01_MODULE_COUNT / 2; i < B01_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "second_half", 1.0f);
    }

    health_agent_stats_t stats_final;
    nimcp_health_agent_get_stats(agent, &stats_final);
    EXPECT_GT(stats_final.heartbeats_received, first_half_beats);
}

/* ========================================================================== */
/* Disconnect While Running                                                   */
/* ========================================================================== */

TEST_F(HeartbeatB01IntegrationTest, DisconnectModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect all */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Send some heartbeats */
    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_disconnect", (float)j / 5.0f);
    }

    /* Disconnect half while agent is running */
    for (size_t i = 0; i < B01_MODULE_COUNT / 2; i++) {
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(nullptr));
    }

    /* Send more heartbeats - should still work for remaining modules */
    for (int j = 0; j < 5; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "post_disconnect", (float)j / 5.0f);
    }

    /* Agent should still be running */
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 10u);
}

TEST_F(HeartbeatB01IntegrationTest, DisconnectAllModulesWhileAgentRunning) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect all */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Disconnect all */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(nullptr));
    }

    /* Agent should still be valid and running */
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

/* ========================================================================== */
/* Agent Restart With Modules Connected                                       */
/* ========================================================================== */

TEST_F(HeartbeatB01IntegrationTest, AgentRestartWithModulesConnected) {
    /* Connect modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Start agent */
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Send heartbeats */
    for (int j = 0; j < 10; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_restart", 0.5f);
    }

    /* Stop agent */
    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    /* Restart agent - modules still connected */
    result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    /* Send more heartbeats - should work */
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

TEST_F(HeartbeatB01IntegrationTest, ConcurrentConnectionDuringHeartbeats) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    std::atomic<bool> running{true};

    /* Thread sending heartbeats continuously */
    std::thread heartbeat_thread([this, &running]() {
        while (running.load()) {
            nimcp_health_agent_heartbeat_ex(agent, "background", 0.5f);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    /* Main thread connects/disconnects modules */
    for (int round = 0; round < 3; round++) {
        for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
            B01_MODULES[i].set_fn(agent);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
            B01_MODULES[i].set_fn(nullptr);
        }
    }

    running.store(false);
    heartbeat_thread.join();

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
}

/* ========================================================================== */
/* Agent Replacement Flow                                                     */
/* ========================================================================== */

TEST_F(HeartbeatB01IntegrationTest, ReplaceAgentOnAllModulesAtomically) {
    /* Start first agent */
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect all modules to first agent */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Create second agent */
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    config2.check_interval_ms = 50;
    config2.heartbeat_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);

    result = nimcp_health_agent_start(agent2);
    ASSERT_EQ(result, 0);

    /* Atomically swap all modules to second agent */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent2);
    }

    /* Send heartbeats - should go to agent2 */
    for (int j = 0; j < 10; j++) {
        nimcp_health_agent_heartbeat_ex(agent2, "new_agent", 1.0f);
    }

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, 10u);

    /* Cleanup second agent */
    clear_all_modules();
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
}

/* ========================================================================== */
/* Progressive Connection                                                     */
/* ========================================================================== */

TEST_F(HeartbeatB01IntegrationTest, ProgressiveModuleConnection) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect modules one at a time, sending heartbeat after each */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B01_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B01_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
