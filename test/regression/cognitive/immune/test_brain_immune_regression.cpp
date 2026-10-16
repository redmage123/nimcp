/**
 * @file test_brain_immune_regression.cpp
 * @brief Regression tests for Brain Immune System
 * @version 1.0.0
 * @date 2025-12-11
 *
 * Tests to prevent regression of fixed bugs and ensure stability.
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Regression Test Fixture
 * ============================================================================ */

class BrainImmuneRegressionTest : public ::testing::Test {
protected:
    brain_immune_system_t* system = nullptr;
    brain_immune_config_t config;

    void SetUp() override {
        brain_immune_default_config(&config);
        system = brain_immune_create(&config);
        ASSERT_NE(system, nullptr);
        brain_immune_start(system);
    }

    void TearDown() override {
        if (system) {
            brain_immune_stop(system);
            brain_immune_destroy(system);
            system = nullptr;
        }
    }
};

/* ============================================================================
 * Memory Safety Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, CreateDestroyNoLeak) {
    // Test that repeated create/destroy doesn't leak memory
    for (int i = 0; i < 10; i++) {
        brain_immune_system_t* sys = brain_immune_create(nullptr);
        ASSERT_NE(sys, nullptr);
        brain_immune_destroy(sys);
    }
}

TEST_F(BrainImmuneRegressionTest, DestroyNullSafe) {
    // Should not crash
    brain_immune_destroy(nullptr);
}

TEST_F(BrainImmuneRegressionTest, DoubleStartSafe) {
    // Double start should be safe
    EXPECT_EQ(brain_immune_start(system), 0);
    EXPECT_EQ(brain_immune_start(system), 0);
    EXPECT_TRUE(system->running);
}

TEST_F(BrainImmuneRegressionTest, DoubleStopSafe) {
    // Double stop should be safe
    brain_immune_stop(system);
    EXPECT_EQ(brain_immune_stop(system), 0);
    EXPECT_FALSE(system->running);
}

/* ============================================================================
 * Boundary Condition Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, ZeroLengthEpitopeRejected) {
    uint8_t epitope[] = {0x01};
    uint32_t antigen_id;

    int result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_MANUAL,
        epitope, 0,  // Zero length
        5, 0, &antigen_id
    );
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneRegressionTest, MaxSizeEpitopeHandled) {
    // Create exactly max-size epitope
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE];
    memset(epitope, 0xAA, sizeof(epitope));

    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        5, 0, &antigen_id
    );
    EXPECT_EQ(result, 0);

    const brain_antigen_t* ag = brain_immune_get_antigen(system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->epitope_len, BRAIN_IMMUNE_EPITOPE_SIZE);
}

TEST_F(BrainImmuneRegressionTest, OversizedEpitopeTruncated) {
    // Create oversized epitope
    uint8_t epitope[BRAIN_IMMUNE_EPITOPE_SIZE + 64];
    memset(epitope, 0xBB, sizeof(epitope));

    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        5, 0, &antigen_id
    );
    EXPECT_EQ(result, 0);

    const brain_antigen_t* ag = brain_immune_get_antigen(system, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->epitope_len, BRAIN_IMMUNE_EPITOPE_SIZE);
}

TEST_F(BrainImmuneRegressionTest, MaxSeverityAllowed) {
    uint8_t epitope[] = {0x01};
    uint32_t antigen_id;

    int result = brain_immune_present_antigen(
        system, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        10,  // Max severity
        0, &antigen_id
    );
    EXPECT_EQ(result, 0);

    const brain_antigen_t* ag = brain_immune_get_antigen(system, antigen_id);
    EXPECT_EQ(ag->severity, 10u);
}

/* ============================================================================
 * ID Management Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, AntigenIDsAreUnique) {
    uint8_t epitope[] = {0x01};
    uint32_t ids[5];

    for (int i = 0; i < 5; i++) {
        brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                      epitope, sizeof(epitope), 5, 0, &ids[i]);
    }

    // All IDs should be unique
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(BrainImmuneRegressionTest, BCellIDsAreUnique) {
    uint8_t epitope[] = {0x01};
    uint32_t antigen_id;
    uint32_t b_cell_ids[3];

    for (int i = 0; i < 3; i++) {
        brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                      epitope, sizeof(epitope), 5, 0, &antigen_id);
        brain_immune_activate_b_cell(system, antigen_id, &b_cell_ids[i]);
    }

    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 3; j++) {
            EXPECT_NE(b_cell_ids[i], b_cell_ids[j]);
        }
    }
}

/* ============================================================================
 * State Transition Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, BCellStateTransitions) {
    uint8_t epitope[] = {0x11};
    uint32_t antigen_id, b_cell_id, t_cell_id;

    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);

    // Find B cell and verify ACTIVATED state
    brain_b_cell_t* b_cell = nullptr;
    for (size_t i = 0; i < system->b_cell_count; i++) {
        if (system->b_cells[i].id == b_cell_id) {
            b_cell = &system->b_cells[i];
            break;
        }
    }
    ASSERT_NE(b_cell, nullptr);
    EXPECT_EQ(b_cell->state, B_CELL_ACTIVATED);

    // T help -> PLASMA
    brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);
    brain_immune_t_help_b(system, t_cell_id, b_cell_id);
    EXPECT_EQ(b_cell->state, B_CELL_PLASMA);

    // To memory
    brain_immune_b_cell_to_memory(system, b_cell_id);
    EXPECT_EQ(b_cell->state, B_CELL_MEMORY);
}

TEST_F(BrainImmuneRegressionTest, PhaseTransitions) {
    EXPECT_EQ(brain_immune_get_phase(system), IMMUNE_PHASE_SURVEILLANCE);

    uint8_t epitope[] = {0x22};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);
    EXPECT_EQ(brain_immune_get_phase(system), IMMUNE_PHASE_RECOGNITION);

    uint32_t b_cell_id;
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    EXPECT_EQ(brain_immune_get_phase(system), IMMUNE_PHASE_ACTIVATION);
}

/* ============================================================================
 * Callback Safety Regression Tests
 * ============================================================================ */

static std::atomic<int> callback_atomic_count{0};

static void atomic_callback(brain_immune_system_t*, const brain_antigen_t*, void*) {
    callback_atomic_count++;
}

TEST_F(BrainImmuneRegressionTest, CallbackWithNullUserData) {
    callback_atomic_count = 0;
    brain_immune_set_antigen_callback(system, atomic_callback, nullptr);

    uint8_t epitope[] = {0x01};
    uint32_t antigen_id;
    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 0, &antigen_id);

    EXPECT_EQ(callback_atomic_count.load(), 1);
}

TEST_F(BrainImmuneRegressionTest, SetCallbackNullSystem) {
    int result = brain_immune_set_antigen_callback(nullptr, atomic_callback, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainImmuneRegressionTest, SetCallbackNull) {
    int result = brain_immune_set_antigen_callback(system, nullptr, nullptr);
    EXPECT_EQ(result, 0);  // Should succeed (clears callback)
}

/* ============================================================================
 * Affinity Calculation Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, AffinitySymmetric) {
    uint8_t pattern1[] = {0x01, 0x02, 0x03};
    uint8_t pattern2[] = {0x01, 0xFF, 0x03};

    float affinity1 = brain_immune_compute_affinity(pattern1, 3, pattern2, 3);
    float affinity2 = brain_immune_compute_affinity(pattern2, 3, pattern1, 3);

    EXPECT_FLOAT_EQ(affinity1, affinity2);
}

TEST_F(BrainImmuneRegressionTest, AffinityDifferentLengths) {
    uint8_t short_pattern[] = {0x01, 0x02};
    uint8_t long_pattern[] = {0x01, 0x02, 0x03, 0x04};

    float affinity = brain_immune_compute_affinity(
        short_pattern, sizeof(short_pattern),
        long_pattern, sizeof(long_pattern)
    );

    // With fuzzy matching:
    // - exact_score: 2/4 = 0.5 (weight 0.5) = 0.25
    // - bit_score: 16/16 = 1.0 (weight 0.3) = 0.3 (all bits match in overlapping region)
    // - length_score: 2/4 = 0.5 (weight 0.2) = 0.1
    // Total: 0.25 + 0.3 + 0.1 = 0.65
    EXPECT_GT(affinity, 0.5f);
    EXPECT_LT(affinity, 0.8f);
}

TEST_F(BrainImmuneRegressionTest, AffinityEmptyPatternsReturnZero) {
    uint8_t pattern[] = {0x01};

    EXPECT_FLOAT_EQ(brain_immune_compute_affinity(nullptr, 0, pattern, 1), 0.0f);
    EXPECT_FLOAT_EQ(brain_immune_compute_affinity(pattern, 1, nullptr, 0), 0.0f);
    EXPECT_FLOAT_EQ(brain_immune_compute_affinity(pattern, 0, pattern, 0), 0.0f);
}

/* ============================================================================
 * Capacity Limit Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, AntigenCapacityEnforced) {
    brain_immune_config_t small_cfg;
    brain_immune_default_config(&small_cfg);
    small_cfg.max_antigens = 3;

    brain_immune_system_t* small_sys = brain_immune_create(&small_cfg);
    ASSERT_NE(small_sys, nullptr);
    brain_immune_start(small_sys);

    uint8_t epitope[] = {0x01};
    uint32_t antigen_id;

    // Fill to capacity
    for (size_t i = 0; i < 3; i++) {
        int result = brain_immune_present_antigen(small_sys, ANTIGEN_SOURCE_MANUAL,
                                                   epitope, 1, 5, 0, &antigen_id);
        EXPECT_EQ(result, 0);
    }

    // Exceed capacity
    int result = brain_immune_present_antigen(small_sys, ANTIGEN_SOURCE_MANUAL,
                                               epitope, 1, 5, 0, &antigen_id);
    EXPECT_EQ(result, -1);

    brain_immune_destroy(small_sys);
}

TEST_F(BrainImmuneRegressionTest, BCellCapacityEnforced) {
    brain_immune_config_t small_cfg;
    brain_immune_default_config(&small_cfg);
    small_cfg.max_b_cells = 2;
    small_cfg.max_antigens = 10;

    brain_immune_system_t* small_sys = brain_immune_create(&small_cfg);
    ASSERT_NE(small_sys, nullptr);
    brain_immune_start(small_sys);

    uint8_t epitope[] = {0x01};
    uint32_t antigen_ids[3], b_cell_id;

    // Present 3 antigens
    for (int i = 0; i < 3; i++) {
        brain_immune_present_antigen(small_sys, ANTIGEN_SOURCE_MANUAL,
                                      epitope, 1, 5, 0, &antigen_ids[i]);
    }

    // First two B cells should succeed
    EXPECT_EQ(brain_immune_activate_b_cell(small_sys, antigen_ids[0], &b_cell_id), 0);
    EXPECT_EQ(brain_immune_activate_b_cell(small_sys, antigen_ids[1], &b_cell_id), 0);

    // Third should fail
    EXPECT_EQ(brain_immune_activate_b_cell(small_sys, antigen_ids[2], &b_cell_id), -1);

    brain_immune_destroy(small_sys);
}

/* ============================================================================
 * Neutralization Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, NeutralizeIdempotent) {
    uint8_t epitope[] = {0xEE};
    uint32_t antigen_id, b_cell_id, t_cell_id, antibody_id;

    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, 1, 5, 0, &antigen_id);
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);
    brain_immune_t_help_b(system, t_cell_id, b_cell_id);
    brain_immune_produce_antibody(system, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // First neutralization
    int result1 = brain_immune_neutralize(system, antigen_id, antibody_id);
    EXPECT_EQ(result1, 0);
    uint64_t count1 = system->stats.threats_neutralized;

    // Second neutralization (same antigen)
    int result2 = brain_immune_neutralize(system, antigen_id, antibody_id);
    EXPECT_EQ(result2, 0);

    // Count should increase (we allow re-neutralization)
    EXPECT_GE(system->stats.threats_neutralized, count1);
}

TEST_F(BrainImmuneRegressionTest, NeutralizeInvalidAntigenFails) {
    uint8_t epitope[] = {0xFF};
    uint32_t antigen_id, b_cell_id, t_cell_id, antibody_id;

    brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, 1, 5, 0, &antigen_id);
    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);
    brain_immune_t_help_b(system, t_cell_id, b_cell_id);
    brain_immune_produce_antibody(system, b_cell_id, ANTIBODY_IGG, &antibody_id);

    int result = brain_immune_neutralize(system, 99999, antibody_id);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Update Cycle Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, UpdateWithZeroDeltaSafe) {
    int result = brain_immune_update(system, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainImmuneRegressionTest, UpdateWithLargeDeltaSafe) {
    int result = brain_immune_update(system, 100000);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainImmuneRegressionTest, UpdateMultipleTimes) {
    for (int i = 0; i < 100; i++) {
        int result = brain_immune_update(system, 10);
        EXPECT_EQ(result, 0);
    }
}

/* ============================================================================
 * String Conversion Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, StringConversionsNotNull) {
    EXPECT_NE(brain_immune_phase_to_string(IMMUNE_PHASE_SURVEILLANCE), nullptr);
    EXPECT_NE(brain_immune_phase_to_string(IMMUNE_PHASE_EFFECTOR), nullptr);
    EXPECT_NE(brain_immune_b_cell_state_to_string(B_CELL_NAIVE), nullptr);
    EXPECT_NE(brain_immune_t_cell_type_to_string(T_CELL_KILLER), nullptr);
    EXPECT_NE(brain_immune_cytokine_to_string(BRAIN_CYTOKINE_IL6), nullptr);
    EXPECT_NE(brain_immune_inflammation_to_string(INFLAMMATION_STORM), nullptr);
}

TEST_F(BrainImmuneRegressionTest, StringConversionsInvalidValues) {
    // Out of range values should return "UNKNOWN"
    EXPECT_STREQ(brain_immune_phase_to_string((brain_immune_phase_t)999), "UNKNOWN");
    EXPECT_STREQ(brain_immune_b_cell_state_to_string((brain_b_cell_state_t)999), "UNKNOWN");
    EXPECT_STREQ(brain_immune_t_cell_type_to_string((brain_t_cell_type_t)999), "UNKNOWN");
}

/* ============================================================================
 * Stats Regression Tests
 * ============================================================================ */

TEST_F(BrainImmuneRegressionTest, StatsInitializedToZero) {
    brain_immune_stats_t stats;
    brain_immune_get_stats(system, &stats);

    EXPECT_EQ(stats.active_b_cells, 0u);
    EXPECT_EQ(stats.active_t_cells, 0u);
    EXPECT_EQ(stats.active_antibodies, 0u);
    EXPECT_EQ(stats.threats_neutralized, 0u);
    EXPECT_EQ(stats.antigens_processed, 0u);
}

TEST_F(BrainImmuneRegressionTest, StatsAccumulateCorrectly) {
    uint8_t epitope[] = {0x01};
    uint32_t antigen_id, b_cell_id, t_cell_id;

    for (int i = 0; i < 5; i++) {
        brain_immune_present_antigen(system, ANTIGEN_SOURCE_MANUAL,
                                      epitope, 1, 5, 0, &antigen_id);
    }

    brain_immune_stats_t stats;
    brain_immune_get_stats(system, &stats);
    EXPECT_EQ(stats.antigens_processed, 5u);

    brain_immune_activate_b_cell(system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(system, antigen_id, &t_cell_id);

    brain_immune_get_stats(system, &stats);
    EXPECT_EQ(stats.active_b_cells, 1u);
    EXPECT_EQ(stats.active_t_cells, 1u);
}
