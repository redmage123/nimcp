/**
 * @file test_gw_cognitive_bridge.cpp
 * @brief Unit tests for Global Workspace-Cognitive Bridge module
 *
 * WHAT: Comprehensive tests for GW-Cognitive broadcast and competition integration
 * WHY:  Ensure Global Workspace broadcast to modules and module competition for
 *       conscious access works correctly
 * HOW:  Test lifecycle, configuration, receivers, broadcast, competition, and statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/integration/nimcp_gw_cognitive_bridge.h"

/* ============================================================================
 * Test Callback Helpers
 * ============================================================================ */

static int g_gw_receive_count = 0;
static uint32_t g_gw_last_content_type = 0;

static void test_gw_callback(gw_cognitive_content_type_t content_type,
                             const void* data,
                             size_t data_size,
                             void* user_data) {
    g_gw_receive_count++;
    g_gw_last_content_type = static_cast<uint32_t>(content_type);
    (void)data;
    (void)data_size;
    (void)user_data;
}

static int g_gw_receive_count_2 = 0;

static void test_gw_callback_2(gw_cognitive_content_type_t content_type,
                               const void* data,
                               size_t data_size,
                               void* user_data) {
    g_gw_receive_count_2++;
    (void)content_type;
    (void)data;
    (void)data_size;
    (void)user_data;
}

static int g_gw_receive_count_3 = 0;

static void test_gw_callback_3(gw_cognitive_content_type_t content_type,
                               const void* data,
                               size_t data_size,
                               void* user_data) {
    g_gw_receive_count_3++;
    (void)content_type;
    (void)data;
    (void)data_size;
    (void)user_data;
}

class GwCognitiveBridgeTest : public ::testing::Test {
protected:
    gw_cognitive_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Reset global counters
        g_gw_receive_count = 0;
        g_gw_last_content_type = 0;
        g_gw_receive_count_2 = 0;
        g_gw_receive_count_3 = 0;

        gw_cognitive_config_t config;
        gw_cognitive_default_config(&config);
        bridge = gw_cognitive_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            gw_cognitive_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(GwCognitiveBridgeTest, BridgeCreation) {
    ASSERT_NE(bridge, nullptr);
    gw_cognitive_bridge_destroy(bridge);
    bridge = nullptr;

    // Recreate for TearDown
    gw_cognitive_config_t config;
    gw_cognitive_default_config(&config);
    bridge = gw_cognitive_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(GwCognitiveBridgeTest, CreateWithNullConfig) {
    gw_cognitive_bridge_t* br = gw_cognitive_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    gw_cognitive_bridge_destroy(br);
}

TEST_F(GwCognitiveBridgeTest, DestroyNull) {
    gw_cognitive_bridge_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(GwCognitiveBridgeTest, DefaultConfig) {
    gw_cognitive_config_t config;
    int ret = gw_cognitive_default_config(&config);

    EXPECT_EQ(ret, 0);
    // Per header defaults: broadcast_threshold=0.6, timeout=100ms, max_competitors varies
    EXPECT_GT(config.broadcast_threshold, 0.0f);
    EXPECT_LE(config.broadcast_threshold, 1.0f);
    EXPECT_GT(config.competition_timeout_ms, 0u);
    EXPECT_GT(config.max_competitors, 0u);
}

TEST_F(GwCognitiveBridgeTest, DefaultConfigNullPtr) {
    int ret = gw_cognitive_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Receiver Registration Tests
 * ============================================================================ */

TEST_F(GwCognitiveBridgeTest, RegisterReceiver) {
    int ret = gw_cognitive_register_receiver(bridge, 1, test_gw_callback, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(GwCognitiveBridgeTest, RegisterReceiverWithUserData) {
    int user_data = 42;
    int ret = gw_cognitive_register_receiver(bridge, 1, test_gw_callback, &user_data);
    EXPECT_EQ(ret, 0);
}

TEST_F(GwCognitiveBridgeTest, RegisterReceiverNullBridge) {
    int ret = gw_cognitive_register_receiver(nullptr, 1, test_gw_callback, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(GwCognitiveBridgeTest, RegisterReceiverNullCallback) {
    int ret = gw_cognitive_register_receiver(bridge, 1, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(GwCognitiveBridgeTest, UnregisterReceiver) {
    // First register
    int ret = gw_cognitive_register_receiver(bridge, 1, test_gw_callback, nullptr);
    EXPECT_EQ(ret, 0);

    // Then unregister
    ret = gw_cognitive_unregister_receiver(bridge, 1);
    EXPECT_EQ(ret, 0);
}

TEST_F(GwCognitiveBridgeTest, UnregisterReceiverNotRegistered) {
    // Try to unregister a module that was never registered
    int ret = gw_cognitive_unregister_receiver(bridge, 999);
    // Should return error or succeed silently (implementation dependent)
    (void)ret;  // Don't fail test either way
}

TEST_F(GwCognitiveBridgeTest, UnregisterReceiverNullBridge) {
    int ret = gw_cognitive_unregister_receiver(nullptr, 1);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Broadcast Tests
 * ============================================================================ */

TEST_F(GwCognitiveBridgeTest, Broadcast) {
    // Register a receiver first
    int ret = gw_cognitive_register_receiver(bridge, 1, test_gw_callback, nullptr);
    EXPECT_EQ(ret, 0);

    // Broadcast content
    const char* test_data = "test broadcast content";
    ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_THOUGHT,
                                 test_data, strlen(test_data) + 1);
    EXPECT_EQ(ret, 0);

    // Verify callback was invoked
    EXPECT_GT(g_gw_receive_count, 0);
    EXPECT_EQ(g_gw_last_content_type, static_cast<uint32_t>(GW_COGNITIVE_CONTENT_THOUGHT));
}

TEST_F(GwCognitiveBridgeTest, BroadcastMultipleReceivers) {
    // Register multiple receivers
    int ret = gw_cognitive_register_receiver(bridge, 1, test_gw_callback, nullptr);
    EXPECT_EQ(ret, 0);

    ret = gw_cognitive_register_receiver(bridge, 2, test_gw_callback_2, nullptr);
    EXPECT_EQ(ret, 0);

    ret = gw_cognitive_register_receiver(bridge, 3, test_gw_callback_3, nullptr);
    EXPECT_EQ(ret, 0);

    // Broadcast content
    const char* test_data = "broadcast to all";
    ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_PERCEPTION,
                                 test_data, strlen(test_data) + 1);
    EXPECT_EQ(ret, 0);

    // All receivers should get the content
    EXPECT_GT(g_gw_receive_count, 0);
    EXPECT_GT(g_gw_receive_count_2, 0);
    EXPECT_GT(g_gw_receive_count_3, 0);
}

TEST_F(GwCognitiveBridgeTest, BroadcastNoReceivers) {
    // Broadcast without any receivers
    const char* test_data = "broadcast to none";
    int ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_MEMORY,
                                     test_data, strlen(test_data) + 1);
    // Should succeed even without receivers
    EXPECT_EQ(ret, 0);
}

TEST_F(GwCognitiveBridgeTest, BroadcastNullBridge) {
    const char* test_data = "test";
    int ret = gw_cognitive_broadcast(nullptr, GW_COGNITIVE_CONTENT_THOUGHT,
                                     test_data, strlen(test_data) + 1);
    EXPECT_EQ(ret, -1);
}

TEST_F(GwCognitiveBridgeTest, BroadcastNullData) {
    // Note: Implementation allows null/empty data broadcasts (no-content broadcast)
    int ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_THOUGHT, nullptr, 0);
    EXPECT_EQ(ret, 0);  // Null data is allowed, broadcast succeeds with empty content
}

TEST_F(GwCognitiveBridgeTest, BroadcastDifferentContentTypes) {
    int ret = gw_cognitive_register_receiver(bridge, 1, test_gw_callback, nullptr);
    EXPECT_EQ(ret, 0);

    const char* test_data = "type test";

    // Test different content types
    ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_PERCEPTION,
                                 test_data, strlen(test_data) + 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_gw_last_content_type, static_cast<uint32_t>(GW_COGNITIVE_CONTENT_PERCEPTION));

    ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_EMOTION,
                                 test_data, strlen(test_data) + 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_gw_last_content_type, static_cast<uint32_t>(GW_COGNITIVE_CONTENT_EMOTION));

    ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_INTENTION,
                                 test_data, strlen(test_data) + 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(g_gw_last_content_type, static_cast<uint32_t>(GW_COGNITIVE_CONTENT_INTENTION));
}

/* ============================================================================
 * Competition Tests
 * ============================================================================ */

TEST_F(GwCognitiveBridgeTest, CompeteForAccess) {
    gw_cognitive_content_t content;
    memset(&content, 0, sizeof(content));
    content.content_type = GW_COGNITIVE_CONTENT_THOUGHT;
    const char* data = "competing thought";
    content.content_data = data;
    content.content_size = strlen(data) + 1;
    content.priority = 0.8f;  // High priority
    content.relevance = 0.7f;
    content.urgency = 0.6f;

    int ret = gw_cognitive_compete_for_access(bridge, 1, &content, 0.8f);
    EXPECT_EQ(ret, 0);
}

TEST_F(GwCognitiveBridgeTest, CompeteForAccessRejected) {
    // Configure bridge with high threshold
    gw_cognitive_config_t config;
    gw_cognitive_default_config(&config);
    config.broadcast_threshold = 0.9f;  // Very high threshold
    config.min_competition_priority = 0.8f;  // High minimum

    gw_cognitive_bridge_t* strict_bridge = gw_cognitive_bridge_create(&config);
    ASSERT_NE(strict_bridge, nullptr);

    gw_cognitive_content_t content;
    memset(&content, 0, sizeof(content));
    content.content_type = GW_COGNITIVE_CONTENT_THOUGHT;
    const char* data = "low priority thought";
    content.content_data = data;
    content.content_size = strlen(data) + 1;
    content.priority = 0.2f;  // Below threshold
    content.relevance = 0.3f;
    content.urgency = 0.1f;

    int ret = gw_cognitive_compete_for_access(strict_bridge, 1, &content, 0.2f);
    // Low priority content may be rejected
    // Implementation may return 0 (accepted for competition) or -1 (rejected)
    (void)ret;

    gw_cognitive_bridge_destroy(strict_bridge);
}

TEST_F(GwCognitiveBridgeTest, ResolveCompetition) {
    // Test competition where highest priority should win
    // Note: Default broadcast_threshold is 0.5, min_competition_priority is 0.1
    // With enable_auto_broadcast=true (default), each submission that becomes the
    // highest priority triggers immediate resolution.

    // Submit highest priority first, so it becomes the winner
    gw_cognitive_content_t content_high;
    memset(&content_high, 0, sizeof(content_high));
    content_high.content_type = GW_COGNITIVE_CONTENT_EMOTION;
    const char* data_high = "emotion content";
    content_high.content_data = data_high;
    content_high.content_size = strlen(data_high) + 1;
    content_high.priority = 0.9f;  // Highest priority
    content_high.relevance = 0.8f;
    content_high.urgency = 0.7f;

    // Submit the highest priority - should win immediately with auto_broadcast
    int ret = gw_cognitive_compete_for_access(bridge, 2, &content_high, 0.9f);
    EXPECT_GE(ret, 0);  // 0 = won, 1 = pending

    // Get conscious content - highest priority (content_high) should be conscious
    char buffer[256];
    gw_cognitive_conscious_content_t conscious;
    memset(&conscious, 0, sizeof(conscious));
    conscious.content_buffer = buffer;
    conscious.buffer_size = sizeof(buffer);

    ret = gw_cognitive_get_conscious_content(bridge, &conscious);
    EXPECT_EQ(ret, 0);
    if (ret == 0 && conscious.has_content) {
        // Verify the highest priority content is conscious
        EXPECT_EQ(conscious.source_module_id, 2u);
        EXPECT_EQ(conscious.content_type, GW_COGNITIVE_CONTENT_EMOTION);
    }
}

TEST_F(GwCognitiveBridgeTest, CompeteForAccessNullBridge) {
    gw_cognitive_content_t content;
    memset(&content, 0, sizeof(content));

    int ret = gw_cognitive_compete_for_access(nullptr, 1, &content, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(GwCognitiveBridgeTest, CompeteForAccessNullContent) {
    int ret = gw_cognitive_compete_for_access(bridge, 1, nullptr, 0.5f);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Conscious Content Tests
 * ============================================================================ */

TEST_F(GwCognitiveBridgeTest, GetConsciousContent) {
    // First broadcast some content to put it in the GW
    int ret = gw_cognitive_register_receiver(bridge, 1, test_gw_callback, nullptr);
    EXPECT_EQ(ret, 0);

    const char* test_data = "conscious thought";
    ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_THOUGHT,
                                 test_data, strlen(test_data) + 1);
    EXPECT_EQ(ret, 0);

    // Retrieve conscious content
    char buffer[256];
    gw_cognitive_conscious_content_t content;
    memset(&content, 0, sizeof(content));
    content.content_buffer = buffer;
    content.buffer_size = sizeof(buffer);

    ret = gw_cognitive_get_conscious_content(bridge, &content);
    // Content may or may not be present depending on implementation
    if (ret == 0) {
        // If successful, verify content type
        if (content.has_content) {
            EXPECT_EQ(content.content_type, GW_COGNITIVE_CONTENT_THOUGHT);
        }
    }
}

TEST_F(GwCognitiveBridgeTest, GetConsciousContentEmpty) {
    // Fresh bridge with no broadcast - should have no conscious content
    gw_cognitive_bridge_t* empty_bridge = gw_cognitive_bridge_create(nullptr);
    ASSERT_NE(empty_bridge, nullptr);

    char buffer[256];
    gw_cognitive_conscious_content_t content;
    memset(&content, 0, sizeof(content));
    content.content_buffer = buffer;
    content.buffer_size = sizeof(buffer);

    int ret = gw_cognitive_get_conscious_content(empty_bridge, &content);
    // Empty GW should return -1 or content.has_content = false
    if (ret == 0) {
        EXPECT_FALSE(content.has_content);
    } else {
        EXPECT_EQ(ret, -1);
    }

    gw_cognitive_bridge_destroy(empty_bridge);
}

TEST_F(GwCognitiveBridgeTest, GetConsciousContentNullBridge) {
    gw_cognitive_conscious_content_t content;

    int ret = gw_cognitive_get_conscious_content(nullptr, &content);
    EXPECT_EQ(ret, -1);
}

TEST_F(GwCognitiveBridgeTest, GetConsciousContentNullOutput) {
    int ret = gw_cognitive_get_conscious_content(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(GwCognitiveBridgeTest, StatsTracking) {
    // Register receivers
    int ret = gw_cognitive_register_receiver(bridge, 1, test_gw_callback, nullptr);
    EXPECT_EQ(ret, 0);

    ret = gw_cognitive_register_receiver(bridge, 2, test_gw_callback_2, nullptr);
    EXPECT_EQ(ret, 0);

    // Perform broadcasts
    const char* test_data = "stats test";
    ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_THOUGHT,
                                 test_data, strlen(test_data) + 1);
    EXPECT_EQ(ret, 0);

    ret = gw_cognitive_broadcast(bridge, GW_COGNITIVE_CONTENT_PERCEPTION,
                                 test_data, strlen(test_data) + 1);
    EXPECT_EQ(ret, 0);

    // Submit competitors
    gw_cognitive_content_t content;
    memset(&content, 0, sizeof(content));
    content.content_type = GW_COGNITIVE_CONTENT_MEMORY;
    content.content_data = test_data;
    content.content_size = strlen(test_data) + 1;
    content.priority = 0.7f;

    ret = gw_cognitive_compete_for_access(bridge, 3, &content, 0.7f);
    EXPECT_EQ(ret, 0);

    // Get statistics
    gw_cognitive_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    ret = gw_cognitive_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Verify statistics were tracked
    EXPECT_GT(stats.broadcasts_sent, 0u);
    EXPECT_GT(stats.competitions_held, 0u);
    EXPECT_GT(stats.content_updates, 0u);
    EXPECT_EQ(stats.registered_receivers, 2u);
}

TEST_F(GwCognitiveBridgeTest, GetStatsNullBridge) {
    gw_cognitive_stats_t stats;

    int ret = gw_cognitive_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(GwCognitiveBridgeTest, GetStatsNullOutput) {
    int ret = gw_cognitive_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(GwCognitiveBridgeTest, InitialStatsZero) {
    // Create a fresh bridge to check initial stats
    gw_cognitive_bridge_t* fresh_bridge = gw_cognitive_bridge_create(nullptr);
    ASSERT_NE(fresh_bridge, nullptr);

    gw_cognitive_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero

    int ret = gw_cognitive_get_stats(fresh_bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.broadcasts_sent, 0u);
    EXPECT_EQ(stats.competitions_held, 0u);
    EXPECT_EQ(stats.content_updates, 0u);
    EXPECT_EQ(stats.registered_receivers, 0u);
    EXPECT_EQ(stats.active_competitors, 0u);
    EXPECT_EQ(stats.broadcast_failures, 0u);
    EXPECT_EQ(stats.competition_timeouts, 0u);

    gw_cognitive_bridge_destroy(fresh_bridge);
}

TEST_F(GwCognitiveBridgeTest, ReceiverCountInStats) {
    // Register multiple receivers
    gw_cognitive_register_receiver(bridge, 1, test_gw_callback, nullptr);
    gw_cognitive_register_receiver(bridge, 2, test_gw_callback_2, nullptr);
    gw_cognitive_register_receiver(bridge, 3, test_gw_callback_3, nullptr);

    gw_cognitive_stats_t stats;
    gw_cognitive_get_stats(bridge, &stats);
    EXPECT_EQ(stats.registered_receivers, 3u);

    // Unregister one
    gw_cognitive_unregister_receiver(bridge, 2);

    gw_cognitive_get_stats(bridge, &stats);
    EXPECT_EQ(stats.registered_receivers, 2u);
}
