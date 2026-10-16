/**
 * @file test_omni_wm_plasticity_bridge.cpp
 * @brief Comprehensive unit tests for World Model Plasticity Bridge
 *
 * WHAT: Tests for WM-Plasticity/STDP bidirectional bridge
 * WHY:  Plasticity bridge enables closed-loop learning between WM and SNN/STDP systems
 * HOW:  Tests all APIs: lifecycle, connections, STDP events, STDP modulation,
 *       spike training, SNN prediction, BCM integration, eligibility, and edge cases
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_plasticity_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr uint32_t TEST_PRE_NEURON_ID = 10;
static constexpr uint32_t TEST_POST_NEURON_ID = 20;
static constexpr uint32_t TEST_HORIZON_MS = 50;
static constexpr uint32_t TEST_NEURON_COUNT = 16;

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

// Helper to create a test STDP event
static wm_stdp_event_t create_test_stdp_event(uint32_t pre_id, uint32_t post_id, bool is_ltp)
{
    wm_stdp_event_t event;
    memset(&event, 0, sizeof(event));
    event.pre_neuron_id = pre_id;
    event.post_neuron_id = post_id;
    event.weight_change = is_ltp ? 0.01f : -0.005f;
    event.pre_post_dt_ms = is_ltp ? 10.0f : -10.0f;  // LTP: post after pre
    event.is_ltp = is_ltp;
    event.timestamp_us = 1000000;  // 1 second
    return event;
}

// Helper to create a test spike sequence
static wm_spike_sequence_t create_test_spike_sequence(uint32_t neuron_count, uint32_t spike_count)
{
    wm_spike_sequence_t seq;
    memset(&seq, 0, sizeof(seq));
    seq.neuron_count = neuron_count;
    seq.spike_count = spike_count;
    seq.sequence_duration_ms = 100.0f;
    seq.start_timestamp_us = 0;
    seq.is_spontaneous = false;
    // Note: neuron_ids and spike_times_ms would need to be allocated in real use
    return seq;
}

// =============================================================================
// Test Fixture
// =============================================================================

class OmniWmPlasticityBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create bridge with default config for most tests
        bridge_ = omni_wm_plasticity_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_plasticity_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    // Helper to create config with custom settings
    omni_wm_plasticity_bridge_config_t create_custom_config()
    {
        omni_wm_plasticity_bridge_config_t config;
        omni_wm_plasticity_bridge_default_config(&config);
        config.enable_modulation = true;
        config.sensitivity = 1.5f;
        config.enable_stdp_to_wm = true;
        config.enable_wm_to_stdp = true;
        config.enable_spike_training = true;
        config.enable_prediction_guidance = true;
        return config;
    }

    omni_wm_plasticity_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Default Config Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, DefaultConfigBasic)
{
    omni_wm_plasticity_bridge_config_t config;
    nimcp_error_t result = omni_wm_plasticity_bridge_default_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_plasticity_bridge_config_t config;
    ASSERT_EQ(omni_wm_plasticity_bridge_default_config(&config), NIMCP_SUCCESS);

    // Check sensitivity is in valid range
    EXPECT_GE(config.sensitivity, 0.5f);
    EXPECT_LE(config.sensitivity, 2.0f);

    // Check learning rates are reasonable
    EXPECT_GT(config.encoder_learning_rate, 0.0f);
    EXPECT_LE(config.encoder_learning_rate, 1.0f);

    EXPECT_GT(config.spike_sequence_learning_rate, 0.0f);
    EXPECT_LE(config.spike_sequence_learning_rate, 1.0f);

    // Check thresholds
    EXPECT_GE(config.pe_threshold_low, 0.0f);
    EXPECT_GE(config.pe_threshold_high, config.pe_threshold_low);
}

TEST_F(OmniWmPlasticityBridgeTest, ValidateConfigValidConfig)
{
    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);

    nimcp_error_t result = omni_wm_plasticity_bridge_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 2. Lifecycle Tests - Create
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, CreateWithNullConfig)
{
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(OmniWmPlasticityBridgeTest, CreateWithCustomConfig)
{
    omni_wm_plasticity_bridge_config_t config = create_custom_config();

    omni_wm_plasticity_bridge_t* bridge = omni_wm_plasticity_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Verify config was applied
    EXPECT_TRUE(bridge->config.enable_modulation);
    EXPECT_FLOAT_EQ(bridge->config.sensitivity, 1.5f);
    EXPECT_TRUE(bridge->config.enable_stdp_to_wm);
    EXPECT_TRUE(bridge->config.enable_wm_to_stdp);

    omni_wm_plasticity_bridge_destroy(bridge);
}

TEST_F(OmniWmPlasticityBridgeTest, CreateInitializesBaseFields)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->base.module_id, BIO_MODULE_WM_PLASTICITY_BRIDGE);
    EXPECT_NE(bridge_->base.module_name, nullptr);
    EXPECT_FALSE(bridge_->base.bridge_active);
}

TEST_F(OmniWmPlasticityBridgeTest, CreateInitializesBuffers)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->stdp_event_count, 0u);
    EXPECT_EQ(bridge_->spike_seq_count, 0u);
    EXPECT_FALSE(bridge_->snn_prediction_valid);
}

TEST_F(OmniWmPlasticityBridgeTest, CreateInitializesStats)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->stats.stdp_events_received, 0u);
    EXPECT_EQ(bridge_->stats.stdp_events_applied, 0u);
    EXPECT_EQ(bridge_->stats.spike_sequences_received, 0u);
    EXPECT_EQ(bridge_->stats.predictions_generated, 0u);
    EXPECT_EQ(bridge_->stats.total_updates, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

TEST_F(OmniWmPlasticityBridgeTest, CreateInitializesEffects)
{
    ASSERT_NE(bridge_, nullptr);

    // Plasticity -> WM effects
    EXPECT_EQ(bridge_->plasticity_to_wm.pending_stdp_events, 0u);
    EXPECT_FLOAT_EQ(bridge_->plasticity_to_wm.total_weight_delta, 0.0f);
    EXPECT_FALSE(bridge_->plasticity_to_wm.spike_sequence_available);

    // WM -> Plasticity effects
    EXPECT_FLOAT_EQ(bridge_->wm_to_plasticity.forward_pe, 0.0f);
    EXPECT_FALSE(bridge_->wm_to_plasticity.modulation_pending);
    EXPECT_FALSE(bridge_->wm_to_plasticity.prediction_available);
}

// =============================================================================
// 3. Lifecycle Tests - Destroy
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, DestroyNullSafe)
{
    omni_wm_plasticity_bridge_destroy(nullptr);
}

TEST_F(OmniWmPlasticityBridgeTest, DestroyValidBridge)
{
    omni_wm_plasticity_bridge_t* bridge = omni_wm_plasticity_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    omni_wm_plasticity_bridge_destroy(bridge);
}

// =============================================================================
// 4. Lifecycle Tests - Reset
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, ResetBasic)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, ResetClearsStats)
{
    bridge_->stats.stdp_events_received = 100;
    bridge_->stats.stdp_events_applied = 80;
    bridge_->stats.errors_total = 5;
    bridge_->stats.total_updates = 50;

    nimcp_error_t result = omni_wm_plasticity_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.stdp_events_received, 0u);
    EXPECT_EQ(bridge_->stats.stdp_events_applied, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
    EXPECT_EQ(bridge_->stats.total_updates, 0u);
}

TEST_F(OmniWmPlasticityBridgeTest, ResetClearsBuffers)
{
    bridge_->stdp_event_count = 10;
    bridge_->spike_seq_count = 5;
    bridge_->snn_prediction_valid = true;

    nimcp_error_t result = omni_wm_plasticity_bridge_reset(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stdp_event_count, 0u);
    EXPECT_EQ(bridge_->spike_seq_count, 0u);
    EXPECT_FALSE(bridge_->snn_prediction_valid);
}

TEST_F(OmniWmPlasticityBridgeTest, ResetPreservesConfig)
{
    bridge_->config.sensitivity = 1.75f;
    bridge_->config.enable_stdp_to_wm = true;

    omni_wm_plasticity_bridge_reset(bridge_);

    EXPECT_FLOAT_EQ(bridge_->config.sensitivity, 1.75f);
    EXPECT_TRUE(bridge_->config.enable_stdp_to_wm);
}

// =============================================================================
// 5. Connection Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, ConnectNullBridgeFails)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);

    nimcp_error_t result = omni_wm_plasticity_bridge_connect(
        nullptr, dummy_wm, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, ConnectNullWorldModelFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_connect(
        bridge_, nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, ConnectWorldModelOnly)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);

    nimcp_error_t result = omni_wm_plasticity_bridge_connect(
        bridge_, dummy_wm, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->world_model, dummy_wm);
    EXPECT_TRUE(omni_wm_plasticity_bridge_is_connected(bridge_));
}

TEST_F(OmniWmPlasticityBridgeTest, ConnectAllSystems)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    stdp_omni_bridge_t dummy_stdp = reinterpret_cast<stdp_omni_bridge_t>(0x5678);
    plasticity_coordinator_t* dummy_coord = reinterpret_cast<plasticity_coordinator_t*>(0x9ABC);
    neural_network_t dummy_snn = reinterpret_cast<neural_network_t>(0xDEF0);

    nimcp_error_t result = omni_wm_plasticity_bridge_connect(
        bridge_, dummy_wm, dummy_stdp, dummy_coord, dummy_snn);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->world_model, dummy_wm);
    EXPECT_EQ(bridge_->stdp_bridge, dummy_stdp);
    EXPECT_EQ(bridge_->coordinator, dummy_coord);
    EXPECT_EQ(bridge_->snn, dummy_snn);
}

TEST_F(OmniWmPlasticityBridgeTest, ConnectWorldModelSeparate)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);

    nimcp_error_t result = omni_wm_plasticity_bridge_connect_world_model(bridge_, dummy_wm);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->world_model, dummy_wm);
}

TEST_F(OmniWmPlasticityBridgeTest, ConnectStdpBridgeSeparate)
{
    stdp_omni_bridge_t dummy_stdp = reinterpret_cast<stdp_omni_bridge_t>(0x5678);

    nimcp_error_t result = omni_wm_plasticity_bridge_connect_stdp_bridge(bridge_, dummy_stdp);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->stdp_bridge, dummy_stdp);
}

TEST_F(OmniWmPlasticityBridgeTest, ConnectCoordinatorSeparate)
{
    plasticity_coordinator_t* dummy_coord = reinterpret_cast<plasticity_coordinator_t*>(0x9ABC);

    nimcp_error_t result = omni_wm_plasticity_bridge_connect_coordinator(bridge_, dummy_coord);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->coordinator, dummy_coord);
}

TEST_F(OmniWmPlasticityBridgeTest, ConnectSnnSeparate)
{
    neural_network_t dummy_snn = reinterpret_cast<neural_network_t>(0xDEF0);

    nimcp_error_t result = omni_wm_plasticity_bridge_connect_snn(bridge_, dummy_snn);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->snn, dummy_snn);
}

TEST_F(OmniWmPlasticityBridgeTest, IsConnectedWithNoConnection)
{
    EXPECT_FALSE(omni_wm_plasticity_bridge_is_connected(bridge_));
}

TEST_F(OmniWmPlasticityBridgeTest, IsConnectedNullFalse)
{
    EXPECT_FALSE(omni_wm_plasticity_bridge_is_connected(nullptr));
}

// =============================================================================
// 6. Update Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_update(nullptr, 0.016f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, UpdateWithoutConnectionHandled)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_update(bridge_, 0.016f);
    // Should handle gracefully without crash
}

TEST_F(OmniWmPlasticityBridgeTest, UpdateWithZeroDt)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    omni_wm_plasticity_bridge_connect(bridge_, dummy_wm, nullptr, nullptr, nullptr);

    nimcp_error_t result = omni_wm_plasticity_bridge_update(bridge_, 0.0f);
    // Should handle zero dt gracefully
}

TEST_F(OmniWmPlasticityBridgeTest, UpdateWithNegativeDt)
{
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    omni_wm_plasticity_bridge_connect(bridge_, dummy_wm, nullptr, nullptr, nullptr);

    nimcp_error_t result = omni_wm_plasticity_bridge_update(bridge_, -0.016f);
    // Should handle negative dt
}

// =============================================================================
// 7. STDP Event Tests (on_stdp_event)
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, OnStdpEventNullBridgeFails)
{
    wm_stdp_event_t event = create_test_stdp_event(TEST_PRE_NEURON_ID, TEST_POST_NEURON_ID, true);

    nimcp_error_t result = omni_wm_plasticity_bridge_on_stdp_event(nullptr, &event);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, OnStdpEventNullEventFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_on_stdp_event(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, OnStdpEventLTPEvent)
{
    wm_stdp_event_t event = create_test_stdp_event(TEST_PRE_NEURON_ID, TEST_POST_NEURON_ID, true);

    nimcp_error_t result = omni_wm_plasticity_bridge_on_stdp_event(bridge_, &event);
    // Without connection may succeed or fail depending on implementation
}

TEST_F(OmniWmPlasticityBridgeTest, OnStdpEventLTDEvent)
{
    wm_stdp_event_t event = create_test_stdp_event(TEST_PRE_NEURON_ID, TEST_POST_NEURON_ID, false);

    nimcp_error_t result = omni_wm_plasticity_bridge_on_stdp_event(bridge_, &event);
    // Should handle LTD event
}

TEST_F(OmniWmPlasticityBridgeTest, OnStdpEventZeroWeightChange)
{
    wm_stdp_event_t event = create_test_stdp_event(TEST_PRE_NEURON_ID, TEST_POST_NEURON_ID, true);
    event.weight_change = 0.0f;

    nimcp_error_t result = omni_wm_plasticity_bridge_on_stdp_event(bridge_, &event);
    // Zero weight change should be handled
}

TEST_F(OmniWmPlasticityBridgeTest, OnStdpEventLargeWeightChange)
{
    wm_stdp_event_t event = create_test_stdp_event(TEST_PRE_NEURON_ID, TEST_POST_NEURON_ID, true);
    event.weight_change = 1.0f;  // Very large

    nimcp_error_t result = omni_wm_plasticity_bridge_on_stdp_event(bridge_, &event);
    // Large weight change should be handled - may be clamped
}

TEST_F(OmniWmPlasticityBridgeTest, ApplyStdpToRssmNullBridgeFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_apply_stdp_to_rssm(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, ApplyStdpToRssmEmptyBuffer)
{
    // With empty buffer, should succeed but do nothing
    nimcp_error_t result = omni_wm_plasticity_bridge_apply_stdp_to_rssm(bridge_);
    // May succeed or indicate no events to apply
}

// =============================================================================
// 8. STDP Modulation Tests (get_stdp_modulation)
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, GetStdpModulationNullBridgeFails)
{
    wm_to_plasticity_modulation_t modulation;

    nimcp_error_t result = omni_wm_plasticity_bridge_get_stdp_modulation(nullptr, &modulation);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, GetStdpModulationNullOutputFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_get_stdp_modulation(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, GetStdpModulationBasic)
{
    wm_to_plasticity_modulation_t modulation;
    memset(&modulation, 0, sizeof(modulation));

    nimcp_error_t result = omni_wm_plasticity_bridge_get_stdp_modulation(bridge_, &modulation);
    // May succeed or fail depending on WM connection
}

TEST_F(OmniWmPlasticityBridgeTest, SetPredictionErrorNullBridgeFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_set_prediction_error(
        nullptr, 0.5f, 0.3f, 0.2f, 0.8f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, SetPredictionErrorBasic)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_set_prediction_error(
        bridge_, 0.5f, 0.3f, 0.2f, 0.8f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify PE values are stored
    EXPECT_FLOAT_EQ(bridge_->wm_to_plasticity.forward_pe, 0.5f);
    EXPECT_FLOAT_EQ(bridge_->wm_to_plasticity.backward_pe, 0.3f);
    EXPECT_FLOAT_EQ(bridge_->wm_to_plasticity.lateral_pe, 0.2f);
}

TEST_F(OmniWmPlasticityBridgeTest, SetPredictionErrorZeroPE)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_set_prediction_error(
        bridge_, 0.0f, 0.0f, 0.0f, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, SetPredictionErrorHighPE)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_set_prediction_error(
        bridge_, 1.0f, 1.0f, 1.0f, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, SetPredictionErrorZeroPrecision)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_set_prediction_error(
        bridge_, 0.5f, 0.3f, 0.2f, 0.0f);
    // Zero precision should be handled
}

TEST_F(OmniWmPlasticityBridgeTest, SetPredictionErrorNegativePE)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_set_prediction_error(
        bridge_, -0.5f, -0.3f, -0.2f, 0.8f);
    // Negative PE should be handled - may be clamped or treated as error
}

// =============================================================================
// 9. Spike Training Tests (train_from_spikes)
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, TrainFromSpikesNullBridgeFails)
{
    wm_spike_sequence_t sequence = create_test_spike_sequence(TEST_NEURON_COUNT, 50);

    nimcp_error_t result = omni_wm_plasticity_bridge_train_from_spikes(nullptr, &sequence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, TrainFromSpikesNullSequenceFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_train_from_spikes(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, TrainFromSpikesEmptySequence)
{
    wm_spike_sequence_t sequence;
    memset(&sequence, 0, sizeof(sequence));
    sequence.neuron_count = 0;
    sequence.spike_count = 0;

    nimcp_error_t result = omni_wm_plasticity_bridge_train_from_spikes(bridge_, &sequence);
    // Empty sequence handling - may be accepted or rejected
}

TEST_F(OmniWmPlasticityBridgeTest, TrainFromSpikesZeroNeurons)
{
    wm_spike_sequence_t sequence = create_test_spike_sequence(0, 10);

    nimcp_error_t result = omni_wm_plasticity_bridge_train_from_spikes(bridge_, &sequence);
    // Zero neurons should be handled
}

TEST_F(OmniWmPlasticityBridgeTest, TrainFromSpikesZeroSpikes)
{
    wm_spike_sequence_t sequence = create_test_spike_sequence(TEST_NEURON_COUNT, 0);

    nimcp_error_t result = omni_wm_plasticity_bridge_train_from_spikes(bridge_, &sequence);
    // Zero spikes should be handled
}

TEST_F(OmniWmPlasticityBridgeTest, TrainFromSpikesMaxSequenceLength)
{
    wm_spike_sequence_t sequence = create_test_spike_sequence(
        TEST_NEURON_COUNT, WM_PLASTICITY_MAX_SPIKE_SEQ_LENGTH);

    nimcp_error_t result = omni_wm_plasticity_bridge_train_from_spikes(bridge_, &sequence);
    // Max length should be handled
}

TEST_F(OmniWmPlasticityBridgeTest, TrainFromSpikesExceedsMaxLength)
{
    wm_spike_sequence_t sequence = create_test_spike_sequence(
        TEST_NEURON_COUNT, WM_PLASTICITY_MAX_SPIKE_SEQ_LENGTH + 100);

    nimcp_error_t result = omni_wm_plasticity_bridge_train_from_spikes(bridge_, &sequence);
    // Exceeding max should be handled - may truncate or error
}

// =============================================================================
// 10. SNN Activity Prediction Tests (predict_snn_activity)
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, PredictSnnActivityNullBridgeFails)
{
    wm_to_snn_prediction_t prediction;

    nimcp_error_t result = omni_wm_plasticity_bridge_predict_snn_activity(
        nullptr, TEST_HORIZON_MS, &prediction);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, PredictSnnActivityNullOutputFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_predict_snn_activity(
        bridge_, TEST_HORIZON_MS, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, PredictSnnActivityWithoutConnectionHandled)
{
    wm_to_snn_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    nimcp_error_t result = omni_wm_plasticity_bridge_predict_snn_activity(
        bridge_, TEST_HORIZON_MS, &prediction);
    // Should handle gracefully without WM connection
}

TEST_F(OmniWmPlasticityBridgeTest, PredictSnnActivityZeroHorizon)
{
    wm_to_snn_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    nimcp_error_t result = omni_wm_plasticity_bridge_predict_snn_activity(
        bridge_, 0, &prediction);
    // Zero horizon handling
}

TEST_F(OmniWmPlasticityBridgeTest, PredictSnnActivityLargeHorizon)
{
    wm_to_snn_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    nimcp_error_t result = omni_wm_plasticity_bridge_predict_snn_activity(
        bridge_, 10000, &prediction);
    // Large horizon should be handled - may be clamped
}

// =============================================================================
// 11. BCM Integration Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, OnBcmThresholdShiftNullBridgeFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_on_bcm_threshold_shift(nullptr, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, OnBcmThresholdShiftBasic)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_on_bcm_threshold_shift(bridge_, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify threshold is stored
    EXPECT_FLOAT_EQ(bridge_->current_plasticity_state.bcm_threshold, 0.5f);
}

TEST_F(OmniWmPlasticityBridgeTest, OnBcmThresholdShiftZero)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_on_bcm_threshold_shift(bridge_, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, OnBcmThresholdShiftNegative)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_on_bcm_threshold_shift(bridge_, -0.5f);
    // Negative threshold handling - implementation dependent
}

TEST_F(OmniWmPlasticityBridgeTest, OnBcmThresholdShiftLarge)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_on_bcm_threshold_shift(bridge_, 10.0f);
    // Large threshold handling - may be clamped
}

// =============================================================================
// 12. Eligibility Trace Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, ApplyEligibilityNullBridgeFails)
{
    float traces[10] = {0};

    nimcp_error_t result = omni_wm_plasticity_bridge_apply_eligibility(
        nullptr, traces, 10, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, ApplyEligibilityNullTracesFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_apply_eligibility(
        bridge_, nullptr, 10, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, ApplyEligibilityZeroCount)
{
    float traces[1] = {0};

    nimcp_error_t result = omni_wm_plasticity_bridge_apply_eligibility(
        bridge_, traces, 0, 0.5f);
    // Zero count handling
}

TEST_F(OmniWmPlasticityBridgeTest, ApplyEligibilityPositiveReward)
{
    float traces[10];
    for (int i = 0; i < 10; i++) {
        traces[i] = 0.1f * (float)i;
    }

    nimcp_error_t result = omni_wm_plasticity_bridge_apply_eligibility(
        bridge_, traces, 10, 1.0f);
    // Positive reward handling
}

TEST_F(OmniWmPlasticityBridgeTest, ApplyEligibilityNegativeReward)
{
    float traces[10];
    for (int i = 0; i < 10; i++) {
        traces[i] = 0.1f * (float)i;
    }

    nimcp_error_t result = omni_wm_plasticity_bridge_apply_eligibility(
        bridge_, traces, 10, -1.0f);
    // Negative reward handling
}

TEST_F(OmniWmPlasticityBridgeTest, ApplyEligibilityZeroReward)
{
    float traces[10];
    for (int i = 0; i < 10; i++) {
        traces[i] = 0.1f * (float)i;
    }

    nimcp_error_t result = omni_wm_plasticity_bridge_apply_eligibility(
        bridge_, traces, 10, 0.0f);
    // Zero reward should result in no update
}

// =============================================================================
// 13. STP State Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, UpdateStpStateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_update_stp_state(
        nullptr, 1.2f, 0.8f, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, UpdateStpStateBasic)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_update_stp_state(
        bridge_, 1.2f, 0.8f, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify values are stored
    EXPECT_FLOAT_EQ(bridge_->current_plasticity_state.stp_facilitation, 1.2f);
    EXPECT_FLOAT_EQ(bridge_->current_plasticity_state.stp_depression, 0.8f);
    EXPECT_FLOAT_EQ(bridge_->current_plasticity_state.stp_avg_utilization, 0.5f);
}

TEST_F(OmniWmPlasticityBridgeTest, UpdateStpStateFacilitationOnly)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_update_stp_state(
        bridge_, 2.0f, 1.0f, 0.5f);  // High facilitation, no depression
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, UpdateStpStateDepressionOnly)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_update_stp_state(
        bridge_, 1.0f, 0.5f, 0.5f);  // No facilitation, depression
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

// =============================================================================
// 14. Query API Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, GetPlasticityStateNullBridgeFails)
{
    plasticity_to_wm_state_t state;

    nimcp_error_t result = omni_wm_plasticity_bridge_get_plasticity_state(nullptr, &state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, GetPlasticityStateNullOutputFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_get_plasticity_state(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, GetPlasticityStateBasic)
{
    plasticity_to_wm_state_t state;
    memset(&state, 0xFF, sizeof(state));

    nimcp_error_t result = omni_wm_plasticity_bridge_get_plasticity_state(bridge_, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, GetPlasticityEffectsBasic)
{
    const plasticity_to_omni_wm_effects_t* effects =
        omni_wm_plasticity_bridge_get_plasticity_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmPlasticityBridgeTest, GetPlasticityEffectsNullBridge)
{
    const plasticity_to_omni_wm_effects_t* effects =
        omni_wm_plasticity_bridge_get_plasticity_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(OmniWmPlasticityBridgeTest, GetWmEffectsBasic)
{
    const omni_wm_to_plasticity_effects_t* effects =
        omni_wm_plasticity_bridge_get_wm_effects(bridge_);
    EXPECT_NE(effects, nullptr);
}

TEST_F(OmniWmPlasticityBridgeTest, GetWmEffectsNullBridge)
{
    const omni_wm_to_plasticity_effects_t* effects =
        omni_wm_plasticity_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

// =============================================================================
// 15. Statistics Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, GetStatsBasic)
{
    omni_wm_plasticity_bridge_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    nimcp_error_t result = omni_wm_plasticity_bridge_get_stats(bridge_, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(OmniWmPlasticityBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_plasticity_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_plasticity_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, GetStatsNullOutputFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, ResetStatsBasic)
{
    bridge_->stats.stdp_events_received = 100;
    bridge_->stats.errors_total = 5;

    nimcp_error_t result = omni_wm_plasticity_bridge_reset_stats(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.stdp_events_received, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

TEST_F(OmniWmPlasticityBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 16. Bio-Async Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, ConnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, DisconnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_plasticity_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmPlasticityBridgeTest, IsBioAsyncConnectedNullFalse)
{
    EXPECT_FALSE(omni_wm_plasticity_bridge_is_bio_async_connected(nullptr));
}

TEST_F(OmniWmPlasticityBridgeTest, IsBioAsyncConnectedInitiallyFalse)
{
    EXPECT_FALSE(omni_wm_plasticity_bridge_is_bio_async_connected(bridge_));
}

// =============================================================================
// 17. Memory Safety Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, MultipleCreateDestroy)
{
    for (int i = 0; i < 10; i++) {
        omni_wm_plasticity_bridge_t* b = omni_wm_plasticity_bridge_create(nullptr);
        ASSERT_NE(b, nullptr);
        omni_wm_plasticity_bridge_destroy(b);
    }
}

TEST_F(OmniWmPlasticityBridgeTest, MutexIsInitialized)
{
    ASSERT_NE(bridge_, nullptr);
    EXPECT_NE(bridge_->mutex, nullptr);
}

TEST_F(OmniWmPlasticityBridgeTest, ConfigAllFeaturesEnabled)
{
    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);

    config.enable_modulation = true;
    config.enable_stdp_to_wm = true;
    config.enable_wm_to_stdp = true;
    config.enable_spike_training = true;
    config.enable_prediction_guidance = true;
    config.enable_bcm_integration = true;
    config.enable_eligibility_integration = true;
    config.enable_stp_integration = true;
    config.enable_coordinator_sync = true;
    config.enable_bio_async = true;

    omni_wm_plasticity_bridge_t* bridge = omni_wm_plasticity_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_plasticity_bridge_destroy(bridge);
}

TEST_F(OmniWmPlasticityBridgeTest, ConfigAllFeaturesDisabled)
{
    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);

    config.enable_modulation = false;
    config.enable_stdp_to_wm = false;
    config.enable_wm_to_stdp = false;
    config.enable_spike_training = false;
    config.enable_prediction_guidance = false;
    config.enable_bcm_integration = false;
    config.enable_eligibility_integration = false;
    config.enable_stp_integration = false;
    config.enable_coordinator_sync = false;
    config.enable_bio_async = false;

    omni_wm_plasticity_bridge_t* bridge = omni_wm_plasticity_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    omni_wm_plasticity_bridge_destroy(bridge);
}

// =============================================================================
// 18. Config Edge Cases
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, ConfigSensitivityBelowMin)
{
    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);
    config.sensitivity = 0.1f;

    omni_wm_plasticity_bridge_t* bridge = omni_wm_plasticity_bridge_create(&config);
    if (bridge) {
        omni_wm_plasticity_bridge_destroy(bridge);
    }
}

TEST_F(OmniWmPlasticityBridgeTest, ConfigSensitivityAboveMax)
{
    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);
    config.sensitivity = 5.0f;

    omni_wm_plasticity_bridge_t* bridge = omni_wm_plasticity_bridge_create(&config);
    if (bridge) {
        omni_wm_plasticity_bridge_destroy(bridge);
    }
}

TEST_F(OmniWmPlasticityBridgeTest, ConfigZeroLearningRate)
{
    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);
    config.encoder_learning_rate = 0.0f;
    config.spike_sequence_learning_rate = 0.0f;

    omni_wm_plasticity_bridge_t* bridge = omni_wm_plasticity_bridge_create(&config);
    // Zero learning rate should be allowed
    if (bridge) {
        omni_wm_plasticity_bridge_destroy(bridge);
    }
}

TEST_F(OmniWmPlasticityBridgeTest, ConfigHighLearningRate)
{
    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);
    config.encoder_learning_rate = 1.0f;
    config.spike_sequence_learning_rate = 1.0f;

    omni_wm_plasticity_bridge_t* bridge = omni_wm_plasticity_bridge_create(&config);
    if (bridge) {
        omni_wm_plasticity_bridge_destroy(bridge);
    }
}

// =============================================================================
// 19. Integration Scenario Tests
// =============================================================================

TEST_F(OmniWmPlasticityBridgeTest, StdpToModulationFlow)
{
    // Simulate STDP event -> PE -> modulation flow
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    omni_wm_plasticity_bridge_connect(bridge_, dummy_wm, nullptr, nullptr, nullptr);

    // Send STDP event
    wm_stdp_event_t event = create_test_stdp_event(TEST_PRE_NEURON_ID, TEST_POST_NEURON_ID, true);
    omni_wm_plasticity_bridge_on_stdp_event(bridge_, &event);

    // Set PE (simulating WM feedback)
    omni_wm_plasticity_bridge_set_prediction_error(bridge_, 0.5f, 0.3f, 0.2f, 0.8f);

    // Get modulation
    wm_to_plasticity_modulation_t modulation;
    omni_wm_plasticity_bridge_get_stdp_modulation(bridge_, &modulation);

    // Flow should work without crash
}

TEST_F(OmniWmPlasticityBridgeTest, SpikeTrainPredictFlow)
{
    // Simulate spike training -> prediction flow
    omni_world_model_t* dummy_wm = reinterpret_cast<omni_world_model_t*>(0x1234);
    omni_wm_plasticity_bridge_connect(bridge_, dummy_wm, nullptr, nullptr, nullptr);

    // Train from spikes
    wm_spike_sequence_t sequence = create_test_spike_sequence(TEST_NEURON_COUNT, 50);
    omni_wm_plasticity_bridge_train_from_spikes(bridge_, &sequence);

    // Get prediction
    wm_to_snn_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    omni_wm_plasticity_bridge_predict_snn_activity(bridge_, TEST_HORIZON_MS, &prediction);

    // Flow should work without crash
}

// Main function for standalone execution
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
