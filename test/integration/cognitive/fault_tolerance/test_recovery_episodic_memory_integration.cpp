/**
 * @file test_recovery_episodic_memory_integration.cpp
 * @brief Integration tests for Recovery Episodic Memory with brain/cognitive systems
 *
 * WHAT: Test episodic memory integration with executive functions, attention, consolidation
 * WHY:  Ensure proper interaction with cognitive architecture
 * HOW:  Test realistic recovery scenarios with full cognitive pipeline
 *
 * INTEGRATION POINTS:
 * - Executive function recovery planning
 * - Attention mechanism for prioritization
 * - Working memory coordination
 * - Semantic memory consolidation
 * - Emotional tagging system
 *
 * @author NIMCP Development Team
 * @date 2025-01-09
 * @version 2.7.0 Phase 10.1
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

// Headers have their own extern "C" guards
    #include "cognitive/fault_tolerance/nimcp_recovery_episodic_memory.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EpisodicMemoryIntegrationTest : public ::testing::Test {
protected:
    episodic_memory_t* memory;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        log_init(nullptr);

        memory = episodic_memory_create_default();
        ASSERT_NE(memory, nullptr);
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

    // Helper: Create recovery scenario
    recovery_episode_t create_recovery_scenario(
        uint64_t id,
        error_type_t error,
        uint32_t code,
        recovery_strategy_type_t strategy,
        bool success,
        uint64_t recovery_time_us,
        float emotion
    ) {
        recovery_episode_t episode = {0};
        episode.episode_id = id;
        episode.timestamp = get_current_timestamp_ms();
        episode.error_sig.error_type = error;
        episode.error_sig.error_code = code;
        episode.error_sig.signature_hash = compute_error_signature_hash(error, code);
        episode.strategy_type = strategy;
        episode.success = success;
        episode.recovery_time_us = recovery_time_us;
        episode.success_confidence = success ? 0.95f : 0.1f;
        episode.emotional_tag = emotion;
        return episode;
    }
};

//=============================================================================
// Realistic Recovery Scenario Tests
//=============================================================================

/**
 * @test End-to-End Recovery Learning
 *
 * WHAT: Simulate multiple recovery attempts, learn optimal strategy
 * WHY:  Verify learning from experience
 * HOW:  Store failures → success, query similar, expect learned strategy
 */
TEST_F(EpisodicMemoryIntegrationTest, EndToEndRecoveryLearning) {
    // ARRANGE: Simulate learning curve for SIGSEGV recovery
    // Attempt 1-3: Different strategies fail
    episodic_memory_store(memory, &create_recovery_scenario(
        1, ERROR_TYPE_SIGSEGV, 0x1234, STRATEGY_RETRY,
        false, 50000, -0.5f));  // Failure

    episodic_memory_store(memory, &create_recovery_scenario(
        2, ERROR_TYPE_SIGSEGV, 0x1234, STRATEGY_REDUCE_LOAD,
        false, 60000, -0.7f));  // Failure

    episodic_memory_store(memory, &create_recovery_scenario(
        3, ERROR_TYPE_SIGSEGV, 0x1234, STRATEGY_CPU_FALLBACK,
        false, 70000, -0.8f));  // Failure

    // Attempt 4-10: Reload checkpoint works!
    for (uint64_t i = 4; i <= 10; i++) {
        episodic_memory_store(memory, &create_recovery_scenario(
            i, ERROR_TYPE_SIGSEGV, 0x1234, STRATEGY_RELOAD_CHECKPOINT,
            true, 15000, 0.8f));  // Success!
    }

    // ACT: New similar error occurs, query for strategy
    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_SIGSEGV;
    query.error_code = 0x1235;  // Similar address
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 0x1235);

    uint32_t count = 0;
    recovery_episode_t** similar = episodic_memory_recall_similar(
        memory, &query, 10, &count);

    // ASSERT: Should recommend RELOAD_CHECKPOINT (learned strategy)
    ASSERT_NE(similar, nullptr);
    EXPECT_GT(count, 0);

    // Count strategies in results
    uint32_t reload_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (similar[i]->strategy_type == STRATEGY_RELOAD_CHECKPOINT &&
            similar[i]->success) {
            reload_count++;
        }
    }

    // Majority should be successful reload checkpoint
    EXPECT_GT(reload_count, count / 2)
        << "Learned strategy should dominate similar episodes";

    nimcp_free(similar);
}

/**
 * @test Multi-Step Recovery Planning Integration
 *
 * WHAT: Store complex multi-step recovery episodes
 * WHY:  Verify support for executive function planning
 * HOW:  Store episodes with multiple recovery steps, recall
 */
TEST_F(EpisodicMemoryIntegrationTest, MultiStepRecoveryPlanning) {
    // ARRANGE: Complex recovery with multiple steps
    // Step 1: Save checkpoint
    // Step 2: Analyze error
    // Step 3: Reload checkpoint
    // Step 4: Reduce learning rate
    // Step 5: Resume

    recovery_episode_t episode = create_recovery_scenario(
        1, ERROR_TYPE_GRADIENT_EXPLOSION, 777,
        STRATEGY_MULTI_STEP, true, 250000, 0.6f);

    // Store multi-step plan in episode
    episode.num_recovery_steps = 5;

    // ACT
    bool stored = episodic_memory_store(memory, &episode);

    // ASSERT
    EXPECT_TRUE(stored);

    // Retrieve and verify
    uint32_t count = 0;
    recovery_episode_t** episodes = episodic_memory_get_all(memory, &count);

    ASSERT_NE(episodes, nullptr);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(episodes[0]->num_recovery_steps, 5);

    nimcp_free(episodes);
}

/**
 * @test Cascading Failure Detection
 *
 * WHAT: Store rapid sequence of failures
 * WHY:  Support working memory cascade detection
 * HOW:  Store 10 failures within 1 minute, verify pattern
 */
TEST_F(EpisodicMemoryIntegrationTest, CascadingFailureDetection) {
    // ARRANGE: Simulate cascade - 10 failures in 1 second
    uint64_t base_time = get_current_timestamp_ms();

    for (uint64_t i = 1; i <= 10; i++) {
        recovery_episode_t episode = create_recovery_scenario(
            i, ERROR_TYPE_TIMEOUT, 8080 + i,
            STRATEGY_RETRY, false, 1000, -0.3f);

        // Override timestamp to simulate rapid failures
        episode.timestamp = base_time + (i * 100);  // 100ms apart

        episodic_memory_store(memory, &episode);
    }

    // ACT: Analyze temporal pattern
    episodic_memory_stats_t stats = episodic_memory_get_stats(memory);

    // ASSERT: Can detect temporal clustering
    EXPECT_EQ(stats.total_episodes_stored, 10);

    // Get all episodes sorted by time
    uint32_t count = 0;
    recovery_episode_t** episodes = episodic_memory_get_all(memory, &count);

    ASSERT_NE(episodes, nullptr);
    EXPECT_EQ(count, 10);

    // Verify temporal pattern (should be within 1 second)
    uint64_t time_span = episodes[9]->timestamp - episodes[0]->timestamp;
    EXPECT_LT(time_span, 1000)  // Within 1 second
        << "Cascade pattern detected: " << time_span << "ms span";

    nimcp_free(episodes);
}

/**
 * @test Emotional Prioritization Integration
 *
 * WHAT: Verify high-emotion episodes get priority treatment
 * WHY:  Integration with attention mechanism
 * HOW:  Store mixed-emotion episodes, verify retrieval priority
 */
TEST_F(EpisodicMemoryIntegrationTest, EmotionalPrioritization) {
    // ARRANGE: Store episodes with varying emotional intensity
    // Critical failures (high emotion)
    episodic_memory_store(memory, &create_recovery_scenario(
        1, ERROR_TYPE_DATA_CORRUPTION, 999,
        STRATEGY_EMERGENCY_SHUTDOWN, false, 100000, -0.95f));  // FEAR

    episodic_memory_store(memory, &create_recovery_scenario(
        2, ERROR_TYPE_KERNEL_PANIC, 888,
        STRATEGY_EMERGENCY_SHUTDOWN, false, 150000, -0.9f));  // FEAR

    // Minor issues (low emotion)
    for (uint64_t i = 3; i <= 10; i++) {
        episodic_memory_store(memory, &create_recovery_scenario(
            i, ERROR_TYPE_TIMEOUT, i,
            STRATEGY_RETRY, true, 5000, 0.1f));  // Neutral
    }

    // ACT: Query for critical-type errors
    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_DATA_CORRUPTION;
    query.error_code = 998;
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_DATA_CORRUPTION, 998);

    uint32_t count = 0;
    recovery_episode_t** similar = episodic_memory_recall_similar(
        memory, &query, 5, &count);

    // ASSERT: High-emotion episode should be in results
    bool found_high_emotion = false;
    if (similar) {
        for (uint32_t i = 0; i < count; i++) {
            if (std::abs(similar[i]->emotional_tag) > 0.8f) {
                found_high_emotion = true;
                break;
            }
        }
        nimcp_free(similar);
    }

    EXPECT_TRUE(found_high_emotion)
        << "High-emotion episodes should be recalled for similar queries";
}

/**
 * @test Consolidation Integration
 *
 * WHAT: Trigger consolidation after threshold reached
 * WHY:  Verify episodic → semantic transfer
 * HOW:  Store repeated pattern, consolidate, verify rule extracted
 */
TEST_F(EpisodicMemoryIntegrationTest, ConsolidationToSemanticMemory) {
    // ARRANGE: Store 20 identical successful recoveries
    for (uint64_t i = 1; i <= 20; i++) {
        episodic_memory_store(memory, &create_recovery_scenario(
            i, ERROR_TYPE_SIGSEGV, 0x5000,
            STRATEGY_RELOAD_CHECKPOINT, true, 15000, 0.8f));
    }

    // ACT: Trigger consolidation
    consolidation_result_t result = episodic_memory_consolidate(memory);

    // ASSERT: Pattern should be extracted
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.patterns_extracted, 0);

    // Verify semantic rule:
    // "SIGSEGV at 0x5000 → RELOAD_CHECKPOINT (100% success, N=20)"
    EXPECT_GE(result.confidence, 0.9f)
        << "High confidence due to consistent pattern";
}

/**
 * @test Performance Under Load
 *
 * WHAT: Simulate high-frequency fault scenario
 * WHY:  Verify performance in production conditions
 * HOW:  Store 1000 episodes rapidly, query frequently
 */
TEST_F(EpisodicMemoryIntegrationTest, PerformanceUnderLoad) {
    // ARRANGE: Prepare query
    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_SIGSEGV;
    query.error_code = 500;
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 500);

    auto start_time = std::chrono::high_resolution_clock::now();

    // ACT: Interleave storage and queries
    for (uint64_t i = 1; i <= 1000; i++) {
        // Store episode
        episodic_memory_store(memory, &create_recovery_scenario(
            i, ERROR_TYPE_SIGSEGV, i % 100,
            STRATEGY_RELOAD_CHECKPOINT, true, 15000, 0.5f));

        // Query every 10 episodes
        if (i % 10 == 0) {
            uint32_t count = 0;
            recovery_episode_t** similar = episodic_memory_recall_similar(
                memory, &query, 5, &count);
            if (similar) nimcp_free(similar);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    // ASSERT: Should handle load efficiently (< 2 seconds for 1000 ops)
    EXPECT_LT(duration, 2000)
        << "1000 storage + 100 queries took " << duration << "ms";
}

/**
 * @test Memory Leak Prevention
 *
 * WHAT: Verify no leaks in create/store/query/destroy cycle
 * WHY:  Ensure production reliability
 * HOW:  Repeated cycles with leak detection
 */
TEST_F(EpisodicMemoryIntegrationTest, MemoryLeakPrevention) {
    // ACT: Multiple create/destroy cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        episodic_memory_t* temp_mem = episodic_memory_create_default();
        ASSERT_NE(temp_mem, nullptr);

        // Store episodes
        for (uint64_t i = 1; i <= 100; i++) {
            episodic_memory_store(temp_mem, &create_recovery_scenario(
                i, ERROR_TYPE_SIGSEGV, i,
                STRATEGY_RELOAD_CHECKPOINT, true, 15000, 0.5f));
        }

        // Query
        error_signature_t query = {0};
        query.error_type = ERROR_TYPE_SIGSEGV;
        query.error_code = 50;
        query.signature_hash = compute_error_signature_hash(
            ERROR_TYPE_SIGSEGV, 50);

        uint32_t count = 0;
        recovery_episode_t** similar = episodic_memory_recall_similar(
            temp_mem, &query, 5, &count);
        if (similar) nimcp_free(similar);

        // Destroy
        episodic_memory_destroy(temp_mem);
    }

    // ASSERT: No cumulative leaks
    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    EXPECT_EQ(stats.current_allocated, 0)
        << "Memory leak after 10 cycles: " << stats.current_allocated;
}

/**
 * @test Strategy Success Rate Tracking
 *
 * WHAT: Track success rates for different strategies
 * WHY:  Support decision-making in executive function
 * HOW:  Store mixed success/failure, analyze results
 */
TEST_F(EpisodicMemoryIntegrationTest, StrategySuccessRateTracking) {
    // ARRANGE: Store episodes with different strategies and outcomes
    // RELOAD_CHECKPOINT: 8/10 success (80%)
    for (uint64_t i = 1; i <= 10; i++) {
        episodic_memory_store(memory, &create_recovery_scenario(
            i, ERROR_TYPE_SIGSEGV, 0x1000,
            STRATEGY_RELOAD_CHECKPOINT, (i <= 8), 15000, 0.6f));
    }

    // RETRY: 3/10 success (30%)
    for (uint64_t i = 11; i <= 20; i++) {
        episodic_memory_store(memory, &create_recovery_scenario(
            i, ERROR_TYPE_SIGSEGV, 0x1000,
            STRATEGY_RETRY, (i <= 13), 5000, 0.2f));
    }

    // ACT: Query for SIGSEGV at 0x1000
    error_signature_t query = {0};
    query.error_type = ERROR_TYPE_SIGSEGV;
    query.error_code = 0x1000;
    query.signature_hash = compute_error_signature_hash(
        ERROR_TYPE_SIGSEGV, 0x1000);

    uint32_t count = 0;
    recovery_episode_t** similar = episodic_memory_recall_similar(
        memory, &query, 20, &count);

    // ASSERT: Analyze success rates
    ASSERT_NE(similar, nullptr);

    uint32_t checkpoint_success = 0, checkpoint_total = 0;
    uint32_t retry_success = 0, retry_total = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (similar[i]->strategy_type == STRATEGY_RELOAD_CHECKPOINT) {
            checkpoint_total++;
            if (similar[i]->success) checkpoint_success++;
        } else if (similar[i]->strategy_type == STRATEGY_RETRY) {
            retry_total++;
            if (similar[i]->success) retry_success++;
        }
    }

    // Verify RELOAD_CHECKPOINT has higher success rate
    float checkpoint_rate = checkpoint_total > 0 ?
        (float)checkpoint_success / checkpoint_total : 0.0f;
    float retry_rate = retry_total > 0 ?
        (float)retry_success / retry_total : 0.0f;

    EXPECT_GT(checkpoint_rate, retry_rate)
        << "RELOAD_CHECKPOINT (" << checkpoint_rate * 100
        << "%) should outperform RETRY (" << retry_rate * 100 << "%)";

    nimcp_free(similar);
}

/**
 * @test Temporal Clustering Analysis
 *
 * WHAT: Identify failures clustered in time
 * WHY:  Detect systemic issues vs random failures
 * HOW:  Store episodes with temporal patterns, analyze
 */
TEST_F(EpisodicMemoryIntegrationTest, TemporalClusteringAnalysis) {
    // ARRANGE: Create two clusters
    uint64_t base_time = get_current_timestamp_ms();

    // Cluster 1: 5 failures at T+0 to T+500ms
    for (uint64_t i = 1; i <= 5; i++) {
        recovery_episode_t ep = create_recovery_scenario(
            i, ERROR_TYPE_TIMEOUT, 8080,
            STRATEGY_RETRY, false, 1000, -0.4f);
        ep.timestamp = base_time + (i * 100);
        episodic_memory_store(memory, &ep);
    }

    // Gap: 10 seconds
    // Cluster 2: 5 failures at T+10000 to T+10500ms
    for (uint64_t i = 6; i <= 10; i++) {
        recovery_episode_t ep = create_recovery_scenario(
            i, ERROR_TYPE_TIMEOUT, 8080,
            STRATEGY_RETRY, false, 1000, -0.4f);
        ep.timestamp = base_time + 10000 + ((i - 5) * 100);
        episodic_memory_store(memory, &ep);
    }

    // ACT: Get all episodes
    uint32_t count = 0;
    recovery_episode_t** episodes = episodic_memory_get_all(memory, &count);

    // ASSERT: Should identify 2 temporal clusters
    ASSERT_NE(episodes, nullptr);
    EXPECT_EQ(count, 10);

    // Simple clustering: gap > 1 second = new cluster
    uint32_t clusters = 1;
    for (uint32_t i = 1; i < count; i++) {
        uint64_t gap = episodes[i]->timestamp - episodes[i-1]->timestamp;
        if (gap > 1000) {  // 1 second threshold
            clusters++;
        }
    }

    EXPECT_EQ(clusters, 2) << "Should detect 2 temporal clusters";

    nimcp_free(episodes);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
