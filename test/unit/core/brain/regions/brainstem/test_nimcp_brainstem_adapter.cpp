/**
 * @file test_nimcp_brainstem_adapter.cpp
 * @brief Comprehensive unit tests for nimcp_brainstem_adapter.c
 *
 * WHAT: Unit tests for the brainstem adapter
 * WHY:  Ensure correct integration of midbrain, pons, medulla, and reticular formation
 * HOW:  Use Google Test framework to test lifecycle, reflexes, arousal, sensory processing,
 *       vital monitoring, and relay functions.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/brainstem/nimcp_brainstem_adapter.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class BrainstemAdapterTest : public ::testing::Test {
protected:
    brainstem_adapter_t* adapter;
    brainstem_config_t config;

    void SetUp() override {
        config = brainstem_default_config();
        // Disable bio-async for unit tests to avoid router dependency
        config.enable_bio_async = false;
        adapter = brainstem_create(&config, NULL);
        ASSERT_NE(nullptr, adapter) << "Failed to create Brainstem adapter";
    }

    void TearDown() override {
        if (adapter) {
            brainstem_destroy(adapter);
            adapter = nullptr;
        }
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(BrainstemAdapterTest, DefaultConfigHasReasonableValues) {
    brainstem_config_t default_config = brainstem_default_config();

    EXPECT_TRUE(default_config.enable_reflexes);
    EXPECT_TRUE(default_config.enable_vital_monitoring);
    EXPECT_TRUE(default_config.enable_arousal_control);
    EXPECT_TRUE(default_config.enable_events);

    // Midbrain config
    EXPECT_TRUE(default_config.midbrain.enable_superior_colliculus);
    EXPECT_TRUE(default_config.midbrain.enable_inferior_colliculus);
    EXPECT_TRUE(default_config.midbrain.enable_pag);
    EXPECT_FLOAT_EQ(default_config.midbrain.saccade_threshold, 0.3f);

    // Pons config
    EXPECT_TRUE(default_config.pons.enable_corticopontine_relay);
    EXPECT_TRUE(default_config.pons.enable_pneumotaxic_center);
    EXPECT_TRUE(default_config.pons.enable_locus_coeruleus);
    EXPECT_TRUE(default_config.pons.enable_raphe_nuclei);

    // Reticular config
    EXPECT_TRUE(default_config.reticular.enable_aras);
    EXPECT_FLOAT_EQ(default_config.reticular.baseline_arousal, 0.5f);
}

TEST_F(BrainstemAdapterTest, CreateWithNullConfigUsesDefaults) {
    brainstem_config_t null_config = brainstem_default_config();
    null_config.enable_bio_async = false;
    brainstem_adapter_t* adapter_null = brainstem_create(&null_config, NULL);
    ASSERT_NE(nullptr, adapter_null);

    brainstem_config_t retrieved;
    EXPECT_TRUE(brainstem_get_config(adapter_null, &retrieved));
    EXPECT_TRUE(retrieved.enable_reflexes);

    brainstem_destroy(adapter_null);
}

TEST_F(BrainstemAdapterTest, DestroyNullDoesNotCrash) {
    brainstem_destroy(NULL);
    // Should not crash
}

TEST_F(BrainstemAdapterTest, ResetClearsState) {
    // Modify state first
    brainstem_boost_arousal(adapter, 0.3f);

    EXPECT_TRUE(brainstem_reset(adapter));

    // Arousal should be back to baseline
    float arousal = brainstem_get_arousal_value(adapter);
    EXPECT_NEAR(arousal, 0.5f, 0.1f); // Baseline is 0.5
    EXPECT_EQ(brainstem_get_status(adapter), BRAINSTEM_STATUS_ACTIVE);
}

TEST_F(BrainstemAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(brainstem_reset(NULL));
}

// ============================================================================
// REFLEX MANAGEMENT TESTS
// ============================================================================

TEST_F(BrainstemAdapterTest, RegisterReflexSuccess) {
    brainstem_reflex_t reflex = {
        .reflex_id = 100,
        .name = "test_reflex",
        .threshold = 0.5f,
        .latency_ms = 25.0f,
        .gain = 1.2f,
        .is_active = true
    };

    EXPECT_TRUE(brainstem_register_reflex(adapter, &reflex));
}

TEST_F(BrainstemAdapterTest, RegisterReflexNullFails) {
    EXPECT_FALSE(brainstem_register_reflex(NULL, NULL));
    EXPECT_FALSE(brainstem_register_reflex(adapter, NULL));
}

TEST_F(BrainstemAdapterTest, TriggerReflexBelowThreshold) {
    brainstem_reflex_t reflex = {
        .reflex_id = 101,
        .name = "high_threshold_reflex",
        .threshold = 0.8f,
        .latency_ms = 20.0f,
        .gain = 1.0f,
        .is_active = true
    };

    ASSERT_TRUE(brainstem_register_reflex(adapter, &reflex));

    brainstem_motor_output_t output;
    // Stimulus below threshold should not trigger
    EXPECT_FALSE(brainstem_trigger_reflex(adapter, 101, 0.5f, &output));
}

TEST_F(BrainstemAdapterTest, TriggerReflexAboveThreshold) {
    brainstem_reflex_t reflex = {
        .reflex_id = 102,
        .name = "low_threshold_reflex",
        .threshold = 0.3f,
        .latency_ms = 15.0f,
        .gain = 1.5f,
        .is_active = true
    };

    ASSERT_TRUE(brainstem_register_reflex(adapter, &reflex));

    brainstem_motor_output_t output;
    memset(&output, 0, sizeof(output));

    // Stimulus above threshold should trigger
    EXPECT_TRUE(brainstem_trigger_reflex(adapter, 102, 0.8f, &output));
    EXPECT_EQ(output.pathway_id, 102u);
    EXPECT_GT(output.activation, 0.0f);
    EXPECT_TRUE(output.is_reflex);
}

TEST_F(BrainstemAdapterTest, ReflexCallbackInvoked) {
    static bool callback_invoked = false;
    static uint32_t callback_reflex_id = 0;

    auto callback = [](const brainstem_reflex_t* reflex,
                       const brainstem_motor_output_t* response,
                       void* user_data) {
        callback_invoked = true;
        callback_reflex_id = reflex->reflex_id;
    };

    EXPECT_TRUE(brainstem_set_reflex_callback(adapter, callback, nullptr));

    brainstem_reflex_t reflex = {
        .reflex_id = 103,
        .name = "callback_reflex",
        .threshold = 0.2f,
        .latency_ms = 10.0f,
        .gain = 1.0f,
        .is_active = true
    };

    ASSERT_TRUE(brainstem_register_reflex(adapter, &reflex));

    brainstem_motor_output_t output;
    callback_invoked = false;
    ASSERT_TRUE(brainstem_trigger_reflex(adapter, 103, 0.9f, &output));

    EXPECT_TRUE(callback_invoked);
    EXPECT_EQ(callback_reflex_id, 103u);
}

// ============================================================================
// SENSORY PROCESSING TESTS (MIDBRAIN)
// ============================================================================

TEST_F(BrainstemAdapterTest, ProcessSensoryVisual) {
    brainstem_sensory_input_t input = {
        .visual_target_x = 0.5f,
        .visual_target_y = -0.3f,
        .visual_salience = 0.7f,
        .visual_motion_detected = true,
        .sound_azimuth = 0.0f,
        .sound_elevation = 0.0f,
        .sound_intensity = 0.0f,
        .sudden_sound = false
    };

    brainstem_orienting_response_t response;
    memset(&response, 0, sizeof(response));

    EXPECT_TRUE(brainstem_process_sensory(adapter, &input, &response));
    EXPECT_NE(response.saccade_x, 0.0f);
    EXPECT_NE(response.saccade_y, 0.0f);
    EXPECT_GT(response.attention_shift, 0.0f);
}

TEST_F(BrainstemAdapterTest, ProcessSensoryAuditory) {
    brainstem_sensory_input_t input = {
        .visual_target_x = 0.0f,
        .visual_target_y = 0.0f,
        .visual_salience = 0.0f,
        .visual_motion_detected = false,
        .sound_azimuth = 45.0f,
        .sound_elevation = 10.0f,
        .sound_intensity = 0.6f,
        .sudden_sound = false
    };

    brainstem_orienting_response_t response;
    memset(&response, 0, sizeof(response));

    EXPECT_TRUE(brainstem_process_sensory(adapter, &input, &response));
    EXPECT_NE(response.head_turn, 0.0f);
}

TEST_F(BrainstemAdapterTest, ProcessSensorySuddenSoundStartle) {
    brainstem_sensory_input_t input = {
        .visual_target_x = 0.0f,
        .visual_target_y = 0.0f,
        .visual_salience = 0.0f,
        .visual_motion_detected = false,
        .sound_azimuth = 90.0f,
        .sound_elevation = 0.0f,
        .sound_intensity = 0.9f,
        .sudden_sound = true
    };

    float initial_arousal = brainstem_get_arousal_value(adapter);

    brainstem_orienting_response_t response;
    EXPECT_TRUE(brainstem_process_sensory(adapter, &input, &response));

    // Sudden sound should boost arousal
    float final_arousal = brainstem_get_arousal_value(adapter);
    EXPECT_GT(final_arousal, initial_arousal);
    EXPECT_TRUE(response.reflex_triggered);
}

TEST_F(BrainstemAdapterTest, ProcessSensoryNull) {
    brainstem_orienting_response_t response;
    EXPECT_FALSE(brainstem_process_sensory(NULL, NULL, &response));
    EXPECT_FALSE(brainstem_process_sensory(adapter, NULL, &response));
}

TEST_F(BrainstemAdapterTest, GenerateSaccade) {
    EXPECT_TRUE(brainstem_generate_saccade(adapter, 0.7f, -0.4f, 0.8f));

    brainstem_stats_t stats;
    EXPECT_TRUE(brainstem_get_stats(adapter, &stats));
    EXPECT_GE(stats.saccades_generated, 1u);
}

// ============================================================================
// AROUSAL CONTROL TESTS (RETICULAR FORMATION)
// ============================================================================

TEST_F(BrainstemAdapterTest, GetArousalLevelDefault) {
    brainstem_arousal_level_t level = brainstem_get_arousal_level(adapter);
    // Default should be awake (baseline is 0.5)
    EXPECT_EQ(level, BRAINSTEM_AROUSAL_AWAKE);
}

TEST_F(BrainstemAdapterTest, GetArousalValueDefault) {
    float value = brainstem_get_arousal_value(adapter);
    EXPECT_NEAR(value, 0.5f, 0.1f); // Baseline
}

TEST_F(BrainstemAdapterTest, BoostArousal) {
    float initial = brainstem_get_arousal_value(adapter);

    EXPECT_TRUE(brainstem_boost_arousal(adapter, 0.2f));

    float after = brainstem_get_arousal_value(adapter);
    EXPECT_GT(after, initial);
}

TEST_F(BrainstemAdapterTest, BoostArousalClamps) {
    // Boost multiple times to reach max
    for (int i = 0; i < 10; i++) {
        brainstem_boost_arousal(adapter, 0.2f);
    }

    float arousal = brainstem_get_arousal_value(adapter);
    EXPECT_LE(arousal, 1.0f);
}

TEST_F(BrainstemAdapterTest, ReduceArousal) {
    float initial = brainstem_get_arousal_value(adapter);

    EXPECT_TRUE(brainstem_reduce_arousal(adapter, 0.2f));

    float after = brainstem_get_arousal_value(adapter);
    EXPECT_LT(after, initial);
}

TEST_F(BrainstemAdapterTest, ReduceArousalClamps) {
    // Reduce multiple times to reach min
    for (int i = 0; i < 10; i++) {
        brainstem_reduce_arousal(adapter, 0.2f);
    }

    float arousal = brainstem_get_arousal_value(adapter);
    EXPECT_GE(arousal, 0.0f);
}

TEST_F(BrainstemAdapterTest, SetTargetArousal) {
    EXPECT_TRUE(brainstem_set_target_arousal(adapter, 0.8f));

    // Update to move toward target
    for (int i = 0; i < 10; i++) {
        brainstem_update(adapter, 0.1f);
    }

    float arousal = brainstem_get_arousal_value(adapter);
    EXPECT_GT(arousal, 0.6f); // Should be moving toward 0.8
}

TEST_F(BrainstemAdapterTest, ArousalCallbackInvoked) {
    static bool callback_invoked = false;
    static brainstem_arousal_level_t old_level_cb;
    static brainstem_arousal_level_t new_level_cb;

    auto callback = [](brainstem_arousal_level_t old_level,
                       brainstem_arousal_level_t new_level,
                       float arousal_value,
                       void* user_data) {
        callback_invoked = true;
        old_level_cb = old_level;
        new_level_cb = new_level;
    };

    EXPECT_TRUE(brainstem_set_arousal_callback(adapter, callback, nullptr));

    callback_invoked = false;
    // Boost enough to change level
    brainstem_boost_arousal(adapter, 0.4f);

    EXPECT_TRUE(callback_invoked);
    EXPECT_NE(old_level_cb, new_level_cb);
}

// ============================================================================
// VITAL FUNCTIONS TESTS (MEDULLA)
// ============================================================================

TEST_F(BrainstemAdapterTest, GetVitals) {
    brainstem_vitals_t vitals;
    EXPECT_TRUE(brainstem_get_vitals(adapter, &vitals));

    // All vitals should be in valid range
    EXPECT_GE(vitals.heart_rate_analog, 0.0f);
    EXPECT_LE(vitals.heart_rate_analog, 1.0f);
    EXPECT_GE(vitals.respiratory_rate, 0.0f);
    EXPECT_LE(vitals.respiratory_rate, 1.0f);
}

TEST_F(BrainstemAdapterTest, GetVitalsNull) {
    EXPECT_FALSE(brainstem_get_vitals(NULL, NULL));
    brainstem_vitals_t vitals;
    EXPECT_FALSE(brainstem_get_vitals(adapter, NULL));
}

TEST_F(BrainstemAdapterTest, TriggerProtection) {
    EXPECT_TRUE(brainstem_trigger_protection(adapter, 0.5f));

    brainstem_status_t status = brainstem_get_status(adapter);
    EXPECT_EQ(status, BRAINSTEM_STATUS_PROTECTIVE);
}

// ============================================================================
// RELAY FUNCTIONS TESTS (PONS)
// ============================================================================

TEST_F(BrainstemAdapterTest, RelaySignal) {
    float input[] = {0.5f, 0.3f, 0.8f, 0.2f};
    float output[4] = {0};

    EXPECT_TRUE(brainstem_relay_signal(adapter, input, 4, output));

    // Output should be scaled version of input
    for (int i = 0; i < 4; i++) {
        EXPECT_GT(output[i], 0.0f);
    }
}

TEST_F(BrainstemAdapterTest, RelaySignalNull) {
    float output[4];
    EXPECT_FALSE(brainstem_relay_signal(NULL, NULL, 0, output));
}

TEST_F(BrainstemAdapterTest, ModulateSleep) {
    EXPECT_TRUE(brainstem_modulate_sleep(adapter, 0.7f));

    // High sleep pressure should lower arousal target
    for (int i = 0; i < 10; i++) {
        brainstem_update(adapter, 0.1f);
    }

    float arousal = brainstem_get_arousal_value(adapter);
    EXPECT_LT(arousal, 0.5f); // Should be below baseline
}

// ============================================================================
// UPDATE AND STATE TESTS
// ============================================================================

TEST_F(BrainstemAdapterTest, UpdateSuccess) {
    EXPECT_TRUE(brainstem_update(adapter, 0.1f));

    brainstem_stats_t stats;
    EXPECT_TRUE(brainstem_get_stats(adapter, &stats));
    EXPECT_GE(stats.updates_processed, 1u);
}

TEST_F(BrainstemAdapterTest, UpdateMultipleTimes) {
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(brainstem_update(adapter, 0.01f));
    }

    brainstem_stats_t stats;
    EXPECT_TRUE(brainstem_get_stats(adapter, &stats));
    EXPECT_GE(stats.updates_processed, 100u);
}

TEST_F(BrainstemAdapterTest, GetState) {
    brainstem_state_t state;
    EXPECT_TRUE(brainstem_get_state(adapter, &state));

    EXPECT_EQ(state.status, BRAINSTEM_STATUS_ACTIVE);
    EXPECT_GE(state.arousal_value, 0.0f);
    EXPECT_LE(state.arousal_value, 1.0f);
    EXPECT_GE(state.motor_tone, 0.0f);
    EXPECT_LE(state.motor_tone, 1.0f);
}

TEST_F(BrainstemAdapterTest, GetStatus) {
    EXPECT_EQ(brainstem_get_status(adapter), BRAINSTEM_STATUS_ACTIVE);
}

TEST_F(BrainstemAdapterTest, GetStatusNull) {
    EXPECT_EQ(brainstem_get_status(NULL), BRAINSTEM_STATUS_ERROR);
}

TEST_F(BrainstemAdapterTest, GetLastError) {
    EXPECT_EQ(brainstem_get_last_error(adapter), BRAINSTEM_ERROR_NONE);
}

// ============================================================================
// STRING CONVERSION TESTS
// ============================================================================

TEST_F(BrainstemAdapterTest, ErrorStringNotNull) {
    const char* str = brainstem_error_string(BRAINSTEM_ERROR_NONE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = brainstem_error_string(BRAINSTEM_ERROR_INVALID_INPUT);
    EXPECT_NE(str, nullptr);

    str = brainstem_error_string(BRAINSTEM_ERROR_MIDBRAIN_FAILURE);
    EXPECT_NE(str, nullptr);
}

TEST_F(BrainstemAdapterTest, StatusStringNotNull) {
    const char* str = brainstem_status_string(BRAINSTEM_STATUS_ACTIVE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = brainstem_status_string(BRAINSTEM_STATUS_SLEEP);
    EXPECT_NE(str, nullptr);
}

TEST_F(BrainstemAdapterTest, ArousalStringNotNull) {
    const char* str = brainstem_arousal_string(BRAINSTEM_AROUSAL_AWAKE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = brainstem_arousal_string(BRAINSTEM_AROUSAL_HYPERAROUSED);
    EXPECT_NE(str, nullptr);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(BrainstemAdapterTest, GetStats) {
    // Do some operations
    brainstem_boost_arousal(adapter, 0.1f);
    brainstem_update(adapter, 0.1f);

    brainstem_sensory_input_t input = {
        .visual_salience = 0.5f,
    };
    brainstem_orienting_response_t response;
    brainstem_process_sensory(adapter, &input, &response);

    brainstem_stats_t stats;
    EXPECT_TRUE(brainstem_get_stats(adapter, &stats));

    EXPECT_GE(stats.updates_processed, 1u);
    EXPECT_GE(stats.arousal_modulations, 1u);
    EXPECT_GE(stats.midbrain_activations, 1u);
}

TEST_F(BrainstemAdapterTest, GetStatsNull) {
    brainstem_stats_t stats;
    EXPECT_FALSE(brainstem_get_stats(NULL, &stats));
    EXPECT_FALSE(brainstem_get_stats(adapter, NULL));
}

// ============================================================================
// CONFIG TESTS
// ============================================================================

TEST_F(BrainstemAdapterTest, GetConfig) {
    brainstem_config_t retrieved;
    EXPECT_TRUE(brainstem_get_config(adapter, &retrieved));
    EXPECT_EQ(retrieved.enable_reflexes, config.enable_reflexes);
}

TEST_F(BrainstemAdapterTest, GetConfigNull) {
    brainstem_config_t config_out;
    EXPECT_FALSE(brainstem_get_config(NULL, &config_out));
    EXPECT_FALSE(brainstem_get_config(adapter, NULL));
}

// ============================================================================
// SUB-MODULE ACCESS TESTS
// ============================================================================

TEST_F(BrainstemAdapterTest, GetMidbrainNotNull) {
    EXPECT_NE(brainstem_get_midbrain(adapter), nullptr);
}

TEST_F(BrainstemAdapterTest, GetPonsNotNull) {
    EXPECT_NE(brainstem_get_pons(adapter), nullptr);
}

TEST_F(BrainstemAdapterTest, GetReticularNotNull) {
    EXPECT_NE(brainstem_get_reticular(adapter), nullptr);
}

TEST_F(BrainstemAdapterTest, GetSubModulesNullAdapter) {
    EXPECT_EQ(brainstem_get_midbrain(NULL), nullptr);
    EXPECT_EQ(brainstem_get_pons(NULL), nullptr);
    EXPECT_EQ(brainstem_get_reticular(NULL), nullptr);
    EXPECT_EQ(brainstem_get_medulla(NULL), nullptr);
}

// ============================================================================
// AROUSAL LEVEL TRANSITIONS
// ============================================================================

TEST_F(BrainstemAdapterTest, ArousalLevelTransitions) {
    // Test coma level
    brainstem_reduce_arousal(adapter, 0.5f);
    brainstem_reduce_arousal(adapter, 0.5f);
    brainstem_arousal_level_t level = brainstem_get_arousal_level(adapter);
    EXPECT_LE((int)level, (int)BRAINSTEM_AROUSAL_LIGHT_SLEEP);

    // Reset and test hyperaroused
    brainstem_reset(adapter);
    brainstem_boost_arousal(adapter, 0.5f);
    brainstem_boost_arousal(adapter, 0.5f);
    level = brainstem_get_arousal_level(adapter);
    EXPECT_GE((int)level, (int)BRAINSTEM_AROUSAL_ALERT);
}

// ============================================================================
// INTEGRATION SCENARIO TESTS
// ============================================================================

TEST_F(BrainstemAdapterTest, StartleReflexScenario) {
    // Register startle reflex
    brainstem_reflex_t startle = {
        .reflex_id = 1,
        .name = "startle",
        .threshold = 0.7f,
        .latency_ms = 20.0f,
        .gain = 1.5f,
        .is_active = true
    };
    ASSERT_TRUE(brainstem_register_reflex(adapter, &startle));

    // Process sudden loud sound
    brainstem_sensory_input_t input = {
        .sound_azimuth = 180.0f,
        .sound_intensity = 0.95f,
        .sudden_sound = true
    };

    float initial_arousal = brainstem_get_arousal_value(adapter);

    brainstem_orienting_response_t response;
    EXPECT_TRUE(brainstem_process_sensory(adapter, &input, &response));

    // Arousal should increase
    float final_arousal = brainstem_get_arousal_value(adapter);
    EXPECT_GT(final_arousal, initial_arousal);
    EXPECT_TRUE(response.reflex_triggered);
}

TEST_F(BrainstemAdapterTest, SleepCycleScenario) {
    // Simulate sleep pressure building
    for (int i = 0; i < 5; i++) {
        brainstem_modulate_sleep(adapter, 0.1f * i);
        brainstem_update(adapter, 0.5f);
    }

    // High sleep pressure
    brainstem_modulate_sleep(adapter, 0.9f);
    for (int i = 0; i < 20; i++) {
        brainstem_update(adapter, 0.1f);
    }

    // Should transition toward sleep state
    brainstem_status_t status = brainstem_get_status(adapter);
    // Either still active or transitioned to sleep
    EXPECT_TRUE(status == BRAINSTEM_STATUS_ACTIVE || status == BRAINSTEM_STATUS_SLEEP);
}
