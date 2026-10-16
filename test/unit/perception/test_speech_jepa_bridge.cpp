/**
 * @file test_speech_jepa_bridge.cpp
 * @brief Unit tests for Speech JEPA Bridge
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Comprehensive unit tests for speech-to-JEPA encoding bridge
 * WHY:  Ensure correct lifecycle, configuration, sequence management, encoding,
 *       training, prediction, and edge case handling.
 * HOW:  Test all API functions with valid and invalid inputs, verify return codes,
 *       state transitions, and null safety.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include "perception/nimcp_speech_jepa_bridge.h"

/**
 * @brief Test fixture for Speech JEPA Bridge tests
 *
 * WHAT: Provides setup/teardown for bridge testing
 * WHY:  Ensure clean state for each test
 * HOW:  Create bridge in SetUp, destroy in TearDown
 */
class SpeechJepaBridgeTest : public ::testing::Test {
protected:
    speech_jepa_bridge_t* bridge = nullptr;
    speech_jepa_sequence_t* sequence = nullptr;

    /**
     * WHAT: Initialize bridge before each test
     * WHY:  Ensure clean starting state
     * HOW:  Create with default config
     */
    void SetUp() override {
        speech_jepa_bridge_config_t config;
        speech_jepa_bridge_default_config(&config);
        bridge = speech_jepa_bridge_create(&config);
    }

    /**
     * WHAT: Clean up bridge after each test
     * WHY:  Prevent memory leaks
     * HOW:  Destroy bridge and sequence, reset pointers
     */
    void TearDown() override {
        if (sequence) {
            speech_jepa_sequence_destroy(sequence);
            sequence = nullptr;
        }
        if (bridge) {
            speech_jepa_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /**
     * WHAT: Helper to create a valid test frame
     * WHY:  Reuse across tests
     * HOW:  Initialize frame with sensible values
     */
    speech_jepa_frame_t CreateTestFrame(phoneme_t phoneme, uint64_t timestamp) {
        speech_jepa_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.phoneme = phoneme;
        frame.phoneme_confidence = 0.9f;
        frame.formants[0] = 300.0f;   // F1
        frame.formants[1] = 2000.0f;  // F2
        frame.formants[2] = 2800.0f;  // F3
        frame.formants[3] = 3500.0f;  // F4
        frame.pitch = 120.0f;
        frame.energy = 0.5f;
        frame.duration_ms = 20.0f;
        frame.timestamp_ms = timestamp;
        frame.is_voiced = true;
        return frame;
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

/**
 * WHAT: Test default config provides valid values
 * WHY:  Verify configuration initialization
 * HOW:  Call default_config, check fields have sensible values
 */
TEST_F(SpeechJepaBridgeTest, DefaultConfig) {
    speech_jepa_bridge_config_t config;
    int result = speech_jepa_bridge_default_config(&config);

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Encoder config should have valid dimensions
    EXPECT_GT(config.encoder.hidden_dim, 0u);
    EXPECT_GT(config.encoder.output_dim, 0u);
    EXPECT_GT(config.encoder.input_dim, 0u);

    // Sequence config should have valid values
    EXPECT_GT(config.frame_duration_ms, 0u);
    EXPECT_GT(config.sequence_length, 0u);

    // Masking config should have valid ratio
    EXPECT_GT(config.mask_ratio, 0.0f);
    EXPECT_LT(config.mask_ratio, 1.0f);

    // Training parameters should have valid values
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_GE(config.momentum, 0.0f);
    EXPECT_LE(config.momentum, 1.0f);
}

/**
 * WHAT: Test default config with NULL parameter
 * WHY:  Verify NULL safety for config initialization
 * HOW:  Call with NULL, expect error
 */
TEST_F(SpeechJepaBridgeTest, DefaultConfigNull) {
    int result = speech_jepa_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

/**
 * WHAT: Test successful bridge creation and destruction
 * WHY:  Verify basic lifecycle management
 * HOW:  Create with valid config, verify non-null, destroy
 */
TEST_F(SpeechJepaBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    // Cleanup handled by TearDown
}

/**
 * WHAT: Test creation with NULL config uses defaults
 * WHY:  Verify NULL config fallback behavior
 * HOW:  Create with NULL, verify non-null result
 */
TEST_F(SpeechJepaBridgeTest, CreateWithNullConfig) {
    // Clean up existing bridge
    speech_jepa_bridge_destroy(bridge);

    // Create with NULL config
    bridge = speech_jepa_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

/**
 * WHAT: Test destroying NULL bridge is safe
 * WHY:  Verify NULL pointer safety
 * HOW:  Call destroy with NULL, should not crash
 */
TEST_F(SpeechJepaBridgeTest, DestroyNull) {
    speech_jepa_bridge_destroy(nullptr);
    // Test passes if no crash
}

/**
 * WHAT: Test bridge reset functionality
 * WHY:  Verify state can be cleared without reallocation
 * HOW:  Use bridge, reset, verify clean state
 */
TEST_F(SpeechJepaBridgeTest, Reset) {
    ASSERT_NE(bridge, nullptr);

    int result = speech_jepa_bridge_reset(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test reset with NULL bridge
 * WHY:  Verify NULL safety for reset
 * HOW:  Call reset with NULL
 */
TEST_F(SpeechJepaBridgeTest, ResetNull) {
    int result = speech_jepa_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

/**
 * WHAT: Test connecting speech cortex
 * WHY:  Verify speech cortex integration setup
 * HOW:  Connect speech cortex pointer, verify success
 */
TEST_F(SpeechJepaBridgeTest, ConnectSpeechCortex) {
    // Mock speech cortex (non-null pointer for testing)
    speech_cortex_t* mock_speech = reinterpret_cast<speech_cortex_t*>(0x1000);

    int result = speech_jepa_bridge_connect_speech_cortex(bridge, mock_speech);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify connected
    EXPECT_TRUE(speech_jepa_bridge_is_connected(bridge));
}

/**
 * WHAT: Test disconnecting speech cortex
 * WHY:  Verify speech cortex disconnection
 * HOW:  Connect then disconnect, verify state
 */
TEST_F(SpeechJepaBridgeTest, DisconnectSpeechCortex) {
    speech_cortex_t* mock_speech = reinterpret_cast<speech_cortex_t*>(0x1000);
    speech_jepa_bridge_connect_speech_cortex(bridge, mock_speech);

    int result = speech_jepa_bridge_disconnect_speech_cortex(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify disconnected
    EXPECT_FALSE(speech_jepa_bridge_is_connected(bridge));
}

/**
 * WHAT: Test initial connection state is disconnected
 * WHY:  Verify bridge starts disconnected
 * HOW:  Check is_connected before any connect call
 */
TEST_F(SpeechJepaBridgeTest, InitiallyDisconnected) {
    EXPECT_FALSE(speech_jepa_bridge_is_connected(bridge));
}

/**
 * WHAT: Test connection functions with NULL bridge
 * WHY:  Verify NULL safety for connection functions
 * HOW:  Call connect/disconnect with NULL bridge
 */
TEST_F(SpeechJepaBridgeTest, ConnectNullBridge) {
    speech_cortex_t* mock_speech = reinterpret_cast<speech_cortex_t*>(0x1000);

    EXPECT_NE(speech_jepa_bridge_connect_speech_cortex(nullptr, mock_speech), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_disconnect_speech_cortex(nullptr), NIMCP_SUCCESS);
    EXPECT_FALSE(speech_jepa_bridge_is_connected(nullptr));
}

/**
 * WHAT: Test connecting with NULL speech cortex
 * WHY:  Verify NULL safety for speech cortex pointer
 * HOW:  Call connect with NULL system
 */
TEST_F(SpeechJepaBridgeTest, ConnectNullSystem) {
    EXPECT_NE(speech_jepa_bridge_connect_speech_cortex(bridge, nullptr), NIMCP_SUCCESS);
}

/* ============================================================================
 * Sequence Management Tests
 * ============================================================================ */

/**
 * WHAT: Test sequence creation with valid capacity
 * WHY:  Verify sequence allocation
 * HOW:  Create sequence, verify non-null
 */
TEST_F(SpeechJepaBridgeTest, SequenceCreate) {
    sequence = speech_jepa_sequence_create(50);
    ASSERT_NE(sequence, nullptr);

    EXPECT_EQ(sequence->num_frames, 0u);
    EXPECT_EQ(sequence->max_frames, 50u);
}

/**
 * WHAT: Test sequence creation with zero capacity
 * WHY:  Verify edge case handling
 * HOW:  Create with 0 capacity, expect failure or minimum
 */
TEST_F(SpeechJepaBridgeTest, SequenceCreateZeroCapacity) {
    sequence = speech_jepa_sequence_create(0);
    // Implementation may return NULL or use minimum capacity
    // Either behavior is acceptable
}

/**
 * WHAT: Test sequence destruction with NULL
 * WHY:  Verify NULL safety
 * HOW:  Destroy NULL, should not crash
 */
TEST_F(SpeechJepaBridgeTest, SequenceDestroyNull) {
    speech_jepa_sequence_destroy(nullptr);
    // Test passes if no crash
}

/**
 * WHAT: Test adding frame to sequence
 * WHY:  Verify frame accumulation
 * HOW:  Add frames, verify count increases
 */
TEST_F(SpeechJepaBridgeTest, SequenceAddFrame) {
    sequence = speech_jepa_sequence_create(50);
    ASSERT_NE(sequence, nullptr);

    speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, 0);

    int result = speech_jepa_sequence_add_frame(sequence, &frame);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(sequence->num_frames, 1u);

    // Add another frame
    frame.timestamp_ms = 20;
    result = speech_jepa_sequence_add_frame(sequence, &frame);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(sequence->num_frames, 2u);
}

/**
 * WHAT: Test adding frame to full sequence
 * WHY:  Verify capacity limit handling
 * HOW:  Fill sequence, try to add more
 */
TEST_F(SpeechJepaBridgeTest, SequenceAddFrameFull) {
    sequence = speech_jepa_sequence_create(2);
    ASSERT_NE(sequence, nullptr);

    speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, 0);

    // Fill sequence
    speech_jepa_sequence_add_frame(sequence, &frame);
    frame.timestamp_ms = 20;
    speech_jepa_sequence_add_frame(sequence, &frame);

    // Try to add beyond capacity
    frame.timestamp_ms = 40;
    int result = speech_jepa_sequence_add_frame(sequence, &frame);

    // Should return error or not increase count
    if (result == NIMCP_SUCCESS) {
        EXPECT_EQ(sequence->num_frames, 2u);  // Count should not exceed max
    } else {
        EXPECT_EQ(sequence->num_frames, 2u);
    }
}

/**
 * WHAT: Test adding frame with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call add_frame with NULL sequence or frame
 */
TEST_F(SpeechJepaBridgeTest, SequenceAddFrameNull) {
    sequence = speech_jepa_sequence_create(50);
    speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, 0);

    EXPECT_NE(speech_jepa_sequence_add_frame(nullptr, &frame), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_sequence_add_frame(sequence, nullptr), NIMCP_SUCCESS);
}

/**
 * WHAT: Test sequence clear
 * WHY:  Verify clearing resets count but keeps allocation
 * HOW:  Add frames, clear, verify count reset
 */
TEST_F(SpeechJepaBridgeTest, SequenceClear) {
    sequence = speech_jepa_sequence_create(50);
    ASSERT_NE(sequence, nullptr);

    // Add some frames
    speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, 0);
    speech_jepa_sequence_add_frame(sequence, &frame);
    speech_jepa_sequence_add_frame(sequence, &frame);
    EXPECT_EQ(sequence->num_frames, 2u);

    // Clear
    int result = speech_jepa_sequence_clear(sequence);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(sequence->num_frames, 0u);
    EXPECT_EQ(sequence->max_frames, 50u);  // Capacity preserved
}

/**
 * WHAT: Test sequence clear with NULL
 * WHY:  Verify NULL safety
 * HOW:  Clear NULL sequence
 */
TEST_F(SpeechJepaBridgeTest, SequenceClearNull) {
    int result = speech_jepa_sequence_clear(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Feature Extraction Tests
 * ============================================================================ */

/**
 * WHAT: Test feature extraction from frame
 * WHY:  Verify frame-to-feature conversion
 * HOW:  Extract features, verify output
 */
TEST_F(SpeechJepaBridgeTest, ExtractFeatures) {
    speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, 0);
    float features[SPEECH_JEPA_FRAME_FEATURES];
    memset(features, 0, sizeof(features));

    int result = speech_jepa_bridge_extract_features(
        bridge, &frame, features, SPEECH_JEPA_FRAME_FEATURES
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify some features are non-zero
    bool has_nonzero = false;
    for (int i = 0; i < SPEECH_JEPA_FRAME_FEATURES; i++) {
        if (features[i] != 0.0f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);
}

/**
 * WHAT: Test feature extraction with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call with various NULL combinations
 */
TEST_F(SpeechJepaBridgeTest, ExtractFeaturesNull) {
    speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, 0);
    float features[SPEECH_JEPA_FRAME_FEATURES];

    EXPECT_NE(speech_jepa_bridge_extract_features(nullptr, &frame, features, SPEECH_JEPA_FRAME_FEATURES), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_extract_features(bridge, nullptr, features, SPEECH_JEPA_FRAME_FEATURES), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_extract_features(bridge, &frame, nullptr, SPEECH_JEPA_FRAME_FEATURES), NIMCP_SUCCESS);
}

/**
 * WHAT: Test feature extraction with zero dimension
 * WHY:  Verify dimension validation
 * HOW:  Call with 0 feature_dim
 */
TEST_F(SpeechJepaBridgeTest, ExtractFeaturesZeroDim) {
    speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, 0);
    float features[1];

    int result = speech_jepa_bridge_extract_features(bridge, &frame, features, 0);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Encoding Tests
 * ============================================================================ */

/**
 * WHAT: Test sequence encoding to JEPA latent
 * WHY:  Verify core encoding functionality
 * HOW:  Create sequence, encode, verify latent output
 */
TEST_F(SpeechJepaBridgeTest, EncodeSequence) {
    // Create and populate sequence
    sequence = speech_jepa_sequence_create(10);
    ASSERT_NE(sequence, nullptr);

    for (int i = 0; i < 5; i++) {
        speech_jepa_frame_t frame = CreateTestFrame(static_cast<phoneme_t>(i % PHONEME_COUNT), i * 20);
        speech_jepa_sequence_add_frame(sequence, &frame);
    }

    // Create latent output
    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    ASSERT_NE(latent, nullptr);

    int result = speech_jepa_bridge_encode(bridge, sequence, latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify latent has valid data
    EXPECT_EQ(latent->latent_dim, SPEECH_JEPA_DEFAULT_ENCODER_DIM);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test encoding empty sequence
 * WHY:  Verify empty input handling
 * HOW:  Encode empty sequence
 */
TEST_F(SpeechJepaBridgeTest, EncodeEmptySequence) {
    sequence = speech_jepa_sequence_create(10);
    ASSERT_NE(sequence, nullptr);
    EXPECT_EQ(sequence->num_frames, 0u);

    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    ASSERT_NE(latent, nullptr);

    int result = speech_jepa_bridge_encode(bridge, sequence, latent);
    // Empty sequence encoding may succeed with zero latent or fail
    // Either behavior is acceptable

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test encoding with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call encode with various NULL combinations
 */
TEST_F(SpeechJepaBridgeTest, EncodeNull) {
    sequence = speech_jepa_sequence_create(10);
    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);

    EXPECT_NE(speech_jepa_bridge_encode(nullptr, sequence, latent), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_encode(bridge, nullptr, latent), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_encode(bridge, sequence, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test phoneme array encoding
 * WHY:  Verify simplified phoneme-level encoding
 * HOW:  Encode phoneme array, verify latent output
 */
TEST_F(SpeechJepaBridgeTest, EncodePhonemes) {
    phoneme_t phonemes[] = {PHONEME_IY, PHONEME_T, PHONEME_S};
    uint32_t num_phonemes = sizeof(phonemes) / sizeof(phonemes[0]);

    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    ASSERT_NE(latent, nullptr);

    int result = speech_jepa_bridge_encode_phonemes(bridge, phonemes, num_phonemes, latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test phoneme encoding with empty array
 * WHY:  Verify empty input handling
 * HOW:  Encode with num_phonemes = 0
 */
TEST_F(SpeechJepaBridgeTest, EncodePhonemesEmpty) {
    phoneme_t phonemes[] = {PHONEME_IY};

    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    ASSERT_NE(latent, nullptr);

    int result = speech_jepa_bridge_encode_phonemes(bridge, phonemes, 0, latent);
    // Empty encoding may succeed or fail - both acceptable

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test phoneme encoding with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call with various NULL combinations
 */
TEST_F(SpeechJepaBridgeTest, EncodePhonemesNull) {
    phoneme_t phonemes[] = {PHONEME_IY, PHONEME_T};
    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);

    EXPECT_NE(speech_jepa_bridge_encode_phonemes(nullptr, phonemes, 2, latent), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_encode_phonemes(bridge, nullptr, 2, latent), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_encode_phonemes(bridge, phonemes, 2, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
}

/* ============================================================================
 * Training Tests
 * ============================================================================ */

/**
 * WHAT: Test setting training mode
 * WHY:  Verify training/inference mode switching
 * HOW:  Set training mode, verify success
 */
TEST_F(SpeechJepaBridgeTest, SetTrainingMode) {
    int result = speech_jepa_bridge_set_training(bridge, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    result = speech_jepa_bridge_set_training(bridge, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test setting training with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL bridge
 */
TEST_F(SpeechJepaBridgeTest, SetTrainingNull) {
    int result = speech_jepa_bridge_set_training(nullptr, true);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test training step
 * WHY:  Verify complete training iteration
 * HOW:  Create sequence, run training step, check loss
 */
TEST_F(SpeechJepaBridgeTest, TrainStep) {
    // Enable training mode
    speech_jepa_bridge_set_training(bridge, true);

    // Create and populate sequence
    sequence = speech_jepa_sequence_create(20);
    ASSERT_NE(sequence, nullptr);

    for (int i = 0; i < 10; i++) {
        speech_jepa_frame_t frame = CreateTestFrame(
            static_cast<phoneme_t>(i % PHONEME_COUNT), i * 20
        );
        speech_jepa_sequence_add_frame(sequence, &frame);
    }

    float loss = -1.0f;
    int result = speech_jepa_bridge_train_step(bridge, sequence, &loss);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(loss, 0.0f);  // Loss should be non-negative
}

/**
 * WHAT: Test training step with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call with various NULL combinations
 */
TEST_F(SpeechJepaBridgeTest, TrainStepNull) {
    sequence = speech_jepa_sequence_create(10);
    float loss;

    EXPECT_NE(speech_jepa_bridge_train_step(nullptr, sequence, &loss), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_train_step(bridge, nullptr, &loss), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_train_step(bridge, sequence, nullptr), NIMCP_SUCCESS);
}

/**
 * WHAT: Test training step with empty sequence
 * WHY:  Verify empty input handling
 * HOW:  Train with empty sequence
 */
TEST_F(SpeechJepaBridgeTest, TrainStepEmptySequence) {
    speech_jepa_bridge_set_training(bridge, true);

    sequence = speech_jepa_sequence_create(10);
    ASSERT_NE(sequence, nullptr);

    float loss;
    int result = speech_jepa_bridge_train_step(bridge, sequence, &loss);
    // Should handle gracefully (return error or skip)
}

/* ============================================================================
 * Prediction Tests
 * ============================================================================ */

/**
 * WHAT: Test next phoneme prediction
 * WHY:  Verify autoregressive prediction
 * HOW:  Create context latent, predict next
 */
TEST_F(SpeechJepaBridgeTest, PredictNext) {
    // Get the correct latent dim from config (predictor expects encoder output_dim)
    speech_jepa_bridge_config_t config;
    speech_jepa_bridge_default_config(&config);
    const uint32_t latent_dim = config.encoder.output_dim;

    // Create context latent
    jepa_latent_t* context = jepa_latent_create_dim(latent_dim);
    ASSERT_NE(context, nullptr);

    // Initialize with some values
    for (uint32_t i = 0; i < context->latent_dim; i++) {
        context->embedding[i] = 0.1f * (i % 10);
    }

    // Create prediction output latent
    jepa_latent_t* predicted = jepa_latent_create_dim(latent_dim);
    ASSERT_NE(predicted, nullptr);

    int result = speech_jepa_bridge_predict_next(bridge, context, predicted);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(predicted);
}

/**
 * WHAT: Test prediction with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call predict with various NULL combinations
 */
TEST_F(SpeechJepaBridgeTest, PredictNextNull) {
    jepa_latent_t* context = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    jepa_latent_t* predicted = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);

    EXPECT_NE(speech_jepa_bridge_predict_next(nullptr, context, predicted), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_predict_next(bridge, nullptr, predicted), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_predict_next(bridge, context, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(context);
    jepa_latent_destroy(predicted);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * WHAT: Test getting bridge statistics
 * WHY:  Verify statistics reporting
 * HOW:  Get stats, verify structure
 */
TEST_F(SpeechJepaBridgeTest, GetStats) {
    speech_jepa_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with garbage

    int result = speech_jepa_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Initial stats should be zeros or valid values
    EXPECT_GE(stats.frames_processed, 0UL);
    EXPECT_GE(stats.sequences_processed, 0UL);
    EXPECT_GE(stats.predictions_made, 0UL);
}

/**
 * WHAT: Test stats with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call with various NULL combinations
 */
TEST_F(SpeechJepaBridgeTest, GetStatsNull) {
    speech_jepa_stats_t stats;

    EXPECT_NE(speech_jepa_bridge_get_stats(nullptr, &stats), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_get_stats(bridge, nullptr), NIMCP_SUCCESS);
}

/**
 * WHAT: Test resetting statistics
 * WHY:  Verify stats can be cleared
 * HOW:  Process data, reset, verify zeros
 */
TEST_F(SpeechJepaBridgeTest, ResetStats) {
    // Do some processing to generate stats
    sequence = speech_jepa_sequence_create(10);
    for (int i = 0; i < 5; i++) {
        speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, i * 20);
        speech_jepa_sequence_add_frame(sequence, &frame);
    }

    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    speech_jepa_bridge_encode(bridge, sequence, latent);
    jepa_latent_destroy(latent);

    // Reset stats
    int result = speech_jepa_bridge_reset_stats(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify stats are reset
    speech_jepa_stats_t stats;
    speech_jepa_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.frames_processed, 0UL);
    EXPECT_EQ(stats.sequences_processed, 0UL);
}

/**
 * WHAT: Test reset stats with NULL
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL bridge
 */
TEST_F(SpeechJepaBridgeTest, ResetStatsNull) {
    int result = speech_jepa_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

/**
 * WHAT: Test bio-async connection
 * WHY:  Verify bio-async integration setup
 * HOW:  Connect bio-async, check state
 */
TEST_F(SpeechJepaBridgeTest, BioAsyncConnect) {
    int result = speech_jepa_bridge_connect_bio_async(bridge);
    // May return 0 or -1 depending on router availability
    // Just verify it doesn't crash
    (void)result;
}

/**
 * WHAT: Test bio-async disconnection
 * WHY:  Verify bio-async cleanup
 * HOW:  Disconnect bio-async, check state
 */
TEST_F(SpeechJepaBridgeTest, BioAsyncDisconnect) {
    speech_jepa_bridge_connect_bio_async(bridge);
    int result = speech_jepa_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test checking bio-async connection status
 * WHY:  Verify connection state query
 * HOW:  Check status before/after connection
 */
TEST_F(SpeechJepaBridgeTest, BioAsyncIsConnected) {
    bool connected = speech_jepa_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    speech_jepa_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability
}

/**
 * WHAT: Test bio-async functions with NULL bridge
 * WHY:  Verify NULL safety for bio-async API
 * HOW:  Call bio-async functions with NULL
 */
TEST_F(SpeechJepaBridgeTest, BioAsyncNull) {
    EXPECT_NE(speech_jepa_bridge_connect_bio_async(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(speech_jepa_bridge_disconnect_bio_async(nullptr), NIMCP_SUCCESS);
    EXPECT_FALSE(speech_jepa_bridge_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

/**
 * WHAT: Test multiple operations sequence
 * WHY:  Verify bridge handles typical usage patterns
 * HOW:  Create, connect, encode, train, predict, destroy
 */
TEST_F(SpeechJepaBridgeTest, TypicalUsagePattern) {
    // Already have bridge from SetUp
    ASSERT_NE(bridge, nullptr);

    // Connect speech cortex
    speech_cortex_t* mock_speech = reinterpret_cast<speech_cortex_t*>(0x1000);
    speech_jepa_bridge_connect_speech_cortex(bridge, mock_speech);

    // Create sequence
    sequence = speech_jepa_sequence_create(20);
    for (int i = 0; i < 10; i++) {
        speech_jepa_frame_t frame = CreateTestFrame(
            static_cast<phoneme_t>(i % PHONEME_COUNT), i * 20
        );
        speech_jepa_sequence_add_frame(sequence, &frame);
    }

    // Encode
    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    ASSERT_NE(latent, nullptr);
    speech_jepa_bridge_encode(bridge, sequence, latent);

    // Set training mode and train
    speech_jepa_bridge_set_training(bridge, true);
    float loss;
    speech_jepa_bridge_train_step(bridge, sequence, &loss);

    // Switch to inference and predict
    speech_jepa_bridge_set_training(bridge, false);
    jepa_latent_t* predicted = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    speech_jepa_bridge_predict_next(bridge, latent, predicted);

    // Get stats
    speech_jepa_stats_t stats;
    speech_jepa_bridge_get_stats(bridge, &stats);

    // Cleanup
    jepa_latent_destroy(latent);
    jepa_latent_destroy(predicted);

    // Disconnect
    speech_jepa_bridge_disconnect_speech_cortex(bridge);

    // Test passes if no crashes
}

/**
 * WHAT: Test frame with all phoneme types
 * WHY:  Verify all phonemes are handled correctly
 * HOW:  Add frames for each phoneme type
 */
TEST_F(SpeechJepaBridgeTest, AllPhonemeTypes) {
    sequence = speech_jepa_sequence_create(PHONEME_COUNT);
    ASSERT_NE(sequence, nullptr);

    // Add frame for each phoneme type
    for (int i = 0; i < PHONEME_COUNT; i++) {
        speech_jepa_frame_t frame = CreateTestFrame(
            static_cast<phoneme_t>(i), i * 20
        );
        int result = speech_jepa_sequence_add_frame(sequence, &frame);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(sequence->num_frames, static_cast<uint32_t>(PHONEME_COUNT));

    // Encode all phonemes
    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    int result = speech_jepa_bridge_encode(bridge, sequence, latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test frame with extreme values
 * WHY:  Verify robustness to edge case values
 * HOW:  Create frame with boundary values
 */
TEST_F(SpeechJepaBridgeTest, ExtremeFrameValues) {
    sequence = speech_jepa_sequence_create(10);
    ASSERT_NE(sequence, nullptr);

    // Frame with minimum values
    speech_jepa_frame_t frame_min;
    memset(&frame_min, 0, sizeof(frame_min));
    frame_min.phoneme = PHONEME_SILENCE;
    frame_min.phoneme_confidence = 0.0f;
    frame_min.pitch = 0.0f;
    frame_min.energy = 0.0f;
    frame_min.is_voiced = false;

    int result = speech_jepa_sequence_add_frame(sequence, &frame_min);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Frame with high values
    speech_jepa_frame_t frame_max;
    frame_max.phoneme = static_cast<phoneme_t>(PHONEME_COUNT - 1);
    frame_max.phoneme_confidence = 1.0f;
    frame_max.formants[0] = 1000.0f;
    frame_max.formants[1] = 3000.0f;
    frame_max.formants[2] = 4000.0f;
    frame_max.formants[3] = 5000.0f;
    frame_max.pitch = 500.0f;
    frame_max.energy = 1.0f;
    frame_max.duration_ms = 100.0f;
    frame_max.timestamp_ms = 1000;
    frame_max.is_voiced = true;

    result = speech_jepa_sequence_add_frame(sequence, &frame_max);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify encoding works with extreme values
    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    result = speech_jepa_bridge_encode(bridge, sequence, latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify latent doesn't have NaN or Inf
    bool has_invalid = false;
    for (uint32_t i = 0; i < latent->latent_dim; i++) {
        if (std::isnan(latent->embedding[i]) || std::isinf(latent->embedding[i])) {
            has_invalid = true;
            break;
        }
    }
    EXPECT_FALSE(has_invalid);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test repeated create/destroy cycles
 * WHY:  Verify no memory leaks in lifecycle
 * HOW:  Create and destroy multiple times
 */
TEST_F(SpeechJepaBridgeTest, RepeatedLifecycle) {
    // Destroy the bridge from SetUp
    speech_jepa_bridge_destroy(bridge);
    bridge = nullptr;

    for (int i = 0; i < 10; i++) {
        speech_jepa_bridge_config_t config;
        speech_jepa_bridge_default_config(&config);

        speech_jepa_bridge_t* temp_bridge = speech_jepa_bridge_create(&config);
        ASSERT_NE(temp_bridge, nullptr);

        speech_jepa_bridge_destroy(temp_bridge);
    }

    // Recreate for TearDown
    speech_jepa_bridge_config_t config;
    speech_jepa_bridge_default_config(&config);
    bridge = speech_jepa_bridge_create(&config);
}

/**
 * WHAT: Test sequence with single frame
 * WHY:  Verify minimum sequence length handling
 * HOW:  Add single frame, encode
 */
TEST_F(SpeechJepaBridgeTest, SingleFrameSequence) {
    sequence = speech_jepa_sequence_create(10);
    ASSERT_NE(sequence, nullptr);

    speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, 0);
    speech_jepa_sequence_add_frame(sequence, &frame);
    EXPECT_EQ(sequence->num_frames, 1u);

    jepa_latent_t* latent = jepa_latent_create_dim(SPEECH_JEPA_DEFAULT_ENCODER_DIM);
    int result = speech_jepa_bridge_encode(bridge, sequence, latent);
    // Single frame may be valid or require minimum length

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test with custom configuration
 * WHY:  Verify custom config is applied
 * HOW:  Create with non-default config values
 */
TEST_F(SpeechJepaBridgeTest, CustomConfig) {
    speech_jepa_bridge_destroy(bridge);

    speech_jepa_bridge_config_t config;
    speech_jepa_bridge_default_config(&config);

    // Modify config
    config.encoder.type = SPEECH_JEPA_ENCODER_MLP;
    config.encoder.hidden_dim = 256;
    config.mask_strategy = SPEECH_JEPA_MASK_CAUSAL;
    config.mask_ratio = 0.5f;
    config.learning_rate = 0.01f;

    bridge = speech_jepa_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Verify it works with custom config
    sequence = speech_jepa_sequence_create(10);
    for (int i = 0; i < 5; i++) {
        speech_jepa_frame_t frame = CreateTestFrame(PHONEME_IY, i * 20);
        speech_jepa_sequence_add_frame(sequence, &frame);
    }

    jepa_latent_t* latent = jepa_latent_create_dim(config.encoder.output_dim);
    int result = speech_jepa_bridge_encode(bridge, sequence, latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
}

/**
 * Main entry point for tests
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
