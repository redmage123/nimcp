/**
 * @file test_recovery_episodic_memory_regression.cpp
 * @brief Regression tests for Recovery Episodic Memory module
 *
 * WHAT: Prevent regression in critical functionality and performance
 * WHY:  Ensure updates don't break existing behavior
 * HOW:  Test known edge cases, performance benchmarks, compatibility
 *
 * REGRESSION TEST COVERAGE:
 * - Historical bugs that were fixed
 * - Performance benchmarks
 * - API compatibility
 * - Edge cases that caused issues
 * - Memory leak scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-01-09
 * @version 2.7.0 Phase 10.1
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>

// Headers have their own extern "C" guards
    #include "cognitive/fault_tolerance/nimcp_recovery_episodic_memory.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EpisodicMemoryRegressionTest : public ::testing::Test {
protected:
    episodic_memory_t* memory;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        log_init(nullptr);
        memory = nullptr;
    }

    void TearDown() override {
        if (memory) {
            episodic_memory_destroy(memory);
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0);

        log_close();
        nimcp_memory_cleanup();
    }
};

//=============================================================================
// Historical Bug Regression Tests
//=============================================================================

/**
 * @test REGRESSION: Circular Buffer Overflow (v1.0 Bug)
 *
 * WHAT: Verify circular buffer doesn't overflow
 * WHY:  v1.0 had buffer overflow when capacity exactly reached
 * HOW:  Store exactly max_episodes, verify no crash/corruption
 */
TEST_F(EpisodicMemoryRegressionTest, CircularBufferNoOverflow) {
    // ARRANGE: Small capacity for easy testing
    episodic_memory_config_t config = episodic_memory_default_config();
    config.max_episodes = 10;
    memory = episodic_memory_create_custom(&config);

    // ACT: Store exactly 10 episodes (fills buffer completely)
    for (uint64_t i = 1; i <= 10; i++) {
        recovery_episode_t episode = {0};
        episode.episode_id = i;
        episode.timestamp = get_current_timestamp_ms();
        episode.error_sig.error_type = ERROR_TYPE_SIGSEGV;
        episode.error_sig.error_code = i;
        episode.error_sig.signature_hash = compute_error_signature_hash(
            ERROR_TYPE_SIGSEGV, i);
        episode.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
        episode.success = true;
        episode.recovery_time_us = 15000;

        bool result = episodic_memory_store(memory, &episode);
        EXPECT_TRUE(result) << "Failed to store episode " << i;
    }

    // ASSERT: Verify count is exactly 10, no overflow
    EXPECT_EQ(episodic_memory_get_count(memory), 10);

    // Store one more (should evict oldest, not overflow)
    recovery_episode_t episode11 = {0};
    episode11.episode_id = 11;
    episode11.timestamp = get_current_timestamp_ms();
    episode11.error_sig.error_type = ERROR_TYPE_SIGSEGV;
    episode11.error_sig.error_code = 11;
    episode11.error_sig.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 11);
    episode11.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
    episode11.success = true;

    EXPECT_TRUE(episodic_memory_store(memory, &episode11));
    EXPECT_EQ(episodic_memory_get_count(memory), 10)
        << "Circular eviction should maintain capacity";
}

/**
 * @test REGRESSION: LSH Hash Collision (v1.1 Bug)
 *
 * WHAT: Verify LSH handles hash collisions correctly
 * WHY:  v1.1 crashed on hash collisions in LSH tables
 * HOW:  Store episodes with same hash, verify correct retrieval
 */
TEST_F(EpisodicMemoryRegressionTest, LSHHashCollisionHandling) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // Create episodes that might hash to same bucket
    for (uint64_t i = 1; i <= 100; i++) {
        recovery_episode_t episode = {0};
        episode.episode_id = i;
        episode.timestamp = get_current_timestamp_ms();
        episode.error_sig.error_type = ERROR_TYPE_SIGSEGV;
        episode.error_sig.error_code = i;  // Sequential codes
        episode.error_sig.signature_hash = compute_error_signature_hash(
            ERROR_TYPE_SIGSEGV, i);
        episode.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
        episode.success = true;

        episodic_memory_store(memory, &episode);
    }

    // ACT: Query for each episode
    for (uint64_t i = 1; i <= 100; i++) {
        error_signature_t query = {0};
        query.error_type = ERROR_TYPE_SIGSEGV;
        query.error_code = i;
        query.signature_hash = compute_error_signature_hash(
            ERROR_TYPE_SIGSEGV, i);

        uint32_t count = 0;
        recovery_episode_t** similar = episodic_memory_recall_similar(
            memory, &query, 10, &count);

        // ASSERT: Should find results without crash
        if (similar) {
            EXPECT_GT(count, 0) << "Query " << i << " should find matches";
            nimcp_free(similar);
        }
    }
}

/**
 * @test REGRESSION: NULL Pointer Dereference (v1.2 Bug)
 *
 * WHAT: Verify NULL checks prevent crashes
 * WHY:  v1.2 crashed when querying with NULL signature
 * HOW:  Call all APIs with NULL params, expect safe failures
 */
TEST_F(EpisodicMemoryRegressionTest, NullPointerSafetyChecks) {
    // ARRANGE
    memory = episodic_memory_create_default();
    recovery_episode_t episode = {0};
    error_signature_t query = {0};
    uint32_t count = 0;

    // ACT & ASSERT: All NULL checks should prevent crashes

    // NULL memory
    EXPECT_FALSE(episodic_memory_store(nullptr, &episode));
    EXPECT_EQ(episodic_memory_recall_similar(nullptr, &query, 5, &count),
              nullptr);
    EXPECT_EQ(episodic_memory_get_count(nullptr), 0);

    // NULL episode
    EXPECT_FALSE(episodic_memory_store(memory, nullptr));

    // NULL query
    EXPECT_EQ(episodic_memory_recall_similar(memory, nullptr, 5, &count),
              nullptr);

    // NULL count pointer
    EXPECT_EQ(episodic_memory_recall_similar(memory, &query, 5, nullptr),
              nullptr);

    // NULL config
    EXPECT_EQ(episodic_memory_create_custom(nullptr), nullptr);

    // Destroy NULL (should not crash)
    episodic_memory_destroy(nullptr);
}

/**
 * @test REGRESSION: Memory Leak on Destroy (v1.3 Bug)
 *
 * WHAT: Verify all memory freed on destroy
 * WHY:  v1.3 leaked LSH table memory on destroy
 * HOW:  Create, populate, destroy multiple times, check for leaks
 */
TEST_F(EpisodicMemoryRegressionTest, NoMemoryLeakOnDestroy) {
    // ACT: Multiple create/destroy cycles
    for (int cycle = 0; cycle < 50; cycle++) {
        episodic_memory_t* temp = episodic_memory_create_default();
        ASSERT_NE(temp, nullptr);

        // Store episodes
        for (uint64_t i = 1; i <= 20; i++) {
            recovery_episode_t episode = {0};
            episode.episode_id = i;
            episode.timestamp = get_current_timestamp_ms();
            episode.error_sig.error_type = ERROR_TYPE_SIGSEGV;
            episode.error_sig.error_code = i;
            episode.error_sig.signature_hash = compute_error_signature_hash(
                ERROR_TYPE_SIGSEGV, i);
            episode.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
            episode.success = true;

            episodic_memory_store(temp, &episode);
        }

        episodic_memory_destroy(temp);
    }

    // ASSERT: No cumulative memory leak
    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_EQ(stats.current_allocated, 0)
        << "Memory leaked after 50 cycles: " << stats.current_allocated;
}

/**
 * @test REGRESSION: Emotional Tag Range Clamping (v1.4 Bug)
 *
 * WHAT: Verify emotional tags clamped to [-1.0, +1.0]
 * WHY:  v1.4 allowed out-of-range emotions, breaking eviction
 * HOW:  Store episodes with extreme values, verify clamped
 */
TEST_F(EpisodicMemoryRegressionTest, EmotionalTagRangeClamping) {
    // ARRANGE
    episodic_memory_config_t config = episodic_memory_default_config();
    config.enable_emotional_tagging = true;
    memory = episodic_memory_create_custom(&config);

    // ACT: Store episodes with out-of-range emotions
    recovery_episode_t ep1 = {0};
    ep1.episode_id = 1;
    ep1.timestamp = get_current_timestamp_ms();
    ep1.error_sig.error_type = ERROR_TYPE_SIGSEGV;
    ep1.error_sig.error_code = 1;
    ep1.error_sig.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 1);
    ep1.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
    ep1.success = true;
    ep1.emotional_tag = 5.0f;  // Way out of range

    recovery_episode_t ep2 = {0};
    ep2.episode_id = 2;
    ep2.timestamp = get_current_timestamp_ms();
    ep2.error_sig.error_type = ERROR_TYPE_SIGSEGV;
    ep2.error_sig.error_code = 2;
    ep2.error_sig.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 2);
    ep2.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
    ep2.success = true;
    ep2.emotional_tag = -10.0f;  // Way out of range

    episodic_memory_store(memory, &ep1);
    episodic_memory_store(memory, &ep2);

    // ASSERT: Should be clamped to valid range
    uint32_t count = 0;
    recovery_episode_t** episodes = episodic_memory_get_all(memory, &count);

    ASSERT_NE(episodes, nullptr);
    EXPECT_EQ(count, 2);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(episodes[i]->emotional_tag, -1.0f)
            << "Emotion should be >= -1.0";
        EXPECT_LE(episodes[i]->emotional_tag, 1.0f)
            << "Emotion should be <= +1.0";
    }

    nimcp_free(episodes);
}

//=============================================================================
// Performance Benchmark Regression Tests
//=============================================================================

/**
 * @test REGRESSION: Storage Performance Benchmark
 *
 * WHAT: Verify O(1) storage time doesn't regress
 * WHY:  Ensure no performance degradation
 * HOW:  Benchmark 10,000 insertions, compare to baseline
 */
TEST_F(EpisodicMemoryRegressionTest, StoragePerformanceBenchmark) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // ACT: Benchmark storage
    auto start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 1; i <= 10000; i++) {
        recovery_episode_t episode = {0};
        episode.episode_id = i;
        episode.timestamp = get_current_timestamp_ms();
        episode.error_sig.error_type = ERROR_TYPE_SIGSEGV;
        episode.error_sig.error_code = i;
        episode.error_sig.signature_hash = compute_error_signature_hash(
            ERROR_TYPE_SIGSEGV, i);
        episode.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
        episode.success = true;

        episodic_memory_store(memory, &episode);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // ASSERT: Should complete within baseline time
    // Baseline: < 1000ms for 10k insertions (from architecture spec)
    EXPECT_LT(duration_ms, 1000)
        << "Storage regression: 10k insertions took " << duration_ms << "ms";

    // Average per insertion: < 100us
    double avg_us = (duration_ms * 1000.0) / 10000.0;
    EXPECT_LT(avg_us, 100.0)
        << "Average insertion time: " << avg_us << "us";
}

/**
 * @test REGRESSION: LSH Search Performance Benchmark
 *
 * WHAT: Verify O(log N) search doesn't regress
 * WHY:  Ensure search remains fast
 * HOW:  Benchmark searches on 10k episodes
 */
TEST_F(EpisodicMemoryRegressionTest, SearchPerformanceBenchmark) {
    // ARRANGE: Store 10,000 episodes
    memory = episodic_memory_create_default();

    for (uint64_t i = 1; i <= 10000; i++) {
        recovery_episode_t episode = {0};
        episode.episode_id = i;
        episode.timestamp = get_current_timestamp_ms();
        episode.error_sig.error_type = ERROR_TYPE_SIGSEGV;
        episode.error_sig.error_code = i % 100;
        episode.error_sig.signature_hash = compute_error_signature_hash(
            ERROR_TYPE_SIGSEGV, i % 100);
        episode.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
        episode.success = true;

        episodic_memory_store(memory, &episode);
    }

    // ACT: Benchmark 100 searches
    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_SIGSEGV;
    query.error_code = 50;
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 50);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        uint32_t count = 0;
        recovery_episode_t** similar = episodic_memory_recall_similar(
            memory, &query, 10, &count);
        if (similar) nimcp_free(similar);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    // ASSERT: Average search < 1000us (per architecture spec)
    double avg_search_us = duration_us / 100.0;
    EXPECT_LT(avg_search_us, 1000.0)
        << "Search regression: average " << avg_search_us << "us";
}

/**
 * @test REGRESSION: Memory Footprint Benchmark
 *
 * WHAT: Verify memory usage stays within bounds
 * WHY:  Prevent memory bloat
 * HOW:  Store 10k episodes, check total memory
 */
TEST_F(EpisodicMemoryRegressionTest, MemoryFootprintBenchmark) {
    // ARRANGE
    nimcp_memory_clear_stats();
    memory = episodic_memory_create_default();

    // ACT: Store 10,000 episodes
    for (uint64_t i = 1; i <= 10000; i++) {
        recovery_episode_t episode = {0};
        episode.episode_id = i;
        episode.timestamp = get_current_timestamp_ms();
        episode.error_sig.error_type = ERROR_TYPE_SIGSEGV;
        episode.error_sig.error_code = i;
        episode.error_sig.signature_hash = compute_error_signature_hash(
            ERROR_TYPE_SIGSEGV, i);
        episode.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
        episode.success = true;

        episodic_memory_store(memory, &episode);
    }

    // ASSERT: Memory usage within expected bounds
    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);

    // Architecture spec: 16MB for 10k episodes
    // Allow 20MB to account for overhead
    size_t max_expected = 20 * 1024 * 1024;  // 20MB
    EXPECT_LT(stats.current_allocated, max_expected)
        << "Memory footprint regression: " << stats.current_allocated
        << " bytes (expected < " << max_expected << ")";
}

//=============================================================================
// API Compatibility Regression Tests
//=============================================================================

/**
 * @test REGRESSION: Backward Compatible Config
 *
 * WHAT: Verify old configs still work
 * WHY:  Ensure API stability
 * HOW:  Use v1.0 config structure, verify creation
 */
TEST_F(EpisodicMemoryRegressionTest, BackwardCompatibleConfig) {
    // ARRANGE: Simulate old config (minimal fields)
    episodic_memory_config_t old_config = {0};
    old_config.max_episodes = 1000;
    old_config.lsh_num_tables = 4;
    old_config.lsh_num_hashes = 8;

    // ACT
    memory = episodic_memory_create_custom(&old_config);

    // ASSERT: Should create successfully with defaults for missing fields
    ASSERT_NE(memory, nullptr);
    EXPECT_EQ(episodic_memory_get_capacity(memory), 1000);
}

/**
 * @test REGRESSION: Default Config Stability
 *
 * WHAT: Verify default config values haven't changed
 * WHY:  Prevent breaking changes
 * HOW:  Check all default values match specification
 */
TEST_F(EpisodicMemoryRegressionTest, DefaultConfigStability) {
    // ACT
    episodic_memory_config_t config = episodic_memory_default_config();

    // ASSERT: Critical values must not change
    EXPECT_EQ(config.max_episodes, 10000)
        << "Default capacity changed!";

    EXPECT_TRUE(config.enable_emotional_tagging)
        << "Emotional tagging default changed!";

    EXPECT_TRUE(config.enable_consolidation)
        << "Consolidation default changed!";
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

/**
 * @test REGRESSION: Empty Memory Operations
 *
 * WHAT: Verify operations on empty memory don't crash
 * WHY:  v1.0 crashed on empty consolidation
 * HOW:  Call all operations on fresh memory
 */
TEST_F(EpisodicMemoryRegressionTest, EmptyMemoryOperations) {
    // ARRANGE
    memory = episodic_memory_create_default();

    // ACT & ASSERT: All operations should handle empty state
    EXPECT_EQ(episodic_memory_get_count(memory), 0);

    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_SIGSEGV;
    query.error_code = 1;
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 1);

    uint32_t count = 0;
    EXPECT_EQ(episodic_memory_recall_similar(memory, &query, 5, &count),
              nullptr);

    consolidation_result_t cons_result = episodic_memory_consolidate(memory);
    EXPECT_EQ(cons_result.patterns_extracted, 0);

    uint32_t all_count = 0;
    recovery_episode_t** all = episodic_memory_get_all(memory, &all_count);
    if (all) {
        EXPECT_EQ(all_count, 0);
        nimcp_free(all);
    }
}

/**
 * @test REGRESSION: Single Episode Edge Case
 *
 * WHAT: Verify operations with exactly 1 episode
 * WHY:  v1.1 had off-by-one errors
 * HOW:  Store 1 episode, test all operations
 */
TEST_F(EpisodicMemoryRegressionTest, SingleEpisodeEdgeCase) {
    // ARRANGE
    memory = episodic_memory_create_default();

    recovery_episode_t episode = {0};
    episode.episode_id = 1;
    episode.timestamp = get_current_timestamp_ms();
    episode.error_sig.error_type = ERROR_TYPE_SIGSEGV;
    episode.error_sig.error_code = 42;
    episode.error_sig.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 42);
    episode.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
    episode.success = true;

    episodic_memory_store(memory, &episode);

    // ACT & ASSERT
    EXPECT_EQ(episodic_memory_get_count(memory), 1);

    // Search should find the episode
    error_signature_t query = episode.error_sig;
    uint32_t count = 0;
    recovery_episode_t** similar = episodic_memory_recall_similar(
        memory, &query, 5, &count);

    EXPECT_NE(similar, nullptr);
    EXPECT_EQ(count, 1);
    if (similar) nimcp_free(similar);

    // Get all should return 1 episode
    uint32_t all_count = 0;
    recovery_episode_t** all = episodic_memory_get_all(memory, &all_count);
    EXPECT_NE(all, nullptr);
    EXPECT_EQ(all_count, 1);
    if (all) nimcp_free(all);
}

/**
 * @test REGRESSION: Consolidation Threshold Boundary
 *
 * WHAT: Test consolidation at exact threshold
 * WHY:  v1.3 had off-by-one in threshold check
 * HOW:  Store threshold-1, threshold, threshold+1 episodes
 */
TEST_F(EpisodicMemoryRegressionTest, ConsolidationThresholdBoundary) {
    // ARRANGE
    episodic_memory_config_t config = episodic_memory_default_config();
    config.consolidation_threshold = 5;
    config.enable_consolidation = true;
    memory = episodic_memory_create_custom(&config);

    // ACT: Store threshold-1 (4 episodes)
    for (uint64_t i = 1; i <= 4; i++) {
        recovery_episode_t ep = {0};
        ep.episode_id = i;
        ep.timestamp = get_current_timestamp_ms();
        ep.error_sig.error_type = ERROR_TYPE_SIGSEGV;
        ep.error_sig.error_code = 100;
        ep.error_sig.signature_hash = compute_error_signature_hash(
            ERROR_TYPE_SIGSEGV, 100);
        ep.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
        ep.success = true;
        episodic_memory_store(memory, &ep);
    }

    consolidation_result_t result1 = episodic_memory_consolidate(memory);
    EXPECT_EQ(result1.patterns_extracted, 0)
        << "Should not consolidate below threshold";

    // Store one more (reaches threshold)
    recovery_episode_t ep5 = {0};
    ep5.episode_id = 5;
    ep5.timestamp = get_current_timestamp_ms();
    ep5.error_sig.error_type = ERROR_TYPE_SIGSEGV;
    ep5.error_sig.error_code = 100;
    ep5.error_sig.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 100);
    ep5.strategy_type = STRATEGY_RELOAD_CHECKPOINT;
    ep5.success = true;
    episodic_memory_store(memory, &ep5);

    consolidation_result_t result2 = episodic_memory_consolidate(memory);
    EXPECT_GT(result2.patterns_extracted, 0)
        << "Should consolidate at threshold";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
