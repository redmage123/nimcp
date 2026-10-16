/**
 * @file test_memory_core_nonbridge_B25nb_regression.cpp
 * @brief Regression tests for B25nb memory/core non-bridge modules: edge cases,
 *        stability, thread safety
 *        (28 non-bridge modules in cognitive/memory/core/)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>
#include <climits>

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

class MemoryCoreNonbridgeB25nbRegressionTest : public ::testing::Test {
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
 * Regression Tests
 * ============================================================================ */

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreNonbridgeModules[i].name);
        kMemoryCoreNonbridgeModules[i].setter(nullptr);
    }
}

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreNonbridgeModules[i].name);
        kMemoryCoreNonbridgeModules[i].setter(agent_);
        kMemoryCoreNonbridgeModules[i].setter(nullptr);
    }
}

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kMemoryCoreNonbridgeModules[i].name);
        kMemoryCoreNonbridgeModules[i].setter(agent_);
    }
}

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kMemoryCoreNonbridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kMemoryCoreNonbridgeModules[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++)
            nimcp_health_agent_heartbeat_ex(agent_, "B25nb_regression", 0);
    });
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(nullptr);
    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 100; cycle++) {
        for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
        for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(nullptr);
    }
}

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 50; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(nullptr, temp);
        for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(temp);
        for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(nullptr);
        nimcp_health_agent_destroy(temp);
    }
}

/* ============================================================================
 * Thread Safety
 * ============================================================================ */

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, ConcurrentSetClearFromMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t]() {
            for (int cycle = 0; cycle < 50; cycle++) {
                size_t idx = (t * 7 + cycle) % kNumModules;
                kMemoryCoreNonbridgeModules[idx].setter(agent_);
                nimcp_health_agent_heartbeat_ex(agent_, kMemoryCoreNonbridgeModules[idx].name, 0);
                kMemoryCoreNonbridgeModules[idx].setter(nullptr);
            }
        });
    }
    for (auto& th : threads) th.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, AllDirectoriesConcurrentSetClear) {
    nimcp_health_agent_start(agent_);
    /* 4 threads, each covering a quarter of the modules */
    std::vector<std::thread> threads;
    size_t quarter = kNumModules / 4;
    for (int d = 0; d < 4; d++) {
        threads.emplace_back([this, d, quarter]() {
            size_t start = d * quarter;
            size_t end = (d == 3) ? kNumModules : start + quarter;
            for (int cycle = 0; cycle < 20; cycle++) {
                for (size_t i = start; i < end; i++)
                    kMemoryCoreNonbridgeModules[i].setter(agent_);
                for (size_t i = start; i < end; i++)
                    nimcp_health_agent_heartbeat_ex(agent_, kMemoryCoreNonbridgeModules[i].name, 0);
                for (size_t i = start; i < end; i++)
                    kMemoryCoreNonbridgeModules[i].setter(nullptr);
            }
        });
    }
    for (auto& th : threads) th.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(MemoryCoreNonbridgeB25nbRegressionTest, BurstHeartbeatsDuringAgentSwap) {
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
    std::thread swapper([this, agent2]() {
        for (int c = 0; c < 50; c++) {
            for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent2);
            for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(agent_);
        }
    });
    for (int j = 0; j < 500; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B25nb_burst_swap", 0);
    swapper.join();
    for (size_t i = 0; i < kNumModules; i++) kMemoryCoreNonbridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
    nimcp_health_agent_destroy(agent2);
}
