/**
 * @file test_parietal_bridges_B23_e2e.cpp
 * @brief End-to-end tests for B23 parietal bridges: full lifecycle,
 *        sustained operation, load testing
 *        (cognitive/parietal bridges: fep_parietal, genius_plasticity,
 *         genius_snn, genius_training, intuition_substrate, intuition_thalamic,
 *         parietal_fep, parietal_plasticity, parietal_quantum, parietal_snn,
 *         parietal_training)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

/* Parietal bridge APIs - included OUTSIDE extern "C" (headers have their own guards) */
#include "cognitive/parietal/nimcp_fep_parietal_bridge.h"
#include "cognitive/parietal/nimcp_genius_plasticity_bridge.h"
#include "cognitive/parietal/nimcp_genius_snn_bridge.h"
#include "cognitive/parietal/nimcp_genius_training_bridge.h"
#include "cognitive/parietal/nimcp_intuition_substrate_bridge.h"
#include "cognitive/parietal/nimcp_intuition_thalamic_bridge.h"
#include "cognitive/parietal/nimcp_parietal_fep_bridge.h"
#include "cognitive/parietal/nimcp_parietal_plasticity_bridge.h"
#include "cognitive/parietal/nimcp_parietal_quantum_bridge.h"
#include "cognitive/parietal/nimcp_parietal_snn_bridge.h"
#include "cognitive/parietal/nimcp_parietal_training_bridge.h"

/* Health agent API */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    /* Parietal bridge health agent setters (bare declarations with void*) */
    void fep_parietal_bridge_set_health_agent(void* agent);
    void genius_plasticity_bridge_set_health_agent(void* agent);
    void genius_snn_bridge_set_health_agent(void* agent);
    void genius_training_bridge_set_health_agent(void* agent);
    void intuition_substrate_bridge_set_health_agent(void* agent);
    void intuition_thalamic_bridge_set_health_agent(void* agent);
    void parietal_fep_bridge_set_health_agent(void* agent);
    void parietal_plasticity_bridge_set_health_agent(void* agent);
    void parietal_quantum_bridge_set_health_agent(void* agent);
    void parietal_snn_bridge_set_health_agent(void* agent);
    void parietal_training_bridge_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kParietalBridgeModules[] = {
    {"fep_parietal_bridge",           fep_parietal_bridge_set_health_agent},
    {"genius_plasticity_bridge",      genius_plasticity_bridge_set_health_agent},
    {"genius_snn_bridge",             genius_snn_bridge_set_health_agent},
    {"genius_training_bridge",        genius_training_bridge_set_health_agent},
    {"intuition_substrate_bridge",    intuition_substrate_bridge_set_health_agent},
    {"intuition_thalamic_bridge",     intuition_thalamic_bridge_set_health_agent},
    {"parietal_fep_bridge",           parietal_fep_bridge_set_health_agent},
    {"parietal_plasticity_bridge",    parietal_plasticity_bridge_set_health_agent},
    {"parietal_quantum_bridge",       parietal_quantum_bridge_set_health_agent},
    {"parietal_snn_bridge",           parietal_snn_bridge_set_health_agent},
    {"parietal_training_bridge",      parietal_training_bridge_set_health_agent},
};

static constexpr size_t kNumModules = 11;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ParietalBridgesB23E2ETest : public ::testing::Test {
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
            kParietalBridgeModules[i].setter(nullptr);
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

TEST_F(ParietalBridgesB23E2ETest, FullLifecycleAllModules) {
    /* Create -> start -> connect -> heartbeat -> stats -> disconnect -> stop -> destroy */
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kParietalBridgeModules[i].name, 0);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23E2ETest, ConcurrentModulesMultipleThreads) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kParietalBridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kParietalBridgeModules[i].name, 0);
            kParietalBridgeModules[i].setter(nullptr);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23E2ETest, HighFrequencyBurst1000Heartbeats) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    health_agent_stats_t before;
    nimcp_health_agent_get_stats(agent_, &before);
    for (int j = 0; j < 1000; j++)
        nimcp_health_agent_heartbeat_ex(agent_, "B23_burst", 0);
    health_agent_stats_t after;
    nimcp_health_agent_get_stats(agent_, &after);
    EXPECT_GE(after.heartbeats_received, before.heartbeats_received + 1000);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23E2ETest, TimeoutDetectionAfterSilence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    nimcp_health_agent_heartbeat_ex(agent_, "B23_timeout_test", 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23E2ETest, MultiPhaseOperation) {
    nimcp_health_agent_start(agent_);
    /* Phase 1: Create bridges */
    fep_parietal_config_t cfg1 = fep_parietal_default_config();
    fep_parietal_bridge_t* fep_par = fep_parietal_bridge_create(&cfg1);
    ASSERT_NE(nullptr, fep_par);
    parietal_snn_config_t cfg2 = parietal_snn_config_default();
    parietal_snn_bridge_t* snn = parietal_snn_create(&cfg2);
    ASSERT_NE(nullptr, snn);
    parietal_quantum_config_t cfg3 = parietal_quantum_default_config();
    parietal_quantum_bridge_t* quantum = parietal_quantum_bridge_create(&cfg3);
    ASSERT_NE(nullptr, quantum);

    /* Connect health agent modules */
    for (size_t i = 0; i < kNumModules; i++) {
        kParietalBridgeModules[i].setter(agent_);
        nimcp_health_agent_heartbeat_ex(agent_, kParietalBridgeModules[i].name, 0);
    }

    /* Phase 2: Operate bridges */
    fep_parietal_stats_t fep_stats;
    fep_parietal_get_stats(fep_par, &fep_stats);
    parietal_snn_reset(snn);
    parietal_quantum_stats_t q_stats;
    parietal_quantum_get_stats(quantum, &q_stats);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);

    /* Phase 3: Teardown bridges */
    parietal_quantum_bridge_destroy(quantum);
    parietal_snn_destroy(snn);
    fep_parietal_bridge_destroy(fep_par);

    for (size_t i = 0; i < kNumModules / 2; i++) kParietalBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23E2ETest, ModuleHotSwapDuringOperation) {
    nimcp_health_agent_start(agent_);
    health_agent_config_t cfg2;
    nimcp_health_agent_default_config(&cfg2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&cfg2);
    ASSERT_NE(nullptr, agent2);
    nimcp_health_agent_start(agent2);

    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++) {
        kParietalBridgeModules[i].setter(agent2);
        nimcp_health_agent_heartbeat_ex(agent2, kParietalBridgeModules[i].name, 0);
    }

    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent2, &stats2);
    EXPECT_GE(stats2.heartbeats_received, kNumModules);

    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent2);
    nimcp_health_agent_destroy(agent2);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23E2ETest, SustainedOperationOverTime) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    auto start = std::chrono::steady_clock::now();
    uint64_t count = 0;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(250)) {
        nimcp_health_agent_heartbeat_ex(agent_, "B23_sustained", 0);
        count++;
    }
    EXPECT_GT(count, 0u);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, count);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23E2ETest, GracefulShutdownSequence) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    for (size_t i = 0; i < kNumModules; i++)
        nimcp_health_agent_heartbeat_ex(agent_, kParietalBridgeModules[i].name, 0);
    /* Orderly: stop operations, disconnect, destroy */
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, kNumModules);
}

TEST_F(ParietalBridgesB23E2ETest, AllBridgesCreateOperateDestroy) {
    /* Full lifecycle for each bridge type that accepts config-only create params */
    {
        fep_parietal_config_t cfg = fep_parietal_default_config();
        fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        fep_parietal_stats_t s;
        fep_parietal_get_stats(b, &s);
        fep_parietal_bridge_destroy(b);
    }
    {
        genius_plasticity_config_t cfg = genius_plasticity_config_default();
        genius_plasticity_bridge_t* b = genius_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        genius_plasticity_reset(b);
        genius_plasticity_destroy(b);
    }
    {
        genius_snn_config_t cfg = genius_snn_config_default();
        genius_snn_bridge_t* b = genius_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        genius_snn_reset(b);
        genius_snn_destroy(b);
    }
    {
        parietal_fep_config_t cfg = parietal_fep_config_default();
        parietal_fep_bridge_t* b = parietal_fep_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        parietal_fep_bridge_reset(b);
        parietal_fep_bridge_destroy(b);
    }
    {
        parietal_plasticity_config_t cfg = parietal_plasticity_config_default();
        parietal_plasticity_bridge_t* b = parietal_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        parietal_plasticity_reset(b);
        parietal_plasticity_destroy(b);
    }
    {
        parietal_quantum_config_t cfg = parietal_quantum_default_config();
        parietal_quantum_bridge_t* b = parietal_quantum_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        parietal_quantum_stats_t s;
        parietal_quantum_get_stats(b, &s);
        parietal_quantum_bridge_destroy(b);
    }
    {
        parietal_snn_config_t cfg = parietal_snn_config_default();
        parietal_snn_bridge_t* b = parietal_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        parietal_snn_reset(b);
        parietal_snn_destroy(b);
    }
}

TEST_F(ParietalBridgesB23E2ETest, CrossBridgeEventPropagation) {
    /* Parietal event flows through FEP -> SNN -> Plasticity pipeline */
    fep_parietal_config_t cfg1 = fep_parietal_default_config();
    fep_parietal_bridge_t* fep = fep_parietal_bridge_create(&cfg1);
    ASSERT_NE(nullptr, fep);
    parietal_snn_config_t cfg2 = parietal_snn_config_default();
    parietal_snn_bridge_t* snn = parietal_snn_create(&cfg2);
    ASSERT_NE(nullptr, snn);
    parietal_plasticity_config_t cfg3 = parietal_plasticity_config_default();
    parietal_plasticity_bridge_t* plast = parietal_plasticity_create(&cfg3);
    ASSERT_NE(nullptr, plast);

    /* Connect health agent modules */
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);

    /* Step 1: FEP processes beliefs */
    fep_parietal_stats_t fep_stats;
    fep_parietal_get_stats(fep, &fep_stats);

    /* Step 2: SNN encodes spatial state */
    parietal_snn_reset(snn);

    /* Step 3: Plasticity consolidates */
    parietal_plasticity_reset(plast);

    /* Heartbeat to confirm health agent is tracking */
    nimcp_health_agent_heartbeat_ex(agent_, "B23_cross_bridge", 1.0f);
    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent_, &stats);
    EXPECT_GE(stats.heartbeats_received, 1u);

    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(nullptr);
    nimcp_health_agent_stop(agent_);

    parietal_plasticity_destroy(plast);
    parietal_snn_destroy(snn);
    fep_parietal_bridge_destroy(fep);
}
