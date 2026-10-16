/**
 * @file test_pattern_db_immune.cpp
 * @brief Unit tests for pattern database-immune bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Tests for pattern database-immune bidirectional integration
 * WHY:  Ensure immune→pattern_db and pattern_db→immune pathways work correctly
 * HOW:  Test cytokine weight modulation, inflammation effects, pattern matching
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "security/immune/nimcp_pattern_db_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_pattern_db.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PatternDbImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    nimcp_pattern_db_t pattern_db;
    pattern_db_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        brain_immune_start(immune_system);

        /* Create pattern database */
        pattern_db = nimcp_pattern_db_create(NULL);
        ASSERT_NE(pattern_db, nullptr);

        /* Create pattern_db-immune bridge */
        pattern_db_immune_config_t config;
        pattern_db_immune_default_config(&config);
        bridge = pattern_db_immune_create(&config, pattern_db, immune_system);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            pattern_db_immune_destroy(bridge);
        }
        if (pattern_db) {
            nimcp_pattern_db_destroy(pattern_db);
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(PatternDbImmuneTest, CreateDestroy) {
    pattern_db_immune_config_t config;
    ASSERT_EQ(pattern_db_immune_default_config(&config), 0);

    pattern_db_immune_bridge_t* test_bridge =
        pattern_db_immune_create(&config, pattern_db, immune_system);
    ASSERT_NE(test_bridge, nullptr);

    pattern_db_immune_destroy(test_bridge);
}

TEST_F(PatternDbImmuneTest, DefaultConfig) {
    pattern_db_immune_config_t config;
    ASSERT_EQ(pattern_db_immune_default_config(&config), 0);

    /* Verify defaults */
    EXPECT_TRUE(config.enable_cytokine_weight_modulation);
    EXPECT_TRUE(config.enable_inflammation_priority_boost);
    EXPECT_TRUE(config.enable_pattern_match_antigen_presentation);
    EXPECT_TRUE(config.enable_memory_cell_pattern_sync);
    EXPECT_TRUE(config.enable_affinity_based_refinement);
    EXPECT_TRUE(config.enable_auto_pattern_pruning);

    EXPECT_FLOAT_EQ(config.max_weight_multiplier, 3.0f);
    EXPECT_FLOAT_EQ(config.min_weight_multiplier, 0.5f);
    EXPECT_EQ(config.max_patterns_from_memory, 100);
}

TEST_F(PatternDbImmuneTest, CreateWithNullPatternDb) {
    pattern_db_immune_config_t config;
    pattern_db_immune_default_config(&config);

    pattern_db_immune_bridge_t* null_bridge =
        pattern_db_immune_create(&config, nullptr, immune_system);
    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(PatternDbImmuneTest, CreateWithNullImmuneSystem) {
    pattern_db_immune_config_t config;
    pattern_db_immune_default_config(&config);

    pattern_db_immune_bridge_t* null_bridge =
        pattern_db_immune_create(&config, pattern_db, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(PatternDbImmuneTest, DefaultConfigNullParam) {
    EXPECT_NE(pattern_db_immune_default_config(nullptr), 0);
}

/* ============================================================================
 * Immune → Pattern Database Tests
 * ============================================================================ */

TEST_F(PatternDbImmuneTest, UpdateBridge) {
    int result = pattern_db_immune_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PatternDbImmuneTest, ApplyModulation) {
    int result = pattern_db_immune_apply_modulation(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(PatternDbImmuneTest, GetWeightMultiplierBaseline) {
    /* Update to compute effects */
    pattern_db_immune_update(bridge);

    float multiplier = pattern_db_immune_get_weight_multiplier(bridge);
    /* At baseline, should be around 1.0 (with cytokine adjustments) */
    EXPECT_GE(multiplier, 0.5f);
    EXPECT_LE(multiplier, 3.0f);
}

TEST_F(PatternDbImmuneTest, GetWeightMultiplierWithInflammation) {
    /* Trigger inflammation */
    uint32_t antigen_id;
    uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF};
    brain_immune_present_antigen(
        immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        8,
        0,
        &antigen_id
    );

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    /* Update to compute effects */
    pattern_db_immune_update(bridge);

    float multiplier = pattern_db_immune_get_weight_multiplier(bridge);
    /* With inflammation, multiplier should increase */
    EXPECT_GE(multiplier, 1.0f);
}

TEST_F(PatternDbImmuneTest, SyncMemoryToPatterns) {
    int result = pattern_db_immune_sync_memory_to_patterns(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Pattern Database → Immune Tests
 * ============================================================================ */

TEST_F(PatternDbImmuneTest, PresentMatch) {
    /* Create a mock match result */
    nimcp_pattern_match_result_t match_result;
    memset(&match_result, 0, sizeof(match_result));
    match_result.pattern_id = 1;
    match_result.threat_score = 0.8f;  /* High score */
    match_result.category = NIMCP_PATTERN_SHELL_INJECTION;

    uint32_t antigen_id = 0;
    int result = pattern_db_immune_present_match(bridge, &match_result, &antigen_id);
    EXPECT_EQ(result, 0);
}

TEST_F(PatternDbImmuneTest, PresentMatchLowScore) {
    /* Create a match result with low score (below threshold) */
    nimcp_pattern_match_result_t match_result;
    memset(&match_result, 0, sizeof(match_result));
    match_result.pattern_id = 2;
    match_result.threat_score = 0.3f;  /* Low score */
    match_result.category = NIMCP_PATTERN_SQL_INJECTION;

    uint32_t antigen_id = 0;
    int result = pattern_db_immune_present_match(bridge, &match_result, &antigen_id);
    /* Low score should not present antigen (returns 0 but antigen_id unchanged) */
    EXPECT_EQ(result, 0);
}

TEST_F(PatternDbImmuneTest, RefineFromAffinity) {
    int result = pattern_db_immune_refine_from_affinity(bridge, 1, 100, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(PatternDbImmuneTest, PruneUnused) {
    uint32_t pruned = pattern_db_immune_prune_unused(bridge);
    /* With no patterns, should prune 0 */
    EXPECT_EQ(pruned, 0);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(PatternDbImmuneTest, GetMapping) {
    pattern_immune_mapping_t mapping;
    int result = pattern_db_immune_get_mapping(bridge, 999, &mapping);
    /* Non-existent pattern should return -1 */
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(PatternDbImmuneTest, BioAsyncConnect) {
    int result = pattern_db_immune_connect_bio_async(bridge);
    (void)result;  /* May fail if router not initialized */

    bool connected = pattern_db_immune_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(PatternDbImmuneTest, BioAsyncDisconnect) {
    pattern_db_immune_connect_bio_async(bridge);
    int result = pattern_db_immune_disconnect_bio_async(bridge);
    /* Should succeed even if not connected */
    (void)result;

    bool connected = pattern_db_immune_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(PatternDbImmuneTest, BioAsyncIsConnected) {
    bool connected = pattern_db_immune_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Null Parameter Tests
 * ============================================================================ */

TEST_F(PatternDbImmuneTest, NullBridgeGetMultiplier) {
    float multiplier = pattern_db_immune_get_weight_multiplier(nullptr);
    EXPECT_FLOAT_EQ(multiplier, 1.0f);
}

TEST_F(PatternDbImmuneTest, NullBridgeUpdate) {
    int result = pattern_db_immune_update(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PatternDbImmuneTest, NullBridgeApplyModulation) {
    int result = pattern_db_immune_apply_modulation(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PatternDbImmuneTest, NullBridgePresentMatch) {
    nimcp_pattern_match_result_t match;
    memset(&match, 0, sizeof(match));
    uint32_t antigen_id;

    int result = pattern_db_immune_present_match(nullptr, &match, &antigen_id);
    EXPECT_EQ(result, -1);
}

TEST_F(PatternDbImmuneTest, NullMatchResult) {
    uint32_t antigen_id;
    int result = pattern_db_immune_present_match(bridge, nullptr, &antigen_id);
    EXPECT_EQ(result, -1);
}
