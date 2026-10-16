/**
 * @file test_immune_tolerance.cpp
 * @brief Unit tests for Immune Tolerance Module
 * @date 2025-12-12
 *
 * Tests self-pattern registration, central tolerance (clonal deletion),
 * peripheral tolerance (anergy), and self vs non-self discrimination.
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_immune_tolerance.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImmuneToleranceTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    tolerance_system_t* tolerance = nullptr;
    tolerance_config_t config;

    void SetUp() override {
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        tolerance_default_config(&config);
        tolerance = tolerance_create(&config, immune_system);
        ASSERT_NE(tolerance, nullptr);
    }

    void TearDown() override {
        if (tolerance) {
            tolerance_destroy(tolerance);
            tolerance = nullptr;
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, DefaultConfigIsValid) {
    tolerance_config_t cfg;
    int result = tolerance_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.max_self_patterns, 0u);
    EXPECT_GT(cfg.self_match_threshold, 0.0f);
    EXPECT_LE(cfg.self_match_threshold, 1.0f);
}

TEST_F(ImmuneToleranceTest, DefaultConfigNullFails) {
    int result = tolerance_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ImmuneToleranceTest, CreateWithNullConfig) {
    tolerance_system_t* sys = tolerance_create(nullptr, immune_system);
    ASSERT_NE(sys, nullptr);
    tolerance_destroy(sys);
}

TEST_F(ImmuneToleranceTest, DestroyNullSafe) {
    tolerance_destroy(nullptr);
}

/* ============================================================================
 * Self Pattern Registration Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, RegisterSelfPattern) {
    uint8_t pattern[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t pattern_id;

    int result = tolerance_register_self_pattern(tolerance, pattern, sizeof(pattern),
                                                 "Normal operation", &pattern_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(pattern_id, 0u);
}

TEST_F(ImmuneToleranceTest, RegisterMultipleSelfPatterns) {
    uint8_t pattern1[] = {0x01, 0x02, 0x03};
    uint8_t pattern2[] = {0x04, 0x05, 0x06};
    uint32_t id1, id2;

    tolerance_register_self_pattern(tolerance, pattern1, sizeof(pattern1), "Pattern 1", &id1);
    tolerance_register_self_pattern(tolerance, pattern2, sizeof(pattern2), "Pattern 2", &id2);

    EXPECT_NE(id1, id2);
    EXPECT_EQ(tolerance_get_self_patterns_count(tolerance), 2u);
}

TEST_F(ImmuneToleranceTest, RegisterNullPatternFails) {
    uint32_t pattern_id;
    int result = tolerance_register_self_pattern(tolerance, nullptr, 5, "Null", &pattern_id);
    EXPECT_EQ(result, -1);
}

TEST_F(ImmuneToleranceTest, RegisterZeroLengthFails) {
    uint8_t pattern[] = {0x01};
    uint32_t pattern_id;
    int result = tolerance_register_self_pattern(tolerance, pattern, 0, "Empty", &pattern_id);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Self Check Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, CheckSelfMatchesRegistered) {
    uint8_t self_pattern[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint32_t pattern_id;
    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                    "Self", &pattern_id);

    uint32_t matched_id;
    float affinity;
    bool is_self = tolerance_check_self(tolerance, self_pattern, sizeof(self_pattern),
                                        &matched_id, &affinity);

    EXPECT_TRUE(is_self);
    EXPECT_EQ(matched_id, pattern_id);
    EXPECT_GT(affinity, 0.9f);  // High affinity for exact match
}

TEST_F(ImmuneToleranceTest, CheckNonSelfDoesNotMatch) {
    uint8_t self_pattern[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t foreign_pattern[] = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB};

    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                    "Self", nullptr);

    bool is_self = tolerance_check_self(tolerance, foreign_pattern, sizeof(foreign_pattern),
                                        nullptr, nullptr);
    EXPECT_FALSE(is_self);
}

TEST_F(ImmuneToleranceTest, CheckSelfSimilarPattern) {
    uint8_t self_pattern[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t similar_pattern[] = {0x01, 0x02, 0x03, 0x04, 0x06};  // One byte different

    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                    "Self", nullptr);

    float affinity;
    bool is_self = tolerance_check_self(tolerance, similar_pattern, sizeof(similar_pattern),
                                        nullptr, &affinity);

    // Depends on threshold setting - similar patterns may or may not match
    EXPECT_GT(affinity, 0.5f);  // Should have some affinity
}

/* ============================================================================
 * Self Pattern Removal Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, RemoveSelfPattern) {
    uint8_t pattern[] = {0x01, 0x02, 0x03};
    uint32_t pattern_id;
    tolerance_register_self_pattern(tolerance, pattern, sizeof(pattern), "Test", &pattern_id);

    size_t before = tolerance_get_self_patterns_count(tolerance);

    int result = tolerance_remove_self_pattern(tolerance, pattern_id);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(tolerance_get_self_patterns_count(tolerance), before - 1);
}

TEST_F(ImmuneToleranceTest, RemoveNonExistentFails) {
    int result = tolerance_remove_self_pattern(tolerance, 99999);
    EXPECT_EQ(result, -1);
}

TEST_F(ImmuneToleranceTest, ClearAllSelfPatterns) {
    uint8_t pattern1[] = {0x01, 0x02};
    uint8_t pattern2[] = {0x03, 0x04};
    tolerance_register_self_pattern(tolerance, pattern1, sizeof(pattern1), "P1", nullptr);
    tolerance_register_self_pattern(tolerance, pattern2, sizeof(pattern2), "P2", nullptr);

    int result = tolerance_clear_self_patterns(tolerance);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(tolerance_get_self_patterns_count(tolerance), 0u);
}

/* ============================================================================
 * Central Tolerance Tests (Thymic Selection)
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, CentralSelectionPassesNonSelfReactive) {
    // Register self pattern
    uint8_t self_pattern[] = {0x01, 0x02, 0x03, 0x04};
    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                    "Self", nullptr);

    // Cell with non-self reactive receptor
    uint8_t receptor[] = {0xFF, 0xFE, 0xFD, 0xFC};
    selection_outcome_t outcome;

    int result = tolerance_central_selection(tolerance, 1, false, receptor, sizeof(receptor),
                                             &outcome);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(outcome, SELECTION_PASS);
}

TEST_F(ImmuneToleranceTest, CentralSelectionDeletesHighAffinitySelfReactive) {
    // Register self pattern
    uint8_t self_pattern[] = {0x01, 0x02, 0x03, 0x04};
    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                    "Self", nullptr);

    // Cell with self-reactive receptor (same as self)
    selection_outcome_t outcome;
    int result = tolerance_central_selection(tolerance, 1, false, self_pattern,
                                             sizeof(self_pattern), &outcome);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(outcome, SELECTION_DELETE);
}

TEST_F(ImmuneToleranceTest, DeleteCell) {
    int result = tolerance_delete_cell(tolerance, 1, false);
    EXPECT_EQ(result, 0);

    tolerance_stats_t stats;
    tolerance_get_stats(tolerance, &stats);
    EXPECT_GT(stats.cells_deleted, 0u);
}

/* ============================================================================
 * Peripheral Tolerance Tests (Anergy)
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, InduceAnergy) {
    uint8_t self_pattern[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t pattern_id;
    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                    "Self", &pattern_id);

    int result = tolerance_induce_anergy(tolerance, 1, false, pattern_id, 0.8f);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(tolerance_is_anergic(tolerance, 1, false));
}

TEST_F(ImmuneToleranceTest, IsAnergicReturnsFalseForNormal) {
    EXPECT_FALSE(tolerance_is_anergic(tolerance, 1, false));
}

TEST_F(ImmuneToleranceTest, ReverseAnergy) {
    uint8_t self_pattern[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t pattern_id;
    tolerance_register_self_pattern(tolerance, self_pattern, sizeof(self_pattern),
                                    "Self", &pattern_id);

    tolerance_induce_anergy(tolerance, 1, false, pattern_id, 0.8f);
    EXPECT_TRUE(tolerance_is_anergic(tolerance, 1, false));

    int result = tolerance_reverse_anergy(tolerance, 1, false);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(tolerance_is_anergic(tolerance, 1, false));
}

TEST_F(ImmuneToleranceTest, ReverseAnergyNonAnergicFails) {
    int result = tolerance_reverse_anergy(tolerance, 999, false);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Phase Management Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, SetPhase) {
    int result = tolerance_set_phase(tolerance, TOLERANCE_PHASE_PERIPHERAL);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(tolerance_get_phase(tolerance), TOLERANCE_PHASE_PERIPHERAL);
}

TEST_F(ImmuneToleranceTest, GetPhase) {
    tolerance_phase_t phase = tolerance_get_phase(tolerance);
    // Default phase
    EXPECT_TRUE(phase == TOLERANCE_PHASE_TRAINING || phase == TOLERANCE_PHASE_OPERATIONAL);
}

TEST_F(ImmuneToleranceTest, PhaseTransitions) {
    tolerance_set_phase(tolerance, TOLERANCE_PHASE_TRAINING);
    EXPECT_EQ(tolerance_get_phase(tolerance), TOLERANCE_PHASE_TRAINING);

    tolerance_set_phase(tolerance, TOLERANCE_PHASE_CENTRAL);
    EXPECT_EQ(tolerance_get_phase(tolerance), TOLERANCE_PHASE_CENTRAL);

    tolerance_set_phase(tolerance, TOLERANCE_PHASE_PERIPHERAL);
    EXPECT_EQ(tolerance_get_phase(tolerance), TOLERANCE_PHASE_PERIPHERAL);

    tolerance_set_phase(tolerance, TOLERANCE_PHASE_OPERATIONAL);
    EXPECT_EQ(tolerance_get_phase(tolerance), TOLERANCE_PHASE_OPERATIONAL);
}

/* ============================================================================
 * Threshold Tuning Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, SetSelfThreshold) {
    int result = tolerance_set_self_threshold(tolerance, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmuneToleranceTest, SetSelfThresholdOutOfRange) {
    EXPECT_EQ(tolerance_set_self_threshold(tolerance, -0.1f), -1);
    EXPECT_EQ(tolerance_set_self_threshold(tolerance, 1.5f), -1);
}

TEST_F(ImmuneToleranceTest, SetCentralThreshold) {
    int result = tolerance_set_central_threshold(tolerance, 0.95f);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmuneToleranceTest, SetAnergyThreshold) {
    int result = tolerance_set_anergy_threshold(tolerance, 0.75f);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, GetStats) {
    tolerance_stats_t stats;
    int result = tolerance_get_stats(tolerance, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(ImmuneToleranceTest, StatsTrackSelfChecks) {
    uint8_t pattern[] = {0x01, 0x02, 0x03};
    tolerance_register_self_pattern(tolerance, pattern, sizeof(pattern), "Self", nullptr);

    tolerance_check_self(tolerance, pattern, sizeof(pattern), nullptr, nullptr);
    tolerance_check_self(tolerance, pattern, sizeof(pattern), nullptr, nullptr);

    tolerance_stats_t stats;
    tolerance_get_stats(tolerance, &stats);
    EXPECT_GE(stats.total_self_checks, 2u);
}

/* ============================================================================
 * Pattern Query Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, GetPatternById) {
    uint8_t pattern[] = {0x01, 0x02, 0x03, 0x04};
    uint32_t pattern_id;
    tolerance_register_self_pattern(tolerance, pattern, sizeof(pattern), "Test", &pattern_id);

    const self_pattern_t* retrieved = tolerance_get_pattern(tolerance, pattern_id);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, pattern_id);
    EXPECT_EQ(retrieved->pattern_len, sizeof(pattern));
}

TEST_F(ImmuneToleranceTest, GetPatternNonExistent) {
    const self_pattern_t* retrieved = tolerance_get_pattern(tolerance, 99999);
    EXPECT_EQ(retrieved, nullptr);
}

/* ============================================================================
 * Affinity Computation Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, ComputeAffinityExactMatch) {
    uint8_t pattern[] = {0x01, 0x02, 0x03, 0x04};
    float affinity = tolerance_compute_affinity(pattern, sizeof(pattern),
                                                pattern, sizeof(pattern));
    EXPECT_GT(affinity, 0.99f);  // Near-perfect match
}

TEST_F(ImmuneToleranceTest, ComputeAffinityNoMatch) {
    uint8_t pattern1[] = {0x00, 0x00, 0x00, 0x00};
    uint8_t pattern2[] = {0xFF, 0xFF, 0xFF, 0xFF};
    float affinity = tolerance_compute_affinity(pattern1, sizeof(pattern1),
                                                pattern2, sizeof(pattern2));
    EXPECT_LT(affinity, 0.5f);  // Low match
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, PhaseToString) {
    EXPECT_STREQ(tolerance_phase_to_string(TOLERANCE_PHASE_TRAINING), "TRAINING");
    EXPECT_STREQ(tolerance_phase_to_string(TOLERANCE_PHASE_CENTRAL), "CENTRAL");
    EXPECT_STREQ(tolerance_phase_to_string(TOLERANCE_PHASE_PERIPHERAL), "PERIPHERAL");
    EXPECT_STREQ(tolerance_phase_to_string(TOLERANCE_PHASE_OPERATIONAL), "OPERATIONAL");
}

TEST_F(ImmuneToleranceTest, SelectionToString) {
    EXPECT_STREQ(tolerance_selection_to_string(SELECTION_PASS), "PASS");
    EXPECT_STREQ(tolerance_selection_to_string(SELECTION_DELETE), "DELETE");
    EXPECT_STREQ(tolerance_selection_to_string(SELECTION_ANERGIZE), "ANERGIZE");
}

TEST_F(ImmuneToleranceTest, CellStateToString) {
    EXPECT_STREQ(tolerance_cell_state_to_string(CELL_TOLERANT), "TOLERANT");
    EXPECT_STREQ(tolerance_cell_state_to_string(CELL_ANERGIC), "ANERGIC");
    EXPECT_STREQ(tolerance_cell_state_to_string(CELL_DELETED), "DELETED");
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(ImmuneToleranceTest, NullSystemChecks) {
    EXPECT_EQ(tolerance_register_self_pattern(nullptr, (uint8_t*)"test", 4, "t", nullptr), -1);
    EXPECT_FALSE(tolerance_check_self(nullptr, (uint8_t*)"test", 4, nullptr, nullptr));
    EXPECT_EQ(tolerance_set_phase(nullptr, TOLERANCE_PHASE_TRAINING), -1);
}

TEST_F(ImmuneToleranceTest, MaxSelfPatterns) {
    // Try to exceed max patterns
    for (size_t i = 0; i < config.max_self_patterns + 5; i++) {
        uint8_t pattern[4];
        pattern[0] = (uint8_t)(i >> 24);
        pattern[1] = (uint8_t)(i >> 16);
        pattern[2] = (uint8_t)(i >> 8);
        pattern[3] = (uint8_t)(i);
        tolerance_register_self_pattern(tolerance, pattern, sizeof(pattern), "Pat", nullptr);
    }

    EXPECT_LE(tolerance_get_self_patterns_count(tolerance), config.max_self_patterns);
}
