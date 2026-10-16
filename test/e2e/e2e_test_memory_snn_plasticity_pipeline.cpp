/**
 * @file e2e_test_memory_snn_plasticity_pipeline.cpp
 * @brief End-to-end tests for Working Memory-SNN-Plasticity learning pipeline
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Complete working memory pipeline with SNN and Plasticity
 * WHY:  Verify full dataflow from memory items -> SNN encoding -> maintenance
 *       -> plasticity learning -> memory consolidation
 * HOW:  Test realistic scenarios combining memory encoding, rehearsal,
 *       consolidation, and capacity-based learning
 *
 * Test Coverage:
 * - Full memory item to slot encoding pipeline via SNN
 * - STDP and maintenance-based learning for memory strengthening
 * - Consolidation learning for long-term memory transfer
 * - Capacity pressure and eviction-based plasticity
 * - Multi-slot memory learning scenarios
 * - Rehearsal-driven memory enhancement
 */

#include <gtest/gtest.h>

#include "cognitive/memory/nimcp_working_memory_snn_bridge.h"
#include "cognitive/memory/nimcp_working_memory_plasticity_bridge.h"
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

class MemorySNNPlasticityE2E : public ::testing::Test {
protected:
    wm_snn_bridge_t* snn_bridge = nullptr;
    wm_plasticity_bridge_t* plasticity_bridge = nullptr;

    struct LearningStats {
        int encoding_events = 0;
        int maintenance_events = 0;
        int retrieval_events = 0;
        int eviction_events = 0;
        int total_evaluations = 0;
        std::vector<float> activity_history;
        std::vector<float> capacity_history;
    } stats;

    void SetUp() override {
        wm_snn_config_t snn_config = wm_snn_config_default();
        snn_config.max_slots = WM_SNN_MAX_SLOTS;
        snn_config.neurons_per_slot = 32;
        snn_config.slot_dim = WM_SNN_SLOT_DIM;
        snn_config.dt_ms = 1.0f;
        snn_config.enable_lateral_inhibition = true;
        snn_config.enable_recurrence = true;
        snn_config.enable_bio_async = false;

        snn_bridge = wm_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        wm_plasticity_config_t plasticity_config = wm_plasticity_config_default();
        plasticity_config.enable_maintenance_ltp = true;
        plasticity_config.enable_rehearsal_boost = true;
        plasticity_config.enable_consolidation = true;
        plasticity_config.enable_capacity_ltd = true;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = wm_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create Plasticity bridge";

        for (uint32_t i = 0; i < WM_SNN_MAX_SLOTS; i++) {
            wm_plasticity_register_synapse(plasticity_bridge, i,
                WM_SYNAPSE_ENCODING, i, 0.5f);
        }

        for (uint32_t i = 0; i < WM_SNN_MAX_SLOTS; i++) {
            wm_plasticity_register_synapse(plasticity_bridge, 100 + i,
                WM_SYNAPSE_MAINTENANCE, i, 0.5f);
        }
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

    enum MemoryScenario {
        SINGLE_ITEM_ENCODING,
        MULTIPLE_ITEM_ENCODING,
        HIGH_SALIENCE_ITEM,
        LOW_SALIENCE_ITEM,
        CAPACITY_FULL,
        REHEARSAL_SEQUENCE,
        RETRIEVAL_FOCUSED,
        DECAY_SCENARIO
    };

    void generate_features(float* features, uint32_t slot, MemoryScenario scenario) {
        memset(features, 0, sizeof(float) * WM_SNN_SLOT_DIM);

        switch (scenario) {
            case SINGLE_ITEM_ENCODING:
                for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
                    features[i] = 0.5f + 0.3f * sinf(i * 0.1f);
                }
                break;
            case MULTIPLE_ITEM_ENCODING:
                for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
                    features[i] = 0.3f + 0.2f * cosf(i * 0.1f + slot * 0.5f);
                }
                break;
            case HIGH_SALIENCE_ITEM:
                for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
                    features[i] = 0.8f + 0.15f * sinf(i * 0.05f);
                }
                break;
            case LOW_SALIENCE_ITEM:
                for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
                    features[i] = 0.2f + 0.1f * cosf(i * 0.1f);
                }
                break;
            case CAPACITY_FULL:
                for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
                    features[i] = 0.5f + 0.25f * sinf(i * 0.1f + slot * 1.0f);
                }
                break;
            case REHEARSAL_SEQUENCE:
                for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
                    features[i] = 0.6f + 0.2f * sinf(i * 0.08f);
                }
                break;
            case RETRIEVAL_FOCUSED:
                for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
                    features[i] = 0.7f + 0.15f * cosf(i * 0.12f);
                }
                break;
            case DECAY_SCENARIO:
                for (int i = 0; i < WM_SNN_SLOT_DIM; i++) {
                    features[i] = 0.4f + 0.2f * sinf(i * 0.15f);
                }
                break;
        }
    }

    struct EvaluationResult {
        uint32_t slot;
        float activity_level;
        float capacity_used;
        int spike_count;
        bool encoding_success;
    };

    EvaluationResult run_encoding(uint32_t slot, float salience, MemoryScenario scenario) {
        EvaluationResult result = {slot, 0, 0, 0, false};

        float features[WM_SNN_SLOT_DIM];
        generate_features(features, slot, scenario);

        result.spike_count = wm_snn_encode_item(snn_bridge, slot, features, WM_SNN_SLOT_DIM, salience);
        result.encoding_success = (result.spike_count >= 0);

        wm_snn_simulate(snn_bridge, 30.0f);

        wm_snn_bridge_state_t state;
        wm_snn_get_state(snn_bridge, &state);
        result.activity_level = state.total_activity;
        result.capacity_used = state.capacity_used;

        stats.total_evaluations++;
        stats.activity_history.push_back(result.activity_level);
        stats.capacity_history.push_back(result.capacity_used);

        return result;
    }
};

//=============================================================================
// Basic Pipeline Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, CompletePipelineInitialization) {
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    wm_plasticity_bridge_state_t state;
    wm_plasticity_get_state(plasticity_bridge, &state);
    EXPECT_GT(state.registered_synapses, WM_SNN_MAX_SLOTS);
}

TEST_F(MemorySNNPlasticityE2E, SingleEncodingPipeline) {
    auto result = run_encoding(0, 0.8f, SINGLE_ITEM_ENCODING);

    EXPECT_TRUE(result.encoding_success);
    EXPECT_GE(result.activity_level, 0.0f);
    EXPECT_GE(result.spike_count, 0);

    int ret = wm_plasticity_encode(plasticity_bridge, 0, 0.8f, 0);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Encoding and Maintenance Learning Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, EncodingLearning) {
    for (int trial = 0; trial < 20; trial++) {
        uint32_t slot = trial % WM_SNN_MAX_SLOTS;
        auto result = run_encoding(slot, 0.7f, SINGLE_ITEM_ENCODING);

        wm_plasticity_encode(plasticity_bridge, slot, 0.7f, trial * 10000);
        wm_plasticity_update(plasticity_bridge, 10.0f);
    }

    wm_plasticity_stats_t pstats;
    wm_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_EQ(pstats.total_encodings, 20u);
}

TEST_F(MemorySNNPlasticityE2E, MaintenanceLearning) {
    auto result = run_encoding(0, 0.8f, SINGLE_ITEM_ENCODING);
    wm_plasticity_encode(plasticity_bridge, 0, 0.8f, 0);

    for (int trial = 0; trial < 30; trial++) {
        wm_snn_simulate(snn_bridge, 20.0f);

        wm_slot_state_t slot_state;
        wm_snn_get_slot_state(snn_bridge, 0, &slot_state);

        wm_plasticity_maintain(plasticity_bridge, 0, slot_state.activity_level, (trial + 1) * 10000);
        wm_plasticity_update(plasticity_bridge, 10.0f);
    }

    wm_plasticity_stats_t pstats;
    wm_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_EQ(pstats.total_maintenance_cycles, 30u);
}

//=============================================================================
// Rehearsal Learning Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, RehearsalLearning) {
    for (uint32_t slot = 0; slot < 4; slot++) {
        run_encoding(slot, 0.7f, REHEARSAL_SEQUENCE);
        wm_plasticity_encode(plasticity_bridge, slot, 0.7f, slot * 5000);
    }

    for (int trial = 0; trial < 20; trial++) {
        uint32_t slot = trial % 4;

        float output[WM_SNN_SLOT_DIM];
        wm_snn_retrieve_item(snn_bridge, slot, output, WM_SNN_SLOT_DIM);
        wm_plasticity_retrieve(plasticity_bridge, slot, 0.8f, 20000 + trial * 5000);

        wm_snn_simulate(snn_bridge, 10.0f);

        wm_slot_state_t slot_state;
        wm_snn_get_slot_state(snn_bridge, slot, &slot_state);
        wm_plasticity_maintain(plasticity_bridge, slot, slot_state.activity_level, 20000 + trial * 5000 + 2000);

        wm_plasticity_update(plasticity_bridge, 10.0f);
    }

    wm_plasticity_stats_t pstats;
    wm_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_EQ(pstats.total_retrievals, 20u);
    EXPECT_EQ(pstats.total_maintenance_cycles, 20u);
}

//=============================================================================
// Consolidation Learning Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, ConsolidationLearning) {
    auto result = run_encoding(0, 0.9f, HIGH_SALIENCE_ITEM);
    wm_plasticity_encode(plasticity_bridge, 0, 0.9f, 0);

    for (int trial = 0; trial < 40; trial++) {
        wm_snn_simulate(snn_bridge, 20.0f);

        wm_slot_state_t slot_state;
        wm_snn_get_slot_state(snn_bridge, 0, &slot_state);
        wm_plasticity_maintain(plasticity_bridge, 0, slot_state.activity_level, (trial + 1) * 10000);

        wm_plasticity_update(plasticity_bridge, 10.0f);
    }

    wm_plasticity_consolidate_slot(plasticity_bridge, 0);

    float consolidation_level = wm_plasticity_get_consolidation_level(plasticity_bridge, 0);
    EXPECT_GE(consolidation_level, 0.0f);

    wm_plasticity_stats_t pstats;
    wm_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.total_consolidations, 0u);
}

//=============================================================================
// Capacity-Based Plasticity Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, CapacityPressureLearning) {
    for (uint32_t slot = 0; slot < WM_SNN_MAX_SLOTS; slot++) {
        run_encoding(slot, 0.6f, CAPACITY_FULL);
        wm_plasticity_encode(plasticity_bridge, slot, 0.6f, slot * 5000);
    }

    wm_plasticity_set_capacity_pressure(plasticity_bridge, 1.0f);

    for (int trial = 0; trial < 10; trial++) {
        uint32_t evict_slot = trial % WM_SNN_MAX_SLOTS;
        wm_snn_clear_slot(snn_bridge, evict_slot);
        wm_plasticity_evict(plasticity_bridge, evict_slot, (WM_SNN_MAX_SLOTS + trial) * 5000);

        uint32_t new_slot = evict_slot;
        run_encoding(new_slot, 0.7f, HIGH_SALIENCE_ITEM);
        wm_plasticity_encode(plasticity_bridge, new_slot, 0.7f, (WM_SNN_MAX_SLOTS + trial) * 5000 + 2000);

        wm_plasticity_update(plasticity_bridge, 10.0f);
    }

    wm_plasticity_stats_t pstats;
    wm_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.total_evictions, 0u);
}

//=============================================================================
// Decay and Forgetting Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, DecayLearning) {
    auto result = run_encoding(0, 0.5f, DECAY_SCENARIO);
    wm_plasticity_encode(plasticity_bridge, 0, 0.5f, 0);

    for (int trial = 0; trial < 20; trial++) {
        wm_snn_simulate(snn_bridge, 30.0f);

        wm_slot_state_t slot_state;
        wm_snn_get_slot_state(snn_bridge, 0, &slot_state);

        float decay_strength = slot_state.activity_level * (1.0f - trial * 0.03f);
        if (decay_strength < 0.1f) decay_strength = 0.1f;

        wm_plasticity_decay(plasticity_bridge, 0, decay_strength, (trial + 1) * 10000);
        wm_plasticity_update(plasticity_bridge, 10.0f);
    }

    wm_plasticity_stats_t pstats;
    wm_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GT(pstats.ltd_events, 0u);
}

//=============================================================================
// Multi-Scenario Learning Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, CompleteMemoryWorkflow) {
    for (int epoch = 0; epoch < 5; epoch++) {
        for (int scenario = 0; scenario < 8; scenario++) {
            uint32_t slot = (epoch * 8 + scenario) % WM_SNN_MAX_SLOTS;
            auto result = run_encoding(slot, 0.6f + 0.1f * (scenario % 4), (MemoryScenario)scenario);

            wm_plasticity_encode(plasticity_bridge, slot, result.activity_level,
                epoch * 80000 + scenario * 10000);

            switch ((MemoryScenario)scenario) {
                case SINGLE_ITEM_ENCODING:
                case MULTIPLE_ITEM_ENCODING:
                    wm_plasticity_maintain(plasticity_bridge, slot, result.activity_level,
                        epoch * 80000 + scenario * 10000 + 2000);
                    break;
                case HIGH_SALIENCE_ITEM:
                    wm_plasticity_maintain(plasticity_bridge, slot, result.activity_level,
                        epoch * 80000 + scenario * 10000 + 2000);
                    wm_plasticity_reward(plasticity_bridge, 0.5f, epoch * 80000 + scenario * 10000 + 4000);
                    break;
                case LOW_SALIENCE_ITEM:
                    wm_plasticity_decay(plasticity_bridge, slot, result.activity_level * 0.8f,
                        epoch * 80000 + scenario * 10000 + 5000);
                    break;
                case CAPACITY_FULL:
                    if (slot > 0) {
                        wm_plasticity_evict(plasticity_bridge, (slot - 1) % WM_SNN_MAX_SLOTS,
                            epoch * 80000 + scenario * 10000 + 3000);
                    }
                    break;
                case REHEARSAL_SEQUENCE:
                    {
                        float output[WM_SNN_SLOT_DIM];
                        wm_snn_retrieve_item(snn_bridge, slot, output, WM_SNN_SLOT_DIM);
                        wm_plasticity_retrieve(plasticity_bridge, slot, 0.8f,
                            epoch * 80000 + scenario * 10000 + 2000);
                    }
                    break;
                case RETRIEVAL_FOCUSED:
                    {
                        float output[WM_SNN_SLOT_DIM];
                        wm_snn_retrieve_item(snn_bridge, slot, output, WM_SNN_SLOT_DIM);
                        wm_plasticity_retrieve(plasticity_bridge, slot, 0.9f,
                            epoch * 80000 + scenario * 10000 + 2000);
                        wm_plasticity_maintain(plasticity_bridge, slot, result.activity_level,
                            epoch * 80000 + scenario * 10000 + 4000);
                    }
                    break;
                default:
                    break;
            }

            wm_plasticity_update(plasticity_bridge, 10.0f);
        }
    }

    wm_plasticity_consolidate_all(plasticity_bridge);

    wm_plasticity_stats_t final_stats;
    wm_plasticity_get_stats(plasticity_bridge, &final_stats);
    EXPECT_GT(final_stats.total_encodings, 30u);

    wm_snn_stats_t snn_stats;
    wm_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_encodings, 40u);
}

//=============================================================================
// Stress and Performance Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, HighVolumeProcessing) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        uint32_t slot = i % WM_SNN_MAX_SLOTS;
        run_encoding(slot, 0.5f, (MemoryScenario)(i % 8));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 5000);
    EXPECT_EQ(stats.total_evaluations, 100);
}

TEST_F(MemorySNNPlasticityE2E, ContinuousLearning) {
    for (int cycle = 0; cycle < 50; cycle++) {
        uint32_t slot = cycle % WM_SNN_MAX_SLOTS;
        auto result = run_encoding(slot, 0.6f, (MemoryScenario)(cycle % 8));

        wm_plasticity_encode(plasticity_bridge, slot, 0.6f, cycle * 10000);

        if (cycle % 3 == 0) {
            wm_plasticity_maintain(plasticity_bridge, slot, result.activity_level, cycle * 10000 + 5000);
        }

        if (cycle % 5 == 0) {
            wm_plasticity_update(plasticity_bridge, 10.0f);
        }
    }

    wm_plasticity_stats_t pstats;
    wm_plasticity_get_stats(plasticity_bridge, &pstats);
    EXPECT_GE(pstats.total_encodings, 50u);
}

//=============================================================================
// Reset and Recovery Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, ResetAndRecovery) {
    for (int i = 0; i < 10; i++) {
        run_encoding(i % WM_SNN_MAX_SLOTS, 0.5f, (MemoryScenario)(i % 8));
    }

    wm_snn_reset(snn_bridge);
    wm_plasticity_reset(plasticity_bridge);

    wm_snn_bridge_state_t snn_state;
    wm_snn_get_state(snn_bridge, &snn_state);
    EXPECT_EQ(snn_state.state, WM_SNN_STATE_IDLE);
    EXPECT_EQ(snn_state.active_slots, 0u);

    wm_plasticity_bridge_state_t plasticity_state;
    wm_plasticity_get_state(plasticity_bridge, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, WM_PLASTICITY_STATE_IDLE);

    auto result = run_encoding(0, 0.7f, SINGLE_ITEM_ENCODING);
    EXPECT_TRUE(result.encoding_success);
}

//=============================================================================
// Statistics Validation Tests
//=============================================================================

TEST_F(MemorySNNPlasticityE2E, StatisticsAccuracy) {
    for (int i = 0; i < 20; i++) {
        uint32_t slot = i % WM_SNN_MAX_SLOTS;
        run_encoding(slot, 0.5f, (MemoryScenario)(i % 8));
        wm_plasticity_encode(plasticity_bridge, slot, 0.5f, i * 10000);
    }

    wm_snn_stats_t snn_stats;
    wm_snn_get_stats(snn_bridge, &snn_stats);
    EXPECT_GE(snn_stats.total_encodings, 20u);

    wm_plasticity_stats_t plasticity_stats;
    wm_plasticity_get_stats(plasticity_bridge, &plasticity_stats);
    EXPECT_GE(plasticity_stats.total_encodings, 20u);
}
