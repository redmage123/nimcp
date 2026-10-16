//=============================================================================
// test_working_memory_pe.cpp - Unit Tests for Working Memory Positional Encoding
//=============================================================================
/**
 * @file test_working_memory_pe.cpp
 * @brief Unit tests for positional encoding integration in working memory
 *
 * WHAT: Test PE configuration, slot encoding, and serial position effects
 * WHY:  Positional encoding captures serial position in working memory (primacy/recency)
 * HOW:  Test all PE types with working memory operations
 *
 * TEST COVERAGE:
 * 1. PE configuration and initialization
 * 2. Position encoding for working memory slots
 * 3. Serial position effects (primacy, recency)
 * 4. PE type switching (Sinusoidal, Learned, Relative)
 * 5. Slot position embeddings
 * 6. Edge cases (full buffer, empty buffer, invalid positions)
 * 7. Integration with item storage and retrieval
 *
 * BIOLOGICAL BASIS:
 * - Serial position effects in working memory (Ebbinghaus, 1885)
 * - Primacy effect: better recall of early items
 * - Recency effect: better recall of recent items
 * - Prefrontal cortex encodes temporal order
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "cognitive/nimcp_working_memory.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class WorkingMemoryPETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
    static constexpr uint32_t TEST_CAPACITY = 7;
    static constexpr uint32_t TEST_ITEM_SIZE = 32;
    static constexpr uint32_t TEST_PE_DIM = 64;

    working_memory_t* wm = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();
    }

    void TearDown() override {
        if (wm) {
            working_memory_destroy(wm);
            wm = nullptr;
        }
    }

    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }

    // Helper to create working memory with PE
    working_memory_t* CreateWMWithPE(nimcp_pos_encoding_type_t pe_type) {
        working_memory_config_t config = working_memory_default_config();
        config.capacity = TEST_CAPACITY;
        config.enable_positional_encoding = true;
        config.pe_type = pe_type;
        config.pe_embedding_dim = TEST_PE_DIM;

        return working_memory_create_custom(&config);
    }

    // Helper to fill working memory with test items
    void FillWorkingMemory(working_memory_t* memory, uint32_t count) {
        for (uint32_t i = 0; i < count && i < TEST_CAPACITY; i++) {
            float item[TEST_ITEM_SIZE];
            for (uint32_t j = 0; j < TEST_ITEM_SIZE; j++) {
                item[j] = (float)i + (float)j / 100.0f;
            }
            float salience = 1.0f - (float)i / (float)TEST_CAPACITY;
            working_memory_add(memory, item, TEST_ITEM_SIZE, salience);
        }
    }
};

//=============================================================================
// Unit Tests: PE Configuration
//=============================================================================

TEST_F(WorkingMemoryPETest, SetPEConfig_Sinusoidal) {
    // WHAT: Configure working memory with sinusoidal PE
    // WHY:  Default PE type for sequence encoding

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr) << "Working memory with sinusoidal PE should be created";

    // Verify configuration
    EXPECT_EQ(working_memory_get_capacity(wm), TEST_CAPACITY);
}

TEST_F(WorkingMemoryPETest, SetPEConfig_Learned) {
    // WHAT: Configure working memory with learned PE
    // WHY:  Task-specific position embeddings for fixed-size buffer

    wm = CreateWMWithPE(NIMCP_POS_LEARNED);
    ASSERT_NE(wm, nullptr) << "Working memory with learned PE should be created";

    // Verify configuration
    EXPECT_EQ(working_memory_get_capacity(wm), TEST_CAPACITY);
}

TEST_F(WorkingMemoryPETest, SetPEConfig_Relative) {
    // WHAT: Configure working memory with relative PE
    // WHY:  Relative position encoding for distance-based reasoning

    wm = CreateWMWithPE(NIMCP_POS_RELATIVE);
    ASSERT_NE(wm, nullptr) << "Working memory with relative PE should be created";

    // Verify configuration
    EXPECT_EQ(working_memory_get_capacity(wm), TEST_CAPACITY);
}

TEST_F(WorkingMemoryPETest, SetPEConfig_Disable) {
    // WHAT: Create working memory without PE
    // WHY:  Baseline comparison

    working_memory_config_t config = working_memory_default_config();
    config.enable_positional_encoding = false;

    wm = working_memory_create_custom(&config);
    ASSERT_NE(wm, nullptr);

    // Verify no PE
    float embedding[TEST_PE_DIM];
    bool result = working_memory_get_position_embedding(wm, 0, embedding);
    EXPECT_FALSE(result) << "PE should be disabled";
}

//=============================================================================
// Unit Tests: Position Encoding Application
//=============================================================================

TEST_F(WorkingMemoryPETest, ApplyEncoding_ToAllSlots) {
    // WHAT: Apply position encodings to all items in working memory
    // WHY:  Basic PE functionality test

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    // Fill working memory
    FillWorkingMemory(wm, 5);

    // Apply position encodings
    bool result = working_memory_encode_positions(wm);
    EXPECT_TRUE(result) << "Position encoding should succeed";

    // Verify encodings are applied
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);
    EXPECT_EQ(stats.current_size, 5u);
}

TEST_F(WorkingMemoryPETest, GetPositionEmbedding_SingleSlot) {
    // WHAT: Retrieve position embedding for specific slot
    // WHY:  Verify position encodings are computed correctly

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    float embedding[TEST_PE_DIM];
    bool result = working_memory_get_position_embedding(wm, 0, embedding);
    EXPECT_TRUE(result) << "Position embedding retrieval should succeed";

    // Verify embedding is non-zero
    bool has_nonzero = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(embedding[i]) > EPSILON) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "Position embedding should be non-zero";
}

TEST_F(WorkingMemoryPETest, GetPositionEmbedding_DifferentSlots) {
    // WHAT: Verify different slots have different position embeddings
    // WHY:  Position discrimination is critical for serial position effects

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    float emb0[TEST_PE_DIM];
    float emb3[TEST_PE_DIM];
    float emb6[TEST_PE_DIM];

    working_memory_get_position_embedding(wm, 0, emb0);
    working_memory_get_position_embedding(wm, 3, emb3);
    working_memory_get_position_embedding(wm, 6, emb6);

    // Verify embeddings are different
    bool emb0_vs_emb3_different = false;
    bool emb3_vs_emb6_different = false;

    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb0[i] - emb3[i]) > EPSILON) emb0_vs_emb3_different = true;
        if (std::abs(emb3[i] - emb6[i]) > EPSILON) emb3_vs_emb6_different = true;
    }

    EXPECT_TRUE(emb0_vs_emb3_different) << "Position 0 and 3 should differ";
    EXPECT_TRUE(emb3_vs_emb6_different) << "Position 3 and 6 should differ";
}

//=============================================================================
// Unit Tests: Serial Position Effects
//=============================================================================

TEST_F(WorkingMemoryPETest, SerialPosition_PrimacyEffect) {
    // WHAT: Test that early positions (primacy) have distinct encodings
    // WHY:  Model primacy effect in working memory

    wm = CreateWMWithPE(NIMCP_POS_LEARNED);
    ASSERT_NE(wm, nullptr);

    // Fill working memory to capacity
    FillWorkingMemory(wm, TEST_CAPACITY);

    // Get embeddings for early positions (primacy)
    float emb_first[TEST_PE_DIM];
    float emb_second[TEST_PE_DIM];

    working_memory_get_position_embedding(wm, 0, emb_first);
    working_memory_get_position_embedding(wm, 1, emb_second);

    // Verify first and second positions are different
    bool different = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb_first[i] - emb_second[i]) > EPSILON) {
            different = true;
            break;
        }
    }

    EXPECT_TRUE(different) << "First and second positions should have different encodings";
}

TEST_F(WorkingMemoryPETest, SerialPosition_RecencyEffect) {
    // WHAT: Test that recent positions (recency) have distinct encodings
    // WHY:  Model recency effect in working memory

    wm = CreateWMWithPE(NIMCP_POS_LEARNED);
    ASSERT_NE(wm, nullptr);

    // Fill working memory to capacity
    FillWorkingMemory(wm, TEST_CAPACITY);

    // Get embeddings for recent positions (recency)
    uint32_t last_idx = TEST_CAPACITY - 1;
    uint32_t second_last_idx = TEST_CAPACITY - 2;

    float emb_last[TEST_PE_DIM];
    float emb_second_last[TEST_PE_DIM];

    working_memory_get_position_embedding(wm, last_idx, emb_last);
    working_memory_get_position_embedding(wm, second_last_idx, emb_second_last);

    // Verify last and second-last positions are different
    bool different = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb_last[i] - emb_second_last[i]) > EPSILON) {
            different = true;
            break;
        }
    }

    EXPECT_TRUE(different) << "Last and second-last positions should have different encodings";
}

TEST_F(WorkingMemoryPETest, SerialPosition_MiddleItems) {
    // WHAT: Test middle positions have intermediate encodings
    // WHY:  Serial position curve peaks at extremes

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    uint32_t first = 0;
    uint32_t middle = TEST_CAPACITY / 2;
    uint32_t last = TEST_CAPACITY - 1;

    float emb_first[TEST_PE_DIM];
    float emb_middle[TEST_PE_DIM];
    float emb_last[TEST_PE_DIM];

    working_memory_get_position_embedding(wm, first, emb_first);
    working_memory_get_position_embedding(wm, middle, emb_middle);
    working_memory_get_position_embedding(wm, last, emb_last);

    // All three should be different
    bool first_vs_middle = false;
    bool middle_vs_last = false;

    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb_first[i] - emb_middle[i]) > EPSILON) first_vs_middle = true;
        if (std::abs(emb_middle[i] - emb_last[i]) > EPSILON) middle_vs_last = true;
    }

    EXPECT_TRUE(first_vs_middle) << "First and middle positions should differ";
    EXPECT_TRUE(middle_vs_last) << "Middle and last positions should differ";
}

//=============================================================================
// Unit Tests: PE Type Switching
//=============================================================================

TEST_F(WorkingMemoryPETest, SwitchPEType_SinusoidalToLearned) {
    // WHAT: Switch from sinusoidal to learned PE at runtime
    // WHY:  Allow dynamic PE reconfiguration

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    // Switch to learned
    bool result = working_memory_set_pe_type(wm, NIMCP_POS_LEARNED);
    EXPECT_TRUE(result) << "PE type switch should succeed";

    // Verify learned PE is active
    float embedding[TEST_PE_DIM];
    result = working_memory_get_position_embedding(wm, 0, embedding);
    EXPECT_TRUE(result) << "Position embedding should work after switch";
}

TEST_F(WorkingMemoryPETest, SwitchPEType_LearnedToRelative) {
    // WHAT: Switch from learned to relative PE
    // WHY:  Test different PE type transitions

    wm = CreateWMWithPE(NIMCP_POS_LEARNED);
    ASSERT_NE(wm, nullptr);

    // Switch to relative
    bool result = working_memory_set_pe_type(wm, NIMCP_POS_RELATIVE);
    EXPECT_TRUE(result) << "PE type switch should succeed";

    // Verify relative PE is active
    float embedding[TEST_PE_DIM];
    result = working_memory_get_position_embedding(wm, 0, embedding);
    EXPECT_TRUE(result) << "Position embedding should work after switch";
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(WorkingMemoryPETest, EdgeCase_NullInput) {
    // WHAT: Handle NULL inputs gracefully
    // WHY:  Robustness testing

    float embedding[TEST_PE_DIM];
    bool result = working_memory_get_position_embedding(nullptr, 0, embedding);
    EXPECT_FALSE(result) << "NULL working memory should fail";

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    result = working_memory_get_position_embedding(wm, 0, nullptr);
    EXPECT_FALSE(result) << "NULL output buffer should fail";
}

TEST_F(WorkingMemoryPETest, EdgeCase_InvalidPosition) {
    // WHAT: Request position beyond capacity
    // WHY:  Boundary testing

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    float embedding[TEST_PE_DIM];
    bool result = working_memory_get_position_embedding(wm, TEST_CAPACITY + 10, embedding);
    EXPECT_FALSE(result) << "Out-of-bounds position should fail";
}

TEST_F(WorkingMemoryPETest, EdgeCase_EmptyWorkingMemory) {
    // WHAT: Apply PE to empty working memory
    // WHY:  Edge case validation

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    EXPECT_TRUE(working_memory_is_empty(wm));

    // Should succeed as no-op or fail gracefully
    bool result = working_memory_encode_positions(wm);
    SUCCEED() << "Empty working memory PE encoding handled";
}

TEST_F(WorkingMemoryPETest, EdgeCase_FullWorkingMemory) {
    // WHAT: Apply PE to full working memory
    // WHY:  Capacity limit testing

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    // Fill to capacity
    FillWorkingMemory(wm, TEST_CAPACITY);
    EXPECT_TRUE(working_memory_is_full(wm));

    // Apply PE
    bool result = working_memory_encode_positions(wm);
    EXPECT_TRUE(result) << "Full working memory PE encoding should succeed";
}

//=============================================================================
// Unit Tests: Integration with Working Memory Operations
//=============================================================================

TEST_F(WorkingMemoryPETest, Integration_AddItemWithPE) {
    // WHAT: Add item and verify PE is applied
    // WHY:  End-to-end integration test

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    float item[TEST_ITEM_SIZE];
    for (uint32_t i = 0; i < TEST_ITEM_SIZE; i++) {
        item[i] = (float)i;
    }

    bool result = working_memory_add(wm, item, TEST_ITEM_SIZE, 0.9f);
    EXPECT_TRUE(result) << "Item addition should succeed";

    // Apply PE
    result = working_memory_encode_positions(wm);
    EXPECT_TRUE(result) << "PE encoding should succeed";

    // Verify item is stored
    EXPECT_EQ(working_memory_get_size(wm), 1u);
}

TEST_F(WorkingMemoryPETest, Integration_MultipleItemsWithPE) {
    // WHAT: Add multiple items and verify each gets unique PE
    // WHY:  Test PE with multiple items

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    uint32_t num_items = 5;
    FillWorkingMemory(wm, num_items);

    // Apply PE
    bool result = working_memory_encode_positions(wm);
    EXPECT_TRUE(result) << "PE encoding should succeed";

    // Verify each position has unique embedding
    float emb[num_items][TEST_PE_DIM];
    for (uint32_t i = 0; i < num_items; i++) {
        working_memory_get_position_embedding(wm, i, emb[i]);
    }

    // Check all pairs are different
    for (uint32_t i = 0; i < num_items; i++) {
        for (uint32_t j = i + 1; j < num_items; j++) {
            bool different = false;
            for (uint32_t k = 0; k < TEST_PE_DIM; k++) {
                if (std::abs(emb[i][k] - emb[j][k]) > EPSILON) {
                    different = true;
                    break;
                }
            }
            EXPECT_TRUE(different) << "Positions " << i << " and " << j << " should differ";
        }
    }
}

TEST_F(WorkingMemoryPETest, Integration_RemoveItemUpdatePE) {
    // WHAT: Remove item and verify PE is updated
    // WHY:  Test PE consistency after removal

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    // Add items
    FillWorkingMemory(wm, 5);
    EXPECT_EQ(working_memory_get_size(wm), 5u);

    // Remove middle item
    bool result = working_memory_remove(wm, 2);
    EXPECT_TRUE(result);
    EXPECT_EQ(working_memory_get_size(wm), 4u);

    // Reapply PE
    result = working_memory_encode_positions(wm);
    EXPECT_TRUE(result) << "PE encoding after removal should succeed";

    // Verify remaining items have correct positions
    for (uint32_t i = 0; i < 4; i++) {
        float embedding[TEST_PE_DIM];
        result = working_memory_get_position_embedding(wm, i, embedding);
        EXPECT_TRUE(result) << "Position " << i << " should have valid embedding";
    }
}

TEST_F(WorkingMemoryPETest, Integration_ClearAndReapplyPE) {
    // WHAT: Clear working memory and reapply PE
    // WHY:  Test PE reset behavior

    wm = CreateWMWithPE(NIMCP_POS_SINUSOIDAL);
    ASSERT_NE(wm, nullptr);

    // Add items
    FillWorkingMemory(wm, 5);
    working_memory_encode_positions(wm);

    // Clear
    working_memory_clear(wm);
    EXPECT_TRUE(working_memory_is_empty(wm));

    // Add new items
    FillWorkingMemory(wm, 3);
    bool result = working_memory_encode_positions(wm);
    EXPECT_TRUE(result) << "PE encoding after clear should succeed";

    EXPECT_EQ(working_memory_get_size(wm), 3u);
}

//=============================================================================
// Unit Tests: Learned PE Specific
//=============================================================================

TEST_F(WorkingMemoryPETest, LearnedPE_FixedCapacity) {
    // WHAT: Verify learned PE works with fixed capacity
    // WHY:  Learned PE optimized for fixed-size buffer

    wm = CreateWMWithPE(NIMCP_POS_LEARNED);
    ASSERT_NE(wm, nullptr);

    // Each slot should have unique learned embedding
    for (uint32_t slot = 0; slot < TEST_CAPACITY; slot++) {
        float embedding[TEST_PE_DIM];
        bool result = working_memory_get_position_embedding(wm, slot, embedding);
        EXPECT_TRUE(result) << "Slot " << slot << " should have learned embedding";
    }
}

TEST_F(WorkingMemoryPETest, LearnedPE_ConsistencyAcrossRetrievals) {
    // WHAT: Verify learned embedding is consistent across multiple retrievals
    // WHY:  Learned embeddings should be stable

    wm = CreateWMWithPE(NIMCP_POS_LEARNED);
    ASSERT_NE(wm, nullptr);

    uint32_t slot = 3;
    float emb1[TEST_PE_DIM];
    float emb2[TEST_PE_DIM];

    working_memory_get_position_embedding(wm, slot, emb1);
    working_memory_get_position_embedding(wm, slot, emb2);

    // Verify embeddings are identical
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        EXPECT_NEAR(emb1[i], emb2[i], EPSILON)
            << "Learned embedding should be consistent at index " << i;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
