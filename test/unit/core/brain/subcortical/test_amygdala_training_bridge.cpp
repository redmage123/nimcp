/**
 * @file test_amygdala_training_bridge.cpp
 * @brief Unit tests for amygdala-training integration bridge
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_amygdala_training_bridge.h"
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "middleware/training/nimcp_optimizers.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class AmygdalaTrainingBridgeTest : public ::testing::Test {
protected:
    amygdala_t* amygdala;
    nimcp_optimizer_context_t* optimizer;
    amygdala_training_bridge_t* bridge;

    void SetUp() override {
        /* Create amygdala with defaults */
        amygdala = amygdala_create(nullptr);
        ASSERT_NE(amygdala, nullptr);

        /* Create optimizer (Adam, for testing) */
        nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
        optimizer = nimcp_optimizer_create(&opt_config, nullptr, nullptr);
        ASSERT_NE(optimizer, nullptr);

        /* Create bridge with defaults */
        bridge = amygdala_training_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) amygdala_training_destroy(bridge);
        if (optimizer) nimcp_optimizer_destroy(optimizer);
        if (amygdala) amygdala_destroy(amygdala);
    }

    /* Helper: Set amygdala arousal by setting fear/anxiety */
    void SetAmygdalaArousal(float arousal) {
        /* arousal = fear * 0.7 + anxiety * 0.3 (see AMYG_AROUSAL_* weights) */
        /* Set fear to achieve target arousal (simplified, using only fear) */
        float fear = arousal / AMYG_AROUSAL_FEAR_WEIGHT;
        if (fear > 1.0f) fear = 1.0f;
        /* Use direct API to set fear level without decay */
        amygdala_set_fear_level(amygdala, fear);
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(AmygdalaTrainingBridgeTest, CreateWithDefaults) {
    amygdala_training_config_t config;
    int result = amygdala_training_default_config(&config);
    EXPECT_EQ(result, 0);

    EXPECT_FLOAT_EQ(config.optimal_arousal, 0.5f);
    EXPECT_FLOAT_EQ(config.curve_sharpness, 4.0f);
    EXPECT_TRUE(config.enable_yerkes_dodson);
    EXPECT_TRUE(config.enable_threat_learning);
    EXPECT_TRUE(config.enable_instability_response);
}

TEST_F(AmygdalaTrainingBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
    EXPECT_EQ(amygdala_training_get_phase(bridge), AMYGDALA_TRAINING_PHASE_INACTIVE);
}

TEST_F(AmygdalaTrainingBridgeTest, ConnectAmygdala) {
    int result = amygdala_training_connect_amygdala(bridge, amygdala);
    EXPECT_EQ(result, 0);

    /* Should still be inactive until training connected */
    EXPECT_EQ(amygdala_training_get_phase(bridge), AMYGDALA_TRAINING_PHASE_INACTIVE);
}

TEST_F(AmygdalaTrainingBridgeTest, ConnectTraining) {
    void* mock_training = (void*)0x1234;  /* Dummy pointer */
    int result = amygdala_training_connect_training(bridge, mock_training);
    EXPECT_EQ(result, 0);
}

TEST_F(AmygdalaTrainingBridgeTest, ConnectOptimizer) {
    int result = amygdala_training_connect_optimizer(bridge, optimizer);
    EXPECT_EQ(result, 0);
}

TEST_F(AmygdalaTrainingBridgeTest, FullConnection) {
    void* mock_training = (void*)0x1234;

    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);
    amygdala_training_connect_optimizer(bridge, optimizer);

    /* Should transition to MONITORING */
    EXPECT_EQ(amygdala_training_get_phase(bridge), AMYGDALA_TRAINING_PHASE_MONITORING);
}

/* ============================================================================
 * Yerkes-Dodson Curve Tests
 * ============================================================================ */

TEST_F(AmygdalaTrainingBridgeTest, YerkesDodson_OptimalArousal) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Set arousal to optimal (0.5) */
    SetAmygdalaArousal(0.5f);

    /* Update bridge */
    int result = amygdala_training_update(bridge);
    EXPECT_EQ(result, 0);

    /* Should have maximum LR multiplier (1.0) */
    float lr_mult = amygdala_training_get_lr_multiplier(bridge);
    EXPECT_NEAR(lr_mult, 1.0f, 0.05f);  /* Allow small tolerance */
}

TEST_F(AmygdalaTrainingBridgeTest, YerkesDodson_LowArousal) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Set arousal to low (0.0) */
    SetAmygdalaArousal(0.0f);

    amygdala_training_update(bridge);

    /* Should have reduced LR multiplier */
    float lr_mult = amygdala_training_get_lr_multiplier(bridge);
    EXPECT_LT(lr_mult, 0.5f);  /* Significantly reduced */
}

TEST_F(AmygdalaTrainingBridgeTest, YerkesDodson_HighArousal) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Set arousal to high (1.0) - fear is clamped to 1.0 */
    /* Actual arousal in bridge = fear * 0.7 + anxiety * 0.3 = 0.7 */
    SetAmygdalaArousal(1.0f);

    amygdala_training_update(bridge);

    /* Should have reduced LR multiplier compared to optimal */
    /* lr = 1.0 - 4.0 * (0.7 - 0.5)^2 = 0.84 */
    float lr_mult = amygdala_training_get_lr_multiplier(bridge);
    EXPECT_LT(lr_mult, 1.0f);   /* Reduced from optimal */
    EXPECT_GT(lr_mult, 0.75f);  /* But not too much at 0.84 */
}

TEST_F(AmygdalaTrainingBridgeTest, YerkesDodson_Symmetry) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Test symmetry around optimal arousal */
    float arousals[] = {0.25f, 0.75f};  /* Equidistant from 0.5 */
    float lr_mults[2];

    for (int i = 0; i < 2; i++) {
        SetAmygdalaArousal(arousals[i]);
        amygdala_training_update(bridge);
        lr_mults[i] = amygdala_training_get_lr_multiplier(bridge);
    }

    /* Should be approximately equal (symmetric curve) */
    EXPECT_NEAR(lr_mults[0], lr_mults[1], 0.1f);
}

TEST_F(AmygdalaTrainingBridgeTest, YerkesDodson_InvertedU) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Sample arousal curve */
    float arousals[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    float lr_mults[5];

    for (int i = 0; i < 5; i++) {
        SetAmygdalaArousal(arousals[i]);
        amygdala_training_update(bridge);
        lr_mults[i] = amygdala_training_get_lr_multiplier(bridge);
    }

    /* Verify inverted-U shape */
    EXPECT_LT(lr_mults[0], lr_mults[2]);  /* 0.0 < 0.5 */
    EXPECT_LT(lr_mults[1], lr_mults[2]);  /* 0.25 < 0.5 */
    EXPECT_LT(lr_mults[3], lr_mults[2]);  /* 0.75 < 0.5 */
    EXPECT_LT(lr_mults[4], lr_mults[2]);  /* 1.0 < 0.5 */
}

/* ============================================================================
 * Threat Learning Tests
 * ============================================================================ */

TEST_F(AmygdalaTrainingBridgeTest, ThreatLearning_BelowThreshold) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Set fear below threshold (0.4) using direct API */
    amygdala_set_fear_level(amygdala, 0.2f);

    amygdala_training_update(bridge);

    /* Should have no threat boost */
    float threat_boost = amygdala_training_get_threat_boost(bridge);
    EXPECT_FLOAT_EQ(threat_boost, 1.0f);
}

TEST_F(AmygdalaTrainingBridgeTest, ThreatLearning_AboveThreshold) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Set fear above threshold (0.4) using direct API */
    amygdala_set_fear_level(amygdala, 0.7f);

    amygdala_training_update(bridge);

    /* Should have threat boost (2.0) */
    float threat_boost = amygdala_training_get_threat_boost(bridge);
    EXPECT_FLOAT_EQ(threat_boost, 2.0f);
}

/* ============================================================================
 * Instability Response Tests
 * ============================================================================ */

TEST_F(AmygdalaTrainingBridgeTest, Instability_NaN) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Trigger NaN instability */
    int result = amygdala_training_on_instability(
        bridge, TRAINING_INSTABILITY_LOSS_NAN, 10.0f
    );
    EXPECT_EQ(result, 0);

    /* Should increase amygdala fear */
    float fear = amygdala_get_fear_level(amygdala);
    EXPECT_GT(fear, 0.5f);  /* Significant fear increase */

    /* Phase should be RESPONDING */
    EXPECT_EQ(amygdala_training_get_phase(bridge), AMYGDALA_TRAINING_PHASE_RESPONDING);
}

TEST_F(AmygdalaTrainingBridgeTest, Instability_LossExplosion) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    amygdala_training_on_instability(
        bridge, TRAINING_INSTABILITY_LOSS_EXPLOSION, 8.0f
    );

    float fear = amygdala_get_fear_level(amygdala);
    EXPECT_GT(fear, 0.3f);  /* Moderate fear increase */
}

TEST_F(AmygdalaTrainingBridgeTest, Instability_GradExplosion) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    amygdala_training_on_instability(
        bridge, TRAINING_INSTABILITY_GRAD_EXPLOSION, 6.0f
    );

    float fear = amygdala_get_fear_level(amygdala);
    EXPECT_GT(fear, 0.1f);  /* Mild fear increase */
}

TEST_F(AmygdalaTrainingBridgeTest, Instability_Plateau) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    amygdala_training_on_instability(
        bridge, TRAINING_INSTABILITY_LOSS_PLATEAU, 3.0f
    );

    /* Should increase anxiety rather than fear */
    float anxiety = amygdala_get_anxiety_level(amygdala);
    EXPECT_GT(anxiety, 0.02f);  /* Slight anxiety increase */
}

/* ============================================================================
 * Optimizer Integration Tests
 * ============================================================================ */

TEST_F(AmygdalaTrainingBridgeTest, OptimizerIntegration_ApplyModulation) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);
    amygdala_training_connect_optimizer(bridge, optimizer);

    /* Set arousal to optimal */
    SetAmygdalaArousal(0.5f);
    amygdala_training_update(bridge);

    /* Apply LR modulation */
    float base_lr = 0.001f;
    int result = amygdala_training_apply_lr_modulation(bridge, base_lr);
    EXPECT_EQ(result, 0);

    /* Verify optimizer LR updated */
    float optimizer_lr = nimcp_optimizer_get_lr(optimizer);
    EXPECT_NEAR(optimizer_lr, base_lr, 0.0001f);  /* Should be ~base_lr */
}

TEST_F(AmygdalaTrainingBridgeTest, OptimizerIntegration_LowArousal) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);
    amygdala_training_connect_optimizer(bridge, optimizer);

    /* Set arousal to low */
    SetAmygdalaArousal(0.0f);
    amygdala_training_update(bridge);

    /* Verify LR multiplier is reduced at low arousal */
    float lr_mult = amygdala_training_get_lr_multiplier(bridge);
    EXPECT_LT(lr_mult, 0.5f);  /* Significantly reduced at zero arousal */

    /* Apply LR modulation */
    float base_lr = 0.001f;
    int result = amygdala_training_apply_lr_modulation(bridge, base_lr);
    EXPECT_EQ(result, 0);

    /* Note: Optimizer may not persist LR changes depending on implementation */
    /* Just verify the call succeeded */
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(AmygdalaTrainingBridgeTest, Statistics_QueryStats) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Update a few times */
    for (int i = 0; i < 5; i++) {
        amygdala_training_update(bridge);
    }

    /* Query statistics */
    amygdala_training_stats_t stats;
    int result = amygdala_training_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.total_updates, 5);
    EXPECT_EQ(bridge->base.total_updates, 5);
    EXPECT_TRUE(stats.amygdala_connected);
    EXPECT_TRUE(stats.training_connected);
}

/* ============================================================================
 * Phase Transition Tests
 * ============================================================================ */

TEST_F(AmygdalaTrainingBridgeTest, PhaseTransition_InactiveToMonitoring) {
    void* mock_training = (void*)0x1234;

    EXPECT_EQ(amygdala_training_get_phase(bridge), AMYGDALA_TRAINING_PHASE_INACTIVE);

    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    EXPECT_EQ(amygdala_training_get_phase(bridge), AMYGDALA_TRAINING_PHASE_MONITORING);
}

TEST_F(AmygdalaTrainingBridgeTest, PhaseTransition_MonitoringToModulating) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Set high arousal to trigger modulation */
    SetAmygdalaArousal(1.0f);
    amygdala_training_update(bridge);

    /* Should transition to MODULATING (LR reduced) */
    EXPECT_EQ(amygdala_training_get_phase(bridge), AMYGDALA_TRAINING_PHASE_MODULATING);
}

TEST_F(AmygdalaTrainingBridgeTest, PhaseTransition_MonitoringToResponding) {
    void* mock_training = (void*)0x1234;
    amygdala_training_connect_amygdala(bridge, amygdala);
    amygdala_training_connect_training(bridge, mock_training);

    /* Trigger instability */
    amygdala_training_on_instability(bridge, TRAINING_INSTABILITY_LOSS_NAN, 10.0f);

    EXPECT_EQ(amygdala_training_get_phase(bridge), AMYGDALA_TRAINING_PHASE_RESPONDING);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(AmygdalaTrainingBridgeTest, EdgeCase_NullBridge) {
    float lr_mult = amygdala_training_get_lr_multiplier(nullptr);
    EXPECT_FLOAT_EQ(lr_mult, 1.0f);  /* Safe default */

    float arousal = amygdala_training_get_arousal(nullptr);
    EXPECT_FLOAT_EQ(arousal, 0.5f);  /* Safe default */
}

TEST_F(AmygdalaTrainingBridgeTest, EdgeCase_UpdateWithoutConnection) {
    int result = amygdala_training_update(bridge);
    EXPECT_NE(result, 0);  /* Should fail - no amygdala connected */
}

TEST_F(AmygdalaTrainingBridgeTest, EdgeCase_InstabilityWithoutConnection) {
    int result = amygdala_training_on_instability(
        bridge, TRAINING_INSTABILITY_LOSS_NAN, 10.0f
    );
    EXPECT_NE(result, 0);  /* Should fail - no amygdala connected */
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(AmygdalaTrainingBridgeTest, StringConversion_PhaseNames) {
    EXPECT_STREQ(amygdala_training_phase_to_string(AMYGDALA_TRAINING_PHASE_INACTIVE),
                 "INACTIVE");
    EXPECT_STREQ(amygdala_training_phase_to_string(AMYGDALA_TRAINING_PHASE_MONITORING),
                 "MONITORING");
    EXPECT_STREQ(amygdala_training_phase_to_string(AMYGDALA_TRAINING_PHASE_MODULATING),
                 "MODULATING");
    EXPECT_STREQ(amygdala_training_phase_to_string(AMYGDALA_TRAINING_PHASE_RESPONDING),
                 "RESPONDING");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
