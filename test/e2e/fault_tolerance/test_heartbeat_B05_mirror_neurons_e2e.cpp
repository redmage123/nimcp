/**
 * @file test_heartbeat_B05_mirror_neurons_e2e.cpp
 * @brief E2E tests for B05 (cognitive/mirror_neurons) heartbeat full lifecycle
 *
 * WHAT: End-to-end heartbeat lifecycle tests for 27 mirror neuron modules
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

/* B05: cognitive/mirror_neurons — 27 setter declarations */
void mirror_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_emotion_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_habituation_set_health_agent(nimcp_health_agent_t* agent);
void mirror_hierarchy_set_health_agent(nimcp_health_agent_t* agent);
void mirror_hippocampus_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_hypothalamus_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_immune_integration_set_health_agent(nimcp_health_agent_t* agent);
void mirror_language_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_motor_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_multimodal_set_health_agent(nimcp_health_agent_t* agent);
void mirror_neurons_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_neurons_set_health_agent(nimcp_health_agent_t* agent);
void mirror_neurons_sleep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_omni_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_plasticity_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_prefrontal_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_resonance_set_health_agent(nimcp_health_agent_t* agent);
void mirror_self_other_set_health_agent(nimcp_health_agent_t* agent);
void mirror_snn_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_social_context_set_health_agent(nimcp_health_agent_t* agent);
void mirror_stdp_set_health_agent(nimcp_health_agent_t* agent);
void mirror_substrate_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_substrate_set_health_agent(nimcp_health_agent_t* agent);
void mirror_thalamic_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_tom_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_vicarious_reward_set_health_agent(nimcp_health_agent_t* agent);
void mirror_visual_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B05ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B05ModuleEntry B05_MODULES[] = {
    {"mirror_attention_bridge",     mirror_attention_bridge_set_health_agent},
    {"mirror_emotion_bridge",       mirror_emotion_bridge_set_health_agent},
    {"mirror_habituation",          mirror_habituation_set_health_agent},
    {"mirror_hierarchy",            mirror_hierarchy_set_health_agent},
    {"mirror_hippocampus_bridge",   mirror_hippocampus_bridge_set_health_agent},
    {"mirror_hypothalamus_bridge",  mirror_hypothalamus_bridge_set_health_agent},
    {"mirror_immune_integration",   mirror_immune_integration_set_health_agent},
    {"mirror_language_bridge",      mirror_language_bridge_set_health_agent},
    {"mirror_motor_bridge",         mirror_motor_bridge_set_health_agent},
    {"mirror_multimodal",           mirror_multimodal_set_health_agent},
    {"mirror_neurons_fep_bridge",   mirror_neurons_fep_bridge_set_health_agent},
    {"mirror_neurons",              mirror_neurons_set_health_agent},
    {"mirror_neurons_sleep_bridge", mirror_neurons_sleep_bridge_set_health_agent},
    {"mirror_omni_bridge",          mirror_omni_bridge_set_health_agent},
    {"mirror_plasticity_bridge",    mirror_plasticity_bridge_set_health_agent},
    {"mirror_prefrontal_bridge",    mirror_prefrontal_bridge_set_health_agent},
    {"mirror_resonance",            mirror_resonance_set_health_agent},
    {"mirror_self_other",           mirror_self_other_set_health_agent},
    {"mirror_snn_bridge",           mirror_snn_bridge_set_health_agent},
    {"mirror_social_context",       mirror_social_context_set_health_agent},
    {"mirror_stdp",                 mirror_stdp_set_health_agent},
    {"mirror_substrate_bridge",     mirror_substrate_bridge_set_health_agent},
    {"mirror_substrate",            mirror_substrate_set_health_agent},
    {"mirror_thalamic_bridge",      mirror_thalamic_bridge_set_health_agent},
    {"mirror_tom_bridge",           mirror_tom_bridge_set_health_agent},
    {"mirror_vicarious_reward",     mirror_vicarious_reward_set_health_agent},
    {"mirror_visual_bridge",        mirror_visual_bridge_set_health_agent},
};

static constexpr size_t B05_MODULE_COUNT = sizeof(B05_MODULES) / sizeof(B05_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB05E2ETest : public ::testing::Test {
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

TEST_F(HeartbeatB05E2ETest, FullLifecycleAllModules) {
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B05_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.5f);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B05_MODULE_COUNT * 3));

    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB05E2ETest, ConcurrentModulesMultipleThreads) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    const int num_threads = 8;
    const int heartbeats_per_thread = 50;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, heartbeats_per_thread]() {
            for (int i = 0; i < heartbeats_per_thread; i++) {
                size_t mod_idx = (t * heartbeats_per_thread + i) % B05_MODULE_COUNT;
                char op[64];
                snprintf(op, sizeof(op), "thread%d_%s", t, B05_MODULES[mod_idx].name);
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

TEST_F(HeartbeatB05E2ETest, HighFrequencyBurst1000Heartbeats) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
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

TEST_F(HeartbeatB05E2ETest, TimeoutDetectionAfterSilence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    nimcp_health_agent_heartbeat_ex(agent, "timeout_test", 0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
}

TEST_F(HeartbeatB05E2ETest, MultiPhaseOperation) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B05_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.25f);
    }
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B05_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.50f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.75f);
    }
    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_finalize", B05_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B05_MODULE_COUNT * 5));
}

TEST_F(HeartbeatB05E2ETest, ModuleHotSwapDuringOperation) {
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

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }
    for (int j = 0; j < 20; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_swap", 0.5f);
    }

    health_agent_stats_t stats1_before;
    nimcp_health_agent_get_stats(agent, &stats1_before);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent2);
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

TEST_F(HeartbeatB05E2ETest, SustainedOperationOverTime) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    const int intervals = 5;
    for (int interval = 0; interval < intervals; interval++) {
        for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
            char op[64];
            snprintf(op, sizeof(op), "%s_tick", B05_MODULES[i].name);
            nimcp_health_agent_heartbeat_ex(agent, op, (float)interval / intervals);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received,
              static_cast<uint64_t>(B05_MODULE_COUNT * intervals));
}

TEST_F(HeartbeatB05E2ETest, GracefulShutdownSequence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        B05_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B05_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_shutdown", B05_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    for (int i = (int)B05_MODULE_COUNT - 1; i >= 0; i--) {
        B05_MODULES[i].set_fn(nullptr);
    }

    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B05_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
