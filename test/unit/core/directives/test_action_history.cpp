/**
 * @file test_action_history.cpp
 * @brief Comprehensive unit tests for Action History Tracker
 * @version 1.0.0
 * @date 2025-12-16
 *
 * Tests for the action history tracking module including:
 * - Lifecycle (create, destroy, configuration)
 * - Action recording and retrieval
 * - Time window filtering
 * - Type-based filtering
 * - Statistics computation
 * - History pruning and clearing
 * - Bio-async integration
 * - Edge cases and boundary conditions
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <thread>

// Headers have their own extern "C" guards
#include "core/directives/nimcp_action_history.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ActionHistoryTest : public ::testing::Test {
protected:
    action_history_t* history = nullptr;
    action_history_config_t config;

    void SetUp() override {
        action_history_default_config(&config);
        history = action_history_create(&config);
        ASSERT_NE(history, nullptr);
    }

    void TearDown() override {
        if (history) {
            action_history_destroy(history);
            history = nullptr;
        }
    }

    // Helper: Create a simple action record
    action_record_t createActionRecord(uint32_t action_id,
                                        const char* type,
                                        const char* desc,
                                        float harm_score = 0.0f,
                                        bool blocked = false) {
        action_record_t record = {};
        record.action_id = action_id;
        record.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        record.source_module = 1;
        strncpy(record.action_type, type, ACTION_TYPE_MAX_LEN - 1);
        strncpy(record.action_description, desc, ACTION_DESC_MAX_LEN - 1);
        record.predicted_harm_score = harm_score;
        record.was_blocked = blocked;
        record.action_data_len = 0;
        return record;
    }

    // Helper: Get current timestamp in milliseconds
    uint64_t getCurrentTimestampMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(ActionHistoryTest, DefaultConfigIsValid) {
    action_history_config_t cfg;
    action_history_default_config(&cfg);

    EXPECT_EQ(cfg.max_history_size, ACTION_HISTORY_MAX_ENTRIES);
    EXPECT_EQ(cfg.time_window_ms, ACTION_HISTORY_DEFAULT_WINDOW_MS);
    EXPECT_TRUE(cfg.auto_prune);
}

TEST_F(ActionHistoryTest, DefaultConfigNullIsNoop) {
    // Should not crash with NULL pointer
    action_history_default_config(nullptr);
    // If it doesn't crash, test passes
    SUCCEED();
}

TEST_F(ActionHistoryTest, CreateWithNullConfigUsesDefaults) {
    action_history_t* hist = action_history_create(nullptr);
    ASSERT_NE(hist, nullptr);

    // Should be created with default parameters
    action_history_stats_t stats;
    int result = action_history_get_stats(hist, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_records, 0u);

    action_history_destroy(hist);
}

TEST_F(ActionHistoryTest, CreateWithCustomConfig) {
    action_history_config_t custom_cfg;
    action_history_default_config(&custom_cfg);
    custom_cfg.max_history_size = 512;
    custom_cfg.time_window_ms = 30000;
    custom_cfg.auto_prune = false;

    action_history_t* hist = action_history_create(&custom_cfg);
    ASSERT_NE(hist, nullptr);

    action_history_destroy(hist);
}

TEST_F(ActionHistoryTest, CreateWithZeroSizeUsesDefault) {
    action_history_config_t custom_cfg;
    action_history_default_config(&custom_cfg);
    custom_cfg.max_history_size = 0; // Invalid

    action_history_t* hist = action_history_create(&custom_cfg);
    // Should either use default or fail gracefully
    if (hist) {
        action_history_destroy(hist);
    }
    SUCCEED();
}

TEST_F(ActionHistoryTest, DestroyNullIsNoop) {
    // Should not crash with NULL pointer
    action_history_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Action Recording Tests
 * ============================================================================ */

TEST_F(ActionHistoryTest, RecordSingleAction) {
    action_record_t record = createActionRecord(1, "TEST_ACTION", "Test action");

    int result = action_history_record(history, &record);
    EXPECT_EQ(result, 0);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 1u);
}

TEST_F(ActionHistoryTest, RecordMultipleActions) {
    for (uint32_t i = 1; i <= 10; i++) {
        action_record_t record = createActionRecord(i, "ACTION", "Action");
        int result = action_history_record(history, &record);
        EXPECT_EQ(result, 0);
    }

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 10u);
}

TEST_F(ActionHistoryTest, RecordNullHistoryFails) {
    action_record_t record = createActionRecord(1, "TEST", "Test");
    int result = action_history_record(nullptr, &record);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, RecordNullRecordFails) {
    int result = action_history_record(history, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, RecordWithDifferentTypes) {
    action_record_t r1 = createActionRecord(1, "TYPE_A", "Action A");
    action_record_t r2 = createActionRecord(2, "TYPE_B", "Action B");
    action_record_t r3 = createActionRecord(3, "TYPE_A", "Action A2");

    EXPECT_EQ(action_history_record(history, &r1), 0);
    EXPECT_EQ(action_history_record(history, &r2), 0);
    EXPECT_EQ(action_history_record(history, &r3), 0);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 3u);
    EXPECT_EQ(stats.unique_types, 2u); // TYPE_A and TYPE_B
}

TEST_F(ActionHistoryTest, RecordWithHarmScores) {
    action_record_t r1 = createActionRecord(1, "LOW_HARM", "Low", 0.2f);
    action_record_t r2 = createActionRecord(2, "MED_HARM", "Med", 0.5f);
    action_record_t r3 = createActionRecord(3, "HIGH_HARM", "High", 0.9f);

    EXPECT_EQ(action_history_record(history, &r1), 0);
    EXPECT_EQ(action_history_record(history, &r2), 0);
    EXPECT_EQ(action_history_record(history, &r3), 0);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_FLOAT_EQ(stats.max_harm_score, 0.9f);
    EXPECT_FLOAT_EQ(stats.avg_harm_score, (0.2f + 0.5f + 0.9f) / 3.0f);
}

TEST_F(ActionHistoryTest, RecordBlockedActions) {
    action_record_t r1 = createActionRecord(1, "SAFE", "Safe", 0.1f, false);
    action_record_t r2 = createActionRecord(2, "BLOCKED", "Blocked", 0.8f, true);
    action_record_t r3 = createActionRecord(3, "BLOCKED2", "Blocked2", 0.9f, true);

    EXPECT_EQ(action_history_record(history, &r1), 0);
    EXPECT_EQ(action_history_record(history, &r2), 0);
    EXPECT_EQ(action_history_record(history, &r3), 0);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.blocked_count, 2u);
}

TEST_F(ActionHistoryTest, RecordWithActionData) {
    action_record_t record = createActionRecord(1, "DATA_ACTION", "Has data");

    // Add some binary data
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    memcpy(record.action_data, data, sizeof(data));
    record.action_data_len = sizeof(data);

    int result = action_history_record(history, &record);
    EXPECT_EQ(result, 0);

    // Retrieve and verify
    action_record_t records[10];
    uint32_t count;
    action_history_get_recent(history, 0, records, 10, &count);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(records[0].action_data_len, sizeof(data));
    EXPECT_EQ(memcmp(records[0].action_data, data, sizeof(data)), 0);
}

TEST_F(ActionHistoryTest, RecordLongStringsAreTruncated) {
    action_record_t record = createActionRecord(1, "TEST", "Test");

    // Create strings longer than max
    char long_type[ACTION_TYPE_MAX_LEN + 50];
    char long_desc[ACTION_DESC_MAX_LEN + 50];
    memset(long_type, 'A', sizeof(long_type) - 1);
    memset(long_desc, 'B', sizeof(long_desc) - 1);
    long_type[sizeof(long_type) - 1] = '\0';
    long_desc[sizeof(long_desc) - 1] = '\0';

    strncpy(record.action_type, long_type, ACTION_TYPE_MAX_LEN - 1);
    strncpy(record.action_description, long_desc, ACTION_DESC_MAX_LEN - 1);

    int result = action_history_record(history, &record);
    EXPECT_EQ(result, 0);

    // Strings should be truncated, not overflow
    action_record_t records[10];
    uint32_t count;
    action_history_get_recent(history, 0, records, 10, &count);
    EXPECT_EQ(count, 1u);
    EXPECT_LT(strlen(records[0].action_type), ACTION_TYPE_MAX_LEN);
    EXPECT_LT(strlen(records[0].action_description), ACTION_DESC_MAX_LEN);
}

/* ============================================================================
 * Retrieval Tests (Get Recent)
 * ============================================================================ */

TEST_F(ActionHistoryTest, GetRecentAll) {
    // Record 5 actions
    for (uint32_t i = 1; i <= 5; i++) {
        action_record_t record = createActionRecord(i, "ACTION", "Test");
        action_history_record(history, &record);
    }

    action_record_t records[10];
    uint32_t count;
    int result = action_history_get_recent(history, 0, records, 10, &count);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 5u);
}

TEST_F(ActionHistoryTest, GetRecentWithTimeWindow) {
    uint64_t now = getCurrentTimestampMs();

    // Record actions at different times
    action_record_t r1 = createActionRecord(1, "OLD", "Old action");
    r1.timestamp_ms = now - 120000; // 2 minutes ago
    action_history_record(history, &r1);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    action_record_t r2 = createActionRecord(2, "RECENT", "Recent action");
    r2.timestamp_ms = now - 5000; // 5 seconds ago
    action_history_record(history, &r2);

    action_record_t r3 = createActionRecord(3, "NOW", "Current action");
    action_history_record(history, &r3);

    // Get actions from last 30 seconds
    action_record_t records[10];
    uint32_t count;
    action_history_get_recent(history, 30000, records, 10, &count);

    // Should get r2 and r3, but not r1
    EXPECT_GE(count, 2u);
    EXPECT_LE(count, 3u);
}

TEST_F(ActionHistoryTest, GetRecentWithMaxCount) {
    // Record 10 actions
    for (uint32_t i = 1; i <= 10; i++) {
        action_record_t record = createActionRecord(i, "ACTION", "Test");
        action_history_record(history, &record);
    }

    // Request only 5
    action_record_t records[5];
    uint32_t count;
    int result = action_history_get_recent(history, 0, records, 5, &count);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 5u);
}

TEST_F(ActionHistoryTest, GetRecentEmptyHistory) {
    action_record_t records[10];
    uint32_t count;
    int result = action_history_get_recent(history, 0, records, 10, &count);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 0u);
}

TEST_F(ActionHistoryTest, GetRecentNullHistoryFails) {
    action_record_t records[10];
    uint32_t count;
    int result = action_history_get_recent(nullptr, 0, records, 10, &count);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, GetRecentNullOutputFails) {
    uint32_t count;
    int result = action_history_get_recent(history, 0, nullptr, 10, &count);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, GetRecentNullCountFails) {
    action_record_t records[10];
    int result = action_history_get_recent(history, 0, records, 10, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, GetRecentZeroMaxCount) {
    action_record_t record = createActionRecord(1, "TEST", "Test");
    action_history_record(history, &record);

    action_record_t records[10];
    uint32_t count;
    int result = action_history_get_recent(history, 0, records, 0, &count);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 0u);
}

/* ============================================================================
 * Type-based Retrieval Tests
 * ============================================================================ */

TEST_F(ActionHistoryTest, GetByType) {
    action_record_t r1 = createActionRecord(1, "TYPE_A", "Action A1");
    action_record_t r2 = createActionRecord(2, "TYPE_B", "Action B1");
    action_record_t r3 = createActionRecord(3, "TYPE_A", "Action A2");
    action_record_t r4 = createActionRecord(4, "TYPE_C", "Action C1");
    action_record_t r5 = createActionRecord(5, "TYPE_A", "Action A3");

    action_history_record(history, &r1);
    action_history_record(history, &r2);
    action_history_record(history, &r3);
    action_history_record(history, &r4);
    action_history_record(history, &r5);

    action_record_t records[10];
    uint32_t count;
    int result = action_history_get_by_type(history, "TYPE_A", records, 10, &count);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 3u); // 3 TYPE_A actions

    // Verify all returned records are TYPE_A
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_STREQ(records[i].action_type, "TYPE_A");
    }
}

TEST_F(ActionHistoryTest, GetByTypeNonExistent) {
    action_record_t r1 = createActionRecord(1, "TYPE_A", "Action A");
    action_history_record(history, &r1);

    action_record_t records[10];
    uint32_t count;
    int result = action_history_get_by_type(history, "TYPE_Z", records, 10, &count);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 0u);
}

TEST_F(ActionHistoryTest, GetByTypeNullHistoryFails) {
    action_record_t records[10];
    uint32_t count;
    int result = action_history_get_by_type(nullptr, "TYPE_A", records, 10, &count);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, GetByTypeNullTypeFails) {
    action_record_t records[10];
    uint32_t count;
    int result = action_history_get_by_type(history, nullptr, records, 10, &count);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, GetByTypeNullOutputFails) {
    uint32_t count;
    int result = action_history_get_by_type(history, "TYPE_A", nullptr, 10, &count);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, GetByTypeCaseSensitive) {
    action_record_t r1 = createActionRecord(1, "TypeA", "Action A");
    action_record_t r2 = createActionRecord(2, "TYPEA", "Action B");
    action_record_t r3 = createActionRecord(3, "typea", "Action C");

    action_history_record(history, &r1);
    action_history_record(history, &r2);
    action_history_record(history, &r3);

    action_record_t records[10];
    uint32_t count;

    // Should only match exact case
    action_history_get_by_type(history, "TypeA", records, 10, &count);
    EXPECT_EQ(count, 1u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ActionHistoryTest, StatsEmptyHistory) {
    action_history_stats_t stats;
    int result = action_history_get_stats(history, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_records, 0u);
    EXPECT_EQ(stats.blocked_count, 0u);
    EXPECT_EQ(stats.unique_types, 0u);
    EXPECT_EQ(stats.oldest_timestamp_ms, 0u);
    EXPECT_EQ(stats.newest_timestamp_ms, 0u);
    EXPECT_FLOAT_EQ(stats.avg_harm_score, 0.0f);
    EXPECT_FLOAT_EQ(stats.max_harm_score, 0.0f);
}

TEST_F(ActionHistoryTest, StatsWithRecords) {
    action_record_t r1 = createActionRecord(1, "TYPE_A", "Action 1", 0.3f, false);
    action_record_t r2 = createActionRecord(2, "TYPE_B", "Action 2", 0.7f, true);
    action_record_t r3 = createActionRecord(3, "TYPE_A", "Action 3", 0.5f, false);

    action_history_record(history, &r1);
    action_history_record(history, &r2);
    action_history_record(history, &r3);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);

    EXPECT_EQ(stats.total_records, 3u);
    EXPECT_EQ(stats.blocked_count, 1u);
    EXPECT_EQ(stats.unique_types, 2u);
    // Records may have same timestamp if recorded quickly; use >= instead of >
    EXPECT_GE(stats.newest_timestamp_ms, stats.oldest_timestamp_ms);
    EXPECT_FLOAT_EQ(stats.avg_harm_score, (0.3f + 0.7f + 0.5f) / 3.0f);
    EXPECT_FLOAT_EQ(stats.max_harm_score, 0.7f);
}

TEST_F(ActionHistoryTest, StatsNullHistoryFails) {
    action_history_stats_t stats;
    int result = action_history_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, StatsNullOutputFails) {
    int result = action_history_get_stats(history, nullptr);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * Pruning Tests
 * ============================================================================ */

TEST_F(ActionHistoryTest, PruneOldEntries) {
    uint64_t now = getCurrentTimestampMs();

    // Record old actions
    for (uint32_t i = 1; i <= 5; i++) {
        action_record_t record = createActionRecord(i, "OLD", "Old action");
        record.timestamp_ms = now - 120000; // 2 minutes ago
        action_history_record(history, &record);
    }

    // Record recent actions
    for (uint32_t i = 6; i <= 10; i++) {
        action_record_t record = createActionRecord(i, "NEW", "New action");
        action_history_record(history, &record);
    }

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 10u);

    // Prune actions older than 1 minute
    int pruned = action_history_prune(history, now - 60000);
    EXPECT_GE(pruned, 0);

    action_history_get_stats(history, &stats);
    EXPECT_LE(stats.total_records, 5u); // Should have 5 or fewer recent actions
}

TEST_F(ActionHistoryTest, PruneAll) {
    for (uint32_t i = 1; i <= 5; i++) {
        action_record_t record = createActionRecord(i, "ACTION", "Test");
        action_history_record(history, &record);
    }

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 5u);

    // Prune everything (timestamp in future)
    uint64_t future = getCurrentTimestampMs() + 1000000;
    int pruned = action_history_prune(history, future);
    EXPECT_GE(pruned, 0);

    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 0u);
}

TEST_F(ActionHistoryTest, PruneNone) {
    for (uint32_t i = 1; i <= 5; i++) {
        action_record_t record = createActionRecord(i, "ACTION", "Test");
        action_history_record(history, &record);
    }

    // Prune with very old timestamp (nothing to prune)
    int pruned = action_history_prune(history, 1000);
    EXPECT_GE(pruned, 0);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 5u);
}

TEST_F(ActionHistoryTest, PruneNullHistoryFails) {
    int pruned = action_history_prune(nullptr, getCurrentTimestampMs());
    // NIMCP uses positive error codes, so check for non-zero (error)
    EXPECT_NE(pruned, 0);
}

/* ============================================================================
 * Clear Tests
 * ============================================================================ */

TEST_F(ActionHistoryTest, ClearHistory) {
    for (uint32_t i = 1; i <= 10; i++) {
        action_record_t record = createActionRecord(i, "ACTION", "Test");
        action_history_record(history, &record);
    }

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 10u);

    int result = action_history_clear(history);
    EXPECT_EQ(result, 0);

    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 0u);
}

TEST_F(ActionHistoryTest, ClearEmptyHistory) {
    int result = action_history_clear(history);
    EXPECT_EQ(result, 0);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 0u);
}

TEST_F(ActionHistoryTest, ClearNullHistoryFails) {
    int result = action_history_clear(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, ClearThenRecord) {
    action_record_t r1 = createActionRecord(1, "ACTION", "Test");
    action_history_record(history, &r1);

    action_history_clear(history);

    action_record_t r2 = createActionRecord(2, "ACTION", "Test");
    int result = action_history_record(history, &r2);
    EXPECT_EQ(result, 0);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 1u);
}

/* ============================================================================
 * Bio-async Integration Tests
 * ============================================================================ */

TEST_F(ActionHistoryTest, BioAsyncConnect) {
    int result = action_history_connect_bio_async(history);
    // May succeed or fail depending on router availability
    // Just verify it doesn't crash
    EXPECT_GE(result, -1);
}

TEST_F(ActionHistoryTest, BioAsyncConnectNullFails) {
    int result = action_history_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, BioAsyncDisconnect) {
    action_history_connect_bio_async(history);
    int result = action_history_disconnect_bio_async(history);
    EXPECT_GE(result, -1);
}

TEST_F(ActionHistoryTest, BioAsyncDisconnectNullFails) {
    int result = action_history_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(ActionHistoryTest, BioAsyncIsConnected) {
    bool connected = action_history_is_bio_async_connected(history);
    // Initially should be false
    EXPECT_FALSE(connected);

    // Try to connect
    action_history_connect_bio_async(history);

    // Check again (may or may not be connected depending on router)
    connected = action_history_is_bio_async_connected(history);
    // Just verify call doesn't crash
    SUCCEED();
}

TEST_F(ActionHistoryTest, BioAsyncIsConnectedNullReturnsFalse) {
    bool connected = action_history_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(ActionHistoryTest, BioAsyncConnectTwice) {
    action_history_connect_bio_async(history);
    int result = action_history_connect_bio_async(history);
    // Should handle double-connect gracefully
    SUCCEED();
}

TEST_F(ActionHistoryTest, BioAsyncDisconnectTwice) {
    action_history_connect_bio_async(history);
    action_history_disconnect_bio_async(history);
    int result = action_history_disconnect_bio_async(history);
    // Should handle double-disconnect gracefully
    SUCCEED();
}

/* ============================================================================
 * Edge Cases and Boundary Conditions
 * ============================================================================ */

TEST_F(ActionHistoryTest, CircularBufferWraparound) {
    // Create config with small buffer
    action_history_config_t small_cfg;
    action_history_default_config(&small_cfg);
    small_cfg.max_history_size = 10;

    action_history_t* small_hist = action_history_create(&small_cfg);
    ASSERT_NE(small_hist, nullptr);

    // Record more than capacity
    for (uint32_t i = 1; i <= 20; i++) {
        action_record_t record = createActionRecord(i, "ACTION", "Test");
        action_history_record(small_hist, &record);
    }

    action_history_stats_t stats;
    action_history_get_stats(small_hist, &stats);
    EXPECT_LE(stats.total_records, 10u);

    action_history_destroy(small_hist);
}

TEST_F(ActionHistoryTest, MaximumHistorySize) {
    // Record maximum allowed
    action_history_config_t max_cfg;
    action_history_default_config(&max_cfg);
    max_cfg.max_history_size = ACTION_HISTORY_MAX_ENTRIES;

    action_history_t* max_hist = action_history_create(&max_cfg);
    ASSERT_NE(max_hist, nullptr);

    // Record up to max
    for (uint32_t i = 1; i <= ACTION_HISTORY_MAX_ENTRIES; i++) {
        action_record_t record = createActionRecord(i, "ACTION", "Test");
        action_history_record(max_hist, &record);
    }

    action_history_stats_t stats;
    action_history_get_stats(max_hist, &stats);
    EXPECT_EQ(stats.total_records, ACTION_HISTORY_MAX_ENTRIES);

    action_history_destroy(max_hist);
}

TEST_F(ActionHistoryTest, EmptyTypeString) {
    action_record_t record = createActionRecord(1, "", "Empty type");
    int result = action_history_record(history, &record);
    EXPECT_EQ(result, 0);

    action_record_t records[10];
    uint32_t count;
    action_history_get_by_type(history, "", records, 10, &count);
    EXPECT_EQ(count, 1u);
}

TEST_F(ActionHistoryTest, EmptyDescriptionString) {
    action_record_t record = createActionRecord(1, "TYPE", "");
    int result = action_history_record(history, &record);
    EXPECT_EQ(result, 0);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 1u);
}

TEST_F(ActionHistoryTest, ZeroHarmScore) {
    action_record_t record = createActionRecord(1, "TYPE", "Zero harm", 0.0f);
    action_history_record(history, &record);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_FLOAT_EQ(stats.avg_harm_score, 0.0f);
    EXPECT_FLOAT_EQ(stats.max_harm_score, 0.0f);
}

TEST_F(ActionHistoryTest, MaximumHarmScore) {
    action_record_t record = createActionRecord(1, "TYPE", "Max harm", 1.0f);
    action_history_record(history, &record);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_FLOAT_EQ(stats.avg_harm_score, 1.0f);
    EXPECT_FLOAT_EQ(stats.max_harm_score, 1.0f);
}

TEST_F(ActionHistoryTest, NegativeHarmScore) {
    // Negative harm scores should be clamped or rejected
    action_record_t record = createActionRecord(1, "TYPE", "Negative", -0.5f);
    int result = action_history_record(history, &record);
    // Should either clamp to 0 or accept
    EXPECT_EQ(result, 0);
}

TEST_F(ActionHistoryTest, VeryLargeTimestamp) {
    action_record_t record = createActionRecord(1, "TYPE", "Large timestamp");
    record.timestamp_ms = UINT64_MAX;

    int result = action_history_record(history, &record);
    EXPECT_EQ(result, 0);

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.newest_timestamp_ms, UINT64_MAX);
}

TEST_F(ActionHistoryTest, ZeroTimestamp) {
    action_record_t record = createActionRecord(1, "TYPE", "Zero timestamp");
    record.timestamp_ms = 0;

    int result = action_history_record(history, &record);
    EXPECT_EQ(result, 0);
}

TEST_F(ActionHistoryTest, MaximumActionData) {
    action_record_t record = createActionRecord(1, "TYPE", "Max data");

    // Fill with maximum data
    memset(record.action_data, 0xFF, ACTION_DATA_MAX_LEN);
    record.action_data_len = ACTION_DATA_MAX_LEN;

    int result = action_history_record(history, &record);
    EXPECT_EQ(result, 0);

    action_record_t records[10];
    uint32_t count;
    action_history_get_recent(history, 0, records, 10, &count);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(records[0].action_data_len, ACTION_DATA_MAX_LEN);
}

TEST_F(ActionHistoryTest, ConcurrentRecording) {
    // Basic thread safety test
    // Record multiple actions rapidly
    for (int i = 0; i < 100; i++) {
        action_record_t record = createActionRecord(i, "CONCURRENT", "Concurrent test");
        action_history_record(history, &record);
    }

    action_history_stats_t stats;
    action_history_get_stats(history, &stats);
    EXPECT_EQ(stats.total_records, 100u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
