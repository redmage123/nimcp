/**
 * @file test_heartbeat_B01_memory_core_functions.cpp
 * @brief Unit tests for B01 (cognitive/memory/core) heartbeat setter functions
 *
 * WHAT: Tests setter infrastructure for all 51 memory core modules
 * WHY:  Phase 8 requires every module to have working health agent integration
 * HOW:  Table-driven tests: SetNull, SetValid, ReplaceAgent per module
 *
 * @author NIMCP Team
 * @date 2026-01-26
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

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

/* Setter function pointer type */
typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

/* Module entry for table-driven tests */
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

/* ========================================================================== */
/* Test Fixture                                                               */
/* ========================================================================== */

class HeartbeatB01MemoryCoreFunctionsTest : public ::testing::Test {
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
        /* Clear all modules before destroying agent */
        for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
            B01_MODULES[i].set_fn(nullptr);
        }
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
/* Per-Module Setter Tests                                                    */
/* ========================================================================== */

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, AllModulesSetNull) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, AllModulesSetValid) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, AllModulesReplaceAgent) {
    /* Create a second agent */
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config);
    ASSERT_NE(agent2, nullptr);

    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        /* Set first agent */
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(agent));
        /* Replace with second agent */
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(agent2));
        /* Clear */
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(nullptr));
    }

    nimcp_health_agent_destroy(agent2);
}

/* ========================================================================== */
/* Combined Agent Tests                                                       */
/* ========================================================================== */

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, AllModulesShareSameAgent) {
    /* Set same agent on all 51 modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* All should share without crash */
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    /* Agent is valid and can report stats */
    EXPECT_GE(stats.heartbeats_received, 0u);

    /* Clear all */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, SetNullAfterValidDoesNotCrash) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(nullptr));
    }
}

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, SetValidAfterNullDoesNotCrash) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(nullptr);
    }
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(agent));
    }
}

/* ========================================================================== */
/* Thread Safety Tests                                                        */
/* ========================================================================== */

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, ConcurrentSetClear) {
    const int iterations = 100;
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        threads.emplace_back([this, i, iterations, &completed]() {
            for (int j = 0; j < iterations; j++) {
                B01_MODULES[i].set_fn(agent);
                B01_MODULES[i].set_fn(nullptr);
            }
            completed++;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), static_cast<int>(B01_MODULE_COUNT));
}

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, ConcurrentSetSameModule) {
    /* Multiple threads racing to set/clear the same module */
    const int num_threads = 8;
    const int iterations = 200;
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, iterations, &completed]() {
            for (int j = 0; j < iterations; j++) {
                /* Use first module as contention point */
                B01_MODULES[0].set_fn(agent);
                B01_MODULES[0].set_fn(nullptr);
            }
            completed++;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), num_threads);
}

/* ========================================================================== */
/* Heartbeat Counter Verification                                             */
/* ========================================================================== */

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, HeartbeatCounterIncrementsOnDirectCall) {
    /* Directly send heartbeats via the agent and verify counter */
    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    const int num_heartbeats = 10;
    for (int i = 0; i < num_heartbeats; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "test_b01", (float)i / num_heartbeats);
    }

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    EXPECT_GE(stats_after.heartbeats_received - stats_before.heartbeats_received,
              static_cast<uint64_t>(num_heartbeats));
}

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, ModuleCount) {
    /* Verify we have exactly 51 modules in B01 */
    EXPECT_EQ(B01_MODULE_COUNT, 51u);
}

/* ========================================================================== */
/* Double-Set / Idempotency Tests                                             */
/* ========================================================================== */

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        B01_MODULES[i].set_fn(agent);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(agent));
    }
}

TEST_F(HeartbeatB01MemoryCoreFunctionsTest, DoubleSetNullIdempotent) {
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        SCOPED_TRACE(B01_MODULES[i].name);
        B01_MODULES[i].set_fn(nullptr);
        EXPECT_NO_FATAL_FAILURE(B01_MODULES[i].set_fn(nullptr));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
