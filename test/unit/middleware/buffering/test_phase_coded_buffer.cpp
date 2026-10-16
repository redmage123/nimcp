//=============================================================================
// test_phase_coded_buffer.cpp - Test Phase-Coded Working Memory Buffer
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "middleware/buffering/nimcp_phase_coded_buffer.h"
    #include "utils/math/nimcp_complex_math.h"
    #include "utils/memory/nimcp_memory.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class PhaseCodedBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        complex_math_init(nullptr);
    }

    void TearDown() override {
        complex_math_cleanup();
    }
};

// ============================================================================
// BASIC FUNCTIONALITY TESTS
// ============================================================================

TEST_F(PhaseCodedBufferTest, CreateDestroy) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);
    phase_buffer_destroy(buffer);
}

TEST_F(PhaseCodedBufferTest, DefaultConfig) {
    phase_buffer_config_t config = phase_buffer_default_config();
    EXPECT_EQ(config.capacity, PHASE_BUFFER_DEFAULT_CAPACITY);
    EXPECT_TRUE(config.auto_phase_increment);
    EXPECT_GT(config.phase_increment, 0.0f);
    EXPECT_EQ(config.theta_frequency_hz, 8.0f);
}

TEST_F(PhaseCodedBufferTest, StoreAndRetrieve) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store items
    EXPECT_TRUE(phase_buffer_store(buffer, 1.0f, 1.0f, 0.0));
    EXPECT_TRUE(phase_buffer_store(buffer, 2.0f, 1.0f, 10.0));
    EXPECT_TRUE(phase_buffer_store(buffer, 3.0f, 1.0f, 20.0));

    // Check stats
    uint32_t count, capacity;
    EXPECT_TRUE(phase_buffer_get_stats(buffer, &count, &capacity, nullptr));
    EXPECT_EQ(count, 3);

    // Retrieve in order
    phase_coded_item_t items[10];
    uint32_t num_retrieved;
    EXPECT_TRUE(phase_buffer_retrieve_ordered(buffer, items, 10, &num_retrieved));
    EXPECT_EQ(num_retrieved, 3);

    // Items should be ordered by phase
    EXPECT_LE(items[0].phase, items[1].phase);
    EXPECT_LE(items[1].phase, items[2].phase);

    phase_buffer_destroy(buffer);
}

TEST_F(PhaseCodedBufferTest, StoreWithExplicitPhase) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store with explicit phases
    EXPECT_TRUE(phase_buffer_store_with_phase(buffer, 10.0f, 0.0f, 1.0f, 0.0));
    EXPECT_TRUE(phase_buffer_store_with_phase(buffer, 20.0f, M_PI/2, 1.0f, 10.0));
    EXPECT_TRUE(phase_buffer_store_with_phase(buffer, 30.0f, M_PI, 1.0f, 20.0));

    // Retrieve ordered
    phase_coded_item_t items[10];
    uint32_t num_retrieved;
    EXPECT_TRUE(phase_buffer_retrieve_ordered(buffer, items, 10, &num_retrieved));
    EXPECT_EQ(num_retrieved, 3);

    // Should be in phase order
    EXPECT_FLOAT_EQ(items[0].data, 10.0f);  // Phase 0
    EXPECT_FLOAT_EQ(items[1].data, 20.0f);  // Phase π/2
    EXPECT_FLOAT_EQ(items[2].data, 30.0f);  // Phase π

    phase_buffer_destroy(buffer);
}

TEST_F(PhaseCodedBufferTest, AutoPhaseIncrement) {
    phase_buffer_config_t config = phase_buffer_default_config();
    config.auto_phase_increment = true;
    config.phase_increment = M_PI / 4.0f;  // 45 degrees

    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store 8 items (should cycle through 360 degrees)
    for (int i = 0; i < 8; i++) {
        EXPECT_TRUE(phase_buffer_store(buffer, (float)i, 1.0f, (double)i * 10.0));
    }

    // Retrieve
    phase_coded_item_t items[10];
    uint32_t num_retrieved;
    EXPECT_TRUE(phase_buffer_retrieve_ordered(buffer, items, 10, &num_retrieved));
    EXPECT_EQ(num_retrieved, 8);

    // Check phase spacing
    for (uint32_t i = 1; i < num_retrieved; i++) {
        float phase_diff = items[i].phase - items[i-1].phase;
        EXPECT_NEAR(phase_diff, M_PI / 4.0f, 0.1f);
    }

    phase_buffer_destroy(buffer);
}

// ============================================================================
// COHERENCE TESTS
// ============================================================================

TEST_F(PhaseCodedBufferTest, Coherence_IdenticalPhases) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store items with same phase - perfect coherence
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(phase_buffer_store_with_phase(buffer, (float)i, 0.0f, 1.0f, (double)i));
    }

    float coherence = phase_buffer_coherence(buffer);
    EXPECT_NEAR(coherence, 1.0f, 0.1f);  // Should be close to 1.0

    phase_buffer_destroy(buffer);
}

TEST_F(PhaseCodedBufferTest, Coherence_RandomPhases) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store items with random phases - low coherence
    for (int i = 0; i < 20; i++) {
        float random_phase = ((float)rand() / RAND_MAX) * 2.0f * M_PI - M_PI;
        EXPECT_TRUE(phase_buffer_store_with_phase(buffer, (float)i, random_phase, 1.0f, (double)i));
    }

    float coherence = phase_buffer_coherence(buffer);
    EXPECT_LT(coherence, 0.5f);  // Should be low

    phase_buffer_destroy(buffer);
}

TEST_F(PhaseCodedBufferTest, MeanPhase) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store items around 0 phase
    EXPECT_TRUE(phase_buffer_store_with_phase(buffer, 1.0f, -0.1f, 1.0f, 0.0));
    EXPECT_TRUE(phase_buffer_store_with_phase(buffer, 2.0f, 0.0f, 1.0f, 10.0));
    EXPECT_TRUE(phase_buffer_store_with_phase(buffer, 3.0f, 0.1f, 1.0f, 20.0));

    float mean_phase = phase_buffer_mean_phase(buffer);
    EXPECT_NEAR(mean_phase, 0.0f, 0.2f);  // Should be close to 0

    phase_buffer_destroy(buffer);
}

// ============================================================================
// PATTERN MATCHING TESTS
// ============================================================================

TEST_F(PhaseCodedBufferTest, PatternMatch_ExactMatch) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store items with known phases
    EXPECT_TRUE(phase_buffer_store_with_phase(buffer, 1.0f, 0.0f, 1.0f, 0.0));
    EXPECT_TRUE(phase_buffer_store_with_phase(buffer, 2.0f, M_PI/2, 1.0f, 10.0));
    EXPECT_TRUE(phase_buffer_store_with_phase(buffer, 3.0f, M_PI, 1.0f, 20.0));

    // Pattern to match: [0, π/2]
    // Mean phase is π/4, items at 0 and π/2 are both π/4 away → coherence ≈ 0.75
    float pattern_phases[] = {0.0f, M_PI/2};
    phase_pattern_match_t result;

    EXPECT_TRUE(phase_buffer_pattern_match(buffer, pattern_phases, 2, 0.7f, &result));

    // Should match items 1 and 2 (both within π/4 of mean phase π/4)
    EXPECT_GE(result.count, 2);
    EXPECT_GT(result.mean_coherence, 0.7f);

    phase_pattern_match_free(&result);
    phase_buffer_destroy(buffer);
}

TEST_F(PhaseCodedBufferTest, PatternMatch_NoMatch) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store items at 0 phase
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(phase_buffer_store_with_phase(buffer, (float)i, 0.0f, 1.0f, (double)i));
    }

    // Pattern at π (opposite phase) - should not match with high threshold
    float pattern_phases[] = {M_PI};
    phase_pattern_match_t result;

    EXPECT_TRUE(phase_buffer_pattern_match(buffer, pattern_phases, 1, 0.9f, &result));

    // Should find few or no matches with high threshold
    EXPECT_LE(result.count, 1);

    phase_pattern_match_free(&result);
    phase_buffer_destroy(buffer);
}

// ============================================================================
// CAPACITY AND EDGE CASES
// ============================================================================

TEST_F(PhaseCodedBufferTest, BufferFull) {
    phase_buffer_config_t config = phase_buffer_default_config();
    config.capacity = 10;

    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Fill buffer
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(phase_buffer_store(buffer, (float)i, 1.0f, (double)i));
    }

    // Next store should fail
    EXPECT_FALSE(phase_buffer_store(buffer, 100.0f, 1.0f, 100.0));

    uint32_t count;
    phase_buffer_get_stats(buffer, &count, nullptr, nullptr);
    EXPECT_EQ(count, 10);

    phase_buffer_destroy(buffer);
}

TEST_F(PhaseCodedBufferTest, ClearBuffer) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store items
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(phase_buffer_store(buffer, (float)i, 1.0f, (double)i));
    }

    uint32_t count;
    phase_buffer_get_stats(buffer, &count, nullptr, nullptr);
    EXPECT_EQ(count, 5);

    // Clear
    phase_buffer_clear(buffer);

    phase_buffer_get_stats(buffer, &count, nullptr, nullptr);
    EXPECT_EQ(count, 0);

    phase_buffer_destroy(buffer);
}

TEST_F(PhaseCodedBufferTest, EmptyBuffer) {
    phase_buffer_config_t config = phase_buffer_default_config();
    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Operations on empty buffer should handle gracefully
    phase_coded_item_t items[10];
    uint32_t num_retrieved;
    EXPECT_TRUE(phase_buffer_retrieve_ordered(buffer, items, 10, &num_retrieved));
    EXPECT_EQ(num_retrieved, 0);

    float coherence = phase_buffer_coherence(buffer);
    EXPECT_EQ(coherence, 0.0f);

    float mean_phase = phase_buffer_mean_phase(buffer);
    EXPECT_EQ(mean_phase, 0.0f);

    phase_buffer_destroy(buffer);
}

// ============================================================================
// NEUROSCIENCE-INSPIRED TESTS
// ============================================================================

TEST_F(PhaseCodedBufferTest, WorkingMemory_SequenceRecall) {
    // Simulate working memory sequence encoding (like remembering a phone number)
    phase_buffer_config_t config = phase_buffer_default_config();
    config.auto_phase_increment = true;
    config.phase_increment = (2.0f * M_PI) / 7.0f;  // 7 items per cycle

    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Store sequence: 5-5-5-1-2-1-2 (phone number)
    float sequence[] = {5, 5, 5, 1, 2, 1, 2};
    for (int i = 0; i < 7; i++) {
        EXPECT_TRUE(phase_buffer_store(buffer, sequence[i], 1.0f, (double)i * 100.0));
    }

    // Retrieve in phase order (temporal order)
    phase_coded_item_t items[10];
    uint32_t num_retrieved;
    EXPECT_TRUE(phase_buffer_retrieve_ordered(buffer, items, 10, &num_retrieved));
    EXPECT_EQ(num_retrieved, 7);

    // Should retrieve in same order as stored
    for (int i = 0; i < 7; i++) {
        EXPECT_FLOAT_EQ(items[i].data, sequence[i]);
    }

    phase_buffer_destroy(buffer);
}

TEST_F(PhaseCodedBufferTest, ThetaPhasePrecession) {
    // Simulate hippocampal theta phase precession during spatial navigation
    phase_buffer_config_t config = phase_buffer_default_config();
    config.theta_frequency_hz = 8.0f;

    phase_coded_buffer_t* buffer = phase_buffer_create(&config);
    ASSERT_NE(buffer, nullptr);

    // Simulate positions along a track, encoded at different theta phases
    float positions[] = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f};
    float phases[] = {0.0f, M_PI/4, M_PI/2, 3*M_PI/4, M_PI};

    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(phase_buffer_store_with_phase(buffer, positions[i], phases[i],
                                                   1.0f, (double)i * 125.0));
    }

    // Retrieve in phase order = spatial order
    phase_coded_item_t items[10];
    uint32_t num_retrieved;
    EXPECT_TRUE(phase_buffer_retrieve_ordered(buffer, items, 10, &num_retrieved));
    EXPECT_EQ(num_retrieved, 5);

    // Positions should be in increasing order
    for (uint32_t i = 1; i < num_retrieved; i++) {
        EXPECT_GE(items[i].data, items[i-1].data);
    }

    phase_buffer_destroy(buffer);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
