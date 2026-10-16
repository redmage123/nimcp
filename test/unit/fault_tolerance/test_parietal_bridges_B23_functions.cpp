/**
 * @file test_parietal_bridges_B23_functions.cpp
 * @brief Unit tests for B23 parietal bridge health agent integration
 *        and functional tests for each bridge's core API
 *        (cognitive/parietal bridges: fep_parietal, genius_plasticity,
 *         genius_snn, genius_training, intuition_substrate, intuition_thalamic,
 *         parietal_fep, parietal_plasticity, parietal_quantum, parietal_snn,
 *         parietal_training)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>

/* Parietal bridge APIs - outside extern "C" since headers have their own guards */
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
    /* Parietal bridge health agent setters */
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

static constexpr size_t kNumModules = sizeof(kParietalBridgeModules) / sizeof(kParietalBridgeModules[0]);

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ParietalBridgesB23Test : public ::testing::Test {
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
 * Health Agent Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, AllModulesSetNull) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kParietalBridgeModules[i].name);
        kParietalBridgeModules[i].setter(nullptr);
    }
}

TEST_F(ParietalBridgesB23Test, AllModulesSetValid) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kParietalBridgeModules[i].name);
        kParietalBridgeModules[i].setter(agent_);
    }
}

TEST_F(ParietalBridgesB23Test, AllModulesReplaceAgent) {
    health_agent_config_t config2;
    nimcp_health_agent_default_config(&config2);
    nimcp_health_agent_t* agent2 = nimcp_health_agent_create(&config2);
    ASSERT_NE(nullptr, agent2);
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kParietalBridgeModules[i].name);
        kParietalBridgeModules[i].setter(agent_);
        kParietalBridgeModules[i].setter(agent2);
    }
    for (size_t i = 0; i < kNumModules; i++) kParietalBridgeModules[i].setter(nullptr);
    nimcp_health_agent_destroy(agent2);
}

TEST_F(ParietalBridgesB23Test, ModuleCount) {
    EXPECT_EQ(kNumModules, 11u);
}

TEST_F(ParietalBridgesB23Test, DoubleSetSameAgentIdempotent) {
    for (size_t i = 0; i < kNumModules; i++) {
        SCOPED_TRACE(kParietalBridgeModules[i].name);
        kParietalBridgeModules[i].setter(agent_);
        kParietalBridgeModules[i].setter(agent_);
    }
}

/* ============================================================================
 * FEP Parietal Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, FepParietalCreateDestroy) {
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* bridge = fep_parietal_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    fep_parietal_bridge_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, FepParietalDefaultConfig) {
    fep_parietal_config_t cfg = fep_parietal_default_config();
    EXPECT_TRUE(cfg.enabled);
}

TEST_F(ParietalBridgesB23Test, FepParietalNullSafety) {
    fep_parietal_bridge_destroy(nullptr);
    int rc = fep_parietal_set_enabled(nullptr, true);
    EXPECT_NE(0, rc);
}

TEST_F(ParietalBridgesB23Test, FepParietalGetStats) {
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* bridge = fep_parietal_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    fep_parietal_stats_t stats;
    int rc = fep_parietal_get_stats(bridge, &stats);
    EXPECT_EQ(0, rc);
    fep_parietal_bridge_destroy(bridge);
}

/* ============================================================================
 * Genius Plasticity Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, GeniusPlasticityCreateDestroy) {
    genius_plasticity_config_t cfg = genius_plasticity_config_default();
    genius_plasticity_bridge_t* bridge = genius_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    genius_plasticity_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, GeniusPlasticityReset) {
    genius_plasticity_config_t cfg = genius_plasticity_config_default();
    genius_plasticity_bridge_t* bridge = genius_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = genius_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    genius_plasticity_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, GeniusPlasticityNullSafety) {
    genius_plasticity_destroy(nullptr);
    int rc = genius_plasticity_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Genius SNN Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, GeniusSnnCreateDestroy) {
    genius_snn_config_t cfg = genius_snn_config_default();
    genius_snn_bridge_t* bridge = genius_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    genius_snn_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, GeniusSnnReset) {
    genius_snn_config_t cfg = genius_snn_config_default();
    genius_snn_bridge_t* bridge = genius_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = genius_snn_reset(bridge);
    EXPECT_EQ(0, rc);
    genius_snn_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, GeniusSnnNullSafety) {
    genius_snn_destroy(nullptr);
    int rc = genius_snn_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Genius Training Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, GeniusTrainingNullSafety) {
    /* Genius training requires non-NULL genius+training params, test destroy(NULL) */
    genius_training_destroy(nullptr);
    int rc = genius_training_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Intuition Substrate Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, IntuitionSubstrateNullSafety) {
    /* Requires non-NULL intuition+substrate params, test destroy(NULL) */
    intuition_substrate_bridge_destroy(nullptr);
    int rc = intuition_substrate_bridge_reset(nullptr);
    EXPECT_NE(0, rc);
}

TEST_F(ParietalBridgesB23Test, IntuitionSubstrateGetEffectsNull) {
    intuition_substrate_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    int rc = intuition_substrate_bridge_get_effects(nullptr, &effects);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Intuition Thalamic Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, IntuitionThalamicNullSafety) {
    /* Requires non-NULL intuition+router params, test destroy(NULL) */
    intuition_thalamic_bridge_destroy(nullptr);
    int rc = intuition_thalamic_bridge_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Parietal FEP Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, ParietalFepCreateDestroy) {
    parietal_fep_config_t cfg = parietal_fep_config_default();
    parietal_fep_bridge_t* bridge = parietal_fep_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    parietal_fep_bridge_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, ParietalFepReset) {
    parietal_fep_config_t cfg = parietal_fep_config_default();
    parietal_fep_bridge_t* bridge = parietal_fep_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = parietal_fep_bridge_reset(bridge);
    EXPECT_EQ(0, rc);
    parietal_fep_bridge_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, ParietalFepNullSafety) {
    parietal_fep_bridge_destroy(nullptr);
    int rc = parietal_fep_bridge_reset(nullptr);
    EXPECT_NE(0, rc);
}

TEST_F(ParietalBridgesB23Test, ParietalFepGetStats) {
    parietal_fep_config_t cfg = parietal_fep_config_default();
    parietal_fep_bridge_t* bridge = parietal_fep_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    parietal_fep_stats_t stats;
    int rc = parietal_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, rc);
    parietal_fep_bridge_destroy(bridge);
}

/* ============================================================================
 * Parietal Plasticity Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, ParietalPlasticityCreateDestroy) {
    parietal_plasticity_config_t cfg = parietal_plasticity_config_default();
    parietal_plasticity_bridge_t* bridge = parietal_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    parietal_plasticity_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, ParietalPlasticityReset) {
    parietal_plasticity_config_t cfg = parietal_plasticity_config_default();
    parietal_plasticity_bridge_t* bridge = parietal_plasticity_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = parietal_plasticity_reset(bridge);
    EXPECT_EQ(0, rc);
    parietal_plasticity_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, ParietalPlasticityNullSafety) {
    parietal_plasticity_destroy(nullptr);
    int rc = parietal_plasticity_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Parietal Quantum Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, ParietalQuantumCreateDestroy) {
    parietal_quantum_config_t cfg = parietal_quantum_default_config();
    parietal_quantum_bridge_t* bridge = parietal_quantum_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    parietal_quantum_bridge_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, ParietalQuantumNullSafety) {
    parietal_quantum_bridge_destroy(nullptr);
    int rc = parietal_quantum_set_enabled(nullptr, true);
    EXPECT_NE(0, rc);
}

TEST_F(ParietalBridgesB23Test, ParietalQuantumGetStats) {
    parietal_quantum_config_t cfg = parietal_quantum_default_config();
    parietal_quantum_bridge_t* bridge = parietal_quantum_bridge_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    parietal_quantum_stats_t stats;
    int rc = parietal_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(0, rc);
    parietal_quantum_bridge_destroy(bridge);
}

/* ============================================================================
 * Parietal SNN Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, ParietalSnnCreateDestroy) {
    parietal_snn_config_t cfg = parietal_snn_config_default();
    parietal_snn_bridge_t* bridge = parietal_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    parietal_snn_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, ParietalSnnReset) {
    parietal_snn_config_t cfg = parietal_snn_config_default();
    parietal_snn_bridge_t* bridge = parietal_snn_create(&cfg);
    ASSERT_NE(nullptr, bridge);
    int rc = parietal_snn_reset(bridge);
    EXPECT_EQ(0, rc);
    parietal_snn_destroy(bridge);
}

TEST_F(ParietalBridgesB23Test, ParietalSnnNullSafety) {
    parietal_snn_destroy(nullptr);
    int rc = parietal_snn_reset(nullptr);
    EXPECT_NE(0, rc);
}

/* ============================================================================
 * Parietal Training Bridge Functional Tests
 * ============================================================================ */

TEST_F(ParietalBridgesB23Test, ParietalTrainingNullSafety) {
    /* Requires non-NULL parietal+training params, test destroy(NULL) */
    parietal_training_destroy(nullptr);
}

TEST_F(ParietalBridgesB23Test, ParietalTrainingDefaultConfig) {
    parietal_training_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    int rc = parietal_training_default_config(&cfg);
    EXPECT_EQ(0, rc);
}
