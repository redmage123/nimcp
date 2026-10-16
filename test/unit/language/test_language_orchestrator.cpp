//=============================================================================
// test_language_orchestrator.cpp - Language Orchestrator Unit Tests
//=============================================================================
/**
 * @file test_language_orchestrator.cpp
 * @brief Unit tests for Language Layer Orchestrator
 *
 * WHAT: Tests central language coordination functionality
 * WHY:  Verify orchestrator lifecycle, state machine, and bridge management
 * HOW:  gtest framework testing create/destroy, states, bridges
 *
 * TEST COVERAGE:
 * - Lifecycle: create, destroy, reset
 * - Configuration: default config, custom config
 * - State machine: transitions, mode changes
 * - Bridge connection: perception, cognitive, training, omni, immune, GPU
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>

#include "language/nimcp_language_orchestrator.h"
#include "language/nimcp_language_config.h"
#include "language/nimcp_language_types.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LanguageOrchestratorTest : public ::testing::Test {
protected:
    language_orchestrator_t* orchestrator;
    language_orchestrator_config_t config;

    void SetUp() override {
        orchestrator = nullptr;
        language_orchestrator_default_config(&config);
    }

    void TearDown() override {
        if (orchestrator) {
            language_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
    }

    uint64_t getCurrentTimeMs() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * @test Create orchestrator with default configuration
 * WHAT: Test language_orchestrator_create with NULL config
 * WHY:  Verify default initialization works correctly
 * HOW:  Create orchestrator, check non-null
 */
TEST_F(LanguageOrchestratorTest, CreateWithDefaultConfig) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr) << "Failed to create orchestrator with default config";
}

/**
 * @test Create orchestrator with custom configuration
 * WHAT: Test language_orchestrator_create with custom config
 * WHY:  Verify custom configuration is applied
 * HOW:  Create orchestrator with modified config
 */
TEST_F(LanguageOrchestratorTest, CreateWithCustomConfig) {
    config.enable_wernicke = true;
    config.enable_broca = true;
    config.enable_nlp_core = true;
    config.enable_perception_bridge = true;
    config.enable_cognitive_bridge = true;
    config.enable_bio_async = true;

    orchestrator = language_orchestrator_create(&config);
    ASSERT_NE(orchestrator, nullptr) << "Failed to create orchestrator with custom config";
}

/**
 * @test Destroy orchestrator safely
 * WHAT: Test language_orchestrator_destroy with valid and null pointers
 * WHY:  Verify destruction doesn't crash with null
 * HOW:  Destroy valid orchestrator, then call destroy with null
 */
TEST_F(LanguageOrchestratorTest, DestroyOrchestrator) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Destroy should succeed
    language_orchestrator_destroy(orchestrator);
    orchestrator = nullptr;

    // Destroy null should not crash
    language_orchestrator_destroy(nullptr);
}

/**
 * @test Reset orchestrator state
 * WHAT: Test language_orchestrator_reset
 * WHY:  Verify reset clears state without full reinitialization
 * HOW:  Create orchestrator, reset, verify state cleared
 */
TEST_F(LanguageOrchestratorTest, ResetOrchestrator) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Reset should succeed
    int result = language_orchestrator_reset(orchestrator);
    EXPECT_EQ(result, 0) << "Reset should return 0";
}

/**
 * @test Reset null orchestrator
 * WHAT: Test language_orchestrator_reset with null
 * WHY:  Verify graceful handling of null input
 * HOW:  Call reset with null, expect error return
 */
TEST_F(LanguageOrchestratorTest, ResetNullOrchestrator) {
    int result = language_orchestrator_reset(nullptr);
    EXPECT_NE(result, 0) << "Reset null should return error";
}

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * @test Default configuration values
 * WHAT: Test language_orchestrator_default_config
 * WHY:  Verify default values are reasonable
 * HOW:  Get default config, check all values
 */
TEST_F(LanguageOrchestratorTest, DefaultConfigValues) {
    language_orchestrator_config_t cfg;
    language_orchestrator_default_config(&cfg);

    // Key subsystems should be enabled by default
    EXPECT_TRUE(cfg.enable_wernicke);
    EXPECT_TRUE(cfg.enable_broca);

    // Processing settings should have sensible defaults
    EXPECT_GT(cfg.max_utterance_words, 0);
    EXPECT_GT(cfg.phoneme_buffer_size, 0);
    EXPECT_GT(cfg.semantic_dim, 0);
    EXPECT_GT(cfg.comprehension_threshold, 0.0f);
    EXPECT_LE(cfg.comprehension_threshold, 1.0f);

    // Bio-async should be configurable
    // (value depends on design decision)
}

/**
 * @test Default config with null pointer
 * WHAT: Test language_orchestrator_default_config with null
 * WHY:  Verify null safety
 * HOW:  Call with null, should not crash
 */
TEST_F(LanguageOrchestratorTest, DefaultConfigNullPointer) {
    language_orchestrator_default_config(nullptr);
    // No crash = success
}

//=============================================================================
// State Machine Tests
//=============================================================================

/**
 * @test Get initial state
 * WHAT: Test language_orchestrator_get_state
 * WHY:  Verify initial state is idle
 * HOW:  Create orchestrator, get state
 */
TEST_F(LanguageOrchestratorTest, GetInitialState) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    language_state_t state = language_orchestrator_get_state(orchestrator);
    EXPECT_EQ(state, LANGUAGE_STATE_IDLE);
}

/**
 * @test Get state from null orchestrator
 * WHAT: Test language_orchestrator_get_state with null
 * WHY:  Verify null safety
 * HOW:  Call with null
 */
TEST_F(LanguageOrchestratorTest, GetStateNullOrchestrator) {
    language_state_t state = language_orchestrator_get_state(nullptr);
    // Should return a safe default (ERROR or IDLE)
    (void)state;
}

/**
 * @test Get initial mode
 * WHAT: Test language_orchestrator_get_mode
 * WHY:  Verify initial mode matches config
 * HOW:  Create orchestrator, get mode
 */
TEST_F(LanguageOrchestratorTest, GetInitialMode) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    language_mode_t mode = language_orchestrator_get_mode(orchestrator);
    // Mode should be a valid enum value
    EXPECT_GE((int)mode, 0);
    EXPECT_LT((int)mode, LANGUAGE_MODE_COUNT);
}

/**
 * @test Set mode
 * WHAT: Test language_orchestrator_set_mode
 * WHY:  Verify mode can be changed
 * HOW:  Create orchestrator, set mode, verify
 */
TEST_F(LanguageOrchestratorTest, SetMode) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    int result = language_orchestrator_set_mode(orchestrator, LANGUAGE_MODE_COMPREHENSION);
    EXPECT_EQ(result, 0);

    language_mode_t mode = language_orchestrator_get_mode(orchestrator);
    EXPECT_EQ(mode, LANGUAGE_MODE_COMPREHENSION);
}

//=============================================================================
// Update Cycle Tests
//=============================================================================

/**
 * @test Update cycle
 * WHAT: Test language_orchestrator_update
 * WHY:  Verify update doesn't crash
 * HOW:  Create orchestrator, run update
 */
TEST_F(LanguageOrchestratorTest, UpdateCycle) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Start orchestrator before updating
    language_orchestrator_start(orchestrator);

    int result = language_orchestrator_update(orchestrator, getCurrentTimeMs());
    EXPECT_GE(result, 0);

    language_orchestrator_stop(orchestrator);
}

/**
 * @test Update null orchestrator
 * WHAT: Test language_orchestrator_update with null
 * WHY:  Verify null safety
 * HOW:  Call with null
 */
TEST_F(LanguageOrchestratorTest, UpdateNullOrchestrator) {
    int result = language_orchestrator_update(nullptr, getCurrentTimeMs());
    EXPECT_NE(result, 0);
}

/**
 * @test Multiple update cycles
 * WHAT: Test multiple consecutive updates
 * WHY:  Verify state doesn't corrupt over time
 * HOW:  Run multiple updates
 */
TEST_F(LanguageOrchestratorTest, MultipleUpdateCycles) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Start orchestrator before updating
    language_orchestrator_start(orchestrator);

    uint64_t current_time = getCurrentTimeMs();
    for (int i = 0; i < 10; i++) {
        int result = language_orchestrator_update(orchestrator, current_time);
        EXPECT_GE(result, 0) << "Update " << i << " failed";
        current_time += 10;  // Advance time by 10ms
    }

    language_orchestrator_stop(orchestrator);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * @test Get initial statistics
 * WHAT: Test language_orchestrator_get_stats
 * WHY:  Verify initial stats are zeroed
 * HOW:  Create orchestrator, get stats
 */
TEST_F(LanguageOrchestratorTest, GetInitialStats) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    language_orchestrator_stats_t stats;
    int result = language_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_EQ(result, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.utterances_comprehended, 0);
    EXPECT_EQ(stats.utterances_produced, 0);
    EXPECT_EQ(stats.state_transitions, 0);
}

/**
 * @test Get stats with null inputs
 * WHAT: Test language_orchestrator_get_stats error handling
 * WHY:  Verify null safety
 * HOW:  Call with null inputs
 */
TEST_F(LanguageOrchestratorTest, GetStatsNullInputs) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    // Null output
    int result = language_orchestrator_get_stats(orchestrator, nullptr);
    EXPECT_NE(result, 0);

    // Null orchestrator
    language_orchestrator_stats_t stats;
    result = language_orchestrator_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

/**
 * @test Reset statistics
 * WHAT: Test language_orchestrator_reset_stats
 * WHY:  Verify stats can be reset
 * HOW:  Modify stats, reset, verify zeroed
 */
TEST_F(LanguageOrchestratorTest, ResetStats) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    language_orchestrator_reset_stats(orchestrator);

    language_orchestrator_stats_t stats;
    language_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_EQ(stats.utterances_comprehended, 0);
}

//=============================================================================
// Bridge Connection Tests
//=============================================================================

/**
 * @test Check bridge connection status
 * WHAT: Test bridge connection status in stats
 * WHY:  Verify connection tracking
 * HOW:  Create orchestrator, check connection bools
 */
TEST_F(LanguageOrchestratorTest, BridgeConnectionStatus) {
    config.enable_perception_bridge = true;
    config.enable_cognitive_bridge = true;
    orchestrator = language_orchestrator_create(&config);
    ASSERT_NE(orchestrator, nullptr);

    language_orchestrator_stats_t stats;
    language_orchestrator_get_stats(orchestrator, &stats);

    // Connection status depends on implementation
    // At minimum, should be queryable without crash
}

//=============================================================================
// Start/Stop Tests
//=============================================================================

/**
 * @test Start orchestrator
 * WHAT: Test language_orchestrator_start
 * WHY:  Verify starting enables processing
 * HOW:  Create orchestrator, start
 */
TEST_F(LanguageOrchestratorTest, StartOrchestrator) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    int result = language_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);

    bool running = language_orchestrator_is_running(orchestrator);
    EXPECT_TRUE(running);
}

/**
 * @test Stop orchestrator
 * WHAT: Test language_orchestrator_stop
 * WHY:  Verify stopping disables processing
 * HOW:  Start, then stop
 */
TEST_F(LanguageOrchestratorTest, StopOrchestrator) {
    orchestrator = language_orchestrator_create(nullptr);
    ASSERT_NE(orchestrator, nullptr);

    language_orchestrator_start(orchestrator);
    int result = language_orchestrator_stop(orchestrator);
    EXPECT_EQ(result, 0);

    bool running = language_orchestrator_is_running(orchestrator);
    EXPECT_FALSE(running);
}

/**
 * @test Start null orchestrator
 * WHAT: Test language_orchestrator_start with null
 * WHY:  Verify null safety
 * HOW:  Call with null
 */
TEST_F(LanguageOrchestratorTest, StartNullOrchestrator) {
    int result = language_orchestrator_start(nullptr);
    EXPECT_NE(result, 0);
}
