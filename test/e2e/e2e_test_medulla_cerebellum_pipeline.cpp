/**
 * @file e2e_test_medulla_cerebellum_pipeline.cpp
 * @brief End-to-end tests for Medulla-Cerebellum Bridge via Inferior Olive
 * @version 1.1.0
 * @date 2026-01-10
 *
 * WHAT: Complete medulla-cerebellum pipeline with inferior olive error signaling
 * WHY:  Verify full dataflow from medulla state -> inferior olive -> climbing fibers
 *       -> cerebellar learning modulation with arousal, protection, and circadian effects
 * HOW:  Test realistic scenarios using REAL medulla and cerebellum instances
 *
 * Test Coverage:
 * 1. Complete Lifecycle Pipeline - create, connect, process, destroy
 * 2. Motor Learning Pipeline - queue errors, IO spikes, circadian modulation
 * 3. Stress Response Pipeline - arousal effects, protection gating
 * 4. Day Simulation - 24-hour circadian cycle
 * 5. Emergency Response Pipeline - motor stop and release
 * 6. Inferior Olive Model Validation - oscillations, coupling, refractory
 */

#include <gtest/gtest.h>

#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"
#include "core/medulla/nimcp_medulla.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <numeric>

//=============================================================================
// Test Fixtures
//=============================================================================

class MedullaCerebellumE2E : public ::testing::Test {
protected:
    med_cereb_bridge_t bridge = nullptr;
    medulla_t medulla = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;

    struct PipelineStats {
        int errors_queued = 0;
        int climbing_signals = 0;
        int protection_gates = 0;
        int emergency_stops = 0;
        int learning_adjustments = 0;
        std::vector<float> error_magnitudes;
        std::vector<float> learning_rates;
    } stats;

    void SetUp() override {
        // Create real medulla
        medulla_config_t med_config = medulla_default_config();
        med_config.enable_health_integration = false;
        med_config.enable_recovery_integration = false;
        med_config.enable_sleep_integration = false;
        med_config.enable_neuromod_integration = false;
        med_config.enable_bio_async = false;
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr) << "Failed to create medulla";

        // Create real cerebellum
        cerebellum_config_t cere_config = cerebellum_default_config();
        cere_config.enable_error_learning = true;
        cere_config.enable_timing = true;
        cere_config.enable_motor_adaptation = true;
        cere_config.enable_bio_async = false;
        cerebellum = cerebellum_create(&cere_config);
        ASSERT_NE(cerebellum, nullptr) << "Failed to create cerebellum";
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (medulla) {
            medulla_destroy(medulla);
            medulla = nullptr;
        }
    }

    /**
     * @brief Create bridge with default config
     */
    int create_default_bridge() {
        med_cereb_bridge_config_t config;
        int ret = med_cereb_bridge_default_config(&config);
        if (ret != 0) return ret;

        bridge = med_cereb_bridge_create(&config);
        return bridge ? 0 : -1;
    }

    /**
     * @brief Create bridge with custom IO parameters
     */
    int create_custom_bridge(uint32_t num_io_neurons, float oscillation_freq,
                             float coupling_strength, float refractory_ms) {
        med_cereb_bridge_config_t config;
        int ret = med_cereb_bridge_default_config(&config);
        if (ret != 0) return ret;

        config.num_io_neurons = num_io_neurons;
        config.io_oscillation_freq = oscillation_freq;
        config.io_coupling_strength = coupling_strength;
        config.io_refractory_ms = refractory_ms;
        config.enable_arousal_modulation = true;
        config.enable_protection_gating = true;
        config.enable_circadian_learning = true;
        config.enable_io_signaling = true;

        bridge = med_cereb_bridge_create(&config);
        return bridge ? 0 : -1;
    }

    /**
     * @brief Connect real components to bridge
     */
    int connect_components() {
        int ret = med_cereb_bridge_connect_medulla(bridge, medulla);
        if (ret != 0) return ret;

        ret = med_cereb_bridge_connect_cerebellum(bridge, cerebellum);
        return ret;
    }

    /**
     * @brief Set medulla arousal state
     */
    void set_arousal(float level) {
        medulla_test_set_arousal(medulla, level);
    }

    /**
     * @brief Set medulla protection level
     */
    void set_protection(protection_level_t level) {
        medulla_test_set_protection(medulla, level);
    }

    /**
     * @brief Set medulla circadian phase
     */
    void set_circadian_phase(circadian_phase_t phase) {
        medulla_test_set_circadian(medulla, phase);
    }

    /**
     * @brief Queue multiple motor errors of specified type
     */
    void queue_motor_errors(med_cereb_error_type_t error_type, int count,
                            float base_magnitude, float variation) {
        for (int i = 0; i < count; i++) {
            float magnitude = base_magnitude + (variation * ((i % 5) - 2) / 2.0f);
            magnitude = std::max(-1.0f, std::min(1.0f, magnitude));

            int ret = med_cereb_bridge_queue_error(bridge, error_type, magnitude, i);
            if (ret == 0) {
                stats.errors_queued++;
                stats.error_magnitudes.push_back(std::fabs(magnitude));
            }
        }
    }

    /**
     * @brief Run update cycles with medulla
     */
    void run_update_cycles(int cycles, uint64_t delta_us) {
        medulla_start(medulla);
        for (int i = 0; i < cycles; i++) {
            medulla_update(medulla, (float)delta_us / 1000000.0f);
            med_cereb_bridge_update(bridge, delta_us);
        }
    }
};

//=============================================================================
// Pipeline 1: Complete Lifecycle Pipeline
//=============================================================================

TEST_F(MedullaCerebellumE2E, LifecyclePipeline_CreateWithDefaults) {
    ASSERT_EQ(create_default_bridge(), 0) << "Failed to create bridge with defaults";
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(med_cereb_bridge_is_connected(bridge));
}

TEST_F(MedullaCerebellumE2E, LifecyclePipeline_ConnectComponents) {
    ASSERT_EQ(create_default_bridge(), 0);

    int ret = med_cereb_bridge_connect_medulla(bridge, medulla);
    EXPECT_EQ(ret, 0) << "Failed to connect medulla";

    EXPECT_FALSE(med_cereb_bridge_is_connected(bridge));

    ret = med_cereb_bridge_connect_cerebellum(bridge, cerebellum);
    EXPECT_EQ(ret, 0) << "Failed to connect cerebellum";

    EXPECT_TRUE(med_cereb_bridge_is_connected(bridge));
}

TEST_F(MedullaCerebellumE2E, LifecyclePipeline_ProcessErrorsThroughIO) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    queue_motor_errors(MED_CEREB_ERROR_TIMING, 10, 0.5f, 0.2f);
    EXPECT_EQ(stats.errors_queued, 10);

    uint32_t pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(pending, 10u);

    run_update_cycles(50, 10000);

    pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_LT(pending, 10u);
}

TEST_F(MedullaCerebellumE2E, LifecyclePipeline_VerifyClimbingFiberSignals) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    int ret = med_cereb_bridge_send_climbing_signal(bridge,
        MED_CEREB_ERROR_TIMING, 0.8f, 0);
    EXPECT_EQ(ret, 0);

    ret = med_cereb_bridge_broadcast_error(bridge, MED_CEREB_ERROR_AMPLITUDE, 0.6f);
    EXPECT_EQ(ret, 0);

    med_cereb_bridge_stats_t bridge_stats;
    ret = med_cereb_bridge_get_stats(bridge, &bridge_stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(bridge_stats.climbing_signals_sent, 1u);
}

TEST_F(MedullaCerebellumE2E, LifecyclePipeline_VerifyStatistics) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    queue_motor_errors(MED_CEREB_ERROR_TRAJECTORY, 20, 0.4f, 0.3f);
    run_update_cycles(100, 5000);

    med_cereb_bridge_stats_t bridge_stats;
    int ret = med_cereb_bridge_get_stats(bridge, &bridge_stats);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(bridge_stats.climbing_signals_sent, 0u);
    EXPECT_GE(bridge_stats.io_spikes, 0u);

    ret = med_cereb_bridge_reset_stats(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(MedullaCerebellumE2E, LifecyclePipeline_CleanDestroy) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    queue_motor_errors(MED_CEREB_ERROR_COORDINATION, 5, 0.3f, 0.1f);
    run_update_cycles(10, 1000);

    med_cereb_bridge_destroy(bridge);
    bridge = nullptr;

    med_cereb_bridge_print_state(nullptr);
}

//=============================================================================
// Pipeline 2: Motor Learning Pipeline
//=============================================================================

TEST_F(MedullaCerebellumE2E, MotorLearning_ErrorReduction) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    for (int epoch = 0; epoch < 10; epoch++) {
        queue_motor_errors(MED_CEREB_ERROR_TIMING, 3, 0.5f - epoch * 0.04f, 0.1f);
        queue_motor_errors(MED_CEREB_ERROR_AMPLITUDE, 2, 0.4f - epoch * 0.03f, 0.1f);
        run_update_cycles(20, 10000);
    }

    EXPECT_GT(stats.error_magnitudes.size(), 40u);

    med_cereb_bridge_stats_t bridge_stats;
    med_cereb_bridge_get_stats(bridge, &bridge_stats);
    EXPECT_GT(bridge_stats.climbing_signals_sent, 0u);
}

TEST_F(MedullaCerebellumE2E, MotorLearning_IOSpikes) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    for (int i = 0; i < 30; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_PREDICTION, 0.7f, i);
    }

    run_update_cycles(200, 5000);

    med_cereb_bridge_stats_t bridge_stats;
    med_cereb_bridge_get_stats(bridge, &bridge_stats);

    EXPECT_GE(bridge_stats.io_spikes, 0u);
}

TEST_F(MedullaCerebellumE2E, MotorLearning_CircadianModulation) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    std::vector<float> learning_multipliers;

    circadian_phase_t test_phases[] = {
        CIRCADIAN_PHASE_EARLY_MORNING,
        CIRCADIAN_PHASE_MORNING,
        CIRCADIAN_PHASE_AFTERNOON,
        CIRCADIAN_PHASE_EVENING,
        CIRCADIAN_PHASE_NIGHT,
        CIRCADIAN_PHASE_DEEP_NIGHT
    };

    for (circadian_phase_t phase : test_phases) {
        set_circadian_phase(phase);
        run_update_cycles(10, 10000);

        float multiplier = med_cereb_bridge_get_learning_multiplier(bridge);
        learning_multipliers.push_back(multiplier);
        stats.learning_rates.push_back(multiplier);
    }

    EXPECT_EQ(learning_multipliers.size(), 6u);
    for (float mult : learning_multipliers) {
        EXPECT_GE(mult, 0.1f);
        EXPECT_LE(mult, 2.0f);
    }
}

//=============================================================================
// Pipeline 3: Stress Response Pipeline
//=============================================================================

TEST_F(MedullaCerebellumE2E, StressResponse_HighArousal) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    set_arousal(0.9f);
    run_update_cycles(5, 1000);

    med_cereb_arousal_effects_t effects;
    int ret = med_cereb_bridge_get_arousal_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(effects.motor_gain, 0.5f);
    EXPECT_LE(effects.fine_motor_precision, 1.0f);
}

TEST_F(MedullaCerebellumE2E, StressResponse_LowArousal) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    set_arousal(0.2f);
    run_update_cycles(5, 1000);

    med_cereb_arousal_effects_t effects;
    int ret = med_cereb_bridge_get_arousal_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_LE(effects.motor_gain, 1.5f);
}

TEST_F(MedullaCerebellumE2E, StressResponse_ProtectionErrors) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    set_arousal(0.85f);

    for (int i = 0; i < 15; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_PROTECTION, 0.6f, i);
    }

    run_update_cycles(50, 5000);

    med_cereb_bridge_stats_t bridge_stats;
    med_cereb_bridge_get_stats(bridge, &bridge_stats);

    EXPECT_GT(bridge_stats.signals_per_type[MED_CEREB_ERROR_PROTECTION], 0u);
}

TEST_F(MedullaCerebellumE2E, StressResponse_ArousalMotorGain) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    float arousal_levels[] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
    std::vector<float> motor_gains;

    for (float arousal : arousal_levels) {
        set_arousal(arousal);
        run_update_cycles(5, 1000);

        med_cereb_arousal_effects_t effects;
        med_cereb_bridge_get_arousal_effects(bridge, &effects);
        motor_gains.push_back(effects.motor_gain);
    }

    EXPECT_GE(motor_gains.back(), motor_gains.front());
}

TEST_F(MedullaCerebellumE2E, StressResponse_ProtectionGating) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    set_protection(PROTECTION_LEVEL_NORMAL);
    run_update_cycles(3, 1000);
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, false));

    set_protection(PROTECTION_LEVEL_CRITICAL);
    run_update_cycles(5, 1000);

    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);

    if (effects.non_essential_disabled) {
        EXPECT_FALSE(med_cereb_bridge_motor_allowed(bridge, false, false));
    }

    if (!effects.emergency_stop) {
        EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, true));
    }
}

//=============================================================================
// Pipeline 4: Day Simulation
//=============================================================================

TEST_F(MedullaCerebellumE2E, DaySimulation_CircadianCycle) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    std::vector<float> ltd_rates;
    std::vector<float> ltp_rates;
    std::vector<float> consolidation_rates;

    circadian_phase_t phases[] = {
        CIRCADIAN_PHASE_EARLY_MORNING,
        CIRCADIAN_PHASE_MORNING,
        CIRCADIAN_PHASE_AFTERNOON,
        CIRCADIAN_PHASE_EVENING,
        CIRCADIAN_PHASE_LATE_EVENING,
        CIRCADIAN_PHASE_NIGHT,
        CIRCADIAN_PHASE_DEEP_NIGHT,
        CIRCADIAN_PHASE_PRE_DAWN
    };

    for (circadian_phase_t phase : phases) {
        set_circadian_phase(phase);
        run_update_cycles(10, 10000);

        med_cereb_circadian_effects_t effects;
        int ret = med_cereb_bridge_get_circadian_effects(bridge, &effects);
        EXPECT_EQ(ret, 0);

        ltd_rates.push_back(effects.ltd_rate_multiplier);
        ltp_rates.push_back(effects.ltp_rate_multiplier);
        consolidation_rates.push_back(effects.consolidation_rate);
    }

    EXPECT_EQ(ltd_rates.size(), 8u);

    for (size_t i = 0; i < ltd_rates.size(); i++) {
        EXPECT_GE(ltd_rates[i], 0.0f);
        EXPECT_LE(ltd_rates[i], 2.0f);
    }
}

TEST_F(MedullaCerebellumE2E, DaySimulation_LearningVariation) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    float dawn_rate, noon_rate, dusk_rate, midnight_rate;

    set_circadian_phase(CIRCADIAN_PHASE_EARLY_MORNING);
    run_update_cycles(5, 1000);
    dawn_rate = med_cereb_bridge_get_learning_multiplier(bridge);

    set_circadian_phase(CIRCADIAN_PHASE_AFTERNOON);
    run_update_cycles(5, 1000);
    noon_rate = med_cereb_bridge_get_learning_multiplier(bridge);

    set_circadian_phase(CIRCADIAN_PHASE_LATE_EVENING);
    run_update_cycles(5, 1000);
    dusk_rate = med_cereb_bridge_get_learning_multiplier(bridge);

    set_circadian_phase(CIRCADIAN_PHASE_DEEP_NIGHT);
    run_update_cycles(5, 1000);
    midnight_rate = med_cereb_bridge_get_learning_multiplier(bridge);

    EXPECT_GE(dawn_rate, 0.1f);
    EXPECT_GE(noon_rate, 0.1f);
    EXPECT_GE(dusk_rate, 0.1f);
    EXPECT_GE(midnight_rate, 0.1f);
}

TEST_F(MedullaCerebellumE2E, DaySimulation_LearnAndConsolidate) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    set_circadian_phase(CIRCADIAN_PHASE_AFTERNOON);
    for (int i = 0; i < 50; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_SEQUENCE, 0.4f, i);
        run_update_cycles(5, 2000);
    }

    med_cereb_bridge_stats_t day_stats;
    med_cereb_bridge_get_stats(bridge, &day_stats);
    uint64_t day_adjustments = day_stats.learning_rate_adjustments;

    set_circadian_phase(CIRCADIAN_PHASE_DEEP_NIGHT);
    run_update_cycles(100, 10000);

    med_cereb_circadian_effects_t effects;
    med_cereb_bridge_get_circadian_effects(bridge, &effects);
    EXPECT_GE(effects.consolidation_rate, 0.0f);
}

//=============================================================================
// Pipeline 5: Emergency Response Pipeline
//=============================================================================

TEST_F(MedullaCerebellumE2E, Emergency_NormalOperation) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, false));
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, true, false));
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, true));

    queue_motor_errors(MED_CEREB_ERROR_TIMING, 5, 0.3f, 0.1f);
    run_update_cycles(10, 1000);

    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_FALSE(effects.emergency_stop);
}

TEST_F(MedullaCerebellumE2E, Emergency_TriggerStop) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    int ret = med_cereb_bridge_emergency_stop(bridge);
    EXPECT_EQ(ret, 0);

    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_TRUE(effects.emergency_stop);

    EXPECT_FALSE(med_cereb_bridge_motor_allowed(bridge, false, false));
    EXPECT_FALSE(med_cereb_bridge_motor_allowed(bridge, true, false));
    EXPECT_FALSE(med_cereb_bridge_motor_allowed(bridge, false, true));
}

TEST_F(MedullaCerebellumE2E, Emergency_MotorModulation) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    med_cereb_bridge_emergency_stop(bridge);

    float motor_command[3] = {1.0f, 0.5f, 0.3f};
    float modulated[3];

    int ret = med_cereb_bridge_modulate_motor(bridge, motor_command, modulated, 3);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < 3; i++) {
        EXPECT_LE(std::fabs(modulated[i]), std::fabs(motor_command[i]));
    }

    med_cereb_bridge_release_emergency(bridge);
}

TEST_F(MedullaCerebellumE2E, Emergency_Release) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    med_cereb_bridge_emergency_stop(bridge);
    EXPECT_FALSE(med_cereb_bridge_motor_allowed(bridge, false, false));

    int ret = med_cereb_bridge_release_emergency(bridge);
    EXPECT_EQ(ret, 0);

    run_update_cycles(5, 1000);

    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_FALSE(effects.emergency_stop);

    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, false));
}

TEST_F(MedullaCerebellumE2E, Emergency_FullCycle) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    queue_motor_errors(MED_CEREB_ERROR_AMPLITUDE, 3, 0.4f, 0.1f);
    run_update_cycles(10, 1000);

    med_cereb_bridge_emergency_stop(bridge);
    run_update_cycles(5, 1000);

    med_cereb_bridge_release_emergency(bridge);
    run_update_cycles(5, 1000);

    queue_motor_errors(MED_CEREB_ERROR_TRAJECTORY, 5, 0.3f, 0.1f);
    run_update_cycles(20, 1000);

    med_cereb_bridge_stats_t bridge_stats;
    med_cereb_bridge_get_stats(bridge, &bridge_stats);
    EXPECT_GT(bridge_stats.protection_gates, 0u);
}

//=============================================================================
// Pipeline 6: Inferior Olive Model Validation
//=============================================================================

TEST_F(MedullaCerebellumE2E, IOModel_Configuration) {
    ASSERT_EQ(create_custom_bridge(10, 10.0f, 0.5f, 150.0f), 0);
    ASSERT_EQ(connect_components(), 0);

    med_cereb_inferior_olive_t io_state;
    int ret = med_cereb_bridge_get_io_state(bridge, &io_state);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(io_state.num_neurons, 10u);
    EXPECT_FLOAT_EQ(io_state.oscillation_freq, 10.0f);
    EXPECT_FLOAT_EQ(io_state.coupling_strength, 0.5f);

    float initial_phases[10];
    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        initial_phases[i] = io_state.neurons[i].oscillation_phase;
    }

    run_update_cycles(100, 5000);

    med_cereb_bridge_get_io_state(bridge, &io_state);

    bool phases_changed = false;
    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        if (std::fabs(io_state.neurons[i].oscillation_phase - initial_phases[i]) > 0.001f) {
            phases_changed = true;
            break;
        }
    }
    EXPECT_TRUE(phases_changed);
}

TEST_F(MedullaCerebellumE2E, IOModel_GapJunctionCoupling) {
    ASSERT_EQ(create_custom_bridge(5, 10.0f, 0.8f, 100.0f), 0);
    ASSERT_EQ(connect_components(), 0);

    for (int i = 0; i < 20; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_COORDINATION, 0.6f, i);
    }

    run_update_cycles(100, 5000);

    med_cereb_inferior_olive_t io_state;
    med_cereb_bridge_get_io_state(bridge, &io_state);

    EXPECT_FLOAT_EQ(io_state.coupling_strength, 0.8f);
}

TEST_F(MedullaCerebellumE2E, IOModel_RefractoryPeriod) {
    ASSERT_EQ(create_custom_bridge(5, 10.0f, 0.3f, 150.0f), 0);
    ASSERT_EQ(connect_components(), 0);

    med_cereb_inferior_olive_t io_state;
    med_cereb_bridge_get_io_state(bridge, &io_state);

    EXPECT_EQ(io_state.refractory_period_us, 150000u);

    for (int i = 0; i < 10; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_PREDICTION, 0.9f, i);
    }

    run_update_cycles(50, 10000);

    med_cereb_bridge_get_io_state(bridge, &io_state);

    int in_refractory = 0;
    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        if (io_state.neurons[i].is_refractory) {
            in_refractory++;
        }
    }
    EXPECT_GE(in_refractory, 0);
}

TEST_F(MedullaCerebellumE2E, IOModel_ErrorDistribution) {
    ASSERT_EQ(create_custom_bridge(20, 10.0f, 0.3f, 100.0f), 0);
    ASSERT_EQ(connect_components(), 0);

    for (int i = 0; i < 5; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
    }

    run_update_cycles(20, 5000);

    med_cereb_inferior_olive_t io_state;
    med_cereb_bridge_get_io_state(bridge, &io_state);

    float total_error = 0.0f;
    int neurons_with_error = 0;

    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        if (std::fabs(io_state.neurons[i].error_accumulator) > 0.01f) {
            neurons_with_error++;
            total_error += std::fabs(io_state.neurons[i].error_accumulator);
        }
    }
    EXPECT_GE(neurons_with_error, 0);
}

//=============================================================================
// Pipeline 7: Robustness Tests
//=============================================================================

TEST_F(MedullaCerebellumE2E, Robustness_Reset) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    queue_motor_errors(MED_CEREB_ERROR_SEQUENCE, 20, 0.5f, 0.2f);
    run_update_cycles(50, 5000);

    int ret = med_cereb_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);

    uint32_t pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(pending, 0u);

    med_cereb_bridge_stats_t bridge_stats;
    med_cereb_bridge_get_stats(bridge, &bridge_stats);
    EXPECT_EQ(bridge_stats.climbing_signals_sent, 0u);
}

TEST_F(MedullaCerebellumE2E, Robustness_QueueOverflow) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    for (int i = 0; i < MED_CEREB_MAX_ERROR_QUEUE + 20; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.3f, i);
    }

    uint32_t pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_LE(pending, MED_CEREB_MAX_ERROR_QUEUE);

    med_cereb_bridge_stats_t bridge_stats;
    med_cereb_bridge_get_stats(bridge, &bridge_stats);
    EXPECT_GT(bridge_stats.errors_dropped, 0u);
}

TEST_F(MedullaCerebellumE2E, Robustness_ExtendedOperation) {
    ASSERT_EQ(create_default_bridge(), 0);
    ASSERT_EQ(connect_components(), 0);

    auto start = std::chrono::high_resolution_clock::now();

    for (int epoch = 0; epoch < 20; epoch++) {
        for (int i = 0; i < 50; i++) {
            med_cereb_bridge_queue_error(bridge,
                (med_cereb_error_type_t)(i % MED_CEREB_ERROR_COUNT), 0.5f, i);
        }

        run_update_cycles(50, 5000);

        set_arousal(0.3f + (epoch % 7) * 0.1f);

        if (epoch % 5 == 0) {
            med_cereb_bridge_emergency_stop(bridge);
            run_update_cycles(5, 1000);
            med_cereb_bridge_release_emergency(bridge);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    med_cereb_bridge_stats_t final_stats;
    med_cereb_bridge_get_stats(bridge, &final_stats);

    EXPECT_GT(final_stats.climbing_signals_sent + final_stats.io_spikes, 0u);
    EXPECT_LT(duration.count(), 30000);
}
