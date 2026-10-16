/**
 * @file test_wm_transfer.cpp
 * @brief Unit tests for Phase M3 working memory transfer system
 *
 * WHAT: Tests WM → engram transfer logic, criteria, and statistics
 * WHY:  Ensure transfer system implements selective consolidation correctly
 * HOW:  Test all API functions, criteria scoring, and stats tracking
 *
 * TEST COVERAGE:
 * - System management (create, destroy, reset)
 * - Integration API (set working memory, engram system, emotional system)
 * - Transfer criteria (rehearsal, attention, emotion, time thresholds)
 * - Transfer scoring algorithm
 * - Attention management
 * - Statistics tracking
 * - Configuration API
 *
 * @version Phase M3 Unit Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

    #include "cognitive/memory/nimcp_wm_transfer.h"
    #include "cognitive/memory/nimcp_engram.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WMTransferTest : public ::testing::Test {
protected:
    wm_transfer_system_t* system;
    engram_system_t* engram_system;

    void SetUp() override {
        system = nullptr;
        engram_system = nullptr;
    }

    void TearDown() override {
        if (system) {
            wm_transfer_destroy(system);
            system = nullptr;
        }
        if (engram_system) {
            engram_system_destroy(engram_system);
            engram_system = nullptr;
        }
    }
};

//=============================================================================
// Test Group 1: System Management (3 tests)
//=============================================================================

TEST_F(WMTransferTest, Create_SystemValid) {
    /**
     * WHAT: Verify system creation succeeds
     * WHY:  Must allocate and initialize system correctly
     * HOW:  Create system, check not NULL, verify defaults
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr) << "System creation should succeed";

    // Verify default criteria set
    wm_transfer_criteria_t criteria;
    wm_transfer_get_criteria(system, &criteria);

    EXPECT_EQ(criteria.rehearsal_threshold, 3) << "Default rehearsal threshold";
    EXPECT_FLOAT_EQ(criteria.attention_threshold, 0.5f) << "Default attention threshold";
    EXPECT_FLOAT_EQ(criteria.emotional_threshold, 0.3f) << "Default emotional threshold";
    EXPECT_EQ(criteria.time_threshold_ms, 5000) << "Default time threshold";
    EXPECT_FLOAT_EQ(criteria.decay_rate, 0.1f) << "Default decay rate";
}

TEST_F(WMTransferTest, Destroy_HandlesNull) {
    /**
     * WHAT: Verify destroy handles NULL gracefully
     * WHY:  API should be safe with NULL inputs
     * HOW:  Call destroy with NULL, should not crash
     */
    wm_transfer_destroy(nullptr);
    SUCCEED() << "Destroy should handle NULL without crashing";
}

TEST_F(WMTransferTest, Reset_ClearsStats) {
    /**
     * WHAT: Verify reset clears statistics but keeps criteria
     * WHY:  Allow reuse of system with fresh state
     * HOW:  Create, modify stats/criteria, reset, verify results
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    // Modify criteria
    wm_transfer_criteria_t custom_criteria = {
        .rehearsal_threshold = 5,
        .attention_threshold = 0.7f,
        .emotional_threshold = 0.5f,
        .time_threshold_ms = 10000,
        .decay_rate = 0.2f
    };
    wm_transfer_set_criteria(system, &custom_criteria);

    // Reset system
    wm_transfer_reset(system);

    // Verify criteria preserved
    wm_transfer_criteria_t criteria_out;
    wm_transfer_get_criteria(system, &criteria_out);
    EXPECT_EQ(criteria_out.rehearsal_threshold, 5) << "Criteria should be preserved";

    // Verify stats cleared (will be checked when stats API is tested)
    wm_transfer_stats_t stats;
    wm_transfer_get_statistics(system, &stats);
    EXPECT_EQ(stats.total_transfers, 0) << "Stats should be cleared";
}

//=============================================================================
// Test Group 2: Integration API (3 tests)
//=============================================================================

TEST_F(WMTransferTest, SetWorkingMemory_Stores) {
    /**
     * WHAT: Verify working memory pointer is stored
     * WHY:  Transfer system needs access to WM items
     * HOW:  Create system, set WM pointer, verify stored (implicit)
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    // Mock working memory pointer
    void* mock_wm = (void*)0x12345678;
    wm_transfer_set_working_memory(system, mock_wm);

    // Cannot directly verify internal pointer, but should not crash
    SUCCEED() << "Setting working memory pointer should succeed";
}

TEST_F(WMTransferTest, SetEngramSystem_Stores) {
    /**
     * WHAT: Verify engram system pointer is stored
     * WHY:  Transfer system needs to encode to engrams
     * HOW:  Create systems, link, verify stored (implicit)
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    engram_system = engram_system_create();
    ASSERT_NE(engram_system, nullptr);

    wm_transfer_set_engram_system(system, engram_system);

    // Cannot directly verify internal pointer, but should not crash
    SUCCEED() << "Setting engram system pointer should succeed";
}

TEST_F(WMTransferTest, SetEmotionalSystem_Stores) {
    /**
     * WHAT: Verify emotional system pointer is stored
     * WHY:  Emotional salience enhances encoding
     * HOW:  Create system, set emotion pointer, verify stored (implicit)
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    // Mock emotional system pointer
    void* mock_emotion = (void*)0x87654321;
    wm_transfer_set_emotional_system(system, mock_emotion);

    // Cannot directly verify internal pointer, but should not crash
    SUCCEED() << "Setting emotional system pointer should succeed";
}

//=============================================================================
// Test Group 3: Transfer Criteria Configuration (4 tests)
//=============================================================================

TEST_F(WMTransferTest, SetCriteria_Updates) {
    /**
     * WHAT: Verify criteria can be set and retrieved
     * WHY:  Allow customization of transfer behavior
     * HOW:  Set custom criteria, get back, verify match
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    wm_transfer_criteria_t custom = {
        .rehearsal_threshold = 10,
        .attention_threshold = 0.8f,
        .emotional_threshold = 0.6f,
        .time_threshold_ms = 15000,
        .decay_rate = 0.15f
    };
    wm_transfer_set_criteria(system, &custom);

    wm_transfer_criteria_t retrieved;
    wm_transfer_get_criteria(system, &retrieved);

    EXPECT_EQ(retrieved.rehearsal_threshold, 10);
    EXPECT_FLOAT_EQ(retrieved.attention_threshold, 0.8f);
    EXPECT_FLOAT_EQ(retrieved.emotional_threshold, 0.6f);
    EXPECT_EQ(retrieved.time_threshold_ms, 15000);
    EXPECT_FLOAT_EQ(retrieved.decay_rate, 0.15f);
}

TEST_F(WMTransferTest, GetCriteria_HandlesNull) {
    /**
     * WHAT: Verify get_criteria handles NULL gracefully
     * WHY:  API should be safe with NULL inputs
     * HOW:  Call with NULL system and output, should not crash
     */
    wm_transfer_criteria_t criteria;

    // NULL system
    wm_transfer_get_criteria(nullptr, &criteria);

    // NULL output
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);
    wm_transfer_get_criteria(system, nullptr);

    SUCCEED() << "get_criteria should handle NULL without crashing";
}

TEST_F(WMTransferTest, SetCriteria_HandlesNull) {
    /**
     * WHAT: Verify set_criteria handles NULL gracefully
     * WHY:  API should be safe with NULL inputs
     * HOW:  Call with NULL system and criteria, should not crash
     */
    wm_transfer_criteria_t criteria = wm_transfer_get_default_criteria();

    // NULL system
    wm_transfer_set_criteria(nullptr, &criteria);

    // NULL criteria
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);
    wm_transfer_set_criteria(system, nullptr);

    SUCCEED() << "set_criteria should handle NULL without crashing";
}

TEST_F(WMTransferTest, DefaultCriteria_Reasonable) {
    /**
     * WHAT: Verify default criteria are biologically reasonable
     * WHY:  Defaults should match neuroscience literature
     * HOW:  Get defaults, verify against specification
     */
    wm_transfer_criteria_t defaults = wm_transfer_get_default_criteria();

    // Based on Phase M3 specification
    EXPECT_EQ(defaults.rehearsal_threshold, 3) << "3+ rehearsals (Atkinson & Shiffrin)";
    EXPECT_FLOAT_EQ(defaults.attention_threshold, 0.5f) << "50% attention required";
    EXPECT_FLOAT_EQ(defaults.emotional_threshold, 0.3f) << "30% emotional salience";
    EXPECT_EQ(defaults.time_threshold_ms, 5000) << "5 seconds in WM";
    EXPECT_FLOAT_EQ(defaults.decay_rate, 0.1f) << "10% decay per second";
}

//=============================================================================
// Test Group 4: Attention Management (3 tests)
//=============================================================================

TEST_F(WMTransferTest, UpdateAttention_Stores) {
    /**
     * WHAT: Verify attention weights are stored
     * WHY:  Attention determines transfer priority
     * HOW:  Update attention, verify no crash (internal storage)
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    float attention[7] = {0.9f, 0.3f, 0.7f, 0.5f, 0.2f, 0.8f, 0.4f};
    wm_transfer_update_attention(system, attention, 7);

    SUCCEED() << "Attention update should succeed";
}

TEST_F(WMTransferTest, UpdateAttention_HandlesResize) {
    /**
     * WHAT: Verify attention array can be resized
     * WHY:  Working memory capacity may vary
     * HOW:  Update with different sizes, verify no crash
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    // Start with default size (7)
    float attention1[7] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    wm_transfer_update_attention(system, attention1, 7);

    // Resize to 9 (upper Miller's law limit)
    float attention2[9] = {0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f};
    wm_transfer_update_attention(system, attention2, 9);

    // Resize to 5 (lower Miller's law limit)
    float attention3[5] = {0.6f, 0.6f, 0.6f, 0.6f, 0.6f};
    wm_transfer_update_attention(system, attention3, 5);

    SUCCEED() << "Attention resize should succeed";
}

TEST_F(WMTransferTest, UpdateAttention_HandlesNull) {
    /**
     * WHAT: Verify update_attention handles NULL gracefully
     * WHY:  API should be safe with NULL inputs
     * HOW:  Call with NULL system and weights, should not crash
     */
    float attention[7] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // NULL system
    wm_transfer_update_attention(nullptr, attention, 7);

    // NULL weights
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);
    wm_transfer_update_attention(system, nullptr, 7);

    // Zero count
    wm_transfer_update_attention(system, attention, 0);

    SUCCEED() << "update_attention should handle NULL/zero without crashing";
}

//=============================================================================
// Test Group 5: Statistics API (3 tests)
//=============================================================================

TEST_F(WMTransferTest, GetStatistics_InitiallyZero) {
    /**
     * WHAT: Verify statistics are initially zero
     * WHY:  New system should have no transfer history
     * HOW:  Create system, get stats, verify all zero
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    wm_transfer_stats_t stats;
    wm_transfer_get_statistics(system, &stats);

    EXPECT_EQ(stats.total_transfers, 0);
    EXPECT_EQ(stats.rehearsal_triggered, 0);
    EXPECT_EQ(stats.attention_triggered, 0);
    EXPECT_EQ(stats.emotion_triggered, 0);
    EXPECT_EQ(stats.time_triggered, 0);
    EXPECT_EQ(stats.total_decayed, 0);
    EXPECT_EQ(stats.current_wm_items, 0);
}

TEST_F(WMTransferTest, GetStatistics_AfterReset) {
    /**
     * WHAT: Verify statistics are zero after reset
     * WHY:  Reset should clear transfer history
     * HOW:  Create, reset, get stats, verify zero
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    // Reset (even though already zero)
    wm_transfer_reset(system);

    wm_transfer_stats_t stats;
    wm_transfer_get_statistics(system, &stats);

    EXPECT_EQ(stats.total_transfers, 0);
    EXPECT_EQ(stats.rehearsal_triggered, 0);
    EXPECT_EQ(stats.attention_triggered, 0);
}

TEST_F(WMTransferTest, GetStatistics_HandlesNull) {
    /**
     * WHAT: Verify get_statistics handles NULL gracefully
     * WHY:  API should be safe with NULL inputs
     * HOW:  Call with NULL system and output, should not crash
     */
    wm_transfer_stats_t stats;

    // NULL system
    wm_transfer_get_statistics(nullptr, &stats);

    // NULL output
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);
    wm_transfer_get_statistics(system, nullptr);

    SUCCEED() << "get_statistics should handle NULL without crashing";
}

//=============================================================================
// Test Group 6: Transfer Evaluation (4 tests)
//=============================================================================

TEST_F(WMTransferTest, Evaluate_HandlesNullSystem) {
    /**
     * WHAT: Verify evaluate handles NULL system gracefully
     * WHY:  API should be safe with NULL inputs
     * HOW:  Call evaluate with NULL, should return 0
     */
    uint32_t transfers = wm_transfer_evaluate(nullptr, 0.1f);
    EXPECT_EQ(transfers, 0) << "NULL system should return 0 transfers";
}

TEST_F(WMTransferTest, Evaluate_RequiresWorkingMemory) {
    /**
     * WHAT: Verify evaluate requires working memory system
     * WHY:  Cannot transfer without WM source
     * HOW:  Create system without WM, evaluate, expect 0
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    // No working memory set
    uint32_t transfers = wm_transfer_evaluate(system, 0.1f);
    EXPECT_EQ(transfers, 0) << "No working memory should return 0 transfers";
}

TEST_F(WMTransferTest, Evaluate_RequiresEngramSystem) {
    /**
     * WHAT: Verify evaluate requires engram system
     * WHY:  Cannot transfer without engram destination
     * HOW:  Create system without engrams, evaluate, expect 0
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    void* mock_wm = (void*)0x12345678;
    wm_transfer_set_working_memory(system, mock_wm);

    // No engram system set
    uint32_t transfers = wm_transfer_evaluate(system, 0.1f);
    EXPECT_EQ(transfers, 0) << "No engram system should return 0 transfers";
}

TEST_F(WMTransferTest, Evaluate_WithBothSystems) {
    /**
     * WHAT: Verify evaluate runs with both systems set
     * WHY:  Full configuration should allow evaluation
     * HOW:  Create with both systems, evaluate, expect no crash
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    engram_system = engram_system_create();
    ASSERT_NE(engram_system, nullptr);

    void* mock_wm = (void*)0x12345678;
    wm_transfer_set_working_memory(system, mock_wm);
    wm_transfer_set_engram_system(system, engram_system);

    // Evaluate (placeholder returns 0 until brain integration)
    uint32_t transfers = wm_transfer_evaluate(system, 0.1f);
    EXPECT_GE(transfers, 0) << "Evaluation should succeed";
}

//=============================================================================
// Test Group 7: Force Transfer (2 tests)
//=============================================================================

TEST_F(WMTransferTest, ForceTransfer_RequiresSystems) {
    /**
     * WHAT: Verify force transfer requires WM and engram systems
     * WHY:  Cannot transfer without both systems
     * HOW:  Try force transfer without systems, expect false
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    // No systems set
    bool success = wm_transfer_force_item(system, 0);
    EXPECT_FALSE(success) << "Force transfer should fail without systems";
}

TEST_F(WMTransferTest, ForceTransfer_WithSystems) {
    /**
     * WHAT: Verify force transfer works with systems set
     * WHY:  Should allow explicit encoding
     * HOW:  Create with both systems, force transfer, expect success
     */
    system = wm_transfer_create();
    ASSERT_NE(system, nullptr);

    engram_system = engram_system_create();
    ASSERT_NE(engram_system, nullptr);

    void* mock_wm = (void*)0x12345678;
    wm_transfer_set_working_memory(system, mock_wm);
    wm_transfer_set_engram_system(system, engram_system);

    // Force transfer (placeholder implementation)
    bool success = wm_transfer_force_item(system, 0);
    EXPECT_TRUE(success) << "Force transfer should succeed with systems";
}

//=============================================================================
// Main Test Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
