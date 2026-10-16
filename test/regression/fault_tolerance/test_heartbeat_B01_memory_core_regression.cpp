/**
 * @file test_heartbeat_B01_memory_core_regression.cpp
 * @brief Regression tests for B01 (cognitive/memory/core) heartbeat API contract
 *
 * WHAT: API stability tests for all 51 memory core module setters
 * WHY:  Ensure setter contract never breaks: NULL always accepted, valid always accepted
 * HOW:  Edge cases, boundary conditions, rapid cycling
 *
 * @author NIMCP Team
 * @date 2026-01-26
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>

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

class HeartbeatB01RegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;
        config.heartbeat_interval_ms = 100;
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
/* API Contract: NULL Always Accepted                                         */
/* ========================================================================== */

TEST_F(HeartbeatB01RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB01RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB01RegressionTest, NullAcceptedRepeatedlyOnSameModule) {
    for (int repeat = 0; repeat < 100; repeat++) {
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[0].set_fn(nullptr));
    }
}

/* ========================================================================== */
/* API Contract: Valid Agent Always Accepted                                  */
/* ========================================================================== */

TEST_F(HeartbeatB01RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB01RegressionTest, ValidAgentAlwaysAcceptedAfterNull) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(nullptr);
    }
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(agent));
    }
}

/* ========================================================================== */
/* Set During Active Heartbeats Doesn't Crash                                 */
/* ========================================================================== */

TEST_F(HeartbeatB01RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Send heartbeats while setting modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "active_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB01RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect all first */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Clear while sending heartbeats */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "clear_test", 0.5f);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(nullptr));
    }
}

/* ========================================================================== */
/* Rapid Cycling: Set/Clear/Set                                               */
/* ========================================================================== */

TEST_F(HeartbeatB01RegressionTest, RapidSetClearCycleAllModules) {
    const int cycles = 50;
    for (int c = 0; c < cycles; c++) {
        for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
            B01_MODULES[i].set_fn(agent);
        }
        for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
            B01_MODULES[i].set_fn(nullptr);
        }
    }
    /* If we reach here without crash, the test passes */
    SUCCEED();
}

TEST_F(HeartbeatB01RegressionTest, RapidSetClearCycleSingleModule) {
    const int cycles = 1000;
    for (int c = 0; c < cycles; c++) {
        B01_MODULES[0].set_fn(agent);
        B01_MODULES[0].set_fn(nullptr);
    }
    SUCCEED();
}

/* ========================================================================== */
/* Multiple Agent Lifecycle                                                   */
/* ========================================================================== */

TEST_F(HeartbeatB01RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int round = 0; round < 5; round++) {
        nimcp_health_agent_t* temp_agent = nimcp_health_agent_create(&config);
        ASSERT_NE(temp_agent, nullptr);

        for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
            B01_MODULES[i].set_fn(temp_agent);
        }

        clear_all_modules();
        nimcp_health_agent_destroy(temp_agent);
    }
}

/* ========================================================================== */
/* Agent Start/Stop With Connected Modules                                    */
/* ========================================================================== */

TEST_F(HeartbeatB01RegressionTest, AgentStartStopCycleWithModules) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    for (int cycle = 0; cycle < 5; cycle++) {
        int result = nimcp_health_agent_start(agent);
        ASSERT_EQ(result, 0);
        EXPECT_TRUE(nimcp_health_agent_is_running(agent));

        nimcp_health_agent_heartbeat_ex(agent, "cycle_test", 0.5f);

        result = nimcp_health_agent_stop(agent);
        ASSERT_EQ(result, 0);
        EXPECT_FALSE(nimcp_health_agent_is_running(agent));
    }
}

/* ========================================================================== */
/* Setter Signature: void return, single pointer param                        */
/* ========================================================================== */

TEST_F(HeartbeatB01RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    /* This test validates at compile time that all setters match the
     * expected signature: void (*)(nimcp_health_agent_t*).
     * If the signature changed, this file would not compile. */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        set_health_agent_fn fn = B01_MODULES[i].set_fn;
        EXPECT_NE(fn, nullptr) << "Module " << B01_MODULES[i].name
                               << " has null function pointer";
    }
}

/* ========================================================================== */
/* Stats Consistency After Module Operations                                  */
/* ========================================================================== */

TEST_F(HeartbeatB01RegressionTest, StatsConsistentAfterModuleSetClear) {
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    /* Set and clear all modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }
    clear_all_modules();

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    /* Setting modules should not change heartbeat count */
    EXPECT_EQ(stats_before.heartbeats_received, stats_after.heartbeats_received);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
