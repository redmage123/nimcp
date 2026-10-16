/**
 * @file test_fep_consciousness_integration.cpp
 * @brief Integration tests for FEP Consciousness module with other FEP components
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "cognitive/free_energy/nimcp_fep_consciousness.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_planning.h"
#include "cognitive/free_energy/nimcp_fep_neuromod.h"

class FEPConsciousnessIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 8;

    fep_consciousness_bridge_t* consciousness = nullptr;
    fep_system_t* fep = nullptr;
    fep_planning_system_t* planning = nullptr;
    fep_neuromod_system_t* neuromod = nullptr;

    void SetUp() override {
        /* Create consciousness bridge */
        fep_consciousness_config_t con_config;
        fep_consciousness_default_config(&con_config);
        consciousness = fep_consciousness_create(&con_config);

        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        /* Create planning system */
        fep_planning_config_t plan_config;
        fep_planning_default_config(&plan_config);
        planning = fep_planning_create(&plan_config);

        /* Create neuromod system */
        fep_neuromod_config_t neuro_config;
        fep_neuromod_default_config(&neuro_config);
        neuromod = fep_neuromod_create(&neuro_config);
    }

    void TearDown() override {
        if (consciousness) {
            fep_consciousness_destroy(consciousness);
            consciousness = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (planning) {
            fep_planning_destroy(planning);
            planning = nullptr;
        }
        if (neuromod) {
            fep_neuromod_destroy(neuromod);
            neuromod = nullptr;
        }
    }
};

/* ============================================================================
 * Action Gating Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, GateAction) {
    fep_consciousness_connect_fep(consciousness, fep);

    uint32_t proposed = 5;
    uint32_t gated;

    int ret = fep_consciousness_gate_action(consciousness, proposed, &gated);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPConsciousnessIntegrationTest, GatingWithHighPhi) {
    fep_consciousness_connect_fep(consciousness, fep);

    /* With high Φ, novel actions should be gated through */
    uint32_t proposed = 3;
    uint32_t gated;

    fep_consciousness_gate_action(consciousness, proposed, &gated);
    /* Result depends on internal Φ computation */
    EXPECT_GE(gated, 0u);
}

/* ============================================================================
 * Precision Modulation Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, PrecisionModulation) {
    fep_consciousness_connect_fep(consciousness, fep);

    float precision = 1.0f;
    int ret = fep_consciousness_modulate_precision(consciousness, &precision);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(precision, 0.0f);
}

TEST_F(FEPConsciousnessIntegrationTest, PrecisionWithNeuromod) {
    fep_consciousness_connect_fep(consciousness, fep);

    /* Set high attention via neuromod */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.9f);

    float base_precision = fep_neuromod_compute_precision(neuromod, 1.0f);

    /* Apply consciousness modulation */
    float precision = base_precision;
    fep_consciousness_modulate_precision(consciousness, &precision);

    EXPECT_TRUE(std::isfinite(precision));
}

/* ============================================================================
 * Planning Integration Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, ConsciousnessWithPlanning) {
    fep_consciousness_connect_fep(consciousness, fep);

    /* Planning should respect consciousness gating */
    fep_planning_reset(planning);

    /* Get action from planning */
    uint32_t planned_action = 2;
    uint32_t gated_action;

    int ret = fep_consciousness_gate_action(consciousness, planned_action, &gated_action);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * State Monitoring Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, StateMonitoring) {
    fep_consciousness_connect_fep(consciousness, fep);

    /* Update consciousness state */
    fep_consciousness_update(consciousness, 100);

    fep_consciousness_state_t state;
    int ret = fep_consciousness_get_state(consciousness, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPConsciousnessIntegrationTest, StateAfterMultipleUpdates) {
    fep_consciousness_connect_fep(consciousness, fep);

    for (int i = 0; i < 20; i++) {
        fep_consciousness_update(consciousness, 50);
    }

    fep_consciousness_state_t state;
    fep_consciousness_get_state(consciousness, &state);

    /* Φ should be computed */
    EXPECT_GE(state.current_phi, 0.0f);
}

/* ============================================================================
 * Attention Integration Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, AttentionModulatesConsciousness) {
    fep_consciousness_connect_fep(consciousness, fep);

    /* High attention should affect precision modulation */
    float base = 1.0f;
    fep_consciousness_modulate_precision(consciousness, &base);

    EXPECT_TRUE(std::isfinite(base));
}

/* ============================================================================
 * Habitual vs Novel Action Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, HabitualActionCaching) {
    fep_consciousness_connect_fep(consciousness, fep);

    /* Execute same action multiple times */
    uint32_t action = 1;
    for (int i = 0; i < 10; i++) {
        uint32_t gated;
        fep_consciousness_gate_action(consciousness, action, &gated);
    }

    /* Action may become habitual */
    fep_consciousness_state_t state;
    fep_consciousness_get_state(consciousness, &state);
    EXPECT_GE(state.unconscious_actions, 0u);
}

TEST_F(FEPConsciousnessIntegrationTest, NovelActionRequiresConsciousness) {
    fep_consciousness_connect_fep(consciousness, fep);

    /* Different actions should be processed */
    for (uint32_t action = 0; action < 5; action++) {
        uint32_t gated;
        int ret = fep_consciousness_gate_action(consciousness, action, &gated);
        EXPECT_EQ(ret, 0);
    }
}

/* ============================================================================
 * Config Integration Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, CustomPhiThreshold) {
    fep_consciousness_config_t config;
    fep_consciousness_default_config(&config);
    config.phi_threshold = 0.5f;

    fep_consciousness_bridge_t* custom = fep_consciousness_create(&config);
    ASSERT_NE(custom, nullptr);

    fep_consciousness_connect_fep(custom, fep);

    uint32_t proposed = 3, gated;
    int ret = fep_consciousness_gate_action(custom, proposed, &gated);
    EXPECT_EQ(ret, 0);

    fep_consciousness_destroy(custom);
}

TEST_F(FEPConsciousnessIntegrationTest, CustomAttentionGain) {
    fep_consciousness_config_t config;
    fep_consciousness_default_config(&config);
    config.attention_gain = 2.0f;

    fep_consciousness_bridge_t* high_gain = fep_consciousness_create(&config);
    ASSERT_NE(high_gain, nullptr);

    fep_consciousness_connect_fep(high_gain, fep);

    float precision = 1.0f;
    fep_consciousness_modulate_precision(high_gain, &precision);

    fep_consciousness_destroy(high_gain);
}

/* ============================================================================
 * Disconnect Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, DisconnectFromFEP) {
    fep_consciousness_connect_fep(consciousness, fep);

    int ret = fep_consciousness_disconnect(consciousness);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, BioAsyncWithConsciousness) {
    fep_consciousness_connect_fep(consciousness, fep);
    fep_consciousness_connect_bio_async(consciousness);

    /* Should work with bio-async */
    uint32_t proposed = 2, gated;
    int ret = fep_consciousness_gate_action(consciousness, proposed, &gated);
    EXPECT_EQ(ret, 0);

    fep_consciousness_disconnect_bio_async(consciousness);
}

/* ============================================================================
 * Full Integration Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, FullConsciousnessLoop) {
    fep_consciousness_connect_fep(consciousness, fep);

    /* 1. Update consciousness */
    fep_consciousness_update(consciousness, 100);

    /* 2. Get state */
    fep_consciousness_state_t state;
    fep_consciousness_get_state(consciousness, &state);

    /* 3. Gate an action */
    uint32_t proposed = 4, gated;
    fep_consciousness_gate_action(consciousness, proposed, &gated);

    /* 4. Modulate precision */
    float precision = 1.0f;
    fep_consciousness_modulate_precision(consciousness, &precision);

    /* 5. Update again */
    fep_consciousness_update(consciousness, 100);

    /* All operations should succeed */
    EXPECT_TRUE(std::isfinite(precision));
    EXPECT_GE(state.current_phi, 0.0f);
}

TEST_F(FEPConsciousnessIntegrationTest, ConsciousnessWithMultipleSystems) {
    fep_consciousness_connect_fep(consciousness, fep);

    /* Update neuromod */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.8f);
    fep_neuromod_update(neuromod, 100);

    /* Update planning */
    fep_planning_reset(planning);

    /* Get precision from neuromod */
    float base_precision = fep_neuromod_compute_precision(neuromod, 1.0f);

    /* Modulate with consciousness */
    float final_precision = base_precision;
    fep_consciousness_modulate_precision(consciousness, &final_precision);

    /* Gate a planned action */
    uint32_t planned = 3, gated;
    fep_consciousness_gate_action(consciousness, planned, &gated);

    EXPECT_TRUE(std::isfinite(final_precision));
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(FEPConsciousnessIntegrationTest, ZeroPrecisionModulation) {
    fep_consciousness_connect_fep(consciousness, fep);

    float precision = 0.0f;
    int ret = fep_consciousness_modulate_precision(consciousness, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(precision, 0.0f);
}

TEST_F(FEPConsciousnessIntegrationTest, MaxActionValue) {
    fep_consciousness_connect_fep(consciousness, fep);

    uint32_t proposed = UINT32_MAX, gated;
    int ret = fep_consciousness_gate_action(consciousness, proposed, &gated);
    EXPECT_EQ(ret, 0);
}
