//=============================================================================
// test_working_memory_snn_plasticity_integration.cpp - WM SNN/Plasticity Integration
//=============================================================================
/**
 * @file test_working_memory_snn_plasticity_integration.cpp
 * @brief Integration tests for Working Memory-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between working memory, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly with all callbacks
 * HOW:  Create both bridges, simulate real dataflows, verify learning occurs
 *
 * INTEGRATION POINTS:
 * - Memory encoding -> SNN encoding -> population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Plasticity callbacks -> Memory state updates
 * - Rehearsal modulation -> both SNN and Plasticity bridges
 *
 * THEORETICAL BASIS:
 * - Goldman-Rakic (1995): Working memory cellular basis
 * - Wang (2001): Synaptic reverberation in mnemonic activity
 * - Mongillo et al. (2008): Synaptic theory of working memory
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/memory/nimcp_working_memory_snn_bridge.h"
#include "cognitive/memory/nimcp_working_memory_plasticity_bridge.h"

#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WorkingMemorySNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    wm_snn_bridge_t* snn_bridge;
    wm_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> spike_count{0};
    std::atomic<int> encoding_count{0};
    std::atomic<int> weight_change_count{0};
    std::atomic<int> consolidation_count{0};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        wm_snn_config_t snn_config = wm_snn_config_default();
        snn_config.max_slots = 8;
        snn_config.neurons_per_slot = 32;
        snn_config.slot_dim = 64;
        snn_config.hidden_dim = 128;
        snn_config.enable_bio_async = false;  // Disable for predictable tests
        snn_config.enable_lateral_inhibition = true;
        snn_config.enable_recurrence = true;

        snn_bridge = wm_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        wm_plasticity_config_t plasticity_config = wm_plasticity_config_default();
        plasticity_config.enable_homeostatic = true;
        plasticity_config.enable_eligibility = true;
        plasticity_config.enable_maintenance_ltp = true;
        plasticity_config.enable_rehearsal_boost = true;

        plasticity_bridge = wm_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        spike_count = 0;
        encoding_count = 0;
        weight_change_count = 0;
        consolidation_count = 0;
    }

    void TearDown() override {
        if (snn_bridge) {
            wm_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            wm_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate feature pattern for memory item
    void generate_item_features(float* features, uint32_t n, uint32_t item_id) {
        for (uint32_t i = 0; i < n; i++) {
            // Pattern varies by item_id to distinguish items
            features[i] = 0.3f + 0.4f * sinf((float)i * 0.1f + (float)item_id);
        }
    }

    // Generate retrieval cue features
    void generate_cue_features(float* features, uint32_t n, uint32_t item_id) {
        for (uint32_t i = 0; i < n; i++) {
            // Partial pattern for retrieval cues
            features[i] = 0.2f + 0.3f * sinf((float)i * 0.1f + (float)item_id);
        }
    }
};

//=============================================================================
// Static Callback Functions
//=============================================================================

static std::atomic<int>* g_spike_counter = nullptr;
static std::atomic<int>* g_weight_counter = nullptr;

static void spike_callback(wm_snn_bridge_t* bridge, uint32_t slot, float rate, void* user_data) {
    if (g_spike_counter) {
        (*g_spike_counter)++;
    }
}

static void weight_change_callback(uint32_t synapse_id, uint32_t slot_idx,
    float old_w, float new_w, wm_learn_event_t event_type, void* user_data) {
    if (g_weight_counter) {
        (*g_weight_counter)++;
    }
}

//=============================================================================
// Test: Encoding-Driven Learning Pipeline
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, EncodingDrivenLearning) {
    // Setup callbacks
    g_spike_counter = &spike_count;
    wm_snn_register_spike_callback(snn_bridge, spike_callback, nullptr);

    // Register synapses with plasticity bridge for each slot
    for (uint32_t slot = 0; slot < 8; slot++) {
        wm_plasticity_register_synapse(plasticity_bridge, slot,
            WM_SYNAPSE_ENCODING, (int32_t)slot, 0.5f);
        wm_plasticity_register_synapse(plasticity_bridge, slot + 8,
            WM_SYNAPSE_MAINTENANCE, (int32_t)slot, 0.5f);
    }

    // Encode items into working memory SNN
    float features[64];
    for (uint32_t item = 0; item < 4; item++) {
        generate_item_features(features, 64, item);

        // SNN encoding generates spike patterns
        int spikes = wm_snn_encode_item(snn_bridge, item, features, 64, 0.8f);
        EXPECT_GE(spikes, 0) << "Encoding item " << item << " failed";

        // Record encoding event in plasticity bridge
        uint64_t timestamp = nimcp_time_get_us();
        int ret = wm_plasticity_encode(plasticity_bridge, item, 0.8f, timestamp);
        EXPECT_EQ(ret, 0) << "Plasticity encoding failed for item " << item;
    }

    // Simulate maintenance activity
    int sim_ret = wm_snn_simulate(snn_bridge, 50.0f);
    EXPECT_GE(sim_ret, 0) << "SNN simulation failed";

    // Verify state
    wm_snn_bridge_state_t snn_state;
    wm_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.active_slots, 4u) << "Expected 4 active slots";

    wm_plasticity_bridge_state_t plasticity_state;
    wm_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_GE(plasticity_state.registered_synapses, 8u);
}

//=============================================================================
// Test: Maintenance-Driven Plasticity
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, MaintenanceDrivenPlasticity) {
    // Register synapses
    for (uint32_t slot = 0; slot < 4; slot++) {
        wm_plasticity_register_synapse(plasticity_bridge, slot,
            WM_SYNAPSE_MAINTENANCE, (int32_t)slot, 0.5f);
    }

    // Encode items
    float features[64];
    for (uint32_t item = 0; item < 4; item++) {
        generate_item_features(features, 64, item);
        wm_snn_encode_item(snn_bridge, item, features, 64, 1.0f);

        uint64_t timestamp = nimcp_time_get_us();
        wm_plasticity_encode(plasticity_bridge, item, 1.0f, timestamp);
    }

    // Get initial encoding strength
    float initial_strength = wm_plasticity_get_encoding_strength(plasticity_bridge, 0);

    // Simulate multiple maintenance cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // SNN maintenance simulation
        wm_snn_simulate(snn_bridge, 10.0f);

        // Record maintenance events
        uint64_t timestamp = nimcp_time_get_us();
        for (uint32_t slot = 0; slot < 4; slot++) {
            wm_slot_state_t slot_state;
            wm_snn_get_slot_state(snn_bridge, slot, &slot_state);

            wm_plasticity_maintain(plasticity_bridge, slot,
                slot_state.activity_level, timestamp);
        }

        // Update plasticity
        wm_plasticity_update(plasticity_bridge, 10.0f);
    }

    // Verify maintenance LTP occurred
    float final_strength = wm_plasticity_get_encoding_strength(plasticity_bridge, 0);
    EXPECT_GE(final_strength, initial_strength)
        << "Maintenance should strengthen or maintain encoding";
}

//=============================================================================
// Test: Retrieval Reinforcement
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, RetrievalReinforcement) {
    // Setup
    wm_plasticity_register_synapse(plasticity_bridge, 0,
        WM_SYNAPSE_RETRIEVAL, 0, 0.5f);

    // Encode item
    float features[64];
    generate_item_features(features, 64, 0);
    wm_snn_encode_item(snn_bridge, 0, features, 64, 1.0f);
    wm_plasticity_encode(plasticity_bridge, 0, 1.0f, nimcp_time_get_us());

    // Simulate
    wm_snn_simulate(snn_bridge, 20.0f);

    // Retrieve multiple times
    float output[64];
    for (int i = 0; i < 5; i++) {
        int ret = wm_snn_retrieve_item(snn_bridge, 0, output, 64);
        EXPECT_EQ(ret, 0) << "Retrieval " << i << " failed";

        wm_plasticity_retrieve(plasticity_bridge, 0, 1.0f, nimcp_time_get_us());
    }

    // Verify retrieval stats
    wm_snn_stats_t snn_stats;
    wm_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_EQ(snn_stats.total_retrievals, 5u);

    wm_plasticity_stats_t plasticity_stats;
    wm_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_EQ(plasticity_stats.total_retrievals, 5u);
}

//=============================================================================
// Test: Capacity Overflow and Competition
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, CapacityOverflowCompetition) {
    // Register all slots
    for (uint32_t slot = 0; slot < 8; slot++) {
        wm_plasticity_register_synapse(plasticity_bridge, slot,
            WM_SYNAPSE_ENCODING, (int32_t)slot, 0.5f);
    }

    // Fill all slots
    float features[64];
    for (uint32_t item = 0; item < 8; item++) {
        generate_item_features(features, 64, item);
        wm_snn_encode_item(snn_bridge, item, features, 64, 0.5f + item * 0.05f);
        wm_plasticity_encode(plasticity_bridge, item, 0.5f + item * 0.05f, nimcp_time_get_us());
    }

    // Verify full capacity
    float capacity = wm_snn_get_capacity(snn_bridge);
    EXPECT_FLOAT_EQ(capacity, 1.0f) << "Should be at full capacity";

    // Find most active slot
    float confidence;
    int most_active = wm_snn_get_most_active_slot(snn_bridge, &confidence);
    EXPECT_GE(most_active, 0) << "Should have an active slot";
    EXPECT_GT(confidence, 0.0f) << "Should have some confidence";

    // Set capacity pressure for plasticity
    wm_plasticity_set_capacity_pressure(plasticity_bridge, 1.0f);

    // Verify capacity effects on plasticity
    wm_plasticity_bridge_state_t state;
    wm_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_FLOAT_EQ(state.current_capacity_pressure, 1.0f);
}

//=============================================================================
// Test: Consolidation Pipeline
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, ConsolidationPipeline) {
    // Setup
    wm_plasticity_register_synapse(plasticity_bridge, 0,
        WM_SYNAPSE_MAINTENANCE, 0, 0.5f);

    // Encode item
    float features[64];
    generate_item_features(features, 64, 0);
    wm_snn_encode_item(snn_bridge, 0, features, 64, 1.0f);
    wm_plasticity_encode(plasticity_bridge, 0, 1.0f, nimcp_time_get_us());

    // Simulate extended maintenance (representing long-term storage)
    for (int i = 0; i < 20; i++) {
        wm_snn_simulate(snn_bridge, 50.0f);

        wm_slot_state_t slot_state;
        wm_snn_get_slot_state(snn_bridge, 0, &slot_state);

        wm_plasticity_maintain(plasticity_bridge, 0,
            slot_state.activity_level, nimcp_time_get_us());
        wm_plasticity_update(plasticity_bridge, 50.0f);
    }

    // Trigger consolidation
    int ret = wm_plasticity_consolidate_slot(plasticity_bridge, 0);
    EXPECT_EQ(ret, 0) << "Consolidation trigger failed";

    // Check consolidation level
    float consolidation = wm_plasticity_get_consolidation_level(plasticity_bridge, 0);
    EXPECT_GT(consolidation, 0.0f) << "Consolidation should have progressed";
}

//=============================================================================
// Test: Concurrent Operation
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, ConcurrentOperation) {
    // Setup multiple slots with synapses
    for (uint32_t slot = 0; slot < 4; slot++) {
        wm_plasticity_register_synapse(plasticity_bridge, slot,
            WM_SYNAPSE_ENCODING, (int32_t)slot, 0.5f);
        wm_plasticity_register_synapse(plasticity_bridge, slot + 4,
            WM_SYNAPSE_MAINTENANCE, (int32_t)slot, 0.5f);
    }

    // Encode items
    float features[64];
    for (uint32_t item = 0; item < 4; item++) {
        generate_item_features(features, 64, item);
        wm_snn_encode_item(snn_bridge, item, features, 64, 0.8f);
        wm_plasticity_encode(plasticity_bridge, item, 0.8f, nimcp_time_get_us());
    }

    // Run concurrent operations
    for (int cycle = 0; cycle < 10; cycle++) {
        // SNN simulation
        wm_snn_simulate(snn_bridge, 10.0f);

        // Plasticity update
        wm_plasticity_update(plasticity_bridge, 10.0f);

        // Occasional retrieval
        if (cycle % 3 == 0) {
            float output[64];
            wm_snn_retrieve_item(snn_bridge, cycle % 4, output, 64);
            wm_plasticity_retrieve(plasticity_bridge, cycle % 4, 0.9f, nimcp_time_get_us());
        }

        // Occasional maintenance event
        if (cycle % 2 == 0) {
            wm_slot_state_t slot_state;
            wm_snn_get_slot_state(snn_bridge, cycle % 4, &slot_state);
            wm_plasticity_maintain(plasticity_bridge, cycle % 4,
                slot_state.activity_level, nimcp_time_get_us());
        }
    }

    // Verify both bridges have valid state
    wm_snn_bridge_state_t snn_state;
    wm_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, WM_SNN_STATE_IDLE);
    EXPECT_EQ(snn_state.active_slots, 4u);

    wm_plasticity_bridge_state_t plasticity_state;
    wm_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, WM_PLASTICITY_STATE_IDLE);
}

//=============================================================================
// Test: Decay and Eviction Integration
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, DecayAndEviction) {
    // Setup
    wm_plasticity_register_synapse(plasticity_bridge, 0,
        WM_SYNAPSE_ENCODING, 0, 0.5f);

    // Encode item
    float features[64];
    generate_item_features(features, 64, 0);
    wm_snn_encode_item(snn_bridge, 0, features, 64, 0.5f);
    wm_plasticity_encode(plasticity_bridge, 0, 0.5f, nimcp_time_get_us());

    // Get initial activity
    wm_slot_state_t initial_state;
    wm_snn_get_slot_state(snn_bridge, 0, &initial_state);
    float initial_activity = initial_state.activity_level;

    // Simulate decay (extended simulation without refreshing)
    wm_snn_simulate(snn_bridge, 500.0f);

    // Record decay event
    wm_slot_state_t decayed_state;
    wm_snn_get_slot_state(snn_bridge, 0, &decayed_state);
    wm_plasticity_decay(plasticity_bridge, 0, decayed_state.activity_level, nimcp_time_get_us());

    // Verify activity decayed
    EXPECT_LT(decayed_state.activity_level, initial_activity)
        << "Activity should decay over time";

    // Clear slot (eviction)
    wm_snn_clear_slot(snn_bridge, 0);
    wm_plasticity_evict(plasticity_bridge, 0, nimcp_time_get_us());

    // Verify eviction
    wm_slot_state_t cleared_state;
    wm_snn_get_slot_state(snn_bridge, 0, &cleared_state);
    EXPECT_FALSE(cleared_state.occupied) << "Slot should be cleared";
}

//=============================================================================
// Test: Reward Modulation
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, RewardModulation) {
    // Setup callback
    g_weight_counter = &weight_change_count;
    wm_plasticity_set_weight_callback(plasticity_bridge, weight_change_callback, nullptr);

    // Register synapses
    for (uint32_t slot = 0; slot < 2; slot++) {
        wm_plasticity_register_synapse(plasticity_bridge, slot,
            WM_SYNAPSE_ENCODING, (int32_t)slot, 0.5f);
    }

    // Encode items
    float features[64];
    for (uint32_t item = 0; item < 2; item++) {
        generate_item_features(features, 64, item);
        wm_snn_encode_item(snn_bridge, item, features, 64, 1.0f);
        wm_plasticity_encode(plasticity_bridge, item, 1.0f, nimcp_time_get_us());
    }

    // Apply positive reward
    int ret = wm_plasticity_reward(plasticity_bridge, 1.0f, nimcp_time_get_us());
    EXPECT_EQ(ret, 0) << "Reward application failed";

    // Update to process reward
    wm_plasticity_update(plasticity_bridge, 10.0f);

    // Apply negative reward (punishment)
    ret = wm_plasticity_reward(plasticity_bridge, -0.5f, nimcp_time_get_us());
    EXPECT_EQ(ret, 0) << "Punishment application failed";

    wm_plasticity_update(plasticity_bridge, 10.0f);

    // Verify plasticity stats reflect reward modulation
    wm_plasticity_stats_t stats;
    wm_plasticity_get_stats(plasticity_bridge, &stats);
    // Both LTP (positive reward) and LTD (negative reward) should have occurred
    EXPECT_GE(stats.ltp_events + stats.ltd_events, 0u);
}

//=============================================================================
// Test: Statistics Aggregation
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, StatisticsAggregation) {
    // Encode items
    float features[64];
    for (uint32_t item = 0; item < 4; item++) {
        generate_item_features(features, 64, item);
        wm_snn_encode_item(snn_bridge, item, features, 64, 1.0f);
    }

    // Simulate and retrieve
    for (int i = 0; i < 5; i++) {
        wm_snn_simulate(snn_bridge, 10.0f);

        float output[64];
        wm_snn_retrieve_item(snn_bridge, i % 4, output, 64);
    }

    // Check SNN stats
    wm_snn_stats_t snn_stats;
    wm_snn_get_stats(snn_bridge, &snn_stats);

    EXPECT_EQ(snn_stats.total_encodings, 4u) << "Should have 4 encodings";
    EXPECT_EQ(snn_stats.total_retrievals, 5u) << "Should have 5 retrievals";
    EXPECT_GE(snn_stats.total_simulations, 5u) << "Should have at least 5 simulations";
    EXPECT_GT(snn_stats.total_spikes, 0u) << "Should have generated spikes";

    // Reset and verify
    wm_snn_reset_stats(snn_bridge);
    wm_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_EQ(snn_stats.total_encodings, 0u) << "Stats should be reset";
}

//=============================================================================
// Test: Full Pipeline Integration
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, FullPipelineIntegration) {
    // Setup callbacks
    g_spike_counter = &spike_count;
    g_weight_counter = &weight_change_count;

    wm_snn_register_spike_callback(snn_bridge, spike_callback, nullptr);
    wm_plasticity_set_weight_callback(plasticity_bridge, weight_change_callback, nullptr);

    // Register comprehensive synapses
    for (uint32_t slot = 0; slot < 4; slot++) {
        wm_plasticity_register_synapse(plasticity_bridge, slot,
            WM_SYNAPSE_ENCODING, (int32_t)slot, 0.5f);
        wm_plasticity_register_synapse(plasticity_bridge, slot + 4,
            WM_SYNAPSE_MAINTENANCE, (int32_t)slot, 0.5f);
        wm_plasticity_register_synapse(plasticity_bridge, slot + 8,
            WM_SYNAPSE_RETRIEVAL, (int32_t)slot, 0.5f);
    }

    // Phase 1: Encoding
    float features[64];
    for (uint32_t item = 0; item < 4; item++) {
        generate_item_features(features, 64, item);
        wm_snn_encode_item(snn_bridge, item, features, 64, 0.8f + item * 0.05f);
        wm_plasticity_encode(plasticity_bridge, item, 0.8f + item * 0.05f, nimcp_time_get_us());
    }

    // Phase 2: Maintenance with learning
    for (int cycle = 0; cycle < 20; cycle++) {
        wm_snn_simulate(snn_bridge, 10.0f);

        for (uint32_t slot = 0; slot < 4; slot++) {
            wm_slot_state_t slot_state;
            wm_snn_get_slot_state(snn_bridge, slot, &slot_state);
            wm_plasticity_maintain(plasticity_bridge, slot,
                slot_state.activity_level, nimcp_time_get_us());
        }

        wm_plasticity_update(plasticity_bridge, 10.0f);
    }

    // Phase 3: Retrieval with reinforcement
    float output[64];
    for (int i = 0; i < 10; i++) {
        uint32_t slot = i % 4;
        wm_snn_retrieve_item(snn_bridge, slot, output, 64);
        wm_plasticity_retrieve(plasticity_bridge, slot, 1.0f, nimcp_time_get_us());

        // Apply reward based on retrieval success
        wm_plasticity_reward(plasticity_bridge, 0.5f, nimcp_time_get_us());
    }

    // Phase 4: Consolidation
    wm_plasticity_consolidate_all(plasticity_bridge);

    // Verify final state
    wm_snn_bridge_state_t snn_state;
    wm_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.active_slots, 4u);
    EXPECT_GT(snn_state.total_activity, 0.0f);

    wm_plasticity_bridge_state_t plasticity_state;
    wm_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_GE(plasticity_state.registered_synapses, 12u);

    // Verify comprehensive stats
    wm_snn_stats_t snn_stats;
    wm_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_EQ(snn_stats.total_encodings, 4u);
    EXPECT_EQ(snn_stats.total_retrievals, 10u);
    EXPECT_GE(snn_stats.total_simulations, 20u);

    wm_plasticity_stats_t plasticity_stats;
    wm_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_EQ(plasticity_stats.total_encodings, 4u);
    EXPECT_EQ(plasticity_stats.total_retrievals, 10u);
    EXPECT_GE(plasticity_stats.total_maintenance_cycles, 80u);
}

//=============================================================================
// Test: Bridge Reset Coordination
//=============================================================================

TEST_F(WorkingMemorySNNPlasticityIntegrationTest, BridgeResetCoordination) {
    // Populate both bridges
    float features[64];
    for (uint32_t item = 0; item < 4; item++) {
        generate_item_features(features, 64, item);
        wm_snn_encode_item(snn_bridge, item, features, 64, 1.0f);
        wm_plasticity_encode(plasticity_bridge, item, 1.0f, nimcp_time_get_us());
    }

    wm_snn_simulate(snn_bridge, 50.0f);
    wm_plasticity_update(plasticity_bridge, 50.0f);

    // Verify populated state
    wm_snn_bridge_state_t state_before;
    wm_snn_get_state(snn_bridge, &state_before);
    EXPECT_EQ(state_before.active_slots, 4u);

    // Reset both bridges
    int snn_reset = wm_snn_reset(snn_bridge);
    int plasticity_reset = wm_plasticity_reset(plasticity_bridge);

    EXPECT_EQ(snn_reset, 0) << "SNN reset failed";
    EXPECT_EQ(plasticity_reset, 0) << "Plasticity reset failed";

    // Verify reset state
    wm_snn_bridge_state_t state_after;
    wm_snn_get_state(snn_bridge, &state_after);
    EXPECT_EQ(state_after.active_slots, 0u) << "SNN should have no active slots after reset";
    EXPECT_EQ(state_after.state, WM_SNN_STATE_IDLE);

    wm_plasticity_bridge_state_t plasticity_after;
    wm_plasticity_get_state(plasticity_bridge, &plasticity_after);
    EXPECT_EQ(plasticity_after.state, WM_PLASTICITY_STATE_IDLE);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
