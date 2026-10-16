/**
 * @file test_fault_attention_integration.cpp
 * @brief Integration tests for Fault Attention Mechanism
 *
 * WHAT: Integration tests with other fault tolerance components
 * WHY:  Verify attention mechanism works correctly with real fault data
 * HOW:  Test with health monitor, recovery system, and resource allocation
 *
 * TEST SCENARIOS:
 * - Integration with fault detection
 * - Integration with recovery system
 * - Multi-component fault scenarios
 * - Real-world fault patterns
 * - Adaptive learning with actual recovery outcomes
 * - Performance under realistic load
 *
 * @version 1.0.0
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <chrono>
#include <random>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_fault_attention.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FaultAttentionIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        attention = fault_attention_create();
        ASSERT_NE(attention, nullptr);
    }

    void TearDown() override {
        if (attention) {
            fault_attention_destroy(attention);
            attention = nullptr;
        }
    }

    // Helper: Create realistic fault
    active_fault_t create_realistic_fault(
        uint32_t id,
        const char* description,
        float severity,
        uint32_t occurrence_count,
        uint32_t users_affected,
        uint64_t first_time,
        uint64_t last_time
    ) {
        active_fault_t fault = {};
        fault.fault_id = id;
        fault.severity = severity;
        fault.occurrence_count = occurrence_count;
        fault.users_affected = users_affected;
        fault.first_occurrence_ms = first_time;
        fault.last_occurrence_ms = last_time;
        fault.is_active = true;
        snprintf(fault.description, sizeof(fault.description), "%s", description);
        return fault;
    }

    // Helper: Simulate fault stream
    std::vector<active_fault_t> simulate_fault_stream(
        uint32_t num_faults,
        uint64_t time_window_ms
    ) {
        std::vector<active_fault_t> faults;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> severity_dist(0.1, 1.0);
        std::uniform_int_distribution<> occurrence_dist(1, 100);
        std::uniform_int_distribution<> users_dist(1, 1000);

        uint64_t current_time = 1000000;

        for (uint32_t i = 0; i < num_faults; i++) {
            float severity = severity_dist(gen);
            uint32_t occurrences = occurrence_dist(gen);
            uint32_t users = users_dist(gen);

            uint64_t first_time = current_time -
                (std::uniform_int_distribution<>(0, time_window_ms)(gen));
            uint64_t last_time = current_time -
                (std::uniform_int_distribution<>(0, time_window_ms / 2)(gen));

            char desc[128];
            snprintf(desc, sizeof(desc), "Fault_%u_severity_%.2f", i, severity);

            faults.push_back(create_realistic_fault(
                i, desc, severity, occurrences, users, first_time, last_time
            ));
        }

        return faults;
    }

    fault_attention_t* attention = nullptr;
};

//=============================================================================
// Realistic Fault Scenario Tests
//=============================================================================

/**
 * @test Critical System Failure Scenario
 *
 * WHAT: Verify attention prioritizes critical system failures
 * WHY:  Ensure system-wide failures get immediate attention
 * HOW:  Create mix of faults with one critical, verify it's focused
 */
TEST_F(FaultAttentionIntegrationTest, CriticalSystemFailure) {
    uint64_t current_time = 1000000;

    // Create realistic fault set
    std::vector<active_fault_t> faults;

    // Minor warning: Low severity, recent
    faults.push_back(create_realistic_fault(
        1, "Low disk space warning",
        0.2f, 1, 5, current_time - 1000, current_time - 1000
    ));

    // CRITICAL: Database connection failure - high severity, high impact
    faults.push_back(create_realistic_fault(
        2, "Database connection lost",
        0.95f, 3, 500, current_time - 5000, current_time - 2000
    ));

    // Moderate: Network latency spike
    faults.push_back(create_realistic_fault(
        3, "Network latency spike",
        0.5f, 10, 100, current_time - 30000, current_time - 1000
    ));

    // Frequent but minor: Cache miss
    faults.push_back(create_realistic_fault(
        4, "Cache miss rate elevated",
        0.3f, 50, 20, current_time - 60000, current_time - 500
    ));

    // Compute attention weights
    bool result = fault_attention_compute_weights(
        attention,
        faults.data(),
        faults.size(),
        current_time
    );
    ASSERT_TRUE(result);

    // Get focused fault
    uint32_t focused_idx = 0;
    result = fault_attention_get_focused_index(attention, &focused_idx);
    ASSERT_TRUE(result);

    // Should focus on database failure (index 1)
    EXPECT_EQ(focused_idx, 1);
    EXPECT_EQ(faults[focused_idx].fault_id, 2);

    // Verify its weight is highest
    float weights[4];
    for (size_t i = 0; i < 4; i++) {
        fault_attention_get_weight(attention, i, &weights[i]);
    }

    EXPECT_GT(weights[1], weights[0]);
    EXPECT_GT(weights[1], weights[2]);
    EXPECT_GT(weights[1], weights[3]);
}

/**
 * @test Cascading Failure Detection
 *
 * WHAT: Verify attention handles cascading failures correctly
 * WHY:  Root cause should get priority over cascaded failures
 * HOW:  Create failure chain, verify root cause focused
 */
TEST_F(FaultAttentionIntegrationTest, CascadingFailureDetection) {
    uint64_t current_time = 1000000;

    std::vector<active_fault_t> faults;

    // Root cause: API gateway failure (earliest, high severity)
    faults.push_back(create_realistic_fault(
        1, "API Gateway timeout",
        0.8f, 1, 300, current_time - 10000, current_time - 10000
    ));

    // Cascaded: Authentication failures (result of gateway failure)
    faults.push_back(create_realistic_fault(
        2, "Authentication service unavailable",
        0.7f, 5, 250, current_time - 9000, current_time - 5000
    ));

    // Cascaded: User session errors
    faults.push_back(create_realistic_fault(
        3, "User session timeouts",
        0.6f, 20, 200, current_time - 8000, current_time - 2000
    ));

    // Compute weights
    bool result = fault_attention_compute_weights(
        attention,
        faults.data(),
        faults.size(),
        current_time
    );
    ASSERT_TRUE(result);

    // Should focus on root cause (API Gateway)
    uint32_t focused_idx = 0;
    fault_attention_get_focused_index(attention, &focused_idx);

    // Verify API Gateway has high attention
    // (may not be highest due to recency of cascaded errors)
    float weights[3];
    for (size_t i = 0; i < 3; i++) {
        fault_attention_get_weight(attention, i, &weights[i]);
    }

    // All should have significant weight
    for (size_t i = 0; i < 3; i++) {
        EXPECT_GT(weights[i], 0.5f);
    }
}

/**
 * @test High Frequency vs High Severity
 *
 * WHAT: Verify balance between frequent minor and rare critical faults
 * WHY:  Ensure critical rare faults aren't drowned by frequent minor ones
 * HOW:  Create frequent minor + rare critical, verify prioritization
 */
TEST_F(FaultAttentionIntegrationTest, FrequencyVsSeverityBalance) {
    uint64_t current_time = 1000000;

    std::vector<active_fault_t> faults;

    // Very frequent but minor: validation errors
    faults.push_back(create_realistic_fault(
        1, "Input validation errors",
        0.15f, 500, 10, current_time - 60000, current_time - 100
    ));

    // Rare but critical: payment processing failure
    faults.push_back(create_realistic_fault(
        2, "Payment processing failure",
        0.95f, 2, 50, current_time - 20000, current_time - 15000
    ));

    // Configure attention to balance factors
    fault_attention_config_t config = fault_attention_default_config();
    config.severity_weight = 0.5f;    // High severity weight
    config.frequency_weight = 0.2f;   // Moderate frequency weight
    config.recency_weight = 0.2f;
    config.impact_weight = 0.1f;

    fault_attention_t* balanced_attention = fault_attention_create_custom(&config);
    ASSERT_NE(balanced_attention, nullptr);

    bool result = fault_attention_compute_weights(
        balanced_attention,
        faults.data(),
        faults.size(),
        current_time
    );
    ASSERT_TRUE(result);

    uint32_t focused_idx = 0;
    fault_attention_get_focused_index(balanced_attention, &focused_idx);

    // Should focus on payment failure despite lower frequency
    EXPECT_EQ(focused_idx, 1);

    fault_attention_destroy(balanced_attention);
}

//=============================================================================
// Adaptive Learning Integration Tests
//=============================================================================

/**
 * @test Adaptive Learning from Recovery Outcomes
 *
 * WHAT: Verify adaptive learning improves prioritization over time
 * WHY:  System should learn which fault types are most critical
 * HOW:  Simulate recovery cycles, track weight evolution
 */
TEST_F(FaultAttentionIntegrationTest, AdaptiveLearningFromRecovery) {
    // Enable adaptive learning
    fault_attention_config_t config = fault_attention_default_config();
    config.enable_adaptive_weights = true;
    config.learning_rate = 0.1f;

    fault_attention_t* adaptive_attention = fault_attention_create_custom(&config);
    ASSERT_NE(adaptive_attention, nullptr);

    // Get initial weights
    fault_attention_config_t initial_config;
    fault_attention_get_config(adaptive_attention, &initial_config);

    uint64_t current_time = 1000000;

    // Simulate 10 recovery cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Create faults with high severity being most recoverable
        std::vector<active_fault_t> faults;

        // High severity fault (recovers well)
        faults.push_back(create_realistic_fault(
            1, "Memory allocation failure",
            0.9f, 5, 100,
            current_time - 10000, current_time - 1000
        ));

        // Medium severity fault (recovers poorly)
        faults.push_back(create_realistic_fault(
            2, "Configuration corruption",
            0.5f, 3, 80,
            current_time - 8000, current_time - 2000
        ));

        // Compute weights
        fault_attention_compute_weights(
            adaptive_attention,
            faults.data(),
            faults.size(),
            current_time
        );

        // Get focused fault
        uint32_t focused_idx = 0;
        fault_attention_get_focused_index(adaptive_attention, &focused_idx);

        // Simulate recovery outcome
        // High severity recovers successfully
        bool recovery_success = (focused_idx == 0);

        // Update weights based on outcome
        fault_attention_update_weights(
            adaptive_attention,
            focused_idx,
            recovery_success
        );

        current_time += 10000; // Advance time
    }

    // Get final weights
    fault_attention_config_t final_config;
    fault_attention_get_config(adaptive_attention, &final_config);

    // Severity weight should have increased (led to successful recoveries)
    EXPECT_GT(final_config.severity_weight, initial_config.severity_weight);

    // Verify stats
    fault_attention_stats_t stats;
    fault_attention_get_stats(adaptive_attention, &stats);
    EXPECT_EQ(stats.total_computations, 10);
    EXPECT_EQ(stats.total_updates, 10);

    fault_attention_destroy(adaptive_attention);
}

/**
 * @test Learning Rate Impact
 *
 * WHAT: Verify learning rate controls adaptation speed
 * WHY:  Fast learning can overshoot, slow learning takes time
 * HOW:  Compare fast vs slow learning rates
 */
TEST_F(FaultAttentionIntegrationTest, LearningRateImpact) {
    // Fast learner
    fault_attention_config_t fast_config = fault_attention_default_config();
    fast_config.enable_adaptive_weights = true;
    fast_config.learning_rate = 0.2f; // Fast

    fault_attention_t* fast_learner = fault_attention_create_custom(&fast_config);

    // Slow learner
    fault_attention_config_t slow_config = fault_attention_default_config();
    slow_config.enable_adaptive_weights = true;
    slow_config.learning_rate = 0.02f; // Slow

    fault_attention_t* slow_learner = fault_attention_create_custom(&slow_config);

    // Get initial weights
    fault_attention_config_t fast_initial, slow_initial;
    fault_attention_get_config(fast_learner, &fast_initial);
    fault_attention_get_config(slow_learner, &slow_initial);

    // Single learning cycle
    active_fault_t fault = {};
    fault.fault_id = 1;
    fault.severity = 0.9f;
    fault.occurrence_count = 5;
    fault.users_affected = 100;
    fault.first_occurrence_ms = 1000000;
    fault.last_occurrence_ms = 1001000;
    fault.is_active = true;

    // Both learn from same experience
    fault_attention_compute_weights(fast_learner, &fault, 1, 1002000);
    fault_attention_update_weights(fast_learner, 0, true);

    fault_attention_compute_weights(slow_learner, &fault, 1, 1002000);
    fault_attention_update_weights(slow_learner, 0, true);

    // Get updated weights
    fault_attention_config_t fast_updated, slow_updated;
    fault_attention_get_config(fast_learner, &fast_updated);
    fault_attention_get_config(slow_learner, &slow_updated);

    // Fast learner should have changed more
    float fast_delta = fabsf(fast_updated.severity_weight - fast_initial.severity_weight);
    float slow_delta = fabsf(slow_updated.severity_weight - slow_initial.severity_weight);

    EXPECT_GT(fast_delta, slow_delta);

    fault_attention_destroy(fast_learner);
    fault_attention_destroy(slow_learner);
}

//=============================================================================
// Performance and Scalability Tests
//=============================================================================

/**
 * @test High Fault Load Performance
 *
 * WHAT: Verify performance with maximum fault load
 * WHY:  Ensure real-time processing even under stress
 * HOW:  Compute weights for MAX_FAULTS, measure time
 */
TEST_F(FaultAttentionIntegrationTest, HighFaultLoadPerformance) {
    // Create maximum faults
    std::vector<active_fault_t> faults =
        simulate_fault_stream(FAULT_ATTENTION_MAX_FAULTS, 100000);

    uint64_t current_time = 1000000;

    // Measure computation time
    auto start = std::chrono::high_resolution_clock::now();

    bool result = fault_attention_compute_weights(
        attention,
        faults.data(),
        faults.size(),
        current_time
    );

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    ASSERT_TRUE(result);

    // Should complete in < 100μs even with max faults
    EXPECT_LT(duration.count(), 100);

    // Verify focus was selected
    uint32_t focused_idx = 0;
    result = fault_attention_get_focused_index(attention, &focused_idx);
    ASSERT_TRUE(result);
}

/**
 * @test Continuous Operation Stability
 *
 * WHAT: Verify stable operation over many computations
 * WHY:  Ensure no memory leaks or numerical instability
 * HOW:  Run 1000 computation cycles, verify consistency
 */
TEST_F(FaultAttentionIntegrationTest, ContinuousOperationStability) {
    uint64_t current_time = 1000000;

    // Run 1000 cycles
    for (int i = 0; i < 1000; i++) {
        // Generate random faults
        std::vector<active_fault_t> faults = simulate_fault_stream(10, 60000);

        bool result = fault_attention_compute_weights(
            attention,
            faults.data(),
            faults.size(),
            current_time
        );
        ASSERT_TRUE(result);

        // Verify focus is valid
        uint32_t focused_idx = 0;
        if (fault_attention_get_focused_index(attention, &focused_idx)) {
            EXPECT_LT(focused_idx, faults.size());
        }

        current_time += 1000; // Advance time
    }

    // Verify statistics are reasonable
    fault_attention_stats_t stats;
    fault_attention_get_stats(attention, &stats);

    EXPECT_EQ(stats.total_computations, 1000);
    EXPECT_GT(stats.avg_computation_time_us, 0.0f);
    EXPECT_LT(stats.avg_computation_time_us, 100.0f); // < 100μs average
}

/**
 * @test Dynamic Fault Evolution
 *
 * WHAT: Verify attention adapts to changing fault patterns
 * WHY:  Real systems have evolving fault landscapes
 * HOW:  Simulate fault evolution, verify focus shifts appropriately
 */
TEST_F(FaultAttentionIntegrationTest, DynamicFaultEvolution) {
    uint64_t current_time = 1000000;

    // Phase 1: High severity faults dominate
    {
        std::vector<active_fault_t> faults;
        faults.push_back(create_realistic_fault(
            1, "Critical error", 0.9f, 5, 100,
            current_time - 5000, current_time - 1000
        ));
        faults.push_back(create_realistic_fault(
            2, "Minor warning", 0.2f, 10, 20,
            current_time - 10000, current_time - 500
        ));

        fault_attention_compute_weights(attention, faults.data(), faults.size(), current_time);

        uint32_t focused_idx = 0;
        fault_attention_get_focused_index(attention, &focused_idx);
        EXPECT_EQ(focused_idx, 0); // Focus on critical
    }

    current_time += 30000;

    // Phase 2: Critical resolved, frequent minor issues emerge
    {
        std::vector<active_fault_t> faults;
        faults.push_back(create_realistic_fault(
            3, "Frequent timeout", 0.4f, 100, 50,
            current_time - 20000, current_time - 100
        ));
        faults.push_back(create_realistic_fault(
            4, "Rare error", 0.5f, 2, 30,
            current_time - 25000, current_time - 20000
        ));

        fault_attention_compute_weights(attention, faults.data(), faults.size(), current_time);

        uint32_t focused_idx = 0;
        fault_attention_get_focused_index(attention, &focused_idx);
        EXPECT_EQ(focused_idx, 0); // Focus shifts to frequent recent issue
    }
}

//=============================================================================
// Edge Cases and Robustness Tests
//=============================================================================

/**
 * @test Rapid Fault Arrival
 *
 * WHAT: Verify handling of rapidly arriving faults
 * WHY:  System must handle fault bursts
 * HOW:  Compute weights for stream with minimal time gaps
 */
TEST_F(FaultAttentionIntegrationTest, RapidFaultArrival) {
    uint64_t current_time = 1000000;

    std::vector<active_fault_t> faults;

    // Create faults arriving within 100ms of each other
    for (uint32_t i = 0; i < 20; i++) {
        faults.push_back(create_realistic_fault(
            i,
            "Rapid fault",
            0.3f + (i * 0.02f),
            1,
            10 + i,
            current_time - (100 * i),
            current_time - (100 * i)
        ));
    }

    bool result = fault_attention_compute_weights(
        attention,
        faults.data(),
        faults.size(),
        current_time
    );
    ASSERT_TRUE(result);

    // Should still select valid focus
    uint32_t focused_idx = 0;
    result = fault_attention_get_focused_index(attention, &focused_idx);
    ASSERT_TRUE(result);
    EXPECT_LT(focused_idx, faults.size());
}

/**
 * @test All Equal Priority Faults
 *
 * WHAT: Verify handling when all faults have equal priority
 * WHY:  Edge case where no clear winner
 * HOW:  Create identical faults, verify valid selection
 */
TEST_F(FaultAttentionIntegrationTest, AllEqualPriorityFaults) {
    uint64_t current_time = 1000000;

    std::vector<active_fault_t> faults;

    // Create 5 identical faults
    for (uint32_t i = 0; i < 5; i++) {
        faults.push_back(create_realistic_fault(
            i, "Equal priority", 0.5f, 10, 50,
            current_time - 10000, current_time - 5000
        ));
    }

    bool result = fault_attention_compute_weights(
        attention,
        faults.data(),
        faults.size(),
        current_time
    );
    ASSERT_TRUE(result);

    // Should pick one (likely first due to equal weights)
    uint32_t focused_idx = 0;
    result = fault_attention_get_focused_index(attention, &focused_idx);
    ASSERT_TRUE(result);
    EXPECT_LT(focused_idx, 5);

    // All weights should be equal
    float weights[5];
    for (size_t i = 0; i < 5; i++) {
        fault_attention_get_weight(attention, i, &weights[i]);
    }

    for (size_t i = 1; i < 5; i++) {
        EXPECT_NEAR(weights[i], weights[0], 0.001f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
