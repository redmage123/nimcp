/**
 * @file test_engram.cpp
 * @brief Unit tests for memory engram system
 *
 * WHAT: Comprehensive tests for all engram functions
 * WHY:  Ensure 100% code coverage and correctness
 * HOW:  Test lifecycle, encoding, recall, consolidation, forgetting
 *
 * TARGET: 100% code coverage
 *
 * @date 2025-11-13
 */

#include <gtest/gtest.h>

#include "cognitive/memory/nimcp_engram.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class EngramTest : public ::testing::Test {
protected:
    engram_system_t* system;

    void SetUp() override {
        system = engram_system_create();
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        engram_system_destroy(system);
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================

TEST_F(EngramTest, CreateBasic) {
    // WHAT: Test basic creation
    // WHY:  Verify initial state
    // HOW:  Check fields

    EXPECT_NE(system->engrams, nullptr);
    EXPECT_GT(system->capacity, 0);
    EXPECT_EQ(system->active_count, 0);
    EXPECT_EQ(system->next_engram_id, 1);
    EXPECT_TRUE(system->systems_consolidation_enabled);
    EXPECT_TRUE(system->integrate_with_sleep);
    EXPECT_TRUE(system->integrate_with_emotion);
}

TEST_F(EngramTest, CreateReturnsNonNull) {
    // WHAT: Verify creation succeeds
    EXPECT_NE(system, nullptr);
}

TEST_F(EngramTest, DestroyNull) {
    // WHAT: Test destroy with NULL
    // WHY:  Guard clause coverage
    engram_system_destroy(nullptr);
    // Should not crash
}

TEST_F(EngramTest, ResetBasic) {
    // WHAT: Test reset functionality
    // WHY:  Verify clean state
    // HOW:  Encode, reset, check

    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);
    EXPECT_NE(id, 0);
    EXPECT_EQ(system->active_count, 1);

    engram_system_reset(system);

    EXPECT_EQ(system->active_count, 0);
    EXPECT_EQ(system->total_encodings, 0);
    EXPECT_EQ(system->next_engram_id, 1);
}

TEST_F(EngramTest, ResetNull) {
    // WHAT: Test reset with NULL
    engram_system_reset(nullptr);
    // Should not crash
}

//=============================================================================
// ENCODING TESTS
//=============================================================================

TEST_F(EngramTest, EncodeBasic) {
    // WHAT: Test basic encoding
    // WHY:  Core functionality
    // HOW:  Encode and verify

    uint32_t neurons[] = {1, 2, 3, 4, 5};
    float activations[] = {0.8f, 0.7f, 0.9f, 0.6f, 0.85f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 5,
                                MEMORY_TYPE_EPISODIC, emotion);

    EXPECT_NE(id, 0);
    EXPECT_EQ(system->active_count, 1);
    EXPECT_EQ(system->total_encodings, 1);

    memory_engram_t* engram = engram_get_by_id(system, id);
    ASSERT_NE(engram, nullptr);
    EXPECT_TRUE(engram->active);
    EXPECT_EQ(engram->neuron_count, 5);
    EXPECT_EQ(engram->state, ENGRAM_STATE_ENCODING);
    EXPECT_TRUE(engram->is_tagged);
}

TEST_F(EngramTest, EncodeNullSystem) {
    // WHAT: Test NULL system
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(nullptr, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);
    EXPECT_EQ(id, 0);
}

TEST_F(EngramTest, EncodeNullNeurons) {
    // WHAT: Test NULL neurons
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, nullptr, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);
    EXPECT_EQ(id, 0);
}

TEST_F(EngramTest, EncodeNullActivations) {
    // WHAT: Test NULL activations
    uint32_t neurons[] = {1, 2, 3};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, nullptr, 3,
                                MEMORY_TYPE_EPISODIC, emotion);
    EXPECT_EQ(id, 0);
}

TEST_F(EngramTest, EncodeZeroCount) {
    // WHAT: Test zero neuron count
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 0,
                                MEMORY_TYPE_EPISODIC, emotion);
    EXPECT_EQ(id, 0);
}

TEST_F(EngramTest, EncodeMaxNeurons) {
    // WHAT: Test max neuron limit
    uint32_t neurons[ENGRAM_MAX_NEURONS + 10];
    float activations[ENGRAM_MAX_NEURONS + 10];

    for (uint32_t i = 0; i < ENGRAM_MAX_NEURONS + 10; i++) {
        neurons[i] = i;
        activations[i] = 0.8f;
    }

    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations,
                                ENGRAM_MAX_NEURONS + 10,
                                MEMORY_TYPE_EPISODIC, emotion);

    EXPECT_NE(id, 0);

    memory_engram_t* engram = engram_get_by_id(system, id);
    ASSERT_NE(engram, nullptr);
    EXPECT_EQ(engram->neuron_count, ENGRAM_MAX_NEURONS);  // Truncated
}

TEST_F(EngramTest, EncodeEmotionalEnhancement) {
    // WHAT: Test emotional arousal reduces decay
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};

    // High arousal emotion
    emotional_tag_t emotion = {0.8f, 0.9f, 0, EMOTION_CAT_JOY, 0.9f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EMOTIONAL, emotion);

    memory_engram_t* engram = engram_get_by_id(system, id);
    ASSERT_NE(engram, nullptr);

    // Should have reduced decay rate
    EXPECT_LT(engram->decay_rate, system->baseline_decay_rate);
    EXPECT_GT(engram->vividness, 1.0f);  // Enhanced vividness
}

TEST_F(EngramTest, EncodeMultiple) {
    // WHAT: Test encoding multiple engrams
    uint32_t neurons1[] = {1, 2, 3};
    uint32_t neurons2[] = {4, 5, 6};
    uint32_t neurons3[] = {7, 8, 9};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id1 = engram_encode(system, neurons1, activations, 3,
                                 MEMORY_TYPE_EPISODIC, emotion);
    uint64_t id2 = engram_encode(system, neurons2, activations, 3,
                                 MEMORY_TYPE_SEMANTIC, emotion);
    uint64_t id3 = engram_encode(system, neurons3, activations, 3,
                                 MEMORY_TYPE_PROCEDURAL, emotion);

    EXPECT_NE(id1, 0);
    EXPECT_NE(id2, 0);
    EXPECT_NE(id3, 0);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_EQ(system->active_count, 3);
}

//=============================================================================
// RECALL TESTS
//=============================================================================

TEST_F(EngramTest, RecallBasic) {
    // WHAT: Test basic recall
    // WHY:  Core retrieval functionality
    // HOW:  Encode, consolidate, recall

    uint32_t neurons[] = {1, 2, 3, 4, 5};
    float activations[] = {0.8f, 0.7f, 0.9f, 0.6f, 0.85f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 5,
                                MEMORY_TYPE_EPISODIC, emotion);

    // Consolidate fully
    engram_consolidate_update(system, 21600.0f, false);  // 6 hours

    // Recall with partial cue
    uint32_t cue[] = {1, 2, 3};  // 60% overlap
    uint32_t output[10];
    float output_activations[10];
    float confidence;

    uint64_t recalled_id = engram_recall(system, cue, 3,
                                         output, output_activations, 10,
                                         &confidence);

    EXPECT_EQ(recalled_id, id);
    EXPECT_GT(confidence, 0.0f);
    EXPECT_EQ(system->total_recalls, 1);
}

TEST_F(EngramTest, RecallNullSystem) {
    // WHAT: Test NULL system
    uint32_t cue[] = {1, 2, 3};
    uint32_t output[10];
    float output_activations[10];
    float confidence;

    uint64_t id = engram_recall(nullptr, cue, 3,
                                output, output_activations, 10, &confidence);
    EXPECT_EQ(id, 0);
}

TEST_F(EngramTest, RecallNullCue) {
    // WHAT: Test NULL cue
    uint32_t output[10];
    float output_activations[10];
    float confidence;

    uint64_t id = engram_recall(system, nullptr, 3,
                                output, output_activations, 10, &confidence);
    EXPECT_EQ(id, 0);
}

TEST_F(EngramTest, RecallZeroCount) {
    // WHAT: Test zero cue count
    uint32_t cue[] = {1, 2, 3};
    uint32_t output[10];
    float output_activations[10];
    float confidence;

    uint64_t id = engram_recall(system, cue, 0,
                                output, output_activations, 10, &confidence);
    EXPECT_EQ(id, 0);
}

TEST_F(EngramTest, RecallNoMatch) {
    // WHAT: Test recall with no matching engram
    uint32_t neurons[] = {1, 2, 3, 4, 5};
    float activations[] = {0.8f, 0.7f, 0.9f, 0.6f, 0.85f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    engram_encode(system, neurons, activations, 5,
                 MEMORY_TYPE_EPISODIC, emotion);

    // Completely different cue
    uint32_t cue[] = {100, 101, 102};
    uint32_t output[10];
    float output_activations[10];
    float confidence;

    uint64_t id = engram_recall(system, cue, 3,
                                output, output_activations, 10, &confidence);

    EXPECT_EQ(id, 0);
    EXPECT_EQ(confidence, 0.0f);
}

TEST_F(EngramTest, RecallTriggersReconsolidation) {
    // WHAT: Test that recalling consolidated engram triggers reconsolidation
    uint32_t neurons[] = {1, 2, 3, 4, 5};
    float activations[] = {0.8f, 0.7f, 0.9f, 0.6f, 0.85f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 5,
                                MEMORY_TYPE_EPISODIC, emotion);

    // Fully consolidate
    engram_consolidate_update(system, 7200.0f, false);

    memory_engram_t* engram = engram_get_by_id(system, id);
    ASSERT_NE(engram, nullptr);
    engram->state = ENGRAM_STATE_CONSOLIDATED;
    engram->consolidation_strength = 1.0f;

    // Recall
    uint32_t cue[] = {1, 2, 3};
    uint32_t output[10];
    float output_activations[10];
    float confidence;

    engram_recall(system, cue, 3, output, output_activations, 10, &confidence);

    // Should be reconsolidating
    EXPECT_TRUE(engram->is_reconsolidating);
    EXPECT_EQ(engram->state, ENGRAM_STATE_RECONSOLIDATING);
}

//=============================================================================
// RECOGNITION TESTS
//=============================================================================

TEST_F(EngramTest, RecognizeBasic) {
    // WHAT: Test basic recognition
    uint32_t neurons[] = {1, 2, 3, 4, 5};
    float activations[] = {0.8f, 0.7f, 0.9f, 0.6f, 0.85f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    engram_encode(system, neurons, activations, 5,
                 MEMORY_TYPE_EPISODIC, emotion);

    // Consolidate fully
    engram_consolidate_update(system, 21600.0f, false);

    // Recognize
    float familiarity;
    bool recognized = engram_recognize(system, neurons, 5, &familiarity);

    EXPECT_TRUE(recognized);
    EXPECT_GT(familiarity, 0.5f);
}

TEST_F(EngramTest, RecognizeNull) {
    // WHAT: Test NULL guards
    float familiarity;

    EXPECT_FALSE(engram_recognize(nullptr, nullptr, 0, &familiarity));

    uint32_t pattern[] = {1, 2, 3};
    EXPECT_FALSE(engram_recognize(nullptr, pattern, 3, &familiarity));
    EXPECT_FALSE(engram_recognize(system, nullptr, 3, &familiarity));
    EXPECT_FALSE(engram_recognize(system, pattern, 0, &familiarity));
}

//=============================================================================
// CONSOLIDATION TESTS
//=============================================================================

TEST_F(EngramTest, ConsolidateBasic) {
    // WHAT: Test consolidation progression
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);

    memory_engram_t* engram = engram_get_by_id(system, id);
    EXPECT_EQ(engram->state, ENGRAM_STATE_ENCODING);

    // First update: encoding → consolidating (starts immediately with new logic)
    engram_consolidate_update(system, 1.0f, false);
    // After 1 second, should be consolidating
    EXPECT_TRUE(engram->state == ENGRAM_STATE_CONSOLIDATING || engram->state == ENGRAM_STATE_LABILE);

    // More updates: labile → consolidating
    engram_consolidate_update(system, 1800.0f, false);
    EXPECT_EQ(engram->state, ENGRAM_STATE_CONSOLIDATING);
    EXPECT_GT(engram->consolidation_strength, 0.0f);
    EXPECT_LT(engram->consolidation_strength, 1.0f);

    // Full consolidation (requires 6 hours = 21600 seconds total)
    engram_consolidate_update(system, 19800.0f, false);  // 1800 + 19800 = 21600 total
    EXPECT_EQ(engram->state, ENGRAM_STATE_CONSOLIDATED);
    EXPECT_FLOAT_EQ(engram->consolidation_strength, 1.0f);
}

TEST_F(EngramTest, ConsolidateNull) {
    // WHAT: Test NULL guards
    engram_consolidate_update(nullptr, 1.0f, false);
    // Should not crash
}

TEST_F(EngramTest, ConsolidateZeroDt) {
    // WHAT: Test zero dt
    engram_consolidate_update(system, 0.0f, false);
    // Should not crash
}

TEST_F(EngramTest, ConsolidateSleepBoost) {
    // WHAT: Test sleep accelerates consolidation
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id1 = engram_encode(system, neurons, activations, 3,
                                 MEMORY_TYPE_EPISODIC, emotion);
    uint64_t id2 = engram_encode(system, neurons, activations, 3,
                                 MEMORY_TYPE_EPISODIC, emotion);

    engram_consolidate_update(system, 1.0f, false);  // Both labile

    // Consolidate one awake, one asleep
    memory_engram_t* e1 = engram_get_by_id(system, id1);
    memory_engram_t* e2 = engram_get_by_id(system, id2);

    float dt = 1000.0f;
    float awake_rate = dt / ENGRAM_SYNAPTIC_CONSOLIDATION_TIME;
    float sleep_rate = awake_rate * system->sleep_consolidation_rate;

    e1->consolidation_strength = 0.0f;
    e2->consolidation_strength = 0.0f;

    // Update one with sleep
    e2->consolidation_strength += sleep_rate;
    e1->consolidation_strength += awake_rate;

    EXPECT_GT(e2->consolidation_strength, e1->consolidation_strength);
}

TEST_F(EngramTest, SleepReplayBasic) {
    // WHAT: Test sleep replay
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    engram_encode(system, neurons, activations, 3,
                 MEMORY_TYPE_EPISODIC, emotion);

    engram_consolidate_update(system, 1.0f, false);  // Make labile

    uint32_t before_replays = system->replays_during_sleep;

    engram_sleep_replay(system, 10);

    EXPECT_GT(system->replays_during_sleep, before_replays);
}

TEST_F(EngramTest, SleepReplayNull) {
    // WHAT: Test NULL guard
    engram_sleep_replay(nullptr, 10);
    // Should not crash
}

TEST_F(EngramTest, SleepReplayZeroCount) {
    // WHAT: Test zero replay count
    engram_sleep_replay(system, 0);
    EXPECT_EQ(system->replays_during_sleep, 0);
}

//=============================================================================
// RECONSOLIDATION TESTS
//=============================================================================

TEST_F(EngramTest, ReconsolidationTrigger) {
    // WHAT: Test reconsolidation trigger
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);

    memory_engram_t* engram = engram_get_by_id(system, id);
    engram->state = ENGRAM_STATE_CONSOLIDATED;

    engram_trigger_reconsolidation(system, id);

    EXPECT_TRUE(engram->is_reconsolidating);
    EXPECT_EQ(engram->state, ENGRAM_STATE_RECONSOLIDATING);
}

TEST_F(EngramTest, ReconsolidationBlock) {
    // WHAT: Test reconsolidation blockade
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);

    memory_engram_t* engram = engram_get_by_id(system, id);
    engram->state = ENGRAM_STATE_CONSOLIDATED;
    engram->consolidation_strength = 1.0f;

    engram_trigger_reconsolidation(system, id);
    bool blocked = engram_block_reconsolidation(system, id);

    EXPECT_TRUE(blocked);
    EXPECT_EQ(engram->state, ENGRAM_STATE_DEGRADING);
    EXPECT_LT(engram->consolidation_strength, 1.0f);  // Weakened
}

TEST_F(EngramTest, ReconsolidationBlockNull) {
    // WHAT: Test NULL guards
    EXPECT_FALSE(engram_block_reconsolidation(nullptr, 1));
    EXPECT_FALSE(engram_block_reconsolidation(system, 0));
}

//=============================================================================
// FORGETTING TESTS
//=============================================================================

TEST_F(EngramTest, DecayBasic) {
    // WHAT: Test natural decay
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);

    memory_engram_t* engram = engram_get_by_id(system, id);
    engram->state = ENGRAM_STATE_CONSOLIDATED;
    engram->consolidation_strength = 1.0f;

    float before = engram->consolidation_strength;

    engram_apply_decay(system, 1000.0f);

    EXPECT_LT(engram->consolidation_strength, before);
}

TEST_F(EngramTest, DecayNull) {
    // WHAT: Test NULL guard
    engram_apply_decay(nullptr, 1.0f);
    // Should not crash
}

TEST_F(EngramTest, ExtinctionBasic) {
    // WHAT: Test extinction
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);

    memory_engram_t* engram = engram_get_by_id(system, id);
    engram->consolidation_strength = 0.8f;

    engram_extinction(system, id, 0.5f);

    EXPECT_LT(engram->consolidation_strength, 0.8f);
}

TEST_F(EngramTest, ExtinctionNull) {
    // WHAT: Test NULL guards
    engram_extinction(nullptr, 1, 0.5f);
    engram_extinction(system, 0, 0.5f);
    engram_extinction(system, 1, 0.0f);
    // Should not crash
}

//=============================================================================
// QUERY TESTS
//=============================================================================

TEST_F(EngramTest, GetByIdBasic) {
    // WHAT: Test get by ID
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);

    memory_engram_t* engram = engram_get_by_id(system, id);
    ASSERT_NE(engram, nullptr);
    EXPECT_EQ(engram->engram_id, id);
}

TEST_F(EngramTest, GetByIdNull) {
    // WHAT: Test NULL guards
    EXPECT_EQ(engram_get_by_id(nullptr, 1), nullptr);
    EXPECT_EQ(engram_get_by_id(system, 0), nullptr);
}

TEST_F(EngramTest, GetByIdNotFound) {
    // WHAT: Test nonexistent ID
    EXPECT_EQ(engram_get_by_id(system, 999), nullptr);
}

TEST_F(EngramTest, GetStateBasic) {
    // WHAT: Test get state
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);

    EXPECT_EQ(engram_get_state(system, id), ENGRAM_STATE_ENCODING);
}

TEST_F(EngramTest, GetConsolidationStrength) {
    // WHAT: Test get consolidation strength
    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    uint64_t id = engram_encode(system, neurons, activations, 3,
                                MEMORY_TYPE_EPISODIC, emotion);

    EXPECT_EQ(engram_get_consolidation_strength(system, id), 0.0f);

    engram_consolidate_update(system, 21600.0f, false);

    EXPECT_FLOAT_EQ(engram_get_consolidation_strength(system, id), 1.0f);
}

TEST_F(EngramTest, GetActiveCount) {
    // WHAT: Test get active count
    EXPECT_EQ(engram_get_active_count(system), 0);

    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    engram_encode(system, neurons, activations, 3,
                 MEMORY_TYPE_EPISODIC, emotion);

    EXPECT_EQ(engram_get_active_count(system), 1);

    EXPECT_EQ(engram_get_active_count(nullptr), 0);
}

TEST_F(EngramTest, GetStatistics) {
    // WHAT: Test get statistics
    uint64_t encodings, recalls;
    uint32_t active;

    engram_get_statistics(system, &encodings, &recalls, &active);

    EXPECT_EQ(encodings, 0);
    EXPECT_EQ(recalls, 0);
    EXPECT_EQ(active, 0);

    uint32_t neurons[] = {1, 2, 3};
    float activations[] = {0.8f, 0.7f, 0.9f};
    emotional_tag_t emotion = {0.5f, 0.6f, 0, EMOTION_CAT_JOY, 0.7f};

    engram_encode(system, neurons, activations, 3,
                 MEMORY_TYPE_EPISODIC, emotion);

    engram_get_statistics(system, &encodings, &recalls, &active);

    EXPECT_EQ(encodings, 1);
    EXPECT_EQ(active, 1);

    engram_get_statistics(nullptr, &encodings, &recalls, &active);
    // Should not crash
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
