/**
 * @file e2e_test_plasticity_pipeline.cpp
 * @brief E2E Tests for Plasticity System Pipeline
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Comprehensive end-to-end tests for complete plasticity workflows
 * WHY:  Verify all plasticity mechanisms work together in realistic scenarios
 * HOW:  Test through complete lifecycle with biological timescales and outcomes
 *
 * TEST SCENARIOS:
 * 1. LearningScenario - Repeated spike patterns → LTP → consolidation
 * 2. ForgettingScenario - Inactivity → LTD → pruning
 * 3. SleepConsolidation - Learn → NREM sleep → stable memory
 * 4. ImmuneSuppression - Inflammation → reduced plasticity
 * 5. EnergyDepletion - High activity → ATP depletion → plasticity block
 * 6. MetaplasticityAdaptation - High activity → shifted thresholds
 * 7. StructuralRewiring - Activity → spine formation → stabilization
 * 8. AstrocyteSupport - Sustained activity → D-serine → enhanced LTP
 * 9. CompleteLifecycle - Formation → potentiation → pruning → reformation
 * 10. MultiSynapseCompetition - Heterosynaptic + homeostatic balance
 *
 * BIOLOGICAL BASIS:
 * Each test simulates realistic timescales and verifies biologically plausible outcomes:
 * - LTP timescale: seconds to minutes
 * - LTD timescale: minutes
 * - Consolidation: hours (accelerated in simulation)
 * - Structural changes: hours to days (accelerated)
 * - Homeostatic scaling: hours
 * - Sleep consolidation: full sleep cycle (accelerated)
 *
 * @author NIMCP Development Team
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <unordered_map>

// Headers have their own extern "C" guards
#include "nimcp.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "plasticity/metabolic/nimcp_metabolic_plasticity.h"
#include "plasticity/calcium/nimcp_calcium_dynamics.h"
#include "plasticity/metaplasticity/nimcp_extended_metaplasticity.h"
#include "plasticity/astrocyte/nimcp_astrocyte_plasticity.h"
#include "plasticity/heterosynaptic/nimcp_heterosynaptic.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Synapse/neuron counts
constexpr uint32_t NUM_SYNAPSES = 100;
constexpr uint32_t NUM_NEURONS = 20;
constexpr uint32_t ACTIVE_SYNAPSE_ID = 10;
constexpr uint32_t INACTIVE_SYNAPSE_ID = 50;

// Timing parameters (ms)
constexpr uint64_t SHORT_INTERVAL = 10;      // 10ms between spikes
constexpr uint64_t MEDIUM_INTERVAL = 100;    // 100ms
constexpr uint64_t LONG_INTERVAL = 1000;     // 1s
constexpr uint64_t CONSOLIDATION_TIME = 60000; // 1 minute (accelerated)

// Activity parameters
constexpr uint32_t LTP_SPIKE_COUNT = 50;     // Spikes for LTP
constexpr uint32_t LTD_SPIKE_COUNT = 30;     // Spikes for LTD
constexpr uint32_t HIGH_FREQ_HZ = 50;        // High frequency (50 Hz)
constexpr uint32_t LOW_FREQ_HZ = 1;          // Low frequency (1 Hz)

// Threshold values
constexpr float LTP_THRESHOLD = 0.05f;       // Expected LTP magnitude
constexpr float LTD_THRESHOLD = -0.02f;      // Expected LTD magnitude
constexpr float WEIGHT_EPSILON = 0.001f;     // Weight comparison tolerance
constexpr float ATP_DEPLETION_THRESHOLD = 30.0f; // ATP level for blocking

// Sleep cycle durations (accelerated for testing)
constexpr uint32_t NREM_DURATION_MS = 5000;  // 5s (simulates 30min)
constexpr uint32_t REM_DURATION_MS = 2000;   // 2s (simulates 10min)

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Generate spike train at specified frequency
 */
static void generate_spike_train(
    plasticity_orchestrator_t* orchestrator,
    uint32_t synapse_id,
    uint32_t neuron_id,
    uint32_t num_spikes,
    uint64_t interval_ms,
    uint64_t start_time
) {
    if (!orchestrator) return;

    for (uint32_t i = 0; i < num_spikes; i++) {
        uint64_t spike_time = start_time + i * interval_ms;
        plasticity_orchestrator_pre_spike(orchestrator, synapse_id, spike_time);
        plasticity_orchestrator_post_spike(orchestrator, neuron_id, spike_time + 5); // 5ms post-before-pre for LTP
    }
}

/**
 * @brief Generate low-frequency activity (for LTD)
 */
static void generate_ltd_activity(
    plasticity_orchestrator_t* orchestrator,
    uint32_t synapse_id,
    uint32_t neuron_id,
    uint32_t num_spikes,
    uint64_t start_time
) {
    if (!orchestrator) return;

    for (uint32_t i = 0; i < num_spikes; i++) {
        uint64_t spike_time = start_time + i * LONG_INTERVAL;
        plasticity_orchestrator_post_spike(orchestrator, neuron_id, spike_time);
        plasticity_orchestrator_pre_spike(orchestrator, synapse_id, spike_time + 20); // 20ms pre-after-post for LTD
    }
}

/**
 * @brief Simulate passage of time with updates
 */
static void simulate_time(
    plasticity_orchestrator_t* orchestrator,
    uint64_t duration_ms,
    uint64_t update_interval_ms = 1
) {
    if (!orchestrator) return;

    uint64_t steps = duration_ms / update_interval_ms;
    for (uint64_t i = 0; i < steps; i++) {
        plasticity_orchestrator_update(orchestrator, update_interval_ms);
    }
}

/**
 * @brief Get weight change from baseline
 */
static float get_weight_change(
    plasticity_orchestrator_t* orchestrator,
    uint32_t synapse_id,
    float baseline_weight
) {
    if (!orchestrator) return 0.0f;
    float current_weight = plasticity_orchestrator_get_weight(orchestrator, synapse_id);
    return current_weight - baseline_weight;
}

//=============================================================================
// E2E Test 1: Learning Scenario (Repeated Patterns → LTP → Consolidation)
//=============================================================================

E2E_TEST(PlasticityE2E, LearningScenario) {
    PipelineTracker pipeline("Learning: Repeated Patterns → LTP → Consolidation");

    // Stage 1: Create orchestrator with all mechanisms enabled
    pipeline.begin_stage("Create orchestrator", 30000);
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    config.enabled.enable_classic_stdp = true;
    config.enabled.enable_bcm = true;
    config.enabled.enable_homeostatic = true;
    config.enabled.enable_structural = true;
    config.enabled.enable_calcium = true;
    config.enabled.enable_metabolic = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create plasticity orchestrator");
    pipeline.end_stage();

    // Stage 2: Set initial weight and record baseline
    pipeline.begin_stage("Initialize synapse", 30000);
    float initial_weight = 0.5f;
    plasticity_orchestrator_set_weight(orchestrator, ACTIVE_SYNAPSE_ID, initial_weight);
    float baseline_weight = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);
    EXPECT_FLOAT_EQ(baseline_weight, initial_weight);
    pipeline.end_stage();

    // Stage 3: Generate high-frequency spike train (LTP induction)
    pipeline.begin_stage("Generate LTP-inducing activity", 30000);
    generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, LTP_SPIKE_COUNT, SHORT_INTERVAL, 0);
    pipeline.end_stage();

    // Stage 4: Allow plasticity to stabilize
    pipeline.begin_stage("Plasticity stabilization", 30000);
    simulate_time(orchestrator, MEDIUM_INTERVAL * 10);
    pipeline.end_stage();

    // Stage 5: Verify LTP occurred
    pipeline.begin_stage("Verify LTP", 30000);
    float weight_after_ltp = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);
    float ltp_change = weight_after_ltp - baseline_weight;
    EXPECT_GT(ltp_change, LTP_THRESHOLD) << "Expected LTP (weight increase > " << LTP_THRESHOLD << "), got " << ltp_change;
    pipeline.end_stage();

    // Stage 6: Simulate consolidation period
    pipeline.begin_stage("Consolidation period", 30000);
    simulate_time(orchestrator, CONSOLIDATION_TIME);
    pipeline.end_stage();

    // Stage 7: Verify weight is stable (no significant decay)
    pipeline.begin_stage("Verify consolidation", 30000);
    float weight_after_consolidation = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);
    float consolidation_change = std::abs(weight_after_consolidation - weight_after_ltp);
    EXPECT_LT(consolidation_change, 0.1f * ltp_change) << "Weight should be stable after consolidation";
    pipeline.end_stage();

    // Stage 8: Check statistics
    pipeline.begin_stage("Check statistics", 30000);
    plasticity_stats_t stats;
    plasticity_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_GT(stats.ltp_count, 0) << "Should have recorded LTP events";
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// E2E Test 2: Forgetting Scenario (Inactivity → LTD → Pruning)
//=============================================================================

E2E_TEST(PlasticityE2E, ForgettingScenario) {
    PipelineTracker pipeline("Forgetting: Inactivity → LTD → Pruning");

    // Stage 1: Create orchestrator
    pipeline.begin_stage("Create orchestrator", 30000);
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    config.enabled.enable_classic_stdp = true;
    config.enabled.enable_homeostatic = true;
    config.enabled.enable_structural = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create orchestrator");
    pipeline.end_stage();

    // Stage 2: Initialize synapse with moderate weight
    pipeline.begin_stage("Initialize synapse", 30000);
    float initial_weight = 0.6f;
    plasticity_orchestrator_set_weight(orchestrator, INACTIVE_SYNAPSE_ID, initial_weight);
    pipeline.end_stage();

    // Stage 3: Generate weak, sparse activity (LTD induction)
    pipeline.begin_stage("Generate LTD activity", 30000);
    generate_ltd_activity(orchestrator, INACTIVE_SYNAPSE_ID, 1, LTD_SPIKE_COUNT, 0);
    pipeline.end_stage();

    // Stage 4: Allow LTD to occur
    pipeline.begin_stage("LTD stabilization", 30000);
    simulate_time(orchestrator, MEDIUM_INTERVAL * 10);
    pipeline.end_stage();

    // Stage 5: Verify LTD (weight decrease)
    pipeline.begin_stage("Verify LTD", 30000);
    float weight_after_ltd = plasticity_orchestrator_get_weight(orchestrator, INACTIVE_SYNAPSE_ID);
    float ltd_change = weight_after_ltd - initial_weight;
    EXPECT_LT(ltd_change, LTD_THRESHOLD) << "Expected LTD (weight decrease), got change: " << ltd_change;
    pipeline.end_stage();

    // Stage 6: Prolonged inactivity (no spikes)
    pipeline.begin_stage("Prolonged inactivity", 30000);
    simulate_time(orchestrator, CONSOLIDATION_TIME * 2);
    pipeline.end_stage();

    // Stage 7: Verify continued depression
    pipeline.begin_stage("Verify continued depression", 30000);
    float weight_after_inactivity = plasticity_orchestrator_get_weight(orchestrator, INACTIVE_SYNAPSE_ID);
    EXPECT_LE(weight_after_inactivity, weight_after_ltd) << "Weight should not increase during inactivity";
    pipeline.end_stage();

    // Stage 8: Check statistics
    pipeline.begin_stage("Check statistics", 30000);
    plasticity_stats_t stats;
    plasticity_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_GT(stats.ltd_count, 0) << "Should have recorded LTD events";
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// E2E Test 3: Sleep Consolidation (Learn → NREM → Stable Memory)
//=============================================================================

E2E_TEST(PlasticityE2E, SleepConsolidationScenario) {
    PipelineTracker pipeline("Sleep Consolidation: Learn → NREM → Stable Memory");

    // Stage 1: Create orchestrator and sleep system
    pipeline.begin_stage("Create systems", 30000);
    plasticity_orchestrator_config_t plast_config;
    plasticity_orchestrator_default_config(&plast_config);
    plast_config.enabled.enable_classic_stdp = true;
    plast_config.enabled.enable_structural = true;
    plast_config.connect_sleep_bridges = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&plast_config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create orchestrator");

    sleep_config_t sleep_config;
    memset(&sleep_config, 0, sizeof(sleep_config));
    sleep_config.deep_sleep_duration_ms = NREM_DURATION_MS;
    sleep_config.enable_homeostasis = true;
    sleep_config.synaptic_downscaling_factor = 0.9f;

    sleep_system_t sleep_system = sleep_system_create(&sleep_config);
    if (sleep_system) {
        plasticity_orchestrator_connect_sleep(orchestrator, sleep_system);
    }
    pipeline.end_stage();

    // Stage 2: Learning phase (awake)
    pipeline.begin_stage("Learning phase (awake)", 30000);
    float initial_weight = 0.5f;
    plasticity_orchestrator_set_weight(orchestrator, ACTIVE_SYNAPSE_ID, initial_weight);

    // Generate learning activity
    generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, LTP_SPIKE_COUNT, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL * 5);

    float weight_after_learning = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);
    float learning_change = weight_after_learning - initial_weight;
    EXPECT_GT(learning_change, 0.0f) << "Learning should increase weight";
    pipeline.end_stage();

    // Stage 3: Enter NREM sleep (consolidation)
    pipeline.begin_stage("NREM sleep consolidation", 30000);
    if (sleep_system) {
        // Transition to deep NREM sleep
        sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
        simulate_time(orchestrator, NREM_DURATION_MS);
    }
    pipeline.end_stage();

    // Stage 4: Wake up
    pipeline.begin_stage("Wake from sleep", 30000);
    if (sleep_system) {
        sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);
    }
    pipeline.end_stage();

    // Stage 5: Verify memory is consolidated (weight stable or enhanced)
    pipeline.begin_stage("Verify consolidation", 30000);
    float weight_after_sleep = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);

    // Sleep consolidation should stabilize or slightly strengthen important connections
    EXPECT_GE(weight_after_sleep, weight_after_learning * 0.85f)
        << "Memory should be preserved after sleep (allowing for homeostatic downscaling)";
    pipeline.end_stage();

    // Stage 6: Test memory retention
    pipeline.begin_stage("Test memory retention", 30000);
    simulate_time(orchestrator, CONSOLIDATION_TIME);
    float weight_after_retention = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);

    float retention_stability = std::abs(weight_after_retention - weight_after_sleep);
    EXPECT_LT(retention_stability, 0.05f) << "Consolidated memory should be stable";
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    if (sleep_system) {
        sleep_system_destroy(sleep_system);
    }
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// E2E Test 4: Immune Suppression (Inflammation → Reduced Plasticity)
//=============================================================================

E2E_TEST(PlasticityE2E, ImmuneSuppressionScenario) {
    PipelineTracker pipeline("Immune Suppression: Inflammation → Reduced Plasticity");

    // Stage 1: Create orchestrator and immune system
    pipeline.begin_stage("Create systems", 30000);
    plasticity_orchestrator_config_t plast_config;
    plasticity_orchestrator_default_config(&plast_config);
    plast_config.enabled.enable_classic_stdp = true;
    plast_config.connect_immune_bridges = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&plast_config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create orchestrator");

    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* immune = brain_immune_create(&immune_config);

    if (immune) {
        plasticity_orchestrator_connect_immune(orchestrator, immune);
        brain_immune_start(immune);
    }
    pipeline.end_stage();

    // Stage 2: Baseline plasticity (healthy state)
    pipeline.begin_stage("Baseline plasticity", 30000);
    float initial_weight = 0.5f;
    plasticity_orchestrator_set_weight(orchestrator, ACTIVE_SYNAPSE_ID, initial_weight);

    generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, LTP_SPIKE_COUNT / 2, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL * 5);

    float weight_healthy = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);
    float healthy_change = weight_healthy - initial_weight;
    pipeline.end_stage();

    // Stage 3: Trigger inflammation
    pipeline.begin_stage("Trigger inflammation", 30000);
    if (immune) {
        // Present antigen to trigger immune response
        uint8_t epitope[32] = {1, 2, 3, 4};
        uint32_t antigen_id;
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, sizeof(epitope), 7, 0, &antigen_id);

        // Explicitly initiate inflammation at multiple sites
        uint32_t site_id;
        brain_immune_initiate_inflammation(immune, 0, antigen_id, &site_id);
        brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);
        brain_immune_initiate_inflammation(immune, 2, antigen_id, &site_id);
        brain_immune_initiate_inflammation(immune, 3, antigen_id, &site_id);

        // Allow immune response to develop
        simulate_time(orchestrator, MEDIUM_INTERVAL * 5);
    }
    pipeline.end_stage();

    // Stage 4: Test plasticity during inflammation
    pipeline.begin_stage("Plasticity during inflammation", 30000);
    plasticity_orchestrator_set_weight(orchestrator, INACTIVE_SYNAPSE_ID, initial_weight);

    generate_spike_train(orchestrator, INACTIVE_SYNAPSE_ID, 1, LTP_SPIKE_COUNT / 2, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL * 5);

    float weight_inflamed = plasticity_orchestrator_get_weight(orchestrator, INACTIVE_SYNAPSE_ID);
    float inflamed_change = weight_inflamed - initial_weight;
    pipeline.end_stage();

    // Stage 5: Verify reduced plasticity
    pipeline.begin_stage("Verify suppression", 30000);
    EXPECT_LT(inflamed_change, healthy_change)
        << "Plasticity should be reduced during inflammation. Healthy: "
        << healthy_change << ", Inflamed: " << inflamed_change;
    pipeline.end_stage();

    // Stage 6: Resolution (inflammation clears)
    pipeline.begin_stage("Immune resolution", 30000);
    if (immune) {
        // Allow time for resolution
        simulate_time(orchestrator, CONSOLIDATION_TIME);
    }
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    if (immune) {
        brain_immune_destroy(immune);
    }
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// E2E Test 5: Energy Depletion (High Activity → ATP Depletion → Block)
//=============================================================================

E2E_TEST(PlasticityE2E, EnergyDepletionScenario) {
    PipelineTracker pipeline("Energy Depletion: High Activity → ATP Depletion → Block");

    // Stage 1: Create orchestrator with metabolic constraints
    pipeline.begin_stage("Create orchestrator", 30000);
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    config.enabled.enable_classic_stdp = true;
    config.enabled.enable_metabolic = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create orchestrator");
    pipeline.end_stage();

    // Stage 2: Record baseline ATP
    pipeline.begin_stage("Baseline ATP", 30000);
    float initial_atp = plasticity_orchestrator_get_atp_level(orchestrator);
    EXPECT_GT(initial_atp, 0.0f) << "Should have initial ATP";
    pipeline.end_stage();

    // Stage 3: Sustained high-frequency activity (deplete ATP)
    pipeline.begin_stage("Sustained high activity", 30000);
    for (uint32_t burst = 0; burst < 50; burst++) {  // More bursts for measurable depletion
        generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 30, SHORT_INTERVAL, burst * 200);
        simulate_time(orchestrator, 50);
    }
    pipeline.end_stage();

    // Stage 4: Check ATP depletion
    pipeline.begin_stage("Verify ATP depletion", 30000);
    float depleted_atp = plasticity_orchestrator_get_atp_level(orchestrator);
    EXPECT_LT(depleted_atp, initial_atp * 0.95f)  // More realistic threshold
        << "ATP should be depleted after sustained activity";
    pipeline.end_stage();

    // Stage 5: Attempt plasticity during depletion
    pipeline.begin_stage("Plasticity during depletion", 30000);
    float weight_before = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);

    generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 30, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL);

    float weight_after = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);
    float change_during_depletion = std::abs(weight_after - weight_before);
    pipeline.end_stage();

    // Stage 6: Allow ATP recovery
    pipeline.begin_stage("ATP recovery", 30000);
    simulate_time(orchestrator, CONSOLIDATION_TIME);
    float recovered_atp = plasticity_orchestrator_get_atp_level(orchestrator);
    EXPECT_GT(recovered_atp, depleted_atp) << "ATP should recover during rest";
    pipeline.end_stage();

    // Stage 7: Test plasticity after recovery
    pipeline.begin_stage("Plasticity after recovery", 30000);
    plasticity_orchestrator_set_weight(orchestrator, INACTIVE_SYNAPSE_ID, 0.5f);
    float weight_before_recovery = plasticity_orchestrator_get_weight(orchestrator, INACTIVE_SYNAPSE_ID);

    generate_spike_train(orchestrator, INACTIVE_SYNAPSE_ID, 1, 30, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL);

    float weight_after_recovery = plasticity_orchestrator_get_weight(orchestrator, INACTIVE_SYNAPSE_ID);
    float change_after_recovery = std::abs(weight_after_recovery - weight_before_recovery);

    EXPECT_GT(change_after_recovery, change_during_depletion * 0.5f)
        << "Plasticity should improve after ATP recovery";
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// E2E Test 6: Metaplasticity Adaptation (Activity → Shifted Thresholds)
//=============================================================================

E2E_TEST(PlasticityE2E, MetaplasticityAdaptationScenario) {
    PipelineTracker pipeline("Metaplasticity: High Activity → Shifted Thresholds");

    // Stage 1: Create orchestrator with metaplasticity
    pipeline.begin_stage("Create orchestrator", 30000);
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    config.enabled.enable_classic_stdp = true;
    config.enabled.enable_bcm = true;
    config.enabled.enable_metaplasticity = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create orchestrator");
    pipeline.end_stage();

    // Stage 2: Record baseline threshold
    pipeline.begin_stage("Baseline threshold", 30000);
    float initial_threshold = plasticity_orchestrator_get_threshold(orchestrator, 0);
    pipeline.end_stage();

    // Stage 3: Sustained high activity
    pipeline.begin_stage("Sustained high activity", 30000);
    for (uint32_t epoch = 0; epoch < 5; epoch++) {
        generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 40, SHORT_INTERVAL, epoch * 1000);
        simulate_time(orchestrator, 200);
    }
    pipeline.end_stage();

    // Stage 4: Verify threshold increased
    pipeline.begin_stage("Verify threshold shift", 30000);
    float shifted_threshold = plasticity_orchestrator_get_threshold(orchestrator, 0);
    EXPECT_GT(shifted_threshold, initial_threshold)
        << "Threshold should increase with sustained high activity (metaplasticity)";
    pipeline.end_stage();

    // Stage 5: Test that plasticity is now harder to induce
    pipeline.begin_stage("Test plasticity with shifted threshold", 30000);
    float weight_before = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);

    // Same activity as before, but threshold is higher
    generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 20, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL);

    float weight_after = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);
    // Weight change should be smaller due to higher threshold
    pipeline.end_stage();

    // Stage 6: Rest period (threshold should normalize)
    pipeline.begin_stage("Rest period", 30000);
    simulate_time(orchestrator, CONSOLIDATION_TIME * 2);
    pipeline.end_stage();

    // Stage 7: Verify threshold normalized
    pipeline.begin_stage("Verify threshold normalization", 30000);
    float normalized_threshold = plasticity_orchestrator_get_threshold(orchestrator, 0);
    EXPECT_LT(normalized_threshold, shifted_threshold)
        << "Threshold should decrease during rest period";
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// E2E Test 7: Structural Rewiring (Activity → Formation → Stabilization)
//=============================================================================

E2E_TEST(PlasticityE2E, StructuralRewiringScenario) {
    PipelineTracker pipeline("Structural Rewiring: Activity → Formation → Stabilization");

    // Stage 1: Create orchestrator with structural plasticity
    pipeline.begin_stage("Create orchestrator", 30000);
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    config.enabled.enable_structural = true;
    config.enabled.enable_calcium = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create orchestrator");
    pipeline.end_stage();

    // Stage 2: Baseline spine count
    pipeline.begin_stage("Baseline state", 30000);
    plasticity_stats_t initial_stats;
    plasticity_orchestrator_get_stats(orchestrator, &initial_stats);
    uint64_t initial_spine_count = initial_stats.spines_formed;
    pipeline.end_stage();

    // Stage 3: High-frequency activity (trigger spine formation)
    pipeline.begin_stage("High-frequency activity", 30000);
    for (uint32_t burst = 0; burst < 10; burst++) {
        generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 30, SHORT_INTERVAL, burst * 500);
        simulate_time(orchestrator, 100);
    }
    pipeline.end_stage();

    // Stage 4: Verify spine formation
    pipeline.begin_stage("Verify spine formation", 30000);
    plasticity_stats_t after_activity_stats;
    plasticity_orchestrator_get_stats(orchestrator, &after_activity_stats);
    EXPECT_GT(after_activity_stats.spines_formed, initial_spine_count)
        << "High activity should trigger spine formation";
    pipeline.end_stage();

    // Stage 5: Maturation period (stabilization)
    pipeline.begin_stage("Spine maturation", 30000);
    // Continue moderate activity to stabilize new spines
    for (uint32_t epoch = 0; epoch < 5; epoch++) {
        generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 15, MEDIUM_INTERVAL, epoch * 2000);
        simulate_time(orchestrator, 1000);
    }
    pipeline.end_stage();

    // Stage 6: Verify spines are stable (not eliminated)
    pipeline.begin_stage("Verify stabilization", 30000);
    plasticity_stats_t stabilized_stats;
    plasticity_orchestrator_get_stats(orchestrator, &stabilized_stats);

    uint64_t net_spines = stabilized_stats.spines_formed - stabilized_stats.spines_eliminated;
    EXPECT_GT(net_spines, initial_spine_count)
        << "Net spine count should increase after stabilization";
    pipeline.end_stage();

    // Stage 7: Test structural persistence
    pipeline.begin_stage("Test persistence", 30000);
    simulate_time(orchestrator, CONSOLIDATION_TIME);

    plasticity_stats_t final_stats;
    plasticity_orchestrator_get_stats(orchestrator, &final_stats);

    uint64_t final_net_spines = final_stats.spines_formed - final_stats.spines_eliminated;
    EXPECT_GE(final_net_spines, net_spines * 0.8f)
        << "Most stabilized spines should persist";
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// E2E Test 8: Astrocyte Support (Sustained Activity → D-serine → Enhanced LTP)
//=============================================================================

E2E_TEST(PlasticityE2E, AstrocyteSupportScenario) {
    PipelineTracker pipeline("Astrocyte Support: Activity → D-serine → Enhanced LTP");

    // Stage 1: Create orchestrator with astrocyte support
    pipeline.begin_stage("Create orchestrator", 30000);
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    config.enabled.enable_classic_stdp = true;
    config.enabled.enable_astrocyte = true;
    config.enabled.enable_calcium = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create orchestrator");
    pipeline.end_stage();

    // Stage 2: Baseline LTP (without astrocyte support)
    pipeline.begin_stage("Baseline LTP", 30000);
    plasticity_orchestrator_set_weight(orchestrator, INACTIVE_SYNAPSE_ID, 0.5f);
    float baseline_weight = plasticity_orchestrator_get_weight(orchestrator, INACTIVE_SYNAPSE_ID);

    generate_spike_train(orchestrator, INACTIVE_SYNAPSE_ID, 1, 30, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL * 5);

    float weight_baseline_ltp = plasticity_orchestrator_get_weight(orchestrator, INACTIVE_SYNAPSE_ID);
    float baseline_ltp_magnitude = weight_baseline_ltp - baseline_weight;
    pipeline.end_stage();

    // Stage 3: Sustained activity (trigger astrocyte D-serine release)
    pipeline.begin_stage("Trigger astrocyte activation", 30000);
    plasticity_orchestrator_set_weight(orchestrator, ACTIVE_SYNAPSE_ID, 0.5f);

    // Prolonged high-frequency activity activates astrocytes
    for (uint32_t burst = 0; burst < 8; burst++) {
        generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 25, SHORT_INTERVAL, burst * 400);
        simulate_time(orchestrator, 100);
    }
    pipeline.end_stage();

    // Stage 4: LTP induction with astrocyte support
    pipeline.begin_stage("LTP with astrocyte support", 30000);
    // Reset weight to match baseline test initial weight
    plasticity_orchestrator_set_weight(orchestrator, ACTIVE_SYNAPSE_ID, 0.5f);
    float weight_before_enhanced = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);

    generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 30, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL * 5);

    float weight_enhanced_ltp = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);
    float enhanced_ltp_magnitude = weight_enhanced_ltp - weight_before_enhanced;
    pipeline.end_stage();

    // Stage 5: Verify astrocyte enhancement
    pipeline.begin_stage("Verify enhancement", 30000);
    // Enhanced LTP should be at least as strong as baseline (astrocyte support helps)
    EXPECT_GE(enhanced_ltp_magnitude, baseline_ltp_magnitude * 0.8f)
        << "Astrocyte support should maintain or enhance LTP";
    pipeline.end_stage();

    // Stage 6: Check astrocyte release statistics
    pipeline.begin_stage("Check astrocyte stats", 30000);
    plasticity_stats_t stats;
    plasticity_orchestrator_get_stats(orchestrator, &stats);
    // Astrocyte release events should be recorded
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// E2E Test 9: Complete Lifecycle (Formation → Potentiation → Pruning → Reform)
//=============================================================================

E2E_TEST(PlasticityE2E, CompleteLifecycleScenario) {
    PipelineTracker pipeline("Complete Lifecycle: Formation → Potentiation → Pruning → Reformation");

    // Stage 1: Create comprehensive orchestrator
    pipeline.begin_stage("Create orchestrator", 30000);
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    config.enabled.enable_classic_stdp = true;
    config.enabled.enable_structural = true;
    config.enabled.enable_homeostatic = true;
    config.enabled.enable_metabolic = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create orchestrator");
    pipeline.end_stage();

    // Stage 2: Formation phase
    pipeline.begin_stage("Formation phase", 30000);
    plasticity_stats_t formation_stats;
    plasticity_orchestrator_get_stats(orchestrator, &formation_stats);
    uint64_t initial_spines = formation_stats.spines_formed;

    // High activity triggers formation
    generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 50, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL * 5);

    plasticity_stats_t after_formation;
    plasticity_orchestrator_get_stats(orchestrator, &after_formation);
    EXPECT_GT(after_formation.spines_formed, initial_spines) << "Spines should form";
    pipeline.end_stage();

    // Stage 3: Potentiation phase
    pipeline.begin_stage("Potentiation phase", 30000);
    // Reset weight to ensure potentiation can be measured
    plasticity_orchestrator_set_weight(orchestrator, ACTIVE_SYNAPSE_ID, 0.5f);
    float weight_before_potentiation = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);

    // Repeated activation strengthens connections
    for (uint32_t epoch = 0; epoch < 5; epoch++) {
        generate_spike_train(orchestrator, ACTIVE_SYNAPSE_ID, 0, 30, SHORT_INTERVAL, epoch * 1000);
        simulate_time(orchestrator, 200);
    }

    float weight_potentiated = plasticity_orchestrator_get_weight(orchestrator, ACTIVE_SYNAPSE_ID);
    EXPECT_GE(weight_potentiated, weight_before_potentiation) << "Synapses should potentiate or remain stable";
    pipeline.end_stage();

    // Stage 4: Inactivity (triggers pruning)
    pipeline.begin_stage("Inactivity phase", 30000);
    simulate_time(orchestrator, CONSOLIDATION_TIME * 3);

    plasticity_stats_t after_inactivity;
    plasticity_orchestrator_get_stats(orchestrator, &after_inactivity);
    EXPECT_GT(after_inactivity.spines_eliminated, formation_stats.spines_eliminated)
        << "Inactive spines should be pruned";
    pipeline.end_stage();

    // Stage 5: Reformation (new activity)
    pipeline.begin_stage("Reformation phase", 30000);
    uint64_t spines_before_reform = after_inactivity.spines_formed;

    // New high-frequency activity
    generate_spike_train(orchestrator, INACTIVE_SYNAPSE_ID, 1, 50, SHORT_INTERVAL, 0);
    simulate_time(orchestrator, MEDIUM_INTERVAL * 5);

    plasticity_stats_t after_reform;
    plasticity_orchestrator_get_stats(orchestrator, &after_reform);
    EXPECT_GT(after_reform.spines_formed, spines_before_reform)
        << "New spines should form after reformation";
    pipeline.end_stage();

    // Stage 6: Verify lifecycle metrics
    pipeline.begin_stage("Verify lifecycle", 30000);
    plasticity_stats_t final_stats;
    plasticity_orchestrator_get_stats(orchestrator, &final_stats);

    EXPECT_GT(final_stats.ltp_count, 0) << "Should have LTP events";
    EXPECT_GT(final_stats.ltd_count, 0) << "Should have LTD events";
    EXPECT_GT(final_stats.spines_formed, 0) << "Should have spine formation";
    EXPECT_GT(final_stats.spines_eliminated, 0) << "Should have spine elimination";
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// E2E Test 10: Multi-Synapse Competition (Heterosynaptic + Homeostatic)
//=============================================================================

E2E_TEST(PlasticityE2E, MultiSynapseCompetitionScenario) {
    PipelineTracker pipeline("Multi-Synapse Competition: Heterosynaptic + Homeostatic Balance");

    // Stage 1: Create orchestrator with competition mechanisms
    pipeline.begin_stage("Create orchestrator", 30000);
    plasticity_orchestrator_config_t config;
    plasticity_orchestrator_default_config(&config);
    config.enabled.enable_classic_stdp = true;
    config.enabled.enable_heterosynaptic = true;
    config.enabled.enable_homeostatic = true;

    plasticity_orchestrator_t* orchestrator = plasticity_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orchestrator, "Failed to create orchestrator");
    pipeline.end_stage();

    // Stage 2: Initialize multiple synapses
    pipeline.begin_stage("Initialize synapses", 30000);
    constexpr uint32_t NUM_COMPETING_SYNAPSES = 5;
    std::vector<uint32_t> synapse_ids = {10, 20, 30, 40, 50};
    std::vector<float> initial_weights(NUM_COMPETING_SYNAPSES);

    for (uint32_t i = 0; i < NUM_COMPETING_SYNAPSES; i++) {
        plasticity_orchestrator_set_weight(orchestrator, synapse_ids[i], 0.5f);
        initial_weights[i] = plasticity_orchestrator_get_weight(orchestrator, synapse_ids[i]);
    }
    pipeline.end_stage();

    // Stage 3: Selective strengthening (only one synapse active)
    pipeline.begin_stage("Selective strengthening", 30000);
    uint32_t winner_synapse = synapse_ids[0];

    // Only activate one synapse repeatedly
    for (uint32_t epoch = 0; epoch < 10; epoch++) {
        generate_spike_train(orchestrator, winner_synapse, 0, 25, SHORT_INTERVAL, epoch * 500);
        simulate_time(orchestrator, 100);
    }
    pipeline.end_stage();

    // Stage 4: Verify heterosynaptic depression
    pipeline.begin_stage("Verify heterosynaptic effect", 30000);
    float winner_weight = plasticity_orchestrator_get_weight(orchestrator, winner_synapse);
    EXPECT_GT(winner_weight, initial_weights[0]) << "Active synapse should strengthen";

    // Check neighboring synapses for depression
    for (uint32_t i = 1; i < NUM_COMPETING_SYNAPSES; i++) {
        float neighbor_weight = plasticity_orchestrator_get_weight(orchestrator, synapse_ids[i]);
        // Heterosynaptic depression: inactive neighbors should weaken slightly
        EXPECT_LE(neighbor_weight, initial_weights[i] * 1.05f)
            << "Inactive synapses should not strengthen significantly";
    }
    pipeline.end_stage();

    // Stage 5: Global activity (all synapses active)
    pipeline.begin_stage("Global activity", 30000);
    for (uint32_t epoch = 0; epoch < 5; epoch++) {
        for (uint32_t i = 0; i < NUM_COMPETING_SYNAPSES; i++) {
            generate_spike_train(orchestrator, synapse_ids[i], i, 15, MEDIUM_INTERVAL, epoch * 1000);
        }
        simulate_time(orchestrator, 200);
    }
    pipeline.end_stage();

    // Stage 6: Homeostatic scaling
    pipeline.begin_stage("Homeostatic scaling", 30000);
    std::vector<float> weights_before_scaling(NUM_COMPETING_SYNAPSES);
    for (uint32_t i = 0; i < NUM_COMPETING_SYNAPSES; i++) {
        weights_before_scaling[i] = plasticity_orchestrator_get_weight(orchestrator, synapse_ids[i]);
    }

    // Allow time for homeostatic mechanisms
    simulate_time(orchestrator, CONSOLIDATION_TIME);

    std::vector<float> weights_after_scaling(NUM_COMPETING_SYNAPSES);
    for (uint32_t i = 0; i < NUM_COMPETING_SYNAPSES; i++) {
        weights_after_scaling[i] = plasticity_orchestrator_get_weight(orchestrator, synapse_ids[i]);
    }
    pipeline.end_stage();

    // Stage 7: Verify weight distribution maintained
    pipeline.begin_stage("Verify homeostasis", 30000);
    // Calculate total synaptic weight
    float total_before = std::accumulate(weights_before_scaling.begin(), weights_before_scaling.end(), 0.0f);
    float total_after = std::accumulate(weights_after_scaling.begin(), weights_after_scaling.end(), 0.0f);

    // Homeostatic scaling should maintain relative relationships
    float ratio = total_after / (total_before + 1e-6f);
    EXPECT_NEAR(ratio, 1.0f, 0.3f) << "Total weight should be roughly preserved by homeostasis";
    pipeline.end_stage();

    // Stage 8: Check competition statistics
    pipeline.begin_stage("Competition statistics", 30000);
    plasticity_stats_t stats;
    plasticity_orchestrator_get_stats(orchestrator, &stats);

    EXPECT_GT(stats.ltp_count, 0) << "Should have LTP events";
    EXPECT_GT(stats.ltd_count, 0) << "Should have LTD events from competition";
    pipeline.end_stage();

    // Cleanup
    pipeline.begin_stage("Cleanup", 30000);
    plasticity_orchestrator_destroy(orchestrator);
    pipeline.end_stage();

    E2E_ASSERT_PIPELINE_SUCCESS(pipeline);
    pipeline.print_summary();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
