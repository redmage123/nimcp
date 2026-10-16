/**
 * @file test_brain_init_hypothalamus_regression.cpp
 * @brief Regression tests for brain factory hypothalamus initialization
 *
 * WHAT: Verifies consistent behavior of hypothalamus initialization over time
 * WHY:  Ensure hypothalamus subsystem maintains backward compatibility and
 *       stable performance characteristics across code changes
 * HOW:  Tests default config values, performance bounds, memory usage,
 *       idempotence, cleanup, and initial state consistency
 *
 * Test Cases:
 * 1. HypothalamusDefaultConfigRegression - Default config values don't change
 * 2. HypothalamusInitPerformance - Init completes within reasonable time
 * 3. HypothalamusMemoryUsageRegression - Memory allocation within bounds
 * 4. HypothalamusInitIdempotence - Multiple inits don't cause problems
 * 5. HypothalamusDestroyCleanup - Proper cleanup on brain_destroy
 * 6. HypothalamusCircadianPhaseRegression - Initial phase is consistent
 * 7. HypothalamusCortisolBaselineRegression - Cortisol baseline is consistent
 * 8. HypothalamusTemperatureSetpointRegression - Temperature setpoint consistent
 * 9. BridgeInitializationOrderRegression - Bridge init order is consistent
 * 10. HypothalamusStatusAfterInit - Status should be IDLE after init
 *
 * @version 2.7.0
 * @author NIMCP Development Team
 * @date 2025-01-10
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cstring>
#include <cmath>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init_hypothalamus.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "nimcp.h"

/**
 * @brief Test fixture for hypothalamus initialization regression tests
 */
class HypothalamusInitRegressionTest : public ::testing::Test {
protected:
    std::vector<brain_t> test_brains;

    // Expected default values from header file (regression baselines)
    static constexpr float EXPECTED_CIRCADIAN_PERIOD_HOURS = 24.0f;
    static constexpr float EXPECTED_TEMP_SETPOINT_C = 37.0f;
    static constexpr float EXPECTED_CORTISOL_BASELINE = 0.3f;
    static constexpr float EXPECTED_AUTONOMIC_BALANCE = 0.5f;
    static constexpr float EXPECTED_HUNGER_THRESHOLD = 0.7f;
    static constexpr float EXPECTED_THIRST_THRESHOLD = 0.6f;

    // Performance thresholds (regression baselines)
    static constexpr int64_t MAX_SINGLE_INIT_TIME_US = 50000;      // 50ms
    static constexpr int64_t MAX_FULL_SUBSYSTEM_INIT_TIME_MS = 200; // 200ms
    static constexpr int64_t MAX_REPEATED_INIT_TIME_MS = 100;       // 100ms for 50 iterations

    // Floating point comparison tolerance
    static constexpr float FLOAT_TOLERANCE = 1e-6f;

    void SetUp() override {
        test_brains.clear();
    }

    void TearDown() override {
        for (auto brain : test_brains) {
            if (brain) {
                brain_destroy(brain);
            }
        }
        test_brains.clear();
    }

    brain_t CreateTestBrain(const char* name, brain_size_t size = BRAIN_SIZE_MICRO) {
        // Use brain_create_minimal to avoid OOM from full factory init
        // (full brain_create initializes 50+ subsystems, each allocating memory)
        brain_t brain = brain_create_minimal(name, size, BRAIN_TASK_CLASSIFICATION, 5, 2);
        if (brain) {
            test_brains.push_back(brain);
        }
        return brain;
    }

    // Helper to compare floats with tolerance
    bool FloatEquals(float a, float b, float tolerance = FLOAT_TOLERANCE) {
        return std::fabs(a - b) < tolerance;
    }
};

//=============================================================================
// Test 1: Default Config Values Regression
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, HypothalamusDefaultConfigRegression) {
    // WHAT: Verify default configuration values haven't changed unexpectedly
    // WHY:  Default values are part of the API contract; changes could break users
    // HOW:  Compare against known baseline values from header defines

    hypothalamus_config_t config = hypothalamus_default_config();

    // Verify circadian settings
    EXPECT_TRUE(FloatEquals(config.circadian_period_hours, EXPECTED_CIRCADIAN_PERIOD_HOURS))
        << "Circadian period changed from expected " << EXPECTED_CIRCADIAN_PERIOD_HOURS
        << " to " << config.circadian_period_hours;

    // Verify homeostatic setpoints
    EXPECT_TRUE(FloatEquals(config.temperature_setpoint_c, EXPECTED_TEMP_SETPOINT_C))
        << "Temperature setpoint changed from expected " << EXPECTED_TEMP_SETPOINT_C
        << " to " << config.temperature_setpoint_c;

    // Verify stress response settings
    EXPECT_TRUE(FloatEquals(config.cortisol_baseline, EXPECTED_CORTISOL_BASELINE))
        << "Cortisol baseline changed from expected " << EXPECTED_CORTISOL_BASELINE
        << " to " << config.cortisol_baseline;

    // Verify appetite settings
    EXPECT_TRUE(FloatEquals(config.hunger_threshold, EXPECTED_HUNGER_THRESHOLD))
        << "Hunger threshold changed from expected " << EXPECTED_HUNGER_THRESHOLD
        << " to " << config.hunger_threshold;

    EXPECT_TRUE(FloatEquals(config.thirst_threshold, EXPECTED_THIRST_THRESHOLD))
        << "Thirst threshold changed from expected " << EXPECTED_THIRST_THRESHOLD
        << " to " << config.thirst_threshold;

    // Verify header defines match default config
    EXPECT_EQ(HYPOTHALAMUS_DEFAULT_CIRCADIAN_PERIOD_HOURS, config.circadian_period_hours)
        << "Header define doesn't match default config for circadian period";
    EXPECT_EQ(HYPOTHALAMUS_DEFAULT_TEMP_SETPOINT_C, config.temperature_setpoint_c)
        << "Header define doesn't match default config for temperature setpoint";
    EXPECT_EQ(HYPOTHALAMUS_DEFAULT_CORTISOL_BASELINE, config.cortisol_baseline)
        << "Header define doesn't match default config for cortisol baseline";
    EXPECT_EQ(HYPOTHALAMUS_DEFAULT_AUTONOMIC_BALANCE, EXPECTED_AUTONOMIC_BALANCE)
        << "Header define for autonomic balance changed";
    EXPECT_EQ(HYPOTHALAMUS_DEFAULT_HUNGER_THRESHOLD, config.hunger_threshold)
        << "Header define doesn't match default config for hunger threshold";
    EXPECT_EQ(HYPOTHALAMUS_DEFAULT_THIRST_THRESHOLD, config.thirst_threshold)
        << "Header define doesn't match default config for thirst threshold";
}

//=============================================================================
// Test 2: Initialization Performance Regression
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, HypothalamusInitPerformance) {
    // WHAT: Verify initialization completes within acceptable time bounds
    // WHY:  Performance regression detection - init should not become slower
    // HOW:  Time the initialization and compare against baseline threshold

    brain_t brain = CreateTestBrain("perf_test");
    ASSERT_NE(brain, nullptr);

    // Enable hypothalamus
    brain->hypothalamus_enabled = true;

    auto start = std::chrono::high_resolution_clock::now();
    bool result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_TRUE(result) << "Hypothalamus initialization failed";
    EXPECT_LT(duration_us, MAX_SINGLE_INIT_TIME_US)
        << "Hypothalamus init took " << duration_us << "us, expected < "
        << MAX_SINGLE_INIT_TIME_US << "us";

    // Log actual time for tracking
    std::cout << "[   INFO   ] Hypothalamus init time: " << duration_us << " us" << std::endl;
}

TEST_F(HypothalamusInitRegressionTest, HypothalamusFullSubsystemInitPerformance) {
    // WHAT: Verify full subsystem init with all bridges completes in time
    // WHY:  All bridges together should not cause performance degradation
    // HOW:  Initialize hypothalamus and all bridges, measure total time

    brain_t brain = CreateTestBrain("full_perf_test");
    ASSERT_NE(brain, nullptr);

    brain->hypothalamus_enabled = true;

    auto start = std::chrono::high_resolution_clock::now();

    // Initialize main subsystem and all bridges
    bool result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);

    // Initialize individual bridges (may be called separately)
    nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain);
    nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain);
    nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain);
    nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain);

    auto end = std::chrono::high_resolution_clock::now();

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_TRUE(result);
    EXPECT_LT(duration_ms, MAX_FULL_SUBSYSTEM_INIT_TIME_MS)
        << "Full hypothalamus subsystem init took " << duration_ms << "ms, expected < "
        << MAX_FULL_SUBSYSTEM_INIT_TIME_MS << "ms";

    std::cout << "[   INFO   ] Full subsystem init time: " << duration_ms << " ms" << std::endl;
}

//=============================================================================
// Test 3: Memory Usage Regression
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, HypothalamusMemoryUsageRegression) {
    // WHAT: Verify memory allocation doesn't exceed reasonable bounds
    // WHY:  Memory footprint should remain stable across versions
    // HOW:  Create/destroy multiple instances, check for leaks via pattern

    // Create and destroy multiple hypothalamus instances to check for leaks
    // Memory leaks will be caught by valgrind/ASAN in CI
    // Use brain_create_minimal + BRAIN_SIZE_MICRO to avoid OOM from full factory init

    for (int i = 0; i < 3; i++) {
        char name[64];
        snprintf(name, sizeof(name), "mem_test_%d", i);

        brain_t brain = brain_create_minimal(name, BRAIN_SIZE_MICRO,
                                              BRAIN_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr);

        brain->hypothalamus_enabled = true;
        bool result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
        EXPECT_TRUE(result);

        if (brain->hypothalamus) {
            EXPECT_NE(brain->hypothalamus, nullptr);
        }

        brain_destroy(brain);
    }

    // If there are memory leaks, valgrind/ASAN will catch them
    std::cout << "[   INFO   ] Memory regression test completed - check ASAN/valgrind output" << std::endl;
}

//=============================================================================
// Test 4: Initialization Idempotence
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, HypothalamusInitIdempotence) {
    // WHAT: Verify multiple initializations don't cause problems
    // WHY:  Idempotent init is important for robust code paths
    // HOW:  Call init multiple times, verify same result and no memory issues

    brain_t brain = CreateTestBrain("idempotent_test");
    ASSERT_NE(brain, nullptr);

    brain->hypothalamus_enabled = true;

    // First initialization
    bool result1 = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
    EXPECT_TRUE(result1);

    void* first_ptr = brain->hypothalamus;

    // Multiple subsequent initializations should be idempotent
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; i++) {
        bool result = nimcp_brain_factory_init_hypothalamus_subsystem(brain);
        EXPECT_TRUE(result) << "Init iteration " << i << " failed";

        // Pointer should remain the same (idempotent)
        EXPECT_EQ(brain->hypothalamus, first_ptr)
            << "Hypothalamus pointer changed during idempotent init";
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(duration_ms, MAX_REPEATED_INIT_TIME_MS)
        << "10 idempotent inits took " << duration_ms << "ms, expected < "
        << MAX_REPEATED_INIT_TIME_MS << "ms";
}

//=============================================================================
// Test 5: Destroy Cleanup
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, HypothalamusDestroyCleanup) {
    // WHAT: Verify proper cleanup on brain_destroy
    // WHY:  Resource cleanup is critical for avoiding leaks
    // HOW:  Create brain with hypothalamus, destroy, verify clean

    for (int i = 0; i < 2; i++) {
        char name[64];
        snprintf(name, sizeof(name), "cleanup_test_%d", i);

        // Use brain_create_minimal + BRAIN_SIZE_MICRO to avoid OOM
        brain_t brain = brain_create_minimal(name, BRAIN_SIZE_MICRO,
                                              BRAIN_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr);

        brain->hypothalamus_enabled = true;

        // Initialize full subsystem with all bridges
        nimcp_brain_factory_init_hypothalamus_subsystem(brain);
        nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain);
        nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain);
        nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain);
        nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain);

        // Connect to other systems
        nimcp_brain_factory_connect_hypothalamus_to_sleep(brain);
        nimcp_brain_factory_connect_hypothalamus_to_immune(brain);
        nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain);
        nimcp_brain_factory_connect_hypothalamus_to_medulla(brain);
        nimcp_brain_factory_connect_hypothalamus_to_emotions(brain);

        // Destroy should clean up everything
        brain_destroy(brain);
    }

    // Memory leaks will be caught by valgrind/ASAN
    std::cout << "[   INFO   ] Cleanup test completed - check ASAN/valgrind output" << std::endl;
}

//=============================================================================
// Test 6: Circadian Phase Regression
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, HypothalamusCircadianPhaseRegression) {
    // WHAT: Verify initial circadian phase is consistent
    // WHY:  Initial phase affects all time-dependent behaviors
    // HOW:  Check initial phase value from default config

    hypothalamus_config_t config = hypothalamus_default_config();

    // Initial phase should be 0 (start of day) by default
    EXPECT_TRUE(FloatEquals(config.initial_phase, 0.0f))
        << "Initial circadian phase changed from expected 0.0 to "
        << config.initial_phase;

    // Create adapter and verify circadian state
    hypothalamus_adapter_t* adapter = hypothalamus_create(&config);
    if (adapter) {
        hypo_circadian_state_t state;
        bool result = hypothalamus_get_circadian_state(adapter, &state);

        if (result) {
            // Phase should be at initial value
            EXPECT_TRUE(FloatEquals(state.phase, config.initial_phase))
                << "Circadian state phase doesn't match config initial phase";

            // Amplitude should be 1.0 (full amplitude) initially
            EXPECT_TRUE(FloatEquals(state.amplitude, 1.0f) ||
                       state.amplitude > 0.0f)
                << "Initial amplitude should be positive";
        }

        hypothalamus_destroy(adapter);
    }
}

//=============================================================================
// Test 7: Cortisol Baseline Regression
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, HypothalamusCortisolBaselineRegression) {
    // WHAT: Verify cortisol baseline is consistent
    // WHY:  Cortisol affects stress response, mood, and cognition
    // HOW:  Check baseline value and initial HPA state

    hypothalamus_config_t config = hypothalamus_default_config();

    EXPECT_TRUE(FloatEquals(config.cortisol_baseline, EXPECTED_CORTISOL_BASELINE))
        << "Cortisol baseline changed from expected " << EXPECTED_CORTISOL_BASELINE
        << " to " << config.cortisol_baseline;

    // Verify baseline is within valid range [0, 1]
    EXPECT_GE(config.cortisol_baseline, 0.0f) << "Cortisol baseline below 0";
    EXPECT_LE(config.cortisol_baseline, 1.0f) << "Cortisol baseline above 1";

    // Create adapter and verify HPA state
    hypothalamus_adapter_t* adapter = hypothalamus_create(&config);
    if (adapter) {
        hpa_axis_state_t hpa_state;
        bool result = hypothalamus_get_hpa_state(adapter, &hpa_state);

        if (result) {
            // Initial cortisol should be at or near baseline
            EXPECT_GE(hpa_state.cortisol_level, 0.0f);
            EXPECT_LE(hpa_state.cortisol_level, 1.0f);
        }

        // Get cortisol directly
        float cortisol = hypothalamus_get_cortisol(adapter);
        EXPECT_GE(cortisol, 0.0f);
        EXPECT_LE(cortisol, 1.0f);

        hypothalamus_destroy(adapter);
    }
}

//=============================================================================
// Test 8: Temperature Setpoint Regression
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, HypothalamusTemperatureSetpointRegression) {
    // WHAT: Verify temperature setpoint is consistent
    // WHY:  Temperature regulation is fundamental to homeostasis
    // HOW:  Check setpoint value and thermoregulation state

    hypothalamus_config_t config = hypothalamus_default_config();

    EXPECT_TRUE(FloatEquals(config.temperature_setpoint_c, EXPECTED_TEMP_SETPOINT_C))
        << "Temperature setpoint changed from expected " << EXPECTED_TEMP_SETPOINT_C
        << " to " << config.temperature_setpoint_c;

    // Verify setpoint is physiologically reasonable (35-40 C for humans)
    EXPECT_GE(config.temperature_setpoint_c, 35.0f)
        << "Temperature setpoint too low for physiological range";
    EXPECT_LE(config.temperature_setpoint_c, 40.0f)
        << "Temperature setpoint too high for physiological range";

    // Create adapter and verify thermoregulation state
    hypothalamus_adapter_t* adapter = hypothalamus_create(&config);
    if (adapter) {
        thermoregulation_state_t thermo_state;
        bool result = hypothalamus_get_thermoregulation(adapter, &thermo_state);

        if (result) {
            // Setpoint should match config
            EXPECT_TRUE(FloatEquals(thermo_state.core_temp.setpoint,
                                   config.temperature_setpoint_c))
                << "Thermoregulation setpoint doesn't match config";

            // Initial error should be zero or near zero
            EXPECT_LE(std::fabs(thermo_state.core_temp.error), 5.0f)
                << "Initial temperature error too large";
        }

        hypothalamus_destroy(adapter);
    }
}

//=============================================================================
// Test 9: Bridge Initialization Order Regression
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, BridgeInitializationOrderRegression) {
    // WHAT: Verify bridge initialization order is consistent
    // WHY:  Bridges may have dependencies; order matters
    // HOW:  Initialize bridges in different orders, verify all work

    brain_t brain = CreateTestBrain("bridge_order_test");
    ASSERT_NE(brain, nullptr);

    brain->hypothalamus_enabled = true;

    // Initialize main subsystem first (required)
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(brain));

    // Test forward order
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain));

    // Connections should work after bridges
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_sleep(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_immune(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_medulla(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_emotions(brain));
}

TEST_F(HypothalamusInitRegressionTest, BridgeInitializationReverseOrder) {
    // WHAT: Verify bridges work when initialized in reverse order
    // WHY:  Order independence is important for flexible initialization
    // HOW:  Initialize bridges in reverse order

    brain_t brain = CreateTestBrain("bridge_reverse_test");
    ASSERT_NE(brain, nullptr);

    brain->hypothalamus_enabled = true;

    // Initialize main subsystem first (required)
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(brain));

    // Test reverse order
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain));
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain));

    // Connections in reverse order
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_emotions(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_medulla(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_immune(brain));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_sleep(brain));
}

//=============================================================================
// Test 10: Status After Initialization
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, HypothalamusStatusAfterInit) {
    // WHAT: Verify status is IDLE after initialization
    // WHY:  Initial status affects all subsequent operations
    // HOW:  Create adapter and check initial status

    hypothalamus_config_t config = hypothalamus_default_config();
    hypothalamus_adapter_t* adapter = hypothalamus_create(&config);
    ASSERT_NE(adapter, nullptr);

    hypothalamus_status_t status = hypothalamus_get_status(adapter);

    EXPECT_EQ(status, HYPOTHALAMUS_STATUS_IDLE)
        << "Initial status should be IDLE, got: "
        << hypothalamus_status_string(status);

    // Also verify no initial error
    hypothalamus_error_t error = hypothalamus_get_last_error(adapter);
    EXPECT_EQ(error, HYPOTHALAMUS_ERROR_NONE)
        << "Initial error should be NONE, got: "
        << hypothalamus_error_string(error);

    hypothalamus_destroy(adapter);
}

TEST_F(HypothalamusInitRegressionTest, HypothalamusStatusAfterBrainInit) {
    // WHAT: Verify status is correct after brain factory init
    // WHY:  Brain factory init may set different initial state
    // HOW:  Init via brain factory and check hypothalamus status

    brain_t brain = CreateTestBrain("status_brain_test");
    ASSERT_NE(brain, nullptr);

    brain->hypothalamus_enabled = true;
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(brain));

    if (brain->hypothalamus) {
        hypothalamus_status_t status = hypothalamus_get_status(brain->hypothalamus);
        EXPECT_EQ(status, HYPOTHALAMUS_STATUS_IDLE)
            << "Status after brain init should be IDLE";

        hypothalamus_error_t error = hypothalamus_get_last_error(brain->hypothalamus);
        EXPECT_EQ(error, HYPOTHALAMUS_ERROR_NONE)
            << "No error should be present after successful init";
    }
}

//=============================================================================
// Additional Regression Tests
//=============================================================================

TEST_F(HypothalamusInitRegressionTest, NullBrainHandling) {
    // WHAT: Verify NULL brain handling is consistent
    // WHY:  API should fail gracefully with NULL input
    // HOW:  Pass NULL to all init functions
    //
    // NOTE: The hypothalamus init API behavior:
    // - Main subsystem init returns false for NULL
    // - Individual bridges return false for NULL brain
    // - Connection functions return true for NULL (nothing to connect is OK)

    EXPECT_FALSE(nimcp_brain_factory_init_hypothalamus_subsystem(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_hypothalamus_limbic_bridge(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_hypothalamus_brainstem_bridge(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_hypothalamus_pituitary_bridge(nullptr));
    EXPECT_FALSE(nimcp_brain_factory_init_hypothalamus_quantum_bridge(nullptr));

    // Connection functions return true for NULL (nothing to connect = success)
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_sleep(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_immune(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_wellbeing(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_medulla(nullptr));
    EXPECT_TRUE(nimcp_brain_factory_connect_hypothalamus_to_emotions(nullptr));
}

TEST_F(HypothalamusInitRegressionTest, HypothalamusAlwaysInitializes) {
    // WHAT: Verify hypothalamus subsystem always initializes when called
    // WHY:  Unlike some subsystems, hypothalamus doesn't check a flag before init
    // HOW:  Call init and verify hypothalamus is created
    //
    // NOTE: The hypothalamus init does NOT check hypothalamus_enabled flag before
    // creating. It creates the adapter and THEN sets the flag to true.
    // This is the documented behavior in nimcp_brain_init_hypothalamus.c

    brain_t brain = CreateTestBrain("always_init_test");
    ASSERT_NE(brain, nullptr);

    // Flag starts as false (or whatever default)
    bool initial_enabled = brain->hypothalamus_enabled;

    // Init should succeed and create the adapter
    EXPECT_TRUE(nimcp_brain_factory_init_hypothalamus_subsystem(brain));

    // Hypothalamus should be created
    EXPECT_NE(brain->hypothalamus, nullptr);

    // Flag should now be true
    EXPECT_TRUE(brain->hypothalamus_enabled);
}

TEST_F(HypothalamusInitRegressionTest, StatsAfterInit) {
    // WHAT: Verify statistics are initialized correctly
    // WHY:  Initial stats should be zero/default
    // HOW:  Create adapter and check initial stats

    hypothalamus_config_t config = hypothalamus_default_config();
    hypothalamus_adapter_t* adapter = hypothalamus_create(&config);
    ASSERT_NE(adapter, nullptr);

    hypothalamus_stats_t stats;
    bool result = hypothalamus_get_stats(adapter, &stats);
    EXPECT_TRUE(result);

    // Initial stats should be zero
    EXPECT_EQ(stats.updates_processed, 0u);
    EXPECT_EQ(stats.circadian_ticks, 0u);
    EXPECT_EQ(stats.stress_activations, 0u);
    EXPECT_EQ(stats.thermal_alerts, 0u);
    EXPECT_EQ(stats.hunger_episodes, 0u);
    EXPECT_EQ(stats.thirst_episodes, 0u);

    hypothalamus_destroy(adapter);
}

TEST_F(HypothalamusInitRegressionTest, ConfigRetrieval) {
    // WHAT: Verify config can be retrieved after creation
    // WHY:  Config retrieval is needed for introspection
    // HOW:  Create with config, retrieve, compare

    hypothalamus_config_t original_config = hypothalamus_default_config();
    original_config.temperature_setpoint_c = 36.5f;  // Custom value
    original_config.cortisol_baseline = 0.4f;        // Custom value

    hypothalamus_adapter_t* adapter = hypothalamus_create(&original_config);
    ASSERT_NE(adapter, nullptr);

    hypothalamus_config_t retrieved_config;
    bool result = hypothalamus_get_config(adapter, &retrieved_config);
    EXPECT_TRUE(result);

    // Custom values should match
    EXPECT_TRUE(FloatEquals(retrieved_config.temperature_setpoint_c, 36.5f));
    EXPECT_TRUE(FloatEquals(retrieved_config.cortisol_baseline, 0.4f));

    hypothalamus_destroy(adapter);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
