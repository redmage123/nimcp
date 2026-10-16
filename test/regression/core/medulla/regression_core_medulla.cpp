/**
 * @file regression_core_medulla.cpp
 * @brief Comprehensive Regression Tests for the Medulla Oblongata Module
 *
 * WHAT: Regression tests ensuring stability, thread-safety, and backward compatibility
 * WHY:  Prevent regressions in arousal, protection, circadian behavior, and memory safety
 * HOW:  Test known edge cases, boundary conditions, race conditions, and historical bugs
 *
 * TEST CATEGORIES:
 * 1. RACE      - Race condition regression (concurrent reads/writes)
 * 2. STATE     - State consistency regression (arousal/protection/circadian alignment)
 * 3. MEMORY    - Memory regression (leak detection, create/destroy cycles)
 * 4. PERF      - Performance regression (update timing, start/stop timing)
 * 5. BOUNDARY  - Boundary condition regression (extreme values, edge cases)
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 2.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <random>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_medulla.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr int RACE_TEST_THREAD_COUNT = 10;
static constexpr int RACE_TEST_ITERATIONS = 100;
static constexpr int MEMORY_TEST_ITERATIONS = 50;
static constexpr int STABILITY_TEST_ITERATIONS = 500;
static constexpr float FLOAT_TOLERANCE = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

class MedullaRegressionTest : public ::testing::Test {
protected:
    medulla_t medulla = nullptr;

    void SetUp() override {
        medulla_config_t config = medulla_default_config();
        medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr);
    }

    void TearDown() override {
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
    }
};

//=============================================================================
// 1. RACE CONDITION REGRESSION TESTS
//=============================================================================

/**
 * @test NoRaceOnConcurrentArousalReads
 * @brief Verify no race conditions with many concurrent readers
 *
 * REGRESSION: Previous versions had potential data races when multiple threads
 * read arousal level simultaneously. This test verifies thread-safety of read
 * operations.
 */
TEST_F(MedullaRegressionTest, NoRaceOnConcurrentArousalReads) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    std::atomic<bool> running{true};
    std::atomic<int> total_reads{0};
    std::atomic<int> invalid_reads{0};

    // Create multiple reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < RACE_TEST_THREAD_COUNT; i++) {
        readers.emplace_back([&]() {
            while (running) {
                float arousal = medulla_get_arousal_level(medulla);
                total_reads++;

                // Check for invalid values (data corruption)
                if (arousal < -0.01f || arousal > 1.01f || std::isnan(arousal)) {
                    invalid_reads++;
                }
            }
        });
    }

    // Let readers run while main thread does updates
    for (int i = 0; i < RACE_TEST_ITERATIONS; i++) {
        medulla_update(medulla, 0.016f);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    running = false;
    for (auto& t : readers) {
        t.join();
    }

    // Should have no invalid reads (race condition symptom)
    EXPECT_EQ(invalid_reads.load(), 0)
        << "Found " << invalid_reads.load() << " invalid reads out of "
        << total_reads.load() << " total (possible race condition)";
    EXPECT_GT(total_reads.load(), 0);
}

/**
 * @test NoRaceOnConcurrentModification
 * @brief Verify no race with concurrent boost/reduce operations
 *
 * REGRESSION: Previous versions had race conditions when multiple threads
 * modified arousal simultaneously. This test verifies atomicity of updates.
 */
TEST_F(MedullaRegressionTest, NoRaceOnConcurrentModification) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    std::atomic<bool> running{true};
    std::atomic<int> boost_errors{0};
    std::atomic<int> reduce_errors{0};

    // Create boost threads
    std::vector<std::thread> boosters;
    for (int i = 0; i < RACE_TEST_THREAD_COUNT / 2; i++) {
        boosters.emplace_back([&]() {
            while (running) {
                int result = medulla_boost_arousal(medulla, 0.01f);
                if (result != NIMCP_SUCCESS && result != NIMCP_ERROR_INVALID_PARAM) {
                    boost_errors++;
                }
                std::this_thread::yield();
            }
        });
    }

    // Create reduce threads
    std::vector<std::thread> reducers;
    for (int i = 0; i < RACE_TEST_THREAD_COUNT / 2; i++) {
        reducers.emplace_back([&]() {
            while (running) {
                int result = medulla_reduce_arousal(medulla, 0.01f);
                if (result != NIMCP_SUCCESS && result != NIMCP_ERROR_INVALID_PARAM) {
                    reduce_errors++;
                }
                std::this_thread::yield();
            }
        });
    }

    // Run concurrent modifications
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : boosters) {
        t.join();
    }
    for (auto& t : reducers) {
        t.join();
    }

    // Should have no unexpected errors
    EXPECT_EQ(boost_errors.load(), 0);
    EXPECT_EQ(reduce_errors.load(), 0);

    // Arousal should still be valid
    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
    EXPECT_FALSE(std::isnan(arousal));
}

/**
 * @test StableUnderLoad
 * @brief Run many updates and verify stability under sustained load
 *
 * REGRESSION: Some numerical instabilities appeared only after prolonged
 * operation. This test verifies stability over extended periods.
 */
TEST_F(MedullaRegressionTest, StableUnderLoad) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> dt_dist(0.001f, 0.1f);
    std::uniform_real_distribution<float> delta_dist(0.01f, 0.1f);

    for (int i = 0; i < STABILITY_TEST_ITERATIONS; i++) {
        // Random delta time
        float dt = dt_dist(rng);
        int update_result = medulla_update(medulla, dt);
        EXPECT_EQ(update_result, NIMCP_SUCCESS);

        // Random arousal modifications
        if (i % 3 == 0) {
            medulla_boost_arousal(medulla, delta_dist(rng));
        } else if (i % 3 == 1) {
            medulla_reduce_arousal(medulla, delta_dist(rng));
        }

        // Verify state remains valid
        medulla_stats_t stats;
        ASSERT_EQ(medulla_get_stats(medulla, &stats), NIMCP_SUCCESS);

        EXPECT_GE(stats.current_arousal, 0.0f);
        EXPECT_LE(stats.current_arousal, 1.0f);
        EXPECT_FALSE(std::isnan(stats.current_arousal));
        EXPECT_FALSE(std::isnan(stats.avg_arousal));
        EXPECT_FALSE(std::isnan(stats.circadian_time_hours));
    }

    // Final state check
    medulla_stats_t final_stats;
    medulla_get_stats(medulla, &final_stats);
    EXPECT_GE(final_stats.total_updates, (uint64_t)STABILITY_TEST_ITERATIONS);
}

//=============================================================================
// 2. STATE CONSISTENCY REGRESSION TESTS
//=============================================================================

/**
 * @test ArousalLevelConsistent
 * @brief Verify arousal level enum matches float value
 *
 * REGRESSION: Arousal level enum and float value could become inconsistent
 * after certain state transitions.
 */
TEST_F(MedullaRegressionTest, ArousalLevelConsistent) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    struct ArousalTest {
        float arousal_value;
        arousal_level_t min_expected;
        arousal_level_t max_expected;
    };

    ArousalTest tests[] = {
        {0.05f, AROUSAL_LEVEL_COMA, AROUSAL_LEVEL_DEEP_SLEEP},
        {0.2f, AROUSAL_LEVEL_DEEP_SLEEP, AROUSAL_LEVEL_LIGHT_SLEEP},
        {0.35f, AROUSAL_LEVEL_LIGHT_SLEEP, AROUSAL_LEVEL_DROWSY},
        {0.5f, AROUSAL_LEVEL_DROWSY, AROUSAL_LEVEL_ALERT},
        {0.6f, AROUSAL_LEVEL_AWAKE, AROUSAL_LEVEL_ALERT},
        {0.75f, AROUSAL_LEVEL_ALERT, AROUSAL_LEVEL_HYPERAROUSAL},
        {0.95f, AROUSAL_LEVEL_HYPERAROUSAL, AROUSAL_LEVEL_HYPERAROUSAL},
    };

    for (const auto& test : tests) {
        // Force arousal to specific value
        medulla_test_set_arousal(medulla, test.arousal_value);

        // Get stats and verify consistency
        medulla_stats_t stats;
        medulla_get_stats(medulla, &stats);

        EXPECT_NEAR(stats.current_arousal, test.arousal_value, FLOAT_TOLERANCE)
            << "Arousal value mismatch for " << test.arousal_value;

        EXPECT_GE((int)stats.arousal_level, (int)test.min_expected)
            << "Arousal level too low for value " << test.arousal_value;
        EXPECT_LE((int)stats.arousal_level, (int)test.max_expected)
            << "Arousal level too high for value " << test.arousal_value;
    }
}

/**
 * @test ProtectionEscalationConsistent
 * @brief Verify protection escalation follows proper sequence
 *
 * REGRESSION: Protection levels could skip intermediate states during rapid
 * escalation, violating the state machine invariants.
 */
TEST_F(MedullaRegressionTest, ProtectionEscalationConsistent) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Test all protection levels in sequence
    protection_level_t levels[] = {
        PROTECTION_LEVEL_NORMAL,
        PROTECTION_LEVEL_CAUTIOUS,
        PROTECTION_LEVEL_GUARDED,
        PROTECTION_LEVEL_DEFENSIVE,
        PROTECTION_LEVEL_CRITICAL,
        PROTECTION_LEVEL_SHUTDOWN
    };

    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(medulla_test_set_protection(medulla, levels[i]), 0)
            << "Failed to set protection level " << i;

        protection_level_t actual = medulla_get_protection_level(medulla);
        EXPECT_EQ(actual, levels[i])
            << "Protection level mismatch at index " << i;
    }

    // Test de-escalation
    for (int i = 5; i >= 0; i--) {
        ASSERT_EQ(medulla_test_set_protection(medulla, levels[i]), 0);
        EXPECT_EQ(medulla_get_protection_level(medulla), levels[i]);
    }
}

/**
 * @test CircadianProgressionCorrect
 * @brief Verify circadian phases progress correctly through 24-hour cycle
 *
 * REGRESSION: Circadian phase could get stuck or skip phases during time
 * progression.
 */
TEST_F(MedullaRegressionTest, CircadianProgressionCorrect) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Start at early morning
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_EARLY_MORNING);

    medulla_stats_t prev_stats;
    medulla_get_stats(medulla, &prev_stats);

    // Simulate 24+ hours with 1-hour steps
    // Each phase is 3 hours, so 24 hours = 8 phases
    float hours_per_step = 1.0f;
    float seconds_per_step = hours_per_step * 3600.0f;

    int phase_changes = 0;
    circadian_phase_t prev_phase = medulla_get_circadian_phase(medulla);

    for (int hour = 0; hour < 26; hour++) {
        medulla_update(medulla, seconds_per_step);

        circadian_phase_t current_phase = medulla_get_circadian_phase(medulla);

        // Check for phase change
        if (current_phase != prev_phase) {
            phase_changes++;
            prev_phase = current_phase;
        }

        // Phase should always be valid
        EXPECT_GE((int)current_phase, 0);
        EXPECT_LT((int)current_phase, 8);

        // Get stats
        medulla_stats_t stats;
        medulla_get_stats(medulla, &stats);

        // Circadian time should be valid
        EXPECT_GE(stats.circadian_time_hours, 0.0f);
        EXPECT_LT(stats.circadian_time_hours, 24.0f);
    }

    // Should have gone through multiple phase changes
    EXPECT_GE(phase_changes, 7) << "Expected at least 7 phase changes in 26 hours";

    // Should have completed at least one full cycle
    medulla_stats_t final_stats;
    medulla_get_stats(medulla, &final_stats);
    EXPECT_GE(final_stats.circadian_cycles, 1u);
}

//=============================================================================
// 3. MEMORY REGRESSION TESTS
//=============================================================================

/**
 * @test NoMemoryLeakOnCreateDestroy
 * @brief Create and destroy medulla many times to detect memory leaks
 *
 * REGRESSION: Memory leaks were found in early versions during repeated
 * create/destroy cycles. This test helps detect such issues.
 */
TEST_F(MedullaRegressionTest, NoMemoryLeakOnCreateDestroy) {
    // First, clean up fixture's medulla
    if (medulla) {
        medulla_stop(medulla);
        medulla_destroy(medulla);
        medulla = nullptr;
    }

    // Create and destroy many times
    for (int i = 0; i < MEMORY_TEST_ITERATIONS; i++) {
        medulla_config_t config = medulla_default_config();
        medulla_t m = medulla_create(&config);
        ASSERT_NE(m, nullptr) << "Create failed at iteration " << i;

        // Start, do some work, stop
        EXPECT_EQ(medulla_start(m), NIMCP_SUCCESS);

        for (int j = 0; j < 10; j++) {
            medulla_update(m, 0.016f);
        }

        EXPECT_EQ(medulla_stop(m), NIMCP_SUCCESS);
        medulla_destroy(m);
    }

    // If we get here without crashing or OOM, test passes
    // Note: For actual leak detection, run with valgrind/asan
    SUCCEED();
}

/**
 * @test NoMemoryLeakOnRestart
 * @brief Start/stop many times to detect memory leaks on restart
 *
 * REGRESSION: Memory leaks during restart cycles due to incomplete cleanup.
 */
TEST_F(MedullaRegressionTest, NoMemoryLeakOnRestart) {
    for (int i = 0; i < MEMORY_TEST_ITERATIONS; i++) {
        // Start
        ASSERT_EQ(medulla_start(medulla), NIMCP_SUCCESS)
            << "Start failed at iteration " << i;

        // Some operations
        for (int j = 0; j < 5; j++) {
            medulla_update(medulla, 0.016f);
            medulla_boost_arousal(medulla, 0.05f);
        }

        // Stop
        ASSERT_EQ(medulla_stop(medulla), NIMCP_SUCCESS)
            << "Stop failed at iteration " << i;
    }

    // Verify final state is clean
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);
}

//=============================================================================
// 4. PERFORMANCE REGRESSION TESTS
//=============================================================================

/**
 * @test UpdatePerformance
 * @brief Verify update completes within time budget
 *
 * REGRESSION: Update function became slower due to added features. This test
 * ensures performance stays within acceptable bounds.
 */
TEST_F(MedullaRegressionTest, UpdatePerformance) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Warm up
    for (int i = 0; i < 100; i++) {
        medulla_update(medulla, 0.016f);
    }

    // Measure update time
    auto start = std::chrono::high_resolution_clock::now();

    const int NUM_UPDATES = 1000;
    for (int i = 0; i < NUM_UPDATES; i++) {
        medulla_update(medulla, 0.016f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    float avg_us = (float)duration_us / NUM_UPDATES;

    // Update should complete in less than 1ms (1000us) on average
    // Allow more time for CI environments
    EXPECT_LT(avg_us, 1000.0f)
        << "Average update time " << avg_us << "us exceeds 1ms budget";

    // Verify stats tracking
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_GT(stats.avg_update_time_us, 0.0f);
}

/**
 * @test StartStopPerformance
 * @brief Verify start/stop operations are fast
 *
 * REGRESSION: Start/stop became slow due to subsystem initialization overhead.
 */
TEST_F(MedullaRegressionTest, StartStopPerformance) {
    // Stop fixture medulla for clean measurement
    medulla_stop(medulla);

    auto start_time = std::chrono::high_resolution_clock::now();

    const int NUM_CYCLES = 20;
    for (int i = 0; i < NUM_CYCLES; i++) {
        ASSERT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
        ASSERT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    float avg_ms = (float)duration_ms / NUM_CYCLES;

    // Start/stop cycle should complete in less than 50ms on average
    EXPECT_LT(avg_ms, 50.0f)
        << "Average start/stop cycle time " << avg_ms << "ms exceeds 50ms budget";
}

//=============================================================================
// 5. BOUNDARY CONDITION REGRESSION TESTS
//=============================================================================

TEST_F(MedullaRegressionTest, ZeroDeltaTimeUpdate) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Zero delta time should not crash
    int result = medulla_update(medulla, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MedullaRegressionTest, VerySmallDeltaTime) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Very small delta should work
    for (int i = 0; i < 100; i++) {
        int result = medulla_update(medulla, 0.0001f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(MedullaRegressionTest, LargeDeltaTime) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Large delta time should work without overflow
    int result = medulla_update(medulla, 10.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Even larger delta
    result = medulla_update(medulla, 100.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify state is still valid
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_GE(stats.circadian_time_hours, 0.0f);
    EXPECT_LT(stats.circadian_time_hours, 24.0f);
}

TEST_F(MedullaRegressionTest, StatsConsistencyAfterUpdates) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats_before;
    medulla_get_stats(medulla, &stats_before);

    // Run updates
    for (int i = 0; i < 100; i++) {
        medulla_update(medulla, 0.016f);
    }

    medulla_stats_t stats_after;
    medulla_get_stats(medulla, &stats_after);

    // Stats should be monotonically increasing
    EXPECT_GE(stats_after.total_updates, stats_before.total_updates + 100);
    EXPECT_GE(stats_after.uptime_ms, stats_before.uptime_ms);
}

TEST_F(MedullaRegressionTest, ProtectionLevelNeverNegative) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    for (int i = 0; i < 50; i++) {
        medulla_update(medulla, 0.1f);
        protection_level_t level = medulla_get_protection_level(medulla);
        EXPECT_GE((int)level, 0);
        EXPECT_LE((int)level, (int)PROTECTION_LEVEL_SHUTDOWN);
    }
}

TEST_F(MedullaRegressionTest, CircadianPhaseAlwaysValid) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    for (int i = 0; i < 100; i++) {
        medulla_update(medulla, 0.5f);  // Larger steps to advance circadian
        circadian_phase_t phase = medulla_get_circadian_phase(medulla);
        EXPECT_GE((int)phase, 0);
        EXPECT_LT((int)phase, 8);
    }
}

//=============================================================================
// 6. STATE TRANSITION REGRESSION TESTS
//=============================================================================

TEST_F(MedullaRegressionTest, NoStateCorruptionOnEmergency) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Get pre-emergency state
    medulla_stats_t stats_before;
    medulla_get_stats(medulla, &stats_before);

    // Trigger emergency
    medulla_emergency_shutdown(medulla, "regression test");

    // Get post-emergency state
    medulla_stats_t stats_after;
    medulla_get_stats(medulla, &stats_after);

    // State should still be valid
    EXPECT_GE((int)stats_after.state, 0);
    EXPECT_LE((int)stats_after.state, (int)MEDULLA_STATE_STOPPING);

    // Emergency count should increment
    EXPECT_GE(stats_after.emergency_shutdowns, 1u);
}

TEST_F(MedullaRegressionTest, RecoveryFromDegradedState) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Go to degraded
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_DEGRADED), NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_DEGRADED);

    // Recover to running
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_RUNNING), NIMCP_SUCCESS);

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);
}

//=============================================================================
// 7. MEMORY SAFETY REGRESSION TESTS
//=============================================================================

TEST_F(MedullaRegressionTest, MultipleCreateDestroy) {
    // Multiple create/destroy cycles shouldn't leak memory
    for (int i = 0; i < 10; i++) {
        medulla_config_t config = medulla_default_config();
        medulla_t m = medulla_create(&config);
        ASSERT_NE(m, nullptr);

        medulla_start(m);
        medulla_update(m, 0.016f);
        medulla_stop(m);
        medulla_destroy(m);
    }
}

TEST_F(MedullaRegressionTest, NullPointerSafety) {
    // All functions should handle null gracefully and return non-success error codes.
    // NIMCP error codes are positive integers (e.g. 1003 = NIMCP_ERROR_NULL_POINTER),
    // so we check != NIMCP_SUCCESS (0) rather than < 0.
    EXPECT_NE(medulla_start(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(medulla_stop(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(medulla_update(nullptr, 0.016f), NIMCP_SUCCESS);
    EXPECT_NE(medulla_emergency_shutdown(nullptr, "test"), NIMCP_SUCCESS);
    EXPECT_NE(medulla_request_state_change(nullptr, MEDULLA_STATE_RUNNING), NIMCP_SUCCESS);

    medulla_stats_t stats;
    EXPECT_NE(medulla_get_stats(nullptr, &stats), NIMCP_SUCCESS);

    // These should return safe defaults
    EXPECT_FALSE(medulla_is_bio_async_connected(nullptr));

    medulla_destroy(nullptr);  // Should not crash
}

//=============================================================================
// 8. AROUSAL STATE REGRESSION TESTS
//=============================================================================

TEST_F(MedullaRegressionTest, ArousalBoundedZeroToOne) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    for (int i = 0; i < 100; i++) {
        medulla_update(medulla, 0.1f);

        medulla_stats_t stats;
        medulla_get_stats(medulla, &stats);

        EXPECT_GE(stats.current_arousal, 0.0f);
        EXPECT_LE(stats.current_arousal, 1.0f);
    }
}

//=============================================================================
// 9. STRESS TEST REGRESSIONS
//=============================================================================

TEST_F(MedullaRegressionTest, RapidStartStopCycles) {
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
        EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);
    }
}

TEST_F(MedullaRegressionTest, HighFrequencyUpdates) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Simulate 60fps for 10 "seconds"
    for (int i = 0; i < 600; i++) {
        int result = medulla_update(medulla, 0.016667f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_GE(stats.total_updates, 600u);
}

//=============================================================================
// 10. CONFIGURATION REGRESSION TESTS
//=============================================================================

TEST_F(MedullaRegressionTest, DefaultConfigStability) {
    medulla_config_t config1 = medulla_default_config();
    medulla_config_t config2 = medulla_default_config();

    // Default configs should be consistent
    EXPECT_EQ(config1.update_interval_ms, config2.update_interval_ms);
    EXPECT_FLOAT_EQ(config1.arousal.baseline_arousal, config2.arousal.baseline_arousal);
    EXPECT_FLOAT_EQ(config1.protection.health_threshold_critical,
                    config2.protection.health_threshold_critical);
}

TEST_F(MedullaRegressionTest, CreateWithNullConfigDefaults) {
    medulla_t m = medulla_create(nullptr);
    ASSERT_NE(m, nullptr);

    // Should start successfully with default config
    EXPECT_EQ(medulla_start(m), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_stop(m), NIMCP_SUCCESS);

    medulla_destroy(m);
}

//=============================================================================
// 11. CIRCADIAN TIME REGRESSION TESTS
//=============================================================================

TEST_F(MedullaRegressionTest, CircadianTimeProgression) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats_before;
    medulla_get_stats(medulla, &stats_before);

    // Run many updates to advance circadian time
    for (int i = 0; i < 1000; i++) {
        medulla_update(medulla, 0.1f);
    }

    medulla_stats_t stats_after;
    medulla_get_stats(medulla, &stats_after);

    // Circadian time should be valid (0-24 hours)
    EXPECT_GE(stats_after.circadian_time_hours, 0.0f);
    EXPECT_LT(stats_after.circadian_time_hours, 24.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
