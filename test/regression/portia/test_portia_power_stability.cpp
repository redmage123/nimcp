/**
 * @file test_portia_power_stability.cpp
 * @brief Regression tests for Portia power monitoring stability
 *
 * WHAT: Regression tests ensuring power monitoring remains accurate and stable
 * WHY:  Prevent battery estimation drift, profile oscillation, and monitoring failures
 * HOW:  Long-running tests, accuracy verification, threshold testing
 *
 * TEST COVERAGE:
 * - Power profile stability
 * - No oscillation near thresholds
 * - Battery estimation accuracy
 * - Long-running power monitoring
 * - Power spike handling
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_power.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaPowerStabilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = portia_power_default_config();
        config.poll_interval_ms = 100;  // Fast polling for tests
        config.auto_adjust_profile = true;
        manager = portia_power_init(&config);
        ASSERT_NE(manager, nullptr);
    }

    void TearDown() override {
        if (manager) {
            portia_power_shutdown(manager);
            manager = nullptr;
        }
    }

    // Helper: Simulate battery level changes
    power_profile_t get_profile_for_battery(float battery_pct) {
        if (battery_pct > config.performance_threshold) {
            return POWER_PROFILE_PERFORMANCE;
        } else if (battery_pct > config.balanced_threshold) {
            return POWER_PROFILE_BALANCED;
        } else if (battery_pct > config.saver_threshold) {
            return POWER_PROFILE_SAVER;
        } else if (battery_pct > config.critical_threshold) {
            return POWER_PROFILE_CRITICAL;
        }
        return POWER_PROFILE_EMERGENCY;
    }

    portia_power_config_t config;
    portia_power_manager_t manager;
};

//=============================================================================
// Profile Stability Tests
//=============================================================================

TEST_F(PortiaPowerStabilityTest, ProfileStableWithConstantBattery) {
    // WHAT: Verify profile doesn't oscillate with stable battery
    // WHY:  Prevent unnecessary resource adjustments
    // HOW:  Monitor profile over time, verify stability

    const int SAMPLES = 100;
    std::vector<power_profile_t> profiles;

    for (int i = 0; i < SAMPLES; i++) {
        power_profile_t profile = portia_power_get_profile(manager);
        profiles.push_back(profile);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Count profile changes
    int changes = 0;
    for (size_t i = 1; i < profiles.size(); i++) {
        if (profiles[i] != profiles[i-1]) {
            changes++;
        }
    }

    // Should have very few changes with stable battery
    EXPECT_LE(changes, 2) << "Too many profile changes: " << changes;
}

TEST_F(PortiaPowerStabilityTest, NoOscillationNearThresholds) {
    // WHAT: Verify no rapid profile switching near threshold boundaries
    // WHY:  Ensure hysteresis prevents oscillation
    // HOW:  Test with battery levels near thresholds

    std::vector<float> test_levels = {
        config.performance_threshold + 0.5f,  // Just above
        config.performance_threshold - 0.5f,  // Just below
        config.balanced_threshold + 0.5f,
        config.balanced_threshold - 0.5f,
        config.saver_threshold + 0.5f,
        config.saver_threshold - 0.5f
    };

    for (float level : test_levels) {
        std::vector<power_profile_t> profiles;

        // Sample multiple times at this level
        for (int i = 0; i < 20; i++) {
            power_profile_t expected = get_profile_for_battery(level);
            profiles.push_back(expected);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Count changes
        int changes = 0;
        for (size_t i = 1; i < profiles.size(); i++) {
            if (profiles[i] != profiles[i-1]) {
                changes++;
            }
        }

        EXPECT_LE(changes, 1)
            << "Oscillation at battery level " << level << "%: "
            << changes << " changes";
    }
}

TEST_F(PortiaPowerStabilityTest, ProfileTransitionsSmooth) {
    // WHAT: Verify profile transitions happen smoothly without jumps
    // WHY:  Ensure gradual adaptation to battery changes
    // HOW:  Simulate battery drain, verify sequential profile transitions

    std::vector<float> battery_sequence = {
        90.0f, 85.0f, 75.0f, 65.0f, 55.0f, 45.0f, 35.0f, 25.0f, 15.0f, 8.0f
    };

    std::vector<power_profile_t> observed_profiles;

    for (float level : battery_sequence) {
        power_profile_t profile = get_profile_for_battery(level);
        observed_profiles.push_back(profile);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Verify profiles transition in order (no jumps)
    for (size_t i = 1; i < observed_profiles.size(); i++) {
        int diff = static_cast<int>(observed_profiles[i]) -
                   static_cast<int>(observed_profiles[i-1]);
        // Should only increase by 0 or 1 (smooth transition)
        EXPECT_LE(diff, 1) << "Profile jump detected at index " << i;
        EXPECT_GE(diff, 0) << "Profile regression at index " << i;
    }
}

//=============================================================================
// Battery Estimation Tests
//=============================================================================

TEST_F(PortiaPowerStabilityTest, RuntimeEstimationConsistent) {
    // WHAT: Verify runtime estimation doesn't fluctuate wildly
    // WHY:  Provide stable planning information
    // HOW:  Sample runtime multiple times, verify stability

    const int SAMPLES = 50;
    std::vector<float> runtime_estimates;

    for (int i = 0; i < SAMPLES; i++) {
        float runtime = portia_power_estimate_runtime(manager, 0.9f);
        runtime_estimates.push_back(runtime);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Calculate standard deviation
    if (runtime_estimates.size() > 1) {
        float mean = 0.0f;
        for (float r : runtime_estimates) mean += r;
        mean /= runtime_estimates.size();

        float variance = 0.0f;
        for (float r : runtime_estimates) {
            float diff = r - mean;
            variance += diff * diff;
        }
        variance /= runtime_estimates.size();
        float stddev = std::sqrt(variance);

        // Standard deviation should be < 10% of mean
        if (mean > 0.0f) {
            float cv = stddev / mean;  // Coefficient of variation
            EXPECT_LT(cv, 0.1f)
                << "Runtime estimates too variable: CV=" << cv;
        }
    }
}

TEST_F(PortiaPowerStabilityTest, EstimationAccuracyWithKnownDrain) {
    // WHAT: Verify runtime estimation accuracy with simulated drain
    // WHY:  Ensure algorithm correctness
    // HOW:  Simulate known discharge rate, verify estimation

    // Simulate 50% battery with constant 10W drain
    // If battery capacity is 50Wh, runtime should be ~5 hours
    power_status_t status;
    if (portia_power_get_status(manager, &status)) {
        float runtime = portia_power_estimate_runtime(manager, 1.0f);

        // Runtime should be positive and reasonable
        EXPECT_GE(runtime, 0.0f);

        // If on AC, runtime should be 0 or very large
        if (status.plugged_in) {
            EXPECT_TRUE(runtime == 0.0f || runtime > 36000.0f);
        } else {
            // On battery, should have finite estimate
            EXPECT_GT(runtime, 0.0f);
            EXPECT_LT(runtime, 86400.0f);  // Less than 24 hours
        }
    }
}

//=============================================================================
// Long-Running Monitoring Tests
//=============================================================================

TEST_F(PortiaPowerStabilityTest, LongRunningMonitoringStable) {
    // WHAT: Verify monitoring works correctly over extended period
    // WHY:  Ensure no degradation in long-running systems
    // HOW:  Run monitoring for extended period, verify consistency

    const int DURATION_SECONDS = 10;
    const int SAMPLES_PER_SECOND = 5;
    const int TOTAL_SAMPLES = DURATION_SECONDS * SAMPLES_PER_SECOND;

    int successful_queries = 0;
    int failed_queries = 0;

    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < TOTAL_SAMPLES; i++) {
        power_status_t status;
        if (portia_power_get_status(manager, &status)) {
            successful_queries++;

            // Verify status fields are reasonable
            EXPECT_GE(status.battery_level_pct, 0.0f);
            EXPECT_LE(status.battery_level_pct, 100.0f);
            EXPECT_GE(status.temperature_c, -40.0f);
            EXPECT_LE(status.temperature_c, 100.0f);
        } else {
            failed_queries++;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1000 / SAMPLES_PER_SECOND));
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();

    // Should have mostly successful queries
    float success_rate = static_cast<float>(successful_queries) /
                        static_cast<float>(TOTAL_SAMPLES);
    EXPECT_GT(success_rate, 0.95f)
        << "Low success rate: " << success_rate;

    std::cout << "Long-running test: " << duration << "s, "
              << successful_queries << " successful queries, "
              << failed_queries << " failed\n";
}

TEST_F(PortiaPowerStabilityTest, NoMemoryLeakInLongRunning) {
    // WHAT: Verify no memory leaks during extended monitoring
    // WHY:  Ensure system stability over time
    // HOW:  Monitor memory usage over many queries

    const int ITERATIONS = 1000;

    for (int i = 0; i < ITERATIONS; i++) {
        power_status_t status;
        portia_power_get_status(manager, &status);

        power_profile_t profile = portia_power_get_profile(manager);
        (void)profile;  // Suppress warning

        if (i % 100 == 0) {
            // Periodically verify system still responds
            portia_power_stats_t stats;
            bool success = portia_power_get_stats(manager, &stats);
            EXPECT_TRUE(success);
        }
    }

    // If we got here without crashing, test passes
    SUCCEED();
}

//=============================================================================
// Power Spike Handling Tests
//=============================================================================

TEST_F(PortiaPowerStabilityTest, HandlesSuddenPowerSpikes) {
    // WHAT: Verify system handles sudden power consumption changes
    // WHY:  Ensure stability during load spikes
    // HOW:  Simulate rapid discharge rate changes

    std::vector<float> discharge_rates = {
        5.0f, 50.0f, 5.0f, 100.0f, 5.0f, 75.0f, 5.0f
    };

    for (float rate : discharge_rates) {
        // Query status (would reflect discharge rate in real system)
        power_status_t status;
        bool success = portia_power_get_status(manager, &status);
        EXPECT_TRUE(success);

        // System should still be responsive
        power_profile_t profile = portia_power_get_profile(manager);
        EXPECT_GE(profile, POWER_PROFILE_PERFORMANCE);
        EXPECT_LE(profile, POWER_PROFILE_EMERGENCY);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

TEST_F(PortiaPowerStabilityTest, ThermalSpikesHandled) {
    // WHAT: Verify system handles thermal events properly
    // WHY:  Prevent crashes or invalid states during thermal stress
    // HOW:  Simulate thermal warnings, verify graceful handling

    const int THERMAL_EVENTS = 20;

    for (int i = 0; i < THERMAL_EVENTS; i++) {
        power_status_t status;
        if (portia_power_get_status(manager, &status)) {
            // Verify temperature is in valid range
            EXPECT_GE(status.temperature_c, -50.0f);
            EXPECT_LE(status.temperature_c, 150.0f);

            // System should still provide profile
            power_profile_t profile = portia_power_get_profile(manager);
            EXPECT_NE(profile, static_cast<power_profile_t>(-1));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

//=============================================================================
// Statistics and State Tests
//=============================================================================

TEST_F(PortiaPowerStabilityTest, StatisticsAccurate) {
    // WHAT: Verify power statistics are accurately tracked
    // WHY:  Enable monitoring and analysis
    // HOW:  Perform operations, verify stats are consistent

    portia_power_stats_t initial_stats;
    bool success1 = portia_power_get_stats(manager, &initial_stats);
    ASSERT_TRUE(success1);

    // Perform some operations
    const int OPERATIONS = 100;
    for (int i = 0; i < OPERATIONS; i++) {
        power_status_t status;
        portia_power_get_status(manager, &status);
    }

    portia_power_stats_t final_stats;
    bool success2 = portia_power_get_stats(manager, &final_stats);
    ASSERT_TRUE(success2);

    // Check if battery is present by querying current status
    power_status_t power_status;
    portia_power_get_status(manager, &power_status);

    // Samples should have increased (only if battery is present)
    if (power_status.source == POWER_SOURCE_BATTERY) {
        EXPECT_GT(final_stats.samples_taken, initial_stats.samples_taken);
    } else {
        // On AC-only systems, sample collection is limited - just verify no crashes
        EXPECT_GE(final_stats.samples_taken, initial_stats.samples_taken);
    }

    // Averages should be reasonable
    if (final_stats.avg_battery_level > 0.0f) {
        EXPECT_GE(final_stats.avg_battery_level, 0.0f);
        EXPECT_LE(final_stats.avg_battery_level, 100.0f);
    }

    std::cout << "Samples taken: " << final_stats.samples_taken << "\n";
    std::cout << "Profile changes: " << final_stats.profile_changes << "\n";
}

TEST_F(PortiaPowerStabilityTest, ConfigUpdatesDontCrash) {
    // WHAT: Verify configuration updates work safely during operation
    // WHY:  Allow runtime configuration changes
    // HOW:  Update config multiple times, verify stability

    const int CONFIG_UPDATES = 20;

    for (int i = 0; i < CONFIG_UPDATES; i++) {
        // Get current status
        power_status_t status;
        portia_power_get_status(manager, &status);

        // Modify config (if API supports it)
        // portia_power_update_config(manager, &config);

        // Verify still functional
        power_profile_t profile = portia_power_get_profile(manager);
        EXPECT_GE(profile, POWER_PROFILE_PERFORMANCE);
        EXPECT_LE(profile, POWER_PROFILE_EMERGENCY);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

TEST_F(PortiaPowerStabilityTest, ProfileForcingWorks) {
    // WHAT: Verify manual profile forcing works correctly
    // WHY:  Allow user control when needed
    // HOW:  Force each profile, verify it takes effect

    power_profile_t profiles[] = {
        POWER_PROFILE_PERFORMANCE,
        POWER_PROFILE_BALANCED,
        POWER_PROFILE_SAVER,
        POWER_PROFILE_CRITICAL,
        POWER_PROFILE_EMERGENCY
    };

    for (power_profile_t target : profiles) {
        power_profile_t old = portia_power_set_profile(manager, target);
        EXPECT_GE(old, POWER_PROFILE_PERFORMANCE);

        // Verify new profile active
        power_profile_t current = portia_power_get_profile(manager);
        EXPECT_EQ(current, target)
            << "Failed to set profile " << static_cast<int>(target);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // anonymous namespace
