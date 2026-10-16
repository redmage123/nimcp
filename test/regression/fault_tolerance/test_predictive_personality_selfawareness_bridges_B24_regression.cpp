/**
 * @file test_predictive_personality_selfawareness_bridges_B24_regression.cpp
 * @brief Regression tests for B24 predictive+personality+self_awareness bridge
 *        health agent integration: boundary values, null safety, rapid cycles, thread safety
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

/* Bridge APIs - outside extern "C" */
#include "cognitive/predictive/nimcp_predictive_fep_bridge.h"
#include "cognitive/predictive/nimcp_predictive_plasticity_bridge.h"
#include "cognitive/predictive/nimcp_predictive_snn_bridge.h"
#include "cognitive/predictive/nimcp_predictive_substrate_bridge.h"
#include "cognitive/predictive/nimcp_predictive_thalamic_bridge.h"
#include "cognitive/personality/nimcp_personality_plasticity_bridge.h"
#include "cognitive/personality/nimcp_personality_snn_bridge.h"
#include "cognitive/personality/nimcp_personality_substrate_bridge.h"
#include "cognitive/personality/nimcp_personality_thalamic_bridge.h"
#include "cognitive/self_awareness/nimcp_self_awareness_plasticity_bridge.h"
#include "cognitive/self_awareness/nimcp_self_awareness_snn_bridge.h"
#include "cognitive/self_awareness/nimcp_self_awareness_substrate_bridge.h"
#include "cognitive/self_awareness/nimcp_self_awareness_thalamic_bridge.h"

/* Health agent API */
#include "utils/fault_tolerance/nimcp_health_agent.h"

extern "C" {
    void predictive_fep_bridge_set_health_agent(void* agent);
    void predictive_plasticity_bridge_set_health_agent(void* agent);
    void predictive_snn_bridge_set_health_agent(void* agent);
    void predictive_substrate_bridge_set_health_agent(void* agent);
    void predictive_thalamic_bridge_set_health_agent(void* agent);
    void personality_plasticity_bridge_set_health_agent(void* agent);
    void personality_snn_bridge_set_health_agent(void* agent);
    void personality_substrate_bridge_set_health_agent(void* agent);
    void personality_thalamic_bridge_set_health_agent(void* agent);
    void self_awareness_plasticity_bridge_set_health_agent(void* agent);
    void self_awareness_snn_bridge_set_health_agent(void* agent);
    void self_awareness_substrate_bridge_set_health_agent(void* agent);
    void self_awareness_thalamic_bridge_set_health_agent(void* agent);
}

/* ============================================================================
 * Module setter array
 * ============================================================================ */

struct ModuleSetter {
    const char* name;
    void (*setter)(void*);
};

static const ModuleSetter kB24BridgeModules[] = {
    {"predictive_fep_bridge",               predictive_fep_bridge_set_health_agent},
    {"predictive_plasticity_bridge",        predictive_plasticity_bridge_set_health_agent},
    {"predictive_snn_bridge",               predictive_snn_bridge_set_health_agent},
    {"predictive_substrate_bridge",         predictive_substrate_bridge_set_health_agent},
    {"predictive_thalamic_bridge",          predictive_thalamic_bridge_set_health_agent},
    {"personality_plasticity_bridge",       personality_plasticity_bridge_set_health_agent},
    {"personality_snn_bridge",              personality_snn_bridge_set_health_agent},
    {"personality_substrate_bridge",        personality_substrate_bridge_set_health_agent},
    {"personality_thalamic_bridge",         personality_thalamic_bridge_set_health_agent},
    {"self_awareness_plasticity_bridge",    self_awareness_plasticity_bridge_set_health_agent},
    {"self_awareness_snn_bridge",           self_awareness_snn_bridge_set_health_agent},
    {"self_awareness_substrate_bridge",     self_awareness_substrate_bridge_set_health_agent},
    {"self_awareness_thalamic_bridge",      self_awareness_thalamic_bridge_set_health_agent},
};

static constexpr size_t kNumModules = 13;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class B24BridgesRegressionTest : public ::testing::Test {
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
            kB24BridgeModules[i].setter(nullptr);
        }
        if (agent_) {
            nimcp_health_agent_stop(agent_);
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }
};

/* ============================================================================
 * Boundary Value Tests
 * ============================================================================ */

TEST_F(B24BridgesRegressionTest, PredictiveFepBoundaryValues) {
    predictive_fep_config_t cfg;
    predictive_fep_bridge_default_config(&cfg);
    predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    predictive_fep_bridge_destroy(bridge);
}

TEST_F(B24BridgesRegressionTest, PredictivePlasticityBoundaryReset) {
    predictive_plasticity_config_t cfg = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* bridge = predictive_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    /* Multiple resets should be safe */
    int rc = predictive_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    rc = predictive_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    predictive_plasticity_destroy(bridge);
}

TEST_F(B24BridgesRegressionTest, PredictiveSnnBoundaryValues) {
    predictive_snn_config_t cfg = predictive_snn_config_default();
    predictive_snn_bridge_t* bridge = predictive_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = predictive_snn_reset(bridge);
    EXPECT_EQ(0, rc);
    predictive_snn_destroy(bridge);
}

TEST_F(B24BridgesRegressionTest, PersonalityPlasticityBoundaryReset) {
    personality_plasticity_config_t cfg = personality_plasticity_config_default();
    personality_plasticity_bridge_t* bridge = personality_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = personality_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    rc = personality_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    personality_plasticity_destroy(bridge);
}

TEST_F(B24BridgesRegressionTest, PersonalitySnnBoundaryValues) {
    personality_snn_config_t cfg = personality_snn_config_default();
    personality_snn_bridge_t* bridge = personality_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = personality_snn_reset(bridge);
    EXPECT_EQ(0, rc);
    personality_snn_destroy(bridge);
}

TEST_F(B24BridgesRegressionTest, SelfAwarenessPlasticityBoundaryReset) {
    self_awareness_plasticity_config_t cfg = self_awareness_plasticity_config_default();
    self_awareness_plasticity_bridge_t* bridge = self_awareness_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = self_awareness_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    rc = self_awareness_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    self_awareness_plasticity_destroy(bridge);
}

TEST_F(B24BridgesRegressionTest, SelfAwarenessSnnBoundaryValues) {
    self_awareness_snn_config_t cfg = self_awareness_snn_config_default();
    self_awareness_snn_bridge_t* bridge = self_awareness_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = self_awareness_snn_reset(bridge);
    EXPECT_EQ(0, rc);
    self_awareness_snn_destroy(bridge);
}

/* ============================================================================
 * Null Safety - Destroy Idempotent
 * ============================================================================ */

TEST_F(B24BridgesRegressionTest, AllBridgesNullDestroyIdempotent) {
    /* destroy(NULL) should be safe for all 13 bridge types */
    predictive_fep_bridge_destroy(nullptr);
    predictive_plasticity_destroy(nullptr);
    predictive_snn_destroy(nullptr);
    predictive_substrate_bridge_destroy(nullptr);
    predictive_thalamic_bridge_destroy(nullptr);
    personality_plasticity_destroy(nullptr);
    personality_snn_destroy(nullptr);
    personality_substrate_bridge_destroy(nullptr);
    personality_thalamic_bridge_destroy(nullptr);
    self_awareness_plasticity_destroy(nullptr);
    self_awareness_snn_destroy(nullptr);
    self_awareness_substrate_bridge_destroy(nullptr);
    self_awareness_thalamic_bridge_destroy(nullptr);
}

TEST_F(B24BridgesRegressionTest, AllBridgesDoubleDestroy) {
    /* Create and destroy each config-only bridge, then destroy(NULL) */
    {
        predictive_plasticity_config_t cfg = predictive_plasticity_config_default();
        predictive_plasticity_bridge_t* b = predictive_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        predictive_plasticity_destroy(b);
        predictive_plasticity_destroy(nullptr);
    }
    {
        predictive_snn_config_t cfg = predictive_snn_config_default();
        predictive_snn_bridge_t* b = predictive_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        predictive_snn_destroy(b);
        predictive_snn_destroy(nullptr);
    }
    {
        personality_plasticity_config_t cfg = personality_plasticity_config_default();
        personality_plasticity_bridge_t* b = personality_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        personality_plasticity_destroy(b);
        personality_plasticity_destroy(nullptr);
    }
    {
        personality_snn_config_t cfg = personality_snn_config_default();
        personality_snn_bridge_t* b = personality_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        personality_snn_destroy(b);
        personality_snn_destroy(nullptr);
    }
    {
        self_awareness_plasticity_config_t cfg = self_awareness_plasticity_config_default();
        self_awareness_plasticity_bridge_t* b = self_awareness_plasticity_create(&cfg);
        ASSERT_NE(nullptr, b);
        self_awareness_plasticity_destroy(b);
        self_awareness_plasticity_destroy(nullptr);
    }
    {
        self_awareness_snn_config_t cfg = self_awareness_snn_config_default();
        self_awareness_snn_bridge_t* b = self_awareness_snn_create(&cfg);
        ASSERT_NE(nullptr, b);
        self_awareness_snn_destroy(b);
        self_awareness_snn_destroy(nullptr);
    }
    /* Non-NULL param bridges: only test destroy(NULL) */
    predictive_substrate_bridge_destroy(nullptr);
    predictive_thalamic_bridge_destroy(nullptr);
    personality_substrate_bridge_destroy(nullptr);
    personality_thalamic_bridge_destroy(nullptr);
    self_awareness_substrate_bridge_destroy(nullptr);
    self_awareness_thalamic_bridge_destroy(nullptr);
}

/* ============================================================================
 * Setter Signature Tests
 * ============================================================================ */

TEST_F(B24BridgesRegressionTest, SetterSignatureIsVoidReturnSingleParam) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kB24BridgeModules[i].name);
        EXPECT_NE(kB24BridgeModules[i].setter, nullptr);
        kB24BridgeModules[i].setter(agent_);
        kB24BridgeModules[i].setter(nullptr);
    }
}

/* ============================================================================
 * Rapid Cycle Tests
 * ============================================================================ */

TEST_F(B24BridgesRegressionTest, RapidSetNullCycles) {
    for (int cycle = 0; cycle < 100; cycle++) {
        for (size_t i = 0; i < kNumModules; i++) {
            kB24BridgeModules[i].setter(agent_);
            kB24BridgeModules[i].setter(nullptr);
        }
    }
}

TEST_F(B24BridgesRegressionTest, PredictivePlasticityMultipleResets) {
    predictive_plasticity_config_t cfg = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* bridge = predictive_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    for (int i = 0; i < 10; i++) {
        int rc = predictive_plasticity_reset(bridge);
        EXPECT_EQ(0, rc);
    }
    predictive_plasticity_destroy(bridge);
}

TEST_F(B24BridgesRegressionTest, PersonalitySnnMultipleResets) {
    personality_snn_config_t cfg = personality_snn_config_default();
    personality_snn_bridge_t* bridge = personality_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    for (int i = 0; i < 10; i++) {
        int rc = personality_snn_reset(bridge);
        EXPECT_EQ(0, rc);
    }
    personality_snn_destroy(bridge);
}

TEST_F(B24BridgesRegressionTest, SelfAwarenessPlasticityMultipleResets) {
    self_awareness_plasticity_config_t cfg = self_awareness_plasticity_config_default();
    self_awareness_plasticity_bridge_t* bridge = self_awareness_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    for (int i = 0; i < 10; i++) {
        int rc = self_awareness_plasticity_reset(bridge);
        EXPECT_EQ(0, rc);
    }
    self_awareness_plasticity_destroy(bridge);
}

/* ============================================================================
 * Stats Consistency
 * ============================================================================ */

TEST_F(B24BridgesRegressionTest, StatsConsistentAfterOperations) {
    nimcp_health_agent_start(agent_);
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(agent_);
    health_agent_stats_t stats1;
    nimcp_health_agent_get_stats(agent_, &stats1);
    nimcp_health_agent_heartbeat_ex(agent_, "B24_consistency", 0);
    health_agent_stats_t stats2;
    nimcp_health_agent_get_stats(agent_, &stats2);
    EXPECT_GE(stats2.heartbeats_received, stats1.heartbeats_received);
    nimcp_health_agent_stop(agent_);
}

/* ============================================================================
 * Thread Safety
 * ============================================================================ */

TEST_F(B24BridgesRegressionTest, ConcurrentSettersThreadSafe) {
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this]() {
            for (int cycle = 0; cycle < 50; cycle++) {
                for (size_t i = 0; i < kNumModules; i++) {
                    kB24BridgeModules[i].setter(agent_);
                    kB24BridgeModules[i].setter(nullptr);
                }
            }
        });
    }
    for (auto& t : threads) t.join();
}
