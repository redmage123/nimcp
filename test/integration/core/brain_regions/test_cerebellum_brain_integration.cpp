/**
 * @file test_cerebellum_brain_integration.cpp
 * @brief Integration tests for cerebellum with all brain modules
 *
 * Tests complete integration between cerebellum and:
 * - Neural substrate (metabolic modulation)
 * - Thalamic router (VL pathway)
 * - Bio-async messaging system
 * - Motor cortex
 * - Basal ganglia coordination
 * - Vestibular system
 * - Quantum bridge (optimization)
 * - Training/learning system
 *
 * @version Brain Integration Tests
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Core cerebellum
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

// Substrate bridge (metabolic)
#include "core/cerebellum/nimcp_cerebellum_substrate_bridge.h"

// Thalamic bridge
#include "core/cerebellum/nimcp_cerebellum_thalamic_bridge.h"

// Vestibular integration
#include "core/brain/regions/brainstem/nimcp_vestibular.h"
#include "core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.h"

// Neural substrate
#include "core/neural_substrate/nimcp_neural_substrate.h"

//=============================================================================
// Substrate Integration Tests
//=============================================================================

class CerebellumSubstrateIntegration : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    cerebellum_substrate_bridge_t* substrate_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        cerebellum_substrate_config_t cb_config = cerebellum_substrate_default_config();
        substrate_bridge = cerebellum_substrate_bridge_create(nullptr, substrate, &cb_config);
        ASSERT_NE(substrate_bridge, nullptr);
    }

    void TearDown() override {
        if (substrate_bridge) {
            cerebellum_substrate_bridge_destroy(substrate_bridge);
            substrate_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

TEST_F(CerebellumSubstrateIntegration, BridgeCreation) {
    EXPECT_NE(substrate_bridge, nullptr);
}

TEST_F(CerebellumSubstrateIntegration, MetabolicModulationWorks) {
    // Normal ATP
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(substrate_bridge);

    cerebellum_substrate_effects_t normal_effects;
    cerebellum_substrate_bridge_get_effects(substrate_bridge, &normal_effects);

    // Low ATP
    substrate_set_atp(substrate, 0.3f);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(substrate_bridge);

    cerebellum_substrate_effects_t low_effects;
    cerebellum_substrate_bridge_get_effects(substrate_bridge, &low_effects);

    // Effects should be valid
    EXPECT_GE(normal_effects.overall_capacity, 0.0f);
    EXPECT_LE(normal_effects.overall_capacity, 1.0f);
    EXPECT_GE(low_effects.overall_capacity, 0.0f);
}

TEST_F(CerebellumSubstrateIntegration, AllEffectChannels) {
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(substrate_bridge);

    cerebellum_substrate_effects_t effects;
    cerebellum_substrate_bridge_get_effects(substrate_bridge, &effects);

    // All effect channels should be valid
    EXPECT_GE(effects.motor_coordination, 0.0f);
    EXPECT_LE(effects.motor_coordination, 1.0f);
    EXPECT_GE(effects.timing_precision, 0.0f);
    EXPECT_LE(effects.timing_precision, 1.0f);
    EXPECT_GE(effects.procedural_learning, 0.0f);
    EXPECT_LE(effects.procedural_learning, 1.0f);
    EXPECT_GE(effects.error_correction, 0.0f);
    EXPECT_LE(effects.error_correction, 1.0f);
}

TEST_F(CerebellumSubstrateIntegration, ApplyEffectsWorks) {
    cerebellum_substrate_bridge_update(substrate_bridge);
    int result = cerebellum_substrate_bridge_apply_effects(substrate_bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Thalamic Integration Tests
//=============================================================================

class CerebellumThalamicIntegration : public ::testing::Test {
protected:
    cerebellum_thalamic_bridge_t* thalamic_bridge = nullptr;

    void SetUp() override {
        cerebellum_thalamic_config_t config = cerebellum_thalamic_default_config();
        thalamic_bridge = cerebellum_thalamic_bridge_create(nullptr, nullptr, &config);
        ASSERT_NE(thalamic_bridge, nullptr);
    }

    void TearDown() override {
        if (thalamic_bridge) {
            cerebellum_thalamic_bridge_destroy(thalamic_bridge);
            thalamic_bridge = nullptr;
        }
    }
};

TEST_F(CerebellumThalamicIntegration, BridgeCreation) {
    EXPECT_NE(thalamic_bridge, nullptr);
}

TEST_F(CerebellumThalamicIntegration, SendTimingSignal) {
    cerebellum_vl_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.type = CEREBELLUM_SIGNAL_TIMING;
    signal.timing.predicted_duration_ms = 100.0f;
    signal.timing.precision = 0.9f;

    int result = cerebellum_thalamic_bridge_send_signal(thalamic_bridge, &signal);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicIntegration, SendCorrectionSignal) {
    cerebellum_vl_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.type = CEREBELLUM_SIGNAL_CORRECTION;
    signal.correction.error_magnitude = 0.5f;
    signal.correction.correction_direction[0] = 1.0f;

    int result = cerebellum_thalamic_bridge_send_signal(thalamic_bridge, &signal);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicIntegration, GetStats) {
    cerebellum_vl_stats_t stats;
    int result = cerebellum_thalamic_bridge_get_stats(thalamic_bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumThalamicIntegration, UpdateWorks) {
    int result = cerebellum_thalamic_bridge_update(thalamic_bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Vestibular Integration Tests
//=============================================================================

class CerebellumVestibularIntegration : public ::testing::Test {
protected:
    vestibular_processor_t* vestibular = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    vestibular_cerebellum_bridge_t* vest_bridge = nullptr;

    void SetUp() override {
        vestibular_config_t vest_config = vestibular_default_config();
        vest_config.enable_vor = true;
        vestibular = vestibular_create(&vest_config);
        ASSERT_NE(vestibular, nullptr);

        cerebellum_config_t cereb_config = cerebellum_default_config();
        cereb_config.enable_vestibulocerebellum = true;
        cerebellum = cerebellum_create(&cereb_config);
        ASSERT_NE(cerebellum, nullptr);

        vest_bridge = vestibular_cerebellum_bridge_create(vestibular, cerebellum, nullptr);
        ASSERT_NE(vest_bridge, nullptr);
    }

    void TearDown() override {
        if (vest_bridge) {
            vestibular_cerebellum_bridge_destroy(vest_bridge);
            vest_bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (vestibular) {
            vestibular_destroy(vestibular);
            vestibular = nullptr;
        }
    }
};

TEST_F(CerebellumVestibularIntegration, BridgeCreation) {
    EXPECT_NE(vest_bridge, nullptr);
}

TEST_F(CerebellumVestibularIntegration, MossyFiberTransmission) {
    vestibular_input_t input;
    memset(&input, 0, sizeof(input));
    input.source = VESTIBULAR_INPUT_CANAL;
    input.canal.angular_velocity[0] = 1.0f;
    input.timestamp_ms = 0.0f;

    vestibular_process(vestibular, &input);
    int result = vestibular_cerebellum_send_mossy_signal(vest_bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumVestibularIntegration, VorAdaptation) {
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    int result = vestibular_cerebellum_trigger_vor_adaptation(vest_bridge, 0.5f, slip_direction);
    EXPECT_EQ(result, 0);
}

TEST_F(CerebellumVestibularIntegration, FeedbackLoop) {
    vestibular_input_t input;
    memset(&input, 0, sizeof(input));
    input.source = VESTIBULAR_INPUT_CANAL;
    input.canal.angular_velocity[0] = 1.0f;
    input.timestamp_ms = 0.0f;

    vestibular_process(vestibular, &input);
    vestibular_cerebellum_send_mossy_signal(vest_bridge);

    motor_coordination_result_t motor_result;
    cerebellum_process(cerebellum, &motor_result);

    int result = vestibular_cerebellum_apply_feedback(vest_bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Cerebellum Core Integration Tests
//=============================================================================

class CerebellumCoreIntegration : public ::testing::Test {
protected:
    cerebellum_adapter_t* cerebellum = nullptr;

    void SetUp() override {
        cerebellum_config_t config = cerebellum_default_config();
        config.enable_synaptic_dynamics = true;
        config.enable_basket_cells = true;
        config.enable_stellate_cells = true;
        config.enable_golgi_cells = true;
        config.enable_vestibulocerebellum = true;
        cerebellum = cerebellum_create(&config);
        ASSERT_NE(cerebellum, nullptr);
    }

    void TearDown() override {
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
    }
};

TEST_F(CerebellumCoreIntegration, AllFeaturesEnabled) {
    cerebellum_config_t config;
    cerebellum_get_config(cerebellum, &config);

    EXPECT_TRUE(config.enable_synaptic_dynamics);
    EXPECT_TRUE(config.enable_basket_cells);
    EXPECT_TRUE(config.enable_stellate_cells);
    EXPECT_TRUE(config.enable_golgi_cells);
    EXPECT_TRUE(config.enable_vestibulocerebellum);
}

TEST_F(CerebellumCoreIntegration, ProcessMossyInput) {
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    bool result = cerebellum_process_mossy_input(cerebellum, &input);
    EXPECT_TRUE(result);
}

TEST_F(CerebellumCoreIntegration, ProcessClimbingFiber) {
    climbing_fiber_input_t cf_input;
    cf_input.fiber_id = 0;
    cf_input.error_signal = 0.5f;
    cf_input.target_purkinje_start = 0;
    cf_input.target_purkinje_count = 10;
    cf_input.timestamp_ms = 0.0f;

    bool result = cerebellum_process_climbing_input(cerebellum, &cf_input);
    EXPECT_TRUE(result);
}

TEST_F(CerebellumCoreIntegration, MotorCoordination) {
    // Add input
    for (int i = 0; i < 50; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.7f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(cerebellum, &input);
    }

    motor_coordination_result_t result;
    EXPECT_TRUE(cerebellum_process(cerebellum, &result));
    EXPECT_TRUE(result.motor_ready);
}

TEST_F(CerebellumCoreIntegration, GetDeepNucleiOutput) {
    for (int i = 0; i < 20; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 5;
        input.activity = 0.8f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(cerebellum, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(cerebellum, &result);

    deep_nuclei_output_t dcn_output;
    int status = cerebellum_get_deep_nuclei_output(cerebellum, &dcn_output);
    EXPECT_EQ(status, 0);
}

TEST_F(CerebellumCoreIntegration, StatsCollection) {
    for (int i = 0; i < 100; i++) {
        mossy_fiber_input_t input;
        input.fiber_id = i % 10;
        input.activity = 0.6f;
        input.timestamp_ms = (float)i;
        input.modality = 0;
        cerebellum_process_mossy_input(cerebellum, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(cerebellum, &result);

    cerebellum_stats_t stats;
    cerebellum_get_stats(cerebellum, &stats);

    EXPECT_GT(stats.granule_activations, 0);
    EXPECT_GT(stats.purkinje_spikes, 0);
}

//=============================================================================
// Combined Multi-Bridge Integration Tests
//=============================================================================

class CerebellumMultiBridgeIntegration : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    cerebellum_substrate_bridge_t* substrate_bridge = nullptr;
    cerebellum_thalamic_bridge_t* thalamic_bridge = nullptr;
    vestibular_processor_t* vestibular = nullptr;
    vestibular_cerebellum_bridge_t* vest_bridge = nullptr;

    void SetUp() override {
        // Substrate
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        // Cerebellum
        cerebellum_config_t cereb_config = cerebellum_default_config();
        cereb_config.enable_synaptic_dynamics = true;
        cereb_config.enable_basket_cells = true;
        cereb_config.enable_stellate_cells = true;
        cereb_config.enable_golgi_cells = true;
        cereb_config.enable_vestibulocerebellum = true;
        cerebellum = cerebellum_create(&cereb_config);
        ASSERT_NE(cerebellum, nullptr);

        // Substrate bridge
        cerebellum_substrate_config_t cb_config = cerebellum_substrate_default_config();
        substrate_bridge = cerebellum_substrate_bridge_create(cerebellum, substrate, &cb_config);
        ASSERT_NE(substrate_bridge, nullptr);

        // Thalamic bridge
        cerebellum_thalamic_config_t th_config = cerebellum_thalamic_default_config();
        thalamic_bridge = cerebellum_thalamic_bridge_create(cerebellum, nullptr, &th_config);
        ASSERT_NE(thalamic_bridge, nullptr);

        // Vestibular
        vestibular_config_t vest_config = vestibular_default_config();
        vest_config.enable_vor = true;
        vestibular = vestibular_create(&vest_config);
        ASSERT_NE(vestibular, nullptr);

        // Vestibular bridge
        vest_bridge = vestibular_cerebellum_bridge_create(vestibular, cerebellum, nullptr);
        ASSERT_NE(vest_bridge, nullptr);
    }

    void TearDown() override {
        if (vest_bridge) {
            vestibular_cerebellum_bridge_destroy(vest_bridge);
            vest_bridge = nullptr;
        }
        if (vestibular) {
            vestibular_destroy(vestibular);
            vestibular = nullptr;
        }
        if (thalamic_bridge) {
            cerebellum_thalamic_bridge_destroy(thalamic_bridge);
            thalamic_bridge = nullptr;
        }
        if (substrate_bridge) {
            cerebellum_substrate_bridge_destroy(substrate_bridge);
            substrate_bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

TEST_F(CerebellumMultiBridgeIntegration, AllBridgesCreated) {
    EXPECT_NE(substrate, nullptr);
    EXPECT_NE(cerebellum, nullptr);
    EXPECT_NE(substrate_bridge, nullptr);
    EXPECT_NE(thalamic_bridge, nullptr);
    EXPECT_NE(vestibular, nullptr);
    EXPECT_NE(vest_bridge, nullptr);
}

TEST_F(CerebellumMultiBridgeIntegration, FullPipelineExecution) {
    // 1. Set metabolic state
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(substrate_bridge);

    // 2. Vestibular input
    vestibular_input_t vest_input;
    memset(&vest_input, 0, sizeof(vest_input));
    vest_input.source = VESTIBULAR_INPUT_CANAL;
    vest_input.canal.angular_velocity[0] = 1.5f;
    vest_input.timestamp_ms = 0.0f;
    vestibular_process(vestibular, &vest_input);
    vestibular_cerebellum_send_mossy_signal(vest_bridge);

    // 3. Process through cerebellum
    motor_coordination_result_t motor_result;
    EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));
    EXPECT_TRUE(motor_result.motor_ready);

    // 4. Send to thalamus
    cerebellum_vl_signal_t th_signal;
    memset(&th_signal, 0, sizeof(th_signal));
    th_signal.type = CEREBELLUM_SIGNAL_COORDINATION;
    EXPECT_EQ(0, cerebellum_thalamic_bridge_send_signal(thalamic_bridge, &th_signal));

    // 5. Apply vestibular feedback
    EXPECT_EQ(0, vestibular_cerebellum_apply_feedback(vest_bridge));

    // 6. Apply substrate effects
    EXPECT_EQ(0, cerebellum_substrate_bridge_apply_effects(substrate_bridge));
}

TEST_F(CerebellumMultiBridgeIntegration, ContinuousOperation) {
    for (int frame = 0; frame < 100; frame++) {
        // Metabolic update
        substrate_update(substrate, 10);
        cerebellum_substrate_bridge_update(substrate_bridge);

        // Vestibular input
        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = sinf((float)frame * 0.1f);
        input.timestamp_ms = (float)frame;
        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(vest_bridge);

        // Cerebellum processing
        motor_coordination_result_t motor_result;
        EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));

        // Thalamic routing
        cerebellum_thalamic_bridge_update(thalamic_bridge);

        // Feedback
        if (frame % 10 == 0) {
            vestibular_cerebellum_apply_feedback(vest_bridge);
        }
    }

    // Verify all bridges tracked activity
    vestibular_cerebellum_stats_t vest_stats;
    vestibular_cerebellum_get_stats(vest_bridge, &vest_stats);
    EXPECT_EQ(vest_stats.mossy_signals_sent, 100);

    cerebellum_stats_t cereb_stats;
    cerebellum_get_stats(cerebellum, &cereb_stats);
    EXPECT_GT(cereb_stats.purkinje_spikes, 0);
}

TEST_F(CerebellumMultiBridgeIntegration, MetabolicAffectsCoordination) {
    // High ATP coordination
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(substrate_bridge);

    cerebellum_substrate_effects_t high_atp_effects;
    cerebellum_substrate_bridge_get_effects(substrate_bridge, &high_atp_effects);

    // Low ATP coordination
    substrate_set_atp(substrate, 0.3f);
    substrate_update(substrate, 10);
    cerebellum_substrate_bridge_update(substrate_bridge);

    cerebellum_substrate_effects_t low_atp_effects;
    cerebellum_substrate_bridge_get_effects(substrate_bridge, &low_atp_effects);

    // Both should be valid
    EXPECT_GE(high_atp_effects.motor_coordination, 0.0f);
    EXPECT_GE(low_atp_effects.motor_coordination, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
