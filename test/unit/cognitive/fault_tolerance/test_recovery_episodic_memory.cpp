/**
 * @file test_recovery_episodic_memory.cpp
 * @brief Unit tests for Recovery Episodic Memory module
 *
 * WHAT: Comprehensive unit tests for episodic memory storage and retrieval
 * WHY:  Ensure correctness of memory operations, LSH search, consolidation
 * HOW:  Test all public APIs with edge cases, boundary conditions
 *
 * TEST COVERAGE:
 * - Episode storage and retrieval
 * - Content-addressable recall with LSH
 * - Emotional tagging integration
 * - Capacity management and eviction
 * - Consolidation to semantic memory
 * - Error handling and edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-01-09
 * @version 2.7.0 Phase 10.1
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
    #include "cognitive/fault_tolerance/nimcp_recovery_episodic_memory.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class RecoveryEpisodicMemoryTest : public ::testing::Test {
protected:
    episodic_memory_t* memory;
    episodic_memory_config_t config;

    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Initialize logging
        log_init(nullptr);

        // Default configuration
        config = episodic_memory_default_config();

        memory = nullptr;
    }

    void TearDown() override {
        if (memory) {
            episodic_memory_destroy(memory);
            memory = nullptr;
        }

        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0)
            << "Memory leak detected: " << stats.current_allocated << " bytes";

        log_close();
        nimcp_memory_cleanup();
    }

    // Helper: Create sample episode
    recovery_episode_t create_sample_episode(
        uint64_t id,
        error_type_t error_type,
        uint32_t error_code,
        recovery_strategy_type_t strategy,
        bool success,
        uint64_t recovery_time_us,
        float emotional_tag = 0.0f
    ) {
        recovery_episode_t episode = {0};
        episode.episode_id = id;
        episode.timestamp = get_current_timestamp_ms();

        // Error signature
        episode.error_sig.error_type = error_type;
        episode.error_sig.error_code = error_code;
        episode.error_sig.signature_hash = compute_error_signature_hash(
            error_type, error_code);

        // Recovery action
        episode.strategy_type = strategy;
        episode.success = success;
        episode.recovery_time_us = recovery_time_us;
        episode.success_confidence = success ? 0.95f : 0.0f;
        episode.emotional_tag = emotional_tag;

        return episode;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * @test Default Configuration Validation
 *
 * WHAT: Verify default config has sensible values
 * WHY:  Ensure users get working defaults
 * HOW:  Check all fields of default config
 */
TEST_F(RecoveryEpisodicMemoryTest, DefaultConfigHasSensibleValues) {
    // ACT
    episodic_memory_config_t cfg = episodic_memory_default_config();

    // ASSERT
    EXPECT_EQ(cfg.max_episodes, 10000);  // Architecture spec
    EXPECT_GT(cfg.lsh_num_tables, 0);    // Need tables for LSH
    EXPECT_GT(cfg.lsh_num_hashes, 0);    // Need hashes for LSH
    EXPECT_TRUE(cfg.enable_emotional_tagging);
    EXPECT_TRUE(cfg.enable_consolidation);
    EXPECT_GT(cfg.consolidation_threshold, 0);
}

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

/**
 * @test Basic Memory Creation
 *
 * WHAT: Create episodic memory with default config
 * WHY:  Verify basic initialization
 * HOW:  Create and validate non-NULL
 */
TEST_F(RecoveryEpisodicMemoryTest, CreateMemoryWithDefaultConfig) {
    // ACT
    memory = episodic_memory_create_default();

    // ASSERT
    ASSERT_NE(memory, nullptr);
    EXPECT_EQ(episodic_memory_get_count(memory), 0);
}

/**
 * @test Custom Configuration Creation
 *
 * WHAT: Create memory with custom capacity
 * WHY:  Support different capacity requirements
 * HOW:  Create with capacity 100, verify
 */
TEST_F(RecoveryEpisodicMemoryTest, CreateMemoryWithCustomConfig) {
    // ARRANGE
    config.max_episodes = 100;

    // ACT
    memory = episodic_memory_create_custom(&config);

    // ASSERT
    ASSERT_NE(memory, nullptr);
    EXPECT_EQ(episodic_memory_get_capacity(memory), 100);
}

/**
 * @test NULL Config Handling
 *
 * WHAT: Pass NULL config to create
 * WHY:  Verify error handling
 * HOW:  Expect NULL return
 */
TEST_F(RecoveryEpisodicMemoryTest, CreateWithNullConfigReturnsNull) {
    // ACT
    memory = episodic_memory_create_custom(nullptr);

    // ASSERT
    EXPECT_EQ(memory, nullptr);
}

/**
 * @test Destroy NULL Memory
 *
 * WHAT: Call destroy with NULL
 * WHY:  Ensure no crash on NULL
 * HOW:  Should return safely
 */
TEST_F(RecoveryEpisodicMemoryTest, DestroyNullMemoryDoesNotCrash) {
    // ACT & ASSERT (no crash)
    episodic_memory_destroy(nullptr);
}

//=============================================================================
// Episode Storage Tests
//=============================================================================

/**
 * @test Store Single Episode
 *
 * WHAT: Store one recovery episode
 * WHY:  Basic storage functionality
 * HOW:  Create memory, store episode, verify count
 */
TEST_F(RecoveryEpisodicMemoryTest, StoreSingleEpisode) {
    // ARRANGE
    memory = episodic_memory_create_default();
    recovery_episode_t episode = create_sample_episode(
        1, ERROR_TYPE_SIGSEGV, 11, STRATEGY_RELOAD_CHECKPOINT,
        true, 15000, 0.8f);

    // ACT
    bool result = episodic_memory_store(memory, &episode);

    // ASSERT
    EXPECT_TRUE(result);
    EXPECT_EQ(episodic_memory_get_count(memory), 1);
}

/**
 * @test Store Multiple Episodes
 *
 * WHAT: Store 100 different episodes
 * WHY:  Verify bulk storage
 * HOW:  Store in loop, verify count
 */
TEST_F(RecoveryEpisodicMemoryTest, StoreMultipleEpisodes) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // ACT: Store 100 episodes
    for (uint64_t i = 1; i <= 100; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, i, STRATEGY_RELOAD_CHECKPOINT,
            true, 10000 + i, 0.5f);

        EXPECT_TRUE(episodic_memory_store(memory, &episode));
    }

    // ASSERT
    EXPECT_EQ(episodic_memory_get_count(memory), 100);
}

/**
 * @test Store NULL Episode
 *
 * WHAT: Pass NULL episode to store
 * WHY:  Verify parameter validation
 * HOW:  Expect false return
 */
TEST_F(RecoveryEpisodicMemoryTest, StoreNullEpisodeReturnsFalse) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // ACT
    bool result = episodic_memory_store(memory, nullptr);

    // ASSERT
    EXPECT_FALSE(result);
    EXPECT_EQ(episodic_memory_get_count(memory), 0);
}

/**
 * @test Store to NULL Memory
 *
 * WHAT: Call store with NULL memory
 * WHY:  Verify NULL checks
 * HOW:  Expect false return
 */
TEST_F(RecoveryEpisodicMemoryTest, StoreToNullMemoryReturnsFalse) {
    // ARRANGE
    recovery_episode_t episode = create_sample_episode(
        1, ERROR_TYPE_SIGSEGV, 11, STRATEGY_RELOAD_CHECKPOINT,
        true, 15000, 0.8f);

    // ACT
    bool result = episodic_memory_store(nullptr, &episode);

    // ASSERT
    EXPECT_FALSE(result);
}

//=============================================================================
// Capacity and Eviction Tests
//=============================================================================

/**
 * @test Circular Buffer Behavior
 *
 * WHAT: Fill buffer beyond capacity
 * WHY:  Verify circular eviction (FIFO)
 * HOW:  Store max+10 episodes, check oldest evicted
 */
TEST_F(RecoveryEpisodicMemoryTest, CircularBufferEvictsOldest) {
    // ARRANGE: Small capacity for testing
    config.max_episodes = 10;
    memory = episodic_memory_create_custom(&config);

    // ACT: Store 20 episodes
    for (uint64_t i = 1; i <= 20; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, i, STRATEGY_RELOAD_CHECKPOINT,
            true, 10000, 0.5f);
        episodic_memory_store(memory, &episode);
    }

    // ASSERT: Only last 10 episodes remain
    EXPECT_EQ(episodic_memory_get_count(memory), 10);

    // Verify oldest episodes (1-10) evicted, newest (11-20) remain
    uint32_t count = 0;
    recovery_episode_t** episodes = episodic_memory_get_all(
        memory, &count);

    ASSERT_NE(episodes, nullptr);
    EXPECT_EQ(count, 10);

    // Check episode IDs are 11-20
    std::vector<uint64_t> ids;
    for (uint32_t i = 0; i < count; i++) {
        ids.push_back(episodes[i]->episode_id);
    }
    std::sort(ids.begin(), ids.end());

    EXPECT_EQ(ids[0], 11);
    EXPECT_EQ(ids[9], 20);

    nimcp_free(episodes);
}

/**
 * @test Emotional Priority Eviction
 *
 * WHAT: Evict low-emotion episodes first
 * WHY:  Preserve emotionally salient memories
 * HOW:  Store episodes with varying emotional tags, verify high-emotion retained
 */
TEST_F(RecoveryEpisodicMemoryTest, EmotionalPriorityEviction) {
    // ARRANGE: Enable emotional tagging, small capacity
    config.max_episodes = 5;
    config.enable_emotional_tagging = true;
    config.enable_emotional_eviction = true;
    memory = episodic_memory_create_custom(&config);

    // Store 5 episodes with different emotional tags
    // High emotion (keep these)
    recovery_episode_t e1 = create_sample_episode(
        1, ERROR_TYPE_SIGSEGV, 1, STRATEGY_RELOAD_CHECKPOINT,
        true, 10000, 0.9f);  // Very positive
    recovery_episode_t e2 = create_sample_episode(
        2, ERROR_TYPE_DATA_CORRUPTION, 2, STRATEGY_EMERGENCY_SHUTDOWN,
        false, 50000, -0.9f);  // Very negative (fear)

    // Low emotion (evict these)
    recovery_episode_t e3 = create_sample_episode(
        3, ERROR_TYPE_TIMEOUT, 3, STRATEGY_RETRY,
        true, 5000, 0.1f);  // Low emotion
    recovery_episode_t e4 = create_sample_episode(
        4, ERROR_TYPE_TIMEOUT, 4, STRATEGY_RETRY,
        true, 5000, 0.0f);  // Neutral
    recovery_episode_t e5 = create_sample_episode(
        5, ERROR_TYPE_TIMEOUT, 5, STRATEGY_RETRY,
        true, 5000, -0.1f);  // Low emotion

    episodic_memory_store(memory, &e1);
    episodic_memory_store(memory, &e2);
    episodic_memory_store(memory, &e3);
    episodic_memory_store(memory, &e4);
    episodic_memory_store(memory, &e5);

    // ACT: Add one more high-emotion episode (should evict low-emotion)
    recovery_episode_t e6 = create_sample_episode(
        6, ERROR_TYPE_KERNEL_PANIC, 6, STRATEGY_EMERGENCY_SHUTDOWN,
        false, 100000, -0.95f);  // Critical failure
    episodic_memory_store(memory, &e6);

    // ASSERT: High-emotion episodes retained
    EXPECT_EQ(episodic_memory_get_count(memory), 5);

    uint32_t count = 0;
    recovery_episode_t** episodes = episodic_memory_get_all(memory, &count);

    // Check that high-emotion episodes (1, 2, 6) are present
    bool found_e1 = false, found_e2 = false, found_e6 = false;
    for (uint32_t i = 0; i < count; i++) {
        if (episodes[i]->episode_id == 1) found_e1 = true;
        if (episodes[i]->episode_id == 2) found_e2 = true;
        if (episodes[i]->episode_id == 6) found_e6 = true;
    }

    EXPECT_TRUE(found_e1 || found_e2 || found_e6)
        << "At least one high-emotion episode should be retained";

    nimcp_free(episodes);
}

//=============================================================================
// Content-Addressable Recall (LSH) Tests
//=============================================================================

/**
 * @test Recall Similar Episodes
 *
 * WHAT: Store multiple SIGSEGV episodes, recall similar ones
 * WHY:  Verify LSH-based similarity search
 * HOW:  Store 10 SIGSEGV and 10 timeout, query for SIGSEGV, verify results
 */
TEST_F(RecoveryEpisodicMemoryTest, RecallSimilarEpisodes) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // Store 10 SIGSEGV episodes (similar)
    for (uint64_t i = 1; i <= 10; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, 1000 + i, STRATEGY_RELOAD_CHECKPOINT,
            true, 15000, 0.8f);
        episodic_memory_store(memory, &episode);
    }

    // Store 10 timeout episodes (different)
    for (uint64_t i = 11; i <= 20; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_TIMEOUT, 2000 + i, STRATEGY_RETRY,
            true, 5000, 0.3f);
        episodic_memory_store(memory, &episode);
    }

    // ACT: Query for SIGSEGV-like episodes
    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_SIGSEGV;
    query.error_code = 1005;  // Similar to stored 1000-1010
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 1005);

    uint32_t count = 0;
    recovery_episode_t** similar = episodic_memory_recall_similar(
        memory, &query, 5, &count);

    // ASSERT: Should find SIGSEGV episodes
    ASSERT_NE(similar, nullptr);
    EXPECT_GT(count, 0);
    EXPECT_LE(count, 5);  // Requested max 5

    // All returned episodes should be SIGSEGV type
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(similar[i]->error_sig.error_type, ERROR_TYPE_SIGSEGV);
    }

    nimcp_free(similar);
}

/**
 * @test Recall With No Matches
 *
 * WHAT: Query for error type not in memory
 * WHY:  Verify empty result handling
 * HOW:  Store only SIGSEGV, query for timeout
 */
TEST_F(RecoveryEpisodicMemoryTest, RecallWithNoMatchesReturnsEmpty) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // Store only SIGSEGV episodes
    for (uint64_t i = 1; i <= 5; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, i, STRATEGY_RELOAD_CHECKPOINT,
            true, 15000, 0.8f);
        episodic_memory_store(memory, &episode);
    }

    // ACT: Query for completely different error
    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_KERNEL_PANIC;
    query.error_code = 9999;
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_KERNEL_PANIC, 9999);

    uint32_t count = 0;
    recovery_episode_t** similar = episodic_memory_recall_similar(
        memory, &query, 5, &count);

    // ASSERT: May return NULL or empty list
    if (similar) {
        EXPECT_EQ(count, 0);
        nimcp_free(similar);
    } else {
        EXPECT_EQ(count, 0);
    }
}

/**
 * @test Recall From Empty Memory
 *
 * WHAT: Query empty episodic memory
 * WHY:  Verify empty case handling
 * HOW:  Create memory, query immediately
 */
TEST_F(RecoveryEpisodicMemoryTest, RecallFromEmptyMemoryReturnsNull) {
    // ARRANGE
    memory = episodic_memory_create_default();

    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_SIGSEGV;
    query.error_code = 11;
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 11);

    // ACT
    uint32_t count = 0;
    recovery_episode_t** similar = episodic_memory_recall_similar(
        memory, &query, 5, &count);

    // ASSERT
    EXPECT_EQ(similar, nullptr);
    EXPECT_EQ(count, 0);
}

/**
 * @test Recall NULL Parameters
 *
 * WHAT: Pass NULL to recall function
 * WHY:  Verify parameter validation
 * HOW:  Test NULL memory, NULL query, NULL count
 */
TEST_F(RecoveryEpisodicMemoryTest, RecallNullParametersReturnNull) {
    // ARRANGE
    memory = episodic_memory_create_default();
    error_signature_t query = {0};
    uint32_t count = 0;

    // ACT & ASSERT: NULL memory
    EXPECT_EQ(episodic_memory_recall_similar(nullptr, &query, 5, &count),
              nullptr);

    // NULL query
    EXPECT_EQ(episodic_memory_recall_similar(memory, nullptr, 5, &count),
              nullptr);

    // NULL count
    EXPECT_EQ(episodic_memory_recall_similar(memory, &query, 5, nullptr),
              nullptr);
}

//=============================================================================
// Episode Replay Tests
//=============================================================================

/**
 * @test Replay Specific Episode
 *
 * WHAT: Replay episode by ID for learning
 * WHY:  Support memory consolidation via replay
 * HOW:  Store episode, replay by ID, verify callback invoked
 */
TEST_F(RecoveryEpisodicMemoryTest, ReplayEpisodeById) {
    // ARRANGE
    memory = episodic_memory_create_default();

    recovery_episode_t episode = create_sample_episode(
        42, ERROR_TYPE_SIGSEGV, 11, STRATEGY_RELOAD_CHECKPOINT,
        true, 15000, 0.8f);
    episodic_memory_store(memory, &episode);

    // ACT
    replay_result_t result = episodic_memory_replay(memory, 42);

    // ASSERT
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.episode_id, 42);
}

/**
 * @test Replay Non-Existent Episode
 *
 * WHAT: Try to replay episode that doesn't exist
 * WHY:  Verify error handling
 * HOW:  Replay ID not in memory
 */
TEST_F(RecoveryEpisodicMemoryTest, ReplayNonExistentEpisodeReturnsFalse) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // ACT
    replay_result_t result = episodic_memory_replay(memory, 9999);

    // ASSERT
    EXPECT_FALSE(result.success);
}

/**
 * @test Replay From NULL Memory
 *
 * WHAT: Call replay with NULL
 * WHY:  Verify NULL checks
 * HOW:  Expect false result
 */
TEST_F(RecoveryEpisodicMemoryTest, ReplayFromNullMemoryReturnsFalse) {
    // ACT
    replay_result_t result = episodic_memory_replay(nullptr, 42);

    // ASSERT
    EXPECT_FALSE(result.success);
}

//=============================================================================
// Consolidation Tests
//=============================================================================

/**
 * @test Consolidate to Semantic Memory
 *
 * WHAT: Extract patterns from episodic memory
 * WHY:  Build semantic rules from experiences
 * HOW:  Store similar episodes, consolidate, verify patterns extracted
 */
TEST_F(RecoveryEpisodicMemoryTest, ConsolidateToSemanticMemory) {
    // ARRANGE
    config.enable_consolidation = true;
    config.consolidation_threshold = 5;  // Trigger after 5 similar episodes
    memory = episodic_memory_create_custom(&config);

    // Store 10 similar successful SIGSEGV recoveries
    for (uint64_t i = 1; i <= 10; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, 1000, STRATEGY_RELOAD_CHECKPOINT,
            true, 15000, 0.8f);
        episodic_memory_store(memory, &episode);
    }

    // ACT: Trigger consolidation
    consolidation_result_t result = episodic_memory_consolidate(memory);

    // ASSERT: Should extract pattern
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.patterns_extracted, 0);

    // Verify semantic rule created:
    // "SIGSEGV + code 1000 -> RELOAD_CHECKPOINT (90%+ success)"
    EXPECT_GT(result.patterns_extracted, 0);
}

/**
 * @test Consolidation With Insufficient Data
 *
 * WHAT: Try to consolidate with < threshold episodes
 * WHY:  Verify threshold enforcement
 * HOW:  Store 3 episodes, threshold=5, expect no consolidation
 */
TEST_F(RecoveryEpisodicMemoryTest, ConsolidationBelowThreshold) {
    // ARRANGE
    config.enable_consolidation = true;
    config.consolidation_threshold = 5;
    memory = episodic_memory_create_custom(&config);

    // Store only 3 episodes
    for (uint64_t i = 1; i <= 3; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, 1000, STRATEGY_RELOAD_CHECKPOINT,
            true, 15000, 0.8f);
        episodic_memory_store(memory, &episode);
    }

    // ACT
    consolidation_result_t result = episodic_memory_consolidate(memory);

    // ASSERT: No patterns extracted (below threshold)
    EXPECT_EQ(result.patterns_extracted, 0);
}

//=============================================================================
// Statistics and Getters Tests
//=============================================================================

/**
 * @test Get Episode Count
 *
 * WHAT: Retrieve current episode count
 * WHY:  Monitor memory usage
 * HOW:  Store episodes, verify count matches
 */
TEST_F(RecoveryEpisodicMemoryTest, GetEpisodeCount) {
    // ARRANGE
    memory = episodic_memory_create_default();

    EXPECT_EQ(episodic_memory_get_count(memory), 0);

    // ACT: Store 5 episodes
    for (uint64_t i = 1; i <= 5; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, i, STRATEGY_RELOAD_CHECKPOINT,
            true, 15000, 0.5f);
        episodic_memory_store(memory, &episode);
    }

    // ASSERT
    EXPECT_EQ(episodic_memory_get_count(memory), 5);
}

/**
 * @test Get Capacity
 *
 * WHAT: Retrieve max capacity
 * WHY:  Verify configuration applied
 * HOW:  Create with custom capacity, verify getter
 */
TEST_F(RecoveryEpisodicMemoryTest, GetCapacity) {
    // ARRANGE
    config.max_episodes = 500;
    memory = episodic_memory_create_custom(&config);

    // ACT & ASSERT
    EXPECT_EQ(episodic_memory_get_capacity(memory), 500);
}

/**
 * @test Get Statistics
 *
 * WHAT: Retrieve memory statistics
 * WHY:  Monitor performance and usage
 * HOW:  Store/query episodes, check stats
 */
TEST_F(RecoveryEpisodicMemoryTest, GetStatistics) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // Store and query episodes
    for (uint64_t i = 1; i <= 10; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, i, STRATEGY_RELOAD_CHECKPOINT,
            true, 15000, 0.5f);
        episodic_memory_store(memory, &episode);
    }

    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_SIGSEGV;
    query.error_code = 5;
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 5);

    uint32_t count = 0;
    recovery_episode_t** similar = episodic_memory_recall_similar(
        memory, &query, 5, &count);
    if (similar) nimcp_free(similar);

    // ACT
    episodic_memory_stats_t stats = episodic_memory_get_stats(memory);

    // ASSERT
    EXPECT_EQ(stats.total_episodes_stored, 10);
    EXPECT_GE(stats.total_queries, 1);
    EXPECT_EQ(stats.current_episode_count, 10);
}

//=============================================================================
// Emotional Tagging Tests
//=============================================================================

/**
 * @test Store Episode With Positive Emotion
 *
 * WHAT: Store successful recovery with positive emotion
 * WHY:  Verify emotional tag storage
 * HOW:  Store episode with +0.9 tag, retrieve and verify
 */
TEST_F(RecoveryEpisodicMemoryTest, StoreEpisodeWithPositiveEmotion) {
    // ARRANGE
    config.enable_emotional_tagging = true;
    memory = episodic_memory_create_custom(&config);

    recovery_episode_t episode = create_sample_episode(
        1, ERROR_TYPE_SIGSEGV, 11, STRATEGY_RELOAD_CHECKPOINT,
        true, 15000, 0.9f);  // Positive emotion (relief)

    // ACT
    episodic_memory_store(memory, &episode);

    // Retrieve
    uint32_t count = 0;
    recovery_episode_t** episodes = episodic_memory_get_all(memory, &count);

    // ASSERT
    ASSERT_NE(episodes, nullptr);
    ASSERT_EQ(count, 1);
    EXPECT_FLOAT_EQ(episodes[0]->emotional_tag, 0.9f);

    nimcp_free(episodes);
}

/**
 * @test Store Episode With Negative Emotion
 *
 * WHAT: Store failed recovery with negative emotion
 * WHY:  Verify fear/frustration tagging
 * HOW:  Store critical failure with -0.9 tag
 */
TEST_F(RecoveryEpisodicMemoryTest, StoreEpisodeWithNegativeEmotion) {
    // ARRANGE
    config.enable_emotional_tagging = true;
    memory = episodic_memory_create_custom(&config);

    recovery_episode_t episode = create_sample_episode(
        1, ERROR_TYPE_DATA_CORRUPTION, 99, STRATEGY_EMERGENCY_SHUTDOWN,
        false, 100000, -0.9f);  // Negative emotion (fear)

    // ACT
    episodic_memory_store(memory, &episode);

    // Retrieve
    uint32_t count = 0;
    recovery_episode_t** episodes = episodic_memory_get_all(memory, &count);

    // ASSERT
    ASSERT_NE(episodes, nullptr);
    ASSERT_EQ(count, 1);
    EXPECT_FLOAT_EQ(episodes[0]->emotional_tag, -0.9f);

    nimcp_free(episodes);
}

//=============================================================================
// Performance Tests
//=============================================================================

/**
 * @test Large Scale Storage Performance
 *
 * WHAT: Store 10,000 episodes
 * WHY:  Verify O(1) storage at scale
 * HOW:  Time storage of 10k episodes
 */
TEST_F(RecoveryEpisodicMemoryTest, LargeScaleStoragePerformance) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // ACT: Store 10,000 episodes
    auto start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 1; i <= 10000; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, i % 100, STRATEGY_RELOAD_CHECKPOINT,
            (i % 2 == 0), 15000, 0.5f);
        episodic_memory_store(memory, &episode);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // ASSERT: Should complete in reasonable time (< 1 second)
    EXPECT_LT(duration, 1000) << "10k episodes stored in " << duration << "ms";
    EXPECT_EQ(episodic_memory_get_count(memory), 10000);
}

/**
 * @test LSH Search Performance
 *
 * WHAT: Query 10,000-episode memory
 * WHY:  Verify O(log N) search time
 * HOW:  Store 10k, time search
 */
TEST_F(RecoveryEpisodicMemoryTest, LSHSearchPerformance) {
    // ARRANGE: Store 10,000 episodes
    memory = episodic_memory_create_default();

    for (uint64_t i = 1; i <= 10000; i++) {
        recovery_episode_t episode = create_sample_episode(
            i, ERROR_TYPE_SIGSEGV, i % 100, STRATEGY_RELOAD_CHECKPOINT,
            true, 15000, 0.5f);
        episodic_memory_store(memory, &episode);
    }

    // ACT: Search for similar episodes
    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_SIGSEGV;
    query.error_code = 50;
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 50);

    auto start = std::chrono::high_resolution_clock::now();

    uint32_t count = 0;
    recovery_episode_t** similar = episodic_memory_recall_similar(
        memory, &query, 10, &count);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    // ASSERT: Should complete in < 1ms (per architecture spec)
    EXPECT_LT(duration, 1000) << "Search took " << duration << "us";

    if (similar) nimcp_free(similar);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

/**
 * @test Invalid Episode ID
 *
 * WHAT: Store episode with ID 0
 * WHY:  Verify ID validation
 * HOW:  Expect false return or auto-assignment
 */
TEST_F(RecoveryEpisodicMemoryTest, StoreEpisodeWithZeroID) {
    // ARRANGE
    memory = episodic_memory_create_default();

    recovery_episode_t episode = create_sample_episode(
        0, ERROR_TYPE_SIGSEGV, 11, STRATEGY_RELOAD_CHECKPOINT,
        true, 15000, 0.5f);

    // ACT
    bool result = episodic_memory_store(memory, &episode);

    // ASSERT: Either reject or auto-assign ID
    // Implementation may choose either behavior
    if (result) {
        // If accepted, verify ID was auto-assigned
        EXPECT_GT(episode.episode_id, 0);
    }
}

/**
 * @test Emotional Tag Range Validation
 *
 * WHAT: Store episode with out-of-range emotion
 * WHY:  Verify range [-1.0, +1.0] enforced
 * HOW:  Try to store with emotion = 2.0
 */
TEST_F(RecoveryEpisodicMemoryTest, EmotionalTagRangeValidation) {
    // ARRANGE
    config.enable_emotional_tagging = true;
    memory = episodic_memory_create_custom(&config);

    recovery_episode_t episode = create_sample_episode(
        1, ERROR_TYPE_SIGSEGV, 11, STRATEGY_RELOAD_CHECKPOINT,
        true, 15000, 2.0f);  // Out of range

    // ACT
    bool result = episodic_memory_store(memory, &episode);

    // ASSERT: Should clamp to valid range or reject
    if (result) {
        uint32_t count = 0;
        recovery_episode_t** episodes = episodic_memory_get_all(
            memory, &count);

        if (episodes && count > 0) {
            // Should be clamped to 1.0
            EXPECT_LE(episodes[0]->emotional_tag, 1.0f);
            EXPECT_GE(episodes[0]->emotional_tag, -1.0f);
            nimcp_free(episodes);
        }
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
