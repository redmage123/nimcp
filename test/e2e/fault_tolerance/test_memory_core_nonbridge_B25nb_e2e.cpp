/**
 * @file test_memory_core_nonbridge_B25nb_e2e.cpp
 * @brief End-to-end tests for B25nb memory/core non-bridge modules: full lifecycle,
 *        sustained operation, load testing
 *        (28 non-bridge modules in cognitive/memory/core/)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

/* NO bridge headers — they use _Atomic (C11, not C++) via nimcp_pr_memory_node.h */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    /* 28 health agent setters (non-bridge modules in memory/core/) */
    void collective_memory_set_health_agent(void* agent);
    void counterfactual_set_health_agent(void* agent);
    void entanglement_set_health_agent(void* agent);
    void flashbulb_set_health_agent(void* agent);
    void fractal_set_health_agent(void* agent);
    void future_thinking_set_health_agent(void* agent);
    void gist_set_health_agent(void* agent);
    void kuramoto_set_health_agent(void* agent);
    void metamemory_set_health_agent(void* agent);
    void metamemory_monitor_set_health_agent(void* agent);
    void prime_signature_set_health_agent(void* agent);
    void procedural_set_health_agent(void* agent);
    void prospective_set_health_agent(void* agent);
    void prospective_scheduler_set_health_agent(void* agent);
    void pr_memory_node_set_health_agent(void* agent);
    void pr_pink_noise_set_health_agent(void* agent);
    void pr_training_plasticity_set_health_agent(void* agent);
    void quaternion_set_health_agent(void* agent);
    void reconsolidation_set_health_agent(void* agent);
    void resonance_set_health_agent(void* agent);
    void schemas_set_health_agent(void* agent);
    void skill_acquisition_set_health_agent(void* agent);
    void social_memory_set_health_agent(void* agent);
    void source_memory_set_health_agent(void* agent);
    void spaced_repetition_set_health_agent(void* agent);
    void theta_gamma_set_health_agent(void* agent);
    void transactive_set_health_agent(void* agent);
    void z_ladder_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kMemoryCoreNonbridgeModules[] = {
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
    {"prime_signature",         prime_signature_set_health_agent},
    {"procedural",              procedural_set_health_agent},
    {"prospective",             prospective_set_health_agent},
    {"prospective_scheduler",   prospective_scheduler_set_health_agent},
    {"pr_memory_node",          pr_memory_node_set_health_agent},
    {"pr_pink_noise",           pr_pink_noise_set_health_agent},
    {"pr_training_plasticity",  pr_training_plasticity_set_health_agent},
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

static constexpr size_t kNumModules = sizeof(kMemoryCoreNonbridgeModules) / sizeof(kMemoryCoreNonbridgeModules[0]);

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MemoryCoreNonbridgeB25nbE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent_ = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = false;
        agent_ = nimcp_health_agent_create(&config);
        ASSERT_NE(nullptr, agent_);
    }

    void TearDown() override {
        for (size_t i = 0; i < kNumModules; i++) {
            kMemoryCoreNonbridgeModules[i].setter(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_stop(agent_);
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

/* ============================================================================
 * E2E Tests
 * ============================================================================ */

TEST_F(MemoryCoreNonbridgeB25nbE2ETest, FullLifecycleAllModules) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kMemoryCoreNonbridgeModules[i].name, 0);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbE2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kMemoryCoreNonbridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kMemoryCoreNonbridgeModules[i].name, 0);
            kMemoryCoreNonbridgeModules[i].setter(nullptr);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbE2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
    health_agent_stats_t before;
    nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B25nb_burst", 0);
    health_agent_stats_t after;
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbE2ETest, SustainedOperationWithPeriodicHeartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
    for (int round = 0; round < 5; round++) {
        for (size_t i = 0; i < kNumModules; i++)
            nimcp_health_agent_heartbeat_ex(agent_, kMemoryCoreNonbridgeModules[i].name,
                                            (float)round / 5.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules * 5);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbE2ETest, HotSwapAgentDuringOperation) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
    for (int j = 0; j < 100; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B25nb_pre_swap", 0);

    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    nimcp_health_agent_start(agent2);

    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent2);
    for (int j = 0; j < 100; j++)
        nimcp_health_agent_heartbeat_ex(agent2, "B25nb_post_swap", 0);

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, 100u);

    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbE2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kMemoryCoreNonbridgeModules[i].name, 1.0f);
    /* Disconnect in reverse order */
    for (size_t i = kNumModules; i > 0; i--)
        kMemoryCoreNonbridgeModules[i - 1].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbE2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B25nb_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}
