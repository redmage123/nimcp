/**
 * @file test_phasic_tonic.cpp
 * @brief Unit tests for phasic-tonic dynamics (Phase C2.2 Enhancement #2)
 *
 * Tests:
 * - Tonic baseline maintenance
 * - Phasic burst triggering and decay
 * - TD error encoding (positive/negative/zero)
 * - Homeostatic regulation
 * - Autoreceptor feedback
 * - Burst statistics
 *
 * @version Phase C2.2 Enhancement #2
 * @date 2025-11-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>

    #include "plasticity/neuromodulators/nimcp_phasic_tonic.h"

// Helper to get current time in microseconds
static uint64_t get_time_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

// ============================================================================
// Test Fixture
// ============================================================================

class PhasicTonicTest : public ::testing::Test {
protected:
    void SetUp() override {
        current_time = get_time_us();

        // Initialize dopamine system
        phasic_tonic_config_t config = phasic_tonic_config_dopamine_default();
        phasic_tonic_init(&dopamine_state, &config, current_time);
    }

    phasic_tonic_state_t dopamine_state = {};
    uint64_t current_time;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(PhasicTonicTest, InitializesWithTonicBaseline) {
    EXPECT_NEAR(phasic_tonic_get_tonic_level(&dopamine_state), DOPAMINE_TONIC_BASELINE, 1e-6f);
    EXPECT_NEAR(phasic_tonic_get_phasic_burst(&dopamine_state), 0.0f, 1e-6f);
    EXPECT_FALSE(dopamine_state.in_burst_state);
    EXPECT_EQ(dopamine_state.burst_count, 0u);
}

TEST_F(PhasicTonicTest, TonicWithinPhysiologicalRange) {
    EXPECT_GE(phasic_tonic_get_tonic_level(&dopamine_state), DOPAMINE_TONIC_RANGE_MIN);
    EXPECT_LE(phasic_tonic_get_tonic_level(&dopamine_state), DOPAMINE_TONIC_RANGE_MAX);
}

// ============================================================================
// Tonic Baseline Tests
// ============================================================================

TEST_F(PhasicTonicTest, TonicRemainsStableWithoutStimulation) {
    float initial_tonic = phasic_tonic_get_tonic_level(&dopamine_state);

    // Simulate 10 seconds without stimulation
    for (int i = 0; i < 10000; i++) {
        phasic_tonic_update(&dopamine_state, 0.001f, current_time);
        current_time += 1000;  // 1ms increments
    }

    // Tonic should remain near baseline (within 5%)
    EXPECT_NEAR(phasic_tonic_get_tonic_level(&dopamine_state), initial_tonic, initial_tonic * 0.05f);
    EXPECT_EQ(phasic_tonic_get_phasic_burst(&dopamine_state), 0.0f);
}

TEST_F(PhasicTonicTest, HomeostaticRegulationRestoresTonic) {
    // Manually reduce tonic level
    phasic_tonic_set_tonic_level(&dopamine_state, DOPAMINE_TONIC_RANGE_MIN);

    // Simulate recovery (should restore toward target)
    for (int i = 0; i < 1000; i++) {
        phasic_tonic_update(&dopamine_state, 0.1f, current_time);  // 100ms steps
        current_time += 100000;
    }

    // Should have recovered significantly toward target
    EXPECT_GT(phasic_tonic_get_tonic_level(&dopamine_state), DOPAMINE_TONIC_RANGE_MIN * 1.5f);
}

// ============================================================================
// Phasic Burst Tests
// ============================================================================

TEST_F(PhasicTonicTest, BurstTriggering) {
    float burst_amplitude = 0.0005f;  // 500 nM

    bool triggered = phasic_tonic_trigger_burst(
        &dopamine_state,
        burst_amplitude,
        200,  // 200ms duration
        current_time
    );

    EXPECT_TRUE(triggered);
    EXPECT_TRUE(dopamine_state.in_burst_state);
    EXPECT_NEAR(phasic_tonic_get_phasic_burst(&dopamine_state), burst_amplitude, 1e-6f);
    EXPECT_EQ(dopamine_state.burst_count, 1u);
}

TEST_F(PhasicTonicTest, BurstExponentialDecay) {
    float burst_amplitude = 0.001f;  // 1 µM
    phasic_tonic_trigger_burst(&dopamine_state, burst_amplitude, 300, current_time);

    float initial_burst = phasic_tonic_get_phasic_burst(&dopamine_state);

    // Simulate decay over 1 second
    for (int i = 0; i < 1000; i++) {
        phasic_tonic_update(&dopamine_state, 0.001f, current_time);
        current_time += 1000;
    }

    // Burst should have decayed significantly
    EXPECT_LT(phasic_tonic_get_phasic_burst(&dopamine_state), initial_burst * 0.1f);
}

TEST_F(PhasicTonicTest, BurstTerminatesAfterDuration) {
    phasic_tonic_trigger_burst(&dopamine_state, 0.0008f, 200, current_time);  // 200ms burst

    EXPECT_TRUE(dopamine_state.in_burst_state);

    // Simulate 300ms (beyond duration)
    for (int i = 0; i < 300; i++) {
        phasic_tonic_update(&dopamine_state, 0.001f, current_time);
        current_time += 1000;
    }

    // Burst should have terminated
    EXPECT_FALSE(dopamine_state.in_burst_state);
    EXPECT_NEAR(phasic_tonic_get_phasic_burst(&dopamine_state), 0.0f, 1e-6f);
}

TEST_F(PhasicTonicTest, MultipleBurstsSuperpose) {
    // Trigger first burst
    phasic_tonic_trigger_burst(&dopamine_state, 0.0003f, 200, current_time);
    float first_amplitude = phasic_tonic_get_phasic_burst(&dopamine_state);

    // Trigger second burst while first is active
    current_time += 50000;  // 50ms later
    phasic_tonic_trigger_burst(&dopamine_state, 0.0003f, 200, current_time);

    // Total burst should be greater than first alone
    EXPECT_GT(phasic_tonic_get_phasic_burst(&dopamine_state), first_amplitude);
}

TEST_F(PhasicTonicTest, WeakBurstsRejected) {
    // Try to trigger burst below threshold
    float weak_amplitude = 0.00001f;  // 10 nM (very weak)

    bool triggered = phasic_tonic_trigger_burst(
        &dopamine_state,
        weak_amplitude,
        200,
        current_time
    );

    EXPECT_FALSE(triggered);
    EXPECT_FALSE(dopamine_state.in_burst_state);
}

// ============================================================================
// TD Error Encoding Tests
// ============================================================================

TEST_F(PhasicTonicTest, PositiveTDErrorTriggersBurst) {
    float td_error = 0.8f;  // Strong positive error

    bool triggered = phasic_tonic_encode_td_error(
        &dopamine_state,
        td_error,
        current_time
    );

    EXPECT_TRUE(triggered);
    EXPECT_TRUE(dopamine_state.in_burst_state);
    EXPECT_GT(phasic_tonic_get_phasic_burst(&dopamine_state), 0.0f);
    EXPECT_EQ(dopamine_state.burst_count, 1u);
}

TEST_F(PhasicTonicTest, NegativeTDErrorInducesTonicDip) {
    float initial_tonic = phasic_tonic_get_tonic_level(&dopamine_state);
    float td_error = -0.5f;  // Negative error (worse than expected)

    bool triggered = phasic_tonic_encode_td_error(
        &dopamine_state,
        td_error,
        current_time
    );

    EXPECT_FALSE(triggered);  // No burst
    EXPECT_FALSE(dopamine_state.in_burst_state);
    EXPECT_LT(phasic_tonic_get_tonic_level(&dopamine_state), initial_tonic);  // Tonic decreased
}

TEST_F(PhasicTonicTest, ZeroTDErrorNoChange) {
    float initial_tonic = phasic_tonic_get_tonic_level(&dopamine_state);
    float td_error = 0.0f;  // Expected outcome

    bool triggered = phasic_tonic_encode_td_error(
        &dopamine_state,
        td_error,
        current_time
    );

    EXPECT_FALSE(triggered);
    EXPECT_FALSE(dopamine_state.in_burst_state);
    EXPECT_NEAR(phasic_tonic_get_tonic_level(&dopamine_state), initial_tonic, 1e-6f);
}

TEST_F(PhasicTonicTest, LargerTDErrorProducesLargerBurst) {
    // Small TD error
    phasic_tonic_encode_td_error(&dopamine_state, 0.2f, current_time);
    float small_burst = phasic_tonic_get_phasic_burst(&dopamine_state);

    // Reset
    phasic_tonic_reset(&dopamine_state, current_time);

    // Large TD error
    phasic_tonic_encode_td_error(&dopamine_state, 0.8f, current_time);
    float large_burst = phasic_tonic_get_phasic_burst(&dopamine_state);

    EXPECT_GT(large_burst, small_burst);
}

// ============================================================================
// Total Concentration Tests
// ============================================================================

TEST_F(PhasicTonicTest, ConcentrationIsTonicPlusPhasic) {
    float tonic = phasic_tonic_get_tonic_level(&dopamine_state);

    // Trigger burst
    float burst_amp = 0.0005f;
    phasic_tonic_trigger_burst(&dopamine_state, burst_amp, 200, current_time);
    phasic_tonic_update(&dopamine_state, 0.001f, current_time);

    float total = phasic_tonic_get_concentration(&dopamine_state);
    float expected = tonic + phasic_tonic_get_phasic_burst(&dopamine_state);

    EXPECT_NEAR(total, expected, 1e-6f);
}

TEST_F(PhasicTonicTest, ConcentrationIncreasesDuringBurst) {
    float baseline_conc = phasic_tonic_get_concentration(&dopamine_state);

    // Trigger burst
    phasic_tonic_encode_td_error(&dopamine_state, 0.7f, current_time);
    phasic_tonic_update(&dopamine_state, 0.001f, current_time);

    float burst_conc = phasic_tonic_get_concentration(&dopamine_state);

    EXPECT_GT(burst_conc, baseline_conc);
}

// ============================================================================
// Autoreceptor Modulation Tests
// ============================================================================

TEST_F(PhasicTonicTest, AutoreceptorReducesTonicLevel) {
    float initial_tonic = phasic_tonic_get_tonic_level(&dopamine_state);

    // Apply inhibitory modulation (D2 autoreceptor activation)
    phasic_tonic_apply_autoreceptor_modulation(&dopamine_state, 0.5f);  // 50% reduction

    EXPECT_LT(phasic_tonic_get_tonic_level(&dopamine_state), initial_tonic);
    EXPECT_NEAR(phasic_tonic_get_tonic_level(&dopamine_state), initial_tonic * 0.5f, 1e-6f);
}

TEST_F(PhasicTonicTest, AutoreceptorEnhancesTonicLevel) {
    float initial_tonic = phasic_tonic_get_tonic_level(&dopamine_state);

    // Apply excitatory modulation
    phasic_tonic_apply_autoreceptor_modulation(&dopamine_state, 1.5f);  // 50% increase

    EXPECT_GT(phasic_tonic_get_tonic_level(&dopamine_state), initial_tonic);
    EXPECT_NEAR(phasic_tonic_get_tonic_level(&dopamine_state), initial_tonic * 1.5f, 1e-6f);
}

// ============================================================================
// Homeostatic Setpoint Tests
// ============================================================================

TEST_F(PhasicTonicTest, ChangingTonicTargetAffectsBaseline) {
    float original_target = phasic_tonic_get_tonic_target(&dopamine_state);
    float new_target = original_target * 1.5f;

    phasic_tonic_set_tonic_target(&dopamine_state, new_target);

    // Simulate time for homeostatic regulation
    for (int i = 0; i < 1000; i++) {
        phasic_tonic_update(&dopamine_state, 0.1f, current_time);
        current_time += 100000;
    }

    // Tonic level should have moved toward new target
    EXPECT_GT(phasic_tonic_get_tonic_level(&dopamine_state), original_target);
}

// ============================================================================
// Burst Statistics Tests
// ============================================================================

TEST_F(PhasicTonicTest, BurstCountIncrementsCorrectly) {
    EXPECT_EQ(dopamine_state.burst_count, 0u);

    // Trigger 3 bursts
    for (int i = 0; i < 3; i++) {
        phasic_tonic_encode_td_error(&dopamine_state, 0.5f, current_time);

        // Simulate time passing and let burst complete
        for (int j = 0; j < 500; j++) {
            phasic_tonic_update(&dopamine_state, 0.001f, current_time);
            current_time += 1000;  // 1ms increments for 500ms total
        }
    }

    uint32_t burst_count;
    phasic_tonic_get_burst_statistics(&dopamine_state, &burst_count, nullptr, nullptr, current_time);

    EXPECT_EQ(burst_count, 3u);
}

TEST_F(PhasicTonicTest, InterBurstIntervalTracked) {
    // Trigger multiple bursts at known intervals to let moving average converge
    for (int burst = 0; burst < 10; burst++) {
        phasic_tonic_encode_td_error(&dopamine_state, 0.5f, current_time);

        // Wait 1 second (allow burst to complete)
        for (int i = 0; i < 1000; i++) {
            phasic_tonic_update(&dopamine_state, 0.001f, current_time);
            current_time += 1000;  // 1ms increments
        }
    }

    float avg_interval;
    phasic_tonic_get_burst_statistics(&dopamine_state, nullptr, &avg_interval, nullptr, current_time);

    // Average interval should be approaching 1 second
    // Exponential moving average with alpha=0.1 converges slowly: after 10 samples, avg ≈ 0.61s
    EXPECT_GT(avg_interval, 0.5f);  // Should be at least halfway to target
    EXPECT_LT(avg_interval, 1.0f);  // Should not exceed the true interval
}

// ============================================================================
// Reset Tests
// ============================================================================

TEST_F(PhasicTonicTest, ResetRestoresBaseline) {
    // Trigger burst and modify tonic
    phasic_tonic_encode_td_error(&dopamine_state, 0.9f, current_time);
    phasic_tonic_set_tonic_target(&dopamine_state, DOPAMINE_TONIC_BASELINE * 1.5f);

    // Reset
    phasic_tonic_reset(&dopamine_state, current_time);

    EXPECT_NEAR(phasic_tonic_get_tonic_level(&dopamine_state), phasic_tonic_get_tonic_target(&dopamine_state), 1e-6f);
    EXPECT_EQ(phasic_tonic_get_phasic_burst(&dopamine_state), 0.0f);
    EXPECT_FALSE(dopamine_state.in_burst_state);
    EXPECT_EQ(dopamine_state.burst_count, 0u);
}

// ============================================================================
// Serotonin System Tests
// ============================================================================

TEST_F(PhasicTonicTest, SerotoninHasDifferentTimescales) {
    phasic_tonic_state_t serotonin_state = {};
    phasic_tonic_config_t config = phasic_tonic_config_serotonin_default();
    phasic_tonic_init(&serotonin_state, &config, current_time);

    // Serotonin should have slower homeostatic regulation
    EXPECT_GT(phasic_tonic_get_homeostatic_tau(&serotonin_state), phasic_tonic_get_homeostatic_tau(&dopamine_state));

    // And longer burst decay
    EXPECT_GT(phasic_tonic_get_burst_decay_tau(&serotonin_state), phasic_tonic_get_burst_decay_tau(&dopamine_state));
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(PhasicTonicTest, PerformanceUpdate10000Steps) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        phasic_tonic_update(&dopamine_state, 0.001f, current_time);
        current_time += 1000;

        // Occasional bursts
        if (i % 1000 == 0) {
            phasic_tonic_encode_td_error(&dopamine_state, 0.5f, current_time);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete in less than 5ms (5000 µs)
    EXPECT_LT(duration.count(), 5000) << "10000 updates should take < 5ms";

    std::cout << "Performance: 10000 phasic-tonic updates in " << duration.count() << " µs" << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
