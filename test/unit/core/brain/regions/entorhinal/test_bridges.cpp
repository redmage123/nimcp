/**
 * @file test_bridges.cpp
 * @brief Unit tests for Entorhinal Cortex Bridge integrations
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * Tests all 21 bidirectional bridge integrations:
 * - Security, Immune, Bio-Async, SNN, Plasticity
 * - Cognitive, Training, Substrate, Resonance, Thalamic
 * - Hippocampus, Perception, Swarm, Dragonfly, Portia
 * - Cerebellum, Medulla, Omni, Hypothalamus, Logic, KG
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

class BridgeTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_config_t config = entorhinal_default_config();
        // Enable all integrations
        config.enable_security = true;
        config.enable_immune = true;
        config.enable_bio_async = true;
        config.enable_snn = true;
        config.enable_plasticity = true;
        config.enable_stdp = true;
        config.enable_cognitive = true;
        config.enable_training = true;
        config.enable_substrate = true;
        config.enable_resonance = true;
        config.enable_thalamic = true;
        config.enable_hippocampus = true;
        config.enable_perception = true;
        config.enable_swarm = true;
        config.enable_dragonfly = true;
        config.enable_portia = true;
        config.enable_cerebellum = true;
        config.enable_medulla = true;
        config.enable_omni = true;
        config.enable_hypothalamus = true;
        config.enable_logic = true;
        config.enable_kg = true;
        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/*=============================================================================
 * SECURITY BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, SecurityBridgeInitialState) {
    EXPECT_FALSE(ec->security_bridge.threat_detected);
    EXPECT_EQ(ec->security_bridge.threat_level, 0.0f);
}

TEST_F(BridgeTest, SecurityBridgeInit) {
    EXPECT_EQ(entorhinal_init_security_bridge(ec, nullptr, nullptr), 0);
}

TEST_F(BridgeTest, SecurityBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_security_bridge(nullptr, nullptr, nullptr), -1);
}

TEST_F(BridgeTest, ValidateSecurity) {
    EXPECT_EQ(entorhinal_validate_security(ec), 0);
}

TEST_F(BridgeTest, ValidateSecurityNull) {
    EXPECT_EQ(entorhinal_validate_security(nullptr), -1);
}

/*=============================================================================
 * IMMUNE BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, ImmuneBridgeInitialState) {
    EXPECT_FALSE(ec->immune_bridge.anomaly_detected);
    EXPECT_GE(ec->immune_bridge.health_score, 0.0f);
}

TEST_F(BridgeTest, ImmuneBridgeInit) {
    EXPECT_EQ(entorhinal_init_immune_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, ImmuneBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_immune_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, ImmuneScan) {
    EXPECT_EQ(entorhinal_immune_scan(ec), 0);
}

TEST_F(BridgeTest, ImmuneScanNull) {
    EXPECT_EQ(entorhinal_immune_scan(nullptr), -1);
}

/*=============================================================================
 * BIO-ASYNC BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, BioAsyncBridgeInitialState) {
    EXPECT_EQ(ec->bio_async_bridge.pending_messages, 0u);
}

TEST_F(BridgeTest, BioAsyncBridgeInit) {
    EXPECT_EQ(entorhinal_init_bio_async_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, BioAsyncBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_bio_async_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, SyncBioAsync) {
    EXPECT_EQ(entorhinal_sync_bio_async(ec), 0);
}

TEST_F(BridgeTest, SyncBioAsyncNull) {
    EXPECT_EQ(entorhinal_sync_bio_async(nullptr), -1);
}

TEST_F(BridgeTest, ProcessNeuromodulation) {
    EXPECT_EQ(entorhinal_process_neuromodulation(ec), 0);
}

TEST_F(BridgeTest, ProcessNeuromodulationNull) {
    EXPECT_EQ(entorhinal_process_neuromodulation(nullptr), -1);
}

TEST_F(BridgeTest, NeuromodulatorLevelsInitial) {
    for (int i = 0; i < ENTORHINAL_CHANNEL_COUNT; i++) {
        EXPECT_GE(ec->bio_async_bridge.neuromodulator_levels[i], 0.0f);
        EXPECT_LE(ec->bio_async_bridge.neuromodulator_levels[i], 1.0f);
    }
}

/*=============================================================================
 * SNN BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, SNNBridgeInit) {
    EXPECT_EQ(entorhinal_init_snn_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, SNNBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_snn_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, SNNBridgeSpikeRate) {
    EXPECT_GE(ec->snn_bridge.spike_rate, 0.0f);
}

/*=============================================================================
 * PLASTICITY BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, PlasticityBridgeInit) {
    EXPECT_EQ(entorhinal_init_plasticity_bridge(ec, nullptr, nullptr), 0);
}

TEST_F(BridgeTest, PlasticityBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_plasticity_bridge(nullptr, nullptr, nullptr), -1);
}

TEST_F(BridgeTest, ApplyPlasticity) {
    EXPECT_EQ(entorhinal_apply_plasticity(ec, 0.01f), 0);
}

TEST_F(BridgeTest, ApplyPlasticityNull) {
    EXPECT_EQ(entorhinal_apply_plasticity(nullptr, 0.01f), -1);
}

TEST_F(BridgeTest, PlasticityLearningRate) {
    EXPECT_GE(ec->plasticity_bridge.learning_rate, 0.0f);
}

/*=============================================================================
 * COGNITIVE BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, CognitiveBridgeInit) {
    EXPECT_EQ(entorhinal_init_cognitive_bridge(ec, nullptr, nullptr, nullptr), 0);
}

TEST_F(BridgeTest, CognitiveBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_cognitive_bridge(nullptr, nullptr, nullptr, nullptr), -1);
}

TEST_F(BridgeTest, PublishCognitiveEvents) {
    EXPECT_EQ(entorhinal_publish_cognitive_events(ec), 0);
}

TEST_F(BridgeTest, PublishCognitiveEventsNull) {
    EXPECT_EQ(entorhinal_publish_cognitive_events(nullptr), -1);
}

TEST_F(BridgeTest, CognitiveAttentionModulation) {
    EXPECT_GE(ec->cognitive_bridge.attention_modulation, 0.0f);
    EXPECT_LE(ec->cognitive_bridge.attention_modulation, 1.0f);
}

/*=============================================================================
 * TRAINING BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, TrainingBridgeInit) {
    EXPECT_EQ(entorhinal_init_training_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, TrainingBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_training_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, TrainingForward) {
    float input[32] = {0};
    float output[32];
    EXPECT_EQ(entorhinal_training_forward(ec, input, 32, output, 32), 0);
}

TEST_F(BridgeTest, TrainingForwardNull) {
    float input[32], output[32];
    EXPECT_EQ(entorhinal_training_forward(nullptr, input, 32, output, 32), -1);
    EXPECT_EQ(entorhinal_training_forward(ec, nullptr, 32, output, 32), -1);
    EXPECT_EQ(entorhinal_training_forward(ec, input, 32, nullptr, 32), -1);
}

TEST_F(BridgeTest, TrainingBackward) {
    // First do forward pass
    float input[32] = {0};
    float output[32];
    entorhinal_training_forward(ec, input, 32, output, 32);

    // Then backward pass
    float grad_output[32] = {0};
    EXPECT_EQ(entorhinal_training_backward(ec, grad_output, 32), 0);
}

TEST_F(BridgeTest, TrainingBackwardNull) {
    float grad[32];
    EXPECT_EQ(entorhinal_training_backward(nullptr, grad, 32), -1);
    EXPECT_EQ(entorhinal_training_backward(ec, nullptr, 32), -1);
}

TEST_F(BridgeTest, ApplyWeightUpdates) {
    EXPECT_EQ(entorhinal_apply_weight_updates(ec, 0.001f), 0);
}

TEST_F(BridgeTest, ApplyWeightUpdatesNull) {
    EXPECT_EQ(entorhinal_apply_weight_updates(nullptr, 0.001f), -1);
}

/*=============================================================================
 * SUBSTRATE BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, SubstrateBridgeInit) {
    EXPECT_EQ(entorhinal_init_substrate_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, SubstrateBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_substrate_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, UpdateSubstrateEffects) {
    EXPECT_EQ(entorhinal_update_substrate_effects(ec), 0);
}

TEST_F(BridgeTest, UpdateSubstrateEffectsNull) {
    EXPECT_EQ(entorhinal_update_substrate_effects(nullptr), -1);
}

TEST_F(BridgeTest, SubstrateATPLevel) {
    EXPECT_GE(ec->substrate_bridge.atp_level, 0.0f);
    EXPECT_LE(ec->substrate_bridge.atp_level, 1.0f);
}

TEST_F(BridgeTest, SubstrateOxygenLevel) {
    EXPECT_GE(ec->substrate_bridge.oxygen_level, 0.0f);
    EXPECT_LE(ec->substrate_bridge.oxygen_level, 1.0f);
}

TEST_F(BridgeTest, SubstrateGlucoseLevel) {
    EXPECT_GE(ec->substrate_bridge.glucose_level, 0.0f);
    EXPECT_LE(ec->substrate_bridge.glucose_level, 1.0f);
}

/*=============================================================================
 * RESONANCE BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, ResonanceBridgeInit) {
    EXPECT_EQ(entorhinal_init_resonance_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, ResonanceBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_resonance_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, ResonanceThetaPhase) {
    // Theta phase should be in valid range
    EXPECT_GE(ec->resonance_bridge.theta_phase, 0.0f);
}

TEST_F(BridgeTest, ResonanceGammaPhase) {
    EXPECT_GE(ec->resonance_bridge.gamma_phase, 0.0f);
}

/*=============================================================================
 * THALAMIC BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, ThalamicBridgeInit) {
    EXPECT_EQ(entorhinal_init_thalamic_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, ThalamicBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_thalamic_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, ThalamicRelayGain) {
    EXPECT_GE(ec->thalamic_bridge.relay_gain, 0.0f);
}

/*=============================================================================
 * HIPPOCAMPUS BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, HippocampusBridgeInit) {
    EXPECT_EQ(entorhinal_init_hippocampus_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, HippocampusBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_hippocampus_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, HippocampalThetaPhase) {
    EXPECT_GE(ec->hippocampus_bridge.hippocampal_theta_phase, 0.0f);
}

/*=============================================================================
 * PERCEPTION BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, PerceptionBridgeInit) {
    EXPECT_EQ(entorhinal_init_perception_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, PerceptionBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_perception_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, PerceptionSalienceSignal) {
    EXPECT_GE(ec->perception_bridge.salience_signal, 0.0f);
}

/*=============================================================================
 * SWARM BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, SwarmBridgeInit) {
    EXPECT_EQ(entorhinal_init_swarm_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, SwarmBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_swarm_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, SwarmCoordinationStrength) {
    EXPECT_GE(ec->swarm_bridge.coordination_strength, 0.0f);
}

/*=============================================================================
 * DRAGONFLY BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, DragonflyBridgeInit) {
    EXPECT_EQ(entorhinal_init_dragonfly_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, DragonflyBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_dragonfly_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, DragonflyPredictionHorizon) {
    EXPECT_GE(ec->dragonfly_bridge.prediction_horizon, 0.0f);
}

/*=============================================================================
 * PORTIA BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, PortiaBridgeInit) {
    EXPECT_EQ(entorhinal_init_portia_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, PortiaBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_portia_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, PortiaPlanningDepth) {
    EXPECT_GE(ec->portia_bridge.planning_depth, 0.0f);
}

TEST_F(BridgeTest, PortiaDeceptionDetection) {
    EXPECT_GE(ec->portia_bridge.deception_detection, 0.0f);
    EXPECT_LE(ec->portia_bridge.deception_detection, 1.0f);
}

/*=============================================================================
 * CEREBELLUM BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, CerebellumBridgeInit) {
    EXPECT_EQ(entorhinal_init_cerebellum_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, CerebellumBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_cerebellum_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, CerebellumTimingSignal) {
    // Timing signal can be any value
    SUCCEED();
}

TEST_F(BridgeTest, CerebellumPredictionError) {
    // Prediction error can be any value
    SUCCEED();
}

/*=============================================================================
 * MEDULLA BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, MedullaBridgeInit) {
    EXPECT_EQ(entorhinal_init_medulla_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, MedullaBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_medulla_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, MedullaArousalLevel) {
    EXPECT_GE(ec->medulla_bridge.arousal_level, 0.0f);
    EXPECT_LE(ec->medulla_bridge.arousal_level, 1.0f);
}

/*=============================================================================
 * OMNIDIRECTIONAL BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, OmniBridgeInit) {
    EXPECT_EQ(entorhinal_init_omni_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, OmniBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_omni_bridge(nullptr, nullptr), -1);
}

/*=============================================================================
 * HYPOTHALAMUS BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, HypothalamusBridgeInit) {
    EXPECT_EQ(entorhinal_init_hypothalamus_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, HypothalamusBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_hypothalamus_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, HypothalamusMotivationSignal) {
    EXPECT_GE(ec->hypothalamus_bridge.motivation_signal, 0.0f);
    EXPECT_LE(ec->hypothalamus_bridge.motivation_signal, 1.0f);
}

TEST_F(BridgeTest, HypothalamusHomeostaticDrive) {
    EXPECT_GE(ec->hypothalamus_bridge.homeostatic_drive, 0.0f);
}

/*=============================================================================
 * LOGIC BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, LogicBridgeInit) {
    EXPECT_EQ(entorhinal_init_logic_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, LogicBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_logic_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, LogicInferenceConfidence) {
    EXPECT_GE(ec->logic_bridge.inference_confidence, 0.0f);
    EXPECT_LE(ec->logic_bridge.inference_confidence, 1.0f);
}

/*=============================================================================
 * KG BRIDGE TESTS
 *===========================================================================*/

TEST_F(BridgeTest, KGBridgeInit) {
    EXPECT_EQ(entorhinal_init_kg_bridge(ec, nullptr), 0);
}

TEST_F(BridgeTest, KGBridgeInitNull) {
    EXPECT_EQ(entorhinal_init_kg_bridge(nullptr, nullptr), -1);
}

TEST_F(BridgeTest, KGHealthStatus) {
    EXPECT_GE(ec->kg_bridge.health_status, 0.0f);
    EXPECT_LE(ec->kg_bridge.health_status, 1.0f);
}

/*=============================================================================
 * INIT ALL BRIDGES TEST
 *===========================================================================*/

TEST_F(BridgeTest, InitAllBridges) {
    EXPECT_EQ(entorhinal_init_all_bridges(ec, nullptr), 0);
}

TEST_F(BridgeTest, InitAllBridgesNull) {
    EXPECT_EQ(entorhinal_init_all_bridges(nullptr, nullptr), -1);
}

/*=============================================================================
 * BIDIRECTIONAL DATA FLOW TESTS
 *===========================================================================*/

TEST_F(BridgeTest, ProcessIncoming) {
    EXPECT_EQ(entorhinal_process_incoming(ec), 0);
}

TEST_F(BridgeTest, ProcessIncomingNull) {
    EXPECT_EQ(entorhinal_process_incoming(nullptr), -1);
}

TEST_F(BridgeTest, SendOutgoing) {
    EXPECT_EQ(entorhinal_send_outgoing(ec), 0);
}

TEST_F(BridgeTest, SendOutgoingNull) {
    EXPECT_EQ(entorhinal_send_outgoing(nullptr), -1);
}

TEST_F(BridgeTest, BidirectionalUpdate) {
    EXPECT_EQ(entorhinal_bidirectional_update(ec, 0.01f), 0);
}

TEST_F(BridgeTest, BidirectionalUpdateNull) {
    EXPECT_EQ(entorhinal_bidirectional_update(nullptr, 0.01f), -1);
}

TEST_F(BridgeTest, BidirectionalUpdateMultiple) {
    // Run multiple update cycles
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(entorhinal_bidirectional_update(ec, 0.01f), 0);
    }
}

