/**
 * @file test_perception_portia_swarm_integration.cpp
 * @brief Integration tests: Perception-Training → Portia-Swarm-Logic Bridge
 *
 * WHAT: Tests integration between perception training and portia swarm logic
 * WHY:  Verify perception quality affects decision confidence modulation
 * HOW:  Create both bridges, connect them, verify confidence modulation
 *
 * TEST COVERAGE:
 * - Connection lifecycle (4 tests)
 * - Confidence modifier computation (6 tests)
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
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"

class PerceptionPortiaSwarmIntegrationTest : public ::testing::Test {
protected:
    portia_swarm_logic_bridge_t* psl_bridge;
    perception_training_bridge_t* perception_bridge;
    portia_swarm_logic_config_t psl_config;
    perception_training_config_t perception_config;

    void SetUp() override {
        portia_swarm_logic_default_config(&psl_config);
        psl_config.enable_bio_async = false;
        psl_bridge = portia_swarm_logic_create(&psl_config, nullptr, nullptr, nullptr);
        ASSERT_NE(psl_bridge, nullptr);

        perception_training_default_config(&perception_config);
        perception_config.enable_bio_async = false;
        perception_bridge = perception_training_create(&perception_config);
        ASSERT_NE(perception_bridge, nullptr);
    }

    void TearDown() override {
        if (psl_bridge) {
            portia_swarm_logic_destroy(psl_bridge);
            psl_bridge = nullptr;
        }
        if (perception_bridge) {
            perception_training_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
    }
};

//=============================================================================
// Connection Lifecycle (4 tests)
//=============================================================================

TEST_F(PerceptionPortiaSwarmIntegrationTest, ConnectPerceptionToPortiaSwarm) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);

    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_TRUE(stats.perception_training_connected);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, DisconnectPerceptionFromPortiaSwarm) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);

    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_TRUE(stats.perception_training_connected);

    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_FALSE(stats.perception_training_connected);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, ConnectNullBridgeReturnsError) {
    EXPECT_NE(portia_swarm_logic_connect_perception_training(nullptr, perception_bridge), 0);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, ReconnectPerceptionBridge) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);

    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, nullptr), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_TRUE(stats.perception_training_connected);
}

//=============================================================================
// Confidence Modifier Computation (6 tests)
//=============================================================================

TEST_F(PerceptionPortiaSwarmIntegrationTest, NoConnectionReturnsDefaultModifier) {
    /* No perception connected → modifier = 1.0 */
    float modifier = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    EXPECT_FLOAT_EQ(modifier, 1.0f);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, HighConfidenceHighLRBoostsModifier) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    /* High visual confidence + high LR factor → modifier > 1.0 */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.9f;
    effects.lr_factor = 1.2f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float modifier = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    /* modifier = (0.5 + 0.5 × 0.9) × 1.2 = 0.95 × 1.2 = 1.14 */
    EXPECT_GT(modifier, 1.0f);
    EXPECT_LE(modifier, 1.5f);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, LowConfidenceLowLRReducesModifier) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    /* Low visual confidence + low LR factor → modifier < 1.0 */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.1f;
    effects.lr_factor = 0.6f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float modifier = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    /* modifier = (0.5 + 0.5 × 0.1) × 0.6 = 0.55 × 0.6 = 0.33, clamped to 0.5 */
    EXPECT_GE(modifier, 0.5f);
    EXPECT_LT(modifier, 1.0f);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, DisconnectedReturnsDefaultModifier) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    /* Set high effects */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.9f;
    effects.lr_factor = 1.2f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float modifier_connected = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    EXPECT_GT(modifier_connected, 1.0f);

    /* Disconnect → default modifier = 1.0 */
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, nullptr), 0);
    float modifier = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    EXPECT_FLOAT_EQ(modifier, 1.0f);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, ModifierClampedToUpperBound) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    /* Very high values → clamped to 1.5 */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 1.0f;
    effects.lr_factor = 2.0f;  /* Would give 1.0 × 2.0 = 2.0 */
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float modifier = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    EXPECT_FLOAT_EQ(modifier, 1.5f);  /* Clamped */
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, ModifierClampedToLowerBound) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    /* Very low values → clamped to 0.5 */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.0f;
    effects.lr_factor = 0.1f;  /* Would give 0.5 × 0.1 = 0.05 */
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    float modifier = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    EXPECT_FLOAT_EQ(modifier, 0.5f);  /* Clamped */
}

//=============================================================================
// Integration with Decisions (5 tests)
//=============================================================================

TEST_F(PerceptionPortiaSwarmIntegrationTest, DecisionWithPerceptionIntegration) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    /* Set good perception effects */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.8f;
    effects.lr_factor = 1.0f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Make a decision - should work with perception integration */
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(psl_bridge, 0, 1, &result), 0);
    EXPECT_GT(result.confidence, 0.0f);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, ModifierEvolvesWithPerception) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    /* Simulate improving perception over steps */
    for (int step = 0; step < 10; ++step) {
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = 0.2f + 0.7f * (step / 10.0f);
        effects.lr_factor = 0.6f + 0.4f * (step / 10.0f);
        effects.valid = true;
        EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

        float modifier = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
        EXPECT_GE(modifier, 0.5f);
        EXPECT_LE(modifier, 1.5f);

        /* Modifier should increase as perception improves */
        if (step > 5) {
            EXPECT_GT(modifier, 0.6f);
        }
    }
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, EmergencyDecisionWithPerception) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    /* Low perception should affect emergency threshold */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.1f;
    effects.lr_factor = 0.5f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_emergency_mode(psl_bridge, &result), 0);
    /* Decision should complete even with low perception */
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, ResourceDecisionWithHighPerception) {
    EXPECT_EQ(perception_training_start(perception_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    EXPECT_EQ(portia_swarm_logic_connect_perception_training(psl_bridge, perception_bridge), 0);

    /* High perception quality for resource allocation */
    perception_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    effects.visual_confidence = 0.95f;
    effects.lr_factor = 1.1f;
    effects.valid = true;
    EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);

    /* Verify modifier is boosted with high perception */
    float modifier = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    EXPECT_GT(modifier, 1.0f);  /* High perception boosts confidence modifier */

    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(psl_bridge, 100, 0.3f, &result), 0);
    /* Decision completes (base confidence from bridge without real logic) */
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(PerceptionPortiaSwarmIntegrationTest, NullContextReturnsDefault) {
    EXPECT_FLOAT_EQ(portia_swarm_logic_get_perception_confidence_modifier(nullptr), 1.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
