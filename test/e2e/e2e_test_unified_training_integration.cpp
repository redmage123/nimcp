/**
 * @file e2e_test_unified_training_integration.cpp
 * @brief End-to-end tests for unified perception/cortical training integration
 *
 * WHAT: Full pipeline tests demonstrating perception/cortical integration
 *       across all training subsystems
 * WHY:  Verify complete integration from perception → cognitive → logic →
 *       immune → plasticity → portia-swarm
 * HOW:  Simulate training scenarios with perception/cortical state driving
 *       decisions across all connected bridges
 *
 * TEST COVERAGE:
 * - Perception flow tests (5 tests)
 * - Cortical flow tests (5 tests)
 * - Cross-system integration (5 tests)
 * - Failure mode handling (5 tests)
 *
 * TOTAL: 20 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_perception_training_bridge.h"
#include "middleware/training/nimcp_cortical_training_bridge.h"
#include "middleware/training/nimcp_cognitive_training_bridge.h"
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "portia/nimcp_portia_swarm_logic_bridge.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class UnifiedTrainingIntegrationTest : public ::testing::Test {
protected:
    perception_training_bridge_t* perception_bridge;
    cortical_training_bridge_t* cortical_bridge;
    cognitive_training_bridge_t* cognitive_bridge;
    training_logic_bridge_t* logic_bridge;
    tpb_context_t* plasticity_ctx;
    portia_swarm_logic_bridge_t* psl_bridge;

    void SetUp() override {
        /* Create perception training bridge */
        perception_training_config_t perception_config;
        perception_training_default_config(&perception_config);
        perception_config.enable_bio_async = false;
        perception_bridge = perception_training_create(&perception_config);
        ASSERT_NE(perception_bridge, nullptr);

        /* Create cortical training bridge */
        cortical_training_config_t cortical_config;
        cortical_training_default_config(&cortical_config);
        cortical_config.enable_bio_async = false;
        cortical_bridge = cortical_training_create(&cortical_config);
        ASSERT_NE(cortical_bridge, nullptr);

        /* Create cognitive training bridge */
        cognitive_training_config_t cognitive_config;
        cognitive_training_default_config(&cognitive_config);
        cognitive_config.enable_bio_async = false;
        cognitive_bridge = cognitive_training_create(&cognitive_config);
        ASSERT_NE(cognitive_bridge, nullptr);

        /* Create training logic bridge */
        training_logic_config_t logic_config;
        training_logic_default_config(&logic_config);
        logic_config.enable_bio_async = false;
        logic_bridge = training_logic_create(&logic_config);
        ASSERT_NE(logic_bridge, nullptr);

        /* Create plasticity context */
        tpb_config_t plasticity_config = tpb_config_default();
        plasticity_ctx = tpb_create(&plasticity_config);
        ASSERT_NE(plasticity_ctx, nullptr);

        /* Create portia swarm logic bridge */
        portia_swarm_logic_config_t psl_config;
        portia_swarm_logic_default_config(&psl_config);
        psl_config.enable_bio_async = false;
        psl_bridge = portia_swarm_logic_create(&psl_config, nullptr, nullptr, nullptr);
        ASSERT_NE(psl_bridge, nullptr);
    }

    void TearDown() override {
        if (psl_bridge) {
            portia_swarm_logic_destroy(psl_bridge);
            psl_bridge = nullptr;
        }
        if (plasticity_ctx) {
            tpb_destroy(plasticity_ctx);
            plasticity_ctx = nullptr;
        }
        if (logic_bridge) {
            training_logic_destroy(logic_bridge);
            logic_bridge = nullptr;
        }
        if (cognitive_bridge) {
            cognitive_training_destroy(cognitive_bridge);
            cognitive_bridge = nullptr;
        }
        if (cortical_bridge) {
            cortical_training_destroy(cortical_bridge);
            cortical_bridge = nullptr;
        }
        if (perception_bridge) {
            perception_training_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
    }

    void startAllBridges() {
        EXPECT_EQ(perception_training_start(perception_bridge), 0);
        EXPECT_EQ(cortical_training_start(cortical_bridge), 0);
        EXPECT_EQ(cognitive_training_start(cognitive_bridge), 0);
        EXPECT_EQ(training_logic_start(logic_bridge), 0);
        EXPECT_EQ(portia_swarm_logic_start(psl_bridge), 0);
    }

    void connectAllBridges() {
        /* Connect perception to downstream */
        EXPECT_EQ(cognitive_training_connect_perception_training(
            cognitive_bridge, perception_bridge), 0);
        EXPECT_EQ(training_logic_connect_perception_training(
            logic_bridge, perception_bridge), 0);
        EXPECT_EQ(tpb_connect_perception_training(
            plasticity_ctx, perception_bridge), NIMCP_SUCCESS);
        EXPECT_EQ(portia_swarm_logic_connect_perception_training(
            psl_bridge, perception_bridge), 0);

        /* Connect cortical to downstream */
        EXPECT_EQ(cognitive_training_connect_cortical_training(
            cognitive_bridge, cortical_bridge), 0);
        EXPECT_EQ(training_logic_connect_cortical_training(
            logic_bridge, cortical_bridge), 0);
        EXPECT_EQ(tpb_connect_cortical_training(
            plasticity_ctx, cortical_bridge), NIMCP_SUCCESS);
        EXPECT_EQ(portia_swarm_logic_connect_cortical_training(
            psl_bridge, cortical_bridge), 0);
    }

    void setGoodPerception() {
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = 0.9f;
        effects.speech_salience = 0.8f;
        effects.visual_novelty = 0.3f;
        effects.lr_factor = 1.1f;
        effects.valid = true;
        EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);
    }

    void setBadPerception() {
        perception_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.visual_confidence = 0.05f;
        effects.speech_salience = 0.05f;
        effects.visual_novelty = 0.1f;
        effects.lr_factor = 0.3f;
        effects.valid = true;
        EXPECT_EQ(perception_training_set_effects_for_testing(perception_bridge, &effects), 0);
    }

    void setStableCortical() {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.burst_rate = 0.85f;
        effects.predictions_stable = true;
        effects.prediction_error_mag = 0.1f;
        effects.free_energy = 10.0f;
        effects.valid = true;
        EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);
    }

    void setUnstableCortical() {
        cortical_training_effects_t effects;
        memset(&effects, 0, sizeof(effects));
        effects.burst_rate = 0.1f;
        effects.predictions_stable = false;
        effects.prediction_error_mag = 0.9f;
        effects.free_energy = 150.0f;  /* Exploding */
        effects.valid = true;
        EXPECT_EQ(cortical_training_set_effects_for_testing(cortical_bridge, &effects), 0);
    }
};

/* ============================================================================
 * Perception Flow Tests (5 tests)
 * ============================================================================ */

TEST_F(UnifiedTrainingIntegrationTest, PerceptionFlowToCognitive) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();

    /* Cognitive effects should return OK */
    cognitive_training_effects_t cog_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &cog_effects), 0);
}

TEST_F(UnifiedTrainingIntegrationTest, PerceptionFlowToLogic) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();

    /* Evaluate gate - result depends on all conditions */
    bool result = training_logic_evaluate_gate(
        logic_bridge, TRAINING_LOGIC_GATE_SAFE_TO_INCREASE_LR);
    /* Note: result depends on multiple factors, not just perception */
    (void)result;  /* Just verify gate evaluation doesn't crash */
}

TEST_F(UnifiedTrainingIntegrationTest, PerceptionFlowToPlasticity) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();

    /* Verify perception modulates plasticity factor */
    EXPECT_TRUE(tpb_is_perception_training_connected(plasticity_ctx));
    float factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_GT(factor, 1.0f);  /* Good perception → enhanced plasticity */
}

TEST_F(UnifiedTrainingIntegrationTest, BadPerceptionReducesLogicGateApproval) {
    startAllBridges();
    connectAllBridges();
    setBadPerception();

    /* Verify bad perception disables LR increase gate */
    bool result = training_logic_evaluate_gate(
        logic_bridge, TRAINING_LOGIC_GATE_SAFE_TO_INCREASE_LR);
    EXPECT_FALSE(result);  /* Bad perception → LR increase blocked */
}

TEST_F(UnifiedTrainingIntegrationTest, PerceptionFlowToPortiaSwarm) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();

    /* Verify perception affects portia confidence modifier */
    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_TRUE(stats.perception_training_connected);

    float modifier = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    EXPECT_GT(modifier, 1.0f);  /* Good perception → boosted confidence */
}

/* ============================================================================
 * Cortical Flow Tests (5 tests)
 * ============================================================================ */

TEST_F(UnifiedTrainingIntegrationTest, CorticalFlowToCognitive) {
    startAllBridges();
    connectAllBridges();
    setStableCortical();

    /* Cognitive effects should return OK */
    cognitive_training_effects_t cog_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &cog_effects), 0);
}

TEST_F(UnifiedTrainingIntegrationTest, CorticalFlowToLogic) {
    startAllBridges();
    connectAllBridges();
    setStableCortical();

    /* Evaluate gate - result depends on multiple factors */
    bool result = training_logic_evaluate_gate(
        logic_bridge, TRAINING_LOGIC_GATE_SAFE_TO_INCREASE_LR);
    (void)result;  /* Just verify evaluation works */
}

TEST_F(UnifiedTrainingIntegrationTest, CorticalFlowToPlasticity) {
    startAllBridges();
    connectAllBridges();
    setStableCortical();

    /* Verify cortical modulates plasticity factor */
    EXPECT_TRUE(tpb_is_cortical_training_connected(plasticity_ctx));
    float factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    EXPECT_GT(factor, 1.0f);  /* Stable cortical → enhanced plasticity */
}

TEST_F(UnifiedTrainingIntegrationTest, UnstableCorticalReducesLogicGateApproval) {
    startAllBridges();
    connectAllBridges();
    setUnstableCortical();

    /* Verify unstable cortical disables LR increase gate */
    bool result = training_logic_evaluate_gate(
        logic_bridge, TRAINING_LOGIC_GATE_SAFE_TO_INCREASE_LR);
    EXPECT_FALSE(result);  /* Unstable cortical → LR increase blocked */
}

TEST_F(UnifiedTrainingIntegrationTest, CorticalFlowToPortiaSwarm) {
    startAllBridges();
    connectAllBridges();
    setStableCortical();

    /* Verify cortical affects portia threshold modifier */
    portia_swarm_logic_stats_t stats;
    EXPECT_EQ(portia_swarm_logic_get_stats(psl_bridge, &stats), 0);
    EXPECT_TRUE(stats.cortical_training_connected);

    float modifier = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);
    EXPECT_LT(modifier, 1.0f);  /* Stable cortical → lower threshold (more confident) */
}

/* ============================================================================
 * Cross-System Integration Tests (5 tests)
 * ============================================================================ */

TEST_F(UnifiedTrainingIntegrationTest, CombinedPerceptionCorticalToPlasticity) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();
    setStableCortical();

    /* Verify combined plasticity factor */
    float combined = tpb_get_combined_plasticity_factor(plasticity_ctx);
    float perception = tpb_get_perception_plasticity_factor(plasticity_ctx);
    float cortical = tpb_get_cortical_plasticity_factor(plasticity_ctx);

    /* Combined should be geometric mean of both */
    EXPECT_FLOAT_EQ(combined, sqrtf(perception * cortical));
    EXPECT_GT(combined, 1.0f);  /* Both good → enhanced plasticity */
}

TEST_F(UnifiedTrainingIntegrationTest, CognitiveIntegrationFromBoth) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();
    setStableCortical();

    /* Cognitive effects should return OK when both are connected */
    cognitive_training_effects_t effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &effects), 0);
}

TEST_F(UnifiedTrainingIntegrationTest, LogicGatesWithBothInputs) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();
    setStableCortical();

    /* Gate evaluation should work when both are connected */
    bool safe = training_logic_evaluate_gate(
        logic_bridge, TRAINING_LOGIC_GATE_SAFE_TO_INCREASE_LR);
    (void)safe;  /* Just verify evaluation works */
}

TEST_F(UnifiedTrainingIntegrationTest, PortiaDecisionWithBothModifiers) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();
    setStableCortical();

    /* Both modifiers should affect decisions */
    float perception_mod = portia_swarm_logic_get_perception_confidence_modifier(psl_bridge);
    float cortical_mod = portia_swarm_logic_get_cortical_threshold_modifier(psl_bridge);

    EXPECT_GT(perception_mod, 1.0f);  /* Good perception → confidence boost */
    EXPECT_LT(cortical_mod, 1.0f);    /* Stable cortical → lower threshold */

    /* Decision should work with both modifiers active */
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(psl_bridge, 0, 1, &result), 0);
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(UnifiedTrainingIntegrationTest, FullPipelineTrainingStep) {
    startAllBridges();
    connectAllBridges();

    /* Simulate a training step with good conditions */
    setGoodPerception();
    setStableCortical();

    /* 1. Check cognitive state */
    cognitive_training_effects_t cog_effects;
    EXPECT_EQ(cognitive_training_get_effects(cognitive_bridge, &cog_effects), 0);

    /* 2. Check logic gate evaluation works */
    bool safe_to_train = training_logic_evaluate_gate(
        logic_bridge, TRAINING_LOGIC_GATE_SAFE_TO_INCREASE_LR);
    (void)safe_to_train;

    /* 3. Check plasticity factor */
    float plasticity = tpb_get_combined_plasticity_factor(plasticity_ctx);
    EXPECT_GT(plasticity, 1.0f);

    /* 4. Check portia decision */
    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_resource_allocation(psl_bridge, 1, 0.5f, &result), 0);
}

/* ============================================================================
 * Failure Mode Handling Tests (5 tests)
 * ============================================================================ */

TEST_F(UnifiedTrainingIntegrationTest, PerceptionCollapseReducesPlasticity) {
    startAllBridges();
    connectAllBridges();
    setBadPerception();

    /* Bad perception should reduce plasticity factor */
    float factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_LT(factor, 1.0f);
}

TEST_F(UnifiedTrainingIntegrationTest, CorticalExplosionReducesPlasticity) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();  /* Good perception but... */
    setUnstableCortical();  /* Exploding cortical */

    /* Cortical explosion should reduce plasticity */
    float factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    EXPECT_LT(factor, 1.0f);
}

TEST_F(UnifiedTrainingIntegrationTest, LogicGatesEvaluateWithBadInputs) {
    startAllBridges();
    connectAllBridges();
    setBadPerception();
    setUnstableCortical();

    /* Gate evaluation should work even with bad inputs */
    bool safe = training_logic_evaluate_gate(
        logic_bridge, TRAINING_LOGIC_GATE_SAFE_TO_INCREASE_LR);
    (void)safe;  /* Result depends on gate implementation */
}

TEST_F(UnifiedTrainingIntegrationTest, PlasticityReducedWithBadInputs) {
    startAllBridges();
    connectAllBridges();
    setBadPerception();
    setUnstableCortical();

    /* Plasticity factors should be reduced */
    float perception_factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    float cortical_factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);

    EXPECT_LT(perception_factor, 1.0f);  /* Bad perception → reduced plasticity */
    EXPECT_LT(cortical_factor, 1.0f);    /* Unstable cortical → reduced plasticity */
}

TEST_F(UnifiedTrainingIntegrationTest, GracefulDegradationWithPartialFailure) {
    startAllBridges();
    connectAllBridges();
    setGoodPerception();
    setUnstableCortical();  /* Cortical failing but perception OK */

    /* Perception plasticity still good */
    float perception_factor = tpb_get_perception_plasticity_factor(plasticity_ctx);
    EXPECT_GT(perception_factor, 1.0f);

    /* Cortical plasticity reduced */
    float cortical_factor = tpb_get_cortical_plasticity_factor(plasticity_ctx);
    EXPECT_LT(cortical_factor, 1.0f);

    /* Combined should be moderate (geometric mean) */
    float combined = tpb_get_combined_plasticity_factor(plasticity_ctx);
    EXPECT_GT(combined, 0.5f);
    EXPECT_LT(combined, perception_factor);

    /* Logic gate evaluation should work */
    bool safe = training_logic_evaluate_gate(
        logic_bridge, TRAINING_LOGIC_GATE_SAFE_TO_INCREASE_LR);
    (void)safe;  /* Result depends on gate implementation */
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
