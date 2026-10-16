/**
 * @file test_portia_degradation.cpp
 * @brief Unit tests for Portia graceful degradation system
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
    #include "nimcp.h"
    #include "portia/nimcp_portia_degradation.h"
    #include "security/nimcp_bbb_helpers.h"
    #include "utils/logging/nimcp_logging.h"
    #include "utils/memory/nimcp_memory.h"

class PortiaDegradationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize BBB helpers
        if (!bbb_helpers_is_initialized()) {
            bbb_helpers_init();
        }

        // Default config
        config.level_thresholds[DEGRADATION_LEVEL_NONE] = 0.0f;
        config.level_thresholds[DEGRADATION_LEVEL_MINOR] = 70.0f;
        config.level_thresholds[DEGRADATION_LEVEL_MODERATE] = 80.0f;
        config.level_thresholds[DEGRADATION_LEVEL_SEVERE] = 90.0f;
        config.level_thresholds[DEGRADATION_LEVEL_CRITICAL] = 95.0f;
        config.hysteresis_ms = 100;  // Short for testing
        config.enable_auto_degrade = true;
        config.enable_auto_restore = true;
        config.restore_threshold = 10.0f;

        state = portia_degradation_init(&config);
        ASSERT_NE(state, nullptr);
    }

    void TearDown() override {
        if (state) {
            portia_degradation_cleanup(state);
            state = nullptr;
        }

        // BBB helpers shutdown is handled globally
    }

    degradation_state_t* state = nullptr;
    degradation_internal_config_t config = {0};
};

// ============================================================================
// Basic Initialization Tests
// ============================================================================

TEST_F(PortiaDegradationTest, InitializationSuccess) {
    EXPECT_NE(state, nullptr);

    degradation_level_t level;
    uint32_t active;
    float usage;

    EXPECT_EQ(portia_degradation_get_state(state, &level, &active, &usage), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_NONE);
    EXPECT_GT(active, 0u);
    EXPECT_EQ(usage, 0.0f);
}

TEST_F(PortiaDegradationTest, InitializationWithNullConfig) {
    portia_degradation_cleanup(state);

    state = portia_degradation_init(nullptr);
    EXPECT_NE(state, nullptr);

    // Should use defaults
    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_NONE);
}

TEST_F(PortiaDegradationTest, CleanupSafety) {
    portia_degradation_cleanup(state);
    state = nullptr;

    // Should be safe to call twice
    portia_degradation_cleanup(nullptr);
}

// ============================================================================
// Degradation Level Tests
// ============================================================================

TEST_F(PortiaDegradationTest, AutoDegradationMinor) {
    // Resource usage at minor threshold
    EXPECT_EQ(portia_degradation_evaluate(state, 72.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    uint32_t active;
    EXPECT_EQ(portia_degradation_get_state(state, &level, &active, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_MINOR);

    // Check that some features were disabled
    bool logging_enabled, metrics_enabled;
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_LOGGING_VERBOSE,
                                                     &logging_enabled), NIMCP_SUCCESS);
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_METRICS,
                                                     &metrics_enabled), NIMCP_SUCCESS);

    // These should be disabled at minor level
    EXPECT_FALSE(logging_enabled);
    EXPECT_FALSE(metrics_enabled);
}

TEST_F(PortiaDegradationTest, AutoDegradationModerate) {
    EXPECT_EQ(portia_degradation_evaluate(state, 85.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_MODERATE);

    // Check features
    bool learning_enabled, plasticity_enabled;
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_LEARNING,
                                                     &learning_enabled), NIMCP_SUCCESS);
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_PLASTICITY,
                                                     &plasticity_enabled), NIMCP_SUCCESS);

    EXPECT_FALSE(learning_enabled);
    EXPECT_FALSE(plasticity_enabled);
}

TEST_F(PortiaDegradationTest, AutoDegradationSevere) {
    EXPECT_EQ(portia_degradation_evaluate(state, 92.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_SEVERE);

    // Check high-level features disabled
    bool emotions_enabled, planning_enabled;
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_EMOTIONS,
                                                     &emotions_enabled), NIMCP_SUCCESS);
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_PLANNING,
                                                     &planning_enabled), NIMCP_SUCCESS);

    EXPECT_FALSE(emotions_enabled);
    EXPECT_FALSE(planning_enabled);
}

TEST_F(PortiaDegradationTest, AutoDegradationCritical) {
    EXPECT_EQ(portia_degradation_evaluate(state, 97.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    uint32_t active;
    EXPECT_EQ(portia_degradation_get_state(state, &level, &active, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_CRITICAL);

    // Should have minimal features active
    EXPECT_LT(active, 5u);

    // Core features should still be enabled
    bool working_memory_enabled;
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_MEMORY_WORKING,
                                                     &working_memory_enabled), NIMCP_SUCCESS);
    EXPECT_TRUE(working_memory_enabled);  // Core feature
}

TEST_F(PortiaDegradationTest, AutoRestoration) {
    // Degrade to severe
    EXPECT_EQ(portia_degradation_evaluate(state, 92.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_SEVERE);

    // Wait for hysteresis
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Restore with usage below threshold
    EXPECT_EQ(portia_degradation_evaluate(state, 60.0f, NULL), NIMCP_SUCCESS);

    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_LT(level, DEGRADATION_LEVEL_SEVERE);
}

TEST_F(PortiaDegradationTest, ManualLevelSet) {
    EXPECT_EQ(portia_degradation_set_level(state, DEGRADATION_LEVEL_MODERATE, NULL),
              NIMCP_SUCCESS);

    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_MODERATE);
}

TEST_F(PortiaDegradationTest, InvalidLevelRejected) {
    EXPECT_EQ(portia_degradation_set_level(state, (degradation_level_t)999, NULL),
              NIMCP_ERROR_INVALID_PARAM);
}

// ============================================================================
// Hysteresis Tests
// ============================================================================

TEST_F(PortiaDegradationTest, HysteresisPreventRapidChange) {
    // Set to minor
    EXPECT_EQ(portia_degradation_evaluate(state, 72.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_MINOR);

    // Immediately try to change - should be blocked by hysteresis
    EXPECT_EQ(portia_degradation_evaluate(state, 85.0f, NULL), NIMCP_SUCCESS);

    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_MINOR);  // Should not change yet

    // Wait for hysteresis
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Now it should change
    EXPECT_EQ(portia_degradation_evaluate(state, 85.0f, NULL), NIMCP_SUCCESS);

    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_MODERATE);
}

// ============================================================================
// Feature Management Tests
// ============================================================================

TEST_F(PortiaDegradationTest, ManualFeatureDisable) {
    bool enabled;
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_LEARNING, &enabled),
              NIMCP_SUCCESS);
    EXPECT_TRUE(enabled);

    EXPECT_EQ(portia_degradation_disable_feature(state, FEATURE_LEARNING, NULL),
              NIMCP_SUCCESS);

    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_LEARNING, &enabled),
              NIMCP_SUCCESS);
    EXPECT_FALSE(enabled);
}

TEST_F(PortiaDegradationTest, ManualFeatureEnable) {
    // First disable
    EXPECT_EQ(portia_degradation_disable_feature(state, FEATURE_LEARNING, NULL),
              NIMCP_SUCCESS);

    // Then enable
    EXPECT_EQ(portia_degradation_enable_feature(state, FEATURE_LEARNING, NULL),
              NIMCP_SUCCESS);

    bool enabled;
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_LEARNING, &enabled),
              NIMCP_SUCCESS);
    EXPECT_TRUE(enabled);
}

TEST_F(PortiaDegradationTest, CoreFeatureCannotBeDisabled) {
    // Working memory is core
    EXPECT_EQ(portia_degradation_disable_feature(state, FEATURE_MEMORY_WORKING, NULL),
              NIMCP_ERROR_INVALID_PARAM);

    bool enabled;
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_MEMORY_WORKING, &enabled),
              NIMCP_SUCCESS);
    EXPECT_TRUE(enabled);  // Should still be enabled
}

TEST_F(PortiaDegradationTest, RegisterCustomFeature) {
    degradation_feature_t custom = {
        .feature_id = 0x1000,
        .name = "Custom Feature",
        .disable_at = DEGRADATION_LEVEL_MODERATE,
        .resource_cost = 0.1f,
        .is_core = false,
        .currently_enabled = true
    };

    EXPECT_EQ(portia_degradation_register_feature(state, &custom), NIMCP_SUCCESS);

    bool enabled;
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, 0x1000, &enabled), NIMCP_SUCCESS);
    EXPECT_TRUE(enabled);

    // Should be disabled at moderate level
    EXPECT_EQ(portia_degradation_set_level(state, DEGRADATION_LEVEL_MODERATE, NULL),
              NIMCP_SUCCESS);

    EXPECT_EQ(portia_degradation_is_feature_enabled(state, 0x1000, &enabled), NIMCP_SUCCESS);
    EXPECT_FALSE(enabled);
}

TEST_F(PortiaDegradationTest, DuplicateFeatureRejected) {
    degradation_feature_t duplicate = {
        .feature_id = FEATURE_LEARNING,  // Already exists
        .name = "Duplicate",
        .disable_at = DEGRADATION_LEVEL_MINOR,
        .resource_cost = 0.1f,
        .is_core = false,
        .currently_enabled = true
    };

    EXPECT_EQ(portia_degradation_register_feature(state, &duplicate),
              NIMCP_ALREADY_EXISTS);
}

// ============================================================================
// Query Tests
// ============================================================================

TEST_F(PortiaDegradationTest, GetDegradationChain) {
    degradation_feature_t chain[32];
    uint32_t actual_count;

    EXPECT_EQ(portia_degradation_get_chain(state, chain, 32, &actual_count), NIMCP_SUCCESS);
    EXPECT_GT(actual_count, 0u);

    // Verify chain is sorted by disable_at level
    for (uint32_t i = 1; i < actual_count; i++) {
        EXPECT_LE(chain[i-1].disable_at, chain[i].disable_at);
    }
}

TEST_F(PortiaDegradationTest, GetFeaturesForLevel) {
    uint32_t features[32];
    uint32_t actual_count;

    EXPECT_EQ(portia_degradation_get_features_for_level(state, DEGRADATION_LEVEL_MODERATE,
                                                         features, 32, &actual_count),
              NIMCP_SUCCESS);

    // Should include features disabled at MINOR and MODERATE levels
    EXPECT_GT(actual_count, 0u);

    // Verify features in list
    bool found_learning = false;
    bool found_logging = false;
    for (uint32_t i = 0; i < actual_count; i++) {
        if (features[i] == FEATURE_LEARNING) found_learning = true;
        if (features[i] == FEATURE_LOGGING_VERBOSE) found_logging = true;
    }

    EXPECT_TRUE(found_learning);
    EXPECT_TRUE(found_logging);
}

TEST_F(PortiaDegradationTest, IsFeatureEnabled) {
    bool enabled;

    // Initially all features should be enabled
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_PLASTICITY, &enabled),
              NIMCP_SUCCESS);
    EXPECT_TRUE(enabled);

    // After degradation
    EXPECT_EQ(portia_degradation_set_level(state, DEGRADATION_LEVEL_SEVERE, NULL),
              NIMCP_SUCCESS);

    EXPECT_EQ(portia_degradation_is_feature_enabled(state, FEATURE_PLASTICITY, &enabled),
              NIMCP_SUCCESS);
    EXPECT_FALSE(enabled);
}

TEST_F(PortiaDegradationTest, NonExistentFeature) {
    bool enabled;
    EXPECT_EQ(portia_degradation_is_feature_enabled(state, 0xFFFF, &enabled),
              NIMCP_NOT_FOUND);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(PortiaDegradationTest, ConcurrentEvaluations) {
    std::atomic<int> errors{0};

    auto evaluate_thread = [&](float usage) {
        for (int i = 0; i < 100; i++) {
            if (portia_degradation_evaluate(state, usage, NULL) != NIMCP_SUCCESS) {
                errors++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    };

    std::thread t1(evaluate_thread, 50.0f);
    std::thread t2(evaluate_thread, 75.0f);
    std::thread t3(evaluate_thread, 85.0f);

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(errors.load(), 0);

    // State should be consistent
    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
}

TEST_F(PortiaDegradationTest, ConcurrentFeatureToggle) {
    std::atomic<int> errors{0};

    auto toggle_thread = [&](uint32_t feature_id) {
        for (int i = 0; i < 50; i++) {
            if (portia_degradation_disable_feature(state, feature_id, NULL) != NIMCP_SUCCESS &&
                portia_degradation_enable_feature(state, feature_id, NULL) != NIMCP_SUCCESS) {
                errors++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    };

    std::thread t1(toggle_thread, FEATURE_LEARNING);
    std::thread t2(toggle_thread, FEATURE_PLASTICITY);

    t1.join();
    t2.join();

    // Should have reasonable error count (due to expected conflicts)
    EXPECT_LT(errors.load(), 10);
}

// ============================================================================
// Security Validation Tests
// ============================================================================

TEST_F(PortiaDegradationTest, NullPointerRejected) {
    EXPECT_EQ(portia_degradation_evaluate(nullptr, 50.0f, NULL),
              NIMCP_ERROR_INVALID_PARAM);

    EXPECT_EQ(portia_degradation_set_level(nullptr, DEGRADATION_LEVEL_MINOR, NULL),
              NIMCP_ERROR_INVALID_PARAM);

    EXPECT_EQ(portia_degradation_get_state(nullptr, nullptr, nullptr, nullptr),
              NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(PortiaDegradationTest, InvalidResourceUsage) {
    // Negative usage
    EXPECT_EQ(portia_degradation_evaluate(state, -10.0f, NULL),
              NIMCP_ERROR_INVALID_PARAM);

    // Over 100%
    EXPECT_EQ(portia_degradation_evaluate(state, 150.0f, NULL),
              NIMCP_ERROR_INVALID_PARAM);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(PortiaDegradationTest, ExactThresholdTrigger) {
    // Exactly at minor threshold
    EXPECT_EQ(portia_degradation_evaluate(state, 70.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_MINOR);
}

TEST_F(PortiaDegradationTest, ZeroResourceUsage) {
    EXPECT_EQ(portia_degradation_evaluate(state, 0.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_NONE);
}

TEST_F(PortiaDegradationTest, MaxResourceUsage) {
    EXPECT_EQ(portia_degradation_evaluate(state, 100.0f, NULL), NIMCP_SUCCESS);

    degradation_level_t level;
    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_CRITICAL);
}

TEST_F(PortiaDegradationTest, RapidDegradationAndRestoration) {
    // Degrade
    EXPECT_EQ(portia_degradation_set_level(state, DEGRADATION_LEVEL_CRITICAL, NULL),
              NIMCP_SUCCESS);

    // Immediately restore
    EXPECT_EQ(portia_degradation_set_level(state, DEGRADATION_LEVEL_NONE, NULL),
              NIMCP_SUCCESS);

    // Check all features restored
    degradation_level_t level;
    uint32_t active;
    EXPECT_EQ(portia_degradation_get_state(state, &level, &active, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_NONE);
    EXPECT_GT(active, 8u);  // Most features should be active
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(PortiaDegradationTest, FullDegradationCycle) {
    // Start normal
    degradation_level_t level;
    uint32_t active_start, active_end;

    EXPECT_EQ(portia_degradation_get_state(state, &level, &active_start, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_NONE);

    // Progress through degradation levels
    EXPECT_EQ(portia_degradation_evaluate(state, 72.0f, NULL), NIMCP_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_EQ(portia_degradation_evaluate(state, 85.0f, NULL), NIMCP_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_EQ(portia_degradation_evaluate(state, 92.0f, NULL), NIMCP_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_EQ(portia_degradation_evaluate(state, 97.0f, NULL), NIMCP_SUCCESS);

    EXPECT_EQ(portia_degradation_get_state(state, &level, nullptr, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(level, DEGRADATION_LEVEL_CRITICAL);

    // Restore
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    EXPECT_EQ(portia_degradation_evaluate(state, 60.0f, NULL), NIMCP_SUCCESS);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_EQ(portia_degradation_evaluate(state, 30.0f, NULL), NIMCP_SUCCESS);

    EXPECT_EQ(portia_degradation_get_state(state, &level, &active_end, nullptr), NIMCP_SUCCESS);
    EXPECT_LT(level, DEGRADATION_LEVEL_CRITICAL);

    // Should have more features active after restoration
    EXPECT_GT(active_end, 2u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
