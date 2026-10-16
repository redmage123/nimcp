/**
 * @file test_executive_middleware_adapter.cpp
 * @brief Unit tests for executive middleware adapter
 *
 * WHAT: Test executive-middleware event routing
 * WHY:  Ensure 100% code coverage and correctness
 * HOW:  GoogleTest-based unit tests
 *
 * COVERAGE TARGET: 100%
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_executive_middleware_adapter.h"
#include "middleware/integration/nimcp_quantum_command_propagator.h"
#include "middleware/integration/nimcp_middleware_command.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Mock Executive Controller
//=============================================================================

// Minimal mock for testing
struct executive_controller {
    int dummy;
};

//=============================================================================
// Test Fixture
//=============================================================================

class ExecutiveMiddlewareAdapterTest : public ::testing::Test {
protected:
    brain_t brain;
    executive_controller_t* executive;
    quantum_command_propagator_t* propagator;
    shannon_monitor_t* shannon_monitor;
    executive_middleware_adapter_t* adapter;

    void SetUp() override {
        // Create brain
        brain = brain_create("test_exec_adapter", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 100, 10);
        ASSERT_NE(brain, nullptr);

        // Create mock executive
        executive = (executive_controller_t*)calloc(1, sizeof(executive_controller));
        ASSERT_NE(executive, nullptr);

        // Create propagator
        propagator = quantum_command_propagator_create(brain, nullptr);
        ASSERT_NE(propagator, nullptr);

        // Shannon monitor optional
        shannon_monitor = nullptr;

        // Create adapter
        adapter = executive_middleware_adapter_create(
            executive, propagator, shannon_monitor
        );
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            executive_middleware_adapter_destroy(adapter);
            adapter = nullptr;
        }
        if (propagator) {
            quantum_command_propagator_destroy(propagator);
            propagator = nullptr;
        }
        if (executive) {
            free(executive);
            executive = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ExecutiveMiddlewareAdapterTest, CreateDestroy) {
    // Test already done in SetUp/TearDown
    EXPECT_NE(adapter, nullptr);
}

TEST_F(ExecutiveMiddlewareAdapterTest, CreateWithNullExecutive) {
    executive_middleware_adapter_t* null_adapter =
        executive_middleware_adapter_create(nullptr, propagator, nullptr);
    EXPECT_EQ(null_adapter, nullptr);
}

TEST_F(ExecutiveMiddlewareAdapterTest, CreateWithNullPropagator) {
    executive_middleware_adapter_t* null_adapter =
        executive_middleware_adapter_create(executive, nullptr, nullptr);
    EXPECT_EQ(null_adapter, nullptr);
}

TEST_F(ExecutiveMiddlewareAdapterTest, CreateWithCustomConfig) {
    executive_middleware_config_t config =
        executive_middleware_adapter_default_config();
    config.command_priority_threshold = 0.5f;
    config.enable_adaptive_routing = false;

    executive_middleware_adapter_t* custom_adapter =
        executive_middleware_adapter_create_custom(
            executive, propagator, nullptr, &config
        );
    ASSERT_NE(custom_adapter, nullptr);

    executive_middleware_adapter_destroy(custom_adapter);
}

TEST_F(ExecutiveMiddlewareAdapterTest, CreateWithNullConfig) {
    executive_middleware_adapter_t* null_adapter =
        executive_middleware_adapter_create_custom(
            executive, propagator, nullptr, nullptr
        );
    EXPECT_EQ(null_adapter, nullptr);
}

TEST_F(ExecutiveMiddlewareAdapterTest, DestroyNull) {
    // Should not crash
    executive_middleware_adapter_destroy(nullptr);
}

//=============================================================================
// Event Handler Tests - Task Switched
//=============================================================================

TEST_F(ExecutiveMiddlewareAdapterTest, OnTaskSwitchedHighPriority) {
    bool success = executive_middleware_adapter_on_task_switched(
        adapter,
        1,      // task_id
        0,      // TASK_TYPE_CLASSIFICATION
        0.9f    // high priority
    );
    EXPECT_TRUE(success);

    // Check metrics
    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 1);
    EXPECT_EQ(metrics.total_commands_issued, 1);
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnTaskSwitchedLowPriority) {
    bool success = executive_middleware_adapter_on_task_switched(
        adapter,
        2,      // task_id
        1,      // TASK_TYPE_REGRESSION
        0.1f    // low priority (below default threshold)
    );
    EXPECT_FALSE(success);  // Should be filtered

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 1);
    EXPECT_EQ(metrics.total_commands_issued, 0);  // Filtered
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnTaskSwitchedDifferentTypes) {
    // Test different task types
    uint32_t task_types[] = {0, 1, 2, 3, 4, 5, 6};  // All types
    for (uint32_t type : task_types) {
        bool success = executive_middleware_adapter_on_task_switched(
            adapter, type, type, 0.8f
        );
        EXPECT_TRUE(success);
    }
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnTaskSwitchedNull) {
    bool success = executive_middleware_adapter_on_task_switched(
        nullptr, 1, 0, 0.8f
    );
    EXPECT_FALSE(success);
}

//=============================================================================
// Event Handler Tests - Cognitive Load
//=============================================================================

TEST_F(ExecutiveMiddlewareAdapterTest, OnCognitiveLoadHigh) {
    bool success = executive_middleware_adapter_on_cognitive_load_changed(
        adapter,
        0.9f  // High load -> should trigger REDUCE_ACTIVITY
    );
    EXPECT_TRUE(success);

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_GT(metrics.total_commands_issued, 0);
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnCognitiveLoadLow) {
    bool success = executive_middleware_adapter_on_cognitive_load_changed(
        adapter,
        0.2f  // Low load -> should trigger INCREASE_ACTIVITY
    );
    EXPECT_TRUE(success);

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_GT(metrics.total_commands_issued, 0);
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnCognitiveLoadNormal) {
    bool success = executive_middleware_adapter_on_cognitive_load_changed(
        adapter,
        0.5f  // Normal load -> no action
    );
    EXPECT_TRUE(success);  // Returns true but no command issued
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnCognitiveLoadNull) {
    bool success = executive_middleware_adapter_on_cognitive_load_changed(
        nullptr, 0.5f
    );
    EXPECT_FALSE(success);
}

//=============================================================================
// Event Handler Tests - Pattern Detected
//=============================================================================

TEST_F(ExecutiveMiddlewareAdapterTest, OnPatternDetectedHighConfidence) {
    bool success = executive_middleware_adapter_on_pattern_detected(
        adapter,
        123,    // pattern_id
        0.9f,   // high confidence
        1       // TARGET_PREFRONTAL
    );
    EXPECT_TRUE(success);
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnPatternDetectedLowConfidence) {
    bool success = executive_middleware_adapter_on_pattern_detected(
        adapter,
        456,    // pattern_id
        0.3f,   // low confidence (< 0.5 threshold)
        2       // TARGET_HIPPOCAMPUS
    );
    EXPECT_FALSE(success);  // Should be filtered
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnPatternDetectedDifferentRegions) {
    for (uint32_t region = 0; region < 7; region++) {
        bool success = executive_middleware_adapter_on_pattern_detected(
            adapter, region, 0.8f, region
        );
        EXPECT_TRUE(success);
    }
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnPatternDetectedNull) {
    bool success = executive_middleware_adapter_on_pattern_detected(
        nullptr, 1, 0.8f, 0
    );
    EXPECT_FALSE(success);
}

//=============================================================================
// Event Handler Tests - Oscillation Changed
//=============================================================================

TEST_F(ExecutiveMiddlewareAdapterTest, OnOscillationChanged) {
    bool success = executive_middleware_adapter_on_oscillation_changed(
        adapter,
        40.0f,  // frequency_hz (gamma)
        0.7f,   // power
        4       // TARGET_VISUAL_CORTEX
    );
    EXPECT_TRUE(success);
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnOscillationChangedDifferentFrequencies) {
    float frequencies[] = {4.0f, 8.0f, 12.0f, 30.0f, 60.0f};  // Delta to gamma
    for (float freq : frequencies) {
        bool success = executive_middleware_adapter_on_oscillation_changed(
            adapter, freq, 0.5f, 0
        );
        EXPECT_TRUE(success);
    }
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnOscillationChangedNull) {
    bool success = executive_middleware_adapter_on_oscillation_changed(
        nullptr, 40.0f, 0.5f, 0
    );
    EXPECT_FALSE(success);
}

//=============================================================================
// Event Handler Tests - Salience Peak
//=============================================================================

TEST_F(ExecutiveMiddlewareAdapterTest, OnSaliencePeak) {
    bool success = executive_middleware_adapter_on_salience_peak(
        adapter,
        0.9f,   // high salience
        3       // TARGET_AMYGDALA
    );
    EXPECT_TRUE(success);
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnSaliencePeakDifferentLevels) {
    float salience_levels[] = {0.3f, 0.5f, 0.7f, 0.9f, 1.0f};
    for (float salience : salience_levels) {
        bool success = executive_middleware_adapter_on_salience_peak(
            adapter, salience, 0
        );
        EXPECT_TRUE(success);
    }
}

TEST_F(ExecutiveMiddlewareAdapterTest, OnSaliencePeakNull) {
    bool success = executive_middleware_adapter_on_salience_peak(
        nullptr, 0.8f, 0
    );
    EXPECT_FALSE(success);
}

//=============================================================================
// Metrics Tests
//=============================================================================

TEST_F(ExecutiveMiddlewareAdapterTest, GetMetrics) {
    executive_middleware_metrics_t metrics;
    bool success = executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_TRUE(success);
    EXPECT_EQ(metrics.total_events_received, 0);
    EXPECT_EQ(metrics.total_commands_issued, 0);
}

TEST_F(ExecutiveMiddlewareAdapterTest, GetMetricsAfterEvents) {
    // Generate some events
    executive_middleware_adapter_on_task_switched(adapter, 1, 0, 0.8f);
    executive_middleware_adapter_on_cognitive_load_changed(adapter, 0.9f);
    executive_middleware_adapter_on_pattern_detected(adapter, 1, 0.8f, 0);

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 3);
    EXPECT_GT(metrics.total_commands_issued, 0);
    EXPECT_GT(metrics.command_success_rate, 0.0f);
}

TEST_F(ExecutiveMiddlewareAdapterTest, GetMetricsWithNull) {
    executive_middleware_metrics_t metrics;
    bool success = executive_middleware_adapter_get_metrics(nullptr, &metrics);
    EXPECT_FALSE(success);
}

TEST_F(ExecutiveMiddlewareAdapterTest, GetMutualInformation) {
    float mi = executive_middleware_adapter_get_mutual_information(adapter);
    EXPECT_GE(mi, 0.0f);
}

TEST_F(ExecutiveMiddlewareAdapterTest, GetSuccessRate) {
    // Initially 0
    float rate = executive_middleware_adapter_get_success_rate(adapter);
    EXPECT_EQ(rate, 0.0f);

    // After successful event
    executive_middleware_adapter_on_task_switched(adapter, 1, 0, 0.8f);
    rate = executive_middleware_adapter_get_success_rate(adapter);
    EXPECT_GT(rate, 0.0f);
    EXPECT_LE(rate, 1.0f);
}

TEST_F(ExecutiveMiddlewareAdapterTest, ResetStats) {
    // Generate events
    executive_middleware_adapter_on_task_switched(adapter, 1, 0, 0.8f);
    executive_middleware_adapter_on_cognitive_load_changed(adapter, 0.9f);

    // Reset
    executive_middleware_adapter_reset_stats(adapter);

    // Check metrics are reset
    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 0);
    EXPECT_EQ(metrics.total_commands_issued, 0);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ExecutiveMiddlewareAdapterTest, EnableAdaptiveRouting) {
    executive_middleware_adapter_enable_adaptive_routing(adapter, true);
    executive_middleware_adapter_enable_adaptive_routing(adapter, false);
    // Should not crash
}

TEST_F(ExecutiveMiddlewareAdapterTest, SetPriorityThreshold) {
    executive_middleware_adapter_set_priority_threshold(adapter, 0.5f);

    // Test filtering with new threshold
    bool success = executive_middleware_adapter_on_task_switched(
        adapter, 1, 0, 0.4f  // Below 0.5 threshold
    );
    EXPECT_FALSE(success);
}

TEST_F(ExecutiveMiddlewareAdapterTest, SetInformationThreshold) {
    executive_middleware_adapter_set_information_threshold(adapter, 5.0f);
    executive_middleware_adapter_set_information_threshold(adapter, 1.0f);
    // Should not crash
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(ExecutiveMiddlewareAdapterTest, MultipleEventsSequence) {
    // Simulate realistic event sequence
    executive_middleware_adapter_on_task_switched(adapter, 1, 0, 0.8f);
    executive_middleware_adapter_on_pattern_detected(adapter, 1, 0.9f, 4);
    executive_middleware_adapter_on_oscillation_changed(adapter, 40.0f, 0.6f, 4);
    executive_middleware_adapter_on_salience_peak(adapter, 0.9f, 3);
    executive_middleware_adapter_on_cognitive_load_changed(adapter, 0.9f);

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 5);
    EXPECT_GT(metrics.total_commands_issued, 0);
}

TEST_F(ExecutiveMiddlewareAdapterTest, RapidFireEvents) {
    for (int i = 0; i < 100; i++) {
        executive_middleware_adapter_on_task_switched(
            adapter, i, i % 7, 0.5f + (i % 5) * 0.1f
        );
    }

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 100);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
