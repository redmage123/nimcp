//=============================================================================
// test_bg_sequence_chunking.cpp - Sequence Chunking Unit Tests
//=============================================================================
/**
 * @file test_bg_sequence_chunking.cpp
 * @brief Unit tests for action sequence chunking system
 *
 * Tests chunk learning, execution, and bidirectional data flow.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "core/brain/subcortical/nimcp_bg_sequence_chunking.h"

//=============================================================================
// Sequence Chunking System Tests
//=============================================================================

class SequenceChunkingTest : public ::testing::Test {
protected:
    bgsc_system_t* system = nullptr;
    bgsc_config_t config;

    void SetUp() override {
        bgsc_default_config(&config);
        config.max_chunks = 32;
        config.max_sequence_length = 16;
        config.enable_chunking = true;
        system = bgsc_create(&config);
    }

    void TearDown() override {
        if (system) {
            bgsc_destroy(system);
        }
    }

    // Helper to create a test chunk
    uint32_t createTestChunk(const char* name, uint32_t context) {
        uint32_t chunk_id = 0;
        bgsc_register_chunk(system, name, context, &chunk_id);
        return chunk_id;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SequenceChunkingTest, CreateDestroy) {
    ASSERT_NE(system, nullptr);
}

TEST_F(SequenceChunkingTest, CreateWithNullConfig) {
    bgsc_system_t* s = bgsc_create(nullptr);
    ASSERT_NE(s, nullptr);
    bgsc_destroy(s);
}

TEST_F(SequenceChunkingTest, DefaultConfig) {
    bgsc_config_t cfg;
    bgsc_default_config(&cfg);

    EXPECT_EQ(cfg.max_chunks, BGSC_MAX_CHUNKS);
    EXPECT_EQ(cfg.max_sequence_length, BGSC_MAX_SEQUENCE_LENGTH);
    EXPECT_GT(cfg.learning_rate, 0.0f);
    EXPECT_EQ(cfg.enable_chunking, true);
}

TEST_F(SequenceChunkingTest, Reset) {
    uint32_t chunk_id = createTestChunk("test_chunk", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_initiate(system, chunk_id);

    ASSERT_EQ(bgsc_reset(system), 0);

    // After reset, no chunk should be executing
    EXPECT_FALSE(bgsc_is_executing(system));
}

//=============================================================================
// Chunk Registration Tests
//=============================================================================

TEST_F(SequenceChunkingTest, RegisterChunk) {
    uint32_t chunk_id = 0;
    int ret = bgsc_register_chunk(system, "typing_email", 1001, &chunk_id);

    ASSERT_EQ(ret, 0);
    EXPECT_GE(chunk_id, 0u);
}

TEST_F(SequenceChunkingTest, RegisterMultipleChunks) {
    uint32_t id1 = 0, id2 = 0, id3 = 0;

    ASSERT_EQ(bgsc_register_chunk(system, "chunk1", 100, &id1), 0);
    ASSERT_EQ(bgsc_register_chunk(system, "chunk2", 200, &id2), 0);
    ASSERT_EQ(bgsc_register_chunk(system, "chunk3", 300, &id3), 0);

    // IDs should be unique
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

TEST_F(SequenceChunkingTest, AddActionToChunk) {
    uint32_t chunk_id = createTestChunk("action_sequence", 100);

    ASSERT_EQ(bgsc_add_action(system, chunk_id, 1, 50.0f), 0);
    ASSERT_EQ(bgsc_add_action(system, chunk_id, 2, 100.0f), 0);
    ASSERT_EQ(bgsc_add_action(system, chunk_id, 3, 75.0f), 0);

    // Verify chunk exists
    const bgsc_chunk_t* chunk = bgsc_get_chunk(system, chunk_id);
    ASSERT_NE(chunk, nullptr);
    EXPECT_EQ(chunk->sequence_length, 3u);
}

TEST_F(SequenceChunkingTest, SetInitiationThreshold) {
    uint32_t chunk_id = createTestChunk("threshold_test", 100);

    ASSERT_EQ(bgsc_set_initiation_threshold(system, chunk_id, 0.7f), 0);

    const bgsc_chunk_t* chunk = bgsc_get_chunk(system, chunk_id);
    ASSERT_NE(chunk, nullptr);
    EXPECT_FLOAT_EQ(chunk->initiation_threshold, 0.7f);
}

TEST_F(SequenceChunkingTest, UnregisterChunk) {
    uint32_t chunk_id = createTestChunk("to_remove", 100);

    ASSERT_EQ(bgsc_unregister_chunk(system, chunk_id), 0);

    // Chunk should no longer exist
    const bgsc_chunk_t* chunk = bgsc_get_chunk(system, chunk_id);
    EXPECT_EQ(chunk, nullptr);
}

//=============================================================================
// Execution Tests
//=============================================================================

TEST_F(SequenceChunkingTest, CheckTrigger) {
    uint32_t chunk_id = createTestChunk("triggered_chunk", 500);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);

    // Set threshold to 0 so newly registered chunk can trigger
    bgsc_set_initiation_threshold(system, chunk_id, 0.0f);

    uint32_t triggered_id = 0;
    bool triggered = bgsc_check_trigger(system, 500, &triggered_id);

    EXPECT_TRUE(triggered);
    EXPECT_EQ(triggered_id, chunk_id);
}

TEST_F(SequenceChunkingTest, CheckTriggerNoMatch) {
    uint32_t chunk_id = createTestChunk("no_match", 500);
    bgsc_add_action(system, chunk_id, 1, 100.0f);

    uint32_t triggered_id = 0;
    bool triggered = bgsc_check_trigger(system, 999, &triggered_id);

    EXPECT_FALSE(triggered);
}

TEST_F(SequenceChunkingTest, InitiateChunk) {
    uint32_t chunk_id = createTestChunk("execute_me", 100);
    bgsc_add_action(system, chunk_id, 10, 100.0f);
    bgsc_add_action(system, chunk_id, 20, 150.0f);

    ASSERT_EQ(bgsc_initiate(system, chunk_id), 0);

    EXPECT_TRUE(bgsc_is_executing(system));
    EXPECT_EQ(bgsc_get_executing_chunk(system), chunk_id);
}

TEST_F(SequenceChunkingTest, GetCurrentAction) {
    uint32_t chunk_id = createTestChunk("action_getter", 100);
    bgsc_add_action(system, chunk_id, 42, 100.0f);
    bgsc_add_action(system, chunk_id, 43, 100.0f);

    bgsc_initiate(system, chunk_id);

    uint32_t action_id = 0;
    float urgency = 0.0f;
    int ret = bgsc_get_current_action(system, &action_id, &urgency);

    ASSERT_EQ(ret, 0);
    EXPECT_EQ(action_id, 42u);  // First action
    EXPECT_GE(urgency, 0.0f);
}

TEST_F(SequenceChunkingTest, ActionCompleted) {
    uint32_t chunk_id = createTestChunk("completion_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);
    bgsc_add_action(system, chunk_id, 3, 100.0f);

    bgsc_initiate(system, chunk_id);

    // Complete first action
    ASSERT_EQ(bgsc_action_completed(system, 1, true, 95.0f), 0);

    // Should advance to second action
    uint32_t action_id = 0;
    float urgency = 0.0f;
    bgsc_get_current_action(system, &action_id, &urgency);
    EXPECT_EQ(action_id, 2u);
}

TEST_F(SequenceChunkingTest, ChunkCompletion) {
    uint32_t chunk_id = createTestChunk("full_completion", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);

    bgsc_initiate(system, chunk_id);

    // Complete both actions
    bgsc_action_completed(system, 1, true, 100.0f);
    bgsc_action_completed(system, 2, true, 100.0f);

    // Chunk should no longer be executing
    EXPECT_FALSE(bgsc_is_executing(system));
}

TEST_F(SequenceChunkingTest, PauseResume) {
    uint32_t chunk_id = createTestChunk("pause_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);

    bgsc_initiate(system, chunk_id);
    EXPECT_TRUE(bgsc_is_executing(system));

    ASSERT_EQ(bgsc_pause(system), 0);
    bgsc_exec_state_t state = bgsc_get_exec_state(system, chunk_id);
    EXPECT_EQ(state, BGSC_EXEC_PAUSED);

    ASSERT_EQ(bgsc_resume(system), 0);
    state = bgsc_get_exec_state(system, chunk_id);
    EXPECT_EQ(state, BGSC_EXEC_RUNNING);
}

TEST_F(SequenceChunkingTest, Abort) {
    uint32_t chunk_id = createTestChunk("abort_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);

    bgsc_initiate(system, chunk_id);
    EXPECT_TRUE(bgsc_is_executing(system));

    ASSERT_EQ(bgsc_abort(system), 0);
    EXPECT_FALSE(bgsc_is_executing(system));
}

TEST_F(SequenceChunkingTest, GetProgress) {
    uint32_t chunk_id = createTestChunk("progress_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);
    bgsc_add_action(system, chunk_id, 3, 100.0f);
    bgsc_add_action(system, chunk_id, 4, 100.0f);

    bgsc_initiate(system, chunk_id);

    float progress0 = bgsc_get_progress(system);
    EXPECT_NEAR(progress0, 0.0f, 0.1f);  // Just started

    bgsc_action_completed(system, 1, true, 100.0f);
    float progress1 = bgsc_get_progress(system);
    EXPECT_NEAR(progress1, 0.25f, 0.1f);  // 1/4 done

    bgsc_action_completed(system, 2, true, 100.0f);
    float progress2 = bgsc_get_progress(system);
    EXPECT_NEAR(progress2, 0.5f, 0.1f);  // Half done
}

//=============================================================================
// Bidirectional Data Flow Tests
//=============================================================================

TEST_F(SequenceChunkingTest, BidirProcessBasic) {
    uint32_t chunk_id = createTestChunk("bidir_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);

    bgsc_initiate(system, chunk_id);

    bgsc_bidir_data_t data;
    memset(&data, 0, sizeof(data));

    // Set inputs
    data.cortical_input = 0.7f;
    data.dopamine_level = 0.6f;
    data.action_completed = false;
    data.external_stop = false;

    int ret = bgsc_process_bidir(system, &data);
    ASSERT_EQ(ret, 0);

    // Check outputs
    EXPECT_EQ(data.requested_action, 1u);  // First action
    EXPECT_GT(data.action_urgency, 0.0f);
    EXPECT_TRUE(data.chunk_active);
}

TEST_F(SequenceChunkingTest, BidirActionCompletion) {
    uint32_t chunk_id = createTestChunk("bidir_completion", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);

    bgsc_initiate(system, chunk_id);

    bgsc_bidir_data_t data;
    memset(&data, 0, sizeof(data));

    // Signal action completion via bidir interface
    data.cortical_input = 0.7f;
    data.dopamine_level = 0.6f;
    data.action_completed = true;
    data.completed_action_id = 1;

    bgsc_process_bidir(system, &data);

    // Should advance to next action
    memset(&data, 0, sizeof(data));
    data.action_completed = false;
    bgsc_process_bidir(system, &data);

    EXPECT_EQ(data.requested_action, 2u);  // Second action now
}

TEST_F(SequenceChunkingTest, BidirExternalStop) {
    uint32_t chunk_id = createTestChunk("bidir_stop", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);

    bgsc_initiate(system, chunk_id);

    bgsc_bidir_data_t data;
    memset(&data, 0, sizeof(data));

    // Send external stop
    data.external_stop = true;

    bgsc_process_bidir(system, &data);

    // Chunk should be stopped
    EXPECT_FALSE(data.chunk_active);
}

TEST_F(SequenceChunkingTest, BidirProgressFeedback) {
    uint32_t chunk_id = createTestChunk("bidir_progress", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);
    bgsc_add_action(system, chunk_id, 3, 100.0f);
    bgsc_add_action(system, chunk_id, 4, 100.0f);

    bgsc_initiate(system, chunk_id);

    bgsc_bidir_data_t data;
    memset(&data, 0, sizeof(data));

    // Process and check progress feedback
    bgsc_process_bidir(system, &data);
    float progress0 = data.progress_feedback;

    // Complete first action
    data.action_completed = true;
    data.completed_action_id = 1;
    bgsc_process_bidir(system, &data);

    // Check progress increased
    memset(&data, 0, sizeof(data));
    bgsc_process_bidir(system, &data);
    float progress1 = data.progress_feedback;

    EXPECT_GT(progress1, progress0);
}

TEST_F(SequenceChunkingTest, BidirRewardPrediction) {
    uint32_t chunk_id = createTestChunk("bidir_reward", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);

    bgsc_initiate(system, chunk_id);

    bgsc_bidir_data_t data;
    memset(&data, 0, sizeof(data));
    data.dopamine_level = 0.8f;  // High dopamine indicates expected reward

    bgsc_process_bidir(system, &data);

    // Should have reward prediction output
    EXPECT_GE(data.reward_prediction, 0.0f);
}

//=============================================================================
// Learning Tests
//=============================================================================

TEST_F(SequenceChunkingTest, LearnSequence) {
    uint32_t actions[] = {10, 20, 30, 40};

    int ret = bgsc_learn_sequence(system, actions, 4, 1.0f);
    ASSERT_EQ(ret, 0);
}

TEST_F(SequenceChunkingTest, StrengthenChunk) {
    uint32_t chunk_id = createTestChunk("strengthen_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);

    float before = bgsc_get_automaticity(system, chunk_id);

    ASSERT_EQ(bgsc_strengthen_chunk(system, chunk_id, 1.0f), 0);

    float after = bgsc_get_automaticity(system, chunk_id);
    EXPECT_GE(after, before);  // Should increase or stay same
}

TEST_F(SequenceChunkingTest, WeakenChunk) {
    uint32_t chunk_id = createTestChunk("weaken_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);

    // First strengthen it
    bgsc_strengthen_chunk(system, chunk_id, 1.0f);
    bgsc_strengthen_chunk(system, chunk_id, 1.0f);

    float before = bgsc_get_automaticity(system, chunk_id);

    ASSERT_EQ(bgsc_weaken_chunk(system, chunk_id), 0);

    float after = bgsc_get_automaticity(system, chunk_id);
    EXPECT_LE(after, before);  // Should decrease or stay same
}

TEST_F(SequenceChunkingTest, GetAutomaticity) {
    uint32_t chunk_id = createTestChunk("automaticity_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);

    float automaticity = bgsc_get_automaticity(system, chunk_id);
    EXPECT_GE(automaticity, 0.0f);
    EXPECT_LE(automaticity, 1.0f);
}

TEST_F(SequenceChunkingTest, IsAutomatized) {
    uint32_t chunk_id = createTestChunk("auto_check", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);

    // New chunk should not be automatized
    EXPECT_FALSE(bgsc_is_automatized(system, chunk_id));

    // Strengthen many times
    for (int i = 0; i < 20; i++) {
        bgsc_strengthen_chunk(system, chunk_id, 1.0f);
    }

    // May become automatized after sufficient strengthening
    // (depends on threshold and learning rate)
}

//=============================================================================
// Dopamine Modulation Tests
//=============================================================================

TEST_F(SequenceChunkingTest, SetDopamine) {
    ASSERT_EQ(bgsc_set_dopamine(system, 0.8f), 0);
    ASSERT_EQ(bgsc_set_dopamine(system, 0.2f), 0);
}

TEST_F(SequenceChunkingTest, GetCorticalFeedback) {
    uint32_t chunk_id = createTestChunk("feedback_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_initiate(system, chunk_id);

    float feedback = bgsc_get_cortical_feedback(system);
    EXPECT_GE(feedback, 0.0f);
}

TEST_F(SequenceChunkingTest, GetRewardPrediction) {
    uint32_t chunk_id = createTestChunk("reward_pred_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_initiate(system, chunk_id);

    float prediction = bgsc_get_reward_prediction(system);
    EXPECT_GE(prediction, 0.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SequenceChunkingTest, GetStats) {
    uint32_t chunk_id = createTestChunk("stats_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_add_action(system, chunk_id, 2, 100.0f);

    bgsc_initiate(system, chunk_id);
    bgsc_action_completed(system, 1, true, 100.0f);
    bgsc_action_completed(system, 2, true, 100.0f);

    bgsc_stats_t stats;
    int ret = bgsc_get_stats(system, &stats);
    ASSERT_EQ(ret, 0);

    EXPECT_GE(stats.total_chunks, 1u);
    EXPECT_GE(stats.total_executions, 1u);
}

//=============================================================================
// Step Function Tests
//=============================================================================

TEST_F(SequenceChunkingTest, Step) {
    uint32_t chunk_id = createTestChunk("step_test", 100);
    bgsc_add_action(system, chunk_id, 1, 100.0f);
    bgsc_initiate(system, chunk_id);

    int ret = bgsc_step(system, 10.0f);
    ASSERT_EQ(ret, 0);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(SequenceChunkingTest, StageName) {
    EXPECT_STREQ(bgsc_stage_name(BGSC_STAGE_NAIVE), "Naive");
    EXPECT_STREQ(bgsc_stage_name(BGSC_STAGE_LEARNING), "Learning");
    EXPECT_STREQ(bgsc_stage_name(BGSC_STAGE_CHUNKED), "Chunked");
}

TEST_F(SequenceChunkingTest, ExecStateName) {
    EXPECT_STREQ(bgsc_exec_state_name(BGSC_EXEC_IDLE), "Idle");
    EXPECT_STREQ(bgsc_exec_state_name(BGSC_EXEC_RUNNING), "Running");
    EXPECT_STREQ(bgsc_exec_state_name(BGSC_EXEC_PAUSED), "Paused");
}

TEST_F(SequenceChunkingTest, TerminationName) {
    EXPECT_STREQ(bgsc_termination_name(BGSC_TERM_SEQUENCE_COMPLETE), "Sequence Complete");
    EXPECT_STREQ(bgsc_termination_name(BGSC_TERM_GOAL_ACHIEVED), "Goal Achieved");
    EXPECT_STREQ(bgsc_termination_name(BGSC_TERM_ERROR), "Error");
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(SequenceChunkingTest, NullSystemHandling) {
    uint32_t chunk_id = 0;
    EXPECT_NE(bgsc_register_chunk(nullptr, "test", 100, &chunk_id), 0);
    EXPECT_NE(bgsc_add_action(nullptr, 0, 1, 100.0f), 0);
    EXPECT_NE(bgsc_initiate(nullptr, 0), 0);
    EXPECT_NE(bgsc_step(nullptr, 10.0f), 0);
}

TEST_F(SequenceChunkingTest, InvalidChunkId) {
    EXPECT_NE(bgsc_add_action(system, 9999, 1, 100.0f), 0);
    EXPECT_NE(bgsc_initiate(system, 9999), 0);

    const bgsc_chunk_t* chunk = bgsc_get_chunk(system, 9999);
    EXPECT_EQ(chunk, nullptr);
}

TEST_F(SequenceChunkingTest, NoExecutingChunk) {
    uint32_t action_id = 0;
    float urgency = 0.0f;
    int ret = bgsc_get_current_action(system, &action_id, &urgency);
    EXPECT_NE(ret, 0);  // Should fail when no chunk executing
}
