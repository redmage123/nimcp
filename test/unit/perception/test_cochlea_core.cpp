/**
 * @file test_cochlea_core.cpp
 * @brief Unit tests for NIMCP cochlea core processing
 *
 * WHAT: Tests for cochlea creation, configuration, and audio processing
 * WHY:  Ensure cochlea core functions work correctly and handle edge cases
 * HOW:  Use GoogleTest framework with comprehensive coverage
 *
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "perception/nimcp_cochlea.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Constants
//=============================================================================

static const uint32_t TEST_NUM_CHANNELS = 64;
static const uint32_t TEST_SAMPLE_RATE = 44100;
static const uint32_t TEST_BUFFER_SIZE = 512;
static const uint32_t TEST_MAX_SAMPLES = 1024;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Generate a sine wave test signal
 * WHY:  Provide consistent test input for audio processing
 */
static void generate_sine_wave(float* buffer, uint32_t size, float freq_hz,
                                float sample_rate, float amplitude = 1.0f) {
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = amplitude * sinf(2.0f * M_PI * freq_hz * i / sample_rate);
    }
}

/**
 * WHAT: Generate silence (zeros)
 * WHY:  Test handling of silent input
 */
static void generate_silence(float* buffer, uint32_t size) {
    memset(buffer, 0, size * sizeof(float));
}

/**
 * WHAT: Generate white noise
 * WHY:  Test broadband processing
 */
static void generate_noise(float* buffer, uint32_t size, float amplitude = 0.5f) {
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = amplitude * (2.0f * ((float)rand() / RAND_MAX) - 1.0f);
    }
}

//=============================================================================
// Test Fixture
//=============================================================================

class CochleaCoreTest : public ::testing::Test {
protected:
    cochlea_t* cochlea = nullptr;
    cochlea_output_t* output = nullptr;
    float* input_buffer = nullptr;

    void SetUp() override {
        input_buffer = (float*)nimcp_calloc(TEST_BUFFER_SIZE, sizeof(float));
        ASSERT_NE(input_buffer, nullptr);
    }

    void TearDown() override {
        if (output) {
            cochlea_output_destroy(output);
            output = nullptr;
        }
        if (cochlea) {
            cochlea_destroy(cochlea);
            cochlea = nullptr;
        }
        if (input_buffer) {
            nimcp_free(input_buffer);
            input_buffer = nullptr;
        }
    }

    /**
     * WHAT: Create cochlea with default config
     * WHY:  Common setup for most tests
     */
    void CreateDefaultCochlea() {
        cochlea_config_t config = cochlea_config_default(BM_MODE_HUMAN, TEST_SAMPLE_RATE);
        config.num_channels = TEST_NUM_CHANNELS;
        cochlea = cochlea_create(&config);
        ASSERT_NE(cochlea, nullptr);

        output = cochlea_output_create(cochlea, TEST_MAX_SAMPLES);
        ASSERT_NE(output, nullptr);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(CochleaCoreTest, DefaultConfigIsValid) {
    cochlea_config_t config = cochlea_config_default(BM_MODE_HUMAN, TEST_SAMPLE_RATE);

    // Verify default values are reasonable
    EXPECT_GT(config.num_channels, 0u);
    EXPECT_EQ(config.sample_rate, TEST_SAMPLE_RATE);

    // Validate config
    nimcp_error_t err = cochlea_config_validate(&config);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, DogModeConfigHasDifferentMode) {
    cochlea_config_t human_config = cochlea_config_default(BM_MODE_HUMAN, TEST_SAMPLE_RATE);
    cochlea_config_t dog_config = cochlea_config_default(BM_MODE_DOG, TEST_SAMPLE_RATE);

    // Different hearing modes
    EXPECT_EQ(human_config.hearing_mode, BM_MODE_HUMAN);
    EXPECT_EQ(dog_config.hearing_mode, BM_MODE_DOG);
}

TEST_F(CochleaCoreTest, BatModeConfigHasDifferentMode) {
    cochlea_config_t human_config = cochlea_config_default(BM_MODE_HUMAN, TEST_SAMPLE_RATE);
    cochlea_config_t bat_config = cochlea_config_default(BM_MODE_BAT, TEST_SAMPLE_RATE);

    // Different hearing modes
    EXPECT_EQ(human_config.hearing_mode, BM_MODE_HUMAN);
    EXPECT_EQ(bat_config.hearing_mode, BM_MODE_BAT);
}

TEST_F(CochleaCoreTest, InvalidConfigZeroChannels) {
    cochlea_config_t config = cochlea_config_default(BM_MODE_HUMAN, TEST_SAMPLE_RATE);

    // Zero channels should fail
    config.num_channels = 0;
    nimcp_error_t err = cochlea_config_validate(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(CochleaCoreTest, CreateWithValidConfig) {
    cochlea_config_t config = cochlea_config_default(BM_MODE_HUMAN, TEST_SAMPLE_RATE);
    cochlea = cochlea_create(&config);

    EXPECT_NE(cochlea, nullptr);
    EXPECT_EQ(cochlea_get_num_channels(cochlea), config.num_channels);
}

TEST_F(CochleaCoreTest, CreateWithNullConfig) {
    cochlea = cochlea_create(nullptr);
    // Should either return NULL or use defaults
    // Implementation-specific, just verify no crash
}

TEST_F(CochleaCoreTest, DestroyNullIsNoOp) {
    // Should not crash
    cochlea_destroy(nullptr);
}

TEST_F(CochleaCoreTest, OutputCreateDestroy) {
    CreateDefaultCochlea();

    // Output should have been created in setup
    EXPECT_NE(output, nullptr);

    // Destroy is handled in TearDown
}

TEST_F(CochleaCoreTest, OutputDestroyNullIsNoOp) {
    cochlea_output_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Processing Tests
//=============================================================================

TEST_F(CochleaCoreTest, ProcessSineWave) {
    CreateDefaultCochlea();

    // Generate 1kHz sine wave (center of hearing range)
    generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, 1000.0f, TEST_SAMPLE_RATE);

    nimcp_error_t err = cochlea_process(cochlea, input_buffer, TEST_BUFFER_SIZE, output);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, ProcessSilence) {
    CreateDefaultCochlea();

    generate_silence(input_buffer, TEST_BUFFER_SIZE);

    nimcp_error_t err = cochlea_process(cochlea, input_buffer, TEST_BUFFER_SIZE, output);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, ProcessNoise) {
    CreateDefaultCochlea();

    generate_noise(input_buffer, TEST_BUFFER_SIZE);

    nimcp_error_t err = cochlea_process(cochlea, input_buffer, TEST_BUFFER_SIZE, output);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, ProcessNullInput) {
    CreateDefaultCochlea();

    nimcp_error_t err = cochlea_process(cochlea, nullptr, TEST_BUFFER_SIZE, output);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CochleaCoreTest, ProcessNullOutput) {
    CreateDefaultCochlea();

    generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, 1000.0f, TEST_SAMPLE_RATE);

    nimcp_error_t err = cochlea_process(cochlea, input_buffer, TEST_BUFFER_SIZE, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CochleaCoreTest, ProcessNullCochlea) {
    nimcp_error_t err = cochlea_process(nullptr, input_buffer, TEST_BUFFER_SIZE, output);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(CochleaCoreTest, ProcessZeroSamples) {
    CreateDefaultCochlea();

    nimcp_error_t err = cochlea_process(cochlea, input_buffer, 0, output);
    // Could be success (no-op) or error depending on implementation
    (void)err;
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(CochleaCoreTest, GetNumChannels) {
    CreateDefaultCochlea();

    uint32_t num_channels = cochlea_get_num_channels(cochlea);
    EXPECT_EQ(num_channels, TEST_NUM_CHANNELS);
}

TEST_F(CochleaCoreTest, GetNumChannelsNull) {
    uint32_t num_channels = cochlea_get_num_channels(nullptr);
    EXPECT_EQ(num_channels, 0u);
}

TEST_F(CochleaCoreTest, GetChannelFreq) {
    CreateDefaultCochlea();

    // Get frequency of first channel
    float freq = cochlea_get_channel_freq(cochlea, 0);
    EXPECT_GT(freq, 0.0f);

    // Get frequency of last channel
    uint32_t num_channels = cochlea_get_num_channels(cochlea);
    freq = cochlea_get_channel_freq(cochlea, num_channels - 1);
    EXPECT_GT(freq, 0.0f);
}

TEST_F(CochleaCoreTest, GetChannelFreqInvalid) {
    CreateDefaultCochlea();

    float freq = cochlea_get_channel_freq(cochlea, 99999);
    EXPECT_LT(freq, 0.0f);  // Error returns -1
}

TEST_F(CochleaCoreTest, ChannelFreqIncreases) {
    CreateDefaultCochlea();

    uint32_t num_channels = cochlea_get_num_channels(cochlea);
    float prev_freq = 0.0f;

    for (uint32_t i = 0; i < num_channels; i++) {
        float freq = cochlea_get_channel_freq(cochlea, i);
        EXPECT_GT(freq, prev_freq) << "Channel " << i << " frequency not increasing";
        prev_freq = freq;
    }
}

TEST_F(CochleaCoreTest, GetAllFreqs) {
    CreateDefaultCochlea();

    uint32_t num_channels = cochlea_get_num_channels(cochlea);
    std::vector<float> freqs(num_channels);

    nimcp_error_t err = cochlea_get_all_freqs(cochlea, freqs.data());
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify matches individual queries
    for (uint32_t i = 0; i < num_channels; i++) {
        EXPECT_FLOAT_EQ(freqs[i], cochlea_get_channel_freq(cochlea, i));
    }
}

TEST_F(CochleaCoreTest, GetHearingMode) {
    CreateDefaultCochlea();

    bm_hearing_mode_t mode = cochlea_get_hearing_mode(cochlea);
    EXPECT_EQ(mode, BM_MODE_HUMAN);
}

TEST_F(CochleaCoreTest, GetStats) {
    CreateDefaultCochlea();

    cochlea_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    nimcp_error_t err = cochlea_get_stats(cochlea, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Gain and Control Tests
//=============================================================================

TEST_F(CochleaCoreTest, SetGain) {
    CreateDefaultCochlea();

    nimcp_error_t err = cochlea_set_gain(cochlea, 6.0f);  // +6dB
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = cochlea_set_gain(cochlea, -6.0f);  // -6dB
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, SetChannelGain) {
    CreateDefaultCochlea();

    nimcp_error_t err = cochlea_set_channel_gain(cochlea, 0, 3.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, SetChannelGainInvalid) {
    CreateDefaultCochlea();

    nimcp_error_t err = cochlea_set_channel_gain(cochlea, 99999, 3.0f);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, ApplyAttention) {
    CreateDefaultCochlea();

    // Focus attention on 1kHz frequency
    nimcp_error_t err = cochlea_apply_attention(cochlea, 1000.0f, 0.5f, 3.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, ApplyEfferent) {
    CreateDefaultCochlea();

    // Moderate ACh level
    nimcp_error_t err = cochlea_apply_efferent(cochlea, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Health and Damage Tests
//=============================================================================

TEST_F(CochleaCoreTest, InitialHealthIs100) {
    CreateDefaultCochlea();

    float health = cochlea_get_health(cochlea);
    EXPECT_NEAR(health, 1.0f, 0.01f);
}

TEST_F(CochleaCoreTest, ApplyDamage) {
    CreateDefaultCochlea();

    float initial_health = cochlea_get_health(cochlea);

    // Apply moderate damage to channel 10: 50% OHC damage, 30% IHC damage
    nimcp_error_t err = cochlea_apply_damage(cochlea, 10, 0.5f, 0.3f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float new_health = cochlea_get_health(cochlea);
    EXPECT_LT(new_health, initial_health);
}

TEST_F(CochleaCoreTest, ApplyAging) {
    CreateDefaultCochlea();

    // Simulate aging to 60 years
    nimcp_error_t err = cochlea_apply_aging(cochlea, 60.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, ApplyNoiseDamage) {
    CreateDefaultCochlea();

    // Prolonged loud noise exposure: 100 dB for 60 hours
    nimcp_error_t err = cochlea_apply_noise_damage(cochlea, 100.0f, 60.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(CochleaCoreTest, Reset) {
    CreateDefaultCochlea();

    // Process some audio
    generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, 1000.0f, TEST_SAMPLE_RATE);
    cochlea_process(cochlea, input_buffer, TEST_BUFFER_SIZE, output);

    // Reset
    nimcp_error_t err = cochlea_reset(cochlea);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should be able to process again
    err = cochlea_process(cochlea, input_buffer, TEST_BUFFER_SIZE, output);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, ResetNullIsError) {
    nimcp_error_t err = cochlea_reset(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Component Access Tests
//=============================================================================

TEST_F(CochleaCoreTest, GetBasilarMembrane) {
    CreateDefaultCochlea();

    basilar_membrane_t* bm = cochlea_get_basilar_membrane(cochlea);
    EXPECT_NE(bm, nullptr);
}

TEST_F(CochleaCoreTest, GetHairCells) {
    CreateDefaultCochlea();

    hair_cell_bank_t* hc = cochlea_get_hair_cells(cochlea);
    EXPECT_NE(hc, nullptr);
}

TEST_F(CochleaCoreTest, GetAuditoryNerve) {
    CreateDefaultCochlea();

    anf_bank_t* anf = cochlea_get_auditory_nerve(cochlea);
    EXPECT_NE(anf, nullptr);
}

//=============================================================================
// Hearing Mode Tests
//=============================================================================

TEST_F(CochleaCoreTest, SetHearingMode) {
    CreateDefaultCochlea();

    nimcp_error_t err = cochlea_set_hearing_mode(cochlea, BM_MODE_DOG);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    bm_hearing_mode_t mode = cochlea_get_hearing_mode(cochlea);
    EXPECT_EQ(mode, BM_MODE_DOG);
}

//=============================================================================
// Protection Mode Tests
//=============================================================================

TEST_F(CochleaCoreTest, TriggerAcousticReflex) {
    CreateDefaultCochlea();

    // Trigger reflex with loud sound
    nimcp_error_t err = cochlea_trigger_acoustic_reflex(cochlea, 100.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(CochleaCoreTest, CheckProtectionMode) {
    CreateDefaultCochlea();

    bool protected_mode = cochlea_is_in_protection_mode(cochlea);
    EXPECT_FALSE(protected_mode);  // Initially not in protection mode
}

//=============================================================================
// Output Clear Tests
//=============================================================================

TEST_F(CochleaCoreTest, OutputClear) {
    CreateDefaultCochlea();

    // Process audio to fill output
    generate_sine_wave(input_buffer, TEST_BUFFER_SIZE, 1000.0f, TEST_SAMPLE_RATE);
    cochlea_process(cochlea, input_buffer, TEST_BUFFER_SIZE, output);

    // Clear output
    cochlea_output_clear(output);
    // Should not crash
}

TEST_F(CochleaCoreTest, OutputClearNull) {
    cochlea_output_clear(nullptr);  // Should not crash
}

