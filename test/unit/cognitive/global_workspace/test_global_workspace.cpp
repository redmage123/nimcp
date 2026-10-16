/**
 * @file test_global_workspace.cpp
 * @brief Comprehensive unit tests for Global Workspace Architecture
 *
 * TEST COVERAGE:
 * - Creation and destruction
 * - Competition mechanisms (winner-take-all, priority-based, round-robin)
 * - Broadcasting and reading
 * - Subscribe/unsubscribe
 * - History tracking
 * - Statistics collection
 * - Configuration validation
 * - Edge cases and error handling
 * - Performance characteristics
 *
 * @author NIMCP Development Team - Part J
 * @date 2025-11-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>

#include "cognitive/global_workspace/nimcp_global_workspace.h"

//=============================================================================
// Test Helpers and Fixtures
//=============================================================================

class GlobalWorkspaceTest : public ::testing::Test {
protected:
    global_workspace_t* workspace;

    void SetUp() override {
        workspace = nullptr;
    }

    void TearDown() override {
        if (workspace != nullptr) {
            global_workspace_destroy(workspace);
            workspace = nullptr;
        }
    }

    // Helper: Create default workspace
    global_workspace_t* CreateDefaultWorkspace() {
        return global_workspace_create();
    }

    // Helper: Create workspace with custom config
    global_workspace_t* CreateCustomWorkspace(const global_workspace_config_t* config) {
        return global_workspace_create_custom(config);
    }

    // Helper: Create test content vector
    std::vector<float> CreateTestContent(uint32_t dim, float base_value = 1.0f) {
        std::vector<float> content(dim);
        for (uint32_t i = 0; i < dim; i++) {
            content[i] = base_value + i * 0.1f;
        }
        return content;
    }

    // Helper: Sleep for milliseconds
    void SleepMs(uint32_t ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
};

//=============================================================================
// 1. Creation and Destruction Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, CreateDefaultValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    // Check default configuration applied
    EXPECT_EQ(global_workspace_get_ignition_threshold(workspace), 0.6f);
    EXPECT_FALSE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 0u);
    EXPECT_EQ(global_workspace_get_competitor_count(workspace), 0u);
}

TEST_F(GlobalWorkspaceTest, CreateCustomValid) {
    global_workspace_config_t config = global_workspace_default_config();
    config.capacity_dim = 128;
    config.ignition_threshold = 0.7f;
    config.refractory_period_ms = 100;

    workspace = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace, nullptr);

    EXPECT_EQ(global_workspace_get_ignition_threshold(workspace), 0.7f);
}

TEST_F(GlobalWorkspaceTest, CreateWithNullConfig) {
    workspace = global_workspace_create_custom(nullptr);
    ASSERT_NE(workspace, nullptr);  // Should use defaults

    // Verify defaults applied
    EXPECT_EQ(global_workspace_get_ignition_threshold(workspace),
              GLOBAL_WORKSPACE_DEFAULT_IGNITION_THRESHOLD);
}

TEST_F(GlobalWorkspaceTest, CreateWithInvalidConfigCapacityTooLarge) {
    global_workspace_config_t config = global_workspace_default_config();
    config.capacity_dim = GLOBAL_WORKSPACE_MAX_DIM + 1;  // Too large

    workspace = CreateCustomWorkspace(&config);
    EXPECT_EQ(workspace, nullptr);  // Should fail validation
}

TEST_F(GlobalWorkspaceTest, CreateWithInvalidConfigThresholdTooLow) {
    global_workspace_config_t config = global_workspace_default_config();
    config.ignition_threshold = 0.1f;  // Below minimum

    workspace = CreateCustomWorkspace(&config);
    EXPECT_EQ(workspace, nullptr);
}

TEST_F(GlobalWorkspaceTest, CreateWithInvalidConfigThresholdTooHigh) {
    global_workspace_config_t config = global_workspace_default_config();
    config.ignition_threshold = 2.5f;  // Above maximum (2.0)

    workspace = CreateCustomWorkspace(&config);
    EXPECT_EQ(workspace, nullptr);
}

TEST_F(GlobalWorkspaceTest, DestroyNull) {
    // Should not crash
    global_workspace_destroy(nullptr);
    SUCCEED();
}

TEST_F(GlobalWorkspaceTest, DestroyValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    global_workspace_destroy(workspace);
    workspace = nullptr;  // Prevent double-free in TearDown
    SUCCEED();
}

//=============================================================================
// 2. Competition Tests - Winner-Take-All
//=============================================================================

TEST_F(GlobalWorkspaceTest, CompeteWinnerTakeAllSingleCompetitorAboveThreshold) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256, 1.0f);
    float strength = 0.8f;  // Above default threshold (0.6)

    bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                         content.data(), 256, strength);

    EXPECT_TRUE(won);  // Should win (only competitor above threshold)
    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_WORKING_MEMORY);
    EXPECT_FLOAT_EQ(global_workspace_get_broadcast_strength(workspace), strength);
}

TEST_F(GlobalWorkspaceTest, CompeteWinnerTakeAllSingleCompetitorBelowThreshold) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256, 1.0f);
    float strength = 0.5f;  // Below default threshold (0.6)

    bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                         content.data(), 256, strength);

    EXPECT_FALSE(won);  // Should lose (below threshold)
    EXPECT_FALSE(global_workspace_has_broadcast(workspace));
}

TEST_F(GlobalWorkspaceTest, CompeteWinnerTakeAllMultipleCompetitorsStrongestWins) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);
    auto content3 = CreateTestContent(256, 3.0f);

    // Use new submit+resolve API for batch competition
    global_workspace_submit(workspace, MODULE_WORKING_MEMORY, content1.data(), 256, 0.7f);
    global_workspace_submit(workspace, MODULE_EXECUTIVE, content2.data(), 256, 0.9f);  // Strongest
    global_workspace_submit(workspace, MODULE_SALIENCE, content3.data(), 256, 0.65f);

    // Resolve competition - only strongest should win
    cognitive_module_t winner = MODULE_NONE;
    bool broadcast = global_workspace_resolve(workspace, &winner);

    EXPECT_TRUE(broadcast);
    EXPECT_EQ(winner, MODULE_EXECUTIVE);  // Strongest wins

    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);
    EXPECT_FLOAT_EQ(global_workspace_get_broadcast_strength(workspace), 0.9f);
}

TEST_F(GlobalWorkspaceTest, CompeteInvalidNullWorkspace) {
    auto content = CreateTestContent(256);
    bool won = global_workspace_compete(nullptr, MODULE_WORKING_MEMORY,
                                         content.data(), 256, 0.8f);
    EXPECT_FALSE(won);
}

TEST_F(GlobalWorkspaceTest, CompeteInvalidNullContent) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                         nullptr, 256, 0.8f);
    EXPECT_FALSE(won);
}

TEST_F(GlobalWorkspaceTest, CompeteInvalidDimensionMismatch) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(128);  // Wrong size (expect 256)
    bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                         content.data(), 128, 0.8f);
    EXPECT_FALSE(won);
}

TEST_F(GlobalWorkspaceTest, CompeteInvalidStrengthNegative) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                         content.data(), 256, -0.1f);
    EXPECT_FALSE(won);
}

TEST_F(GlobalWorkspaceTest, CompeteInvalidStrengthTooLarge) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                         content.data(), 256, 1.5f);
    EXPECT_FALSE(won);
}

//=============================================================================
// 3. Refractory Period Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, RefractoryPeriodBlocksCompetition) {
    global_workspace_config_t config = global_workspace_default_config();
    config.refractory_period_ms = 100;  // 100ms refractory
    workspace = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);

    // First competition (should win)
    bool won1 = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                          content1.data(), 256, 0.8f);
    EXPECT_TRUE(won1);

    // Second competition immediately (should be blocked by refractory)
    bool won2 = global_workspace_compete(workspace, MODULE_EXECUTIVE,
                                          content2.data(), 256, 0.9f);
    EXPECT_FALSE(won2);  // Blocked by refractory period

    // Broadcast should still be from first winner
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_WORKING_MEMORY);
}

TEST_F(GlobalWorkspaceTest, RefractoryPeriodAllowsCompetitionAfterDelay) {
    global_workspace_config_t config = global_workspace_default_config();
    config.refractory_period_ms = 50;  // Short refractory
    workspace = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);

    // First competition
    bool won1 = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                          content1.data(), 256, 0.8f);
    EXPECT_TRUE(won1);

    // Wait for refractory period to expire
    SleepMs(60);  // Wait longer than refractory period

    // Second competition (should now be allowed)
    bool won2 = global_workspace_compete(workspace, MODULE_EXECUTIVE,
                                          content2.data(), 256, 0.9f);
    EXPECT_TRUE(won2);  // Should succeed

    // Broadcast should be from second winner
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);
}

//=============================================================================
// 4. Broadcast Reading Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, ReadBroadcastValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto original_content = CreateTestContent(256, 5.0f);

    // Broadcast something
    bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                         original_content.data(), 256, 0.8f);
    ASSERT_TRUE(won);

    // Read broadcast
    std::vector<float> read_content(256);
    uint32_t dim;
    cognitive_module_t source;

    bool read_ok = global_workspace_read_broadcast(workspace, read_content.data(),
                                                    256, &dim, &source);

    EXPECT_TRUE(read_ok);
    EXPECT_EQ(dim, 256u);
    EXPECT_EQ(source, MODULE_WORKING_MEMORY);

    // Verify content matches
    for (uint32_t i = 0; i < 256; i++) {
        EXPECT_FLOAT_EQ(read_content[i], original_content[i]);
    }
}

TEST_F(GlobalWorkspaceTest, ReadBroadcastNoBroadcastAvailable) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    std::vector<float> read_content(256);
    uint32_t dim;
    cognitive_module_t source;

    bool read_ok = global_workspace_read_broadcast(workspace, read_content.data(),
                                                    256, &dim, &source);

    EXPECT_FALSE(read_ok);  // No broadcast available
}

TEST_F(GlobalWorkspaceTest, ReadBroadcastInvalidNullWorkspace) {
    std::vector<float> read_content(256);
    uint32_t dim;
    cognitive_module_t source;

    bool read_ok = global_workspace_read_broadcast(nullptr, read_content.data(),
                                                    256, &dim, &source);
    EXPECT_FALSE(read_ok);
}

TEST_F(GlobalWorkspaceTest, ReadBroadcastInvalidNullBuffer) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);

    uint32_t dim;
    cognitive_module_t source;

    bool read_ok = global_workspace_read_broadcast(workspace, nullptr, 256, &dim, &source);
    EXPECT_FALSE(read_ok);
}

TEST_F(GlobalWorkspaceTest, ReadBroadcastBufferTooSmall) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);

    std::vector<float> read_content(128);  // Too small
    uint32_t dim;
    cognitive_module_t source;

    bool read_ok = global_workspace_read_broadcast(workspace, read_content.data(),
                                                    128, &dim, &source);
    EXPECT_FALSE(read_ok);  // Should fail due to buffer size
}

//=============================================================================
// 5. Subscribe/Unsubscribe Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, SubscribeValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    bool ok = global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
    EXPECT_TRUE(ok);
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 1u);
}

TEST_F(GlobalWorkspaceTest, SubscribeMultipleModules) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    EXPECT_TRUE(global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY));
    EXPECT_TRUE(global_workspace_subscribe(workspace, MODULE_EXECUTIVE));
    EXPECT_TRUE(global_workspace_subscribe(workspace, MODULE_THEORY_OF_MIND));

    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 3u);
}

TEST_F(GlobalWorkspaceTest, SubscribeIdempotent) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    EXPECT_TRUE(global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY));
    EXPECT_TRUE(global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY));  // Again

    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 1u);  // Only counted once
}

TEST_F(GlobalWorkspaceTest, UnsubscribeValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 1u);

    bool ok = global_workspace_unsubscribe(workspace, MODULE_WORKING_MEMORY);
    EXPECT_TRUE(ok);
    EXPECT_EQ(global_workspace_get_subscriber_count(workspace), 0u);
}

TEST_F(GlobalWorkspaceTest, UnsubscribeNotSubscribed) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    bool ok = global_workspace_unsubscribe(workspace, MODULE_WORKING_MEMORY);
    EXPECT_FALSE(ok);  // Wasn't subscribed
}

TEST_F(GlobalWorkspaceTest, SubscribeMaxCapacity) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    // Subscribe up to maximum
    for (uint32_t i = 0; i < GLOBAL_WORKSPACE_MAX_SUBSCRIBERS; i++) {
        cognitive_module_t module = static_cast<cognitive_module_t>(
            MODULE_PERCEPTION + i % (MODULE_CUSTOM_START - MODULE_PERCEPTION)
        );
        EXPECT_TRUE(global_workspace_subscribe(workspace, module));
    }

    EXPECT_EQ(global_workspace_get_subscriber_count(workspace),
              GLOBAL_WORKSPACE_MAX_SUBSCRIBERS);

    // Try to exceed maximum (should fail)
    bool ok = global_workspace_subscribe(workspace, MODULE_CUSTOM_START);
    EXPECT_FALSE(ok);
}

//=============================================================================
// 6. Query and Inspection Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, HasBroadcastInitiallyFalse) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    EXPECT_FALSE(global_workspace_has_broadcast(workspace));
}

TEST_F(GlobalWorkspaceTest, HasBroadcastTrueAfterWinning) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);

    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
}

TEST_F(GlobalWorkspaceTest, GetBroadcastSourceValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    global_workspace_compete(workspace, MODULE_EXECUTIVE, content.data(), 256, 0.8f);

    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);
}

TEST_F(GlobalWorkspaceTest, GetBroadcastSourceNoBroadcast) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_NONE);
}

TEST_F(GlobalWorkspaceTest, GetBroadcastStrengthValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    float strength = 0.75f;
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content.data(), 256, strength);

    EXPECT_FLOAT_EQ(global_workspace_get_broadcast_strength(workspace), strength);
}

TEST_F(GlobalWorkspaceTest, IsCompetingTrue) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);

    // Module 1 competes and wins
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content1.data(), 256, 0.8f);

    // Module 2 competes but doesn't win yet (still in pool)
    global_workspace_compete(workspace, MODULE_EXECUTIVE, content2.data(), 256, 0.7f);

    // Module 2 should still be in competition pool
    EXPECT_TRUE(global_workspace_is_competing(workspace, MODULE_EXECUTIVE));
}

//=============================================================================
// 7. History Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, HistoryTracksBroadcasts) {
    global_workspace_config_t config = global_workspace_default_config();
    config.enable_history = true;
    config.history_depth = 5;
    config.refractory_period_ms = 10;  // Short for faster testing
    workspace = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace, nullptr);

    // Generate 3 broadcasts
    std::vector<cognitive_module_t> modules = {
        MODULE_WORKING_MEMORY, MODULE_EXECUTIVE, MODULE_SALIENCE
    };

    for (size_t i = 0; i < modules.size(); i++) {
        auto content = CreateTestContent(256, 1.0f + i);
        global_workspace_compete(workspace, modules[i], content.data(), 256, 0.8f);
        SleepMs(15);  // Wait for refractory period
    }

    // Read history
    workspace_broadcast_t history[5];
    uint32_t count;
    bool ok = global_workspace_get_history(workspace, history, 5, &count);

    EXPECT_TRUE(ok);
    EXPECT_EQ(count, 3u);  // Should have 3 broadcasts

    // Most recent first
    EXPECT_EQ(history[0].source_module, MODULE_SALIENCE);
    EXPECT_EQ(history[1].source_module, MODULE_EXECUTIVE);
    EXPECT_EQ(history[2].source_module, MODULE_WORKING_MEMORY);
}

TEST_F(GlobalWorkspaceTest, HistoryCircularBuffer) {
    global_workspace_config_t config = global_workspace_default_config();
    config.enable_history = true;
    config.history_depth = 3;  // Small circular buffer
    config.refractory_period_ms = 10;
    workspace = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace, nullptr);

    // Generate 5 broadcasts (more than history depth)
    for (uint32_t i = 0; i < 5; i++) {
        auto content = CreateTestContent(256, 1.0f + i);
        cognitive_module_t module = static_cast<cognitive_module_t>(MODULE_PERCEPTION + i);
        global_workspace_compete(workspace, module, content.data(), 256, 0.8f);
        SleepMs(15);
    }

    // Read history (should only get last 3)
    workspace_broadcast_t history[5];
    uint32_t count;
    global_workspace_get_history(workspace, history, 5, &count);

    EXPECT_EQ(count, 3u);  // Circular buffer holds only 3
}

TEST_F(GlobalWorkspaceTest, HistoryDisabled) {
    global_workspace_config_t config = global_workspace_default_config();
    config.enable_history = false;
    workspace = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);

    workspace_broadcast_t history[10];
    uint32_t count;
    bool ok = global_workspace_get_history(workspace, history, 10, &count);

    EXPECT_FALSE(ok);  // History disabled
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// 8. Statistics Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, StatisticsTracksCompetitions) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);

    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content1.data(), 256, 0.8f);
    global_workspace_compete(workspace, MODULE_EXECUTIVE, content2.data(), 256, 0.9f);

    workspace_statistics_t stats;
    bool ok = global_workspace_get_statistics(workspace, &stats);

    EXPECT_TRUE(ok);
    EXPECT_GE(stats.total_competitions, 2u);
    EXPECT_GE(stats.total_broadcasts, 1u);
}

TEST_F(GlobalWorkspaceTest, StatisticsPerModule) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    // Module 1 wins twice
    for (uint32_t i = 0; i < 2; i++) {
        auto content = CreateTestContent(256, 1.0f + i);
        global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);
        SleepMs(60);  // Wait for refractory
    }

    workspace_statistics_t stats;
    global_workspace_get_statistics(workspace, &stats);

    EXPECT_EQ(stats.broadcasts_per_module[MODULE_WORKING_MEMORY], 2u);
}

TEST_F(GlobalWorkspaceTest, StatisticsReset) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);

    global_workspace_reset_statistics(workspace);

    workspace_statistics_t stats;
    global_workspace_get_statistics(workspace, &stats);

    EXPECT_EQ(stats.total_competitions, 0u);
    EXPECT_EQ(stats.total_broadcasts, 0u);
}

//=============================================================================
// 9. Configuration and Tuning Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, SetIgnitionThresholdValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    bool ok = global_workspace_set_ignition_threshold(workspace, 0.75f);
    EXPECT_TRUE(ok);
    EXPECT_FLOAT_EQ(global_workspace_get_ignition_threshold(workspace), 0.75f);
}

TEST_F(GlobalWorkspaceTest, SetIgnitionThresholdClamps) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    // Too low - should clamp
    global_workspace_set_ignition_threshold(workspace, 0.1f);
    EXPECT_GE(global_workspace_get_ignition_threshold(workspace),
              GLOBAL_WORKSPACE_MIN_IGNITION_THRESHOLD);

    // Too high - should clamp (max is now 2.0 for inflammation modulation)
    global_workspace_set_ignition_threshold(workspace, 2.5f);
    EXPECT_LE(global_workspace_get_ignition_threshold(workspace),
              GLOBAL_WORKSPACE_MAX_IGNITION_THRESHOLD);
}

TEST_F(GlobalWorkspaceTest, SetModulePriorityValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    bool ok = global_workspace_set_module_priority(workspace, MODULE_WELLBEING, 0.9f);
    EXPECT_TRUE(ok);

    // Verify priority affects competition (test with priority-based strategy)
    global_workspace_config_t config = global_workspace_default_config();
    config.strategy = COMPETITION_PRIORITY_BASED;
    global_workspace_t* workspace2 = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace2, nullptr);

    global_workspace_set_module_priority(workspace2, MODULE_WELLBEING, 1.0f);
    global_workspace_set_module_priority(workspace2, MODULE_CURIOSITY, 0.3f);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);

    // Use new submit+resolve API for batch priority testing
    global_workspace_submit(workspace2, MODULE_CURIOSITY, content1.data(), 256, 0.9f);
    global_workspace_submit(workspace2, MODULE_WELLBEING, content2.data(), 256, 0.7f);

    cognitive_module_t winner = MODULE_NONE;
    global_workspace_resolve(workspace2, &winner);

    // Higher priority should win despite lower strength
    EXPECT_EQ(winner, MODULE_WELLBEING);
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace2), MODULE_WELLBEING);

    global_workspace_destroy(workspace2);
}

//=============================================================================
// 10. Utility Function Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, ModuleToStringValid) {
    EXPECT_STREQ(cognitive_module_to_string(MODULE_WORKING_MEMORY), "WORKING_MEMORY");
    EXPECT_STREQ(cognitive_module_to_string(MODULE_EXECUTIVE), "EXECUTIVE");
    EXPECT_STREQ(cognitive_module_to_string(MODULE_NONE), "NONE");
}

TEST_F(GlobalWorkspaceTest, ModuleToStringUnknown) {
    cognitive_module_t invalid = static_cast<cognitive_module_t>(9999);
    EXPECT_STREQ(cognitive_module_to_string(invalid), "UNKNOWN");
}

TEST_F(GlobalWorkspaceTest, StrategyToStringValid) {
    EXPECT_STREQ(competition_strategy_to_string(COMPETITION_WINNER_TAKE_ALL),
                 "WINNER_TAKE_ALL");
    EXPECT_STREQ(competition_strategy_to_string(COMPETITION_PRIORITY_BASED),
                 "PRIORITY_BASED");
}

TEST_F(GlobalWorkspaceTest, ValidateConfigValid) {
    global_workspace_config_t config = global_workspace_default_config();
    char error[256];

    bool ok = global_workspace_validate_config(&config, error, sizeof(error));
    EXPECT_TRUE(ok);
    EXPECT_EQ(error[0], '\0');  // No error message
}

TEST_F(GlobalWorkspaceTest, ValidateConfigInvalidCapacity) {
    global_workspace_config_t config = global_workspace_default_config();
    config.capacity_dim = 0;  // Invalid
    char error[256];

    bool ok = global_workspace_validate_config(&config, error, sizeof(error));
    EXPECT_FALSE(ok);
    EXPECT_NE(error[0], '\0');  // Should have error message
}

//=============================================================================
// 11. Priority-Based Competition Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, PriorityBasedHighPriorityWins) {
    global_workspace_config_t config = global_workspace_default_config();
    config.strategy = COMPETITION_PRIORITY_BASED;
    workspace = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace, nullptr);

    // Set priorities
    global_workspace_set_module_priority(workspace, MODULE_WELLBEING, 1.0f);    // Highest
    global_workspace_set_module_priority(workspace, MODULE_CURIOSITY, 0.3f);    // Lowest

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);

    // Use new submit+resolve API for batch competition
    global_workspace_submit(workspace, MODULE_CURIOSITY, content1.data(), 256, 0.95f);
    global_workspace_submit(workspace, MODULE_WELLBEING, content2.data(), 256, 0.7f);

    cognitive_module_t winner = MODULE_NONE;
    global_workspace_resolve(workspace, &winner);

    // High priority should win
    EXPECT_EQ(winner, MODULE_WELLBEING);
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_WELLBEING);
}

//=============================================================================
// 12. Round-Robin Competition Tests
//=============================================================================

TEST_F(GlobalWorkspaceTest, RoundRobinFairness) {
    global_workspace_config_t config = global_workspace_default_config();
    config.strategy = COMPETITION_ROUND_ROBIN;
    config.refractory_period_ms = 10;
    workspace = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace, nullptr);

    // Add multiple competitors
    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);
    auto content3 = CreateTestContent(256, 3.0f);

    std::vector<cognitive_module_t> winners;

    for (int round = 0; round < 6; round++) {
        // Use new submit+resolve API for batch competition
        global_workspace_submit(workspace, MODULE_WORKING_MEMORY, content1.data(), 256, 0.7f);
        global_workspace_submit(workspace, MODULE_EXECUTIVE, content2.data(), 256, 0.7f);
        global_workspace_submit(workspace, MODULE_SALIENCE, content3.data(), 256, 0.7f);

        cognitive_module_t winner = MODULE_NONE;
        bool broadcast = global_workspace_resolve(workspace, &winner);

        if (broadcast) {
            winners.push_back(winner);
        }

        SleepMs(15);  // Wait for refractory period
    }

    // Should have multiple winners (round-robin)
    // Check that not all winners are the same (fairness)
    cognitive_module_t first_winner = winners[0];
    bool has_different_winner = false;
    for (size_t i = 1; i < winners.size(); i++) {
        if (winners[i] != first_winner) {
            has_different_winner = true;
            break;
        }
    }
    EXPECT_TRUE(has_different_winner);  // Round-robin should alternate
}

//=============================================================================
// 13. Print State Tests (Visual Inspection)
//=============================================================================

TEST_F(GlobalWorkspaceTest, PrintStateValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    // Add some state
    global_workspace_subscribe(workspace, MODULE_WORKING_MEMORY);
    global_workspace_subscribe(workspace, MODULE_EXECUTIVE);

    auto content = CreateTestContent(256, 42.0f);
    global_workspace_compete(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);

    // Should not crash (output goes to stderr)
    global_workspace_print_state(workspace, false);
    global_workspace_print_state(workspace, true);
    SUCCEED();
}

//=============================================================================
// New API Tests: submit() and resolve()
//=============================================================================

TEST_F(GlobalWorkspaceTest, SubmitValid) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256, 1.0f);

    // Submit should succeed
    bool submitted = global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                                             content.data(), 256, 0.8f);
    EXPECT_TRUE(submitted);

    // Should have 1 competitor in pool
    EXPECT_EQ(global_workspace_get_competitor_count(workspace), 1u);

    // Should NOT have broadcast yet (submit doesn't resolve)
    EXPECT_FALSE(global_workspace_has_broadcast(workspace));
}

TEST_F(GlobalWorkspaceTest, SubmitInvalidNullWorkspace) {
    auto content = CreateTestContent(256);

    bool submitted = global_workspace_submit(nullptr, MODULE_WORKING_MEMORY,
                                             content.data(), 256, 0.8f);
    EXPECT_FALSE(submitted);
}

TEST_F(GlobalWorkspaceTest, SubmitInvalidNullContent) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    bool submitted = global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                                             nullptr, 256, 0.8f);
    EXPECT_FALSE(submitted);
}

TEST_F(GlobalWorkspaceTest, SubmitInvalidDimensionMismatch) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(128);  // Wrong size

    bool submitted = global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                                             content.data(), 128, 0.8f);
    EXPECT_FALSE(submitted);
}

TEST_F(GlobalWorkspaceTest, SubmitInvalidStrengthNegative) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);

    bool submitted = global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                                             content.data(), 256, -0.5f);
    EXPECT_FALSE(submitted);
}

TEST_F(GlobalWorkspaceTest, SubmitInvalidStrengthTooLarge) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);

    bool submitted = global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                                             content.data(), 256, 1.5f);
    EXPECT_FALSE(submitted);
}

TEST_F(GlobalWorkspaceTest, SubmitMultipleCompetitors) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);
    auto content3 = CreateTestContent(256, 3.0f);

    // Submit three competitors
    EXPECT_TRUE(global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                                        content1.data(), 256, 0.7f));
    EXPECT_TRUE(global_workspace_submit(workspace, MODULE_EXECUTIVE,
                                        content2.data(), 256, 0.9f));
    EXPECT_TRUE(global_workspace_submit(workspace, MODULE_SALIENCE,
                                        content3.data(), 256, 0.65f));

    // Should have 3 competitors in pool
    EXPECT_EQ(global_workspace_get_competitor_count(workspace), 3u);

    // Should NOT have broadcast yet
    EXPECT_FALSE(global_workspace_has_broadcast(workspace));
}

TEST_F(GlobalWorkspaceTest, SubmitUpdateExisting) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);

    // Submit first time
    EXPECT_TRUE(global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                                        content1.data(), 256, 0.7f));
    EXPECT_EQ(global_workspace_get_competitor_count(workspace), 1u);

    // Submit again with same module (should update, not add)
    EXPECT_TRUE(global_workspace_submit(workspace, MODULE_WORKING_MEMORY,
                                        content2.data(), 256, 0.8f));
    EXPECT_EQ(global_workspace_get_competitor_count(workspace), 1u);  // Still 1
}

TEST_F(GlobalWorkspaceTest, ResolveEmptyPool) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    cognitive_module_t winner = MODULE_WORKING_MEMORY;  // Init to non-NONE

    // Resolve with empty pool should fail
    bool resolved = global_workspace_resolve(workspace, &winner);
    EXPECT_FALSE(resolved);
    EXPECT_EQ(winner, MODULE_NONE);
}

TEST_F(GlobalWorkspaceTest, ResolveInvalidNullWorkspace) {
    cognitive_module_t winner = MODULE_NONE;

    bool resolved = global_workspace_resolve(nullptr, &winner);
    EXPECT_FALSE(resolved);
}

TEST_F(GlobalWorkspaceTest, ResolveNullWinningModule) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    global_workspace_submit(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);

    // Should succeed even with NULL winning_module parameter
    bool resolved = global_workspace_resolve(workspace, nullptr);
    EXPECT_TRUE(resolved);
}

TEST_F(GlobalWorkspaceTest, ResolveSingleCompetitor) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    global_workspace_submit(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);

    cognitive_module_t winner = MODULE_NONE;
    bool resolved = global_workspace_resolve(workspace, &winner);

    EXPECT_TRUE(resolved);
    EXPECT_EQ(winner, MODULE_WORKING_MEMORY);
    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_WORKING_MEMORY);
    EXPECT_FLOAT_EQ(global_workspace_get_broadcast_strength(workspace), 0.8f);
}

TEST_F(GlobalWorkspaceTest, ResolveMultipleCompetitorsStrongestWins) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);
    auto content3 = CreateTestContent(256, 3.0f);

    // Submit three competitors with different strengths
    global_workspace_submit(workspace, MODULE_WORKING_MEMORY, content1.data(), 256, 0.7f);
    global_workspace_submit(workspace, MODULE_EXECUTIVE, content2.data(), 256, 0.9f);  // Strongest
    global_workspace_submit(workspace, MODULE_SALIENCE, content3.data(), 256, 0.65f);

    // Resolve - strongest should win
    cognitive_module_t winner = MODULE_NONE;
    bool resolved = global_workspace_resolve(workspace, &winner);

    EXPECT_TRUE(resolved);
    EXPECT_EQ(winner, MODULE_EXECUTIVE);
    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);
    EXPECT_FLOAT_EQ(global_workspace_get_broadcast_strength(workspace), 0.9f);
}

TEST_F(GlobalWorkspaceTest, ResolveAllBelowThreshold) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);

    // Submit with strength below default threshold (0.6)
    global_workspace_submit(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.5f);

    cognitive_module_t winner = MODULE_NONE;
    bool resolved = global_workspace_resolve(workspace, &winner);

    // Should fail (no winner above threshold)
    EXPECT_FALSE(resolved);
    EXPECT_EQ(winner, MODULE_NONE);
    EXPECT_FALSE(global_workspace_has_broadcast(workspace));
}

TEST_F(GlobalWorkspaceTest, ResolveClearsPool) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);
    global_workspace_submit(workspace, MODULE_WORKING_MEMORY, content.data(), 256, 0.8f);

    EXPECT_EQ(global_workspace_get_competitor_count(workspace), 1u);

    cognitive_module_t winner = MODULE_NONE;
    global_workspace_resolve(workspace, &winner);

    // Pool should be cleared after resolve
    EXPECT_EQ(global_workspace_get_competitor_count(workspace), 0u);
}

TEST_F(GlobalWorkspaceTest, ResolveRefractoryPeriodBlocks) {
    global_workspace_config_t config = global_workspace_default_config();
    config.refractory_period_ms = 100;
    workspace = CreateCustomWorkspace(&config);
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);

    // First resolve
    global_workspace_submit(workspace, MODULE_WORKING_MEMORY, content1.data(), 256, 0.8f);
    cognitive_module_t winner1 = MODULE_NONE;
    bool resolved1 = global_workspace_resolve(workspace, &winner1);
    EXPECT_TRUE(resolved1);

    // Immediately submit and try to resolve again (should be blocked)
    global_workspace_submit(workspace, MODULE_EXECUTIVE, content2.data(), 256, 0.9f);
    cognitive_module_t winner2 = MODULE_NONE;
    bool resolved2 = global_workspace_resolve(workspace, &winner2);
    EXPECT_FALSE(resolved2);  // Blocked by refractory

    // First broadcast should still be active
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_WORKING_MEMORY);
}

TEST_F(GlobalWorkspaceTest, SubmitResolveIntegration) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content1 = CreateTestContent(256, 1.0f);
    auto content2 = CreateTestContent(256, 2.0f);
    auto content3 = CreateTestContent(256, 3.0f);

    // Batch submit multiple competitors
    global_workspace_submit(workspace, MODULE_WORKING_MEMORY, content1.data(), 256, 0.7f);
    global_workspace_submit(workspace, MODULE_EXECUTIVE, content2.data(), 256, 0.9f);
    global_workspace_submit(workspace, MODULE_SALIENCE, content3.data(), 256, 0.65f);

    // All should be in pool, no broadcast yet
    EXPECT_EQ(global_workspace_get_competitor_count(workspace), 3u);
    EXPECT_FALSE(global_workspace_has_broadcast(workspace));

    // Now resolve - strongest should win
    cognitive_module_t winner = MODULE_NONE;
    bool resolved = global_workspace_resolve(workspace, &winner);

    EXPECT_TRUE(resolved);
    EXPECT_EQ(winner, MODULE_EXECUTIVE);
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_EXECUTIVE);
    EXPECT_FLOAT_EQ(global_workspace_get_broadcast_strength(workspace), 0.9f);
    EXPECT_EQ(global_workspace_get_competitor_count(workspace), 0u);  // Pool cleared
}

TEST_F(GlobalWorkspaceTest, CompeteBackwardCompatibility) {
    workspace = CreateDefaultWorkspace();
    ASSERT_NE(workspace, nullptr);

    auto content = CreateTestContent(256);

    // compete() should still work (backward compatibility)
    bool won = global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                         content.data(), 256, 0.8f);

    EXPECT_TRUE(won);
    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_WORKING_MEMORY);
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
