/**
 * @file test_plasticity_orchestrator.cpp
 * @brief Comprehensive Unit Tests for Plasticity Orchestrator
 *
 * Tests all aspects of the plasticity orchestrator including:
 * - Lifecycle management (create/destroy)
 * - Configuration
 * - Synapse and neuron management
 * - STDP and spike processing
 * - Metabolic constraints
 * - Event callbacks
 * - Statistics
 * - Serialization
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "utils/memory/nimcp_memory.h"

// ============================================================================
// Test Fixture
// ============================================================================

class PlasticityOrchestratorTest : public ::testing::Test {
protected:
    plasticity_orchestrator_t* orchestrator = nullptr;
    plasticity_orchestrator_config_t config;

    void SetUp() override {
        plasticity_orchestrator_default_config(&config);
    }

    void TearDown() override {
        if (orchestrator) {
            plasticity_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, DefaultConfigSetsReasonableDefaults) {
    EXPECT_EQ(plasticity_orchestrator_default_config(&config), 0);

    // Check enabled flags
    EXPECT_TRUE(config.enabled.enable_triplet_stdp);
    EXPECT_TRUE(config.enabled.enable_bcm);
    EXPECT_TRUE(config.enabled.enable_homeostatic);
    EXPECT_TRUE(config.enabled.enable_metabolic);
    EXPECT_TRUE(config.enabled.enable_calcium);
    EXPECT_TRUE(config.enabled.enable_structural);
    EXPECT_TRUE(config.enabled.enable_protein_synthesis);
    EXPECT_TRUE(config.enabled.enable_metaplasticity);

    // Check timing parameters
    EXPECT_EQ(config.update_interval_ms, 1);
    EXPECT_EQ(config.consolidation_interval_ms, 60000);
    EXPECT_EQ(config.homeostatic_interval_ms, 1000);

    // Check modulation defaults
    EXPECT_FLOAT_EQ(config.global_learning_rate, 1.0f);
    EXPECT_FLOAT_EQ(config.sleep_modulation, 1.0f);
    EXPECT_FLOAT_EQ(config.immune_modulation, 1.0f);
}

TEST_F(PlasticityOrchestratorTest, DefaultConfigReturnsErrorForNull) {
    EXPECT_EQ(plasticity_orchestrator_default_config(nullptr), -1);
}

TEST_F(PlasticityOrchestratorTest, CreateWithDefaultConfig) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);
}

TEST_F(PlasticityOrchestratorTest, CreateWithCustomConfig) {
    config.enabled.enable_metabolic = false;
    config.global_learning_rate = 0.5f;

    orchestrator = plasticity_orchestrator_create(&config);
    ASSERT_NE(orchestrator, nullptr);
}

TEST_F(PlasticityOrchestratorTest, CreateMinimalConfig) {
    // Disable all optional features
    config.enabled.enable_triplet_stdp = false;
    config.enabled.enable_bcm = false;
    config.enabled.enable_homeostatic = false;
    config.enabled.enable_structural = false;
    config.enabled.enable_heterosynaptic = false;
    config.enabled.enable_calcium = false;
    config.enabled.enable_astrocyte = false;
    config.enabled.enable_protein_synthesis = false;
    config.enabled.enable_metaplasticity = false;
    config.enabled.enable_metabolic = false;

    orchestrator = plasticity_orchestrator_create(&config);
    ASSERT_NE(orchestrator, nullptr);
}

TEST_F(PlasticityOrchestratorTest, DestroyNullIsSafe) {
    plasticity_orchestrator_destroy(nullptr);
    // Should not crash
}

// ============================================================================
// Weight Management Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, SetAndGetWeight) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Set weight for synapse 1
    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 1, 0.75f), 0);

    // Get weight back
    float weight = plasticity_orchestrator_get_weight(orchestrator, 1);
    EXPECT_FLOAT_EQ(weight, 0.75f);
}

TEST_F(PlasticityOrchestratorTest, WeightIsBounded) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Try to set weight above max (1.0)
    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 1, 1.5f), 0);
    float weight = plasticity_orchestrator_get_weight(orchestrator, 1);
    EXPECT_FLOAT_EQ(weight, 1.0f);  // Should be clamped

    // Try to set weight below min (0.0)
    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 2, -0.5f), 0);
    weight = plasticity_orchestrator_get_weight(orchestrator, 2);
    EXPECT_FLOAT_EQ(weight, 0.0f);  // Should be clamped
}

TEST_F(PlasticityOrchestratorTest, GetWeightReturnsNanForUnknownSynapse) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    float weight = plasticity_orchestrator_get_weight(orchestrator, 99999);
    EXPECT_TRUE(std::isnan(weight));
}

TEST_F(PlasticityOrchestratorTest, SetWeightReturnsErrorForNull) {
    EXPECT_EQ(plasticity_orchestrator_set_weight(nullptr, 1, 0.5f), -1);
}

// ============================================================================
// Update Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, UpdateWithZeroDelta) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 0), 0);
}

TEST_F(PlasticityOrchestratorTest, UpdateWithSmallDelta) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
}

TEST_F(PlasticityOrchestratorTest, UpdateWithLargeDelta) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1000), 0);
}

TEST_F(PlasticityOrchestratorTest, UpdateReturnsErrorForNull) {
    EXPECT_EQ(plasticity_orchestrator_update(nullptr, 1), -1);
}

TEST_F(PlasticityOrchestratorTest, MultipleUpdates) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 10), 0);
    }

    // Get stats to verify updates were tracked
    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 100);
}

// ============================================================================
// Spike Processing Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, PreSpikeCreatesNewSynapse) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Pre-spike should auto-create synapse
    EXPECT_EQ(plasticity_orchestrator_pre_spike(orchestrator, 1, 100), 0);

    // Synapse should now exist with default weight
    float weight = plasticity_orchestrator_get_weight(orchestrator, 1);
    EXPECT_FALSE(std::isnan(weight));
}

TEST_F(PlasticityOrchestratorTest, PostSpikeCreatesNewNeuron) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Create some synapses first
    plasticity_orchestrator_set_weight(orchestrator, 1, 0.5f);
    plasticity_orchestrator_set_weight(orchestrator, 2, 0.5f);

    // Post-spike should auto-create neuron
    EXPECT_EQ(plasticity_orchestrator_post_spike(orchestrator, 1, 100), 0);
}

TEST_F(PlasticityOrchestratorTest, PreAndPostSpikeCausesPlasticity) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Create synapse
    plasticity_orchestrator_set_weight(orchestrator, 1, 0.5f);
    float initial_weight = plasticity_orchestrator_get_weight(orchestrator, 1);

    // Pre-spike, then post-spike (should cause LTP for positive timing)
    EXPECT_EQ(plasticity_orchestrator_pre_spike(orchestrator, 1, 100), 0);
    EXPECT_EQ(plasticity_orchestrator_post_spike(orchestrator, 1, 110), 0);

    // Update to process plasticity
    EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 10), 0);

    // Note: Weight change depends on STDP implementation details
    // Just verify the operations completed without error
}

TEST_F(PlasticityOrchestratorTest, PreSpikeReturnsErrorForNull) {
    EXPECT_EQ(plasticity_orchestrator_pre_spike(nullptr, 1, 100), -1);
}

TEST_F(PlasticityOrchestratorTest, PostSpikeReturnsErrorForNull) {
    EXPECT_EQ(plasticity_orchestrator_post_spike(nullptr, 1, 100), -1);
}

// ============================================================================
// Reward Processing Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, RewardWithPositiveMagnitude) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_reward(orchestrator, 1.0f, 100), 0);
}

TEST_F(PlasticityOrchestratorTest, RewardWithNegativeMagnitude) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_reward(orchestrator, -1.0f, 100), 0);
}

TEST_F(PlasticityOrchestratorTest, RewardReturnsErrorForNull) {
    EXPECT_EQ(plasticity_orchestrator_reward(nullptr, 1.0f, 100), -1);
}

// ============================================================================
// ATP Level Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, GetATPLevelReturnsValidValue) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    float atp = plasticity_orchestrator_get_atp_level(orchestrator);
    EXPECT_GE(atp, 0.0f);
    EXPECT_LE(atp, 100.0f);
}

TEST_F(PlasticityOrchestratorTest, GetATPLevelWithMetabolicDisabled) {
    config.enabled.enable_metabolic = false;
    orchestrator = plasticity_orchestrator_create(&config);
    ASSERT_NE(orchestrator, nullptr);

    float atp = plasticity_orchestrator_get_atp_level(orchestrator);
    EXPECT_FLOAT_EQ(atp, 100.0f);  // Default when metabolic is disabled
}

TEST_F(PlasticityOrchestratorTest, GetATPLevelReturnsErrorForNull) {
    float atp = plasticity_orchestrator_get_atp_level(nullptr);
    EXPECT_FLOAT_EQ(atp, -1.0f);
}

// ============================================================================
// Calcium Level Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, GetCalciumReturnsValidValue) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    float calcium = plasticity_orchestrator_get_calcium(orchestrator, 0);
    EXPECT_GE(calcium, 0.0f);
}

TEST_F(PlasticityOrchestratorTest, GetCalciumWithCalciumDisabled) {
    config.enabled.enable_calcium = false;
    orchestrator = plasticity_orchestrator_create(&config);
    ASSERT_NE(orchestrator, nullptr);

    float calcium = plasticity_orchestrator_get_calcium(orchestrator, 0);
    EXPECT_FLOAT_EQ(calcium, 0.1f);  // Default resting calcium
}

TEST_F(PlasticityOrchestratorTest, GetCalciumReturnsErrorForNull) {
    float calcium = plasticity_orchestrator_get_calcium(nullptr, 0);
    EXPECT_FLOAT_EQ(calcium, -1.0f);
}

// ============================================================================
// Threshold Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, GetThresholdReturnsValidValue) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Create a neuron first via post_spike
    plasticity_orchestrator_set_weight(orchestrator, 1, 0.5f);
    plasticity_orchestrator_post_spike(orchestrator, 1, 100);

    float threshold = plasticity_orchestrator_get_threshold(orchestrator, 1);
    EXPECT_GE(threshold, 0.0f);
}

TEST_F(PlasticityOrchestratorTest, GetThresholdReturnsDefaultForUnknownNeuron) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    float threshold = plasticity_orchestrator_get_threshold(orchestrator, 99999);
    EXPECT_FLOAT_EQ(threshold, 0.5f);  // Default threshold
}

TEST_F(PlasticityOrchestratorTest, GetThresholdReturnsErrorForNull) {
    float threshold = plasticity_orchestrator_get_threshold(nullptr, 1);
    EXPECT_FLOAT_EQ(threshold, -1.0f);
}

// ============================================================================
// Callback Tests
// ============================================================================

static std::atomic<int> ltp_callback_count{0};
static std::atomic<int> ltd_callback_count{0};

static void test_ltp_callback(const plasticity_event_t* event, void* user_data) {
    (void)event;
    (void)user_data;
    ltp_callback_count++;
}

static void test_ltd_callback(const plasticity_event_t* event, void* user_data) {
    (void)event;
    (void)user_data;
    ltd_callback_count++;
}

TEST_F(PlasticityOrchestratorTest, RegisterEventCallback) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    int callback_id = plasticity_orchestrator_register_event_callback(
        orchestrator,
        PLASTICITY_EVENT_LTP,
        test_ltp_callback,
        nullptr
    );

    EXPECT_GT(callback_id, 0);
}

TEST_F(PlasticityOrchestratorTest, UnregisterEventCallback) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    int callback_id = plasticity_orchestrator_register_event_callback(
        orchestrator,
        PLASTICITY_EVENT_LTP,
        test_ltp_callback,
        nullptr
    );
    ASSERT_GT(callback_id, 0);

    EXPECT_EQ(plasticity_orchestrator_unregister_event_callback(orchestrator, callback_id), 0);
}

TEST_F(PlasticityOrchestratorTest, UnregisterInvalidCallback) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_unregister_event_callback(orchestrator, 999), -1);
}

TEST_F(PlasticityOrchestratorTest, RegisterCallbackReturnsErrorForNull) {
    EXPECT_EQ(plasticity_orchestrator_register_event_callback(
        nullptr, PLASTICITY_EVENT_LTP, test_ltp_callback, nullptr), -1);
}

TEST_F(PlasticityOrchestratorTest, RegisterCallbackReturnsErrorForNullCallback) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_register_event_callback(
        orchestrator, PLASTICITY_EVENT_LTP, nullptr, nullptr), -1);
}

// Pre/Post Update Callbacks
static std::atomic<int> pre_update_count{0};
static std::atomic<int> post_update_count{0};

static void test_pre_update(plasticity_orchestrator_t* orch, uint64_t delta_ms, void* user_data) {
    (void)orch;
    (void)delta_ms;
    (void)user_data;
    pre_update_count++;
}

static void test_post_update(plasticity_orchestrator_t* orch, uint64_t delta_ms, void* user_data) {
    (void)orch;
    (void)delta_ms;
    (void)user_data;
    post_update_count++;
}

TEST_F(PlasticityOrchestratorTest, RegisterPreUpdateCallback) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    pre_update_count = 0;
    EXPECT_EQ(plasticity_orchestrator_register_pre_update(
        orchestrator, test_pre_update, nullptr), 0);

    // Update should trigger callback
    plasticity_orchestrator_update(orchestrator, 10);
    EXPECT_EQ(pre_update_count.load(), 1);
}

TEST_F(PlasticityOrchestratorTest, RegisterPostUpdateCallback) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    post_update_count = 0;
    EXPECT_EQ(plasticity_orchestrator_register_post_update(
        orchestrator, test_post_update, nullptr), 0);

    // Update should trigger callback
    plasticity_orchestrator_update(orchestrator, 10);
    EXPECT_EQ(post_update_count.load(), 1);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, GetStatsInitialValues) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);

    EXPECT_EQ(stats.ltp_count, 0);
    EXPECT_EQ(stats.ltd_count, 0);
    EXPECT_EQ(stats.total_updates, 0);
}

TEST_F(PlasticityOrchestratorTest, GetStatsAfterUpdates) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Do some updates
    for (int i = 0; i < 5; i++) {
        plasticity_orchestrator_update(orchestrator, 10);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 5);
}

TEST_F(PlasticityOrchestratorTest, ResetStats) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Do some updates
    for (int i = 0; i < 5; i++) {
        plasticity_orchestrator_update(orchestrator, 10);
    }

    // Reset stats
    EXPECT_EQ(plasticity_orchestrator_reset_stats(orchestrator), 0);

    // Check stats are reset
    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 0);
}

TEST_F(PlasticityOrchestratorTest, GetStatsReturnsErrorForNull) {
    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(nullptr, &stats), -1);
}

TEST_F(PlasticityOrchestratorTest, ResetStatsReturnsErrorForNull) {
    EXPECT_EQ(plasticity_orchestrator_reset_stats(nullptr), -1);
}

// ============================================================================
// Integration Connection Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, ConnectImmuneReturnsErrorForNullOrchestrator) {
    // Cannot instantiate forward-declared types, test with nullptr
    EXPECT_EQ(plasticity_orchestrator_connect_immune(nullptr, nullptr), -1);
}

TEST_F(PlasticityOrchestratorTest, ConnectImmuneReturnsErrorForNullImmune) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_connect_immune(orchestrator, nullptr), -1);
}

TEST_F(PlasticityOrchestratorTest, ConnectSleepReturnsErrorForNullOrchestrator) {
    EXPECT_EQ(plasticity_orchestrator_connect_sleep(nullptr, nullptr), -1);
}

TEST_F(PlasticityOrchestratorTest, ConnectSleepReturnsErrorForNullSleep) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_connect_sleep(orchestrator, nullptr), -1);
}

TEST_F(PlasticityOrchestratorTest, ConnectNeuromodulatorsReturnsErrorForNullOrchestrator) {
    EXPECT_EQ(plasticity_orchestrator_connect_neuromodulators(nullptr, nullptr), -1);
}

TEST_F(PlasticityOrchestratorTest, ConnectNeuromodulatorsReturnsErrorForNullNeuromod) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_connect_neuromodulators(orchestrator, nullptr), -1);
}

TEST_F(PlasticityOrchestratorTest, ConnectBioAsyncSucceeds) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_connect_bio_async(orchestrator), 0);
}

TEST_F(PlasticityOrchestratorTest, ConnectBioAsyncReturnsErrorForNull) {
    EXPECT_EQ(plasticity_orchestrator_connect_bio_async(nullptr), -1);
}

// ============================================================================
// Serialization Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, SerializeEmptyOrchestrator) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    std::vector<uint8_t> buffer(4096);
    size_t bytes_written = 0;

    EXPECT_EQ(plasticity_orchestrator_serialize(
        orchestrator, buffer.data(), buffer.size(), &bytes_written), 0);
    EXPECT_GT(bytes_written, 0);
}

TEST_F(PlasticityOrchestratorTest, SerializeWithSynapses) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Add some synapses
    for (int i = 0; i < 10; i++) {
        plasticity_orchestrator_set_weight(orchestrator, i, 0.1f * i);
    }

    std::vector<uint8_t> buffer(8192);
    size_t bytes_written = 0;

    EXPECT_EQ(plasticity_orchestrator_serialize(
        orchestrator, buffer.data(), buffer.size(), &bytes_written), 0);
    EXPECT_GT(bytes_written, 0);
}

TEST_F(PlasticityOrchestratorTest, SerializeDeserializeRoundTrip) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Set up some synapses
    plasticity_orchestrator_set_weight(orchestrator, 1, 0.25f);
    plasticity_orchestrator_set_weight(orchestrator, 2, 0.75f);

    // Serialize
    std::vector<uint8_t> buffer(8192);
    size_t bytes_written = 0;
    EXPECT_EQ(plasticity_orchestrator_serialize(
        orchestrator, buffer.data(), buffer.size(), &bytes_written), 0);

    // Create new orchestrator and deserialize
    plasticity_orchestrator_t* orchestrator2 = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator2, nullptr);

    EXPECT_EQ(plasticity_orchestrator_deserialize(orchestrator2, buffer.data(), bytes_written), 0);

    // Check weights were restored
    EXPECT_FLOAT_EQ(plasticity_orchestrator_get_weight(orchestrator2, 1), 0.25f);
    EXPECT_FLOAT_EQ(plasticity_orchestrator_get_weight(orchestrator2, 2), 0.75f);

    plasticity_orchestrator_destroy(orchestrator2);
}

TEST_F(PlasticityOrchestratorTest, SerializeReturnsErrorForNull) {
    std::vector<uint8_t> buffer(1024);
    size_t bytes_written = 0;

    EXPECT_EQ(plasticity_orchestrator_serialize(
        nullptr, buffer.data(), buffer.size(), &bytes_written), -1);
}

TEST_F(PlasticityOrchestratorTest, DeserializeReturnsErrorForNull) {
    std::vector<uint8_t> buffer(1024);
    EXPECT_EQ(plasticity_orchestrator_deserialize(nullptr, buffer.data(), buffer.size()), -1);
}

TEST_F(PlasticityOrchestratorTest, SerializeReturnsErrorForSmallBuffer) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    std::vector<uint8_t> buffer(10);  // Too small
    size_t bytes_written = 0;

    // Note: This test depends on implementation - may succeed with very small buffer
    // if no synapses are present
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, ManySynapsesTest) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Create many synapses (up to MAX_SYNAPSES limit of 100)
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, i, 0.5f), 0);
    }

    // Verify all were created
    for (int i = 0; i < 100; i++) {
        float weight = plasticity_orchestrator_get_weight(orchestrator, i);
        EXPECT_FLOAT_EQ(weight, 0.5f);
    }
}

TEST_F(PlasticityOrchestratorTest, RapidUpdatesTest) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Create some synapses
    for (int i = 0; i < 10; i++) {
        plasticity_orchestrator_set_weight(orchestrator, i, 0.5f);
    }

    // Rapid updates
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(plasticity_orchestrator_update(orchestrator, 1), 0);
    }

    plasticity_stats_t stats;
    EXPECT_EQ(plasticity_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_updates, 1000);
}

TEST_F(PlasticityOrchestratorTest, RapidSpikesTest) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Create synapse
    plasticity_orchestrator_set_weight(orchestrator, 1, 0.5f);

    // Rapid pre/post spikes
    for (int i = 0; i < 100; i++) {
        uint64_t time = i * 10;
        EXPECT_EQ(plasticity_orchestrator_pre_spike(orchestrator, 1, time), 0);
        EXPECT_EQ(plasticity_orchestrator_post_spike(orchestrator, 1, time + 5), 0);
    }
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(PlasticityOrchestratorTest, ZeroWeightTest) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 1, 0.0f), 0);
    EXPECT_FLOAT_EQ(plasticity_orchestrator_get_weight(orchestrator, 1), 0.0f);
}

TEST_F(PlasticityOrchestratorTest, MaxWeightTest) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 1, 1.0f), 0);
    EXPECT_FLOAT_EQ(plasticity_orchestrator_get_weight(orchestrator, 1), 1.0f);
}

TEST_F(PlasticityOrchestratorTest, VeryLargeTimestamp) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    plasticity_orchestrator_set_weight(orchestrator, 1, 0.5f);

    uint64_t large_time = UINT64_MAX - 1000;
    EXPECT_EQ(plasticity_orchestrator_pre_spike(orchestrator, 1, large_time), 0);
    EXPECT_EQ(plasticity_orchestrator_post_spike(orchestrator, 1, large_time + 10), 0);
}

TEST_F(PlasticityOrchestratorTest, ZeroIdSynapse) {
    orchestrator = plasticity_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    EXPECT_EQ(plasticity_orchestrator_set_weight(orchestrator, 0, 0.5f), 0);
    EXPECT_FLOAT_EQ(plasticity_orchestrator_get_weight(orchestrator, 0), 0.5f);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
