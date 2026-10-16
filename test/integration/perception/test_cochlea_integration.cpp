/**
 * @file test_cochlea_integration.cpp
 * @brief Integration tests for NIMCP cochlea module with brain components
 *
 * WHAT: Tests for cochlea integration with brain regions and systems
 * WHY:  Ensure cochlea works correctly as part of the larger brain system
 * HOW:  Use GoogleTest framework with multi-component scenarios
 *
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "perception/nimcp_cochlea.h"
#include "perception/bridges/nimcp_cochlea_medulla_bridge.h"
#include "perception/bridges/nimcp_cochlea_thalamic_bridge.h"
#include "perception/bridges/nimcp_cochlea_audio_cortex_bridge.h"
#include "perception/bridges/nimcp_cochlea_fep_bridge.h"
#include "perception/bridges/nimcp_cochlea_sleep_bridge.h"
#include "perception/bridges/nimcp_cochlea_verification_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t TEST_NUM_CHANNELS = 64;
static const uint32_t TEST_SAMPLE_RATE = 44100;
static const uint32_t TEST_BUFFER_SIZE = 512;
static const uint32_t TEST_MAX_SAMPLES = 1024;
static const float TEST_DT_MS = 10.0f;

//=============================================================================
// Helper Functions
//=============================================================================

static void generate_sine_wave(float* buffer, uint32_t size, float freq_hz,
                                float sample_rate, float amplitude = 1.0f) {
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * freq_hz * i / sample_rate);
    }
}

static void generate_speech_like(float* buffer, uint32_t size, float sample_rate) {
    // Generate speech-like signal with formants
    for (uint32_t i = 0; i < size; i++) {
        float t = (float)i / sample_rate;
        // F0 (fundamental) around 120Hz
        float f0 = 0.3f * sinf(2.0f * M_PI * 120.0f * t);
        // F1 (first formant) around 500Hz
        float f1 = 0.5f * sinf(2.0f * M_PI * 500.0f * t);
        // F2 (second formant) around 1500Hz
        float f2 = 0.3f * sinf(2.0f * M_PI * 1500.0f * t);
        // F3 (third formant) around 2500Hz
        float f3 = 0.1f * sinf(2.0f * M_PI * 2500.0f * t);

        buffer[i] = 0.5f * (f0 + f1 + f2 + f3);
    }
}

static void generate_alarm_like(float* buffer, uint32_t size, float sample_rate) {
    // Generate alarm-like signal (500-1000Hz sweep)
    for (uint32_t i = 0; i < size; i++) {
        float t = (float)i / sample_rate;
        float phase = (float)i / (float)size;
        float freq = 500.0f + 500.0f * phase;  // Sweep up
        buffer[i] = 0.8f * sinf(2.0f * M_PI * freq * t);
    }
}

//=============================================================================
// Test Fixture
//=============================================================================

class CochleaIntegrationTest : public ::testing::Test {
protected:
    cochlea_t* cochlea = nullptr;
    cochlea_output_t* output = nullptr;
    float* input_buffer = nullptr;

    // Bridges
    cochlea_medulla_bridge_t* medulla = nullptr;
    cochlea_thalamic_bridge_t* thalamic = nullptr;
    cochlea_audio_cortex_bridge_t* cortical = nullptr;
    cochlea_fep_bridge_t* fep = nullptr;
    cochlea_sleep_bridge_t* sleep_bridge = nullptr;
    cochlea_verification_bridge_t* verifier = nullptr;

    void SetUp() override {
        // Create cochlea
        cochlea_config_t config = cochlea_config_default(BM_MODE_HUMAN, TEST_SAMPLE_RATE);
        config.num_channels = TEST_NUM_CHANNELS;
        cochlea = cochlea_create(&config);
        ASSERT_NE(cochlea, nullptr);

        output = cochlea_output_create(cochlea, TEST_MAX_SAMPLES);
        ASSERT_NE(output, nullptr);

        input_buffer = (float*)nimcp_calloc(TEST_BUFFER_SIZE, sizeof(float));
        ASSERT_NE(input_buffer, nullptr);

        // Create bridges
        cochlea_medulla_config_t medulla_cfg = cochlea_medulla_config_default();
        medulla = cochlea_medulla_bridge_create(cochlea, nullptr, &medulla_cfg);

        cochlea_thalamic_config_t thalamic_cfg = cochlea_thalamic_config_default();
        thalamic = cochlea_thalamic_bridge_create(cochlea, nullptr, &thalamic_cfg);

        cochlea_audio_cortex_config_t cortical_cfg = cochlea_audio_cortex_config_default();
        cortical = cochlea_audio_cortex_bridge_create(cochlea, nullptr, nullptr, &cortical_cfg);

        cochlea_fep_config_t fep_cfg = cochlea_fep_config_default();
        fep = cochlea_fep_bridge_create(cochlea, nullptr, &fep_cfg);

        cochlea_sleep_config_t sleep_cfg = cochlea_sleep_config_default();
        sleep_bridge = cochlea_sleep_bridge_create(cochlea, nullptr, &sleep_cfg);

        cochlea_verification_config_t verify_cfg = cochlea_verification_config_default();
        verifier = cochlea_verification_bridge_create(cochlea, &verify_cfg);
    }

    void TearDown() override {
        if (verifier) cochlea_verification_bridge_destroy(verifier);
        if (sleep_bridge) cochlea_sleep_bridge_destroy(sleep_bridge);
        if (fep) cochlea_fep_bridge_destroy(fep);
        if (cortical) cochlea_audio_cortex_bridge_destroy(cortical);
        if (thalamic) cochlea_thalamic_bridge_destroy(thalamic);
        if (medulla) cochlea_medulla_bridge_destroy(medulla);

        if (input_buffer) nimcp_free(input_buffer);
        if (output) cochlea_output_destroy(output);
        if (cochlea) cochlea_destroy(cochlea);
    }

    void ProcessCochlea() {
        cochlea_process(cochlea, input_buffer, TEST_BUFFER_SIZE, output);
    }
};

//=============================================================================
// Full Pipeline Tests
//=============================================================================

TEST_F(CochleaIntegrationTest, FullPipelineSineWave) {
    // Process a 1kHz tone through the full pipeline
    generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, 1000.0f, TEST_SAMPLE_RATE);

    for (int frame = 0; frame < 10; frame++) {
        ProcessCochlea();
    }

    // Verify output has meaningful data
    EXPECT_GT(output->total_energy, 0.0f);
}

TEST_F(CochleaIntegrationTest, FullPipelineSpeech) {
    // Process speech-like signal
    generate_speech_like(input_buffer, TEST_BUFFER_SIZE, TEST_SAMPLE_RATE);

    for (int frame = 0; frame < 10; frame++) {
        ProcessCochlea();
    }

    // Output should have meaningful data
    EXPECT_GT(output->total_energy, 0.0f);
}

TEST_F(CochleaIntegrationTest, FullPipelineAlarm) {
    // Process alarm-like signal
    generate_alarm_like(input_buffer, TEST_BUFFER_SIZE, TEST_SAMPLE_RATE);

    for (int frame = 0; frame < 10; frame++) {
        ProcessCochlea();
    }

    // Alarm should have energy
    EXPECT_GT(output->total_energy, 0.0f);
}

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(CochleaIntegrationTest, MedullaBridgeReset) {
    if (!medulla) GTEST_SKIP() << "Medulla bridge not available";

    nimcp_error_t err = cochlea_medulla_bridge_reset(medulla);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaIntegrationTest, ThalamicBridgeReset) {
    if (!thalamic) GTEST_SKIP() << "Thalamic bridge not available";

    nimcp_error_t err = cochlea_thalamic_bridge_reset(thalamic);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaIntegrationTest, AudioCortexBridgeReset) {
    if (!cortical) GTEST_SKIP() << "Audio cortex bridge not available";

    nimcp_error_t err = cochlea_audio_cortex_bridge_reset(cortical);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaIntegrationTest, FEPBridgeReset) {
    if (!fep) GTEST_SKIP() << "FEP bridge not available";

    nimcp_error_t err = cochlea_fep_bridge_reset(fep);
    EXPECT_EQ(err, 0);
}

TEST_F(CochleaIntegrationTest, SleepBridgeReset) {
    if (!sleep_bridge) GTEST_SKIP() << "Sleep bridge not available";

    nimcp_error_t err = cochlea_sleep_bridge_reset(sleep_bridge);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaIntegrationTest, VerificationBridgeReset) {
    if (!verifier) GTEST_SKIP() << "Verification bridge not available";

    nimcp_error_t err = cochlea_verification_bridge_reset(verifier);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Protection System Tests
//=============================================================================

TEST_F(CochleaIntegrationTest, ProtectionModeCheck) {
    // Initially not in protection mode
    bool protected_mode = cochlea_is_in_protection_mode(cochlea);
    EXPECT_FALSE(protected_mode);

    // Trigger acoustic reflex with loud sound
    nimcp_error_t err = cochlea_trigger_acoustic_reflex(cochlea, 120.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaIntegrationTest, MedullaProtection) {
    if (!medulla) GTEST_SKIP() << "Medulla bridge not available";

    bool protected_mode = cochlea_medulla_is_protection_active(medulla);
    EXPECT_FALSE(protected_mode);  // Initially not protected
}

//=============================================================================
// Verification System Tests
//=============================================================================

TEST_F(CochleaIntegrationTest, VerificationVerifyAll) {
    if (!verifier) GTEST_SKIP() << "Verification bridge not available";

    nimcp_error_t err = cochlea_verification_verify_all(verifier);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaIntegrationTest, VerificationHealthCheck) {
    if (!verifier) GTEST_SKIP() << "Verification bridge not available";

    float health = cochlea_verification_get_health(verifier);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

//=============================================================================
// Multi-Stage Processing Tests
//=============================================================================

TEST_F(CochleaIntegrationTest, ProcessMultipleFrames) {
    generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, 1000.0f, TEST_SAMPLE_RATE);

    // Process many frames
    for (int frame = 0; frame < 100; frame++) {
        ProcessCochlea();
    }

    // Should still produce valid output
    EXPECT_GT(output->total_energy, 0.0f);
}

TEST_F(CochleaIntegrationTest, ProcessVaryingFrequencies) {
    float frequencies[] = {200.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f};

    for (float freq : frequencies) {
        generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, freq, TEST_SAMPLE_RATE);
        ProcessCochlea();

        // Each frequency should produce output
        EXPECT_GT(output->total_energy, 0.0f) << "Failed at frequency " << freq;
    }
}

TEST_F(CochleaIntegrationTest, ProcessReset) {
    // Process some audio
    generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, 1000.0f, TEST_SAMPLE_RATE);
    for (int frame = 0; frame < 10; frame++) {
        ProcessCochlea();
    }

    // Reset
    nimcp_error_t err = cochlea_reset(cochlea);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process again
    for (int frame = 0; frame < 10; frame++) {
        ProcessCochlea();
    }

    // Should still work
    EXPECT_GT(output->total_energy, 0.0f);
}

//=============================================================================
// Hearing Mode Integration Tests
//=============================================================================

TEST_F(CochleaIntegrationTest, SwitchHearingMode) {
    // Start in human mode
    EXPECT_EQ(cochlea_get_hearing_mode(cochlea), BM_MODE_HUMAN);

    // Process some audio
    generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, 1000.0f, TEST_SAMPLE_RATE);
    ProcessCochlea();
    float human_energy = output->total_energy;

    // Switch to dog mode
    nimcp_error_t err = cochlea_set_hearing_mode(cochlea, BM_MODE_DOG);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(cochlea_get_hearing_mode(cochlea), BM_MODE_DOG);

    // Process again
    ProcessCochlea();
    float dog_energy = output->total_energy;

    // Both should produce valid output
    EXPECT_GT(human_energy, 0.0f);
    EXPECT_GT(dog_energy, 0.0f);
}

//=============================================================================
// Component Access Integration Tests
//=============================================================================

TEST_F(CochleaIntegrationTest, ComponentsAvailable) {
    // All components should be accessible
    basilar_membrane_t* bm = cochlea_get_basilar_membrane(cochlea);
    hair_cell_bank_t* hc = cochlea_get_hair_cells(cochlea);
    anf_bank_t* anf = cochlea_get_auditory_nerve(cochlea);

    EXPECT_NE(bm, nullptr);
    EXPECT_NE(hc, nullptr);
    EXPECT_NE(anf, nullptr);
}

//=============================================================================
// Health Integration Tests
//=============================================================================

TEST_F(CochleaIntegrationTest, HealthAfterDamage) {
    float initial_health = cochlea_get_health(cochlea);
    EXPECT_NEAR(initial_health, 1.0f, 0.01f);

    // Apply damage
    nimcp_error_t err = cochlea_apply_damage(cochlea, 10, 0.5f, 0.3f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float damaged_health = cochlea_get_health(cochlea);
    EXPECT_LT(damaged_health, initial_health);

    // Processing should still work
    generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, 1000.0f, TEST_SAMPLE_RATE);
    ProcessCochlea();
    EXPECT_GE(output->total_energy, 0.0f);  // May be reduced due to damage
}

