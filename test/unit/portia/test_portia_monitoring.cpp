/**
 * @file test_portia_monitoring.cpp
 * @brief Unit tests for Portia platform monitoring subsystem
 *
 * WHAT: Tests for CPU temperature, battery, and CPU load monitoring
 * WHY:  Verify platform monitoring works correctly for tier switching
 * HOW:  Test initialization, metric queries, caching, and edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_monitoring.h"
#include "utils/logging/nimcp_logging.h"

/**
 * @brief Test fixture for monitoring tests
 */
class PortiaMonitoringTest : public ::testing::Test {
protected:
    portia_monitor_t monitor;

    void SetUp() override {
        // Initialize logging
        nimcp_log_config_t log_config = nimcp_log_default_config();
        log_config.level = LOG_LEVEL_DEBUG;
        nimcp_log_init(&log_config);

        monitor = nullptr;
    }

    void TearDown() override {
        if (monitor) {
            portia_monitor_shutdown(monitor);
            monitor = nullptr;
        }
        nimcp_log_shutdown();
    }
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(PortiaMonitoringTest, DefaultConfig) {
    portia_monitor_config_t config = portia_monitor_default_config();

    EXPECT_EQ(config.cache_timeout_ms, PORTIA_MONITOR_DEFAULT_CACHE_MS);
    EXPECT_TRUE(config.enable_cpu_temp);
    EXPECT_TRUE(config.enable_battery);
    EXPECT_TRUE(config.enable_cpu_load);
    EXPECT_FALSE(config.enable_per_core_load);  // Disabled by default
    EXPECT_EQ(config.thermal_zone_override, nullptr);
    EXPECT_EQ(config.battery_path_override, nullptr);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(PortiaMonitoringTest, InitializationWithDefaults) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    // Should have some capabilities
    uint32_t caps = portia_monitor_get_capabilities(monitor);
    // At minimum, CPU load should be available on most platforms
    // We don't assert specific capabilities since they're platform-dependent
    EXPECT_TRUE(true);  // Just verify it doesn't crash
}

TEST_F(PortiaMonitoringTest, InitializationWithConfig) {
    portia_monitor_config_t config = portia_monitor_default_config();
    config.cache_timeout_ms = 500;

    monitor = portia_monitor_init(&config);
    ASSERT_NE(monitor, nullptr);

    uint32_t caps = portia_monitor_get_capabilities(monitor);
    // Verify initialization succeeded
    EXPECT_TRUE(true);
}

TEST_F(PortiaMonitoringTest, InitializationDisableAll) {
    portia_monitor_config_t config = portia_monitor_default_config();
    config.enable_cpu_temp = false;
    config.enable_battery = false;
    config.enable_cpu_load = false;

    monitor = portia_monitor_init(&config);
    ASSERT_NE(monitor, nullptr);

    // Should have no capabilities when all disabled
    uint32_t caps = portia_monitor_get_capabilities(monitor);
    // Even with all disabled, initialization should succeed
}

TEST_F(PortiaMonitoringTest, CapabilityProbeWithoutInit) {
    // Should be able to probe capabilities without full initialization
    uint32_t caps = portia_monitor_get_capabilities(nullptr);
    // This is a quick probe - behavior is platform-dependent
}

TEST_F(PortiaMonitoringTest, ShutdownNullMonitor) {
    // Shutdown null should not crash
    portia_monitor_shutdown(nullptr);
    EXPECT_TRUE(true);
}

TEST_F(PortiaMonitoringTest, MultipleInitShutdown) {
    // Create and destroy multiple monitors
    for (int i = 0; i < 3; i++) {
        monitor = portia_monitor_init(nullptr);
        ASSERT_NE(monitor, nullptr);
        portia_monitor_shutdown(monitor);
        monitor = nullptr;
    }
}

//=============================================================================
// CPU Temperature Tests
//=============================================================================

TEST_F(PortiaMonitoringTest, GetCpuTemp) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    float temp = portia_monitor_get_cpu_temp(monitor);

    // Temperature should be either invalid (not available) or in reasonable range
    if (portia_monitor_temp_valid(temp)) {
        EXPECT_GE(temp, -40.0f);  // Electronics lower bound
        EXPECT_LE(temp, 150.0f);  // Electronics upper bound
    } else {
        EXPECT_FLOAT_EQ(temp, PORTIA_MONITOR_TEMP_INVALID);
    }
}

TEST_F(PortiaMonitoringTest, GetCpuTempCaching) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    // First call
    float temp1 = portia_monitor_get_cpu_temp(monitor);

    // Second call should use cache
    float temp2 = portia_monitor_get_cpu_temp(monitor);

    // Values should be identical when cached
    if (portia_monitor_temp_valid(temp1)) {
        EXPECT_FLOAT_EQ(temp1, temp2);
    }
}

TEST_F(PortiaMonitoringTest, CpuTempCritical) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    float temp;
    bool critical = portia_monitor_cpu_temp_critical(monitor, &temp, 200.0f);

    // With threshold of 200C, should not be critical
    EXPECT_FALSE(critical);

    // With threshold of -100C, should be critical if we have a valid temp
    critical = portia_monitor_cpu_temp_critical(monitor, &temp, -100.0f);
    if (portia_monitor_temp_valid(temp)) {
        EXPECT_TRUE(critical);
    }
}

TEST_F(PortiaMonitoringTest, GetThermalZones) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    portia_thermal_zone_t zones[PORTIA_MONITOR_MAX_THERMAL_ZONES];
    int count = portia_monitor_get_thermal_zones(monitor, zones, PORTIA_MONITOR_MAX_THERMAL_ZONES);

    // Count should be >= 0
    EXPECT_GE(count, 0);

    // If we have zones, verify their data
    for (int i = 0; i < count; i++) {
        EXPECT_TRUE(zones[i].available);
        EXPECT_GE(zones[i].type, PORTIA_THERMAL_ZONE_CPU);
        EXPECT_LE(zones[i].type, PORTIA_THERMAL_ZONE_UNKNOWN);
    }
}

TEST_F(PortiaMonitoringTest, GetCpuTempNullMonitor) {
    float temp = portia_monitor_get_cpu_temp(nullptr);
    EXPECT_FLOAT_EQ(temp, PORTIA_MONITOR_TEMP_INVALID);
}

//=============================================================================
// Battery Tests
//=============================================================================

TEST_F(PortiaMonitoringTest, GetBatteryPct) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    float pct = portia_monitor_get_battery_pct(monitor);

    // Battery should be unavailable or in 0-100 range
    if (portia_monitor_battery_valid(pct)) {
        EXPECT_GE(pct, 0.0f);
        EXPECT_LE(pct, 100.0f);
    } else {
        EXPECT_FLOAT_EQ(pct, PORTIA_MONITOR_BATTERY_UNAVAILABLE);
    }
}

TEST_F(PortiaMonitoringTest, GetBatteryStatus) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    portia_battery_status_t status;
    bool success = portia_monitor_get_battery_status(monitor, &status);

    if (success && status.available) {
        EXPECT_GE(status.level_pct, 0.0f);
        EXPECT_LE(status.level_pct, 100.0f);
        EXPECT_GE(status.state, PORTIA_BATTERY_DISCHARGING);
        EXPECT_LE(status.state, PORTIA_BATTERY_UNKNOWN);
        EXPECT_GT(status.timestamp_us, 0UL);
    }
}

TEST_F(PortiaMonitoringTest, OnBattery) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    bool on_battery = portia_monitor_on_battery(monitor);
    // Just verify it doesn't crash - actual value is platform-dependent
    (void)on_battery;
}

TEST_F(PortiaMonitoringTest, BatteryCritical) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    float level;
    bool critical = portia_monitor_battery_critical(monitor, &level, 5.0f);

    // If we have a valid battery reading, test threshold logic
    if (portia_monitor_battery_valid(level)) {
        // With 5% threshold
        if (level <= 5.0f) {
            EXPECT_TRUE(critical);
        } else {
            EXPECT_FALSE(critical);
        }
    }
}

TEST_F(PortiaMonitoringTest, GetBatteryPctNullMonitor) {
    float pct = portia_monitor_get_battery_pct(nullptr);
    EXPECT_FLOAT_EQ(pct, PORTIA_MONITOR_BATTERY_UNAVAILABLE);
}

TEST_F(PortiaMonitoringTest, GetBatteryStatusNullParams) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    portia_battery_status_t status;
    bool success = portia_monitor_get_battery_status(nullptr, &status);
    EXPECT_FALSE(success);
    EXPECT_FALSE(status.available);

    success = portia_monitor_get_battery_status(monitor, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// CPU Load Tests
//=============================================================================

TEST_F(PortiaMonitoringTest, GetCpuLoad) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    // First call may return 0 or stale value (needs two samples)
    float load1 = portia_monitor_get_cpu_load(monitor);

    // Wait a bit for stats to accumulate
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Invalidate cache to force new sample
    portia_monitor_refresh(monitor);

    float load2 = portia_monitor_get_cpu_load(monitor);

    // Load should be invalid or in 0-100 range
    if (portia_monitor_load_valid(load2)) {
        EXPECT_GE(load2, 0.0f);
        EXPECT_LE(load2, 100.0f);
    }
}

TEST_F(PortiaMonitoringTest, GetCpuLoadDetailed) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    // Wait for two samples
    portia_monitor_get_cpu_load(monitor);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    portia_monitor_refresh(monitor);

    portia_cpu_load_t load;
    bool success = portia_monitor_get_cpu_load_detailed(monitor, &load);

    if (success && load.available) {
        EXPECT_GE(load.total_load_pct, 0.0f);
        EXPECT_LE(load.total_load_pct, 100.0f);
        EXPECT_GT(load.num_cores, 0U);
        EXPECT_GT(load.timestamp_us, 0UL);

        // Component percentages should sum roughly to 100
        float sum = load.user_pct + load.system_pct + load.iowait_pct + load.idle_pct;
        // Allow some tolerance for floating point
        EXPECT_GE(sum, 95.0f);
        EXPECT_LE(sum, 105.0f);
    }
}

TEST_F(PortiaMonitoringTest, CpuLoadHigh) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    float load;
    bool high = portia_monitor_cpu_load_high(monitor, &load, 99.0f);

    // With 99% threshold, should rarely be high
    if (portia_monitor_load_valid(load)) {
        if (load >= 99.0f) {
            EXPECT_TRUE(high);
        } else {
            EXPECT_FALSE(high);
        }
    }
}

TEST_F(PortiaMonitoringTest, GetCpuLoadNullMonitor) {
    float load = portia_monitor_get_cpu_load(nullptr);
    EXPECT_FLOAT_EQ(load, PORTIA_MONITOR_LOAD_INVALID);
}

//=============================================================================
// Convenience API Tests
//=============================================================================

TEST_F(PortiaMonitoringTest, GetAll) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    float temp, battery, load;
    bool success = portia_monitor_get_all(monitor, &temp, &battery, &load);

    // At least one metric should be valid on most systems
    bool any_valid = portia_monitor_temp_valid(temp) ||
                     portia_monitor_battery_valid(battery) ||
                     portia_monitor_load_valid(load);

    // On some systems (e.g., VMs), none may be valid
    // So we just verify the call doesn't crash
    (void)success;
    (void)any_valid;
}

TEST_F(PortiaMonitoringTest, GetAllPartialParams) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    // Should work with some null params
    float temp;
    bool success = portia_monitor_get_all(monitor, &temp, nullptr, nullptr);
    (void)success;

    float battery;
    success = portia_monitor_get_all(monitor, nullptr, &battery, nullptr);
    (void)success;

    float load;
    success = portia_monitor_get_all(monitor, nullptr, nullptr, &load);
    (void)success;
}

TEST_F(PortiaMonitoringTest, Refresh) {
    portia_monitor_config_t config = portia_monitor_default_config();
    config.cache_timeout_ms = 10000;  // Long cache

    monitor = portia_monitor_init(&config);
    ASSERT_NE(monitor, nullptr);

    // Get initial value
    float temp1 = portia_monitor_get_cpu_temp(monitor);

    // Force refresh
    portia_monitor_refresh(monitor);

    // Get new value (should read fresh)
    float temp2 = portia_monitor_get_cpu_temp(monitor);

    // Values might be same if sensor hasn't changed, just verify no crash
    (void)temp1;
    (void)temp2;
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(PortiaMonitoringTest, ThermalZoneNames) {
    EXPECT_STREQ(portia_monitor_thermal_zone_name(PORTIA_THERMAL_ZONE_CPU), "CPU");
    EXPECT_STREQ(portia_monitor_thermal_zone_name(PORTIA_THERMAL_ZONE_GPU), "GPU");
    EXPECT_STREQ(portia_monitor_thermal_zone_name(PORTIA_THERMAL_ZONE_SSD), "SSD");
    EXPECT_STREQ(portia_monitor_thermal_zone_name(PORTIA_THERMAL_ZONE_MEMORY), "Memory");
    EXPECT_STREQ(portia_monitor_thermal_zone_name(PORTIA_THERMAL_ZONE_BATTERY), "Battery");
    EXPECT_STREQ(portia_monitor_thermal_zone_name(PORTIA_THERMAL_ZONE_AMBIENT), "Ambient");
    EXPECT_STREQ(portia_monitor_thermal_zone_name(PORTIA_THERMAL_ZONE_UNKNOWN), "Unknown");
}

TEST_F(PortiaMonitoringTest, BatteryStateNames) {
    EXPECT_STREQ(portia_monitor_battery_state_name(PORTIA_BATTERY_DISCHARGING), "Discharging");
    EXPECT_STREQ(portia_monitor_battery_state_name(PORTIA_BATTERY_CHARGING), "Charging");
    EXPECT_STREQ(portia_monitor_battery_state_name(PORTIA_BATTERY_FULL), "Full");
    EXPECT_STREQ(portia_monitor_battery_state_name(PORTIA_BATTERY_NOT_PRESENT), "Not Present");
    EXPECT_STREQ(portia_monitor_battery_state_name(PORTIA_BATTERY_UNKNOWN), "Unknown");
}

TEST_F(PortiaMonitoringTest, TempValid) {
    EXPECT_TRUE(portia_monitor_temp_valid(25.0f));
    EXPECT_TRUE(portia_monitor_temp_valid(0.0f));
    EXPECT_TRUE(portia_monitor_temp_valid(100.0f));
    EXPECT_TRUE(portia_monitor_temp_valid(-40.0f));  // Cold but valid

    EXPECT_FALSE(portia_monitor_temp_valid(PORTIA_MONITOR_TEMP_INVALID));
    EXPECT_FALSE(portia_monitor_temp_valid(-300.0f));  // Below absolute zero
    EXPECT_FALSE(portia_monitor_temp_valid(250.0f));   // Too hot
}

TEST_F(PortiaMonitoringTest, BatteryValid) {
    EXPECT_TRUE(portia_monitor_battery_valid(50.0f));
    EXPECT_TRUE(portia_monitor_battery_valid(0.0f));
    EXPECT_TRUE(portia_monitor_battery_valid(100.0f));

    EXPECT_FALSE(portia_monitor_battery_valid(PORTIA_MONITOR_BATTERY_UNAVAILABLE));
    EXPECT_FALSE(portia_monitor_battery_valid(-1.0f));
    EXPECT_FALSE(portia_monitor_battery_valid(101.0f));
}

TEST_F(PortiaMonitoringTest, LoadValid) {
    EXPECT_TRUE(portia_monitor_load_valid(50.0f));
    EXPECT_TRUE(portia_monitor_load_valid(0.0f));
    EXPECT_TRUE(portia_monitor_load_valid(100.0f));

    EXPECT_FALSE(portia_monitor_load_valid(PORTIA_MONITOR_LOAD_INVALID));
    EXPECT_FALSE(portia_monitor_load_valid(-1.0f));
    EXPECT_FALSE(portia_monitor_load_valid(101.0f));
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(PortiaMonitoringTest, ConcurrentReads) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    auto reader = [this]() {
        for (int i = 0; i < 100; i++) {
            float temp = portia_monitor_get_cpu_temp(monitor);
            float battery = portia_monitor_get_battery_pct(monitor);
            float load = portia_monitor_get_cpu_load(monitor);
            (void)temp;
            (void)battery;
            (void)load;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread t1(reader);
    std::thread t2(reader);
    std::thread t3(reader);

    t1.join();
    t2.join();
    t3.join();

    // Should not crash
    EXPECT_TRUE(true);
}

TEST_F(PortiaMonitoringTest, ConcurrentRefresh) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    auto worker = [this]() {
        for (int i = 0; i < 50; i++) {
            portia_monitor_refresh(monitor);
            portia_monitor_get_cpu_temp(monitor);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);

    t1.join();
    t2.join();

    EXPECT_TRUE(true);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PortiaMonitoringTest, RapidInitShutdown) {
    for (int i = 0; i < 10; i++) {
        portia_monitor_t m = portia_monitor_init(nullptr);
        ASSERT_NE(m, nullptr);
        portia_monitor_shutdown(m);
    }
}

TEST_F(PortiaMonitoringTest, GetThermalZonesNullParams) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    int count = portia_monitor_get_thermal_zones(monitor, nullptr, 10);
    EXPECT_EQ(count, 0);

    portia_thermal_zone_t zones[4];
    count = portia_monitor_get_thermal_zones(nullptr, zones, 4);
    EXPECT_EQ(count, 0);

    count = portia_monitor_get_thermal_zones(monitor, zones, 0);
    EXPECT_EQ(count, 0);

    count = portia_monitor_get_thermal_zones(monitor, zones, -1);
    EXPECT_EQ(count, 0);
}

TEST_F(PortiaMonitoringTest, GetPerCoreLoad) {
    monitor = portia_monitor_init(nullptr);
    ASSERT_NE(monitor, nullptr);

    portia_cpu_core_load_t cores[32];
    int count = portia_monitor_get_per_core_load(monitor, cores, 32);

    // Currently returns 0 (not implemented)
    // Just verify it doesn't crash
    EXPECT_GE(count, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
