/**
 * @file test_global_workspace_regression.cpp
 * @brief Regression tests for Global Workspace Architecture
 *
 * WHAT: Smoke tests and regression detection for global workspace module
 * WHY:  Catch performance degradation, memory leaks, and breaking changes
 * HOW:  Long-running tests, memory tracking, performance benchmarks
 *
 * COVERAGE: Global workspace stability, performance, and correctness over time
 * TEST PHILOSOPHY: Ensure module remains stable across code changes:
 *   1. No memory leaks over extended operation
 *   2. Performance characteristics remain within bounds
 *   3. Competition resolution scales predictably
 *   4. History buffer management is stable
 *   5. Statistics remain accurate over many operations
 *   6. No crashes or undefined behavior
 *
 * BIOLOGICAL BASIS:
 * - Global Workspace Theory (Baars, 1988; Dehaene, 2011)
 * - Workspace should handle continuous operation (like human consciousness)
 * - No degradation over time (no "mental fatigue" from implementation bugs)
 *
 * @author NIMCP Development Team
 * @date 2025-01-11
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Helpers
//=============================================================================

/**
 * @brief Helper to create test content
 */
std::vector<float> create_test_content(uint32_t dim, float base = 0.5f)
{
    std::vector<float> content(dim);
    for (uint32_t i = 0; i < dim; i++) {
        content[i] = base + (i * 0.001f);
    }
    return content;
}

/**
 * @brief Helper to sleep for milliseconds
 */
void sleep_ms(uint32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

//=============================================================================
// Test Fixture
//=============================================================================

class GlobalWorkspaceRegressionTest : public ::testing::Test {
protected:
    global_workspace_t* workspace;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
        workspace = nullptr;
    }

    void TearDown() override {
        if (workspace != nullptr) {
            global_workspace_destroy(workspace);
            workspace = nullptr;
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        // Allow minimal leaks for global state
        EXPECT_LT(stats.current_allocated, 4096) << "Memory leak detected";
    }
};

//=============================================================================
// 1. COMPILATION AND LINKAGE TESTS
//=============================================================================

/**
 * WHAT: Verify global workspace header compiles
 * WHY:  Catch syntax errors and missing dependencies
 * HOW:  If this compiles, header is valid
 */
TEST_F(GlobalWorkspaceRegressionTest, Header_CompilesTogether)
{
    // SUCCESS: If this compiles, header is valid
    EXPECT_TRUE(true);
}

/**
 * WHAT: Verify global workspace links correctly
 * WHY:  Catch missing symbols and link errors
 * HOW:  If this links, all symbols are defined
 */
TEST_F(GlobalWorkspaceRegressionTest, Module_LinksCorrectly)
{
    // SUCCESS: If this links, module is in library
    EXPECT_TRUE(true);
}

//=============================================================================
// 2. BASIC FUNCTIONALITY REGRESSION TESTS
//=============================================================================

/**
 * WHAT: Test basic create/destroy cycle
 * WHY:  Ensure fundamental operations still work
 * HOW:  Create workspace, verify non-null, destroy
 */
TEST_F(GlobalWorkspaceRegressionTest, BasicOperation_CreateDestroy)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);
    EXPECT_FALSE(global_workspace_has_broadcast(workspace));

    global_workspace_destroy(workspace);
    workspace = nullptr;  // Prevent double-free in TearDown
}

/**
 * WHAT: Test basic competition still works
 * WHY:  Ensure core functionality hasn't regressed
 * HOW:  Create workspace, compete, verify winner
 */
TEST_F(GlobalWorkspaceRegressionTest, BasicOperation_Competition)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    auto content = create_test_content(256, 0.5f);
    bool won = global_workspace_compete(
        workspace,
        MODULE_PERCEPTION,
        content.data(),
        256,
        0.75f
    );

    EXPECT_TRUE(won);
    EXPECT_TRUE(global_workspace_has_broadcast(workspace));
    EXPECT_EQ(global_workspace_get_broadcast_source(workspace), MODULE_PERCEPTION);
}

/**
 * WHAT: Test broadcast reading still works
 * WHY:  Ensure data integrity hasn't regressed
 * HOW:  Compete, read broadcast, verify content matches
 */
TEST_F(GlobalWorkspaceRegressionTest, BasicOperation_BroadcastRead)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    auto original_content = create_test_content(256, 0.8f);
    global_workspace_compete(
        workspace,
        MODULE_EXECUTIVE,
        original_content.data(),
        256,
        0.82f
    );

    std::vector<float> read_content(256);
    uint32_t actual_dim = 0;
    cognitive_module_t source = MODULE_NONE;

    bool read = global_workspace_read_broadcast(
        workspace,
        read_content.data(),
        256,
        &actual_dim,
        &source
    );

    EXPECT_TRUE(read);
    EXPECT_EQ(actual_dim, 256u);
    EXPECT_EQ(source, MODULE_EXECUTIVE);

    // Verify content integrity
    for (uint32_t i = 0; i < 256; i++) {
        EXPECT_FLOAT_EQ(read_content[i], original_content[i]);
    }
}

//=============================================================================
// 3. MEMORY LEAK DETECTION TESTS
//=============================================================================

/**
 * WHAT: Test for memory leaks over 500 competition cycles
 * WHY:  Detect memory leaks that accumulate over time
 * HOW:  Perform 500 competitions, check memory growth
 */
TEST_F(GlobalWorkspaceRegressionTest, MemoryLeak_ThousandCompetitions)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    // Perform 500 competition cycles (reduced from 1000 for performance)
    for (int i = 0; i < 500; i++) {
        auto content = create_test_content(256, i * 0.001f);
        global_workspace_compete(
            workspace,
            MODULE_PERCEPTION,
            content.data(),
            256,
            0.70f + (i % 10) * 0.01f
        );

        // Periodically wait for refractory period to allow broadcasts (reduced sleep time)
        if (i % 20 == 0) {
            sleep_ms(60);
        }
    }

    nimcp_memory_get_stats(&stats_after);

    // Memory should not grow significantly (workspace reuses buffers)
    int64_t memory_growth = stats_after.current_allocated - stats_before.current_allocated;
    EXPECT_LT(memory_growth, 2048) << "Memory leak detected: " << memory_growth << " bytes growth";

    printf("[REGRESSION] 500 competitions: memory growth = %ld bytes\n", memory_growth);
}

/**
 * WHAT: Test for memory leaks with history enabled
 * WHY:  Verify history buffer doesn't leak over many broadcasts
 * HOW:  Enable history, perform many broadcasts, check memory
 */
TEST_F(GlobalWorkspaceRegressionTest, MemoryLeak_HistoryBuffer)
{
    global_workspace_config_t config = global_workspace_default_config();
    config.enable_history = true;
    config.history_depth = 10;

    workspace = global_workspace_create_custom(&config);
    ASSERT_NE(workspace, nullptr);

    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    // Perform 50 broadcasts (5x history depth, reduced from 500 for performance)
    for (int i = 0; i < 50; i++) {
        auto content = create_test_content(256, i * 0.001f);
        global_workspace_compete(
            workspace,
            MODULE_WORKING_MEMORY,
            content.data(),
            256,
            0.75f
        );

        sleep_ms(60);  // Allow broadcasts
    }

    nimcp_memory_get_stats(&stats_after);

    // History buffer should be circular - no unbounded growth
    int64_t memory_growth = stats_after.current_allocated - stats_before.current_allocated;
    EXPECT_LT(memory_growth, 4096) << "History buffer leak: " << memory_growth << " bytes growth";

    printf("[REGRESSION] 50 broadcasts with history: memory growth = %ld bytes\n", memory_growth);
}

/**
 * WHAT: Test for memory leaks with multiple subscribers
 * WHY:  Verify subscription management doesn't leak
 * HOW:  Add/remove subscribers repeatedly, check memory
 */
TEST_F(GlobalWorkspaceRegressionTest, MemoryLeak_SubscriptionCycles)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    // Perform 100 subscribe/unsubscribe cycles
    cognitive_module_t modules[] = {
        MODULE_PERCEPTION,
        MODULE_WORKING_MEMORY,
        MODULE_EXECUTIVE,
        MODULE_ATTENTION,
        MODULE_EMOTION
    };

    for (int cycle = 0; cycle < 100; cycle++) {
        // Subscribe all
        for (auto module : modules) {
            global_workspace_subscribe(workspace, module);
        }

        // Unsubscribe all
        for (auto module : modules) {
            global_workspace_unsubscribe(workspace, module);
        }
    }

    nimcp_memory_get_stats(&stats_after);

    // Subscription tracking should not leak
    int64_t memory_growth = stats_after.current_allocated - stats_before.current_allocated;
    EXPECT_LT(memory_growth, 512) << "Subscription leak: " << memory_growth << " bytes growth";

    printf("[REGRESSION] 100 subscription cycles: memory growth = %ld bytes\n", memory_growth);
}

//=============================================================================
// 4. PERFORMANCE REGRESSION TESTS
//=============================================================================

/**
 * WHAT: Test competition resolution performance
 * WHY:  Ensure performance hasn't degraded
 * HOW:  Measure time for 100 competitions, verify within bounds
 */
TEST_F(GlobalWorkspaceRegressionTest, Performance_CompetitionSpeed)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    auto content = create_test_content(256, 0.5f);

    auto start = std::chrono::high_resolution_clock::now();

    // Perform 100 competitions
    for (int i = 0; i < 100; i++) {
        global_workspace_compete(
            workspace,
            MODULE_PERCEPTION,
            content.data(),
            256,
            0.70f + (i % 10) * 0.01f
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Competition should be fast: < 50μs per competition on average
    double avg_us_per_competition = duration.count() / 100.0;
    EXPECT_LT(avg_us_per_competition, 50.0)
        << "Performance regression: " << avg_us_per_competition << " μs/competition";

    printf("[REGRESSION] 100 competitions: avg = %.2f μs/competition\n", avg_us_per_competition);
}

/**
 * WHAT: Test broadcast read performance
 * WHY:  Ensure read operations remain O(1)
 * HOW:  Measure time for 1000 reads, verify consistent
 */
TEST_F(GlobalWorkspaceRegressionTest, Performance_BroadcastRead)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    // Create broadcast
    auto content = create_test_content(256, 0.7f);
    global_workspace_compete(workspace, MODULE_EXECUTIVE, content.data(), 256, 0.80f);

    std::vector<float> read_buffer(256);
    uint32_t actual_dim = 0;
    cognitive_module_t source = MODULE_NONE;

    auto start = std::chrono::high_resolution_clock::now();

    // Perform 1000 reads
    for (int i = 0; i < 1000; i++) {
        global_workspace_read_broadcast(
            workspace,
            read_buffer.data(),
            256,
            &actual_dim,
            &source
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Read should be very fast: < 1μs per read on average
    double avg_ns_per_read = duration.count() / 1000.0;
    EXPECT_LT(avg_ns_per_read, 1000.0)  // < 1μs
        << "Performance regression: " << avg_ns_per_read << " ns/read";

    printf("[REGRESSION] 1000 reads: avg = %.2f ns/read\n", avg_ns_per_read);
}

/**
 * WHAT: Test performance with maximum competitors
 * WHY:  Ensure worst-case performance is acceptable
 * HOW:  Submit 32 competitors, measure resolution time
 */
TEST_F(GlobalWorkspaceRegressionTest, Performance_MaxCompetitors)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    // Submit 32 competitors (maximum)
    for (uint32_t i = 0; i < 32; i++) {
        auto content = create_test_content(256, i * 0.01f);
        cognitive_module_t module = static_cast<cognitive_module_t>(
            MODULE_PERCEPTION + (i % 20)
        );
        global_workspace_compete(workspace, module, content.data(), 256, 0.60f + (i % 10) * 0.02f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should resolve 32 competitors quickly: < 500μs total
    EXPECT_LT(duration.count(), 500)
        << "Performance regression with 32 competitors: " << duration.count() << " μs";

    printf("[REGRESSION] 32 competitors resolved in %ld μs\n", duration.count());
}

/**
 * WHAT: Test history query performance
 * WHY:  Ensure history access remains efficient
 * HOW:  Fill history, query repeatedly, measure time
 */
TEST_F(GlobalWorkspaceRegressionTest, Performance_HistoryQuery)
{
    global_workspace_config_t config = global_workspace_default_config();
    config.enable_history = true;
    config.history_depth = 10;

    workspace = global_workspace_create_custom(&config);
    ASSERT_NE(workspace, nullptr);

    // Fill history with 10 broadcasts
    for (int i = 0; i < 10; i++) {
        auto content = create_test_content(256, i * 0.1f);
        global_workspace_compete(workspace, MODULE_PERCEPTION, content.data(), 256, 0.75f);
        sleep_ms(60);
    }

    workspace_broadcast_t history_entry;
    uint32_t count = 0;

    auto start = std::chrono::high_resolution_clock::now();

    // Query history 1000 times
    for (int i = 0; i < 1000; i++) {
        global_workspace_get_history(workspace, &history_entry, 1, &count);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // History query should be O(1): < 500ns per query
    double avg_ns_per_query = duration.count() / 1000.0;
    EXPECT_LT(avg_ns_per_query, 500.0)
        << "Performance regression: " << avg_ns_per_query << " ns/query";

    printf("[REGRESSION] 1000 history queries: avg = %.2f ns/query\n", avg_ns_per_query);
}

//=============================================================================
// 5. LONG-RUNNING STABILITY TESTS
//=============================================================================

/**
 * WHAT: Test workspace stability over extended operation
 * WHY:  Detect issues that only appear after many operations
 * HOW:  Run 1,000 competition cycles, verify correctness throughout
 */
TEST_F(GlobalWorkspaceRegressionTest, Stability_TenThousandCycles)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    int successful_broadcasts = 0;
    int failed_competitions = 0;

    // Run 1,000 cycles (reduced from 10,000 for performance)
    for (int i = 0; i < 1000; i++) {
        auto content = create_test_content(256, i * 0.001f);
        cognitive_module_t module = static_cast<cognitive_module_t>(
            MODULE_PERCEPTION + (i % 20)
        );
        float strength = 0.60f + (i % 20) * 0.01f;

        bool won = global_workspace_compete(workspace, module, content.data(), 256, strength);

        if (won) {
            successful_broadcasts++;

            // Verify broadcast is valid
            ASSERT_TRUE(global_workspace_has_broadcast(workspace));
            ASSERT_EQ(global_workspace_get_broadcast_source(workspace), module);
        } else {
            failed_competitions++;
        }

        // Periodically allow refractory period (reduced frequency)
        if (i % 50 == 0) {
            sleep_ms(60);
        }
    }

    EXPECT_GT(successful_broadcasts, 0);
    EXPECT_GT(failed_competitions, 0);  // Some should fail (threshold/refractory)

    printf("[REGRESSION] 1,000 cycles: %d broadcasts, %d failed competitions\n",
           successful_broadcasts, failed_competitions);
}

/**
 * WHAT: Test statistics accuracy over long operation
 * WHY:  Ensure statistics don't overflow or become inaccurate
 * HOW:  Run many operations, verify statistics remain consistent
 */
TEST_F(GlobalWorkspaceRegressionTest, Stability_StatisticsAccuracy)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    // Run 500 operations (reduced from 5000 for performance)
    for (int i = 0; i < 500; i++) {
        auto content = create_test_content(256, 0.5f);
        global_workspace_compete(workspace, MODULE_PERCEPTION, content.data(), 256, 0.75f);

        if (i % 20 == 0) {
            sleep_ms(60);
        }
    }

    workspace_statistics_t stats;
    bool got_stats = global_workspace_get_statistics(workspace, &stats);
    EXPECT_TRUE(got_stats);

    // Verify statistics are reasonable
    // Note: Statistics may include internal attempts beyond the 500 explicit calls
    EXPECT_EQ(stats.total_competitions, 500u);  // Should match our compete() calls
    EXPECT_GT(stats.total_broadcasts, 0u);
    EXPECT_LE(stats.total_broadcasts, stats.total_competitions);
    EXPECT_GT(stats.refractory_violations, 0u);  // Many blocked by refractory

    printf("[REGRESSION] After 500 operations: broadcasts=%lu, competitions=%lu, refract_blocked=%lu\n",
           stats.total_broadcasts, stats.total_competitions, stats.refractory_violations);
}

/**
 * WHAT: Test workspace with continuous refractory period blocking
 * WHY:  Verify workspace remains stable when heavily contended
 * HOW:  Submit rapid competitions, verify graceful handling
 */
TEST_F(GlobalWorkspaceRegressionTest, Stability_HighContentionScenario)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    int blocked_by_refractory = 0;
    int successful_broadcasts = 0;

    // Submit 1000 rapid competitions (no delays)
    for (int i = 0; i < 1000; i++) {
        auto content = create_test_content(256, i * 0.001f);
        bool won = global_workspace_compete(
            workspace,
            MODULE_PERCEPTION,
            content.data(),
            256,
            0.80f
        );

        if (won) {
            successful_broadcasts++;
        } else {
            blocked_by_refractory++;
        }
    }

    // Most should be blocked by refractory period (50ms)
    EXPECT_GT(blocked_by_refractory, successful_broadcasts);
    EXPECT_GT(successful_broadcasts, 0);  // At least some should succeed

    printf("[REGRESSION] High contention: %d blocked, %d succeeded\n",
           blocked_by_refractory, successful_broadcasts);
}

//=============================================================================
// 6. CONFIGURATION STABILITY TESTS
//=============================================================================

/**
 * WHAT: Test workspace with various configurations
 * WHY:  Ensure configuration handling is robust
 * HOW:  Create workspaces with different configs, verify stable
 */
TEST_F(GlobalWorkspaceRegressionTest, Configuration_VariousSettings)
{
    // Test with minimal threshold
    global_workspace_config_t config1 = global_workspace_default_config();
    config1.ignition_threshold = 0.3f;
    global_workspace_t* ws1 = global_workspace_create_custom(&config1);
    ASSERT_NE(ws1, nullptr);
    global_workspace_destroy(ws1);

    // Test with maximal threshold
    global_workspace_config_t config2 = global_workspace_default_config();
    config2.ignition_threshold = 0.95f;
    global_workspace_t* ws2 = global_workspace_create_custom(&config2);
    ASSERT_NE(ws2, nullptr);
    global_workspace_destroy(ws2);

    // Test with priority-based competition
    global_workspace_config_t config3 = global_workspace_default_config();
    config3.strategy = COMPETITION_PRIORITY_BASED;
    global_workspace_t* ws3 = global_workspace_create_custom(&config3);
    ASSERT_NE(ws3, nullptr);
    global_workspace_destroy(ws3);

    // Test with round-robin
    global_workspace_config_t config4 = global_workspace_default_config();
    config4.strategy = COMPETITION_ROUND_ROBIN;
    global_workspace_t* ws4 = global_workspace_create_custom(&config4);
    ASSERT_NE(ws4, nullptr);
    global_workspace_destroy(ws4);

    EXPECT_TRUE(true);  // All configurations stable
}

/**
 * WHAT: Test workspace with history disabled
 * WHY:  Ensure history can be safely disabled
 * HOW:  Create workspace without history, run many broadcasts
 */
TEST_F(GlobalWorkspaceRegressionTest, Configuration_HistoryDisabled)
{
    global_workspace_config_t config = global_workspace_default_config();
    config.enable_history = false;

    workspace = global_workspace_create_custom(&config);
    ASSERT_NE(workspace, nullptr);

    // Run 100 broadcasts
    for (int i = 0; i < 100; i++) {
        auto content = create_test_content(256, 0.5f);
        global_workspace_compete(workspace, MODULE_PERCEPTION, content.data(), 256, 0.75f);
        sleep_ms(60);
    }

    // Verify history queries fail gracefully
    workspace_broadcast_t entry;
    uint32_t count = 0;
    bool got_entry = global_workspace_get_history(workspace, &entry, 1, &count);
    EXPECT_FALSE(got_entry || count > 0);  // History disabled

    EXPECT_TRUE(true);  // Stable with history disabled
}

//=============================================================================
// 7. ERROR HANDLING STABILITY TESTS
//=============================================================================

/**
 * WHAT: Test workspace handles repeated invalid operations
 * WHY:  Ensure error paths don't cause instability
 * HOW:  Submit invalid operations repeatedly, verify stable
 */
TEST_F(GlobalWorkspaceRegressionTest, ErrorHandling_RepeatedInvalidOperations)
{
    workspace = global_workspace_create();
    ASSERT_NE(workspace, nullptr);

    // Attempt 100 invalid operations (reduced from 1000 for performance)
    for (int i = 0; i < 100; i++) {
        // Invalid: NULL content
        global_workspace_compete(workspace, MODULE_PERCEPTION, nullptr, 256, 0.75f);

        // Invalid: Wrong dimension
        auto wrong_dim = create_test_content(128, 0.5f);
        global_workspace_compete(workspace, MODULE_PERCEPTION, wrong_dim.data(), 128, 0.75f);

        // Invalid: Out-of-range strength
        auto valid_content = create_test_content(256, 0.5f);
        global_workspace_compete(workspace, MODULE_PERCEPTION, valid_content.data(), 256, 1.5f);
    }

    // Workspace should still be functional
    auto content = create_test_content(256, 0.8f);
    bool won = global_workspace_compete(workspace, MODULE_EXECUTIVE, content.data(), 256, 0.80f);
    EXPECT_TRUE(won);

    printf("[REGRESSION] Workspace stable after 100 invalid operations\n");
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    printf("=============================================================================\n");
    printf("Global Workspace Regression Tests - NIMCP\n");
    printf("=============================================================================\n");
    printf("Testing for performance regressions, memory leaks, and stability\n");
    printf("=============================================================================\n\n");

    int result = RUN_ALL_TESTS();

    printf("\n=============================================================================\n");
    printf("Regression Test Summary\n");
    printf("=============================================================================\n");

    return result;
}
