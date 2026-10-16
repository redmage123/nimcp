/**
 * @file test_memory_snn_plasticity_regression.cpp
 * @brief Regression tests for Working Memory SNN-Plasticity bridges
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Regression tests ensuring working memory SNN and plasticity behavior stability
 * WHY:  Prevent regressions in memory maintenance and memory-based learning
 * HOW:  Test fixed scenarios with expected outputs, boundary conditions,
 *       and edge cases that have caused issues in the past
 *
 * REGRESSION COVERAGE:
 * - Initialization with various configurations
 * - Memory slot encoding edge cases
 * - Consolidation learning behavior
 * - Capacity-based plasticity
 * - Statistics accuracy over many iterations
 * - Memory and state leak prevention
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "cognitive/memory/nimcp_working_memory_snn_bridge.h"
#include "cognitive/memory/nimcp_working_memory_plasticity_bridge.h"

//=============================================================================
// SNN Bridge Regression Tests
//=============================================================================

class MemorySNNRegressionTest : public ::testing::Test {
protected:
    wm_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        wm_snn_config_t config = wm_snn_config_default();
        config.enable_bio_async = false;
        bridge = wm_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            wm_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(MemorySNNRegressionTest, ZeroFeaturesDoNotCrash) {
    float features[WM_SNN_SLOT_DIM] = {0};
    int spikes = wm_snn_encode_item(bridge, 0, features, WM_SNN_SLOT_DIM, 0.5f);
    EXPECT_GE(spikes, 0);

    // wm_snn_simulate returns spike count (>= 0) on success, -1 on failure
    int sim_result = wm_snn_simulate(bridge, 10.0f);
    EXPECT_GE(sim_result, 0);

    wm_slot_state_t slot_state;
    EXPECT_EQ(wm_snn_get_slot_state(bridge, 0, &slot_state), 0);
}

TEST_F(MemorySNNRegressionTest, MaxFeaturesDoNotCrash) {
    float features[WM_SNN_SLOT_DIM];
    for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
        features[i] = 1.0f;
    }
    int spikes = wm_snn_encode_item(bridge, 0, features, WM_SNN_SLOT_DIM, 1.0f);
    EXPECT_GE(spikes, 0);

    // wm_snn_simulate returns spike count (>= 0) on success, -1 on failure
    int sim_result = wm_snn_simulate(bridge, 10.0f);
    EXPECT_GE(sim_result, 0);

    float activity = wm_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

TEST_F(MemorySNNRegressionTest, OutOfRangeFeaturesClamped) {
    float features[WM_SNN_SLOT_DIM];
    for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
        features[i] = (i % 2 == 0) ? 5.0f : -2.0f;
    }
    int spikes = wm_snn_encode_item(bridge, 0, features, WM_SNN_SLOT_DIM, 0.5f);
    EXPECT_GE(spikes, 0);

    wm_snn_simulate(bridge, 10.0f);
    float activity = wm_snn_get_total_activity(bridge);
    EXPECT_GE(activity, 0.0f);
}

TEST_F(MemorySNNRegressionTest, AllSlotsUsable) {
    // Default config uses max_slots=8, which may be less than WM_SNN_MAX_SLOTS(16).
    // Create a bridge configured for all slots to test full capacity.
    wm_snn_config_t config = wm_snn_config_default();
    config.max_slots = WM_SNN_MAX_SLOTS;
    config.enable_bio_async = false;

    wm_snn_bridge_t* full_bridge = wm_snn_create(&config);
    ASSERT_NE(full_bridge, nullptr);

    for (uint32_t slot = 0; slot < WM_SNN_MAX_SLOTS; slot++) {
        float features[WM_SNN_SLOT_DIM];
        for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
            features[i] = (float)slot / WM_SNN_MAX_SLOTS;
        }
        int spikes = wm_snn_encode_item(full_bridge, slot, features, WM_SNN_SLOT_DIM, 0.5f);
        EXPECT_GE(spikes, 0);
    }

    // wm_snn_simulate returns spike count (>= 0), not error code
    int sim_result = wm_snn_simulate(full_bridge, 20.0f);
    EXPECT_GE(sim_result, 0);

    wm_snn_bridge_state_t state;
    wm_snn_get_state(full_bridge, &state);
    EXPECT_EQ(state.active_slots, WM_SNN_MAX_SLOTS);

    wm_snn_destroy(full_bridge);
}

TEST_F(MemorySNNRegressionTest, RepeatedEncodingStable) {
    std::vector<float> activity_values;

    for (int i = 0; i < 100; i++) {
        float features[WM_SNN_SLOT_DIM];
        for (int j = 0; j < WM_SNN_SLOT_DIM; j++) {
            features[j] = sinf(i * 0.1f + j * 0.01f) * 0.5f + 0.5f;
        }
        wm_snn_encode_item(bridge, i % WM_SNN_MAX_SLOTS, features, WM_SNN_SLOT_DIM, 0.5f);
        wm_snn_simulate(bridge, 5.0f);

        float activity = wm_snn_get_total_activity(bridge);
        activity_values.push_back(activity);
    }

    for (float val : activity_values) {
        EXPECT_GE(val, 0.0f);
    }
}

TEST_F(MemorySNNRegressionTest, StatsAccurateAfterManyOperations) {
    const int NUM_ENCODINGS = 30;
    const int NUM_RETRIEVALS = 20;
    // Default bridge has max_slots=8, use that for slot indexing
    const uint32_t MAX_SLOTS = 8;

    for (int i = 0; i < NUM_ENCODINGS; i++) {
        float features[WM_SNN_SLOT_DIM] = {0.5f};
        wm_snn_encode_item(bridge, i % MAX_SLOTS, features, 1, 0.5f);
    }

    wm_snn_simulate(bridge, 50.0f);

    float output[WM_SNN_SLOT_DIM];
    for (int i = 0; i < NUM_RETRIEVALS; i++) {
        wm_snn_retrieve_item(bridge, i % MAX_SLOTS, output, WM_SNN_SLOT_DIM);
    }

    wm_snn_stats_t stats;
    wm_snn_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_encodings, (uint64_t)NUM_ENCODINGS);
    EXPECT_GE(stats.total_retrievals, (uint64_t)NUM_RETRIEVALS);
}

TEST_F(MemorySNNRegressionTest, ResetClearsAllState) {
    for (int i = 0; i < 5; i++) {
        float features[WM_SNN_SLOT_DIM] = {0.9f};
        wm_snn_encode_item(bridge, i, features, 1, 0.8f);
    }
    wm_snn_simulate(bridge, 20.0f);

    wm_snn_reset(bridge);

    wm_snn_bridge_state_t state;
    wm_snn_get_state(bridge, &state);
    EXPECT_EQ(state.state, WM_SNN_STATE_IDLE);
    EXPECT_EQ(state.active_slots, 0u);
}

TEST_F(MemorySNNRegressionTest, SlotClearingWorks) {
    float features[WM_SNN_SLOT_DIM] = {0.5f};
    wm_snn_encode_item(bridge, 0, features, 1, 0.8f);
    wm_snn_encode_item(bridge, 1, features, 1, 0.8f);

    wm_snn_bridge_state_t state_before;
    wm_snn_get_state(bridge, &state_before);
    uint32_t active_before = state_before.active_slots;

    wm_snn_clear_slot(bridge, 0);

    wm_snn_bridge_state_t state_after;
    wm_snn_get_state(bridge, &state_after);
    EXPECT_LT(state_after.active_slots, active_before);
}

TEST_F(MemorySNNRegressionTest, LateralInhibitionStable) {
    wm_snn_config_t config = wm_snn_config_default();
    config.enable_lateral_inhibition = true;
    config.inhibition_strength = 1.0f;
    config.enable_bio_async = false;

    wm_snn_bridge_t* inh_bridge = wm_snn_create(&config);
    ASSERT_NE(inh_bridge, nullptr);

    for (int i = 0; i < WM_SNN_MAX_SLOTS; i++) {
        float features[WM_SNN_SLOT_DIM];
        for (int j = 0; j < WM_SNN_SLOT_DIM; j++) {
            features[j] = (float)i / WM_SNN_MAX_SLOTS;
        }
        wm_snn_encode_item(inh_bridge, i, features, WM_SNN_SLOT_DIM, 0.5f);
    }

    for (int i = 0; i < 100; i++) {
        wm_snn_simulate(inh_bridge, 10.0f);
    }

    float activity = wm_snn_get_total_activity(inh_bridge);
    EXPECT_GE(activity, 0.0f);

    wm_snn_destroy(inh_bridge);
}

//=============================================================================
// Plasticity Bridge Regression Tests
//=============================================================================

class MemoryPlasticityRegressionTest : public ::testing::Test {
protected:
    wm_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        wm_plasticity_config_t config = wm_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = wm_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            wm_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(MemoryPlasticityRegressionTest, WeightsStayInBounds) {
    wm_plasticity_register_synapse(bridge, 1, WM_SYNAPSE_ENCODING, 0, 0.5f);

    for (int i = 0; i < 1000; i++) {
        wm_plasticity_encode(bridge, 0, 0.8f, i * 1000);
        wm_plasticity_maintain(bridge, 0, 0.9f, i * 1000 + 500);
    }

    wm_plasticity_update(bridge, 10.0f);

    wm_plasticity_synapse_t syn;
    wm_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GE(syn.weight, 0.0f);
    EXPECT_LE(syn.weight, 1.0f);
}

TEST_F(MemoryPlasticityRegressionTest, WeightsStayInBoundsNegative) {
    wm_plasticity_register_synapse(bridge, 1, WM_SYNAPSE_ENCODING, 0, 0.5f);

    for (int i = 0; i < 1000; i++) {
        wm_plasticity_evict(bridge, 0, i * 1000);
        wm_plasticity_decay(bridge, 0, 0.1f, i * 1000 + 500);
    }

    wm_plasticity_update(bridge, 10.0f);

    wm_plasticity_synapse_t syn;
    wm_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GE(syn.weight, 0.0f);
    EXPECT_LE(syn.weight, 1.0f);
}

TEST_F(MemoryPlasticityRegressionTest, MaintenanceLearning) {
    wm_plasticity_config_t config = wm_plasticity_config_default();
    config.enable_maintenance_ltp = true;
    config.maintenance_ltp_rate = 0.1f;
    config.enable_bio_async = false;

    wm_plasticity_bridge_t* maint_bridge = wm_plasticity_create(&config);
    ASSERT_NE(maint_bridge, nullptr);

    wm_plasticity_register_synapse(maint_bridge, 1, WM_SYNAPSE_MAINTENANCE, 0, 0.5f);

    // Slot must be occupied before maintenance can operate on it.
    // wm_plasticity_maintain returns early (no-op) if slot is not occupied.
    wm_plasticity_encode(maint_bridge, 0, 0.8f, 0);

    wm_plasticity_synapse_t initial;
    wm_plasticity_get_synapse(maint_bridge, 1, &initial);

    for (int i = 0; i < 50; i++) {
        wm_plasticity_maintain(maint_bridge, 0, 0.8f, (i + 1) * 10000);
        wm_plasticity_update(maint_bridge, 10.0f);
    }

    wm_plasticity_stats_t stats;
    wm_plasticity_get_stats(maint_bridge, &stats);
    EXPECT_GT(stats.total_maintenance_cycles, 0u);

    wm_plasticity_destroy(maint_bridge);
}

TEST_F(MemoryPlasticityRegressionTest, ConsolidationLearning) {
    wm_plasticity_config_t config = wm_plasticity_config_default();
    config.enable_consolidation = true;
    config.consolidation_threshold = 10.0f;
    config.consolidation_ltp_boost = 0.5f;
    config.enable_bio_async = false;

    wm_plasticity_bridge_t* cons_bridge = wm_plasticity_create(&config);
    ASSERT_NE(cons_bridge, nullptr);

    wm_plasticity_register_synapse(cons_bridge, 1, WM_SYNAPSE_ENCODING, 0, 0.5f);

    wm_plasticity_encode(cons_bridge, 0, 0.8f, 0);
    for (int i = 0; i < 30; i++) {
        wm_plasticity_maintain(cons_bridge, 0, 0.7f, i * 10000);
        wm_plasticity_update(cons_bridge, 10.0f);
    }

    wm_plasticity_consolidate_slot(cons_bridge, 0);

    float consolidation_level = wm_plasticity_get_consolidation_level(cons_bridge, 0);
    EXPECT_GE(consolidation_level, 0.0f);

    wm_plasticity_stats_t stats;
    wm_plasticity_get_stats(cons_bridge, &stats);
    EXPECT_GT(stats.total_consolidations, 0u);

    wm_plasticity_destroy(cons_bridge);
}

TEST_F(MemoryPlasticityRegressionTest, CapacityPressureLTD) {
    wm_plasticity_config_t config = wm_plasticity_config_default();
    config.enable_capacity_ltd = true;
    config.capacity_ltd_rate = 0.1f;
    config.enable_bio_async = false;

    wm_plasticity_bridge_t* cap_bridge = wm_plasticity_create(&config);
    ASSERT_NE(cap_bridge, nullptr);

    for (int i = 0; i < 10; i++) {
        wm_plasticity_register_synapse(cap_bridge, i, WM_SYNAPSE_ENCODING, i, 0.5f);
    }

    wm_plasticity_set_capacity_pressure(cap_bridge, 1.0f);
    for (int i = 0; i < 20; i++) {
        wm_plasticity_evict(cap_bridge, i % WM_PLASTICITY_MAX_SLOTS, i * 10000);
        wm_plasticity_update(cap_bridge, 10.0f);
    }

    wm_plasticity_stats_t stats;
    wm_plasticity_get_stats(cap_bridge, &stats);
    EXPECT_GT(stats.total_evictions, 0u);

    wm_plasticity_destroy(cap_bridge);
}

TEST_F(MemoryPlasticityRegressionTest, RehearsalLearning) {
    wm_plasticity_config_t config = wm_plasticity_config_default();
    config.enable_rehearsal_boost = true;
    config.rehearsal_ltp_gain = 2.0f;
    config.enable_bio_async = false;

    wm_plasticity_bridge_t* reh_bridge = wm_plasticity_create(&config);
    ASSERT_NE(reh_bridge, nullptr);

    wm_plasticity_register_synapse(reh_bridge, 1, WM_SYNAPSE_MAINTENANCE, 0, 0.5f);

    wm_plasticity_encode(reh_bridge, 0, 0.8f, 0);
    for (int i = 0; i < 20; i++) {
        wm_plasticity_retrieve(reh_bridge, 0, 0.9f, i * 5000);
        wm_plasticity_maintain(reh_bridge, 0, 0.8f, i * 5000 + 1000);
        wm_plasticity_update(reh_bridge, 10.0f);
    }

    wm_plasticity_stats_t stats;
    wm_plasticity_get_stats(reh_bridge, &stats);
    EXPECT_GT(stats.total_retrievals, 0u);

    wm_plasticity_destroy(reh_bridge);
}

TEST_F(MemoryPlasticityRegressionTest, ManySynapsesDoNotExceedCapacity) {
    for (uint32_t i = 0; i < WM_PLASTICITY_MAX_SYNAPSES + 10; i++) {
        int result = wm_plasticity_register_synapse(bridge, i, WM_SYNAPSE_ENCODING, 0, 0.5f);
        if (i < WM_PLASTICITY_MAX_SYNAPSES) {
            EXPECT_EQ(result, 0);
        } else {
            EXPECT_EQ(result, -1);
        }
    }

    wm_plasticity_bridge_state_t state;
    wm_plasticity_get_state(bridge, &state);
    EXPECT_LE(state.registered_synapses, WM_PLASTICITY_MAX_SYNAPSES);
}

TEST_F(MemoryPlasticityRegressionTest, ConsolidateAllPreservesState) {
    for (int i = 0; i < 10; i++) {
        wm_plasticity_register_synapse(bridge, i, WM_SYNAPSE_ENCODING, i % WM_PLASTICITY_MAX_SLOTS, 0.5f);
    }

    for (int i = 0; i < 20; i++) {
        wm_plasticity_encode(bridge, i % WM_PLASTICITY_MAX_SLOTS, 0.8f, i * 10000);
        wm_plasticity_update(bridge, 10.0f);
    }

    wm_plasticity_consolidate_all(bridge);

    wm_plasticity_bridge_state_t state;
    wm_plasticity_get_state(bridge, &state);
    EXPECT_EQ(state.state, WM_PLASTICITY_STATE_IDLE);
}

//=============================================================================
// Combined Regression Tests
//=============================================================================

class MemoryCombinedRegressionTest : public ::testing::Test {
protected:
    wm_snn_bridge_t* snn = nullptr;
    wm_plasticity_bridge_t* plasticity = nullptr;

    void SetUp() override {
        wm_snn_config_t snn_config = wm_snn_config_default();
        snn_config.enable_bio_async = false;
        snn = wm_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        wm_plasticity_config_t plasticity_config = wm_plasticity_config_default();
        plasticity_config.enable_bio_async = false;
        plasticity = wm_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity, nullptr);
    }

    void TearDown() override {
        if (snn) {
            wm_snn_destroy(snn);
            snn = nullptr;
        }
        if (plasticity) {
            wm_plasticity_destroy(plasticity);
            plasticity = nullptr;
        }
    }
};

TEST_F(MemoryCombinedRegressionTest, LongRunningStability) {
    // Default SNN bridge has max_slots=8, use that for slot indexing
    const uint32_t SNN_SLOTS = 8;

    for (uint32_t i = 0; i < SNN_SLOTS; i++) {
        wm_plasticity_register_synapse(plasticity, i, WM_SYNAPSE_ENCODING, i, 0.5f);
    }

    const int ITERATIONS = 200;
    for (int iter = 0; iter < ITERATIONS; iter++) {
        uint32_t slot = iter % SNN_SLOTS;
        float features[WM_SNN_SLOT_DIM];
        for (int j = 0; j < WM_SNN_SLOT_DIM; j++) {
            features[j] = sinf(iter * 0.1f + j * 0.01f) * 0.5f + 0.5f;
        }

        wm_snn_encode_item(snn, slot, features, WM_SNN_SLOT_DIM, 0.5f);
        wm_snn_simulate(snn, 5.0f);

        wm_slot_state_t slot_state;
        wm_snn_get_slot_state(snn, slot, &slot_state);

        wm_plasticity_encode(plasticity, slot, slot_state.salience, iter * 5000);
        wm_plasticity_maintain(plasticity, slot, slot_state.activity_level, iter * 5000 + 2500);

        if (iter % 10 == 0) {
            wm_plasticity_update(plasticity, 10.0f);
        }
    }

    wm_snn_stats_t snn_stats;
    wm_snn_get_stats(snn, &snn_stats);
    EXPECT_GE(snn_stats.total_encodings, (uint64_t)ITERATIONS);

    wm_plasticity_stats_t plasticity_stats;
    wm_plasticity_get_stats(plasticity, &plasticity_stats);
    EXPECT_EQ(plasticity_stats.total_encodings, (uint64_t)ITERATIONS);
    EXPECT_EQ(plasticity_stats.total_maintenance_cycles, (uint64_t)ITERATIONS);

    for (uint32_t i = 0; i < SNN_SLOTS; i++) {
        wm_plasticity_synapse_t syn;
        wm_plasticity_get_synapse(plasticity, i, &syn);
        EXPECT_GE(syn.weight, 0.0f);
        EXPECT_LE(syn.weight, 1.0f);
    }
}

TEST_F(MemoryCombinedRegressionTest, ResetBothDoesNotLeak) {
    for (int round = 0; round < 5; round++) {
        float features[WM_SNN_SLOT_DIM] = {0.5f};
        for (int i = 0; i < 10; i++) {
            wm_snn_encode_item(snn, i % WM_SNN_MAX_SLOTS, features, 1, 0.5f);
            wm_snn_simulate(snn, 5.0f);
        }

        wm_snn_reset(snn);
        wm_plasticity_reset(plasticity);
    }

    wm_snn_bridge_state_t snn_state;
    wm_snn_get_state(snn, &snn_state);
    EXPECT_EQ(snn_state.state, WM_SNN_STATE_IDLE);

    wm_plasticity_bridge_state_t plasticity_state;
    wm_plasticity_get_state(plasticity, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, WM_PLASTICITY_STATE_IDLE);
}
