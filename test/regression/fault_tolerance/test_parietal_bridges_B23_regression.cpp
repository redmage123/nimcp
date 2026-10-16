/**
 * @file test_parietal_bridges_B23_regression.cpp
 * @brief Regression tests for B23 parietal bridges: edge cases, stability,
 *        thread safety
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
#include <climits>

/* Parietal bridge APIs - includes outside extern "C" (headers have their own guards) */
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
    /* Parietal bridge health agent setters (not declared in headers) */
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

class ParietalBridgesB23RegressionTest : public ::testing::Test {
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
 * Regression Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23RegressionTest, NullAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kParietalBridgeModules[i].name);
        kParietalBridgeModules[i].setter(nullptr);
    }
}

TEST_F(ParietalBridgesB23RegressionTest, NullAlwaysAcceptedAfterValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kParietalBridgeModules[i].name);
        kParietalBridgeModules[i].setter(agent_);
        kParietalBridgeModules[i].setter(nullptr);
    }
}

TEST_F(ParietalBridgesB23RegressionTest, ValidAgentAlwaysAcceptedOnFirstCall) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kParietalBridgeModules[i].name);
        kParietalBridgeModules[i].setter(agent_);
    }
}

TEST_F(ParietalBridgesB23RegressionTest, SetDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumModules; i++) {
        threads.emplace_back([this, i]() {
            kParietalBridgeModules[i].setter(agent_);
            for (int j = 0; j < 20; j++)
                nimcp_health_agent_heartbeat_ex(agent_, kParietalBridgeModules[i].name, 0);
        });
    }
    for (auto& t : threads) t.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23RegressionTest, ClearDuringActiveHeartbeatsNoCrash) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    std::thread heartbeat_thread([this]() {
        for (int j = 0; j < 100; j++)
            nimcp_health_agent_heartbeat_ex(agent_, "B23_regression", 0);
    });
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(nullptr);
    heartbeat_thread.join();
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23RegressionTest, RapidSetClearCycleAllModules) {
    for (int cycle = 0; cycle < 100; cycle++) {
        for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
        for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(nullptr);
    }
}

TEST_F(ParietalBridgesB23RegressionTest, MultipleAgentCreationDestructionCycle) {
    for (int cycle = 0; cycle < 50; cycle++) {
        health_agent_config_t cfg_temp;
        nimcp_health_agent_default_config(&cfg_temp);
        nimcp_health_agent_t* temp = nimcp_health_agent_create(&cfg_temp);
        ASSERT_NE(nullptr, temp);
        for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(temp);
        for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(nullptr);
        nimcp_health_agent_destroy(temp);
    }
}

/* ============================================================================
 * Bridge-Specific Boundary Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23RegressionTest, FepParietalBoundaryValues) {
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* bridge = fep_parietal_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    /* Set and verify boundary inflammation/fatigue values */
    int rc = fep_parietal_set_inflammation(bridge, 0.0f);
    EXPECT_EQ(0, rc);
    rc = fep_parietal_set_inflammation(bridge, 1.0f);
    EXPECT_EQ(0, rc);
    rc = fep_parietal_set_fatigue(bridge, 0.0f);
    EXPECT_EQ(0, rc);
    rc = fep_parietal_set_fatigue(bridge, 1.0f);
    EXPECT_EQ(0, rc);

    fep_parietal_bridge_destroy(bridge);
}

TEST_F(ParietalBridgesB23RegressionTest, ParietalFepBoundaryValues) {
    parietal_fep_config_t cfg = parietal_fep_config_default();
    parietal_fep_bridge_t* bridge = parietal_fep_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    /* Reset and verify stats */
    int rc = parietal_fep_bridge_reset(bridge);
    EXPECT_EQ(0, rc);

    parietal_fep_stats_t stats;
    rc = parietal_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, rc);

    parietal_fep_bridge_destroy(bridge);
}

TEST_F(ParietalBridgesB23RegressionTest, ParietalQuantumBoundaryValues) {
    parietal_quantum_config_t cfg = parietal_quantum_default_config();
    parietal_quantum_bridge_t* bridge = parietal_quantum_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    /* Set boundary inflammation/fatigue values */
    int rc = parietal_quantum_set_inflammation(bridge, 0.0f);
    EXPECT_EQ(0, rc);
    rc = parietal_quantum_set_fatigue(bridge, 0.0f);
    EXPECT_EQ(0, rc);

    parietal_quantum_bridge_destroy(bridge);
}

TEST_F(ParietalBridgesB23RegressionTest, GeniusPlasticityBoundaryReset) {
    genius_plasticity_config_t cfg = genius_plasticity_config_default();
    genius_plasticity_bridge_t* bridge = genius_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    /* Multiple resets should be safe */
    int rc = genius_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    rc = genius_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);

    genius_plasticity_destroy(bridge);
}

TEST_F(ParietalBridgesB23RegressionTest, ParietalSnnBoundaryValues) {
    parietal_snn_config_t cfg = parietal_snn_config_default();
    parietal_snn_bridge_t* bridge = parietal_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    /* Reset should be safe */
    int rc = parietal_snn_reset(bridge);
    EXPECT_EQ(0, rc);

    parietal_snn_destroy(bridge);
}

/* ============================================================================
 * Comprehensive Null Safety
 * ============================================================================ */

TEST_F(ParietalBridgesB23RegressionTest, AllBridgesNullDestroyIdempotent) {
    /* destroy(NULL) should be safe for all 11 bridge types */
    fep_parietal_bridge_destroy(nullptr);
    genius_plasticity_destroy(nullptr);
    genius_snn_destroy(nullptr);
    genius_training_destroy(nullptr);
    intuition_substrate_bridge_destroy(nullptr);
    intuition_thalamic_bridge_destroy(nullptr);
    parietal_fep_bridge_destroy(nullptr);
    parietal_plasticity_destroy(nullptr);
    parietal_quantum_bridge_destroy(nullptr);
    parietal_snn_destroy(nullptr);
    parietal_training_destroy(nullptr);
}

TEST_F(ParietalBridgesB23RegressionTest, AllBridgesDoubleDestroy) {
    /* Create and destroy each bridge that accepts config-only create params,
     * then call destroy(NULL) to verify safety. */

    /* fep_parietal */
    {
        fep_parietal_config_t cfg = fep_parietal_default_config();
        fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        fep_parietal_bridge_destroy(b);
        fep_parietal_bridge_destroy(nullptr);
    }

    /* genius_plasticity */
    {
        genius_plasticity_config_t cfg = genius_plasticity_config_default();
        genius_plasticity_bridge_t* b = genius_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        genius_plasticity_destroy(b);
        genius_plasticity_destroy(nullptr);
    }

    /* genius_snn */
    {
        genius_snn_config_t cfg = genius_snn_config_default();
        genius_snn_bridge_t* b = genius_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        genius_snn_destroy(b);
        genius_snn_destroy(nullptr);
    }

    /* parietal_fep */
    {
        parietal_fep_config_t cfg = parietal_fep_config_default();
        parietal_fep_bridge_t* b = parietal_fep_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        parietal_fep_bridge_destroy(b);
        parietal_fep_bridge_destroy(nullptr);
    }

    /* parietal_plasticity */
    {
        parietal_plasticity_config_t cfg = parietal_plasticity_config_default();
        parietal_plasticity_bridge_t* b = parietal_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        parietal_plasticity_destroy(b);
        parietal_plasticity_destroy(nullptr);
    }

    /* parietal_quantum */
    {
        parietal_quantum_config_t cfg = parietal_quantum_default_config();
        parietal_quantum_bridge_t* b = parietal_quantum_bridge_create(&cfg);
        ASSERT_NE(nullptr, b);
        parietal_quantum_bridge_destroy(b);
        parietal_quantum_bridge_destroy(nullptr);
    }

    /* parietal_snn */
    {
        parietal_snn_config_t cfg = parietal_snn_config_default();
        parietal_snn_bridge_t* b = parietal_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        parietal_snn_destroy(b);
        parietal_snn_destroy(nullptr);
    }

    /* Bridges requiring non-NULL params: only test destroy(NULL) */
    genius_training_destroy(nullptr);
    intuition_substrate_bridge_destroy(nullptr);
    intuition_thalamic_bridge_destroy(nullptr);
    parietal_training_destroy(nullptr);
}

/* ============================================================================
 * Misc Regression Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23RegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kParietalBridgeModules[i].name);
        EXPECT_NE(kParietalBridgeModules[i].setter, nullptr);
        kParietalBridgeModules[i].setter(agent_);
        kParietalBridgeModules[i].setter(nullptr);
    }
}

TEST_F(ParietalBridgesB23RegressionTest, StatsConsistentAfterOperations) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(agent_);
    health_agent_stats_t stats1;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_heartbeat_ex(agent_, "B23_consistency", 0);
    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent_, &stats2);
    EXPECT_GE(stats2.heartbeats_received, stats1.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}

TEST_F(ParietalBridgesB23RegressionTest, ParietalPlasticityMultipleResets) {
    parietal_plasticity_config_t cfg = parietal_plasticity_config_default();
    parietal_plasticity_bridge_t* bridge = parietal_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    /* Multiple resets should be safe */
    for (int i = 0; i < 10; i++) {
        int rc = parietal_plasticity_reset(bridge);
        EXPECT_EQ(0, rc);
    }

    parietal_plasticity_destroy(bridge);
}

TEST_F(ParietalBridgesB23RegressionTest, GeniusSnnMultipleResets) {
    genius_snn_config_t cfg = genius_snn_config_default();
    genius_snn_bridge_t* bridge = genius_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);

    /* Multiple resets should be safe */
    for (int i = 0; i < 10; i++) {
        int rc = genius_snn_reset(bridge);
        EXPECT_EQ(0, rc);
    }

    genius_snn_destroy(bridge);
}
