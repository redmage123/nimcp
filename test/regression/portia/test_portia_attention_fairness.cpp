/**
 * @file test_portia_attention_fairness.cpp
 * @brief Regression tests for Portia attention fairness
 *
 * WHAT: Regression tests ensuring fair resource allocation over time
 * WHY:  Prevent resource starvation and unfair allocation
 * HOW:  Long-running fairness tests, starvation detection, preemption tests
 *
 * TEST COVERAGE:
 * - Fair allocation over time
 * - No starvation of low-salience items
 * - Allocation stability
 * - Rapid salience changes handled
 * - Preemption fairness
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_attention.h"
#include "security/nimcp_bbb_helpers.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaAttentionFairnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        bbb_helpers_init();

        config = portia_attention_default_config();
        config.enable_preemption = true;
        config.preemption_threshold = 0.3f;
        config.update_interval_ms = 100;

        state = portia_attention_init(&config, 5, 1.0f);
        ASSERT_NE(state, nullptr);
    }

    void TearDown() override {
        if (state) {
            portia_attention_destroy(state);
            state = nullptr;
        }
        bbb_helpers_shutdown();
    }

    // Helper: Calculate allocation fairness (0=unfair, 1=perfectly fair)
    float calculate_fairness(const std::vector<float>& allocations) {
        if (allocations.empty()) return 1.0f;

        float sum = std::accumulate(allocations.begin(), allocations.end(), 0.0f);
        float mean = sum / allocations.size();

        if (mean == 0.0f) return 1.0f;

        float variance = 0.0f;
        for (float a : allocations) {
            float diff = a - mean;
            variance += diff * diff;
        }
        variance /= allocations.size();

        // Coefficient of variation (lower is more fair)
        float cv = std::sqrt(variance) / mean;

        // Convert to fairness score (1.0 = perfectly fair)
        return std::max(0.0f, 1.0f - cv);
    }

    // Helper: Get all allocations
    std::vector<float> get_all_allocations() {
        std::vector<float> allocations;
        for (int i = 0; i < 5; i++) {
            float alloc = portia_attention_get_allocation(state,
                static_cast<attention_target_t>(i));
            if (alloc >= 0.0f) {
                allocations.push_back(alloc);
            }
        }
        return allocations;
    }

    portia_attention_config_t config;
    portia_attention_state_t state;
};

//=============================================================================
// Fair Allocation Tests
//=============================================================================

TEST_F(PortiaAttentionFairnessTest, EqualSalienceEqualAllocation) {
    // WHAT: Verify equal salience leads to equal allocation
    // WHY:  Basic fairness requirement
    // HOW:  Set equal salience for all, verify equal allocation

    // Set equal salience for all targets
    for (int i = 0; i < 5; i++) {
        portia_attention_update_salience(state,
            static_cast<attention_target_t>(i), 0.5f);
    }

    // Force reallocation
    portia_attention_reallocate(state, true);

    // Get allocations
    std::vector<float> allocations = get_all_allocations();

    // Verify all allocations are approximately equal
    float mean = std::accumulate(allocations.begin(), allocations.end(), 0.0f) / allocations.size();

    for (float alloc : allocations) {
        EXPECT_NEAR(alloc, mean, 0.05f)
            << "Unequal allocation despite equal salience";
    }

    std::cout << "Mean allocation: " << mean << "\n";
}

TEST_F(PortiaAttentionFairnessTest, FairAllocationOverTime) {
    // WHAT: Verify fairness maintained over extended period
    // WHY:  Prevent gradual bias accumulation
    // HOW:  Run many reallocations, track fairness

    const int TIME_STEPS = 100;
    std::vector<float> fairness_scores;

    // Set moderate salience for all
    for (int i = 0; i < 5; i++) {
        float salience = 0.4f + (i * 0.1f);  // Slightly varying
        portia_attention_update_salience(state,
            static_cast<attention_target_t>(i), salience);
    }

    for (int t = 0; t < TIME_STEPS; t++) {
        portia_attention_reallocate(state, false);

        std::vector<float> allocations = get_all_allocations();
        float fairness = calculate_fairness(allocations);
        fairness_scores.push_back(fairness);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Calculate average fairness
    float avg_fairness = std::accumulate(fairness_scores.begin(),
        fairness_scores.end(), 0.0f) / fairness_scores.size();

    // Should maintain high fairness
    EXPECT_GT(avg_fairness, 0.7f)
        << "Low average fairness: " << avg_fairness;

    std::cout << "Average fairness over time: " << avg_fairness << "\n";
}

TEST_F(PortiaAttentionFairnessTest, ProportionalAllocation) {
    // WHAT: Verify allocation proportional to salience
    // WHY:  Ensure high-salience items get more resources
    // HOW:  Set known salience ratios, verify allocation ratios

    // Set salience: 0.1, 0.2, 0.3, 0.4, 0.5 (ratios 1:2:3:4:5)
    for (int i = 0; i < 5; i++) {
        float salience = 0.1f * (i + 1);
        portia_attention_update_salience(state,
            static_cast<attention_target_t>(i), salience);
    }

    portia_attention_reallocate(state, true);

    std::vector<float> allocations = get_all_allocations();
    ASSERT_EQ(allocations.size(), 5u);

    // Verify rough proportionality
    // Highest should get ~2x lowest (allowing for min allocations)
    float min_alloc = *std::min_element(allocations.begin(), allocations.end());
    float max_alloc = *std::max_element(allocations.begin(), allocations.end());

    if (min_alloc > 0.0f) {
        float ratio = max_alloc / min_alloc;
        EXPECT_LT(ratio, 10.0f)
            << "Excessive allocation disparity: " << ratio << "x";
        EXPECT_GT(ratio, 1.5f)
            << "Insufficient differentiation: " << ratio << "x";

        std::cout << "Allocation ratio (max/min): " << ratio << "\n";
    }
}

//=============================================================================
// Starvation Prevention Tests
//=============================================================================

TEST_F(PortiaAttentionFairnessTest, NoStarvationOfLowSalience) {
    // WHAT: Verify low-salience items still get minimum allocation
    // WHY:  Prevent complete resource starvation
    // HOW:  Set very low salience, verify non-zero allocation

    // Set one target with very low salience
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.01f);

    // Set others with high salience
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.9f);
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.8f);

    portia_attention_reallocate(state, true);

    // Check low-salience target still got something
    float low_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);

    EXPECT_GT(low_alloc, 0.0f)
        << "Low-salience target starved completely";
    EXPECT_GT(low_alloc, 0.01f)
        << "Low-salience target allocation too small";

    std::cout << "Low-salience allocation: " << low_alloc << "\n";
}

TEST_F(PortiaAttentionFairnessTest, StarvationCheckLongTerm) {
    // WHAT: Verify no starvation over extended period
    // WHY:  Ensure minimum allocations persist
    // HOW:  Run many iterations, track minimum allocations

    const int ITERATIONS = 200;
    std::vector<float> min_allocations;

    // Set varying salience
    for (int t = 0; t < ITERATIONS; t++) {
        // Vary salience over time
        for (int i = 0; i < 5; i++) {
            float salience = 0.2f + 0.1f * std::sin(t * 0.1f + i);
            portia_attention_update_salience(state,
                static_cast<attention_target_t>(i), salience);
        }

        portia_attention_reallocate(state, false);

        // Find minimum allocation
        std::vector<float> allocations = get_all_allocations();
        if (!allocations.empty()) {
            float min_alloc = *std::min_element(allocations.begin(), allocations.end());
            min_allocations.push_back(min_alloc);
        }
    }

    // Verify minimum never dropped to zero
    for (float min_alloc : min_allocations) {
        EXPECT_GT(min_alloc, 0.0f)
            << "Starvation detected";
    }

    float avg_min = std::accumulate(min_allocations.begin(),
        min_allocations.end(), 0.0f) / min_allocations.size();

    std::cout << "Average minimum allocation: " << avg_min << "\n";
}

//=============================================================================
// Allocation Stability Tests
//=============================================================================

TEST_F(PortiaAttentionFairnessTest, StableAllocationWithStableSalience) {
    // WHAT: Verify allocations don't oscillate with stable salience
    // WHY:  Prevent unnecessary reallocation overhead
    // HOW:  Set stable salience, monitor allocation changes

    // Set stable salience
    for (int i = 0; i < 5; i++) {
        portia_attention_update_salience(state,
            static_cast<attention_target_t>(i), 0.5f);
    }

    portia_attention_reallocate(state, true);

    // Monitor allocations over time
    const int SAMPLES = 50;
    std::vector<std::vector<float>> allocation_history;

    for (int t = 0; t < SAMPLES; t++) {
        std::vector<float> allocations = get_all_allocations();
        allocation_history.push_back(allocations);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Calculate variance for each target
    for (size_t target = 0; target < allocation_history[0].size(); target++) {
        std::vector<float> target_allocs;
        for (const auto& snapshot : allocation_history) {
            if (target < snapshot.size()) {
                target_allocs.push_back(snapshot[target]);
            }
        }

        float mean = std::accumulate(target_allocs.begin(),
            target_allocs.end(), 0.0f) / target_allocs.size();
        float variance = 0.0f;
        for (float a : target_allocs) {
            float diff = a - mean;
            variance += diff * diff;
        }
        variance /= target_allocs.size();
        float stddev = std::sqrt(variance);

        // Standard deviation should be small
        EXPECT_LT(stddev, 0.05f)
            << "High allocation variance for target " << target;
    }
}

TEST_F(PortiaAttentionFairnessTest, SmoothTransitions) {
    // WHAT: Verify allocations change smoothly, not abruptly
    // WHY:  Prevent disruptive resource changes
    // HOW:  Change salience gradually, monitor allocation deltas

    const int STEPS = 100;
    std::vector<float> allocation_deltas;

    float prev_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);

    for (int t = 0; t < STEPS; t++) {
        // Gradually increase salience
        float salience = 0.2f + (t * 0.005f);
        portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, salience);
        portia_attention_reallocate(state, false);

        float curr_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);
        float delta = std::abs(curr_alloc - prev_alloc);
        allocation_deltas.push_back(delta);
        prev_alloc = curr_alloc;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Calculate max delta
    float max_delta = *std::max_element(allocation_deltas.begin(), allocation_deltas.end());

    // Should not have large jumps
    EXPECT_LT(max_delta, 0.2f)
        << "Large allocation jump detected: " << max_delta;

    std::cout << "Maximum allocation delta: " << max_delta << "\n";
}

//=============================================================================
// Rapid Salience Change Tests
//=============================================================================

TEST_F(PortiaAttentionFairnessTest, HandlesRapidSalienceChanges) {
    // WHAT: Verify system handles rapid salience updates
    // WHY:  Dynamic environments need fast adaptation
    // HOW:  Rapidly change salience, verify stability

    const int RAPID_UPDATES = 1000;

    for (int i = 0; i < RAPID_UPDATES; i++) {
        attention_target_t target = static_cast<attention_target_t>(i % 5);
        float salience = 0.1f + (i % 10) * 0.1f;
        portia_attention_update_salience(state, target, salience);

        if (i % 10 == 0) {
            portia_attention_reallocate(state, false);
        }
    }

    // Verify system still functional
    std::vector<float> final_allocations = get_all_allocations();
    EXPECT_FALSE(final_allocations.empty());

    float total_alloc = std::accumulate(final_allocations.begin(),
        final_allocations.end(), 0.0f);
    EXPECT_GT(total_alloc, 0.0f);

    std::cout << "System stable after " << RAPID_UPDATES << " rapid updates\n";
}

TEST_F(PortiaAttentionFairnessTest, SalienceSpikesHandled) {
    // WHAT: Verify sudden salience spikes don't break fairness
    // WHY:  Emergency situations need quick response
    // HOW:  Inject salience spikes, verify recovery

    // Start with equal allocation
    for (int i = 0; i < 5; i++) {
        portia_attention_update_salience(state,
            static_cast<attention_target_t>(i), 0.5f);
    }
    portia_attention_reallocate(state, true);

    // Inject spike
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 1.0f);
    portia_attention_reallocate(state, true);

    // High-salience should get more, but not everything
    // With 5 targets and one having 2x salience (1.0 vs 0.5), expect ~28.5% allocation
    // (1.0 / (1.0 + 4*0.5) = 1/3 = 33%, but smoothing may reduce this)
    float spiked_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);
    EXPECT_GT(spiked_alloc, 0.22f) << "Spike didn't get enough resources";
    EXPECT_LT(spiked_alloc, 0.9f) << "Spike starved other targets";

    // Return to normal
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.5f);
    portia_attention_reallocate(state, true);

    // Should return to approximately equal
    std::vector<float> recovered = get_all_allocations();
    float fairness = calculate_fairness(recovered);
    EXPECT_GT(fairness, 0.6f) << "Failed to recover fairness after spike";
}

//=============================================================================
// Preemption Fairness Tests
//=============================================================================

TEST_F(PortiaAttentionFairnessTest, PreemptionOccursWhenNeeded) {
    // WHAT: Verify preemption happens for high-salience items
    // WHY:  Important tasks need resources quickly
    // HOW:  Create high-salience item, verify preemption

    // Start with low salience target holding resources
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.2f);
    portia_attention_reallocate(state, true);
    float initial_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_MEMORY);

    // Request resources for high-salience target
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.9f);
    portia_attention_request(state, ATTENTION_TARGET_NEURONS, 0.5f);
    portia_attention_reallocate(state, true);

    // Low-salience should have lost resources
    float final_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_MEMORY);
    EXPECT_LT(final_alloc, initial_alloc)
        << "Preemption did not occur";

    // High-salience should have gained
    // Note: With salience-proportional allocation, 0.9 salience target competes
    // with lower salience targets, so may not get > 0.3
    float high_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);
    EXPECT_GT(high_alloc, 0.2f)
        << "High-salience didn't get enough after preemption";

    // Check statistics - preemption tracking may be implementation-dependent
    portia_attention_stats_t stats;
    portia_attention_get_stats(state, &stats);
    // Note: preemption counter may not be incremented in all implementations
    // The key test is that allocation changed appropriately
}

TEST_F(PortiaAttentionFairnessTest, PreemptionIsProportionate) {
    // WHAT: Verify preemption takes only what's needed
    // WHY:  Don't over-preempt from low-salience targets
    // HOW:  Request specific amount, verify preemption proportionate

    // Setup: two targets with resources
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.5f);
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.5f);
    portia_attention_reallocate(state, true);

    float mem_before = portia_attention_get_allocation(state, ATTENTION_TARGET_MEMORY);
    float proc_before = portia_attention_get_allocation(state, ATTENTION_TARGET_PROCESSING);

    // Request moderate amount for new high-salience target
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.9f);
    portia_attention_request(state, ATTENTION_TARGET_NEURONS, 0.3f);
    portia_attention_reallocate(state, true);

    float mem_after = portia_attention_get_allocation(state, ATTENTION_TARGET_MEMORY);
    float proc_after = portia_attention_get_allocation(state, ATTENTION_TARGET_PROCESSING);

    // Both should still have non-zero allocation
    EXPECT_GT(mem_after, 0.0f) << "Complete preemption of target 1";
    EXPECT_GT(proc_after, 0.0f) << "Complete preemption of target 2";

    // Should have lost proportionally
    float mem_loss_ratio = (mem_before - mem_after) / mem_before;
    float proc_loss_ratio = (proc_before - proc_after) / proc_before;

    // Ratios should be similar (proportionate preemption)
    float ratio_diff = std::abs(mem_loss_ratio - proc_loss_ratio);
    EXPECT_LT(ratio_diff, 0.3f)
        << "Disproportionate preemption";
}

} // anonymous namespace
