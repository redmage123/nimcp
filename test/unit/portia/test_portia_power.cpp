/**
 * @file test_portia_power.cpp
 * @brief Unit tests for Portia power-aware tier system
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_power.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_tier.h"

/**
 * @brief Test fixture for power monitoring tests
 */
class PortiaPowerTest : public ::testing::Test {
protected:
    portia_power_manager_t manager;

    void SetUp() override {
        // Initialize logging
        nimcp_log_config_t log_config = nimcp_log_default_config();
        log_config.level = LOG_LEVEL_DEBUG;
        nimcp_log_init(&log_config);

        manager = nullptr;
    }

    void TearDown() override {
        if (manager) {
            portia_power_shutdown(manager);
            manager = nullptr;
        }
        nimcp_log_shutdown();
    }
};

/**
 * @brief Test default configuration
 */
TEST_F(PortiaPowerTest, DefaultConfig) {
    portia_power_config_t config = portia_power_default_config();

    EXPECT_EQ(config.poll_interval_ms, 5000);
    EXPECT_TRUE(config.auto_adjust_profile);
    EXPECT_TRUE(config.enable_bio_async_events);
    EXPECT_FLOAT_EQ(config.performance_threshold, 80.0f);
    EXPECT_FLOAT_EQ(config.balanced_threshold, 40.0f);
    EXPECT_FLOAT_EQ(config.saver_threshold, 20.0f);
    EXPECT_FLOAT_EQ(config.critical_threshold, 10.0f);
    EXPECT_FLOAT_EQ(config.max_safe_temp_c, 45.0f);
}

/**
 * @brief Test power monitor initialization
 */
TEST_F(PortiaPowerTest, Initialization) {
    portia_power_config_t config = portia_power_default_config();
    manager = portia_power_init(&config);

    ASSERT_NE(manager, nullptr);

    // Should be able to get status
    power_status_t status;
    EXPECT_TRUE(portia_power_get_status(manager, &status));

    // Status should have reasonable values
    EXPECT_GE(status.battery_level_pct, 0.0f);
    EXPECT_LE(status.battery_level_pct, 100.0f);
    EXPECT_GE(status.temperature_c, -40.0f);
    EXPECT_LE(status.temperature_c, 100.0f);
}

/**
 * @brief Test profile detection
 */
TEST_F(PortiaPowerTest, ProfileDetection) {
    portia_power_config_t config = portia_power_default_config();
    manager = portia_power_init(&config);
    ASSERT_NE(manager, nullptr);

    power_profile_t profile = portia_power_get_profile(manager);

    // Profile should be valid
    EXPECT_GE(profile, POWER_PROFILE_PERFORMANCE);
    EXPECT_LT(profile, POWER_PROFILE_COUNT);
}

/**
 * @brief Test manual profile setting
 */
TEST_F(PortiaPowerTest, ManualProfileSetting) {
    portia_power_config_t config = portia_power_default_config();
    manager = portia_power_init(&config);
    ASSERT_NE(manager, nullptr);

    // Set to performance
    power_profile_t old = portia_power_set_profile(manager, POWER_PROFILE_PERFORMANCE);
    EXPECT_GE(old, POWER_PROFILE_PERFORMANCE);
    EXPECT_LT(old, POWER_PROFILE_COUNT);

    power_profile_t current = portia_power_get_profile(manager);
    EXPECT_EQ(current, POWER_PROFILE_PERFORMANCE);

    // Set to saver
    portia_power_set_profile(manager, POWER_PROFILE_SAVER);
    current = portia_power_get_profile(manager);
    EXPECT_EQ(current, POWER_PROFILE_SAVER);

    // Set to emergency
    portia_power_set_profile(manager, POWER_PROFILE_EMERGENCY);
    current = portia_power_get_profile(manager);
    EXPECT_EQ(current, POWER_PROFILE_EMERGENCY);
}

/**
 * @brief Test tier configuration scaling
 */
TEST_F(PortiaPowerTest, TierConfigScaling) {
    portia_power_config_t config = portia_power_default_config();
    manager = portia_power_init(&config);
    ASSERT_NE(manager, nullptr);

    platform_tier_t base_tier = PLATFORM_TIER_MEDIUM;

    // Test all profiles
    power_tier_config_t perf = portia_power_get_tier_config(
        manager, base_tier, POWER_PROFILE_PERFORMANCE);
    power_tier_config_t balanced = portia_power_get_tier_config(
        manager, base_tier, POWER_PROFILE_BALANCED);
    power_tier_config_t saver = portia_power_get_tier_config(
        manager, base_tier, POWER_PROFILE_SAVER);
    power_tier_config_t critical = portia_power_get_tier_config(
        manager, base_tier, POWER_PROFILE_CRITICAL);
    power_tier_config_t emergency = portia_power_get_tier_config(
        manager, base_tier, POWER_PROFILE_EMERGENCY);

    // Neuron counts should decrease with lower profiles
    EXPECT_GT(perf.max_neurons, balanced.max_neurons);
    EXPECT_GT(balanced.max_neurons, saver.max_neurons);
    EXPECT_GT(saver.max_neurons, critical.max_neurons);
    EXPECT_GT(critical.max_neurons, emergency.max_neurons);

    // Processing rates should decrease
    EXPECT_GT(perf.processing_rate_hz, balanced.processing_rate_hz);
    EXPECT_GT(balanced.processing_rate_hz, saver.processing_rate_hz);
    EXPECT_GT(saver.processing_rate_hz, critical.processing_rate_hz);
    EXPECT_GT(critical.processing_rate_hz, emergency.processing_rate_hz);

    // Learning should be disabled in lower profiles
    EXPECT_TRUE(perf.enable_learning);
    EXPECT_TRUE(balanced.enable_learning);
    EXPECT_FALSE(saver.enable_learning);
    EXPECT_FALSE(critical.enable_learning);
    EXPECT_FALSE(emergency.enable_learning);
}

/**
 * @brief Test cognitive module scaling
 */
TEST_F(PortiaPowerTest, CognitiveModuleScaling) {
    portia_power_config_t config = portia_power_default_config();
    manager = portia_power_init(&config);
    ASSERT_NE(manager, nullptr);

    platform_tier_t base_tier = PLATFORM_TIER_FULL;

    // Performance should have many modules
    power_tier_config_t perf = portia_power_get_tier_config(
        manager, base_tier, POWER_PROFILE_PERFORMANCE);

    // Emergency should have minimal modules
    power_tier_config_t emergency = portia_power_get_tier_config(
        manager, base_tier, POWER_PROFILE_EMERGENCY);

    // Count enabled modules
    auto count_modules = [](uint32_t mask) {
        int count = 0;
        for (int i = 0; i < 32; i++) {
            if (mask & (1u << i)) count++;
        }
        return count;
    };

    int perf_modules = count_modules(perf.cognitive_modules);
    int emergency_modules = count_modules(emergency.cognitive_modules);

    EXPECT_GT(perf_modules, emergency_modules);

    // Emergency should only have attention
    EXPECT_TRUE(emergency.cognitive_modules & COGNITIVE_MODULE_ATTENTION);
}

/**
 * @brief Test runtime estimation
 */
TEST_F(PortiaPowerTest, RuntimeEstimation) {
    portia_power_config_t config = portia_power_default_config();
    manager = portia_power_init(&config);
    ASSERT_NE(manager, nullptr);

    // Estimate runtime
    float runtime = portia_power_estimate_runtime(manager, 0.9f);

    // Should be non-negative
    EXPECT_GE(runtime, 0.0f);

    // If on AC, should be 0 (unlimited)
    power_status_t status;
    if (portia_power_get_status(manager, &status)) {
        if (status.source == POWER_SOURCE_AC) {
            EXPECT_FLOAT_EQ(runtime, 0.0f);
        }
    }
}

/**
 * @brief Test statistics
 */
TEST_F(PortiaPowerTest, Statistics) {
    portia_power_config_t config = portia_power_default_config();
    config.poll_interval_ms = 100;  // Fast polling for test
    manager = portia_power_init(&config);
    ASSERT_NE(manager, nullptr);

    // Wait for a few samples
    sleep(1);

    portia_power_stats_t stats;
    EXPECT_TRUE(portia_power_get_stats(manager, &stats));

    // Should have taken some samples
    EXPECT_GT(stats.samples_taken, 0);

    // Reset stats
    portia_power_reset_stats(manager);
    EXPECT_TRUE(portia_power_get_stats(manager, &stats));
    EXPECT_EQ(stats.samples_taken, 0);
}

/**
 * @brief Test utility functions
 */
TEST_F(PortiaPowerTest, UtilityFunctions) {
    // Power source names
    EXPECT_STREQ(portia_power_get_source_name(POWER_SOURCE_AC), "AC");
    EXPECT_STREQ(portia_power_get_source_name(POWER_SOURCE_BATTERY), "Battery");
    EXPECT_STREQ(portia_power_get_source_name(POWER_SOURCE_SOLAR), "Solar");
    EXPECT_STREQ(portia_power_get_source_name(POWER_SOURCE_USB), "USB");

    // Power profile names
    EXPECT_STREQ(portia_power_get_profile_name(POWER_PROFILE_PERFORMANCE), "Performance");
    EXPECT_STREQ(portia_power_get_profile_name(POWER_PROFILE_BALANCED), "Balanced");
    EXPECT_STREQ(portia_power_get_profile_name(POWER_PROFILE_SAVER), "Power Saver");
    EXPECT_STREQ(portia_power_get_profile_name(POWER_PROFILE_CRITICAL), "Critical");
    EXPECT_STREQ(portia_power_get_profile_name(POWER_PROFILE_EMERGENCY), "Emergency");

    // Limited power sources
    EXPECT_FALSE(portia_power_is_limited(POWER_SOURCE_AC));
    EXPECT_TRUE(portia_power_is_limited(POWER_SOURCE_BATTERY));
    EXPECT_TRUE(portia_power_is_limited(POWER_SOURCE_SOLAR));
    EXPECT_FALSE(portia_power_is_limited(POWER_SOURCE_USB));
}

/**
 * @brief Test null pointer handling
 */
TEST_F(PortiaPowerTest, NullPointerHandling) {
    power_status_t status;

    // Get status with null manager
    EXPECT_FALSE(portia_power_get_status(nullptr, &status));

    // Get profile with null manager
    power_profile_t profile = portia_power_get_profile(nullptr);
    EXPECT_EQ(profile, POWER_PROFILE_BALANCED);  // Default

    // Shutdown null manager (should not crash)
    portia_power_shutdown(nullptr);
}

/**
 * @brief Test invalid profile handling
 */
TEST_F(PortiaPowerTest, InvalidProfile) {
    portia_power_config_t config = portia_power_default_config();
    manager = portia_power_init(&config);
    ASSERT_NE(manager, nullptr);

    power_profile_t old_profile = portia_power_get_profile(manager);

    // Try to set invalid profile
    portia_power_set_profile(manager, (power_profile_t)999);

    // Should still have valid profile
    power_profile_t current = portia_power_get_profile(manager);
    EXPECT_EQ(current, old_profile);
}

/**
 * @brief Test multiple initializations
 */
TEST_F(PortiaPowerTest, MultipleInitializations) {
    portia_power_config_t config = portia_power_default_config();

    // Create first manager
    portia_power_manager_t mgr1 = portia_power_init(&config);
    ASSERT_NE(mgr1, nullptr);

    // Create second manager
    portia_power_manager_t mgr2 = portia_power_init(&config);
    ASSERT_NE(mgr2, nullptr);

    // Both should work independently
    power_status_t status1, status2;
    EXPECT_TRUE(portia_power_get_status(mgr1, &status1));
    EXPECT_TRUE(portia_power_get_status(mgr2, &status2));

    // Cleanup
    portia_power_shutdown(mgr1);
    portia_power_shutdown(mgr2);
    manager = nullptr;  // Don't double-free in TearDown
}

/**
 * @brief Test profile transitions
 */
TEST_F(PortiaPowerTest, ProfileTransitions) {
    portia_power_config_t config = portia_power_default_config();
    config.poll_interval_ms = 100;
    manager = portia_power_init(&config);
    ASSERT_NE(manager, nullptr);

    // Test all profile transitions
    power_profile_t profiles[] = {
        POWER_PROFILE_PERFORMANCE,
        POWER_PROFILE_BALANCED,
        POWER_PROFILE_SAVER,
        POWER_PROFILE_CRITICAL,
        POWER_PROFILE_EMERGENCY
    };

    for (int i = 0; i < 5; i++) {
        portia_power_set_profile(manager, profiles[i]);
        power_profile_t current = portia_power_get_profile(manager);
        EXPECT_EQ(current, profiles[i]);
    }
}

/**
 * @brief Main test entry point
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
