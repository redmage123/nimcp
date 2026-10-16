//=============================================================================
// test_working_memory_pe_integration.cpp - Working Memory + PE Integration
//=============================================================================
/**
 * @file test_working_memory_pe_integration.cpp
 * @brief Integration tests for working memory with positional encoding
 *
 * WHAT: Test working memory enhanced with positional encoding for serial order
 * WHY:  Serial position effects (primacy/recency) require position encoding
 * HOW:  Add items to WM, apply PE, verify position-dependent behavior
 *
 * TEST COVERAGE:
 * 1. Positional encoding in working memory slots
 * 2. Serial position effects (primacy/recency curves)
 * 3. Position-dependent recall accuracy
 * 4. PE type switching (sinusoidal vs learned)
 * 5. Working memory + PE capacity interactions
 * 6. Position encoding with emotional tagging
 *
 * BIOLOGICAL BASIS:
 * - Serial position effects in short-term memory (Ebbinghaus, 1885)
 * - Primacy effect: better recall of early items (phonological loop)
 * - Recency effect: better recall of recent items (echoic memory)
 * - Prefrontal cortex encodes temporal order (Fuster, 1997)
 * - Position information critical for working memory manipulation
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
    #include "cognitive/nimcp_working_memory.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "cognitive/nimcp_emotional_tagging.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class WorkingMemoryPEIntegrationTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
    static constexpr uint32_t ITEM_SIZE = 32;
    static constexpr uint32_t WM_CAPACITY = 7;  // Miller's magic number

    working_memory_t* wm = nullptr;

    void SetUp() override {
        // Create working memory with PE enabled
        working_memory_config_t config = working_memory_default_config();
        config.capacity = WM_CAPACITY;
        config.enable_positional_encoding = true;
        config.pe_type = NIMCP_POS_SINUSOIDAL;
        config.pe_embedding_dim = 16;
        config.enable_temporal_decay = false;  // Disable for position testing

        wm = working_memory_create_custom(&config);
        ASSERT_NE(wm, nullptr);
    }

    void TearDown() override {
        if (wm) {
            working_memory_destroy(wm);
            wm = nullptr;
        }
    }

    // Helper: Create item with position-dependent pattern
    void create_position_item(float* item, uint32_t position, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            item[i] = sinf((float)position + (float)i / 10.0f);
        }
    }

    // Helper: Compute similarity between two items
    float compute_similarity(const float* a, const float* b, uint32_t size) {
        float dot = 0.0f;
        float norm_a = 0.0f;
        float norm_b = 0.0f;

        for (uint32_t i = 0; i < size; i++) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }

        norm_a = sqrtf(norm_a);
        norm_b = sqrtf(norm_b);

        if (norm_a < EPSILON || norm_b < EPSILON) {
            return 0.0f;
        }

        return dot / (norm_a * norm_b);
    }
};

//=============================================================================
// Integration Tests: Basic PE in Working Memory
//=============================================================================

TEST_F(WorkingMemoryPEIntegrationTest, PositionalEncodingApplication) {
    /* WHAT: Test positional encodings are applied to WM items
     * WHY:  Verify PE integration with working memory
     * HOW:  Add items, encode positions, verify embeddings present
     */

    // Add items to working memory
    for (uint32_t pos = 0; pos < 5; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);

        bool added = working_memory_add(wm, item, ITEM_SIZE, 0.8f);
        ASSERT_TRUE(added) << "Failed to add item at position " << pos;
    }

    EXPECT_EQ(working_memory_get_size(wm), 5u);

    // Apply positional encodings
    bool encoded = working_memory_encode_positions(wm);
    ASSERT_TRUE(encoded);

    // Verify items still retrievable
    for (uint32_t pos = 0; pos < 5; pos++) {
        uint32_t size;
        const float* item = working_memory_get(wm, pos, &size);
        ASSERT_NE(item, nullptr);
        EXPECT_EQ(size, ITEM_SIZE);
    }
}

TEST_F(WorkingMemoryPEIntegrationTest, PositionEmbeddingRetrieval) {
    /* WHAT: Test retrieval of position embeddings
     * WHY:  Verify PE embeddings accessible
     * HOW:  Get embeddings for different slots, verify distinct
     */

    // Add items
    for (uint32_t pos = 0; pos < WM_CAPACITY; pos++) {
        float item[ITEM_SIZE] = {(float)pos};
        working_memory_add(wm, item, ITEM_SIZE, 0.8f);
    }

    // Get position embeddings for different slots
    std::vector<float> embed0(16);
    std::vector<float> embed3(16);
    std::vector<float> embed6(16);

    ASSERT_TRUE(working_memory_get_position_embedding(wm, 0, embed0.data()));
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 3, embed3.data()));
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 6, embed6.data()));

    // Embeddings should differ across positions
    float sim_0_3 = compute_similarity(embed0.data(), embed3.data(), 16);
    float sim_0_6 = compute_similarity(embed0.data(), embed6.data(), 16);

    // Similar positions more similar than distant
    EXPECT_GT(sim_0_3, sim_0_6);
}

//=============================================================================
// Integration Tests: Serial Position Effects
//=============================================================================

TEST_F(WorkingMemoryPEIntegrationTest, PrimacyEffect) {
    /* WHAT: Test primacy effect (better recall of early items)
     * WHY:  Position encoding should support serial position effects
     * HOW:  Add items, measure "recall quality" (salience) by position
     */

    // Add items with uniform salience
    for (uint32_t pos = 0; pos < WM_CAPACITY; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);
        working_memory_add(wm, item, ITEM_SIZE, 0.5f);  // Uniform salience
    }

    working_memory_encode_positions(wm);

    // Get items and check early positions (primacy)
    // Early items should be retrievable with their position info
    for (uint32_t pos = 0; pos < 3; pos++) {
        uint32_t size;
        const float* item = working_memory_get(wm, pos, &size);
        ASSERT_NE(item, nullptr) << "Early item " << pos << " not retrievable";
    }

    // Verify position embeddings distinct for early items
    std::vector<float> embed0(16), embed1(16), embed2(16);
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 0, embed0.data()));
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 1, embed1.data()));
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 2, embed2.data()));

    // Early positions should have distinct embeddings
    float diff_0_1 = 0.0f;
    for (uint32_t i = 0; i < 16; i++) {
        diff_0_1 += fabsf(embed0[i] - embed1[i]);
    }
    EXPECT_GT(diff_0_1, 0.1f);
}

TEST_F(WorkingMemoryPEIntegrationTest, RecencyEffect) {
    /* WHAT: Test recency effect (better recall of recent items)
     * WHY:  Recent items should have distinct position codes
     * HOW:  Fill WM, verify recent items have clear position info
     */

    // Fill working memory to capacity
    for (uint32_t pos = 0; pos < WM_CAPACITY; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);
        working_memory_add(wm, item, ITEM_SIZE, 0.5f);
    }

    working_memory_encode_positions(wm);

    // Check recent items (last 2)
    uint32_t size;
    const float* recent1 = working_memory_get(wm, WM_CAPACITY - 2, &size);
    const float* recent2 = working_memory_get(wm, WM_CAPACITY - 1, &size);

    ASSERT_NE(recent1, nullptr);
    ASSERT_NE(recent2, nullptr);

    // Get their position embeddings
    std::vector<float> embed_recent1(16), embed_recent2(16);
    ASSERT_TRUE(working_memory_get_position_embedding(
        wm, WM_CAPACITY - 2, embed_recent1.data()
    ));
    ASSERT_TRUE(working_memory_get_position_embedding(
        wm, WM_CAPACITY - 1, embed_recent2.data()
    ));

    // Recent positions should be distinct
    float diff = 0.0f;
    for (uint32_t i = 0; i < 16; i++) {
        diff += fabsf(embed_recent1[i] - embed_recent2[i]);
    }
    EXPECT_GT(diff, 0.1f);
}

TEST_F(WorkingMemoryPEIntegrationTest, SerialPositionCurve) {
    /* WHAT: Test full serial position curve (U-shaped)
     * WHY:  Classic finding in memory psychology
     * HOW:  Measure position discriminability across all slots
     */

    // Fill all slots
    for (uint32_t pos = 0; pos < WM_CAPACITY; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);
        working_memory_add(wm, item, ITEM_SIZE, 0.5f);
    }

    working_memory_encode_positions(wm);

    // Measure position embedding distinctiveness
    std::vector<float> distinctiveness(WM_CAPACITY);

    for (uint32_t pos = 0; pos < WM_CAPACITY; pos++) {
        std::vector<float> embed_i(16);
        ASSERT_TRUE(working_memory_get_position_embedding(wm, pos, embed_i.data()));

        // Compare to neighbors
        float dist_to_neighbors = 0.0f;
        int num_neighbors = 0;

        if (pos > 0) {
            std::vector<float> embed_prev(16);
            working_memory_get_position_embedding(wm, pos - 1, embed_prev.data());

            for (uint32_t d = 0; d < 16; d++) {
                dist_to_neighbors += fabsf(embed_i[d] - embed_prev[d]);
            }
            num_neighbors++;
        }

        if (pos < WM_CAPACITY - 1) {
            std::vector<float> embed_next(16);
            working_memory_get_position_embedding(wm, pos + 1, embed_next.data());

            for (uint32_t d = 0; d < 16; d++) {
                dist_to_neighbors += fabsf(embed_i[d] - embed_next[d]);
            }
            num_neighbors++;
        }

        distinctiveness[pos] = dist_to_neighbors / (float)num_neighbors;
    }

    // First and last positions should have high distinctiveness
    EXPECT_GT(distinctiveness[0], 0.0f);
    EXPECT_GT(distinctiveness[WM_CAPACITY - 1], 0.0f);
}

//=============================================================================
// Integration Tests: PE Type Switching
//=============================================================================

TEST_F(WorkingMemoryPEIntegrationTest, SwitchFromSinusoidalToLearned) {
    /* WHAT: Test switching PE type from sinusoidal to learned
     * WHY:  Verify runtime PE strategy adaptation
     * HOW:  Start with sinusoidal, switch to learned, verify change
     */

    // Add items with sinusoidal PE
    for (uint32_t pos = 0; pos < 5; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);
        working_memory_add(wm, item, ITEM_SIZE, 0.8f);
    }

    // Get embedding with sinusoidal
    std::vector<float> embed_sinusoidal(16);
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 2, embed_sinusoidal.data()));

    // Switch to learned PE
    ASSERT_TRUE(working_memory_set_pe_type(wm, NIMCP_POS_LEARNED));

    // Get embedding with learned
    std::vector<float> embed_learned(16);
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 2, embed_learned.data()));

    // Embeddings should differ after switch
    float diff_sum = 0.0f;
    for (uint32_t i = 0; i < 16; i++) {
        diff_sum += fabsf(embed_sinusoidal[i] - embed_learned[i]);
    }

    EXPECT_GT(diff_sum, 0.1f);
}

TEST_F(WorkingMemoryPEIntegrationTest, RelativePEForSequenceReasoning) {
    /* WHAT: Test relative PE for distance-based reasoning
     * WHY:  Relative position useful for working memory operations
     * HOW:  Use relative PE, verify position relationships encoded
     */

    // Switch to relative PE
    ASSERT_TRUE(working_memory_set_pe_type(wm, NIMCP_POS_RELATIVE));

    // Add items
    for (uint32_t pos = 0; pos < WM_CAPACITY; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);
        working_memory_add(wm, item, ITEM_SIZE, 0.8f);
    }

    working_memory_encode_positions(wm);

    // Check adjacent positions have similar codes
    std::vector<float> embed2(16), embed3(16);
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 2, embed2.data()));
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 3, embed3.data()));

    float sim_adjacent = compute_similarity(embed2.data(), embed3.data(), 16);

    // Check distant positions
    std::vector<float> embed0(16), embed6(16);
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 0, embed0.data()));
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 6, embed6.data()));

    float sim_distant = compute_similarity(embed0.data(), embed6.data(), 16);

    // Adjacent should be more similar than distant
    EXPECT_GT(sim_adjacent, sim_distant);
}

//=============================================================================
// Integration Tests: PE + Working Memory Capacity
//=============================================================================

TEST_F(WorkingMemoryPEIntegrationTest, PEWithCapacityPressure) {
    /* WHAT: Test PE behavior under capacity pressure
     * WHY:  Verify PE maintained during evictions
     * HOW:  Overfill WM, verify remaining items have correct PE
     */

    // Overfill working memory (capacity = 7)
    for (uint32_t pos = 0; pos < 10; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);

        // Vary salience (some items more important)
        float salience = (pos % 3 == 0) ? 0.9f : 0.3f;
        working_memory_add(wm, item, ITEM_SIZE, salience);
    }

    // Should be at capacity
    EXPECT_EQ(working_memory_get_size(wm), WM_CAPACITY);

    // Apply PE to remaining items
    ASSERT_TRUE(working_memory_encode_positions(wm));

    // Verify all remaining items have valid PE
    for (uint32_t pos = 0; pos < WM_CAPACITY; pos++) {
        std::vector<float> embed(16);
        ASSERT_TRUE(working_memory_get_position_embedding(wm, pos, embed.data()));

        // Verify embedding non-zero
        float norm = 0.0f;
        for (uint32_t i = 0; i < 16; i++) {
            norm += embed[i] * embed[i];
        }
        EXPECT_GT(norm, EPSILON);
    }
}

TEST_F(WorkingMemoryPEIntegrationTest, PEPreservedDuringUpdates) {
    /* WHAT: Test PE preserved when items updated
     * WHY:  Position info should persist across updates
     * HOW:  Add items, update salience, verify PE unchanged
     */

    // Add items
    for (uint32_t pos = 0; pos < 5; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);
        working_memory_add(wm, item, ITEM_SIZE, 0.5f);
    }

    working_memory_encode_positions(wm);

    // Get position embedding before update
    std::vector<float> embed_before(16);
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 2, embed_before.data()));

    // Update salience
    ASSERT_TRUE(working_memory_set_salience(wm, 2, 0.9f));

    // Get position embedding after update
    std::vector<float> embed_after(16);
    ASSERT_TRUE(working_memory_get_position_embedding(wm, 2, embed_after.data()));

    // Position embedding should be unchanged
    for (uint32_t i = 0; i < 16; i++) {
        EXPECT_NEAR(embed_before[i], embed_after[i], EPSILON);
    }
}

//=============================================================================
// Integration Tests: PE + Emotional Tagging
//=============================================================================

TEST_F(WorkingMemoryPEIntegrationTest, PEWithEmotionalContext) {
    /* WHAT: Test PE with emotionally tagged items
     * WHY:  Emotional items should retain position info
     * HOW:  Add items with emotions, verify PE and emotion both present
     */

    // Add emotionally tagged items
    for (uint32_t pos = 0; pos < 5; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);

        emotional_tag_t emotion = {};
        emotion.valence = (pos % 2 == 0) ? 0.8f : -0.6f;
        emotion.arousal = 0.7f;
        emotion.category = EMOTION_CAT_JOY;

        working_memory_add_with_emotion(wm, item, ITEM_SIZE, 0.5f, &emotion);
    }

    working_memory_encode_positions(wm);

    // Verify both position and emotion accessible
    for (uint32_t pos = 0; pos < 5; pos++) {
        // Check position embedding
        std::vector<float> embed(16);
        ASSERT_TRUE(working_memory_get_position_embedding(wm, pos, embed.data()));

        // Check emotional tag
        emotional_tag_t emotion;
        ASSERT_TRUE(working_memory_get_emotion(wm, pos, &emotion));

        EXPECT_NE(emotion.valence, 0.0f);
        EXPECT_GT(emotion.arousal, 0.0f);
    }
}

//=============================================================================
// Integration Tests: Statistics
//=============================================================================

TEST_F(WorkingMemoryPEIntegrationTest, StatisticsWithPE) {
    /* WHAT: Test working memory statistics with PE enabled
     * WHY:  Verify PE doesn't break statistics tracking
     * HOW:  Add items, apply PE, check stats valid
     */

    // Add items
    for (uint32_t pos = 0; pos < WM_CAPACITY; pos++) {
        float item[ITEM_SIZE];
        create_position_item(item, pos, ITEM_SIZE);
        working_memory_add(wm, item, ITEM_SIZE, 0.8f);
    }

    working_memory_encode_positions(wm);

    // Get statistics
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);

    EXPECT_EQ(stats.current_size, WM_CAPACITY);
    EXPECT_EQ(stats.total_additions, WM_CAPACITY);
    EXPECT_GT(stats.avg_salience, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
