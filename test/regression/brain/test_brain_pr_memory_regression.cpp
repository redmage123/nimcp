//=============================================================================
// test_brain_pr_memory_regression.cpp - PR Memory Brain Regression Tests
//=============================================================================
/**
 * @file test_brain_pr_memory_regression.cpp
 * @brief Regression and performance tests for brain-PR memory integration
 *
 * WHAT: Performance benchmarks and regression tests for PR memory operations
 * WHY:  Ensure optimizations don't break correctness and track performance
 * HOW:  Benchmark key operations, verify results against known baselines
 *
 * REGRESSION SCENARIOS:
 * 1. Initialization time benchmarks
 * 2. Tick/update performance
 * 3. Stats retrieval performance
 * 4. Memory usage tracking
 * 5. API backward compatibility
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/internal/nimcp_brain_pr_memory.h"
#include "nimcp.h"

// Forward declare types needed for accessor function returns
// (full headers have C11 _Atomic which C++ doesn't handle)
typedef struct z_ladder_struct* z_ladder_t;
typedef struct theta_gamma_manager_internal* theta_gamma_manager_t;
typedef struct entangle_graph_struct* entangle_graph_t;
}

//=============================================================================
// Performance Measurement Utilities
//=============================================================================

class PerformanceTimer {
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double stopMs() {
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
        return elapsed.count();
    }

    double stopUs() {
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed = end_time - start_time;
        return elapsed.count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_time;
};

//=============================================================================
// Test Fixture
//=============================================================================

class BrainPRMemoryRegressionTest : public ::testing::Test {
protected:
    PerformanceTimer timer;

    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }

    brain_t createTestBrain(bool enable_pr_memory = true, bool lazy = false) {
        brain_config_t config = {};
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        strncpy(config.task_name, "pr_regression_test", 63);
        config.enable_pr_memory = enable_pr_memory;
        config.lazy_pr_memory_init = lazy;
        return brain_create_custom(&config);
    }
};

//=============================================================================
// Regression Test: Initialization Performance
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, InitializationTime_WithPRMemory) {
    // WHAT: Benchmark brain creation with PR memory
    // WHY:  Track initialization overhead
    // HOW:  Measure time to create brain with PR memory

    const int NUM_ITERATIONS = 10;
    double total_time_ms = 0.0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        timer.start();
        brain_t brain = createTestBrain(true);
        double elapsed = timer.stopMs();

        ASSERT_NE(brain, nullptr);
        total_time_ms += elapsed;

        brain_destroy(brain);
    }

    double avg_time_ms = total_time_ms / NUM_ITERATIONS;

    // Regression check: initialization should take less than 50ms
    // (this is a generous limit to account for system variance)
    EXPECT_LT(avg_time_ms, 50.0)
        << "Average init time: " << avg_time_ms << "ms exceeds threshold";

    RecordProperty("avg_init_time_ms", avg_time_ms);
}

//=============================================================================
// Regression Test: Initialization Performance Without PR Memory
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, InitializationTime_WithoutPRMemory) {
    // WHAT: Benchmark brain creation without PR memory
    // WHY:  Measure baseline to compare overhead
    // HOW:  Measure time to create brain without PR memory

    const int NUM_ITERATIONS = 10;
    double total_time_ms = 0.0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        timer.start();
        brain_t brain = createTestBrain(false);
        double elapsed = timer.stopMs();

        ASSERT_NE(brain, nullptr);
        total_time_ms += elapsed;

        brain_destroy(brain);
    }

    double avg_time_ms = total_time_ms / NUM_ITERATIONS;

    RecordProperty("avg_init_time_without_pr_ms", avg_time_ms);
}

//=============================================================================
// Regression Test: Tick Performance
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, TickPerformance) {
    // WHAT: Benchmark PR memory tick performance
    // WHY:  Ensure tick doesn't become a bottleneck
    // HOW:  Measure time for many tick operations

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    const int NUM_TICKS = 10000;
    uint64_t time_us = 0;

    timer.start();
    for (int i = 0; i < NUM_TICKS; i++) {
        time_us += 1000;  // 1ms steps
        nimcp_brain_pr_memory_tick(brain, time_us);
    }
    double total_time_us = timer.stopUs();

    double avg_tick_us = total_time_us / NUM_TICKS;

    // Regression check: each tick should take less than 50us
    EXPECT_LT(avg_tick_us, 50.0)
        << "Average tick time: " << avg_tick_us << "us exceeds threshold";

    RecordProperty("avg_tick_time_us", avg_tick_us);
    RecordProperty("total_ticks", NUM_TICKS);

    brain_destroy(brain);
}

//=============================================================================
// Regression Test: Stats Retrieval Performance
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, StatsRetrievalPerformance) {
    // WHAT: Benchmark stats retrieval performance
    // WHY:  Stats are queried frequently, must be fast
    // HOW:  Measure time for many stats retrievals

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    const int NUM_QUERIES = 10000;
    brain_pr_memory_stats_t stats;

    timer.start();
    for (int i = 0; i < NUM_QUERIES; i++) {
        nimcp_brain_pr_memory_get_stats(brain, &stats);
    }
    double total_time_us = timer.stopUs();

    double avg_query_us = total_time_us / NUM_QUERIES;

    // Regression check: each query should take less than 10us
    EXPECT_LT(avg_query_us, 10.0)
        << "Average stats query time: " << avg_query_us << "us exceeds threshold";

    RecordProperty("avg_stats_query_us", avg_query_us);

    brain_destroy(brain);
}

//=============================================================================
// Regression Test: Consolidation Performance
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, ConsolidationPerformance) {
    // WHAT: Benchmark consolidation cycle performance
    // WHY:  Consolidation is CPU-intensive, track it
    // HOW:  Measure time for consolidation cycles

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    uint64_t time_us = 0;
    double total_consolidation_time_us = 0.0;
    int consolidation_count = 0;

    // Run enough ticks to trigger many consolidations
    for (int i = 0; i < 1000; i++) {
        time_us += 100000;  // 100ms steps (triggers consolidation each time)

        timer.start();
        bool triggered = nimcp_brain_pr_memory_tick(brain, time_us);
        double elapsed = timer.stopUs();

        if (triggered) {
            total_consolidation_time_us += elapsed;
            consolidation_count++;
        }
    }

    if (consolidation_count > 0) {
        double avg_consolidation_us = total_consolidation_time_us / consolidation_count;

        // Regression check: each consolidation should take less than 100us
        EXPECT_LT(avg_consolidation_us, 100.0)
            << "Average consolidation time: " << avg_consolidation_us << "us exceeds threshold";

        RecordProperty("avg_consolidation_time_us", avg_consolidation_us);
        RecordProperty("consolidation_count", consolidation_count);
    }

    brain_destroy(brain);
}

//=============================================================================
// Regression Test: Default Config Values (API Compatibility)
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, DefaultConfig_APICompatibility) {
    // WHAT: Verify default config values haven't changed unexpectedly
    // WHY:  API changes should be intentional, not accidental
    // HOW:  Check that default values match expected values

    brain_pr_memory_config_t config = brain_pr_memory_config_default();

    // These values are documented/expected - changing them is a regression
    EXPECT_EQ(config.z0_capacity, 9u) << "Z0 capacity changed (Miller's 7±2)";
    EXPECT_EQ(config.z1_capacity, 100u) << "Z1 capacity changed";
    EXPECT_EQ(config.z2_capacity, 10000u) << "Z2 capacity changed";
    EXPECT_EQ(config.z3_capacity, 100000u) << "Z3 capacity changed";

    EXPECT_FLOAT_EQ(config.theta_freq_hz, 6.0f) << "Theta frequency changed";
    EXPECT_FLOAT_EQ(config.gamma_freq_hz, 40.0f) << "Gamma frequency changed";

    EXPECT_EQ(config.max_entangle_nodes, 50000u) << "Max entangle nodes changed";
    EXPECT_EQ(config.max_entangle_edges, 200000u) << "Max entangle edges changed";
    EXPECT_FLOAT_EQ(config.auto_link_threshold, 0.6f) << "Auto link threshold changed";

    EXPECT_EQ(config.consolidation_interval_us, 100000u) << "Consolidation interval changed";
    EXPECT_TRUE(config.enable_phase_gating) << "Phase gating default changed";
    EXPECT_TRUE(config.enable_sleep_boost) << "Sleep boost default changed";
}

//=============================================================================
// Regression Test: Phase Window Ranges
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, PhaseWindows_CorrectRanges) {
    // WHAT: Verify phase window detection uses correct ranges
    // WHY:  Phase gating is biologically important, must be correct
    // HOW:  Check encoding (0-90°) and retrieval (180-270°) windows

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    // Collect samples of each window type
    int encoding_samples = 0;
    int retrieval_samples = 0;
    float min_encoding_phase = 360.0f;
    float max_encoding_phase = 0.0f;
    float min_retrieval_phase = 360.0f;
    float max_retrieval_phase = 0.0f;

    uint64_t time_us = 0;
    for (int i = 0; i < 1000; i++) {
        time_us += 5000;
        nimcp_brain_pr_memory_tick(brain, time_us);

        brain_pr_memory_stats_t stats;
        nimcp_brain_pr_memory_get_stats(brain, &stats);

        if (stats.is_encoding_window) {
            encoding_samples++;
            if (stats.current_theta_phase < min_encoding_phase)
                min_encoding_phase = stats.current_theta_phase;
            if (stats.current_theta_phase > max_encoding_phase)
                max_encoding_phase = stats.current_theta_phase;
        }

        if (stats.is_retrieval_window) {
            retrieval_samples++;
            if (stats.current_theta_phase < min_retrieval_phase)
                min_retrieval_phase = stats.current_theta_phase;
            if (stats.current_theta_phase > max_retrieval_phase)
                max_retrieval_phase = stats.current_theta_phase;
        }
    }

    // Verify encoding window is in [0, 90)
    if (encoding_samples > 0) {
        EXPECT_GE(min_encoding_phase, 0.0f) << "Encoding min phase incorrect";
        EXPECT_LT(max_encoding_phase, 90.0f) << "Encoding max phase incorrect";
    }

    // Verify retrieval window is in [180, 270)
    if (retrieval_samples > 0) {
        EXPECT_GE(min_retrieval_phase, 180.0f) << "Retrieval min phase incorrect";
        EXPECT_LT(max_retrieval_phase, 270.0f) << "Retrieval max phase incorrect";
    }

    brain_destroy(brain);
}

//=============================================================================
// Regression Test: Accessor Functions Return Types
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, AccessorFunctions_ReturnTypes) {
    // WHAT: Verify accessor functions return correct types
    // WHY:  API compatibility check
    // HOW:  Get each component, verify non-null

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    // Z-Ladder accessor
    z_ladder_t ladder = nimcp_brain_get_z_ladder(brain);
    ASSERT_NE(ladder, nullptr);

    // Theta-gamma accessor
    theta_gamma_manager_t tg = nimcp_brain_get_theta_gamma(brain);
    ASSERT_NE(tg, nullptr);

    // Verify phase through brain stats
    brain_pr_memory_stats_t stats;
    EXPECT_TRUE(nimcp_brain_pr_memory_get_stats(brain, &stats));
    EXPECT_GE(stats.current_theta_phase, 0.0f);
    EXPECT_LT(stats.current_theta_phase, 360.0f);

    // Entanglement accessor
    entangle_graph_t entangle = nimcp_brain_get_entanglement(brain);
    ASSERT_NE(entangle, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// Regression Test: Stats Structure Completeness
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, StatsStructure_Completeness) {
    // WHAT: Verify stats structure has all expected fields
    // WHY:  Ensure no fields were accidentally removed
    // HOW:  Get stats, check all fields are accessible

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    brain_pr_memory_stats_t stats;
    bool result = nimcp_brain_pr_memory_get_stats(brain, &stats);
    ASSERT_TRUE(result);

    // Z-Ladder counts
    (void)stats.z0_count;
    (void)stats.z1_count;
    (void)stats.z2_count;
    (void)stats.z3_count;

    // Transition counts
    (void)stats.total_promotions;
    (void)stats.total_demotions;
    (void)stats.total_evictions;

    // Theta-gamma state
    (void)stats.current_theta_phase;
    (void)stats.current_gamma_amplitude;
    (void)stats.is_encoding_window;
    (void)stats.is_retrieval_window;

    // Entanglement state
    (void)stats.entangle_node_count;
    (void)stats.entangle_edge_count;
    (void)stats.avg_node_degree;

    // Timing
    (void)stats.last_consolidation_us;

    brain_destroy(brain);
}

//=============================================================================
// Regression Test: Memory Leak Detection
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, MemoryLeak_CreateDestroyCycle) {
    // WHAT: Test for memory leaks in create/destroy cycle
    // WHY:  Memory leaks cause long-running system issues
    // HOW:  Create and destroy many brains, verify no accumulation

    const int NUM_CYCLES = 100;

    for (int i = 0; i < NUM_CYCLES; i++) {
        brain_t brain = createTestBrain(true);
        ASSERT_NE(brain, nullptr) << "Failed to create brain on cycle " << i;

        // Do some operations
        uint64_t time_us = 0;
        for (int j = 0; j < 10; j++) {
            time_us += 100000;
            nimcp_brain_pr_memory_tick(brain, time_us);
        }

        brain_destroy(brain);
    }

    // If we get here without OOM, no obvious leaks
    SUCCEED();
}

//=============================================================================
// Regression Test: Error Handling Stability
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, ErrorHandling_NoAssert) {
    // WHAT: Verify error conditions don't cause asserts/crashes
    // WHY:  Graceful error handling is essential
    // HOW:  Test various error conditions

    // NULL brain operations
    EXPECT_FALSE(nimcp_brain_pr_memory_init(NULL, NULL));
    EXPECT_FALSE(nimcp_brain_pr_memory_tick(NULL, 0));
    EXPECT_FALSE(nimcp_brain_pr_memory_is_initialized(NULL));
    EXPECT_EQ(nimcp_brain_get_z_ladder(NULL), nullptr);
    EXPECT_EQ(nimcp_brain_get_theta_gamma(NULL), nullptr);
    EXPECT_EQ(nimcp_brain_get_entanglement(NULL), nullptr);

    brain_t brain = createTestBrain(true);
    ASSERT_NE(brain, nullptr);

    // NULL stats pointer
    EXPECT_FALSE(nimcp_brain_pr_memory_get_stats(brain, NULL));
    EXPECT_FALSE(nimcp_brain_pr_memory_get_stats(NULL, NULL));

    // Destroy doesn't crash with NULL
    nimcp_brain_pr_memory_destroy(NULL);

    brain_destroy(brain);
}

//=============================================================================
// Regression Test: Config Struct Size
//=============================================================================

TEST_F(BrainPRMemoryRegressionTest, ConfigStruct_Size) {
    // WHAT: Track config struct size for binary compatibility
    // WHY:  Size changes can break ABI
    // HOW:  Record struct sizes

    size_t config_size = sizeof(brain_pr_memory_config_t);
    size_t stats_size = sizeof(brain_pr_memory_stats_t);

    RecordProperty("config_struct_size", config_size);
    RecordProperty("stats_struct_size", stats_size);

    // These aren't hard requirements, just tracking
    // If sizes change significantly, investigate why
}

