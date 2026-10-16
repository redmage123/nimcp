/**
 * @file test_predictive_personality_selfawareness_bridges_B24_functions.cpp
 * @brief Unit tests for B24 predictive+personality+self_awareness bridge
 *        health agent integration and functional tests for each bridge's core API
 *        (cognitive/predictive bridges: predictive_fep, predictive_plasticity,
 *         predictive_snn, predictive_substrate, predictive_thalamic;
 *         cognitive/personality bridges: personality_plasticity, personality_snn,
 *         personality_substrate, personality_thalamic;
 *         cognitive/self_awareness bridges: self_awareness_plasticity,
 *         self_awareness_snn, self_awareness_substrate, self_awareness_thalamic)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>

/* Bridge APIs - outside extern "C" since headers have their own guards */
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
    /* B24 bridge health agent setters (bare declarations with void*) */
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

static constexpr size_t kNumModules = sizeof(kB24BridgeModules) / sizeof(kB24BridgeModules[0]);

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class B24BridgesTest : public ::testing::Test {
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
 * Health Agent Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, AllModulesSetNull) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kB24BridgeModules[i].name);
        kB24BridgeModules[i].setter(nullptr);
    }
}

TEST_F(B24BridgesTest, AllModulesSetValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kB24BridgeModules[i].name);
        kB24BridgeModules[i].setter(agent_);
    }
}

TEST_F(B24BridgesTest, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(nullptr, agent2);
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kB24BridgeModules[i].name);
        kB24BridgeModules[i].setter(agent_);
        kB24BridgeModules[i].setter(agent2);
    }
    for (size_t i = 0; i < kNumModules; i++) kB24BridgeModules[i].setter(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(B24BridgesTest, ModuleCount) {
    EXPECT_EQ(kNumModules, 13u);
}

TEST_F(B24BridgesTest, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kB24BridgeModules[i].name);
        kB24BridgeModules[i].setter(agent_);
        kB24BridgeModules[i].setter(agent_);
    }
}

/* ============================================================================
 * Predictive FEP Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, PredictiveFepCreateDestroy) {
    predictive_fep_config_t cfg;
    int rc = predictive_fep_bridge_default_config(&cfg);
    EXPECT_EQ(0, rc);
    predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    predictive_fep_bridge_destroy(bridge);
}

TEST_F(B24BridgesTest, PredictiveFepDefaultConfig) {
    predictive_fep_config_t cfg;
    int rc = predictive_fep_bridge_default_config(&cfg);
    EXPECT_EQ(0, rc);
}

TEST_F(B24BridgesTest, PredictiveFepNullSafety) {
    predictive_fep_bridge_destroy(nullptr);
}

/* ============================================================================
 * Predictive Plasticity Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, PredictivePlasticityCreateDestroy) {
    predictive_plasticity_config_t cfg = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* bridge = predictive_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    predictive_plasticity_destroy(bridge);
}

TEST_F(B24BridgesTest, PredictivePlasticityReset) {
    predictive_plasticity_config_t cfg = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* bridge = predictive_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = predictive_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    predictive_plasticity_destroy(bridge);
}

TEST_F(B24BridgesTest, PredictivePlasticityNullSafety) {
    predictive_plasticity_destroy(nullptr);
    int rc = predictive_plasticity_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Predictive SNN Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, PredictiveSnnCreateDestroy) {
    predictive_snn_config_t cfg = predictive_snn_config_default();
    predictive_snn_bridge_t* bridge = predictive_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    predictive_snn_destroy(bridge);
}

TEST_F(B24BridgesTest, PredictiveSnnReset) {
    predictive_snn_config_t cfg = predictive_snn_config_default();
    predictive_snn_bridge_t* bridge = predictive_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = predictive_snn_reset(bridge);
    EXPECT_EQ(0, rc);
    predictive_snn_destroy(bridge);
}

TEST_F(B24BridgesTest, PredictiveSnnNullSafety) {
    predictive_snn_destroy(nullptr);
    int rc = predictive_snn_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Predictive Substrate Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, PredictiveSubstrateNullSafety) {
    /* Requires non-NULL predictive+substrate params, test destroy(NULL) */
    predictive_substrate_bridge_destroy(nullptr);
}

/* ============================================================================
 * Predictive Thalamic Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, PredictiveThalamicNullSafety) {
    /* Requires non-NULL predictive+router params, test destroy(NULL) */
    predictive_thalamic_bridge_destroy(nullptr);
}

/* ============================================================================
 * Personality Plasticity Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, PersonalityPlasticityCreateDestroy) {
    personality_plasticity_config_t cfg = personality_plasticity_config_default();
    personality_plasticity_bridge_t* bridge = personality_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    personality_plasticity_destroy(bridge);
}

TEST_F(B24BridgesTest, PersonalityPlasticityReset) {
    personality_plasticity_config_t cfg = personality_plasticity_config_default();
    personality_plasticity_bridge_t* bridge = personality_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = personality_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    personality_plasticity_destroy(bridge);
}

TEST_F(B24BridgesTest, PersonalityPlasticityNullSafety) {
    personality_plasticity_destroy(nullptr);
    int rc = personality_plasticity_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Personality SNN Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, PersonalitySnnCreateDestroy) {
    personality_snn_config_t cfg = personality_snn_config_default();
    personality_snn_bridge_t* bridge = personality_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    personality_snn_destroy(bridge);
}

TEST_F(B24BridgesTest, PersonalitySnnReset) {
    personality_snn_config_t cfg = personality_snn_config_default();
    personality_snn_bridge_t* bridge = personality_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = personality_snn_reset(bridge);
    EXPECT_EQ(0, rc);
    personality_snn_destroy(bridge);
}

TEST_F(B24BridgesTest, PersonalitySnnNullSafety) {
    personality_snn_destroy(nullptr);
    int rc = personality_snn_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Personality Substrate Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, PersonalitySubstrateNullSafety) {
    /* Requires non-NULL personality+substrate params, test destroy(NULL) */
    personality_substrate_bridge_destroy(nullptr);
}

/* ============================================================================
 * Personality Thalamic Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, PersonalityThalamicNullSafety) {
    /* Requires non-NULL personality+router params, test destroy(NULL) */
    personality_thalamic_bridge_destroy(nullptr);
}

/* ============================================================================
 * Self-Awareness Plasticity Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, SelfAwarenessPlasticityCreateDestroy) {
    self_awareness_plasticity_config_t cfg = self_awareness_plasticity_config_default();
    self_awareness_plasticity_bridge_t* bridge = self_awareness_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    self_awareness_plasticity_destroy(bridge);
}

TEST_F(B24BridgesTest, SelfAwarenessPlasticityReset) {
    self_awareness_plasticity_config_t cfg = self_awareness_plasticity_config_default();
    self_awareness_plasticity_bridge_t* bridge = self_awareness_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = self_awareness_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    self_awareness_plasticity_destroy(bridge);
}

TEST_F(B24BridgesTest, SelfAwarenessPlasticityNullSafety) {
    self_awareness_plasticity_destroy(nullptr);
    int rc = self_awareness_plasticity_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Self-Awareness SNN Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, SelfAwarenessSnnCreateDestroy) {
    self_awareness_snn_config_t cfg = self_awareness_snn_config_default();
    self_awareness_snn_bridge_t* bridge = self_awareness_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    self_awareness_snn_destroy(bridge);
}

TEST_F(B24BridgesTest, SelfAwarenessSnnReset) {
    self_awareness_snn_config_t cfg = self_awareness_snn_config_default();
    self_awareness_snn_bridge_t* bridge = self_awareness_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = self_awareness_snn_reset(bridge);
    EXPECT_EQ(0, rc);
    self_awareness_snn_destroy(bridge);
}

TEST_F(B24BridgesTest, SelfAwarenessSnnNullSafety) {
    self_awareness_snn_destroy(nullptr);
    int rc = self_awareness_snn_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Self-Awareness Substrate Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, SelfAwarenessSubstrateNullSafety) {
    /* Requires non-NULL self_awareness+substrate params, test destroy(NULL) */
    self_awareness_substrate_bridge_destroy(nullptr);
}

/* ============================================================================
 * Self-Awareness Thalamic Bridge Functional Tests
 * ============================================================================ */

TEST_F(B24BridgesTest, SelfAwarenessThalamicNullSafety) {
    /* Requires non-NULL self_awareness+router params, test destroy(NULL) */
    self_awareness_thalamic_bridge_destroy(nullptr);
}
