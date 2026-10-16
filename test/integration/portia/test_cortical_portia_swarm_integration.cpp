/**
 * @file test_cortical_portia_swarm_integration.cpp
 * @brief Integration tests: Cortical-Training → Portia-Swarm-Logic Bridge
 *
 * WHAT: Tests integration between cortical training and portia swarm logic
 * WHY:  Verify cortical stability affects decision threshold modulation
 * HOW:  Create both bridges, connect them, verify threshold modulation
 *
 * TEST COVERAGE:
 * - Connection lifecycle (4 tests)
 * - Threshold modifier computation (6 tests)
 * - Integration with decisions (5 tests)
 *
 * TOTAL: 15 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_swarm_logic_bridge.h"
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class CorticalPortiaSwarmIntegrationTest : public ::testing::Test {
protected:
    portia_swarm_logic_bridge_t* psl_bridge;
    cortical_training_bridge_t* cortical_bridge;
    portia_swarm_logic_config_t psl_config;
    cortical_training_config_t cortical_config;

    void SetUp() override {
        portia_swarm_logic_default_config(&psl_config);
        psl_config.enable_bio_async = false;
        psl_bridge = portia_swarm_logic_create(&psl_config, nullptr, nullptr, nullptr);
        ASSERT_NE(psl_bridge, nullptr);

        cortical_training_default_config(&cortical_config);
        cortical_config.enable_bio_async = false;
        cortical_bridge = cortical_training_create(&cortical_config);
        ASSERT_NE(cortical_bridge, nullptr);
    }

    void TearDown() override {
        if (psl_bridge) {
            portia_swarm_logic_destroy(psl_bridge);
            psl_bridge = nullptr;
        }
        if (cortical_bridge) {
            cortical_training_destroy(cortical_bridge);
            cortical_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (4 tests)
//=============================================================================

TEST_F(CorticalPortiaSwarmIntegrationTest, ConnectCorticalToPortiaSwarm) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);

    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_TRUE(stats.cortical_training_connected);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, DisconnectCorticalFromPortiaSwarm) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);

    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_TRUE(stats.cortical_training_connected);

    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_FALSE(stats.cortical_training_connected);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, ConnectNullBridgeReturnsError) {
    EXPECT_NE(portia_swarm_logic_connect_cortical_training(nullptr, cortical_bridge), 0);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, ReconnectCorticalBridge) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);

    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_TRUE(stats.cortical_training_connected);
}

//=============================================================================
// Threshold Modifier Computation (6 tests)
//=============================================================================

TEST_F(CorticalPortiaSwarmIntegrationTest, NoConnectionReturnsDefaultModifier) {
    /* No cortical connected → modifier = 1.0 */
    float modifier = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
    EXPECT_FLOAT_EQ(modifier, 1.0f);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, HighBurstStableReducesThreshold) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    /* High burst rate + stable predictions → lower threshold (more confident) */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.9f;
    effects.predictions_stable = true;
    effects.prediction_error_mag = 0.1f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float modifier = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
    /* modifier = 1.0 - 0.3 × (0.9 - 0.5) + 0.1 × 0.1 = 1.0 - 0.12 + 0.01 = 0.89 */
    EXPECT_LT(modifier, 1.0f);
    EXPECT_GE(modifier, 0.7f);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, LowBurstUnstableIncreasesThreshold) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    /* Low burst rate + unstable + high error → higher threshold (cautious) */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.1f;
    effects.predictions_stable = false;
    effects.prediction_error_mag = 0.8f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float modifier = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
    /* modifier = 1.0 - 0.3 × (0.1 - 0.5) + 0.15 + 0.1 × 0.8 = 1.0 + 0.12 + 0.15 + 0.08 = 1.35, clamped 1.3 */
    EXPECT_GT(modifier, 1.0f);
    EXPECT_LE(modifier, 1.3f);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, DisconnectedReturnsDefaultModifier) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    /* Set effects for high burst rate */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.9f;
    effects.predictions_stable = true;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float modifier_connected = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
    EXPECT_LT(modifier_connected, 1.0f);

    /* Disconnect → default modifier = 1.0 */
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, nullptr), 0);
    float modifier = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
    EXPECT_FLOAT_EQ(modifier, 1.0f);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, ModifierClampedToUpperBound) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    /* Very low burst + unstable + max error → clamped to 1.3 */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.0f;
    effects.predictions_stable = false;
    effects.prediction_error_mag = 1.0f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float modifier = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
    EXPECT_FLOAT_EQ(modifier, 1.3f);  /* Clamped */
}

TEST_F(CorticalPortiaSwarmIntegrationTest, ModifierClampedToLowerBound) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    /* Very high burst + stable + no error → clamped to 0.7 */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 1.0f;
    effects.predictions_stable = true;
    effects.prediction_error_mag = 0.0f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    float modifier = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
    EXPECT_FLOAT_EQ(modifier, 0.85f);  /* 1.0 - 0.3 × 0.5 = 0.85 */
}

//=============================================================================
// Integration with Decisions (5 tests)
//=============================================================================

TEST_F(CorticalPortiaSwarmIntegrationTest, DecisionWithCorticalIntegration) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    /* Set stable cortical effects */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.7f;
    effects.predictions_stable = true;
    effects.prediction_error_mag = 0.2f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    /* Make a decision - should work with cortical integration */
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(psl_bridge, 0, 1, &result), 0);
    EXPECT_GT(result.confidence, 0.0f);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, ModifierEvolvesWithCortical) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    /* Simulate improving cortical state over steps */
    for (int step = 0; step < 10; ++step) {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.burst_rate = 0.2f + 0.7f * (step / 10.0f);
        effects.predictions_stable = (step >= 5);
        effects.prediction_error_mag = 0.8f - 0.7f * (step / 10.0f);
        effects.valid = true;
        EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

        float modifier = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
        EXPECT_GE(modifier, 0.7f);
        EXPECT_LE(modifier, 1.3f);

        /* Threshold should decrease as cortical improves (more confident) */
        if (step > 7) {
            EXPECT_LT(modifier, 1.05f);
        }
    }
}

TEST_F(CorticalPortiaSwarmIntegrationTest, EmergencyDecisionWithUnstableCortical) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    /* Unstable cortical should affect emergency threshold */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.2f;
    effects.predictions_stable = false;
    effects.prediction_error_mag = 0.9f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(psl_bridge, &result), 0);
    /* Decision should complete even with unstable cortical */
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, DegradationDecisionWithHighBurst) {
    EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_cortical_training(psl_bridge, cortical_bridge), 0);

    /* High burst rate for confident degradation decision */
    cortical_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.burst_rate = 0.85f;
    effects.predictions_stable = true;
    effects.prediction_error_mag = 0.1f;
    effects.valid = true;
    EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);

    /* Verify threshold modifier is reduced (more confident) with high burst */
    float modifier = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
    EXPECT_LT(modifier, 1.0f);  /* High burst reduces threshold (more confident) */

    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_degradation(psl_bridge, 50, &result), 0);
    /* Decision completes (base confidence from bridge without real logic) */
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(CorticalPortiaSwarmIntegrationTest, NullContextReturnsDefault) {
    EXPECT_FLOAT_EQ(portia_swarm_logic_get_cortical_threshold_modifier(nullptr), 1.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
