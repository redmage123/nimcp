/**
 * @file test_cognitive_substrate_bridges_regression.cpp
 * @brief Comprehensive regression tests for ALL cognitive substrate bridges
 *
 * WHAT: Performance, stability, and correctness regression tests for 8 cognitive
 *       substrate bridges (attention, emotion, executive, introspection, memory,
 *       reasoning, tom, working_memory)
 *
 * WHY: Ensure substrate bridges maintain acceptable performance, memory usage,
 *      numerical stability, and biological correctness across code changes
 *
 * HOW: Benchmark throughput, test memory leak resistance, verify edge case
 *      handling, validate scaling behavior, confirm biological effects
 *
 * PERFORMANCE REQUIREMENTS:
 * - Bridge create/destroy: < 100 us/operation
 * - Update cycle: < 50 us/bridge
 * - Effects query: < 1 us/call
 * - Bio-async connection: < 200 us/operation
 * - Memory: No leaks over 1000 create/destroy cycles
 * - Scaling: Linear performance up to 100 concurrent bridges
 *
 * COVERAGE:
 * 1. PerformanceBenchmarks - Throughput for each bridge type (8 tests)
 * 2. MemoryRegression - No leaks over repeated cycles (8 tests)
 * 3. NumericalStability - Edge cases (ATP=0, temp=45°C, etc.) (5 tests)
 * 4. ScalingTests - Performance with many bridges (3 tests)
 * 5. CorrectnessRegression - Known biological effects preserved (8 tests)
 * 6. ThreadSafetyRegression - Concurrent access to shared substrate (2 tests)
 * Total: ~34 tests
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <thread>
#include <cmath>
#include <algorithm>

// Headers have their own extern "C" guards
#include "cognitive/attention/nimcp_attention_substrate_bridge.h"
#include "cognitive/emotion/nimcp_emotion_substrate_bridge.h"
#include "cognitive/executive/nimcp_executive_substrate_bridge.h"
#include "cognitive/introspection/nimcp_introspection_substrate_bridge.h"
#include "cognitive/memory/nimcp_memory_consolidation_substrate_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_substrate_bridge.h"
#include "cognitive/tom/nimcp_tom_substrate_bridge.h"
#include "cognitive/working_memory/nimcp_working_memory_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"

/* ============================================================================
 * Test Fixture
 * ========================================================================== */

class CognitiveSubstrateBridgesRegressionTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;

    // Warmup/benchmark constants
    static constexpr int WARMUP_ITERATIONS = 50;
    static constexpr int BENCHMARK_ITERATIONS = 1000;
    static constexpr int MEMORY_TEST_CYCLES = 1000;
    static constexpr int SCALING_TEST_BRIDGES = 100;

    void SetUp() override {
        // Create neural substrate with default config
        neural_substrate_config_t config = neural_substrate_get_default_config();
        substrate = neural_substrate_create(&config);
        ASSERT_NE(substrate, nullptr);

        // Set to healthy state (ATP=0.9, temp=37°C)
        neural_substrate_set_atp_level(substrate, 0.9f);
        neural_substrate_set_temperature(substrate, 37.0f);
    }

    void TearDown() override {
        if (substrate) {
            neural_substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    // Helper: Calculate timing statistics
    struct TimingStats {
        double avg_ns;
        double min_ns;
        double max_ns;
        double stddev_ns;
    };

    TimingStats calculate_stats(const std::vector<long long>& times) {
        TimingStats stats = {0, 0, 0, 0};
        if (times.empty()) return stats;

        stats.min_ns = static_cast<double>(times[0]);
        stats.max_ns = static_cast<double>(times[0]);
        double sum = 0;

        for (auto t : times) {
            sum += static_cast<double>(t);
            if (t < stats.min_ns) stats.min_ns = static_cast<double>(t);
            if (t > stats.max_ns) stats.max_ns = static_cast<double>(t);
        }
        stats.avg_ns = sum / times.size();

        double variance = 0;
        for (auto t : times) {
            double diff = static_cast<double>(t) - stats.avg_ns;
            variance += diff * diff;
        }
        stats.stddev_ns = std::sqrt(variance / times.size());

        return stats;
    }

    // Helper: Create mock attention system (minimal stub)
    nimcp_attention_system_t* create_mock_attention() {
        // Return minimal valid pointer (tests don't use it, just need non-NULL)
        return reinterpret_cast<nimcp_attention_system_t*>(0x1);
    }

    // Helper: Create mock emotion system
    emotional_system_t* create_mock_emotion() {
        return reinterpret_cast<emotional_system_t*>(0x2);
    }

    // Helper: Create mock executive system
    nimcp_executive_t* create_mock_executive() {
        return reinterpret_cast<nimcp_executive_t*>(0x3);
    }

    // Helper: Create mock introspection system
    nimcp_introspection_t* create_mock_introspection() {
        return reinterpret_cast<nimcp_introspection_t*>(0x4);
    }

    // Helper: Create mock memory consolidation
    memory_consolidation_t* create_mock_memory() {
        return reinterpret_cast<memory_consolidation_t*>(0x5);
    }

    // Helper: Create mock reasoning system
    nimcp_reasoning_system_t* create_mock_reasoning() {
        return reinterpret_cast<nimcp_reasoning_system_t*>(0x6);
    }

    // Helper: Create mock ToM system
    theory_of_mind_t* create_mock_tom() {
        return reinterpret_cast<theory_of_mind_t*>(0x7);
    }

    // Helper: Create mock working memory
    working_memory_t* create_mock_working_memory() {
        return reinterpret_cast<working_memory_t*>(0x8);
    }
};

/* ============================================================================
 * CATEGORY 1: Performance Benchmarks (8 tests)
 * ========================================================================== */

TEST_F(CognitiveSubstrateBridgesRegressionTest, Performance_AttentionBridge_CreateUpdateDestroy) {
    std::vector<long long> create_times, update_times, destroy_times;
    create_times.reserve(BENCHMARK_ITERATIONS);
    update_times.reserve(BENCHMARK_ITERATIONS);
    destroy_times.reserve(BENCHMARK_ITERATIONS);

    auto* attention = create_mock_attention();
    attention_substrate_config_t config;
    attention_substrate_default_config(&config);
    config.enable_bio_async = false;  // Disable for pure benchmark

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = attention_substrate_bridge_create(&config, substrate, attention);
        attention_substrate_update(bridge);
        attention_substrate_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* bridge = attention_substrate_bridge_create(&config, substrate, attention);
        auto end = std::chrono::high_resolution_clock::now();
        create_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        attention_substrate_update(bridge);
        end = std::chrono::high_resolution_clock::now();
        update_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        attention_substrate_bridge_destroy(bridge);
        end = std::chrono::high_resolution_clock::now();
        destroy_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto create_stats = calculate_stats(create_times);
    auto update_stats = calculate_stats(update_times);
    auto destroy_stats = calculate_stats(destroy_times);

    std::cout << "Attention Bridge Performance (" << BENCHMARK_ITERATIONS << " iterations):\n";
    std::cout << "  Create: avg=" << create_stats.avg_ns << " ns\n";
    std::cout << "  Update: avg=" << update_stats.avg_ns << " ns\n";
    std::cout << "  Destroy: avg=" << destroy_stats.avg_ns << " ns\n";

    EXPECT_LT(create_stats.avg_ns, 100000.0) << "Create should be < 100 us";
    EXPECT_LT(update_stats.avg_ns, 50000.0) << "Update should be < 50 us";
    EXPECT_LT(destroy_stats.avg_ns, 100000.0) << "Destroy should be < 100 us";
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Performance_EmotionBridge_CreateUpdateDestroy) {
    std::vector<long long> create_times, update_times, destroy_times;
    create_times.reserve(BENCHMARK_ITERATIONS);
    update_times.reserve(BENCHMARK_ITERATIONS);
    destroy_times.reserve(BENCHMARK_ITERATIONS);

    auto* emotion = create_mock_emotion();
    emotion_substrate_config_t config;
    emotion_substrate_default_config(&config);
    config.enable_bio_async = false;

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = emotion_substrate_bridge_create(&config, emotion, substrate);
        emotion_substrate_update(bridge);
        emotion_substrate_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* bridge = emotion_substrate_bridge_create(&config, emotion, substrate);
        auto end = std::chrono::high_resolution_clock::now();
        create_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        emotion_substrate_update(bridge);
        end = std::chrono::high_resolution_clock::now();
        update_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        emotion_substrate_bridge_destroy(bridge);
        end = std::chrono::high_resolution_clock::now();
        destroy_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto create_stats = calculate_stats(create_times);
    auto update_stats = calculate_stats(update_times);
    auto destroy_stats = calculate_stats(destroy_times);

    std::cout << "Emotion Bridge Performance (" << BENCHMARK_ITERATIONS << " iterations):\n";
    std::cout << "  Create: avg=" << create_stats.avg_ns << " ns\n";
    std::cout << "  Update: avg=" << update_stats.avg_ns << " ns\n";
    std::cout << "  Destroy: avg=" << destroy_stats.avg_ns << " ns\n";

    EXPECT_LT(create_stats.avg_ns, 100000.0) << "Create should be < 100 us";
    EXPECT_LT(update_stats.avg_ns, 50000.0) << "Update should be < 50 us";
    EXPECT_LT(destroy_stats.avg_ns, 100000.0) << "Destroy should be < 100 us";
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Performance_ExecutiveBridge_CreateUpdateDestroy) {
    std::vector<long long> create_times, update_times, destroy_times;
    create_times.reserve(BENCHMARK_ITERATIONS);
    update_times.reserve(BENCHMARK_ITERATIONS);
    destroy_times.reserve(BENCHMARK_ITERATIONS);

    auto* executive = create_mock_executive();
    executive_substrate_config_t config;
    executive_substrate_default_config(&config);
    config.enable_bio_async = false;

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = executive_substrate_bridge_create(&config, executive, substrate);
        executive_substrate_update(bridge);
        executive_substrate_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* bridge = executive_substrate_bridge_create(&config, executive, substrate);
        auto end = std::chrono::high_resolution_clock::now();
        create_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        executive_substrate_update(bridge);
        end = std::chrono::high_resolution_clock::now();
        update_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        executive_substrate_bridge_destroy(bridge);
        end = std::chrono::high_resolution_clock::now();
        destroy_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto create_stats = calculate_stats(create_times);
    auto update_stats = calculate_stats(update_times);
    auto destroy_stats = calculate_stats(destroy_times);

    std::cout << "Executive Bridge Performance (" << BENCHMARK_ITERATIONS << " iterations):\n";
    std::cout << "  Create: avg=" << create_stats.avg_ns << " ns\n";
    std::cout << "  Update: avg=" << update_stats.avg_ns << " ns\n";
    std::cout << "  Destroy: avg=" << destroy_stats.avg_ns << " ns\n";

    EXPECT_LT(create_stats.avg_ns, 100000.0) << "Create should be < 100 us";
    EXPECT_LT(update_stats.avg_ns, 50000.0) << "Update should be < 50 us";
    EXPECT_LT(destroy_stats.avg_ns, 100000.0) << "Destroy should be < 100 us";
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Performance_IntrospectionBridge_CreateUpdateDestroy) {
    std::vector<long long> create_times, update_times, destroy_times;
    create_times.reserve(BENCHMARK_ITERATIONS);
    update_times.reserve(BENCHMARK_ITERATIONS);
    destroy_times.reserve(BENCHMARK_ITERATIONS);

    auto* introspection = create_mock_introspection();
    introspection_substrate_config_t config;
    introspection_substrate_default_config(&config);
    config.enable_bio_async = false;

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = introspection_substrate_bridge_create(&config, introspection, substrate);
        introspection_substrate_update(bridge);
        introspection_substrate_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* bridge = introspection_substrate_bridge_create(&config, introspection, substrate);
        auto end = std::chrono::high_resolution_clock::now();
        create_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        introspection_substrate_update(bridge);
        end = std::chrono::high_resolution_clock::now();
        update_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        introspection_substrate_bridge_destroy(bridge);
        end = std::chrono::high_resolution_clock::now();
        destroy_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto create_stats = calculate_stats(create_times);
    auto update_stats = calculate_stats(update_times);
    auto destroy_stats = calculate_stats(destroy_times);

    std::cout << "Introspection Bridge Performance (" << BENCHMARK_ITERATIONS << " iterations):\n";
    std::cout << "  Create: avg=" << create_stats.avg_ns << " ns\n";
    std::cout << "  Update: avg=" << update_stats.avg_ns << " ns\n";
    std::cout << "  Destroy: avg=" << destroy_stats.avg_ns << " ns\n";

    EXPECT_LT(create_stats.avg_ns, 100000.0) << "Create should be < 100 us";
    EXPECT_LT(update_stats.avg_ns, 50000.0) << "Update should be < 50 us";
    EXPECT_LT(destroy_stats.avg_ns, 100000.0) << "Destroy should be < 100 us";
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Performance_MemoryBridge_CreateUpdateDestroy) {
    std::vector<long long> create_times, update_times, destroy_times;
    create_times.reserve(BENCHMARK_ITERATIONS);
    update_times.reserve(BENCHMARK_ITERATIONS);
    destroy_times.reserve(BENCHMARK_ITERATIONS);

    auto* memory = create_mock_memory();
    consolidation_substrate_config_t config;
    consolidation_substrate_default_config(&config);
    config.enable_bio_async = false;

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = consolidation_substrate_bridge_create(&config, memory, substrate);
        consolidation_substrate_update(bridge);
        consolidation_substrate_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* bridge = consolidation_substrate_bridge_create(&config, memory, substrate);
        auto end = std::chrono::high_resolution_clock::now();
        create_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        consolidation_substrate_update(bridge);
        end = std::chrono::high_resolution_clock::now();
        update_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        consolidation_substrate_bridge_destroy(bridge);
        end = std::chrono::high_resolution_clock::now();
        destroy_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto create_stats = calculate_stats(create_times);
    auto update_stats = calculate_stats(update_times);
    auto destroy_stats = calculate_stats(destroy_times);

    std::cout << "Memory Consolidation Bridge Performance (" << BENCHMARK_ITERATIONS << " iterations):\n";
    std::cout << "  Create: avg=" << create_stats.avg_ns << " ns\n";
    std::cout << "  Update: avg=" << update_stats.avg_ns << " ns\n";
    std::cout << "  Destroy: avg=" << destroy_stats.avg_ns << " ns\n";

    EXPECT_LT(create_stats.avg_ns, 100000.0) << "Create should be < 100 us";
    EXPECT_LT(update_stats.avg_ns, 50000.0) << "Update should be < 50 us";
    EXPECT_LT(destroy_stats.avg_ns, 100000.0) << "Destroy should be < 100 us";
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Performance_ReasoningBridge_CreateUpdateDestroy) {
    std::vector<long long> create_times, update_times, destroy_times;
    create_times.reserve(BENCHMARK_ITERATIONS);
    update_times.reserve(BENCHMARK_ITERATIONS);
    destroy_times.reserve(BENCHMARK_ITERATIONS);

    auto* reasoning = create_mock_reasoning();
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = reasoning_substrate_bridge_create(&config, reasoning, substrate);
        reasoning_substrate_update(bridge);
        reasoning_substrate_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* bridge = reasoning_substrate_bridge_create(&config, reasoning, substrate);
        auto end = std::chrono::high_resolution_clock::now();
        create_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        reasoning_substrate_update(bridge);
        end = std::chrono::high_resolution_clock::now();
        update_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        reasoning_substrate_bridge_destroy(bridge);
        end = std::chrono::high_resolution_clock::now();
        destroy_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto create_stats = calculate_stats(create_times);
    auto update_stats = calculate_stats(update_times);
    auto destroy_stats = calculate_stats(destroy_times);

    std::cout << "Reasoning Bridge Performance (" << BENCHMARK_ITERATIONS << " iterations):\n";
    std::cout << "  Create: avg=" << create_stats.avg_ns << " ns\n";
    std::cout << "  Update: avg=" << update_stats.avg_ns << " ns\n";
    std::cout << "  Destroy: avg=" << destroy_stats.avg_ns << " ns\n";

    EXPECT_LT(create_stats.avg_ns, 100000.0) << "Create should be < 100 us";
    EXPECT_LT(update_stats.avg_ns, 50000.0) << "Update should be < 50 us";
    EXPECT_LT(destroy_stats.avg_ns, 100000.0) << "Destroy should be < 100 us";
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Performance_ToMBridge_CreateUpdateDestroy) {
    std::vector<long long> create_times, update_times, destroy_times;
    create_times.reserve(BENCHMARK_ITERATIONS);
    update_times.reserve(BENCHMARK_ITERATIONS);
    destroy_times.reserve(BENCHMARK_ITERATIONS);

    auto* tom = create_mock_tom();
    tom_substrate_config_t config;
    tom_substrate_default_config(&config);
    config.enable_bio_async = false;

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = tom_substrate_bridge_create(&config, tom, substrate);
        tom_substrate_update(bridge);
        tom_substrate_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* bridge = tom_substrate_bridge_create(&config, tom, substrate);
        auto end = std::chrono::high_resolution_clock::now();
        create_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        tom_substrate_update(bridge);
        end = std::chrono::high_resolution_clock::now();
        update_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        tom_substrate_bridge_destroy(bridge);
        end = std::chrono::high_resolution_clock::now();
        destroy_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto create_stats = calculate_stats(create_times);
    auto update_stats = calculate_stats(update_times);
    auto destroy_stats = calculate_stats(destroy_times);

    std::cout << "ToM Bridge Performance (" << BENCHMARK_ITERATIONS << " iterations):\n";
    std::cout << "  Create: avg=" << create_stats.avg_ns << " ns\n";
    std::cout << "  Update: avg=" << update_stats.avg_ns << " ns\n";
    std::cout << "  Destroy: avg=" << destroy_stats.avg_ns << " ns\n";

    EXPECT_LT(create_stats.avg_ns, 100000.0) << "Create should be < 100 us";
    EXPECT_LT(update_stats.avg_ns, 50000.0) << "Update should be < 50 us";
    EXPECT_LT(destroy_stats.avg_ns, 100000.0) << "Destroy should be < 100 us";
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Performance_WorkingMemoryBridge_CreateUpdateDestroy) {
    std::vector<long long> create_times, update_times, destroy_times;
    create_times.reserve(BENCHMARK_ITERATIONS);
    update_times.reserve(BENCHMARK_ITERATIONS);
    destroy_times.reserve(BENCHMARK_ITERATIONS);

    auto* wm = create_mock_working_memory();
    wm_substrate_config_t config;
    wm_substrate_default_config(&config);
    config.enable_bio_async = false;

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        auto* bridge = wm_substrate_bridge_create(&config, wm, substrate);
        wm_substrate_update(bridge);
        wm_substrate_bridge_destroy(bridge);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        auto* bridge = wm_substrate_bridge_create(&config, wm, substrate);
        auto end = std::chrono::high_resolution_clock::now();
        create_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        wm_substrate_update(bridge);
        end = std::chrono::high_resolution_clock::now();
        update_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());

        start = std::chrono::high_resolution_clock::now();
        wm_substrate_bridge_destroy(bridge);
        end = std::chrono::high_resolution_clock::now();
        destroy_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }

    auto create_stats = calculate_stats(create_times);
    auto update_stats = calculate_stats(update_times);
    auto destroy_stats = calculate_stats(destroy_times);

    std::cout << "Working Memory Bridge Performance (" << BENCHMARK_ITERATIONS << " iterations):\n";
    std::cout << "  Create: avg=" << create_stats.avg_ns << " ns\n";
    std::cout << "  Update: avg=" << update_stats.avg_ns << " ns\n";
    std::cout << "  Destroy: avg=" << destroy_stats.avg_ns << " ns\n";

    EXPECT_LT(create_stats.avg_ns, 100000.0) << "Create should be < 100 us";
    EXPECT_LT(update_stats.avg_ns, 50000.0) << "Update should be < 50 us";
    EXPECT_LT(destroy_stats.avg_ns, 100000.0) << "Destroy should be < 100 us";
}

/* ============================================================================
 * CATEGORY 2: Memory Regression (8 tests)
 * ========================================================================== */

TEST_F(CognitiveSubstrateBridgesRegressionTest, Memory_AttentionBridge_NoLeaksOverCycles) {
    auto* attention = create_mock_attention();
    attention_substrate_config_t config;
    attention_substrate_default_config(&config);
    config.enable_bio_async = false;

    // Run many create/destroy cycles
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = attention_substrate_bridge_create(&config, substrate, attention);
        ASSERT_NE(bridge, nullptr);

        // Use the bridge
        attention_substrate_update(bridge);
        float focus = attention_substrate_get_focus_capacity(bridge);
        (void)focus;  // Suppress unused warning

        attention_substrate_bridge_destroy(bridge);
    }

    // No assertion needed - valgrind/asan will detect leaks
    // This test passes if no memory errors occur
    SUCCEED();
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Memory_EmotionBridge_NoLeaksOverCycles) {
    auto* emotion = create_mock_emotion();
    emotion_substrate_config_t config;
    emotion_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = emotion_substrate_bridge_create(&config, emotion, substrate);
        ASSERT_NE(bridge, nullptr);

        emotion_substrate_update(bridge);
        float intensity = emotion_substrate_get_intensity_mod(bridge);
        (void)intensity;

        emotion_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Memory_ExecutiveBridge_NoLeaksOverCycles) {
    auto* executive = create_mock_executive();
    executive_substrate_config_t config;
    executive_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = executive_substrate_bridge_create(&config, executive, substrate);
        ASSERT_NE(bridge, nullptr);

        executive_substrate_update(bridge);
        float decision_quality = executive_substrate_get_decision_quality(bridge);
        (void)decision_quality;

        executive_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Memory_IntrospectionBridge_NoLeaksOverCycles) {
    auto* introspection = create_mock_introspection();
    introspection_substrate_config_t config;
    introspection_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = introspection_substrate_bridge_create(&config, introspection, substrate);
        ASSERT_NE(bridge, nullptr);

        introspection_substrate_update(bridge);
        float awareness = introspection_substrate_get_self_awareness(bridge);
        (void)awareness;

        introspection_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Memory_MemoryBridge_NoLeaksOverCycles) {
    auto* memory = create_mock_memory();
    consolidation_substrate_config_t config;
    consolidation_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = consolidation_substrate_bridge_create(&config, memory, substrate);
        ASSERT_NE(bridge, nullptr);

        consolidation_substrate_update(bridge);
        float consolidation = consolidation_substrate_get_consolidation_rate(bridge);
        (void)consolidation;

        consolidation_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Memory_ReasoningBridge_NoLeaksOverCycles) {
    auto* reasoning = create_mock_reasoning();
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = reasoning_substrate_bridge_create(&config, reasoning, substrate);
        ASSERT_NE(bridge, nullptr);

        reasoning_substrate_update(bridge);
        float depth = reasoning_substrate_get_inference_depth(bridge);
        (void)depth;

        reasoning_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Memory_ToMBridge_NoLeaksOverCycles) {
    auto* tom = create_mock_tom();
    tom_substrate_config_t config;
    tom_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = tom_substrate_bridge_create(&config, tom, substrate);
        ASSERT_NE(bridge, nullptr);

        tom_substrate_update(bridge);
        float mentalizing = tom_substrate_get_mentalizing_capacity(bridge);
        (void)mentalizing;

        tom_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Memory_WorkingMemoryBridge_NoLeaksOverCycles) {
    auto* wm = create_mock_working_memory();
    wm_substrate_config_t config;
    wm_substrate_default_config(&config);
    config.enable_bio_async = false;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        auto* bridge = wm_substrate_bridge_create(&config, wm, substrate);
        ASSERT_NE(bridge, nullptr);

        wm_substrate_update(bridge);
        float capacity = wm_substrate_get_capacity_factor(bridge);
        (void)capacity;

        wm_substrate_bridge_destroy(bridge);
    }

    SUCCEED();
}

/* ============================================================================
 * CATEGORY 3: Numerical Stability (5 tests)
 * ========================================================================== */

TEST_F(CognitiveSubstrateBridgesRegressionTest, Stability_ZeroATP_AllBridges) {
    // Test all bridges with ATP=0 (critical state)
    neural_substrate_set_atp_level(substrate, 0.0f);

    auto* attention = create_mock_attention();
    auto* emotion = create_mock_emotion();
    auto* executive = create_mock_executive();
    auto* reasoning = create_mock_reasoning();

    attention_substrate_config_t att_config;
    attention_substrate_default_config(&att_config);
    att_config.enable_bio_async = false;

    emotion_substrate_config_t emo_config;
    emotion_substrate_default_config(&emo_config);
    emo_config.enable_bio_async = false;

    executive_substrate_config_t exec_config;
    executive_substrate_default_config(&exec_config);
    exec_config.enable_bio_async = false;

    reasoning_substrate_config_t reas_config;
    reasoning_substrate_default_config(&reas_config);
    reas_config.enable_bio_async = false;

    // Create and update bridges
    auto* att_bridge = attention_substrate_bridge_create(&att_config, substrate, attention);
    auto* emo_bridge = emotion_substrate_bridge_create(&emo_config, emotion, substrate);
    auto* exec_bridge = executive_substrate_bridge_create(&exec_config, executive, substrate);
    auto* reas_bridge = reasoning_substrate_bridge_create(&reas_config, reasoning, substrate);

    ASSERT_NE(att_bridge, nullptr);
    ASSERT_NE(emo_bridge, nullptr);
    ASSERT_NE(exec_bridge, nullptr);
    ASSERT_NE(reas_bridge, nullptr);

    // Update all - should not crash
    EXPECT_EQ(0, attention_substrate_update(att_bridge));
    EXPECT_EQ(0, emotion_substrate_update(emo_bridge));
    EXPECT_EQ(0, executive_substrate_update(exec_bridge));
    EXPECT_EQ(0, reasoning_substrate_update(reas_bridge));

    // All should report impairment
    EXPECT_TRUE(attention_substrate_is_impaired(att_bridge));
    EXPECT_TRUE(emotion_substrate_is_impaired(emo_bridge));
    EXPECT_TRUE(executive_substrate_is_impaired(exec_bridge));
    EXPECT_TRUE(reasoning_substrate_is_impaired(reas_bridge));

    // Effects should be minimal but valid (not NaN/Inf)
    float focus = attention_substrate_get_focus_capacity(att_bridge);
    EXPECT_FALSE(std::isnan(focus));
    EXPECT_FALSE(std::isinf(focus));
    EXPECT_GE(focus, 0.0f);
    EXPECT_LE(focus, 1.0f);

    attention_substrate_bridge_destroy(att_bridge);
    emotion_substrate_bridge_destroy(emo_bridge);
    executive_substrate_bridge_destroy(exec_bridge);
    reasoning_substrate_bridge_destroy(reas_bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Stability_HighTemperature_AllBridges) {
    // Test with extreme fever (45°C)
    neural_substrate_set_temperature(substrate, 45.0f);
    neural_substrate_set_atp_level(substrate, 0.5f);

    auto* attention = create_mock_attention();
    auto* emotion = create_mock_emotion();
    auto* wm = create_mock_working_memory();

    attention_substrate_config_t att_config;
    attention_substrate_default_config(&att_config);
    att_config.enable_bio_async = false;

    emotion_substrate_config_t emo_config;
    emotion_substrate_default_config(&emo_config);
    emo_config.enable_bio_async = false;

    wm_substrate_config_t wm_config;
    wm_substrate_default_config(&wm_config);
    wm_config.enable_bio_async = false;

    auto* att_bridge = attention_substrate_bridge_create(&att_config, substrate, attention);
    auto* emo_bridge = emotion_substrate_bridge_create(&emo_config, emotion, substrate);
    auto* wm_bridge = wm_substrate_bridge_create(&wm_config, wm, substrate);

    ASSERT_NE(att_bridge, nullptr);
    ASSERT_NE(emo_bridge, nullptr);
    ASSERT_NE(wm_bridge, nullptr);

    // Update all
    EXPECT_EQ(0, attention_substrate_update(att_bridge));
    EXPECT_EQ(0, emotion_substrate_update(emo_bridge));
    EXPECT_EQ(0, wm_substrate_update(wm_bridge));

    // Check for numerical stability
    float shifting = attention_substrate_get_shifting_efficiency(att_bridge);
    float intensity = emotion_substrate_get_intensity_mod(emo_bridge);
    float decay = wm_substrate_get_decay_rate_mod(wm_bridge);

    EXPECT_FALSE(std::isnan(shifting));
    EXPECT_FALSE(std::isnan(intensity));
    EXPECT_FALSE(std::isnan(decay));
    EXPECT_FALSE(std::isinf(shifting));
    EXPECT_FALSE(std::isinf(intensity));
    EXPECT_FALSE(std::isinf(decay));

    attention_substrate_bridge_destroy(att_bridge);
    emotion_substrate_bridge_destroy(emo_bridge);
    wm_substrate_bridge_destroy(wm_bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Stability_RapidATPFluctuations) {
    // Rapidly change ATP levels, test stability
    auto* executive = create_mock_executive();
    executive_substrate_config_t config;
    executive_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = executive_substrate_bridge_create(&config, executive, substrate);
    ASSERT_NE(bridge, nullptr);

    float atp_levels[] = {0.9f, 0.1f, 0.5f, 0.0f, 1.0f, 0.3f, 0.7f, 0.2f};

    for (int i = 0; i < 8; i++) {
        neural_substrate_set_atp_level(substrate, atp_levels[i]);
        EXPECT_EQ(0, executive_substrate_update(bridge));

        float decision = executive_substrate_get_decision_quality(bridge);
        EXPECT_FALSE(std::isnan(decision));
        EXPECT_GE(decision, 0.0f);
        EXPECT_LE(decision, 1.0f);
    }

    executive_substrate_bridge_destroy(bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Stability_ConcurrentUpdates_SingleBridge) {
    // Test thread safety with concurrent updates
    auto* attention = create_mock_attention();
    attention_substrate_config_t config;
    attention_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = attention_substrate_bridge_create(&config, substrate, attention);
    ASSERT_NE(bridge, nullptr);

    const int NUM_THREADS = 4;
    const int UPDATES_PER_THREAD = 100;

    auto update_worker = [&]() {
        for (int i = 0; i < UPDATES_PER_THREAD; i++) {
            attention_substrate_update(bridge);
            float focus = attention_substrate_get_focus_capacity(bridge);
            EXPECT_FALSE(std::isnan(focus));
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(update_worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    attention_substrate_bridge_destroy(bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Stability_ExtremeSensitivitySettings) {
    // Test with extreme sensitivity parameters
    auto* emotion = create_mock_emotion();
    emotion_substrate_config_t config;
    emotion_substrate_default_config(&config);
    config.enable_bio_async = false;

    // Set extreme sensitivities
    config.atp_sensitivity = 0.0f;
    config.neurotransmitter_sensitivity = 1.0f;
    config.temperature_sensitivity = 1.0f;

    auto* bridge = emotion_substrate_bridge_create(&config, emotion, substrate);
    ASSERT_NE(bridge, nullptr);

    // Update should work
    EXPECT_EQ(0, emotion_substrate_update(bridge));

    // Results should be valid
    float intensity = emotion_substrate_get_intensity_mod(bridge);
    EXPECT_FALSE(std::isnan(intensity));
    EXPECT_GE(intensity, 0.0f);

    emotion_substrate_bridge_destroy(bridge);
}

/* ============================================================================
 * CATEGORY 4: Scaling Tests (3 tests)
 * ========================================================================== */

TEST_F(CognitiveSubstrateBridgesRegressionTest, Scaling_ManyAttentionBridges) {
    auto* attention = create_mock_attention();
    attention_substrate_config_t config;
    attention_substrate_default_config(&config);
    config.enable_bio_async = false;

    std::vector<attention_substrate_bridge_t*> bridges;
    bridges.reserve(SCALING_TEST_BRIDGES);

    // Create many bridges
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < SCALING_TEST_BRIDGES; i++) {
        auto* bridge = attention_substrate_bridge_create(&config, substrate, attention);
        ASSERT_NE(bridge, nullptr);
        bridges.push_back(bridge);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto create_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Update all bridges
    start = std::chrono::high_resolution_clock::now();
    for (auto* bridge : bridges) {
        attention_substrate_update(bridge);
    }
    end = std::chrono::high_resolution_clock::now();
    auto update_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Cleanup
    for (auto* bridge : bridges) {
        attention_substrate_bridge_destroy(bridge);
    }

    std::cout << "Scaling Test (" << SCALING_TEST_BRIDGES << " attention bridges):\n";
    std::cout << "  Create all: " << create_time << " us\n";
    std::cout << "  Update all: " << update_time << " us\n";
    std::cout << "  Avg create: " << (create_time / SCALING_TEST_BRIDGES) << " us/bridge\n";
    std::cout << "  Avg update: " << (update_time / SCALING_TEST_BRIDGES) << " us/bridge\n";

    // Should scale reasonably (not exponential)
    EXPECT_LT(create_time / SCALING_TEST_BRIDGES, 1000) << "Avg create time should be < 1 ms";
    EXPECT_LT(update_time / SCALING_TEST_BRIDGES, 500) << "Avg update time should be < 500 us";
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Scaling_MixedBridgeTypes) {
    // Create mix of different bridge types
    auto* attention = create_mock_attention();
    auto* emotion = create_mock_emotion();
    auto* executive = create_mock_executive();
    auto* reasoning = create_mock_reasoning();

    attention_substrate_config_t att_config;
    attention_substrate_default_config(&att_config);
    att_config.enable_bio_async = false;

    emotion_substrate_config_t emo_config;
    emotion_substrate_default_config(&emo_config);
    emo_config.enable_bio_async = false;

    executive_substrate_config_t exec_config;
    executive_substrate_default_config(&exec_config);
    exec_config.enable_bio_async = false;

    reasoning_substrate_config_t reas_config;
    reasoning_substrate_default_config(&reas_config);
    reas_config.enable_bio_async = false;

    const int BRIDGES_PER_TYPE = 25;
    std::vector<void*> bridges;  // Type-erased for simplicity

    // Create mixed bridges
    for (int i = 0; i < BRIDGES_PER_TYPE; i++) {
        bridges.push_back(attention_substrate_bridge_create(&att_config, substrate, attention));
        bridges.push_back(emotion_substrate_bridge_create(&emo_config, emotion, substrate));
        bridges.push_back(executive_substrate_bridge_create(&exec_config, executive, substrate));
        bridges.push_back(reasoning_substrate_bridge_create(&reas_config, reasoning, substrate));
    }

    // All should be valid
    for (auto* bridge : bridges) {
        EXPECT_NE(bridge, nullptr);
    }

    // Cleanup (type-specific destroy)
    for (int i = 0; i < BRIDGES_PER_TYPE; i++) {
        attention_substrate_bridge_destroy((attention_substrate_bridge_t*)bridges[i * 4]);
        emotion_substrate_bridge_destroy((emotion_substrate_bridge_t*)bridges[i * 4 + 1]);
        executive_substrate_bridge_destroy((executive_substrate_bridge_t*)bridges[i * 4 + 2]);
        reasoning_substrate_bridge_destroy((reasoning_substrate_bridge_t*)bridges[i * 4 + 3]);
    }

    std::cout << "Scaling Test: Created " << (BRIDGES_PER_TYPE * 4) << " mixed bridges\n";
    SUCCEED();
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Scaling_UpdatePerformanceLinear) {
    // Verify update time scales linearly with number of bridges
    auto* executive = create_mock_executive();
    executive_substrate_config_t config;
    executive_substrate_default_config(&config);
    config.enable_bio_async = false;

    std::vector<int> bridge_counts = {10, 25, 50, 100};
    std::vector<double> avg_update_times;

    for (int count : bridge_counts) {
        std::vector<executive_substrate_bridge_t*> bridges;
        for (int i = 0; i < count; i++) {
            bridges.push_back(executive_substrate_bridge_create(&config, executive, substrate));
        }

        // Measure update time
        auto start = std::chrono::high_resolution_clock::now();
        for (auto* bridge : bridges) {
            executive_substrate_update(bridge);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double total_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        avg_update_times.push_back(total_us / count);

        for (auto* bridge : bridges) {
            executive_substrate_bridge_destroy(bridge);
        }
    }

    // Check linearity - avg time per bridge should be relatively constant
    double min_avg = *std::min_element(avg_update_times.begin(), avg_update_times.end());
    double max_avg = *std::max_element(avg_update_times.begin(), avg_update_times.end());

    std::cout << "Linearity Test:\n";
    for (size_t i = 0; i < bridge_counts.size(); i++) {
        std::cout << "  " << bridge_counts[i] << " bridges: " << avg_update_times[i] << " us/bridge\n";
    }

    // Max should not be more than 3x min (allowing for cache effects)
    EXPECT_LT(max_avg, min_avg * 3.0) << "Update time should scale linearly";
}

/* ============================================================================
 * CATEGORY 5: Correctness Regression (8 tests)
 * ========================================================================== */

TEST_F(CognitiveSubstrateBridgesRegressionTest, Correctness_AttentionBridge_LowATP_ImpairedFocus) {
    // BIOLOGICAL: Low ATP impairs attention focus capacity
    neural_substrate_set_atp_level(substrate, 0.2f);  // Below threshold

    auto* attention = create_mock_attention();
    attention_substrate_config_t config;
    attention_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = attention_substrate_bridge_create(&config, substrate, attention);
    ASSERT_NE(bridge, nullptr);

    attention_substrate_update(bridge);

    float focus = attention_substrate_get_focus_capacity(bridge);

    // Low ATP should significantly reduce focus
    EXPECT_LT(focus, 0.5f) << "Focus should be impaired with ATP=0.2";
    EXPECT_TRUE(attention_substrate_is_impaired(bridge));

    attention_substrate_bridge_destroy(bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Correctness_EmotionBridge_HighFever_BluntedEmotions) {
    // BIOLOGICAL: High fever reduces emotional intensity
    neural_substrate_set_temperature(substrate, 39.5f);  // High fever
    neural_substrate_set_atp_level(substrate, 0.7f);     // Normal ATP

    auto* emotion = create_mock_emotion();
    emotion_substrate_config_t config;
    emotion_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = emotion_substrate_bridge_create(&config, emotion, substrate);
    ASSERT_NE(bridge, nullptr);

    emotion_substrate_update(bridge);

    float intensity = emotion_substrate_get_intensity_mod(bridge);

    // High fever should blunt emotions (intensity < 1.0)
    EXPECT_LT(intensity, 1.0f) << "Fever should blunt emotional intensity";
    EXPECT_GT(intensity, 0.0f) << "Should not be completely suppressed";

    emotion_substrate_bridge_destroy(bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Correctness_ExecutiveBridge_ATPDepletion_WeakInhibition) {
    // BIOLOGICAL: Executive inhibition fails first under ATP depletion
    neural_substrate_set_atp_level(substrate, 0.35f);  // Below threshold

    auto* executive = create_mock_executive();
    executive_substrate_config_t config;
    executive_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = executive_substrate_bridge_create(&config, executive, substrate);
    ASSERT_NE(bridge, nullptr);

    executive_substrate_update(bridge);

    float inhibition = executive_substrate_get_inhibition_strength(bridge);
    float decision = executive_substrate_get_decision_quality(bridge);

    // Inhibition should be more impaired than decision quality
    EXPECT_LT(inhibition, 0.6f) << "Inhibition very sensitive to ATP";
    EXPECT_TRUE(executive_substrate_is_impaired(bridge));

    executive_substrate_bridge_destroy(bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Correctness_IntrospectionBridge_LowATP_ReducedAwareness) {
    // BIOLOGICAL: Self-awareness requires high metabolic support
    neural_substrate_set_atp_level(substrate, 0.25f);

    auto* introspection = create_mock_introspection();
    introspection_substrate_config_t config;
    introspection_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = introspection_substrate_bridge_create(&config, introspection, substrate);
    ASSERT_NE(bridge, nullptr);

    introspection_substrate_update(bridge);

    float awareness = introspection_substrate_get_self_awareness(bridge);

    // Low ATP should reduce self-awareness depth
    EXPECT_LT(awareness, 0.5f) << "Self-awareness impaired with low ATP";
    EXPECT_TRUE(introspection_substrate_is_impaired(bridge));

    introspection_substrate_bridge_destroy(bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Correctness_MemoryBridge_LowATP_SlowConsolidation) {
    // BIOLOGICAL: Memory consolidation requires ATP for protein synthesis
    neural_substrate_set_atp_level(substrate, 0.35f);

    auto* memory = create_mock_memory();
    consolidation_substrate_config_t config;
    consolidation_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = consolidation_substrate_bridge_create(&config, memory, substrate);
    ASSERT_NE(bridge, nullptr);

    consolidation_substrate_update(bridge);

    float consolidation = consolidation_substrate_get_consolidation_rate(bridge);
    float protein = consolidation_substrate_get_protein_synthesis_rate(bridge);

    // Low ATP should slow consolidation and protein synthesis
    EXPECT_LT(consolidation, 0.6f) << "Consolidation slowed with low ATP";
    EXPECT_LT(protein, 0.6f) << "Protein synthesis requires ATP";

    consolidation_substrate_bridge_destroy(bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Correctness_ReasoningBridge_LowATP_ShallowInference) {
    // BIOLOGICAL: Reasoning depth correlates with ATP availability
    neural_substrate_set_atp_level(substrate, 0.4f);

    auto* reasoning = create_mock_reasoning();
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = reasoning_substrate_bridge_create(&config, reasoning, substrate);
    ASSERT_NE(bridge, nullptr);

    reasoning_substrate_update(bridge);

    float depth = reasoning_substrate_get_inference_depth(bridge);

    // Low ATP should reduce inference chain depth
    EXPECT_LT(depth, 0.7f) << "Inference depth reduced with low ATP";

    reasoning_substrate_bridge_destroy(bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Correctness_ToMBridge_LowATP_ImpairedMentalizing) {
    // BIOLOGICAL: Mentalizing requires sustained prefrontal-TPJ activity
    neural_substrate_set_atp_level(substrate, 0.3f);

    auto* tom = create_mock_tom();
    tom_substrate_config_t config;
    tom_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = tom_substrate_bridge_create(&config, tom, substrate);
    ASSERT_NE(bridge, nullptr);

    tom_substrate_update(bridge);

    float mentalizing = tom_substrate_get_mentalizing_capacity(bridge);

    // Low ATP should impair mentalizing capacity
    EXPECT_LT(mentalizing, 0.6f) << "Mentalizing impaired with low ATP";
    EXPECT_TRUE(tom_substrate_is_impaired(bridge));

    tom_substrate_bridge_destroy(bridge);
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, Correctness_WorkingMemoryBridge_LowATP_ReducedCapacity) {
    // BIOLOGICAL: Working memory capacity drops from 7±2 to 3-4 items with low ATP
    neural_substrate_set_atp_level(substrate, 0.4f);

    auto* wm = create_mock_working_memory();
    wm_substrate_config_t config;
    wm_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = wm_substrate_bridge_create(&config, wm, substrate);
    ASSERT_NE(bridge, nullptr);

    wm_substrate_update(bridge);

    float capacity_factor = wm_substrate_get_capacity_factor(bridge);

    // Low ATP should reduce capacity factor
    EXPECT_LT(capacity_factor, 0.7f) << "WM capacity reduced with low ATP";

    wm_substrate_bridge_destroy(bridge);
}

/* ============================================================================
 * CATEGORY 6: Thread Safety Regression (2 tests)
 * ========================================================================== */

TEST_F(CognitiveSubstrateBridgesRegressionTest, ThreadSafety_SharedSubstrate_MultipleBridges) {
    // Multiple threads updating different bridges sharing same substrate
    auto* attention = create_mock_attention();
    auto* emotion = create_mock_emotion();
    auto* executive = create_mock_executive();

    attention_substrate_config_t att_config;
    attention_substrate_default_config(&att_config);
    att_config.enable_bio_async = false;

    emotion_substrate_config_t emo_config;
    emotion_substrate_default_config(&emo_config);
    emo_config.enable_bio_async = false;

    executive_substrate_config_t exec_config;
    executive_substrate_default_config(&exec_config);
    exec_config.enable_bio_async = false;

    auto* att_bridge = attention_substrate_bridge_create(&att_config, substrate, attention);
    auto* emo_bridge = emotion_substrate_bridge_create(&emo_config, emotion, substrate);
    auto* exec_bridge = executive_substrate_bridge_create(&exec_config, executive, substrate);

    const int ITERATIONS = 200;

    auto attention_worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            attention_substrate_update(att_bridge);
            float focus = attention_substrate_get_focus_capacity(att_bridge);
            (void)focus;
        }
    };

    auto emotion_worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            emotion_substrate_update(emo_bridge);
            float intensity = emotion_substrate_get_intensity_mod(emo_bridge);
            (void)intensity;
        }
    };

    auto executive_worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            executive_substrate_update(exec_bridge);
            float decision = executive_substrate_get_decision_quality(exec_bridge);
            (void)decision;
        }
    };

    std::thread t1(attention_worker);
    std::thread t2(emotion_worker);
    std::thread t3(executive_worker);

    t1.join();
    t2.join();
    t3.join();

    attention_substrate_bridge_destroy(att_bridge);
    emotion_substrate_bridge_destroy(emo_bridge);
    executive_substrate_bridge_destroy(exec_bridge);

    SUCCEED();
}

TEST_F(CognitiveSubstrateBridgesRegressionTest, ThreadSafety_ConcurrentSubstrateModification) {
    // One thread modifies substrate while others read bridge effects
    auto* reasoning = create_mock_reasoning();
    reasoning_substrate_config_t config;
    reasoning_substrate_default_config(&config);
    config.enable_bio_async = false;

    auto* bridge = reasoning_substrate_bridge_create(&config, reasoning, substrate);
    ASSERT_NE(bridge, nullptr);

    const int ITERATIONS = 100;

    auto substrate_modifier = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            float atp = 0.3f + (i % 7) * 0.1f;
            neural_substrate_set_atp_level(substrate, atp);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    auto bridge_reader = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            reasoning_substrate_update(bridge);
            float depth = reasoning_substrate_get_inference_depth(bridge);
            EXPECT_FALSE(std::isnan(depth));
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    };

    std::thread t1(substrate_modifier);
    std::thread t2(bridge_reader);
    std::thread t3(bridge_reader);

    t1.join();
    t2.join();
    t3.join();

    reasoning_substrate_bridge_destroy(bridge);
    SUCCEED();
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
