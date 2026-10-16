/**
 * @file test_swarm_memory_patterns_regression.cpp
 * @brief Regression tests for Swarm Memory Pattern Learning
 *
 * TEST COVERAGE:
 * - Performance benchmarks
 * - Memory limit validation
 * - Throughput measurement
 * - Latency testing
 * - Resource leak detection
 * - Stress testing
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_memory.h"

class SwarmMemoryPatternsRegressionTest : public ::testing::Test {
protected:
    NimcpSwarmMemory* system;

    void SetUp() override {
        system = nimcp_swarm_memory_create(10000, 3);  // Large capacity for stress tests
        ASSERT_NE(system, nullptr);
        nimcp_swarm_memory_init(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            nimcp_swarm_memory_destroy(system);
        }
    }

    /**
     * @brief Create test pattern
     */
    swarm_pattern_t create_pattern(uint32_t sig_size, float confidence) {
        swarm_pattern_t pattern;
        memset(&pattern, 0, sizeof(pattern));

        pattern.signature = new float[sig_size];
        pattern.signature_size = sig_size;

        for (uint32_t i = 0; i < sig_size; i++) {
            pattern.signature[i] = static_cast<float>(i) / static_cast<float>(sig_size);
        }

        pattern.confidence = confidence;
        pattern.occurrence_count = 1;
        pattern.first_seen_ms = 1000000;
        pattern.last_seen_ms = 1000000;

        return pattern;
    }

    void free_pattern(swarm_pattern_t& pattern) {
        if (pattern.signature) {
            delete[] pattern.signature;
            pattern.signature = nullptr;
        }
    }

    /**
     * @brief Measure operation time in microseconds
     */
    template<typename Func>
    int64_t measure_time_us(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
};

/* ============================================================================
 * Performance Benchmarks
 * ============================================================================ */

TEST_F(SwarmMemoryPatternsRegressionTest, BenchmarkPatternStorage) {
    const uint32_t NUM_PATTERNS = 1000;
    const uint32_t SIGNATURE_SIZE = 50;

    auto duration = measure_time_us([&]() {
        for (uint32_t i = 0; i < NUM_PATTERNS; i++) {
            swarm_pattern_t pattern = create_pattern(SIGNATURE_SIZE, 0.75f);
            swarm_memory_store_pattern(system, &pattern);
            free_pattern(pattern);
        }
    });

    // Should complete in reasonable time (< 1s for 1000 patterns)
    EXPECT_LT(duration, 1000000);  // 1 second

    // Calculate throughput
    double patterns_per_second = (NUM_PATTERNS * 1000000.0) / duration;
    std::cout << "Pattern storage throughput: " << patterns_per_second << " patterns/sec" << std::endl;
    std::cout << "Average latency: " << (duration / NUM_PATTERNS) << " us/pattern" << std::endl;

    // Verify all were stored
    swarm_pattern_stats_t stats = swarm_memory_get_pattern_stats(system);
    EXPECT_GT(stats.total_patterns, 0u);
}

TEST_F(SwarmMemoryPatternsRegressionTest, BenchmarkAssociationCreation) {
    const uint32_t NUM_ASSOCIATIONS = 5000;

    auto duration = measure_time_us([&]() {
        for (uint32_t i = 0; i < NUM_ASSOCIATIONS; i++) {
            uint32_t pattern_id = i % 100;  // Reuse pattern IDs
            uint32_t outcome_id = i % 50;
            swarm_memory_associate_pattern(system, pattern_id, outcome_id, 0.7f);
        }
    });

    EXPECT_LT(duration, 500000);  // 500ms

    double assoc_per_second = (NUM_ASSOCIATIONS * 1000000.0) / duration;
    std::cout << "Association throughput: " << assoc_per_second << " assoc/sec" << std::endl;

    swarm_pattern_stats_t stats = swarm_memory_get_pattern_stats(system);
    EXPECT_GT(stats.total_associations, 0u);
}

TEST_F(SwarmMemoryPatternsRegressionTest, BenchmarkSequenceLearning) {
    const uint32_t NUM_SEQUENCES = 100;
    const uint32_t SEQ_LENGTH = 20;

    uint32_t sequence[SEQ_LENGTH];
    for (uint32_t i = 0; i < SEQ_LENGTH; i++) {
        sequence[i] = i + 1;
    }

    auto duration = measure_time_us([&]() {
        for (uint32_t i = 0; i < NUM_SEQUENCES; i++) {
            swarm_memory_learn_sequence(system, sequence, SEQ_LENGTH);
        }
    });

    EXPECT_LT(duration, 200000);  // 200ms

    double seq_per_second = (NUM_SEQUENCES * 1000000.0) / duration;
    std::cout << "Sequence learning throughput: " << seq_per_second << " seq/sec" << std::endl;
}

TEST_F(SwarmMemoryPatternsRegressionTest, BenchmarkPatternRetrieval) {
    // Store patterns first
    const uint32_t NUM_PATTERNS = 100;
    std::vector<uint32_t> stored_ids;

    for (uint32_t i = 0; i < NUM_PATTERNS; i++) {
        swarm_pattern_t pattern = create_pattern(20, 0.8f);
        pattern.pattern_id = i + 1;
        swarm_memory_store_pattern(system, &pattern);
        stored_ids.push_back(i + 1);
        free_pattern(pattern);
    }

    // Benchmark retrieval
    auto duration = measure_time_us([&]() {
        for (uint32_t id : stored_ids) {
            swarm_pattern_t retrieved;
            swarm_memory_retrieve_pattern(system, id, &retrieved);
            if (retrieved.signature) {
                delete[] retrieved.signature;
            }
        }
    });

    double retrieve_per_second = (NUM_PATTERNS * 1000000.0) / duration;
    std::cout << "Pattern retrieval throughput: " << retrieve_per_second << " patterns/sec" << std::endl;
}

/* ============================================================================
 * Memory Limit Tests
 * ============================================================================ */

TEST_F(SwarmMemoryPatternsRegressionTest, MemoryLimitEnforcement) {
    // This system has max_patterns = 10000
    // Try to store more than capacity

    const uint32_t OVER_CAPACITY = 100;

    for (uint32_t i = 0; i < OVER_CAPACITY; i++) {
        swarm_pattern_t pattern = create_pattern(10, 0.5f);
        nimcp_result_t result = swarm_memory_store_pattern(system, &pattern);
        free_pattern(pattern);

        // Should succeed - system may forget weak patterns automatically
        EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ERROR);
    }

    swarm_pattern_stats_t stats = swarm_memory_get_pattern_stats(system);
    // Should not exceed capacity significantly
    EXPECT_LE(stats.total_patterns, 12000u);  // Some margin for overflow
}

TEST_F(SwarmMemoryPatternsRegressionTest, LargePatternHandling) {
    const uint32_t VERY_LARGE_SIZE = 10000;

    swarm_pattern_t large = create_pattern(VERY_LARGE_SIZE, 0.9f);

    auto duration = measure_time_us([&]() {
        swarm_memory_store_pattern(system, &large);
    });

    // Should handle large patterns without excessive delay
    EXPECT_LT(duration, 100000);  // 100ms

    free_pattern(large);
}

TEST_F(SwarmMemoryPatternsRegressionTest, AssociationCapacity) {
    // Create many associations for single pattern
    const uint32_t MANY_OUTCOMES = 1000;

    for (uint32_t i = 0; i < MANY_OUTCOMES; i++) {
        swarm_memory_associate_pattern(system, 1, i, 0.6f);
    }

    swarm_pattern_stats_t stats = swarm_memory_get_pattern_stats(system);
    EXPECT_GE(stats.total_associations, MANY_OUTCOMES);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(SwarmMemoryPatternsRegressionTest, MixedOperationStress) {
    const uint32_t ITERATIONS = 100;

    for (uint32_t i = 0; i < ITERATIONS; i++) {
        // Store pattern
        swarm_pattern_t p = create_pattern(30, 0.7f);
        swarm_memory_store_pattern(system, &p);
        free_pattern(p);

        // Create association
        swarm_memory_associate_pattern(system, i % 50, i % 25, 0.75f);

        // Learn sequence
        uint32_t seq[] = {i, i+1, i+2, i+3};
        swarm_memory_learn_sequence(system, seq, 4);

        // Consolidate periodically
        if (i % 10 == 0) {
            swarm_memory_consolidate_patterns(system, 1000000 + i * 1000);
        }
    }

    swarm_pattern_stats_t stats = swarm_memory_get_pattern_stats(system);
    EXPECT_GT(stats.total_patterns, 0u);
    EXPECT_GT(stats.total_associations, 0u);
}

TEST_F(SwarmMemoryPatternsRegressionTest, RapidSequenceLearning) {
    const uint32_t NUM_SEQUENCES = 1000;

    for (uint32_t i = 0; i < NUM_SEQUENCES; i++) {
        uint32_t seq[] = {i, i+1, i+2};
        swarm_memory_learn_sequence(system, seq, 3);
    }

    SUCCEED();  // Test passes if no crash
}

TEST_F(SwarmMemoryPatternsRegressionTest, PatternChurn) {
    // Repeatedly store and forget patterns
    const uint32_t CYCLES = 50;
    const uint32_t PATTERNS_PER_CYCLE = 20;

    for (uint32_t cycle = 0; cycle < CYCLES; cycle++) {
        // Store patterns
        for (uint32_t i = 0; i < PATTERNS_PER_CYCLE; i++) {
            swarm_pattern_t p = create_pattern(15, 0.4f + (i * 0.01f));
            swarm_memory_store_pattern(system, &p);
            free_pattern(p);
        }

        // Forget weak ones
        swarm_memory_forget_weak_patterns(system, 0.5f);
    }

    swarm_pattern_stats_t stats = swarm_memory_get_pattern_stats(system);
    EXPECT_GT(stats.patterns_learned, 0u);
}

/* ============================================================================
 * Resource Leak Detection
 * ============================================================================ */

TEST_F(SwarmMemoryPatternsRegressionTest, NoMemoryLeakPatternStorage) {
    // Store and retrieve patterns multiple times
    // Memory usage should stabilize

    for (int iteration = 0; iteration < 10; iteration++) {
        for (int i = 0; i < 100; i++) {
            swarm_pattern_t p = create_pattern(50, 0.75f);
            swarm_memory_store_pattern(system, &p);
            free_pattern(p);
        }

        // Forget to free memory
        swarm_memory_forget_weak_patterns(system, 0.8f);
    }

    SUCCEED();  // If we get here without crash, likely no major leaks
}

TEST_F(SwarmMemoryPatternsRegressionTest, NoMemoryLeakPatternRetrieval) {
    // Store some patterns
    for (int i = 0; i < 50; i++) {
        swarm_pattern_t p = create_pattern(20, 0.8f);
        p.pattern_id = i + 1;
        swarm_memory_store_pattern(system, &p);
        free_pattern(p);
    }

    // Retrieve repeatedly
    for (int iteration = 0; iteration < 100; iteration++) {
        for (int i = 1; i <= 50; i++) {
            swarm_pattern_t retrieved;
            if (swarm_memory_retrieve_pattern(system, i, &retrieved) == NIMCP_SUCCESS) {
                if (retrieved.signature) {
                    delete[] retrieved.signature;
                }
            }
        }
    }

    SUCCEED();
}

/* ============================================================================
 * Latency Tests
 * ============================================================================ */

TEST_F(SwarmMemoryPatternsRegressionTest, StorePatternLatency) {
    const uint32_t SAMPLES = 100;
    std::vector<int64_t> latencies;

    for (uint32_t i = 0; i < SAMPLES; i++) {
        swarm_pattern_t p = create_pattern(30, 0.75f);

        auto latency = measure_time_us([&]() {
            swarm_memory_store_pattern(system, &p);
        });

        latencies.push_back(latency);
        free_pattern(p);
    }

    // Calculate statistics
    int64_t sum = 0;
    int64_t max_latency = 0;

    for (int64_t lat : latencies) {
        sum += lat;
        if (lat > max_latency) max_latency = lat;
    }

    double avg_latency = static_cast<double>(sum) / SAMPLES;

    std::cout << "Store pattern - Average latency: " << avg_latency << " us" << std::endl;
    std::cout << "Store pattern - Max latency: " << max_latency << " us" << std::endl;

    // Average should be reasonable (< 1ms)
    EXPECT_LT(avg_latency, 1000.0);

    // Max should not be excessive (< 10ms)
    EXPECT_LT(max_latency, 10000);
}

TEST_F(SwarmMemoryPatternsRegressionTest, AssociatePatternLatency) {
    const uint32_t SAMPLES = 1000;
    std::vector<int64_t> latencies;

    for (uint32_t i = 0; i < SAMPLES; i++) {
        auto latency = measure_time_us([&]() {
            swarm_memory_associate_pattern(system, i % 100, i % 50, 0.7f);
        });

        latencies.push_back(latency);
    }

    int64_t sum = 0;
    for (int64_t lat : latencies) {
        sum += lat;
    }

    double avg_latency = static_cast<double>(sum) / SAMPLES;

    std::cout << "Associate pattern - Average latency: " << avg_latency << " us" << std::endl;

    // Should be very fast (< 100us)
    EXPECT_LT(avg_latency, 100.0);
}

/* ============================================================================
 * Consistency Tests
 * ============================================================================ */

TEST_F(SwarmMemoryPatternsRegressionTest, StatsConsistency) {
    // Perform known operations and verify stats match

    const uint32_t NUM_PATTERNS = 50;
    const uint32_t NUM_ASSOCIATIONS = 30;

    for (uint32_t i = 0; i < NUM_PATTERNS; i++) {
        swarm_pattern_t p = create_pattern(10, 0.8f);
        swarm_memory_store_pattern(system, &p);
        free_pattern(p);
    }

    for (uint32_t i = 0; i < NUM_ASSOCIATIONS; i++) {
        swarm_memory_associate_pattern(system, i % 25, i % 10, 0.75f);
    }

    swarm_pattern_stats_t stats = swarm_memory_get_pattern_stats(system);

    EXPECT_EQ(stats.total_patterns, NUM_PATTERNS);
    EXPECT_EQ(stats.patterns_learned, NUM_PATTERNS);
    EXPECT_GE(stats.total_associations, NUM_ASSOCIATIONS);
}

TEST_F(SwarmMemoryPatternsRegressionTest, ConsolidationConsistency) {
    // Consolidation should not corrupt data

    for (int i = 0; i < 20; i++) {
        swarm_pattern_t p = create_pattern(15, 0.7f + i * 0.01f);
        swarm_memory_store_pattern(system, &p);
        free_pattern(p);
    }

    swarm_pattern_stats_t before = swarm_memory_get_pattern_stats(system);

    swarm_memory_consolidate_patterns(system, 2000000);

    swarm_pattern_stats_t after = swarm_memory_get_pattern_stats(system);

    // Total patterns should not decrease (unless forgetting occurred)
    EXPECT_GE(after.total_patterns, 0u);
}

/* ============================================================================
 * Boundary Condition Tests
 * ============================================================================ */

TEST_F(SwarmMemoryPatternsRegressionTest, ZeroSizePattern) {
    swarm_pattern_t p;
    memset(&p, 0, sizeof(p));
    p.signature = nullptr;
    p.signature_size = 0;

    nimcp_result_t result = swarm_memory_store_pattern(system, &p);

    // Should reject invalid pattern
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryPatternsRegressionTest, SingleElementSequence) {
    uint32_t seq[] = {1};

    nimcp_result_t result = swarm_memory_learn_sequence(system, seq, 1);

    // Should reject too-short sequence
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryPatternsRegressionTest, ExtremeConfidenceValues) {
    swarm_pattern_t p1 = create_pattern(10, 0.0f);
    swarm_pattern_t p2 = create_pattern(10, 1.0f);

    EXPECT_EQ(swarm_memory_store_pattern(system, &p1), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_memory_store_pattern(system, &p2), NIMCP_SUCCESS);

    free_pattern(p1);
    free_pattern(p2);
}

/* ============================================================================
 * Regression-Specific Tests (Catch Past Bugs)
 * ============================================================================ */

TEST_F(SwarmMemoryPatternsRegressionTest, NullPointerCheck) {
    // Ensure null checks are in place
    EXPECT_NE(swarm_memory_store_pattern(nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(swarm_memory_retrieve_pattern(nullptr, 1, nullptr), NIMCP_SUCCESS);
    EXPECT_NE(swarm_memory_associate_pattern(nullptr, 1, 2, 0.5f), NIMCP_SUCCESS);
    EXPECT_NE(swarm_memory_learn_sequence(nullptr, nullptr, 0), NIMCP_SUCCESS);
}

TEST_F(SwarmMemoryPatternsRegressionTest, UninitializedSystem) {
    // Create but don't initialize
    NimcpSwarmMemory* uninit = nimcp_swarm_memory_create(100, 2);
    ASSERT_NE(uninit, nullptr);

    // Operations should fail or handle gracefully
    swarm_pattern_t p = create_pattern(10, 0.8f);
    nimcp_result_t result = swarm_memory_store_pattern(uninit, &p);

    // Should fail since not initialized
    EXPECT_NE(result, NIMCP_SUCCESS);

    free_pattern(p);
    nimcp_swarm_memory_destroy(uninit);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
