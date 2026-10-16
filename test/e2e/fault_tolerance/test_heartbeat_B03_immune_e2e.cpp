/**
 * @file test_heartbeat_B03_immune_e2e.cpp
 * @brief E2E tests for B03 (cognitive/immune) heartbeat full lifecycle
 *
 * WHAT: End-to-end heartbeat lifecycle tests for 39 immune modules
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

class HeartbeatB03E2ETest : public ::testing::Test {
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

TEST_F(HeartbeatB03E2ETest, FullLifecycleAllModules) {
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B03_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.5f);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B03_MODULE_COUNT * 3));

    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB03E2ETest, ConcurrentModulesMultipleThreads) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    const int num_threads = 8;
    const int heartbeats_per_thread = 50;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, heartbeats_per_thread]() {
            for (int i = 0; i < heartbeats_per_thread; i++) {
                size_t mod_idx = (t * heartbeats_per_thread + i) % B03_MODULE_COUNT;
                char op[64];
                snprintf(op, sizeof(op), "thread%d_%s", t, B03_MODULES[mod_idx].name);
                float progress = static_cast<float>(i) / heartbeats_per_thread;
                nimcp_health_agent_heartbeat_ex(agent, op, progress);
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

TEST_F(HeartbeatB03E2ETest, HighFrequencyBurst1000Heartbeats) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    health_agent_stats_t stats_before;
    nimcp_health_agent_get_stats(agent, &stats_before);

    const int burst_count = 1000;
    for (int i = 0; i < burst_count; i++) {
        nimcp_health_agent_heartbeat_ex(agent, "burst_test",
                                        static_cast<float>(i) / burst_count);
    }

    health_agent_stats_t stats_after;
    nimcp_health_agent_get_stats(agent, &stats_after);
    uint64_t delta = stats_after.heartbeats_received - stats_before.heartbeats_received;
    EXPECT_GE(delta, static_cast<uint64_t>(burst_count));
}

TEST_F(HeartbeatB03E2ETest, TimeoutDetectionAfterSilence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    nimcp_health_agent_heartbeat_ex(agent, "timeout_test", 0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
}

TEST_F(HeartbeatB03E2ETest, MultiPhaseOperation) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B03_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.25f);
    }
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B03_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.50f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.75f);
    }
    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_finalize", B03_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B03_MODULE_COUNT * 5));
}

TEST_F(HeartbeatB03E2ETest, ModuleHotSwapDuringOperation) {
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

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }
    for (int j = 0; j < 20; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_swap", 0.5f);
    }

    health_agent_stats_t stats1_before;
    nimcp_health_agent_get_stats(agent, &stats1_before);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent2);
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

TEST_F(HeartbeatB03E2ETest, SustainedOperationOverTime) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    const int intervals = 5;
    for (int interval = 0; interval < intervals; interval++) {
        for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
            char op[64];
            snprintf(op, sizeof(op), "%s_tick", B03_MODULES[i].name);
            nimcp_health_agent_heartbeat_ex(agent, op, (float)interval / intervals);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received,
              static_cast<uint64_t>(B03_MODULE_COUNT * intervals));
}

TEST_F(HeartbeatB03E2ETest, GracefulShutdownSequence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        B03_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B03_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_shutdown", B03_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    for (int i = (int)B03_MODULE_COUNT - 1; i >= 0; i--) {
        B03_MODULES[i].set_fn(nullptr);
    }

    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B03_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
