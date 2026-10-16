/**
 * @file test_heartbeat_B02_memory_e2e.cpp
 * @brief E2E tests for B02 (cognitive/memory non-core) heartbeat full lifecycle
 *
 * WHAT: End-to-end heartbeat lifecycle tests for 13 memory (non-core) modules
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

class HeartbeatB02E2ETest : public ::testing::Test {
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

TEST_F(HeartbeatB02E2ETest, FullLifecycleAllModules) {
    /* 1. Connect all modules */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    /* 2. Start agent */
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    /* 3. Send heartbeats simulating module activity */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B02_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.5f);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    /* 4. Verify heartbeat reception */
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B02_MODULE_COUNT * 3));

    /* 5. Stop agent */
    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    /* 6. Disconnect all modules */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(nullptr);
    }
}

/* ========================================================================== */
/* Concurrent Modules From Multiple Threads                                   */
/* ========================================================================== */

TEST_F(HeartbeatB02E2ETest, ConcurrentModulesMultipleThreads) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    const int num_threads = 8;
    const int heartbeats_per_thread = 50;
    std::atomic<int> total_sent{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, heartbeats_per_thread, &total_sent]() {
            for (int i = 0; i < heartbeats_per_thread; i++) {
                size_t mod_idx = (t * heartbeats_per_thread + i) % B02_MODULE_COUNT;
                char op[64];
                snprintf(op, sizeof(op), "thread%d_%s", t, B02_MODULES[mod_idx].name);
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

TEST_F(HeartbeatB02E2ETest, HighFrequencyBurst1000Heartbeats) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

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

TEST_F(HeartbeatB02E2ETest, TimeoutDetectionAfterSilence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    nimcp_health_agent_heartbeat_ex(agent, "timeout_test", 0.0f);

    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
}

/* ========================================================================== */
/* Complete Multi-Phase Operation                                             */
/* ========================================================================== */

TEST_F(HeartbeatB02E2ETest, MultiPhaseOperation) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    /* Phase 1: Initialization (0.0 - 0.25) */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B02_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.25f);
    }

    /* Phase 2: Processing (0.25 - 0.75) */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B02_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.50f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.75f);
    }

    /* Phase 3: Finalization (0.75 - 1.0) */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_finalize", B02_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    /* 5 heartbeats per module * 13 modules = 65 */
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B02_MODULE_COUNT * 5));
}

/* ========================================================================== */
/* Module Hot-Swap During Operation                                           */
/* ========================================================================== */

TEST_F(HeartbeatB02E2ETest, ModuleHotSwapDuringOperation) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    config2.check_interval_ms = 50;
    config2.heartbeat_interval_ms = 100;
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(agent2, nullptr);
    result = nimcp_health_agent_start(agent2);
    ASSERT_EQ(result, 0);

    /* Connect all to agent1 */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    for (int j = 0; j < 20; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_swap", 0.5f);
    }

    health_agent_stats_t stats1_before;
    nimcp_health_agent_get_stats(agent, &stats1_before);

    /* Hot-swap all modules to agent2 */
    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent2);
    }

    for (int j = 0; j < 20; j++) {
        nimcp_health_agent_heartbeat_ex(agent2, "post_swap", 1.0f);
    }

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, 20u);

    health_agent_stats_t stats1_after;
    nimcp_health_agent_get_stats(agent, &stats1_after);
    EXPECT_EQ(stats1_before.heartbeats_received, stats1_after.heartbeats_received);

    clear_all_modules();
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
}

/* ========================================================================== */
/* Sustained Operation Over Time                                              */
/* ========================================================================== */

TEST_F(HeartbeatB02E2ETest, SustainedOperationOverTime) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    const int intervals = 5;
    for (int interval = 0; interval < intervals; interval++) {
        for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
            char op[64];
            snprintf(op, sizeof(op), "%s_tick", B02_MODULES[i].name);
            nimcp_health_agent_heartbeat_ex(agent, op, (float)interval / intervals);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received,
              static_cast<uint64_t>(B02_MODULE_COUNT * intervals));
}

/* ========================================================================== */
/* Graceful Shutdown Sequence                                                 */
/* ========================================================================== */

TEST_F(HeartbeatB02E2ETest, GracefulShutdownSequence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        B02_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B02_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_shutdown", B02_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    /* Disconnect modules in reverse order */
    for (int i = (int)B02_MODULE_COUNT - 1; i >= 0; i--) {
        B02_MODULES[i].set_fn(nullptr);
    }

    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B02_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
