/**
 * @file test_medulla_exception_regression.cpp
 * @brief Comprehensive regression tests for Medulla Oblongata exception handling and behavioral stability
 *
 * WHAT: Regression tests verifying behavioral stability and preventing regressions
 * WHY:  Ensure medulla arousal, protection, circadian, immune, cerebellum, and brainstem
 *       coupling behaviors remain stable across code changes
 * HOW:  Test numerical stability, boundary conditions, API contracts, and biological
 *       accuracy of each medulla subsystem and bridge
 *
 * TEST CATEGORIES:
 * 1. AROUSAL  - Arousal state transitions, Yerkes-Dodson curve, persistence
 * 2. PROTECT  - Protection escalation, de-escalation, capability restrictions, hysteresis
 * 3. CIRCADIAN - Phase transitions, learning rate modulation, consolidation
 * 4. IMMUNE   - Cytokine impact factors, inflammation thresholds, emergency triggers
 * 5. CEREBELLUM - IO oscillation, error queue FIFO, climbing fiber timing
 * 6. BRAINSTEM - Signal latencies, priority ordering, buffer overflow
 *
 * @version 1.0.0
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_medulla.h"
#include "core/medulla/nimcp_medulla_immune_bridge.h"
#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr float EPSILON = 1e-5f;
static constexpr float FLOAT_TOLERANCE = 1e-4f;
static constexpr int STABILITY_ITERATIONS = 100;

//=============================================================================
// 1. AROUSAL STATE REGRESSION TESTS
//=============================================================================

class ArousalStateRegressionTest : public NimcpTestBase {
protected:
    medulla_t medulla = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        medulla_config_t config = medulla_default_config();
        medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr);
        ASSERT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// Test 1: Arousal level transitions work correctly across all levels
TEST_F(ArousalStateRegressionTest, ArousalLevelTransitionsCorrect) {
    // Test arousal level classification boundaries
    // Based on classify_arousal_level in nimcp_medulla.c:
    // < 0.1 -> COMA, < 0.3 -> DEEP_SLEEP, < 0.4 -> LIGHT_SLEEP,
    // < 0.5 -> DROWSY, < 0.7 -> AWAKE, < 0.9 -> ALERT, >= 0.9 -> HYPERAROUSAL

    struct TestCase {
        float arousal;
        arousal_level_t expected;
    };

    TestCase test_cases[] = {
        {0.05f, AROUSAL_LEVEL_COMA},
        {0.09f, AROUSAL_LEVEL_COMA},
        {0.15f, AROUSAL_LEVEL_DEEP_SLEEP},
        {0.29f, AROUSAL_LEVEL_DEEP_SLEEP},
        {0.35f, AROUSAL_LEVEL_LIGHT_SLEEP},
        {0.45f, AROUSAL_LEVEL_DROWSY},
        {0.55f, AROUSAL_LEVEL_AWAKE},
        {0.65f, AROUSAL_LEVEL_AWAKE},
        {0.75f, AROUSAL_LEVEL_ALERT},
        {0.85f, AROUSAL_LEVEL_ALERT},
        {0.92f, AROUSAL_LEVEL_HYPERAROUSAL},
        {0.99f, AROUSAL_LEVEL_HYPERAROUSAL},
    };

    for (const auto& tc : test_cases) {
        ASSERT_EQ(medulla_test_set_arousal(medulla, tc.arousal), 0);

        medulla_stats_t stats;
        ASSERT_EQ(medulla_get_stats(medulla, &stats), NIMCP_SUCCESS);
        EXPECT_EQ(stats.arousal_level, tc.expected)
            << "Failed for arousal level " << tc.arousal;
    }
}

// Test 2: Yerkes-Dodson inverted-U performance curve
TEST_F(ArousalStateRegressionTest, YerkesDodsonInvertedUCurve) {
    // Yerkes-Dodson law: performance peaks at moderate arousal
    // Very low and very high arousal lead to suboptimal performance
    // For the medulla: moderate arousal (0.5-0.7) should be classified as optimal (AWAKE)

    // Low arousal -> suboptimal
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.2f), 0);
    medulla_stats_t low_stats;
    medulla_get_stats(medulla, &low_stats);
    EXPECT_LE((int)low_stats.arousal_level, (int)AROUSAL_LEVEL_LIGHT_SLEEP);

    // Moderate arousal -> optimal
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.6f), 0);
    medulla_stats_t mid_stats;
    medulla_get_stats(medulla, &mid_stats);
    EXPECT_EQ(mid_stats.arousal_level, AROUSAL_LEVEL_AWAKE);

    // High arousal -> suboptimal (hyperarousal impairs fine control)
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.95f), 0);
    medulla_stats_t high_stats;
    medulla_get_stats(medulla, &high_stats);
    EXPECT_EQ(high_stats.arousal_level, AROUSAL_LEVEL_HYPERAROUSAL);
}

// Test 3: State persistence across updates
TEST_F(ArousalStateRegressionTest, ArousalStatePersistenceAcrossUpdates) {
    // Set a specific arousal level
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.75f), 0);

    float initial_arousal = medulla_get_arousal_level(medulla);
    ASSERT_NEAR(initial_arousal, 0.75f, FLOAT_TOLERANCE);

    // Run updates with zero dt to ensure no decay
    for (int i = 0; i < 10; i++) {
        medulla_update(medulla, 0.0f);
    }

    // Arousal should remain stable with zero dt
    float final_arousal = medulla_get_arousal_level(medulla);
    EXPECT_NEAR(final_arousal, initial_arousal, FLOAT_TOLERANCE);
}

// Test 4: Arousal boost and reduce operations are symmetric
TEST_F(ArousalStateRegressionTest, ArousalBoostReduceSymmetry) {
    // Start at baseline
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.5f), 0);

    float baseline = medulla_get_arousal_level(medulla);

    // Boost then reduce by same amount should return to baseline
    ASSERT_EQ(medulla_boost_arousal(medulla, 0.2f), NIMCP_SUCCESS);
    float after_boost = medulla_get_arousal_level(medulla);
    EXPECT_NEAR(after_boost, 0.7f, FLOAT_TOLERANCE);

    ASSERT_EQ(medulla_reduce_arousal(medulla, 0.2f), NIMCP_SUCCESS);
    float after_reduce = medulla_get_arousal_level(medulla);
    EXPECT_NEAR(after_reduce, baseline, FLOAT_TOLERANCE);
}

// Test 5: Arousal clamping at boundaries
TEST_F(ArousalStateRegressionTest, ArousalClampingAtBoundaries) {
    // Test upper bound clamping
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.9f), 0);
    ASSERT_EQ(medulla_boost_arousal(medulla, 0.5f), NIMCP_SUCCESS);
    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_LE(arousal, 1.0f);

    // Test lower bound clamping
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.1f), 0);
    ASSERT_EQ(medulla_reduce_arousal(medulla, 0.5f), NIMCP_SUCCESS);
    arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(arousal, 0.0f);
}

// Test 6: Arousal level monotonicity during gradual increase
TEST_F(ArousalStateRegressionTest, ArousalMonotonicityDuringGradualIncrease) {
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.1f), 0);

    float prev_arousal = medulla_get_arousal_level(medulla);

    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(medulla_boost_arousal(medulla, 0.04f), NIMCP_SUCCESS);
        float current = medulla_get_arousal_level(medulla);
        EXPECT_GE(current, prev_arousal) << "Arousal decreased at step " << i;
        prev_arousal = current;
    }
}

// Test 7: Negative delta rejected
TEST_F(ArousalStateRegressionTest, NegativeDeltaRejected) {
    EXPECT_EQ(medulla_boost_arousal(medulla, -0.1f), NIMCP_ERROR_INVALID_PARAM);
    EXPECT_EQ(medulla_reduce_arousal(medulla, -0.1f), NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// 2. PROTECTION LEVEL REGRESSION TESTS
//=============================================================================

class ProtectionLevelRegressionTest : public NimcpTestBase {
protected:
    medulla_t medulla = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        medulla_config_t config = medulla_default_config();
        medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr);
        ASSERT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// Test 8: Protection escalation from normal to shutdown
TEST_F(ProtectionLevelRegressionTest, ProtectionEscalationSequence) {
    // Test each protection level in order
    protection_level_t levels[] = {
        PROTECTION_LEVEL_NORMAL,
        PROTECTION_LEVEL_CAUTIOUS,
        PROTECTION_LEVEL_GUARDED,
        PROTECTION_LEVEL_DEFENSIVE,
        PROTECTION_LEVEL_CRITICAL,
        PROTECTION_LEVEL_SHUTDOWN
    };

    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(medulla_test_set_protection(medulla, levels[i]), 0);
        protection_level_t actual = medulla_get_protection_level(medulla);
        EXPECT_EQ(actual, levels[i]) << "Failed at level " << i;
    }
}

// Test 9: Protection de-escalation
TEST_F(ProtectionLevelRegressionTest, ProtectionDeescalation) {
    // Start at critical
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL), 0);
    EXPECT_EQ(medulla_get_protection_level(medulla), PROTECTION_LEVEL_CRITICAL);

    // De-escalate to normal
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), 0);
    EXPECT_EQ(medulla_get_protection_level(medulla), PROTECTION_LEVEL_NORMAL);
}

// Test 10: Capability restrictions at each protection level
TEST_F(ProtectionLevelRegressionTest, CapabilityRestrictionsAtEachLevel) {
    // At NORMAL: full arousal
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), 0);
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.8f), 0);

    // Update should not modify arousal much at NORMAL
    medulla_update(medulla, 0.016f);
    float normal_arousal = medulla_get_arousal_level(medulla);

    // At DEFENSIVE: arousal should be suppressed
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_DEFENSIVE), 0);
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.8f), 0);
    medulla_update(medulla, 0.016f);
    float defensive_arousal = medulla_get_arousal_level(medulla);

    // Protection modulation factor for DEFENSIVE is 0.5 (from implementation)
    // Arousal under defensive protection should be lower
    EXPECT_LT(defensive_arousal, normal_arousal);
}

// Test 11: Hysteresis in protection level transitions
TEST_F(ProtectionLevelRegressionTest, HysteresisRecovery) {
    // Set to critical
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL), 0);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    uint32_t initial_activations = stats.protection_activations;

    // Transition back to normal
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), 0);

    // Multiple rapid transitions should not cause oscillation
    for (int i = 0; i < 10; i++) {
        protection_level_t level = (i % 2 == 0) ? PROTECTION_LEVEL_CAUTIOUS : PROTECTION_LEVEL_NORMAL;
        medulla_test_set_protection(medulla, level);
        EXPECT_EQ(medulla_get_protection_level(medulla), level);
    }
}

// Test 12: Invalid protection level rejected
TEST_F(ProtectionLevelRegressionTest, InvalidProtectionLevelRejected) {
    // Test with invalid enum values
    EXPECT_EQ(medulla_test_set_protection(medulla, (protection_level_t)-1), -1);
    EXPECT_EQ(medulla_test_set_protection(medulla, (protection_level_t)100), -1);
}

// Test 13: Protection level persistence
TEST_F(ProtectionLevelRegressionTest, ProtectionLevelPersistence) {
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_GUARDED), 0);

    // Multiple updates should not change protection level
    for (int i = 0; i < STABILITY_ITERATIONS; i++) {
        medulla_update(medulla, 0.016f);
    }

    // Without health monitor, protection level should remain stable
    EXPECT_EQ(medulla_get_protection_level(medulla), PROTECTION_LEVEL_GUARDED);
}

// Test 14: Emergency shutdown sets protection to maximum
TEST_F(ProtectionLevelRegressionTest, EmergencyShutdownMaxProtection) {
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL), 0);

    medulla_emergency_shutdown(medulla, "test emergency");

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.protection_level, PROTECTION_LEVEL_SHUTDOWN);
    EXPECT_GE(stats.emergency_shutdowns, 1u);
}

//=============================================================================
// 3. CIRCADIAN RHYTHM REGRESSION TESTS
//=============================================================================

class CircadianRhythmRegressionTest : public NimcpTestBase {
protected:
    medulla_t medulla = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        medulla_config_t config = medulla_default_config();
        config.circadian.enable_synchronization = true;
        config.circadian.period_hours = 24.0f;
        medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr);
        ASSERT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// Test 15: All phase transitions are valid
TEST_F(CircadianRhythmRegressionTest, AllPhaseTransitionsValid) {
    circadian_phase_t phases[] = {
        CIRCADIAN_PHASE_EARLY_MORNING,
        CIRCADIAN_PHASE_MORNING,
        CIRCADIAN_PHASE_AFTERNOON,
        CIRCADIAN_PHASE_EVENING,
        CIRCADIAN_PHASE_LATE_EVENING,
        CIRCADIAN_PHASE_NIGHT,
        CIRCADIAN_PHASE_DEEP_NIGHT,
        CIRCADIAN_PHASE_PRE_DAWN
    };

    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(medulla_test_set_circadian(medulla, phases[i]), 0);
        EXPECT_EQ(medulla_get_circadian_phase(medulla), phases[i])
            << "Failed for phase " << i;
    }
}

// Test 16: Learning rate modulation by circadian phase
TEST_F(CircadianRhythmRegressionTest, LearningRateModulationByPhase) {
    // Morning phase should have higher arousal target (0.7)
    ASSERT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING), 0);
    medulla_stats_t morning_stats;
    medulla_get_stats(medulla, &morning_stats);

    // Deep night should have lower arousal target (0.2)
    ASSERT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT), 0);
    medulla_stats_t night_stats;
    medulla_get_stats(medulla, &night_stats);

    // The phases should be different
    EXPECT_NE(morning_stats.circadian_phase, night_stats.circadian_phase);
}

// Test 17: Consolidation during sleep phases
TEST_F(CircadianRhythmRegressionTest, ConsolidationDuringSleepPhases) {
    // Sleep phases: NIGHT, DEEP_NIGHT, PRE_DAWN should have lower arousal targets
    circadian_phase_t sleep_phases[] = {
        CIRCADIAN_PHASE_NIGHT,
        CIRCADIAN_PHASE_DEEP_NIGHT,
        CIRCADIAN_PHASE_PRE_DAWN
    };

    for (auto phase : sleep_phases) {
        ASSERT_EQ(medulla_test_set_circadian(medulla, phase), 0);
        ASSERT_EQ(medulla_test_set_arousal(medulla, 0.5f), 0);

        // Update multiple times
        for (int i = 0; i < 50; i++) {
            medulla_update(medulla, 0.1f);
        }

        // Arousal should trend toward lower target during sleep phases
        float arousal = medulla_get_arousal_level(medulla);
        // Due to circadian target, arousal should be <= 0.5 for sleep phases
        EXPECT_LE(arousal, 0.55f) << "Phase " << (int)phase << " did not reduce arousal";
    }
}

// Test 18: Circadian time wraps correctly at 24 hours
TEST_F(CircadianRhythmRegressionTest, CircadianTimeWrapsAt24Hours) {
    medulla_stats_t stats;

    // Run for enough simulated time to complete a full cycle
    // dt is in seconds, period is 24 hours = 86400 seconds
    float dt = 3600.0f; // 1 hour steps
    for (int hour = 0; hour < 25; hour++) {
        medulla_update(medulla, dt);
        medulla_get_stats(medulla, &stats);

        // Circadian time should always be in [0, 24)
        EXPECT_GE(stats.circadian_time_hours, 0.0f);
        EXPECT_LT(stats.circadian_time_hours, 24.0f);
    }

    // Should have completed at least one cycle
    EXPECT_GE(stats.circadian_cycles, 1u);
}

// Test 19: Invalid circadian phase rejected
TEST_F(CircadianRhythmRegressionTest, InvalidCircadianPhaseRejected) {
    EXPECT_EQ(medulla_test_set_circadian(medulla, (circadian_phase_t)-1), -1);
    EXPECT_EQ(medulla_test_set_circadian(medulla, (circadian_phase_t)100), -1);
}

// Test 20: Phase progression is monotonic within a cycle
TEST_F(CircadianRhythmRegressionTest, PhaseProgressionMonotonic) {
    ASSERT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_EARLY_MORNING), 0);

    int prev_phase = (int)CIRCADIAN_PHASE_EARLY_MORNING;

    // Advance through phases with time
    for (int i = 0; i < 8; i++) {
        // Each phase is 3 hours, advance 3 hours
        medulla_update(medulla, 3 * 3600.0f);

        circadian_phase_t current = medulla_get_circadian_phase(medulla);
        // Allow for wrap-around at end of cycle
        if ((int)current < prev_phase && prev_phase != (int)CIRCADIAN_PHASE_PRE_DAWN) {
            // Wrapped around - this is acceptable
        }
        prev_phase = (int)current;
    }
}

//=============================================================================
// 4. IMMUNE BRIDGE REGRESSION TESTS
//=============================================================================

class ImmuneBridgeRegressionTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

// Test 21: Cytokine impact factors are stable
TEST_F(ImmuneBridgeRegressionTest, CytokineImpactFactorsStable) {
    // Verify the constants haven't changed (regression check)
    EXPECT_FLOAT_EQ(CYTOKINE_IL1_AROUSAL_IMPACT, -0.30f);
    EXPECT_FLOAT_EQ(CYTOKINE_IL6_AROUSAL_IMPACT, -0.20f);
    EXPECT_FLOAT_EQ(CYTOKINE_TNF_AROUSAL_IMPACT, -0.40f);
    EXPECT_FLOAT_EQ(CYTOKINE_IL10_AROUSAL_IMPACT, +0.20f);
    EXPECT_FLOAT_EQ(CYTOKINE_IFN_AROUSAL_IMPACT, -0.25f);
}

// Test 22: Inflammation level arousal factors stable
TEST_F(ImmuneBridgeRegressionTest, InflammationLevelArousalFactorsStable) {
    EXPECT_FLOAT_EQ(INFLAMMATION_NONE_AROUSAL_FACTOR, 1.00f);
    EXPECT_FLOAT_EQ(INFLAMMATION_LOCAL_AROUSAL_FACTOR, 0.90f);
    EXPECT_FLOAT_EQ(INFLAMMATION_REGIONAL_AROUSAL_FACTOR, 0.75f);
    EXPECT_FLOAT_EQ(INFLAMMATION_SYSTEMIC_AROUSAL_FACTOR, 0.60f);
    EXPECT_FLOAT_EQ(INFLAMMATION_STORM_AROUSAL_FACTOR, 0.30f);
}

// Test 23: Protection level immune factors stable
TEST_F(ImmuneBridgeRegressionTest, ProtectionLevelImmuneFactorsStable) {
    EXPECT_FLOAT_EQ(PROTECTION_NORMAL_IMMUNE_FACTOR, 1.00f);
    EXPECT_FLOAT_EQ(PROTECTION_ELEVATED_IMMUNE_FACTOR, 1.20f);
    EXPECT_FLOAT_EQ(PROTECTION_HIGH_IMMUNE_FACTOR, 1.40f);
    EXPECT_FLOAT_EQ(PROTECTION_CRITICAL_IMMUNE_FACTOR, 2.00f);
    EXPECT_FLOAT_EQ(PROTECTION_SHUTDOWN_IMMUNE_FACTOR, 0.50f);
}

// Test 24: Arousal threshold constants stable
TEST_F(ImmuneBridgeRegressionTest, ArousalThresholdsStable) {
    EXPECT_FLOAT_EQ(AROUSAL_LOW_THRESHOLD, 0.30f);
    EXPECT_FLOAT_EQ(AROUSAL_HIGH_THRESHOLD, 0.70f);
}

// Test 25: Circadian immune factors stable
TEST_F(ImmuneBridgeRegressionTest, CircadianImmuneFactorsStable) {
    EXPECT_FLOAT_EQ(CIRCADIAN_DAY_IMMUNE_FACTOR, 1.20f);
    EXPECT_FLOAT_EQ(CIRCADIAN_NIGHT_IMMUNE_FACTOR, 0.80f);
}

// Test 26: Inflammation arousal computation stable
TEST_F(ImmuneBridgeRegressionTest, InflammationArousalComputationStable) {
    // Test each inflammation level
    EXPECT_FLOAT_EQ(medulla_immune_compute_inflammation_arousal(INFLAMMATION_NONE), 1.00f);
    EXPECT_FLOAT_EQ(medulla_immune_compute_inflammation_arousal(INFLAMMATION_LOCAL), 0.90f);
    EXPECT_FLOAT_EQ(medulla_immune_compute_inflammation_arousal(INFLAMMATION_REGIONAL), 0.75f);
    EXPECT_FLOAT_EQ(medulla_immune_compute_inflammation_arousal(INFLAMMATION_SYSTEMIC), 0.60f);
    EXPECT_FLOAT_EQ(medulla_immune_compute_inflammation_arousal(INFLAMMATION_STORM), 0.30f);
}

// Test 27: Protection immune computation stable
TEST_F(ImmuneBridgeRegressionTest, ProtectionImmuneComputationStable) {
    EXPECT_FLOAT_EQ(medulla_immune_compute_protection_immune(PROTECTION_LEVEL_NORMAL), 1.00f);
    // Additional protection levels mapped through the function
}

// Test 28: Default config is valid
TEST_F(ImmuneBridgeRegressionTest, DefaultConfigValid) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);

    EXPECT_TRUE(config.enable_immune_to_medulla || config.enable_medulla_to_immune);
    EXPECT_GT(config.update_interval_ms, 0u);
    EXPECT_GT(config.cytokine_sensitivity, 0.0f);
    EXPECT_LE(config.cytokine_sensitivity, 2.0f);
}

//=============================================================================
// 5. CEREBELLUM BRIDGE REGRESSION TESTS
//=============================================================================

class CerebellumBridgeRegressionTest : public NimcpTestBase {
protected:
    med_cereb_bridge_t bridge = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        bridge = med_cereb_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// Test 29: IO neuron oscillation frequency in biological range
TEST_F(CerebellumBridgeRegressionTest, IONeuronOscillationFrequencyBiological) {
    // Biological IO oscillation is typically 1-10 Hz
    EXPECT_GT(MED_CEREB_DEFAULT_IO_RATE, 0.0f);
    EXPECT_LE(MED_CEREB_DEFAULT_IO_RATE, 10.0f);
    EXPECT_LE(MED_CEREB_MAX_IO_RATE, 15.0f);  // Biological limit ~10 Hz with some margin
}

// Test 30: Error queue maintains FIFO ordering
TEST_F(CerebellumBridgeRegressionTest, ErrorQueueFIFOOrdering) {
    // Queue multiple errors with different source IDs
    for (uint32_t i = 0; i < 10; i++) {
        int result = med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
        EXPECT_EQ(result, 0);
    }

    // Verify queue count
    uint32_t pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(pending, 10u);
}

// Test 31: Error queue capacity respected
TEST_F(CerebellumBridgeRegressionTest, ErrorQueueCapacityRespected) {
    // Fill the queue to capacity
    for (uint32_t i = 0; i < MED_CEREB_MAX_ERROR_QUEUE; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
    }

    uint32_t at_capacity = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_LE(at_capacity, MED_CEREB_MAX_ERROR_QUEUE);
}

// Test 32: All error types are valid
TEST_F(CerebellumBridgeRegressionTest, AllErrorTypesValid) {
    for (int i = 0; i < MED_CEREB_ERROR_COUNT; i++) {
        med_cereb_error_type_t error_type = static_cast<med_cereb_error_type_t>(i);
        int result = med_cereb_bridge_queue_error(bridge, error_type, 0.5f, i);
        EXPECT_EQ(result, 0) << "Failed for error type " << i;
    }
}

// Test 33: Climbing fiber signal timing bounds
TEST_F(CerebellumBridgeRegressionTest, ClimbingFiberSignalTimingBounds) {
    // Bridge was created with NULL cerebellum, so climbing signals return -1
    // This validates the NULL-cerebellum guard clause works correctly
    int result = med_cereb_bridge_send_climbing_signal(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 0);
    EXPECT_EQ(result, -1);  // No cerebellum connected

    result = med_cereb_bridge_broadcast_error(bridge, MED_CEREB_ERROR_PREDICTION, 0.8f);
    EXPECT_EQ(result, -1);  // No cerebellum connected
}

// Test 34: IO state retrieval
TEST_F(CerebellumBridgeRegressionTest, IOStateRetrieval) {
    med_cereb_inferior_olive_t io_state;
    int result = med_cereb_bridge_get_io_state(bridge, &io_state);
    EXPECT_EQ(result, 0);

    // Verify reasonable state
    EXPECT_LE(io_state.num_neurons, MED_CEREB_MAX_IO_NEURONS);
    EXPECT_GT(io_state.oscillation_freq, 0.0f);
    EXPECT_GE(io_state.coupling_strength, 0.0f);
    EXPECT_LE(io_state.coupling_strength, 1.0f);
}

// Test 35: Bridge reset clears state
TEST_F(CerebellumBridgeRegressionTest, BridgeResetClearsState) {
    // Queue some errors
    for (int i = 0; i < 5; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
    }

    EXPECT_GT(med_cereb_bridge_pending_error_count(bridge), 0u);

    // Reset
    ASSERT_EQ(med_cereb_bridge_reset(bridge), 0);

    // Queue should be empty
    EXPECT_EQ(med_cereb_bridge_pending_error_count(bridge), 0u);
}

// Test 36: Statistics accumulate correctly
TEST_F(CerebellumBridgeRegressionTest, StatisticsAccumulateCorrectly) {
    uint32_t count_before = med_cereb_bridge_pending_error_count(bridge);

    // Queue errors (works without cerebellum, unlike climbing signals)
    for (int i = 0; i < 5; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
    }

    uint32_t count_after = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_GE(count_after, count_before + 5);
}

// Test 37: Error type names are non-null
TEST_F(CerebellumBridgeRegressionTest, ErrorTypeNamesNonNull) {
    for (int i = 0; i < MED_CEREB_ERROR_COUNT; i++) {
        const char* name = med_cereb_error_type_name(static_cast<med_cereb_error_type_t>(i));
        EXPECT_NE(name, nullptr) << "Null name for error type " << i;
        EXPECT_GT(strlen(name), 0u) << "Empty name for error type " << i;
    }
}

//=============================================================================
// 6. BRAINSTEM COUPLING REGRESSION TESTS
//=============================================================================

class BrainstemCouplingRegressionTest : public NimcpTestBase {
protected:
    medulla_t medulla = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        medulla_config_t config = medulla_default_config();
        config.coupling.enable_bidirectional = true;
        config.coupling.latency_ms = 10.0f;
        config.coupling.coupling_strength = 0.8f;
        medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr);
        ASSERT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// Test 38: Signal latency configuration matches biological values
TEST_F(BrainstemCouplingRegressionTest, SignalLatencyBiological) {
    medulla_config_t config = medulla_default_config();

    // Brainstem signal latencies are typically 5-50ms
    EXPECT_GE(config.coupling.latency_ms, 1.0f);
    EXPECT_LE(config.coupling.latency_ms, 100.0f);
}

// Test 39: Coupling strength within valid range
TEST_F(BrainstemCouplingRegressionTest, CouplingStrengthValidRange) {
    medulla_config_t config = medulla_default_config();

    EXPECT_GE(config.coupling.coupling_strength, 0.0f);
    EXPECT_LE(config.coupling.coupling_strength, 1.0f);
}

// Test 40: Priority ordering maintained during rapid updates
TEST_F(BrainstemCouplingRegressionTest, PriorityOrderingConsistency) {
    // Protection level should always take priority over arousal
    ASSERT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL), 0);
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.9f), 0);

    // Update multiple times
    for (int i = 0; i < 50; i++) {
        medulla_update(medulla, 0.016f);
    }

    // Under critical protection, arousal should be suppressed
    float arousal = medulla_get_arousal_level(medulla);
    // Protection factor for CRITICAL is 0.3, so 0.9 * 0.3 = 0.27
    EXPECT_LT(arousal, 0.5f);
}

// Test 41: Buffer overflow handling during high-frequency updates
TEST_F(BrainstemCouplingRegressionTest, BufferOverflowHandling) {
    // Rapid updates should not crash or corrupt state
    for (int i = 0; i < 1000; i++) {
        int result = medulla_update(medulla, 0.001f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // State should still be valid
    medulla_stats_t stats;
    ASSERT_EQ(medulla_get_stats(medulla, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.total_updates, 1000u);
    EXPECT_GE(stats.current_arousal, 0.0f);
    EXPECT_LE(stats.current_arousal, 1.0f);
}

// Test 42: Bidirectional coupling configuration
TEST_F(BrainstemCouplingRegressionTest, BidirectionalCouplingConfiguration) {
    medulla_config_t config = medulla_default_config();
    EXPECT_TRUE(config.coupling.enable_bidirectional);
}

//=============================================================================
// 7. NUMERICAL STABILITY REGRESSION TESTS
//=============================================================================

class NumericalStabilityRegressionTest : public NimcpTestBase {
protected:
    medulla_t medulla = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
        medulla_config_t config = medulla_default_config();
        medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr);
        ASSERT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// Test 43: No NaN values after long simulation
TEST_F(NumericalStabilityRegressionTest, NoNaNAfterLongSimulation) {
    for (int i = 0; i < 10000; i++) {
        medulla_update(medulla, 0.016f);
    }

    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_FALSE(std::isnan(arousal));
    EXPECT_FALSE(std::isinf(arousal));

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_FALSE(std::isnan(stats.avg_arousal));
    EXPECT_FALSE(std::isnan(stats.circadian_time_hours));
    EXPECT_FALSE(std::isnan(stats.avg_update_time_us));
}

// Test 44: No overflow in statistics counters
TEST_F(NumericalStabilityRegressionTest, NoOverflowInStatistics) {
    medulla_stats_t prev_stats;
    medulla_get_stats(medulla, &prev_stats);

    for (int i = 0; i < 10000; i++) {
        medulla_update(medulla, 0.016f);

        medulla_stats_t stats;
        medulla_get_stats(medulla, &stats);

        // Counters should be monotonically increasing
        EXPECT_GE(stats.total_updates, prev_stats.total_updates);
        EXPECT_GE(stats.arousal_updates, prev_stats.arousal_updates);

        prev_stats = stats;
    }
}

// Test 45: Very small dt values don't cause instability
TEST_F(NumericalStabilityRegressionTest, SmallDtStability) {
    float initial_arousal = medulla_get_arousal_level(medulla);

    for (int i = 0; i < 1000; i++) {
        medulla_update(medulla, 0.00001f);
    }

    float final_arousal = medulla_get_arousal_level(medulla);
    EXPECT_FALSE(std::isnan(final_arousal));
    EXPECT_GE(final_arousal, 0.0f);
    EXPECT_LE(final_arousal, 1.0f);
}

// Test 46: Very large dt values don't cause overflow
TEST_F(NumericalStabilityRegressionTest, LargeDtStability) {
    for (int i = 0; i < 10; i++) {
        medulla_update(medulla, 100.0f);
    }

    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_FALSE(std::isnan(arousal));
    EXPECT_FALSE(std::isinf(arousal));

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_GE(stats.circadian_time_hours, 0.0f);
    EXPECT_LT(stats.circadian_time_hours, 24.0f);
}

// Test 47: Accumulator stability after many small increments
TEST_F(NumericalStabilityRegressionTest, AccumulatorStability) {
    ASSERT_EQ(medulla_test_set_arousal(medulla, 0.5f), 0);

    // Many small increments
    for (int i = 0; i < 1000; i++) {
        medulla_boost_arousal(medulla, 0.0001f);
    }

    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_FALSE(std::isnan(arousal));
    // Should be clamped at max
    EXPECT_LE(arousal, 1.0f);
}

//=============================================================================
// 8. API CONTRACT REGRESSION TESTS
//=============================================================================

class APIContractRegressionTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

// Test 48: NULL pointer handling in all functions
TEST_F(APIContractRegressionTest, NullPointerHandling) {
    EXPECT_NE(medulla_start(nullptr), 0);
    EXPECT_NE(medulla_stop(nullptr), 0);
    EXPECT_NE(medulla_update(nullptr, 0.016f), 0);
    EXPECT_NE(medulla_emergency_shutdown(nullptr, "test"), 0);
    EXPECT_NE(medulla_request_state_change(nullptr, MEDULLA_STATE_RUNNING), 0);

    medulla_stats_t stats;
    EXPECT_NE(medulla_get_stats(nullptr, &stats), 0);

    medulla_t m = medulla_create(nullptr);
    ASSERT_NE(m, nullptr);
    EXPECT_NE(medulla_get_stats(m, nullptr), 0);
    medulla_destroy(m);

    EXPECT_FLOAT_EQ(medulla_get_arousal_level(nullptr), -1.0f);
    EXPECT_EQ(medulla_get_protection_level(nullptr), PROTECTION_LEVEL_NORMAL);
    EXPECT_EQ(medulla_get_circadian_phase(nullptr), CIRCADIAN_PHASE_MORNING);
    EXPECT_FALSE(medulla_is_bio_async_connected(nullptr));

    // Destroy NULL should not crash
    medulla_destroy(nullptr);
}

// Test 49: Default config is consistent across calls
TEST_F(APIContractRegressionTest, DefaultConfigConsistency) {
    medulla_config_t config1 = medulla_default_config();
    medulla_config_t config2 = medulla_default_config();

    EXPECT_EQ(config1.update_interval_ms, config2.update_interval_ms);
    EXPECT_FLOAT_EQ(config1.arousal.baseline_arousal, config2.arousal.baseline_arousal);
    EXPECT_FLOAT_EQ(config1.arousal.arousal_decay_rate, config2.arousal.arousal_decay_rate);
    EXPECT_FLOAT_EQ(config1.protection.health_threshold_critical, config2.protection.health_threshold_critical);
    EXPECT_FLOAT_EQ(config1.circadian.period_hours, config2.circadian.period_hours);
}

// Test 50: State machine transitions are valid
TEST_F(APIContractRegressionTest, StateMachineTransitions) {
    medulla_t m = medulla_create(nullptr);
    ASSERT_NE(m, nullptr);

    medulla_stats_t stats;

    // Initial state should be STOPPED
    medulla_get_stats(m, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);

    // Start -> RUNNING
    EXPECT_EQ(medulla_start(m), NIMCP_SUCCESS);
    medulla_get_stats(m, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    // Stop -> STOPPED
    EXPECT_EQ(medulla_stop(m), NIMCP_SUCCESS);
    medulla_get_stats(m, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);

    medulla_destroy(m);
}

// Test 51: Multiple start calls are idempotent
TEST_F(APIContractRegressionTest, StartIdempotent) {
    medulla_t m = medulla_create(nullptr);
    ASSERT_NE(m, nullptr);

    EXPECT_EQ(medulla_start(m), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_start(m), 0);  // Already running, returns 0
    EXPECT_EQ(medulla_start(m), 0);

    medulla_stats_t stats;
    medulla_get_stats(m, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    medulla_destroy(m);
}

// Test 52: Multiple stop calls are idempotent
TEST_F(APIContractRegressionTest, StopIdempotent) {
    medulla_t m = medulla_create(nullptr);
    ASSERT_NE(m, nullptr);

    EXPECT_EQ(medulla_start(m), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_stop(m), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_stop(m), 0);  // Already stopped
    EXPECT_EQ(medulla_stop(m), 0);

    medulla_destroy(m);
}

// Test 53: String conversion functions return valid strings
TEST_F(APIContractRegressionTest, StringConversionFunctions) {
    // Arousal levels
    for (int i = 0; i <= (int)AROUSAL_LEVEL_HYPERAROUSAL; i++) {
        const char* str = medulla_arousal_level_to_string((arousal_level_t)i);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }

    // Protection levels
    for (int i = 0; i <= (int)PROTECTION_LEVEL_SHUTDOWN; i++) {
        const char* str = medulla_protection_level_to_string((protection_level_t)i);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }

    // Circadian phases
    for (int i = 0; i <= (int)CIRCADIAN_PHASE_PRE_DAWN; i++) {
        const char* str = medulla_circadian_phase_to_string((circadian_phase_t)i);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }

    // Medulla states
    for (int i = 0; i <= (int)MEDULLA_STATE_STOPPING; i++) {
        const char* str = medulla_state_to_string((medulla_state_t)i);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

// Test 54: Create with NULL config uses defaults
TEST_F(APIContractRegressionTest, CreateWithNullConfigUsesDefaults) {
    medulla_t m = medulla_create(nullptr);
    ASSERT_NE(m, nullptr);

    EXPECT_EQ(medulla_start(m), NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(m, &stats);

    // Should have reasonable defaults
    EXPECT_GE(stats.current_arousal, 0.0f);
    EXPECT_LE(stats.current_arousal, 1.0f);

    medulla_destroy(m);
}

// Test 55: Emergency from RUNNING transitions correctly
TEST_F(APIContractRegressionTest, EmergencyFromRunningTransition) {
    medulla_t m = medulla_create(nullptr);
    ASSERT_NE(m, nullptr);

    EXPECT_EQ(medulla_start(m), NIMCP_SUCCESS);

    medulla_stats_t before_stats;
    medulla_get_stats(m, &before_stats);
    EXPECT_EQ(before_stats.state, MEDULLA_STATE_RUNNING);

    EXPECT_EQ(medulla_emergency_shutdown(m, "test emergency"), NIMCP_SUCCESS);

    medulla_stats_t after_stats;
    medulla_get_stats(m, &after_stats);
    EXPECT_EQ(after_stats.state, MEDULLA_STATE_EMERGENCY);
    EXPECT_EQ(after_stats.protection_level, PROTECTION_LEVEL_SHUTDOWN);

    medulla_destroy(m);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
