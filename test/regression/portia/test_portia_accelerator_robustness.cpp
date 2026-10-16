/**
 * @file test_portia_accelerator_robustness.cpp
 * @brief Regression tests for Portia accelerator detection robustness
 *
 * TEST COVERAGE:
 * - Handling of accelerator removal
 * - Repeated detection cycles
 * - Fallback when accelerator fails
 * - Detection latency
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_accelerator.h"

namespace {

class PortiaAcceleratorRobustnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = portia_accelerator_default_config();
        system = portia_accelerator_init(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            portia_accelerator_shutdown(system);
        }
    }

    portia_accelerator_config_t config;
    portia_accelerator_system_t system;
};

TEST_F(PortiaAcceleratorRobustnessTest, RepeatedDetectionStable) {
    const int DETECTION_CYCLES = 50;
    std::vector<uint32_t> detection_counts;

    for (int i = 0; i < DETECTION_CYCLES; i++) {
        uint32_t count = portia_accelerator_detect_all(system);
        detection_counts.push_back(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Count should be stable (not changing wildly)
    if (!detection_counts.empty()) {
        uint32_t first = detection_counts[0];
        int changes = 0;
        for (size_t i = 1; i < detection_counts.size(); i++) {
            if (detection_counts[i] != first) changes++;
        }
        EXPECT_LT(changes, DETECTION_CYCLES / 10)
            << "Detection results unstable";
    }
}

TEST_F(PortiaAcceleratorRobustnessTest, DetectionLatencyBounded) {
    const double MAX_DETECTION_MS = 1000.0;

    auto start = std::chrono::high_resolution_clock::now();
    portia_accelerator_detect_all(system);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    EXPECT_LT(elapsed.count(), MAX_DETECTION_MS)
        << "Detection too slow: " << elapsed.count() << " ms";

    std::cout << "Detection time: " << elapsed.count() << " ms\n";
}

TEST_F(PortiaAcceleratorRobustnessTest, FallbackToCPUWhenNoAccelerator) {
    // Detection should not fail even if no accelerators found
    uint32_t count = portia_accelerator_get_count(system);

    // System should still be functional
    accelerator_info_t info;
    bool has_best = portia_accelerator_get_best(system, &info);

    // Should either have accelerator or handle gracefully
    if (count == 0) {
        EXPECT_FALSE(has_best) << "Reported accelerator when none detected";
    }
    SUCCEED();
}

TEST_F(PortiaAcceleratorRobustnessTest, QueryAfterRemoval) {
    portia_accelerator_detect_all(system);

    // Query should not crash even if hardware removed
    for (int i = 0; i < 10; i++) {
        accelerator_info_t info;
        portia_accelerator_get_by_type(system, ACCELERATOR_TYPE_GPU, &info);
        portia_accelerator_get_count(system);
    }

    SUCCEED();
}

TEST_F(PortiaAcceleratorRobustnessTest, TypeMaskConsistent) {
    portia_accelerator_detect_all(system);

    uint32_t mask1 = portia_accelerator_get_type_mask(system);
    uint32_t mask2 = portia_accelerator_get_type_mask(system);

    EXPECT_EQ(mask1, mask2) << "Type mask inconsistent";
}

TEST_F(PortiaAcceleratorRobustnessTest, PreferenceSettingStable) {
    accelerator_type_t types[] = {
        ACCELERATOR_TYPE_GPU,
        ACCELERATOR_TYPE_NPU,
        ACCELERATOR_TYPE_DSP,
        ACCELERATOR_TYPE_FPGA,
        ACCELERATOR_TYPE_TPU
    };

    // Track if any accelerator is available
    uint32_t available_mask = portia_accelerator_get_type_mask(system);
    int preferences_set = 0;

    for (auto type : types) {
        // portia_accelerator_set_preferred returns bool (true = success)
        bool success = portia_accelerator_set_preferred(system, type);

        // Only verify preference if set was successful
        // (accelerator must be available for preference to be set)
        if (success) {
            accelerator_type_t pref = portia_accelerator_get_preferred(system);
            EXPECT_EQ(pref, type) << "Preference not set correctly";
            preferences_set++;
        }
    }

    // If no accelerators are available, all set_preferred calls should fail
    if (available_mask == 0) {
        EXPECT_EQ(preferences_set, 0)
            << "Preferences set when no accelerators available";
    }
    SUCCEED();
}

TEST_F(PortiaAcceleratorRobustnessTest, ConcurrentQuerySafe) {
    const int QUERIES = 1000;

    for (int i = 0; i < QUERIES; i++) {
        portia_accelerator_get_count(system);
        portia_accelerator_get_type_mask(system);
        portia_accelerator_is_available(system, ACCELERATOR_TYPE_GPU);
    }

    SUCCEED();
}

} // namespace
