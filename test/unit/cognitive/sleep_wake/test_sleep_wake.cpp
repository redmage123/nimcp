/**
 * @file test_sleep_wake.cpp
 * @brief Test suite for sleep/wake cycle system (TDD - Red Phase)
 *
 * WHAT: Comprehensive test coverage for sleep/wake cycle
 * WHY:  Test-Driven Development - write tests first, then implement
 * HOW:  Unit tests for all sleep functions + integration tests with working memory
 *
 * TEST STRUCTURE:
 * 1. Lifecycle tests (create, destroy, defaults)
 * 2. Sleep pressure tests (accumulation, threshold, reset)
 * 3. State transition tests (enter state, validate transitions)
 * 4. Sleep cycle tests (full cycle, stages, timing)
 * 5. Statistics tests (tracking, reset)
 * 6. Integration tests (working memory, emotional prioritization)
 * 7. Edge cases (NULL handling, invalid states)
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "core/brain/nimcp_brain.h"
#include "utils/time/nimcp_time.h"

/* ========================================================================
 * TEST FIXTURE
 * ======================================================================== */

/**
 * WHAT: Test fixture for sleep/wake tests
 * WHY:  Setup/teardown common test resources
 * HOW:  Create/destroy sleep system for each test
 */
class SleepWakeTest : public ::testing::Test {
protected:
    sleep_system_t sleep;
    sleep_config_t config;

    void SetUp() override {
        // WHAT: Initialize with default config
        config = sleep_default_config();
        sleep = sleep_system_create(&config);
    }

    void TearDown() override {
        // WHAT: Clean up resources
        sleep_system_destroy(sleep);
    }
};

/* ========================================================================
 * LIFECYCLE TESTS
 * ======================================================================== */

TEST_F(SleepWakeTest, CreateWithDefaultConfig) {
    // WHAT: Verify sleep system can be created with defaults
    // WHY:  Users should be able to use sensible defaults
    // HOW:  Create with NULL config, verify non-NULL return

    sleep_system_t sleep_default = sleep_system_create(NULL);

    ASSERT_NE(sleep_default, nullptr)
        << "Sleep system should be created with NULL config (uses defaults)";

    sleep_system_destroy(sleep_default);
}

TEST_F(SleepWakeTest, CreateWithCustomConfig) {
    // WHAT: Verify sleep system can be created with custom config
    // WHY:  Users need to customize sleep behavior
    // HOW:  Create with modified config, verify settings applied

    sleep_config_t custom_config = sleep_default_config();
    custom_config.sleep_pressure_threshold = 0.5f;
    custom_config.replay_batch_size = 50;
    custom_config.enable_homeostasis = false;

    sleep_system_t sleep_custom = sleep_system_create(&custom_config);

    ASSERT_NE(sleep_custom, nullptr)
        << "Sleep system should be created with custom config";

    // Verify custom settings (would check via getter functions)

    sleep_system_destroy(sleep_custom);
}

TEST_F(SleepWakeTest, DestroyNullSafe) {
    // WHAT: Verify destroy is safe with NULL pointer
    // WHY:  Prevent crashes from accidental NULL destroy
    // HOW:  Call destroy with NULL, should not crash

    EXPECT_NO_FATAL_FAILURE(sleep_system_destroy(NULL))
        << "Destroying NULL sleep system should not crash";
}

TEST_F(SleepWakeTest, DefaultConfigValues) {
    // WHAT: Verify default configuration has sensible biological values
    // WHY:  Defaults should match neuroscience literature
    // HOW:  Check each default value against specification

    sleep_config_t defaults = sleep_default_config();

    // Sleep pressure
    EXPECT_FLOAT_EQ(defaults.adenosine_accumulation_rate, 0.0001f)
        << "Default accumulation rate should be 0.0001";
    EXPECT_FLOAT_EQ(defaults.sleep_pressure_threshold, 0.8f)
        << "Default threshold should be 0.8 (80%)";
    EXPECT_FLOAT_EQ(defaults.adenosine_clearance_rate, 0.05f)
        << "Default clearance rate should be 0.05";

    // Stage durations
    EXPECT_EQ(defaults.drowsy_duration_ms, 120000u)
        << "Drowsy should be 2 minutes (120,000 ms)";
    EXPECT_EQ(defaults.light_sleep_duration_ms, 900000u)
        << "Light sleep should be 15 minutes";
    EXPECT_EQ(defaults.deep_sleep_duration_ms, 1800000u)
        << "Deep sleep should be 30 minutes";
    EXPECT_EQ(defaults.rem_duration_ms, 600000u)
        << "REM should be 10 minutes";

    // Replay parameters
    EXPECT_EQ(defaults.replay_batch_size, 100u)
        << "Default batch size should be 100 memories";
    EXPECT_FLOAT_EQ(defaults.replay_speed_multiplier, 15.0f)
        << "Replay should be 15x speed";
    EXPECT_TRUE(defaults.prioritize_emotional)
        << "Should prioritize emotional memories by default (Phase 10.3)";
    EXPECT_TRUE(defaults.prioritize_novel)
        << "Should prioritize novel memories by default";

    // Homeostasis
    EXPECT_FLOAT_EQ(defaults.synaptic_downscaling_factor, 0.85f)
        << "Downscaling to 85% (Tononi & Cirelli, 2014)";
    EXPECT_FLOAT_EQ(defaults.synaptic_pruning_threshold, 0.01f)
        << "Prune synapses below 0.01";
    EXPECT_TRUE(defaults.enable_homeostasis)
        << "Homeostasis should be enabled by default";

    // REM
    EXPECT_FLOAT_EQ(defaults.rem_creativity_noise, 0.3f)
        << "REM noise should be 0.3 for creativity";
    EXPECT_TRUE(defaults.enable_rem)
        << "REM should be enabled by default";
}

/* ========================================================================
 * SLEEP PRESSURE TESTS
 * ======================================================================== */

TEST_F(SleepWakeTest, InitialSleepPressureIsZero) {
    // WHAT: Verify newly created system has zero sleep pressure
    // WHY:  Brain starts awake and refreshed
    // HOW:  Check pressure immediately after creation

    float pressure = sleep_get_pressure(sleep);

    EXPECT_FLOAT_EQ(pressure, 0.0f)
        << "Initial sleep pressure should be 0.0 (fully rested)";
}

TEST_F(SleepWakeTest, PressureAccumulatesWithLearning) {
    // WHAT: Verify sleep pressure increases after learning
    // WHY:  Learning → synaptic activity → adenosine buildup
    // HOW:  Accumulate pressure, check it increased

    float initial_pressure = sleep_get_pressure(sleep);

    sleep_accumulate_pressure(sleep, 1000);  // 1000 learning steps

    float after_pressure = sleep_get_pressure(sleep);

    EXPECT_GT(after_pressure, initial_pressure)
        << "Pressure should increase after learning";

    // Expected: 1000 * 0.0001 = 0.1
    EXPECT_FLOAT_EQ(after_pressure, 0.1f)
        << "Pressure should increase by steps * rate";
}

TEST_F(SleepWakeTest, PressureClampedAtOne) {
    // WHAT: Verify pressure cannot exceed 1.0
    // WHY:  Prevent overflow, maintain [0,1] range
    // HOW:  Accumulate extreme pressure, check clamped

    sleep_accumulate_pressure(sleep, 100000);  // Huge learning

    float pressure = sleep_get_pressure(sleep);

    EXPECT_LE(pressure, 1.0f)
        << "Pressure should be clamped at maximum 1.0";
    EXPECT_FLOAT_EQ(pressure, 1.0f)
        << "Extreme learning should saturate at 1.0";
}

TEST_F(SleepWakeTest, SleepNotNeededInitially) {
    // WHAT: Verify sleep not needed when pressure is low
    // WHY:  Fresh brain doesn't need sleep yet
    // HOW:  Check is_needed with zero pressure

    EXPECT_FALSE(sleep_is_needed(sleep))
        << "Sleep should not be needed initially (pressure = 0)";
}

TEST_F(SleepWakeTest, SleepNeededWhenPressureExceedsThreshold) {
    // WHAT: Verify sleep is needed when pressure > threshold
    // WHY:  High adenosine signals need for sleep
    // HOW:  Accumulate pressure past threshold, check flag

    // Threshold is 0.8, accumulate to 0.85
    sleep_accumulate_pressure(sleep, 8500);  // 8500 * 0.0001 = 0.85

    EXPECT_TRUE(sleep_is_needed(sleep))
        << "Sleep should be needed when pressure > 0.8";
}

TEST_F(SleepWakeTest, PressureResetAfterWakeUp) {
    // WHAT: Verify pressure is cleared after waking up
    // WHY:  Sleep clears adenosine, refreshes brain
    // HOW:  Accumulate pressure, wake up, check zero

    sleep_accumulate_pressure(sleep, 5000);  // Pressure = 0.5
    EXPECT_GT(sleep_get_pressure(sleep), 0.0f);

    sleep_wake_up(sleep);

    EXPECT_FLOAT_EQ(sleep_get_pressure(sleep), 0.0f)
        << "Pressure should be reset to 0 after wake up";
}

TEST_F(SleepWakeTest, PressureAccumulationNullSafe) {
    // WHAT: Verify accumulation is safe with NULL
    // WHY:  Prevent crashes from NULL pointer
    // HOW:  Call with NULL, should not crash

    EXPECT_NO_FATAL_FAILURE(sleep_accumulate_pressure(NULL, 100))
        << "Accumulating pressure on NULL should not crash";
}

TEST_F(SleepWakeTest, GetPressureNullSafe) {
    // WHAT: Verify get_pressure is safe with NULL
    // WHY:  Prevent crashes from NULL pointer
    // HOW:  Call with NULL, should return 0.0

    float pressure = sleep_get_pressure(NULL);

    EXPECT_FLOAT_EQ(pressure, 0.0f)
        << "Getting pressure from NULL should return 0.0";
}

/* ========================================================================
 * STATE TRANSITION TESTS
 * ======================================================================== */

TEST_F(SleepWakeTest, InitialStateIsAwake) {
    // WHAT: Verify initial state is AWAKE
    // WHY:  Brain starts in active processing mode
    // HOW:  Check state immediately after creation

    sleep_state_t state = sleep_get_current_state(sleep);

    EXPECT_EQ(state, SLEEP_STATE_AWAKE)
        << "Initial state should be AWAKE";
}

TEST_F(SleepWakeTest, CanEnterDrowsyState) {
    // WHAT: Verify can transition to DROWSY
    // WHY:  First stage of sleep
    // HOW:  Enter drowsy, check state changed

    bool success = sleep_enter_state(sleep, SLEEP_STATE_DROWSY);

    EXPECT_TRUE(success)
        << "Should successfully enter DROWSY state";
    EXPECT_EQ(sleep_get_current_state(sleep), SLEEP_STATE_DROWSY)
        << "Current state should be DROWSY";
}

TEST_F(SleepWakeTest, CanEnterLightNREMState) {
    // WHAT: Verify can transition to LIGHT_NREM
    // WHY:  Second stage of sleep
    // HOW:  Enter light NREM, check state

    bool success = sleep_enter_state(sleep, SLEEP_STATE_LIGHT_NREM);

    EXPECT_TRUE(success);
    EXPECT_EQ(sleep_get_current_state(sleep), SLEEP_STATE_LIGHT_NREM);
}

TEST_F(SleepWakeTest, CanEnterDeepNREMState) {
    // WHAT: Verify can transition to DEEP_NREM
    // WHY:  Main consolidation stage
    // HOW:  Enter deep NREM, check state

    bool success = sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);

    EXPECT_TRUE(success);
    EXPECT_EQ(sleep_get_current_state(sleep), SLEEP_STATE_DEEP_NREM)
        << "Should enter DEEP_NREM (consolidation stage)";
}

TEST_F(SleepWakeTest, CanEnterREMState) {
    // WHAT: Verify can transition to REM
    // WHY:  Creative recombination stage
    // HOW:  Enter REM, check state

    bool success = sleep_enter_state(sleep, SLEEP_STATE_REM);

    EXPECT_TRUE(success);
    EXPECT_EQ(sleep_get_current_state(sleep), SLEEP_STATE_REM);
}

TEST_F(SleepWakeTest, CanReturnToAwakeState) {
    // WHAT: Verify can wake up from any sleep state
    // WHY:  Need to return to active processing
    // HOW:  Enter sleep state, wake up, check awake

    sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM);
    EXPECT_EQ(sleep_get_current_state(sleep), SLEEP_STATE_DEEP_NREM);

    sleep_wake_up(sleep);

    EXPECT_EQ(sleep_get_current_state(sleep), SLEEP_STATE_AWAKE)
        << "Should return to AWAKE after wake_up()";
}

TEST_F(SleepWakeTest, EnterStateNullSafe) {
    // WHAT: Verify enter_state is safe with NULL
    // WHY:  Prevent crashes
    // HOW:  Call with NULL, check returns false

    bool success = sleep_enter_state(NULL, SLEEP_STATE_DROWSY);

    EXPECT_FALSE(success)
        << "Entering state on NULL should return false";
}

TEST_F(SleepWakeTest, GetCurrentStateNullSafe) {
    // WHAT: Verify get_current_state is safe with NULL
    // WHY:  Prevent crashes
    // HOW:  Call with NULL, should return AWAKE

    sleep_state_t state = sleep_get_current_state(NULL);

    EXPECT_EQ(state, SLEEP_STATE_AWAKE)
        << "Getting state from NULL should return AWAKE";
}

/* ========================================================================
 * SLEEP CYCLE TESTS
 * ======================================================================== */

TEST_F(SleepWakeTest, RunSingleSleepCycle) {
    // WHAT: Verify can run complete sleep cycle
    // WHY:  Core functionality of system
    // HOW:  Run cycle, check final state is AWAKE

    bool success = sleep_run_cycle(sleep, 1);

    EXPECT_TRUE(success)
        << "Should successfully run 1 sleep cycle";
    EXPECT_EQ(sleep_get_current_state(sleep), SLEEP_STATE_AWAKE)
        << "Should be AWAKE after cycle completes";
}

TEST_F(SleepWakeTest, RunMultipleSleepCycles) {
    // WHAT: Verify can run multiple cycles
    // WHY:  Simulate longer sleep period
    // HOW:  Run 3 cycles, check counter

    bool success = sleep_run_cycle(sleep, 3);

    EXPECT_TRUE(success)
        << "Should successfully run 3 sleep cycles";

    sleep_stats_t stats;
    sleep_get_statistics(sleep, &stats);

    EXPECT_EQ(stats.sleep_cycles_completed, 3u)
        << "Should have completed 3 cycles";
}

TEST_F(SleepWakeTest, CycleClearsPressure) {
    // WHAT: Verify sleep cycle clears accumulated pressure
    // WHY:  Sleep clears adenosine
    // HOW:  Accumulate pressure, run cycle, check zero

    sleep_accumulate_pressure(sleep, 8000);  // Pressure = 0.8
    EXPECT_GT(sleep_get_pressure(sleep), 0.0f);

    sleep_run_cycle(sleep, 1);

    EXPECT_FLOAT_EQ(sleep_get_pressure(sleep), 0.0f)
        << "Cycle should clear sleep pressure";
}

TEST_F(SleepWakeTest, RunCycleNullSafe) {
    // WHAT: Verify run_cycle is safe with NULL
    // WHY:  Prevent crashes
    // HOW:  Call with NULL, check returns false

    bool success = sleep_run_cycle(NULL, 1);

    EXPECT_FALSE(success)
        << "Running cycle on NULL should return false";
}

TEST_F(SleepWakeTest, RunZeroCyclesReturnsFalse) {
    // WHAT: Verify running 0 cycles returns false
    // WHY:  Invalid input validation
    // HOW:  Call with 0 cycles, check false

    bool success = sleep_run_cycle(sleep, 0);

    EXPECT_FALSE(success)
        << "Running 0 cycles should return false";
}

/* ========================================================================
 * STATISTICS TESTS
 * ======================================================================== */

TEST_F(SleepWakeTest, InitialStatisticsAreZero) {
    // WHAT: Verify initial statistics are zeroed
    // WHY:  New system has no history
    // HOW:  Get stats, check all zero

    sleep_stats_t stats;
    bool success = sleep_get_statistics(sleep, &stats);

    ASSERT_TRUE(success);
    EXPECT_EQ(stats.total_awake_time_ms, 0u);
    EXPECT_EQ(stats.total_sleep_time_ms, 0u);
    EXPECT_EQ(stats.sleep_cycles_completed, 0u);
    EXPECT_EQ(stats.total_memories_replayed, 0u);
    EXPECT_EQ(stats.total_synapses_pruned, 0u);
    EXPECT_FLOAT_EQ(stats.current_sleep_pressure, 0.0f);
}

TEST_F(SleepWakeTest, CycleUpdatesStatistics) {
    // WHAT: Verify running cycle updates statistics
    // WHY:  Track sleep quality and efficiency
    // HOW:  Run cycle, check stats incremented

    sleep_run_cycle(sleep, 1);

    sleep_stats_t stats;
    sleep_get_statistics(sleep, &stats);

    EXPECT_EQ(stats.sleep_cycles_completed, 1u)
        << "Cycle counter should increment";
    EXPECT_GT(stats.total_memories_replayed, 0u)
        << "Should have replayed some memories";
}

TEST_F(SleepWakeTest, MultipleUpdatesCumulative) {
    // WHAT: Verify statistics accumulate over multiple cycles
    // WHY:  Lifetime tracking of sleep behavior
    // HOW:  Run 2 cycles, check cumulative stats

    sleep_run_cycle(sleep, 2);

    sleep_stats_t stats;
    sleep_get_statistics(sleep, &stats);

    EXPECT_EQ(stats.sleep_cycles_completed, 2u)
        << "Should accumulate cycles";
    EXPECT_GE(stats.total_memories_replayed, 2 * config.replay_batch_size)
        << "Should accumulate replay counts";
}

TEST_F(SleepWakeTest, ResetStatisticsWorks) {
    // WHAT: Verify can reset statistics
    // WHY:  New measurement period
    // HOW:  Run cycle, reset, check zeroed

    sleep_run_cycle(sleep, 1);

    sleep_stats_t before;
    sleep_get_statistics(sleep, &before);
    EXPECT_GT(before.sleep_cycles_completed, 0u);

    sleep_reset_statistics(sleep);

    sleep_stats_t after;
    sleep_get_statistics(sleep, &after);

    EXPECT_EQ(after.sleep_cycles_completed, 0u)
        << "Cycles should be reset";
    EXPECT_EQ(after.total_memories_replayed, 0u)
        << "Replay count should be reset";
}

TEST_F(SleepWakeTest, ResetPreservesCurrentState) {
    // WHAT: Verify reset doesn't change current state or pressure
    // WHY:  Only statistics reset, not sleep state
    // HOW:  Accumulate pressure, reset stats, check pressure unchanged

    sleep_accumulate_pressure(sleep, 5000);
    float pressure_before = sleep_get_pressure(sleep);

    sleep_reset_statistics(sleep);

    float pressure_after = sleep_get_pressure(sleep);

    EXPECT_FLOAT_EQ(pressure_after, pressure_before)
        << "Reset should not affect current pressure";
}

TEST_F(SleepWakeTest, GetStatisticsNullSafe) {
    // WHAT: Verify get_statistics is safe with NULL pointers
    // WHY:  Prevent crashes
    // HOW:  Call with various NULL combinations

    sleep_stats_t stats;

    EXPECT_FALSE(sleep_get_statistics(NULL, &stats))
        << "NULL sleep should return false";
    EXPECT_FALSE(sleep_get_statistics(sleep, NULL))
        << "NULL stats should return false";
    EXPECT_FALSE(sleep_get_statistics(NULL, NULL))
        << "Both NULL should return false";
}

/* ========================================================================
 * INTEGRATION TESTS - PHASE 10.3
 * ======================================================================== */

TEST_F(SleepWakeTest, EmotionalWorkingMemoryIntegration) {
    // WHAT: Verify sleep system integrates with working memory
    // WHY:  Phase 10.3 - emotional prioritization during consolidation
    // HOW:  Create brain with working memory, run sleep cycle

    // This test requires full brain integration
    // Placeholder for now - will be implemented with brain integration
    GTEST_SKIP() << "Requires brain integration (pending)";
}

TEST_F(SleepWakeTest, EmotionalMemoriesPrioritized) {
    // WHAT: Verify emotional memories replayed first
    // WHY:  Phase 10.3 - amygdala-tagged events get priority
    // HOW:  Add emotional and neutral memories, check replay order

    GTEST_SKIP() << "Requires brain integration (pending)";
}

TEST_F(SleepWakeTest, NoveltyPrioritization) {
    // WHAT: Verify novel memories replayed preferentially
    // WHY:  New information more important to consolidate
    // HOW:  Add novel and familiar memories, check replay order

    GTEST_SKIP() << "Requires brain integration (pending)";
}

/* ========================================================================
 * NOTE: main() provided by test framework (cognitive_tests binary)
 * ======================================================================== */
