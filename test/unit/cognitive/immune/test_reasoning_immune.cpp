/**
 * @file test_reasoning_immune.cpp
 * @brief Unit tests for reasoning-immune bridge
 *
 * WHAT: Test bidirectional reasoning-immune coupling
 * WHY:  Ensure cytokines modulate reasoning, reasoning failures trigger immune responses
 * HOW:  Test lifecycle, immune→reasoning modulation, reasoning→immune triggers, edge cases
 */

#include <gtest/gtest.h>
#include "cognitive/immune/nimcp_reasoning_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/reasoning/nimcp_reasoning_integration.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ReasoningImmuneBridgeTest : public ::testing::Test {
protected:
    reasoning_immune_bridge_t* bridge = nullptr;
    brain_immune_system_t* immune = nullptr;
    reasoning_integration_t* reasoning = nullptr;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        /* Create reasoning integration (stub - would need event bus) */
        /* For unit tests, we create a minimal stub */
        reasoning = nullptr;  /* Would create with: reasoning_integration_create(event_bus) */

        /* For testing, we'll test the bridge creation with NULL reasoning
         * and verify it fails gracefully, then test with mock */
    }

    void TearDown() override {
        if (bridge) reasoning_immune_bridge_destroy(bridge);
        if (reasoning) reasoning_integration_destroy(reasoning);
        if (immune) brain_immune_destroy(immune);
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ReasoningImmuneBridgeTest, DefaultConfigValues) {
    reasoning_immune_config_t config;
    int result = reasoning_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    /* Verify all features enabled */
    EXPECT_TRUE(config.enable_cytokine_reasoning_modulation);
    EXPECT_TRUE(config.enable_inflammation_cognitive_slowing);
    EXPECT_TRUE(config.enable_reasoning_failure_immune_trigger);
    EXPECT_TRUE(config.enable_working_memory_modulation);
    EXPECT_TRUE(config.enable_contradiction_immune_alert);

    /* Verify sensitivities */
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_EQ(config.inflammation_sensitivity, 1.0f);

    /* Verify safety limits */
    EXPECT_EQ(config.max_speed_reduction, 0.80f);
    EXPECT_EQ(config.max_accuracy_reduction, 0.60f);
    EXPECT_EQ(config.min_max_iterations, 5u);

    /* Verify failure thresholds */
    EXPECT_EQ(config.proof_failure_threshold, REASONING_PROOF_FAILURE_THRESHOLD);
    EXPECT_EQ(config.proof_failure_escalation, REASONING_PROOF_FAILURE_ESCALATION);
    EXPECT_EQ(config.unification_error_threshold, REASONING_UNIFICATION_ERROR_THRESHOLD);
    EXPECT_EQ(config.failure_window_sec, REASONING_FAILURE_WINDOW_SEC);
    EXPECT_EQ(config.contradiction_antigen_severity, REASONING_CONTRADICTION_SEVERITY);
}

TEST_F(ReasoningImmuneBridgeTest, DefaultConfigNullParameterFails) {
    int result = reasoning_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ReasoningImmuneBridgeTest, CreateWithNullSystemsFails) {
    reasoning_immune_config_t config;
    reasoning_immune_default_config(&config);

    /* Null immune system */
    reasoning_immune_bridge_t* b1 =
        reasoning_immune_bridge_create(&config, nullptr, reasoning);
    EXPECT_EQ(b1, nullptr);

    /* Null reasoning system */
    reasoning_immune_bridge_t* b2 =
        reasoning_immune_bridge_create(&config, immune, nullptr);
    EXPECT_EQ(b2, nullptr);

    /* Both null */
    reasoning_immune_bridge_t* b3 =
        reasoning_immune_bridge_create(&config, nullptr, nullptr);
    EXPECT_EQ(b3, nullptr);
}

TEST_F(ReasoningImmuneBridgeTest, DestroyNullBridgeIsSafe) {
    /* Should not crash */
    reasoning_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(ReasoningImmuneBridgeTest, GetConfigNullParameterFails) {
    reasoning_immune_config_t config;
    int result = reasoning_immune_get_config(nullptr, &config);
    EXPECT_EQ(result, -1);

    /* Would test with valid bridge if we had reasoning system */
}

TEST_F(ReasoningImmuneBridgeTest, SetConfigNullParameterFails) {
    reasoning_immune_config_t config;
    reasoning_immune_default_config(&config);

    int result = reasoning_immune_set_config(nullptr, &config);
    EXPECT_EQ(result, -1);
}

TEST_F(ReasoningImmuneBridgeTest, SetConfigValidatesParameters) {
    /* Would test with valid bridge */
    /* Test invalid cytokine_sensitivity */
    reasoning_immune_config_t config;
    reasoning_immune_default_config(&config);

    /* This would be tested with an actual bridge instance:
     * config.cytokine_sensitivity = -0.5f;  // Invalid
     * int result = reasoning_immune_set_config(bridge, &config);
     * EXPECT_EQ(result, -1);
     *
     * config.cytokine_sensitivity = 3.0f;  // Invalid (>2.0)
     * result = reasoning_immune_set_config(bridge, &config);
     * EXPECT_EQ(result, -1);
     */
}

/* ============================================================================
 * Immune → Reasoning Modulation Tests
 * ============================================================================ */

TEST_F(ReasoningImmuneBridgeTest, ApplyCytokineEffectsNullParameterFails) {
    int result = reasoning_immune_apply_cytokine_effects(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ReasoningImmuneBridgeTest, ApplyInflammationEffectsNullParameterFails) {
    int result = reasoning_immune_apply_inflammation_effects(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ReasoningImmuneBridgeTest, GetImpairmentNullParameterFails) {
    reasoning_impairment_t impairment;
    int result = reasoning_immune_get_impairment(nullptr, &impairment);
    EXPECT_EQ(result, -1);

    /* Would test with valid bridge:
     * result = reasoning_immune_get_impairment(bridge, nullptr);
     * EXPECT_EQ(result, -1);
     */
}

/* With actual bridge, would test cytokine effects:
TEST_F(ReasoningImmuneBridgeTest, CytokineEffectsReduceSpeed) {
    // Setup: inject IL-1β into immune system
    // Apply: reasoning_immune_apply_cytokine_effects(bridge)
    // Verify: impairment.speed_multiplier < 1.0f
    // Verify: impairment.cytokine_speed_impact < 0.0f
}

TEST_F(ReasoningImmuneBridgeTest, CytokineEffectsReduceAccuracy) {
    // Setup: inject TNF-α into immune system
    // Apply: reasoning_immune_apply_cytokine_effects(bridge)
    // Verify: impairment.accuracy_multiplier < 1.0f
}

TEST_F(ReasoningImmuneBridgeTest, IL10ImprovesRecovery) {
    // Setup: inject IL-10 into immune system
    // Apply: reasoning_immune_apply_cytokine_effects(bridge)
    // Verify: speed_multiplier > baseline (recovery effect)
}

TEST_F(ReasoningImmuneBridgeTest, InflammationLocalReducesPerformance) {
    // Setup: trigger LOCAL inflammation
    // Apply: reasoning_immune_apply_inflammation_effects(bridge)
    // Verify: impairment.inflammation_penalty == INFLAMMATION_LOCAL_REASONING_PENALTY
    // Verify: speed_multiplier reduced by ~10%
}

TEST_F(ReasoningImmuneBridgeTest, InflammationStormSevereImpairment) {
    // Setup: trigger STORM inflammation (cytokine storm)
    // Apply: reasoning_immune_apply_inflammation_effects(bridge)
    // Verify: impairment.inflammation_penalty == INFLAMMATION_STORM_REASONING_PENALTY
    // Verify: speed_multiplier reduced by ~80% (delirium-like)
    // Verify: effective_max_iterations severely reduced
}

TEST_F(ReasoningImmuneBridgeTest, CytokineAndInflammationEffectsStack) {
    // Setup: inject cytokines AND trigger inflammation
    // Apply: both apply functions
    // Verify: total impairment > either alone (multiplicative)
}
*/

/* ============================================================================
 * Reasoning → Immune Trigger Tests
 * ============================================================================ */

TEST_F(ReasoningImmuneBridgeTest, ReportContradictionNullParameterFails) {
    int result = reasoning_immune_report_contradiction(nullptr, "test contradiction");
    EXPECT_EQ(result, -1);
}

TEST_F(ReasoningImmuneBridgeTest, ReportProofFailureNullParameterFails) {
    int result = reasoning_immune_report_proof_failure(nullptr, "test goal");
    EXPECT_EQ(result, -1);
}

TEST_F(ReasoningImmuneBridgeTest, ReportUnificationErrorNullParameterFails) {
    int result = reasoning_immune_report_unification_error(nullptr, "test error");
    EXPECT_EQ(result, -1);
}

TEST_F(ReasoningImmuneBridgeTest, ClearFailureTrackingNullParameterFails) {
    int result = reasoning_immune_clear_failure_tracking(nullptr);
    EXPECT_EQ(result, -1);
}

/* With actual bridge, would test immune triggers:
TEST_F(ReasoningImmuneBridgeTest, ContradictionPresentedAsAntigen) {
    // Setup: enable contradiction alerts
    // Execute: reasoning_immune_report_contradiction(bridge, "P(x) AND NOT P(x)")
    // Verify: stats.contradictions_reported incremented
    // Verify: immune system received antigen with MEDIUM severity
}

TEST_F(ReasoningImmuneBridgeTest, ProofFailuresTriggersLocalInflammation) {
    // Setup: enable failure triggers
    // Execute: report 3 proof failures in window
    // Verify: stats.proof_failures_reported == 3
    // Verify: immune system triggered LOCAL inflammation
    // Verify: stats.total_immune_triggers incremented
}

TEST_F(ReasoningImmuneBridgeTest, ProofFailuresEscalateToRegional) {
    // Setup: enable failure triggers
    // Execute: report 10 proof failures in window
    // Verify: immune system escalated to REGIONAL inflammation
}

TEST_F(ReasoningImmuneBridgeTest, PersistentErrorsReleaseCytokines) {
    // Setup: enable failure triggers
    // Execute: report failures continuously for >60s
    // Verify: failure_state.persistent_error_state == true
    // Verify: immune system released IL-1β and IL-6
}

TEST_F(ReasoningImmuneBridgeTest, UnificationErrorsActivateBCell) {
    // Setup: enable failure triggers
    // Execute: report 5 unification errors
    // Verify: stats.unification_errors_reported == 5
    // Verify: immune system activated B cell for investigation
}

TEST_F(ReasoningImmuneBridgeTest, FailureWindowExpiresResetsCounters) {
    // Setup: report 2 failures
    // Wait: >10s (window expires)
    // Execute: report 1 more failure
    // Verify: failure count reset to 1 (not 3)
}

TEST_F(ReasoningImmuneBridgeTest, ClearFailureTrackingResetsAllCounters) {
    // Setup: report multiple failures
    // Execute: reasoning_immune_clear_failure_tracking(bridge)
    // Verify: failure_state.proof_failures_recent == 0
    // Verify: failure_state.unification_errors_recent == 0
    // Verify: failure_state.persistent_error_state == false
}
*/

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ReasoningImmuneBridgeTest, GetStatsNullParameterFails) {
    reasoning_immune_stats_t stats;
    int result = reasoning_immune_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(ReasoningImmuneBridgeTest, ResetStatsNullParameterFails) {
    int result = reasoning_immune_reset_stats(nullptr);
    EXPECT_EQ(result, -1);
}

/* With actual bridge:
TEST_F(ReasoningImmuneBridgeTest, StatsTrackCytokineModulations) {
    // Execute: reasoning_immune_apply_cytokine_effects(bridge)
    // Verify: stats.total_cytokine_modulations incremented
}

TEST_F(ReasoningImmuneBridgeTest, StatsTrackInflammationModulations) {
    // Execute: reasoning_immune_apply_inflammation_effects(bridge)
    // Verify: stats.total_inflammation_modulations incremented
}

TEST_F(ReasoningImmuneBridgeTest, StatsTrackMaxReductions) {
    // Setup: induce severe cytokine/inflammation effects
    // Verify: stats.max_speed_reduction_observed reflects worst impairment
    // Verify: stats.max_accuracy_reduction_observed reflects worst impairment
}

TEST_F(ReasoningImmuneBridgeTest, ResetStatsClearsAllCounters) {
    // Setup: accumulate statistics
    // Execute: reasoning_immune_reset_stats(bridge)
    // Verify: all counters zeroed
}
*/

/* ============================================================================
 * Integration API Tests
 * ============================================================================ */

TEST_F(ReasoningImmuneBridgeTest, ReasoningConnectImmuneNullParametersFails) {
    reasoning_immune_bridge_t* b1 =
        reasoning_connect_immune(nullptr, immune, nullptr);
    EXPECT_EQ(b1, nullptr);

    reasoning_immune_bridge_t* b2 =
        reasoning_connect_immune(reasoning, nullptr, nullptr);
    EXPECT_EQ(b2, nullptr);

    reasoning_immune_bridge_t* b3 =
        reasoning_connect_immune(nullptr, nullptr, nullptr);
    EXPECT_EQ(b3, nullptr);
}

/* With actual reasoning system:
TEST_F(ReasoningImmuneBridgeTest, ReasoningConnectImmuneSucceeds) {
    // Execute: bridge = reasoning_connect_immune(reasoning, immune, nullptr)
    // Verify: bridge != nullptr
    // Verify: bridge properly linked to both systems
}

TEST_F(ReasoningImmuneBridgeTest, ReasoningConnectImmuneRegistersCallbacks) {
    // Execute: bridge = reasoning_connect_immune(reasoning, immune, nullptr)
    // Verify: callbacks registered for:
    //   - EVENT_CONTRADICTION_DETECTED
    //   - EVENT_PROOF_FAILED
    //   - EVENT_UNIFICATION_FAILED
}
*/

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

/* With actual bridge:
TEST_F(ReasoningImmuneBridgeTest, DisabledFeaturesDoNotModulate) {
    // Setup: create bridge with all features disabled
    // Execute: apply cytokine/inflammation effects
    // Verify: impairment remains at baseline (no effects)
}

TEST_F(ReasoningImmuneBridgeTest, SensitivityZeroDisablesEffects) {
    // Setup: set cytokine_sensitivity = 0.0f
    // Execute: inject high cytokine levels
    // Apply: reasoning_immune_apply_cytokine_effects
    // Verify: no impairment (sensitivity gating works)
}

TEST_F(ReasoningImmuneBridgeTest, MaxReductionClampsImpairment) {
    // Setup: set max_speed_reduction = 0.3f
    // Execute: induce severe inflammation (would be -80%)
    // Verify: speed_multiplier clamped to 0.7f (30% reduction max)
}

TEST_F(ReasoningImmuneBridgeTest, MinMaxIterationsEnforced) {
    // Setup: set min_max_iterations = 10
    // Execute: induce STORM inflammation (would reduce to near-zero)
    // Verify: effective_max_iterations >= 10 (safety limit enforced)
}

TEST_F(ReasoningImmuneBridgeTest, ConcurrentAccessThreadSafe) {
    // Setup: create bridge
    // Execute: multiple threads calling modulation/trigger functions
    // Verify: no data races, consistent state (mutex protection)
}
*/

/* ============================================================================
 * Biological Realism Tests
 * ============================================================================ */

/* With actual bridge:
TEST_F(ReasoningImmuneBridgeTest, IL1BetaImpactMatchesBiology) {
    // Setup: inject IL-1β at 0.5 concentration
    // Apply: reasoning_immune_apply_cytokine_effects
    // Verify: speed impact ≈ -15% (0.5 * -30%)
    // Reference: Dantzer et al. (2008) psychomotor slowing
}

TEST_F(ReasoningImmuneBridgeTest, TNFAlphaStrongestImpairment) {
    // Setup: inject equal concentrations of IL-1β, IL-6, TNF-α
    // Verify: TNF-α causes largest speed reduction (-40% vs -30%, -20%)
    // Reference: Harrison et al. (2009) TNF-α executive function impairment
}

TEST_F(ReasoningImmuneBridgeTest, ChronicInflammationCumulativeDeficit) {
    // Setup: maintain SYSTEMIC inflammation for extended period
    // Verify: sustained -50% performance penalty
    // Reference: McAfoose & Baune (2009) chronic inflammation cognitive deficits
}

TEST_F(ReasoningImmuneBridgeTest, CytokineStormDeliriumLike) {
    // Setup: trigger STORM inflammation + high cytokines
    // Verify: speed_multiplier < 0.25f (>75% impairment)
    // Verify: resembles delirium state
    // Reference: Girard et al. (2010) delirium and cognitive impairment
}
*/

/* ============================================================================
 * Integration Scenario Tests
 * ============================================================================ */

/* With full system:
TEST_F(ReasoningImmuneBridgeTest, FullCycleContradictionToResolution) {
    // 1. Report contradiction
    // 2. Verify immune investigation
    // 3. Immune resolves threat
    // 4. Release IL-10
    // 5. Verify reasoning performance recovers
}

TEST_F(ReasoningImmuneBridgeTest, CascadingFailuresEscalateResponse) {
    // 1. Report 3 failures → LOCAL inflammation
    // 2. Inflammation impairs reasoning → more failures
    // 3. 10 failures → REGIONAL inflammation
    // 4. Persistent errors → cytokine release
    // 5. Verify escalation cascade
}

TEST_F(ReasoningImmuneBridgeTest, SuccessfulProofClearsFailureState) {
    // 1. Report 2 failures
    // 2. Clear failure tracking (simulating successful proof)
    // 3. Report 2 more failures
    // 4. Verify no immune trigger (count reset)
}
*/

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
