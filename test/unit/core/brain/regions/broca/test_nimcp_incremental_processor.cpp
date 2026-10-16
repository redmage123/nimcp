/**
 * @file test_nimcp_incremental_processor.cpp
 * @brief Unit tests for nimcp_incremental_processor.c
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>

#include "core/brain/regions/broca/nimcp_incremental_processor.h"

class IncrementalProcessorTest : public ::testing::Test {
protected:
    incremental_processor_t* processor;
    incremental_config_t config;

    void SetUp() override {
        config = incremental_default_config();
        processor = incremental_create(&config);
        ASSERT_NE(nullptr, processor);
    }

    void TearDown() override {
        incremental_destroy(processor);
    }
};

// Lifecycle Tests
TEST_F(IncrementalProcessorTest, DefaultConfigReasonable) {
    auto cfg = incremental_default_config();
    EXPECT_GT(cfg.input_buffer_size, 0u);
    EXPECT_GT(cfg.output_buffer_size, 0u);
    EXPECT_GT(cfg.commit_delay_ms, 0u);
    EXPECT_TRUE(cfg.enable_revision);
}

TEST_F(IncrementalProcessorTest, CreateWithNullConfig) {
    auto* p = incremental_create(NULL);
    ASSERT_NE(nullptr, p);
    incremental_destroy(p);
}

TEST_F(IncrementalProcessorTest, DestroyNull) {
    incremental_destroy(NULL);
}

TEST_F(IncrementalProcessorTest, Reset) {
    incremental_add_word(processor, "test", 1000);
    EXPECT_TRUE(incremental_reset(processor));
    EXPECT_EQ(incremental_get_status(processor), INCREMENTAL_STATUS_IDLE);
    EXPECT_EQ(incremental_get_pending_count(processor), 0u);
}

// Input Tests
TEST_F(IncrementalProcessorTest, AddUnit) {
    uint32_t id = incremental_add_unit(processor, "hello", UNIT_TYPE_WORD, 1000);
    EXPECT_GT(id, 0u);
    EXPECT_EQ(incremental_get_pending_count(processor), 1u);
}

TEST_F(IncrementalProcessorTest, AddWord) {
    uint32_t id = incremental_add_word(processor, "world", 1000);
    EXPECT_GT(id, 0u);
}

TEST_F(IncrementalProcessorTest, AddPhoneme) {
    uint32_t id = incremental_add_phoneme(processor, 'a', 1000);
    EXPECT_GT(id, 0u);
}

TEST_F(IncrementalProcessorTest, AddMultipleUnits) {
    incremental_add_word(processor, "hello", 1000);
    incremental_add_word(processor, "world", 1100);
    incremental_add_word(processor, "test", 1200);

    EXPECT_EQ(incremental_get_pending_count(processor), 3u);
}

TEST_F(IncrementalProcessorTest, EndInput) {
    incremental_add_word(processor, "test", 1000);
    EXPECT_TRUE(incremental_end_input(processor));
}

// Processing Tests
TEST_F(IncrementalProcessorTest, ProcessPending) {
    incremental_add_word(processor, "hello", 1000);
    incremental_add_word(processor, "world", 1050);

    EXPECT_TRUE(incremental_process(processor, 2000));
}

TEST_F(IncrementalProcessorTest, ForceCommit) {
    incremental_add_word(processor, "test", 1000);
    incremental_process(processor, 1010);

    EXPECT_TRUE(incremental_force_commit(processor));
}

TEST_F(IncrementalProcessorTest, GetOutput) {
    incremental_add_word(processor, "hello", 1000);
    incremental_process(processor, 1100);
    incremental_force_commit(processor);

    incremental_output_t output;
    EXPECT_TRUE(incremental_get_output(processor, &output));

    if (output.unit_count > 0) {
        EXPECT_NE(output.units, nullptr);
        incremental_free_output(&output);
    }
}

TEST_F(IncrementalProcessorTest, FreeOutputNull) {
    incremental_free_output(NULL);
}

// Revision Tests
TEST_F(IncrementalProcessorTest, CanReviseBeforeCommit) {
    uint32_t id = incremental_add_word(processor, "tset", 1000);
    incremental_process(processor, 1010);

    EXPECT_TRUE(incremental_can_revise(processor, id));
}

TEST_F(IncrementalProcessorTest, ReviseUnit) {
    uint32_t id = incremental_add_word(processor, "tset", 1000);
    incremental_process(processor, 1010);

    EXPECT_TRUE(incremental_revise_unit(processor, id, "test"));
}

TEST_F(IncrementalProcessorTest, CannotReviseAfterCommit) {
    uint32_t id = incremental_add_word(processor, "test", 1000);
    incremental_process(processor, 1100);
    incremental_force_commit(processor);

    EXPECT_FALSE(incremental_can_revise(processor, id));
}

TEST_F(IncrementalProcessorTest, GetRevisions) {
    uint32_t id = incremental_add_word(processor, "tset", 1000);
    incremental_process(processor, 1010);
    incremental_revise_unit(processor, id, "test");

    revision_record_t revisions[5];
    uint32_t count = incremental_get_revisions(processor, revisions, 5);

    EXPECT_GT(count, 0u);
}

// Buffer Management Tests
TEST_F(IncrementalProcessorTest, PendingCount) {
    EXPECT_EQ(incremental_get_pending_count(processor), 0u);

    incremental_add_word(processor, "test", 1000);
    EXPECT_GE(incremental_get_pending_count(processor), 1u);
}

TEST_F(IncrementalProcessorTest, CommittedCount) {
    incremental_add_word(processor, "test", 1000);
    incremental_process(processor, 1100);
    incremental_force_commit(processor);

    EXPECT_GT(incremental_get_committed_count(processor), 0u);
}

TEST_F(IncrementalProcessorTest, ClearPending) {
    incremental_add_word(processor, "test1", 1000);
    incremental_add_word(processor, "test2", 1050);

    EXPECT_TRUE(incremental_clear_pending(processor));
}

// Status Tests
TEST_F(IncrementalProcessorTest, StatusTransitions) {
    EXPECT_EQ(incremental_get_status(processor), INCREMENTAL_STATUS_IDLE);

    incremental_add_word(processor, "test", 1000);
    EXPECT_EQ(incremental_get_status(processor), INCREMENTAL_STATUS_BUFFERING);

    incremental_process(processor, 1100);
    // Status should transition during processing
}

// Statistics Tests
TEST_F(IncrementalProcessorTest, StatsTracking) {
    incremental_add_word(processor, "hello", 1000);
    incremental_add_word(processor, "world", 1050);
    incremental_process(processor, 1200);
    incremental_force_commit(processor);

    incremental_stats_t stats;
    EXPECT_TRUE(incremental_get_stats(processor, &stats));
    EXPECT_GT(stats.units_received, 0u);
    EXPECT_GT(stats.units_committed, 0u);
}

TEST_F(IncrementalProcessorTest, StatsReset) {
    incremental_add_word(processor, "test", 1000);

    incremental_reset_stats(processor);

    incremental_stats_t stats;
    incremental_get_stats(processor, &stats);
    EXPECT_EQ(stats.units_received, 0u);
}

// Configuration Tests
TEST_F(IncrementalProcessorTest, GetConfig) {
    incremental_config_t retrieved;
    EXPECT_TRUE(incremental_get_config(processor, &retrieved));
    EXPECT_EQ(retrieved.input_buffer_size, config.input_buffer_size);
}

// Null Checks
TEST_F(IncrementalProcessorTest, NullChecks) {
    EXPECT_EQ(incremental_add_unit(NULL, "test", UNIT_TYPE_WORD, 1000), 0u);
    EXPECT_EQ(incremental_add_unit(processor, NULL, UNIT_TYPE_WORD, 1000), 0u);
    EXPECT_FALSE(incremental_process(NULL, 1000));
    EXPECT_FALSE(incremental_reset(NULL));
    EXPECT_EQ(incremental_get_status(NULL), INCREMENTAL_STATUS_ERROR);
    EXPECT_EQ(incremental_get_pending_count(NULL), 0u);
}

// Disable Revision
TEST_F(IncrementalProcessorTest, RevisionDisabled) {
    incremental_config_t no_rev_config = incremental_default_config();
    no_rev_config.enable_revision = false;

    incremental_processor_t* no_rev = incremental_create(&no_rev_config);
    ASSERT_NE(nullptr, no_rev);

    uint32_t id = incremental_add_word(no_rev, "test", 1000);
    EXPECT_FALSE(incremental_can_revise(no_rev, id));

    incremental_destroy(no_rev);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
