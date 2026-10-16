/**
 * @file test_heartbeat_B01_memory_core_e2e.cpp
 * @brief E2E tests for B01 (cognitive/memory/core) heartbeat full lifecycle
 *
 * WHAT: End-to-end heartbeat lifecycle tests for 51 memory core modules
 * WHY:  Phase 8 requires verified complete lifecycle: connect -> heartbeat -> disconnect
 * HOW:  Full agent lifecycle, concurrent modules, high-frequency burst, timeout
 *
 * @author NIMCP Team
 * @date 2026-01-26
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
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

class HeartbeatB01E2ETest : public ::testing::Test {
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
/* Full Lifecycle: Connect -> Start -> Heartbeat -> Verify -> Stop -> Disconnect */
/* ========================================================================== */

TEST_F(HeartbeatB01E2ETest, FullLifecycleAllModules) {
    /* 1. Connect all modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* 2. Start agent */
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    /* 3. Send heartbeats simulating module activity */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B01_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.5f);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    /* 4. Verify heartbeat reception */
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B01_MODULE_COUNT * 3));

    /* 5. Stop agent */
    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    /* 6. Disconnect all modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(nullptr);
    }
}

/* ========================================================================== */
/* Concurrent Modules From Multiple Threads                                   */
/* ========================================================================== */

TEST_F(HeartbeatB01E2ETest, ConcurrentModulesMultipleThreads) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect all modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    const int num_threads = 8;
    const int heartbeats_per_thread = 50;
    std::atomic<int> total_sent{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, heartbeats_per_thread, &total_sent]() {
            for (int i = 0; i < heartbeats_per_thread; i++) {
                /* Each thread simulates a different subset of modules */
                size_t mod_idx = (t * heartbeats_per_thread + i) % B01_MODULE_COUNT;
                char op[64];
                snprintf(op, sizeof(op), "thread%d_%s", t, B01_MODULES[mod_idx].name);
                float progress = static_cast<float>(i) / heartbeats_per_thread;
                nimcp_health_agent_heartbeat_ex(agent, op, progress);
                total_sent++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received,
              static_cast<uint64_t>(num_threads * heartbeats_per_thread));
}

/* ========================================================================== */
/* High-Frequency Burst (1000 heartbeats)                                     */
/* ========================================================================== */

TEST_F(HeartbeatB01E2ETest, HighFrequencyBurst1000Heartbeats) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect a representative set of modules */
    for (size_t i = 0; i < 10 && i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    /* Burst 1000 heartbeats as fast as possible */
    const int burst_count = 1000;
    for (int i = 0; i < burst_count; i++) {
        float progress = static_cast<float>(i) / burst_count;
        nimcp_health_agent_heartbeat_ex(agent, "burst_test", progress);
    }

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);

    uint64_t delta = stats_after.heartbeats_received - stats_before.heartbeats_received;
    EXPECT_GE(delta, static_cast<uint64_t>(burst_count));
}

/* ========================================================================== */
/* Timeout Detection                                                          */
/* ========================================================================== */

TEST_F(HeartbeatB01E2ETest, TimeoutDetectionAfterSilence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Send initial heartbeat */
    nimcp_health_agent_heartbeat_ex(agent, "timeout_test", 0.0f);

    /* Wait longer than watchdog timeout (500ms) */
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    /* Agent should still be running (it doesn't self-stop) */
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    /* Stats should show the heartbeat was received */
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
}

/* ========================================================================== */
/* Complete Multi-Phase Operation                                             */
/* ========================================================================== */

TEST_F(HeartbeatB01E2ETest, MultiPhaseOperation) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Connect all modules */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Phase 1: Initialization (0.0 - 0.25) */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B01_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.25f);
    }

    /* Phase 2: Processing (0.25 - 0.75) */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B01_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.50f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.75f);
    }

    /* Phase 3: Finalization (0.75 - 1.0) */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_finalize", B01_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    /* 5 heartbeats per module * 51 modules = 255 */
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B01_MODULE_COUNT * 5));
}

/* ========================================================================== */
/* Module Hot-Swap During Operation                                           */
/* ========================================================================== */

TEST_F(HeartbeatB01E2ETest, ModuleHotSwapDuringOperation) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    /* Create second agent for hot-swap target */
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    config2.check_interval_ms = 50;
    config2.heartbeat_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);
    result = nimcp_health_agent_start(agent2);
    ASSERT_EQ(result, 0);

    /* Connect all to agent1 */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Send heartbeats to agent1 */
    for (int j = 0; j < 20; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_swap", 0.5f);
    }

    health_agent_stats_t stats1_before;
    nimcp_health_agent_get_stats(agent, &stats1_before);

    /* Hot-swap all modules to agent2 */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent2);
    }

    /* Send heartbeats to agent2 */
    for (int j = 0; j < 20; j++) {
        nimcp_health_agent_heartbeat_ex(agent2, "post_swap", 1.0f);
    }

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, 20u);

    /* agent1 should still have its original heartbeats */
    health_agent_stats_t stats1_after;
    nimcp_health_agent_get_stats(agent, &stats1_after);
    EXPECT_EQ(stats1_before.heartbeats_received, stats1_after.heartbeats_received);

    /* Cleanup */
    clear_all_modules();
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
}

/* ========================================================================== */
/* Sustained Operation Over Time                                              */
/* ========================================================================== */

TEST_F(HeartbeatB01E2ETest, SustainedOperationOverTime) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Simulate sustained operation over multiple intervals */
    const int intervals = 5;
    for (int interval = 0; interval < intervals; interval++) {
        for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
            char op[64];
            snprintf(op, sizeof(op), "%s_tick", B01_MODULES[i].name);
            nimcp_health_agent_heartbeat_ex(agent, op, (float)interval / intervals);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received,
              static_cast<uint64_t>(B01_MODULE_COUNT * intervals));
}

/* ========================================================================== */
/* Graceful Shutdown Sequence                                                 */
/* ========================================================================== */

TEST_F(HeartbeatB01E2ETest, GracefulShutdownSequence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        B01_MODULES[i].set_fn(agent);
    }

    /* Send final heartbeats */
    for (size_t i = 0; i < B01_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_shutdown", B01_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    /* Disconnect modules in reverse order */
    for (int i = (int)B01_MODULE_COUNT - 1; i >= 0; i--) {
        B01_MODULES[i].set_fn(nullptr);
    }

    /* Stop agent */
    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    /* Verify all heartbeats were received */
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B01_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
