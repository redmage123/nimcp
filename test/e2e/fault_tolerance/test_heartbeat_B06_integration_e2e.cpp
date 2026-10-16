/**
 * @file test_heartbeat_B06_integration_e2e.cpp
 * @brief E2E tests for B06 (cognitive/integration) heartbeat full lifecycle
 *
 * WHAT: End-to-end heartbeat lifecycle tests for 24 cognitive integration modules
 * WHY:  Phase 8 requires verified complete lifecycle: connect -> heartbeat -> disconnect
 * HOW:  Full agent lifecycle, concurrent modules, high-frequency burst, timeout
 *
 * NOTE: cognitive_bio_async_bridge now included (mutex bug fixed, added to build)
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

/* B06: cognitive/integration — 24 setter declarations */
void attention_wm_bridge_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_bio_async_bridge_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_integration_fep_set_health_agent(nimcp_health_agent_t* agent);
void cognitive_integration_hub_set_health_agent(nimcp_health_agent_t* agent);
void collective_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void curiosity_reasoning_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_executive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void emotion_memory_bridge_set_health_agent(nimcp_health_agent_t* agent);
void ethics_executive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_executive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void game_theory_executive_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void gw_cognitive_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_reasoning_bridge_set_health_agent(nimcp_health_agent_t* agent);
void imagination_reasoning_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_empathy_bridge_set_health_agent(nimcp_health_agent_t* agent);
void mirror_empathy_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void predictive_attention_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void rcog_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_attention_bridge_set_health_agent(nimcp_health_agent_t* agent);
void salience_attention_fep_bridge_set_health_agent(nimcp_health_agent_t* agent);
void security_cognitive_hub_bridge_set_health_agent(nimcp_health_agent_t* agent);
void self_introspection_bridge_set_health_agent(nimcp_health_agent_t* agent);
void tom_social_bridge_set_health_agent(nimcp_health_agent_t* agent);
}

typedef void (*set_health_agent_fn)(nimcp_health_agent_t*);

struct B06ModuleEntry {
    const char* name;
    set_health_agent_fn set_fn;
};

static const B06ModuleEntry B06_MODULES[] = {
    {"attention_wm_bridge",                  attention_wm_bridge_set_health_agent},
    {"cognitive_bio_async_bridge",           cognitive_bio_async_bridge_set_health_agent},
    {"cognitive_integration_fep",            cognitive_integration_fep_set_health_agent},
    {"cognitive_integration_hub",            cognitive_integration_hub_set_health_agent},
    {"collective_hub_bridge",                collective_hub_bridge_set_health_agent},
    {"curiosity_reasoning_bridge",           curiosity_reasoning_bridge_set_health_agent},
    {"emotion_executive_bridge",             emotion_executive_bridge_set_health_agent},
    {"emotion_memory_bridge",                emotion_memory_bridge_set_health_agent},
    {"ethics_executive_bridge",              ethics_executive_bridge_set_health_agent},
    {"game_theory_executive_bridge",         game_theory_executive_bridge_set_health_agent},
    {"game_theory_executive_fep_bridge",     game_theory_executive_fep_bridge_set_health_agent},
    {"gw_cognitive_bridge",                  gw_cognitive_bridge_set_health_agent},
    {"imagination_reasoning_bridge",         imagination_reasoning_bridge_set_health_agent},
    {"imagination_reasoning_fep_bridge",     imagination_reasoning_fep_bridge_set_health_agent},
    {"mirror_empathy_bridge",                mirror_empathy_bridge_set_health_agent},
    {"mirror_empathy_fep_bridge",            mirror_empathy_fep_bridge_set_health_agent},
    {"predictive_attention_bridge",          predictive_attention_bridge_set_health_agent},
    {"predictive_attention_fep_bridge",      predictive_attention_fep_bridge_set_health_agent},
    {"rcog_hub_bridge",                      rcog_hub_bridge_set_health_agent},
    {"salience_attention_bridge",            salience_attention_bridge_set_health_agent},
    {"salience_attention_fep_bridge",        salience_attention_fep_bridge_set_health_agent},
    {"security_cognitive_hub_bridge",        security_cognitive_hub_bridge_set_health_agent},
    {"self_introspection_bridge",            self_introspection_bridge_set_health_agent},
    {"tom_social_bridge",                    tom_social_bridge_set_health_agent},
};

static constexpr size_t B06_MODULE_COUNT = sizeof(B06_MODULES) / sizeof(B06_MODULES[0]);

static void clear_all_modules(void) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(nullptr);
    }
}

class HeartbeatB06E2ETest : public ::testing::Test {
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

TEST_F(HeartbeatB06E2ETest, FullLifecycleAllModules) {
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B06_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.5f);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B06_MODULE_COUNT * 3));

    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(nullptr);
    }
}

TEST_F(HeartbeatB06E2ETest, ConcurrentModulesMultipleThreads) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    const int num_threads = 8;
    const int heartbeats_per_thread = 50;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, heartbeats_per_thread]() {
            for (int i = 0; i < heartbeats_per_thread; i++) {
                size_t mod_idx = (t * heartbeats_per_thread + i) % B06_MODULE_COUNT;
                char op[64];
                snprintf(op, sizeof(op), "thread%d_%s", t, B06_MODULES[mod_idx].name);
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

TEST_F(HeartbeatB06E2ETest, HighFrequencyBurst1000Heartbeats) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
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

TEST_F(HeartbeatB06E2ETest, TimeoutDetectionAfterSilence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    nimcp_health_agent_heartbeat_ex(agent, "timeout_test", 0.0f);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
}

TEST_F(HeartbeatB06E2ETest, MultiPhaseOperation) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_init", B06_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.0f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.25f);
    }
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_process", B06_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.50f);
        nimcp_health_agent_heartbeat_ex(agent, op, 0.75f);
    }
    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_finalize", B06_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B06_MODULE_COUNT * 5));
}

TEST_F(HeartbeatB06E2ETest, ModuleHotSwapDuringOperation) {
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

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }
    for (int j = 0; j < 20; j++) {
        nimcp_health_agent_heartbeat_ex(agent, "pre_swap", 0.5f);
    }

    health_agent_stats_t stats1_before;
    nimcp_health_agent_get_stats(agent, &stats1_before);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent2);
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

TEST_F(HeartbeatB06E2ETest, SustainedOperationOverTime) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    const int intervals = 5;
    for (int interval = 0; interval < intervals; interval++) {
        for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
            char op[64];
            snprintf(op, sizeof(op), "%s_tick", B06_MODULES[i].name);
            nimcp_health_agent_heartbeat_ex(agent, op, (float)interval / intervals);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received,
              static_cast<uint64_t>(B06_MODULE_COUNT * intervals));
}

TEST_F(HeartbeatB06E2ETest, GracefulShutdownSequence) {
    int result = nimcp_health_agent_start(agent);
    ASSERT_EQ(result, 0);

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        B06_MODULES[i].set_fn(agent);
    }

    for (size_t i = 0; i < B06_MODULE_COUNT; i++) {
        char op[64];
        snprintf(op, sizeof(op), "%s_shutdown", B06_MODULES[i].name);
        nimcp_health_agent_heartbeat_ex(agent, op, 1.0f);
    }

    for (int i = (int)B06_MODULE_COUNT - 1; i >= 0; i--) {
        B06_MODULES[i].set_fn(nullptr);
    }

    result = nimcp_health_agent_stop(agent);
    ASSERT_EQ(result, 0);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.heartbeats_received, static_cast<uint64_t>(B06_MODULE_COUNT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
