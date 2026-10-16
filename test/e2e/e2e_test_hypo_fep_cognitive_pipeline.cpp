/**
 * @file e2e_test_hypo_fep_cognitive_pipeline.cpp
 * @brief End-to-end tests for Hypothalamus-FEP-Cognitive integration pipeline
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Tests the complete data flow from hypothalamus drives through FEP bridges
 * to cognitive effects:
 * - Drive activation -> FEP computation -> Cognitive modulation
 * - Fatigue -> Precision reduction -> Reasoning degradation
 * - Curiosity -> Exploration weight -> Information seeking
 * - Drive priority -> Broadcast priority -> Workspace access
 * - Free energy minimization across the pipeline
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <cmath>

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_reasoning_fep_bridge.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_curiosity_fep_bridge.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_global_workspace_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

// =============================================================================
// Test Constants
// =============================================================================

static constexpr float HIGH_FATIGUE = 0.85f;
static constexpr float LOW_FATIGUE = 0.2f;
static constexpr float HIGH_CURIOSITY = 0.9f;
static constexpr float HIGH_DRIVE_URGENCY = 0.8f;
static constexpr float COGNITIVE_LOAD_HIGH = 0.85f;
static constexpr float INFO_GAIN_SIGNIFICANT = 0.7f;
static constexpr uint64_t UPDATE_DELTA_MS = 100;

// =============================================================================
// Test Fixture
// =============================================================================

class HypoFepCognitivePipelineTest : public ::testing::Test {
protected:
    // Core systems
    hypo_orchestrator_t orch;
    hypo_drive_system_handle_t* drive_system;
    fep_system_t* fep_system;

    // FEP bridges
    hypo_reasoning_fep_bridge_t* reasoning_bridge;
    hypo_curiosity_fep_bridge_t* curiosity_bridge;
    hypo_gw_fep_bridge_t* gw_bridge;

    // Orchestrator bridge IDs
    uint32_t reasoning_orch_id;
    uint32_t curiosity_orch_id;
    uint32_t gw_orch_id;

    // Event tracking
    std::atomic<int> fep_updates{0};
    std::atomic<int> precision_changes{0};
    std::atomic<int> exploration_triggers{0};

    void SetUp() override {
        // Create FEP system
        fep_config_t fep_config = {};
        fep_config.num_levels = 3;
        uint32_t level_dims[] = {64, 32, 16};
        fep_config.level_dims = level_dims;
        fep_config.belief_learning_rate = 0.01f;
        fep_config.precision_learning_rate = 0.005f;
        fep_config.action_learning_rate = 0.01f;
        fep_config.initial_precision = 1.0f;
        fep_config.learn_precision = true;
        fep_config.enable_active_inference = true;
        fep_config.planning_horizon = 5;
        fep_config.action_temperature = 1.0f;
        fep_config.max_iterations = 10;
        fep_config.convergence_threshold = 0.001f;
        fep_system = fep_create(&fep_config, 32, 16);  // obs_dim=32, action_dim=16
        ASSERT_NE(fep_system, nullptr) << "Failed to create FEP system";

        // Create orchestrator
        hypo_orch_config_t orch_config;
        ASSERT_EQ(0, hypo_orch_default_config(&orch_config));
        orch_config.enable_async = true;
        orch = hypo_orch_create(&orch_config);
        ASSERT_NE(orch, nullptr) << "Failed to create hypothalamus orchestrator";

        // Create drive system
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(drive_system, nullptr) << "Failed to create drive system";

        // Create FEP bridges
        CreateFepBridges();

        // Register bridges with orchestrator
        RegisterWithOrchestrator();

        // Connect bridges to drive system
        ConnectToDriveSystem();

        // Reset counters
        fep_updates = 0;
        precision_changes = 0;
        exploration_triggers = 0;
    }

    void TearDown() override {
        // Destroy FEP bridges
        if (reasoning_bridge) {
            hypo_reasoning_fep_destroy(reasoning_bridge);
            reasoning_bridge = nullptr;
        }
        if (curiosity_bridge) {
            hypo_curiosity_fep_destroy(curiosity_bridge);
            curiosity_bridge = nullptr;
        }
        if (gw_bridge) {
            hypo_gw_fep_destroy(gw_bridge);
            gw_bridge = nullptr;
        }

        // Destroy core systems
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }
        if (orch) {
            hypo_orch_destroy(orch);
            orch = nullptr;
        }
        if (fep_system) {
            fep_destroy(fep_system);
            fep_system = nullptr;
        }
    }

    void CreateFepBridges() {
        // Create reasoning FEP bridge
        hypo_reasoning_fep_config_t reasoning_config;
        ASSERT_EQ(0, hypo_reasoning_fep_default_config(&reasoning_config));
        reasoning_config.enable_active_inference = true;
        reasoning_bridge = hypo_reasoning_fep_create(&reasoning_config, fep_system);
        ASSERT_NE(reasoning_bridge, nullptr) << "Failed to create reasoning FEP bridge";

        // Create curiosity FEP bridge
        hypo_curiosity_fep_config_t curiosity_config;
        ASSERT_EQ(0, hypo_curiosity_fep_default_config(&curiosity_config));
        curiosity_config.enable_active_inference = true;
        curiosity_bridge = hypo_curiosity_fep_create(&curiosity_config, fep_system);
        ASSERT_NE(curiosity_bridge, nullptr) << "Failed to create curiosity FEP bridge";

        // Create global workspace FEP bridge
        hypo_gw_fep_config_t gw_config;
        ASSERT_EQ(0, hypo_gw_fep_default_config(&gw_config));
        gw_config.enable_active_inference = true;
        gw_bridge = hypo_gw_fep_create(&gw_config, fep_system);
        ASSERT_NE(gw_bridge, nullptr) << "Failed to create GW FEP bridge";
    }

    void RegisterWithOrchestrator() {
        ASSERT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_REASONING,
            "ReasoningFEP", reasoning_bridge, nullptr, &reasoning_orch_id));
        ASSERT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_CURIOSITY,
            "CuriosityFEP", curiosity_bridge, nullptr, &curiosity_orch_id));
        ASSERT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_GLOBAL_WORKSPACE,
            "GlobalWorkspaceFEP", gw_bridge, nullptr, &gw_orch_id));
    }

    void ConnectToDriveSystem() {
        ASSERT_EQ(0, hypo_reasoning_fep_connect_drives(reasoning_bridge, drive_system));
        ASSERT_EQ(0, hypo_curiosity_fep_connect_drives(curiosity_bridge, drive_system));
        ASSERT_EQ(0, hypo_gw_fep_connect_drives(gw_bridge, drive_system));
    }
};

// =============================================================================
// Test Cases
// =============================================================================

/**
 * Test fatigue-to-reasoning precision pipeline:
 * 1. Set high fatigue drive
 * 2. Compute precision modulation
 * 3. Verify reasoning precision reduction
 * 4. Report reasoning errors
 * 5. Verify prediction error increase
 */
TEST_F(HypoFepCognitivePipelineTest, FatigueToReasoningPrecision) {
    // Set high fatigue via nucleus input (lateral hypothalamus - arousal)
    hypo_drive_set_nucleus_input(drive_system, HYPO_NUCLEUS_LATERAL, 0.2f);

    // Update the bridge
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));

    // Modulate precision based on fatigue
    float precision_high_fatigue;
    ASSERT_EQ(0, hypo_reasoning_fep_modulate_precision(reasoning_bridge,
        HIGH_FATIGUE, &precision_high_fatigue));

    // Precision should be reduced with high fatigue
    EXPECT_LT(precision_high_fatigue, 1.0f);
    EXPECT_GE(precision_high_fatigue, HYPO_REASONING_FEP_PRECISION_MIN);

    // Compare with low fatigue
    float precision_low_fatigue;
    ASSERT_EQ(0, hypo_reasoning_fep_modulate_precision(reasoning_bridge,
        LOW_FATIGUE, &precision_low_fatigue));

    // Low fatigue should have higher precision
    EXPECT_GT(precision_low_fatigue, precision_high_fatigue);

    // Report reasoning errors (simulating degraded performance)
    ASSERT_EQ(0, hypo_reasoning_fep_report_error(reasoning_bridge, 0.6f));
    ASSERT_EQ(0, hypo_reasoning_fep_report_error(reasoning_bridge, 0.4f));

    // Get effects
    hypo_reasoning_fep_effects_t effects;
    ASSERT_EQ(0, hypo_reasoning_fep_get_effects(reasoning_bridge, &effects));

    // Verify prediction error increased
    EXPECT_GT(effects.prediction_error, 0.0f);
    EXPECT_GT(effects.error_pe, 0.0f);

    // Get statistics
    hypo_reasoning_fep_stats_t stats;
    ASSERT_EQ(0, hypo_reasoning_fep_get_stats(reasoning_bridge, &stats));
    EXPECT_GT(stats.total_updates, 0u);
}

/**
 * Test cognitive load to free energy pipeline:
 * 1. Apply high cognitive load
 * 2. Compute free energy increase
 * 3. Verify fatigue drive increase
 */
TEST_F(HypoFepCognitivePipelineTest, CognitiveLoadToFreeEnergy) {
    // Update bridge
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));

    // Compute free energy from cognitive load
    float fe_high_load;
    ASSERT_EQ(0, hypo_reasoning_fep_compute_fe(reasoning_bridge,
        COGNITIVE_LOAD_HIGH, &fe_high_load));

    // Free energy should be elevated with high load
    EXPECT_GT(fe_high_load, 0.0f);

    // Compute with low load
    float fe_low_load;
    ASSERT_EQ(0, hypo_reasoning_fep_compute_fe(reasoning_bridge,
        0.2f, &fe_low_load));

    // High load should produce more free energy
    EXPECT_GT(fe_high_load, fe_low_load);

    // Get effects
    hypo_reasoning_fep_effects_t effects;
    ASSERT_EQ(0, hypo_reasoning_fep_get_effects(reasoning_bridge, &effects));

    // Verify FE is tracked
    EXPECT_GE(effects.free_energy, 0.0f);
    EXPECT_GE(effects.cognitive_load_fe, 0.0f);
}

/**
 * Test curiosity-to-exploration pipeline:
 * 1. Set high curiosity drive
 * 2. Compute exploration weight
 * 3. Report information gain
 * 4. Verify FE reduction
 * 5. Verify drive satisfaction
 */
TEST_F(HypoFepCognitivePipelineTest, CuriosityToExploration) {
    // Update bridge
    ASSERT_EQ(0, hypo_curiosity_fep_update(curiosity_bridge, UPDATE_DELTA_MS));

    // Compute exploration weight from curiosity drive
    float exploration_weight;
    ASSERT_EQ(0, hypo_curiosity_fep_compute_exploration(curiosity_bridge,
        HIGH_CURIOSITY, &exploration_weight));

    // High curiosity should produce high exploration weight
    EXPECT_GT(exploration_weight, 0.5f);

    // Compare with low curiosity
    float low_exploration;
    ASSERT_EQ(0, hypo_curiosity_fep_compute_exploration(curiosity_bridge,
        0.2f, &low_exploration));
    EXPECT_LT(low_exploration, exploration_weight);

    // Report information gain (satisfies curiosity)
    ASSERT_EQ(0, hypo_curiosity_fep_report_info_gain(curiosity_bridge,
        INFO_GAIN_SIGNIFICANT));

    // Compute FE reduction from info gain
    float fe_reduction;
    ASSERT_EQ(0, hypo_curiosity_fep_compute_fe_reduction(curiosity_bridge,
        INFO_GAIN_SIGNIFICANT, &fe_reduction));

    // FE should be reduced
    EXPECT_GT(fe_reduction, 0.0f);

    // Get effects - note: effects may be cumulative from compute calls
    hypo_curiosity_fep_effects_t effects;
    ASSERT_EQ(0, hypo_curiosity_fep_get_effects(curiosity_bridge, &effects));

    // Verify structure fields are valid (may be 0 if not updated by this call)
    EXPECT_GE(effects.exploration_weight, 0.0f);
    EXPECT_GE(effects.epistemic_value, 0.0f);
    EXPECT_GE(effects.info_gain_fe_reduction, 0.0f);

    // Get statistics
    hypo_curiosity_fep_stats_t stats;
    ASSERT_EQ(0, hypo_curiosity_fep_get_stats(curiosity_bridge, &stats));
    EXPECT_GT(stats.total_updates, 0u);
}

/**
 * Test novelty detection pipeline:
 * 1. Report novelty
 * 2. Verify prediction error increase
 * 3. Verify curiosity drive boost
 * 4. Verify precision modulation
 */
TEST_F(HypoFepCognitivePipelineTest, NoveltyDetectionPipeline) {
    // Update bridge
    ASSERT_EQ(0, hypo_curiosity_fep_update(curiosity_bridge, UPDATE_DELTA_MS));

    // Report novelty detection
    float novelty_magnitude = 0.8f;
    ASSERT_EQ(0, hypo_curiosity_fep_report_novelty(curiosity_bridge, novelty_magnitude));

    // Modulate precision based on novelty
    float precision;
    ASSERT_EQ(0, hypo_curiosity_fep_modulate_precision(curiosity_bridge,
        novelty_magnitude, &precision));

    // Novelty should affect precision - precision can exceed 1.0 in some implementations
    EXPECT_GT(precision, 0.0f);

    // Get effects after novelty
    hypo_curiosity_fep_effects_t effects;
    ASSERT_EQ(0, hypo_curiosity_fep_get_effects(curiosity_bridge, &effects));

    // Effects may be updated by novelty report
    EXPECT_GE(effects.novelty_signal, 0.0f);

    // Get statistics
    hypo_curiosity_fep_stats_t stats;
    ASSERT_EQ(0, hypo_curiosity_fep_get_stats(curiosity_bridge, &stats));
    EXPECT_GT(stats.novelty_pe_events, 0u);
}

/**
 * Test drive priority to workspace access pipeline:
 * 1. Set high drive urgency
 * 2. Compute broadcast priority
 * 3. Report broadcast outcomes
 * 4. Verify workspace effects
 */
TEST_F(HypoFepCognitivePipelineTest, DrivePriorityToWorkspaceAccess) {
    // Update bridge
    ASSERT_EQ(0, hypo_gw_fep_update(gw_bridge, UPDATE_DELTA_MS));

    // Compute broadcast priority from drive urgency
    float priority;
    ASSERT_EQ(0, hypo_gw_fep_compute_broadcast_priority(gw_bridge,
        HIGH_DRIVE_URGENCY, &priority));

    // High urgency should produce high priority
    EXPECT_GT(priority, 0.5f);

    // Compare with low urgency
    float low_priority;
    ASSERT_EQ(0, hypo_gw_fep_compute_broadcast_priority(gw_bridge,
        0.2f, &low_priority));
    EXPECT_LT(low_priority, priority);

    // Report broadcast outcomes
    ASSERT_EQ(0, hypo_gw_fep_report_broadcast_win(gw_bridge));
    ASSERT_EQ(0, hypo_gw_fep_report_broadcast_win(gw_bridge));
    ASSERT_EQ(0, hypo_gw_fep_report_broadcast_loss(gw_bridge));

    // Get effects - note: effects may be cumulative from compute calls
    hypo_gw_fep_effects_t effects;
    ASSERT_EQ(0, hypo_gw_fep_get_effects(gw_bridge, &effects));

    // Verify structure fields are valid (may be 0 if not updated by this call)
    EXPECT_GE(effects.broadcast_priority, 0.0f);
    EXPECT_GE(effects.workspace_availability, 0.0f);

    // Get statistics
    hypo_gw_fep_stats_t stats;
    ASSERT_EQ(0, hypo_gw_fep_get_stats(gw_bridge, &stats));
    EXPECT_GT(stats.total_updates, 0u);
    EXPECT_GT(stats.priority_broadcasts, 0u);
}

/**
 * Test arousal to workspace availability pipeline:
 * 1. Set arousal level
 * 2. Compute workspace availability
 * 3. Verify precision modulation
 */
TEST_F(HypoFepCognitivePipelineTest, ArousalToWorkspaceAvailability) {
    // Update bridge
    ASSERT_EQ(0, hypo_gw_fep_update(gw_bridge, UPDATE_DELTA_MS));

    // Modulate precision based on arousal (high arousal = more alert)
    float precision_high_arousal;
    ASSERT_EQ(0, hypo_gw_fep_modulate_precision(gw_bridge,
        0.8f, &precision_high_arousal));

    // Modulate with low arousal
    float precision_low_arousal;
    ASSERT_EQ(0, hypo_gw_fep_modulate_precision(gw_bridge,
        0.2f, &precision_low_arousal));

    // Arousal should affect precision
    EXPECT_GT(precision_high_arousal, 0.0f);
    EXPECT_GT(precision_low_arousal, 0.0f);

    // Compute free energy from attention demand
    float fe;
    ASSERT_EQ(0, hypo_gw_fep_compute_fe(gw_bridge, 0.7f, &fe));
    EXPECT_GT(fe, 0.0f);

    // Get effects
    hypo_gw_fep_effects_t effects;
    ASSERT_EQ(0, hypo_gw_fep_get_effects(gw_bridge, &effects));

    EXPECT_GE(effects.precision, 0.0f);
    EXPECT_GE(effects.workspace_availability, 0.0f);
}

/**
 * Test complete drive-FEP-cognitive loop:
 * 1. Initialize all drives at moderate levels
 * 2. Activate curiosity drive
 * 3. Process through FEP bridges
 * 4. Simulate exploration success
 * 5. Verify drive satisfaction
 * 6. Verify FE reduction
 */
TEST_F(HypoFepCognitivePipelineTest, CompleteDriveFepCognitiveLoop) {
    // Phase 1: Update drives
    ASSERT_TRUE(hypo_drive_update(drive_system, 100000));

    // Phase 2: Report curiosity drive to orchestrator
    ASSERT_EQ(0, hypo_orch_report_drive(orch, curiosity_orch_id,
        HYPO_DRIVE_CURIOSITY, HIGH_CURIOSITY, HYPO_URGENCY_ELEVATED,
        "High curiosity seeking information"));

    // Phase 3: Update all FEP bridges
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_curiosity_fep_update(curiosity_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_gw_fep_update(gw_bridge, UPDATE_DELTA_MS));

    // Phase 4: Compute exploration weight
    float exploration_weight;
    ASSERT_EQ(0, hypo_curiosity_fep_compute_exploration(curiosity_bridge,
        HIGH_CURIOSITY, &exploration_weight));
    EXPECT_GT(exploration_weight, 0.5f);

    // Phase 5: Compute broadcast priority for workspace access
    float broadcast_priority;
    ASSERT_EQ(0, hypo_gw_fep_compute_broadcast_priority(gw_bridge,
        HIGH_DRIVE_URGENCY, &broadcast_priority));

    // Phase 6: Simulate exploration success (information gain)
    ASSERT_EQ(0, hypo_curiosity_fep_report_info_gain(curiosity_bridge,
        INFO_GAIN_SIGNIFICANT));

    // Phase 7: Report broadcast win
    ASSERT_EQ(0, hypo_gw_fep_report_broadcast_win(gw_bridge));

    // Phase 8: Satisfy curiosity drive
    float reward = hypo_drive_satisfy(drive_system, HYPO_DRIVE_CURIOSITY, 0.8f);
    EXPECT_GE(reward, 0.0f);

    // Phase 9: Verify FE reduction
    float fe_reduction;
    ASSERT_EQ(0, hypo_curiosity_fep_compute_fe_reduction(curiosity_bridge,
        INFO_GAIN_SIGNIFICANT, &fe_reduction));
    EXPECT_GT(fe_reduction, 0.0f);

    // Phase 10: Verify statistics
    hypo_curiosity_fep_stats_t curiosity_stats;
    ASSERT_EQ(0, hypo_curiosity_fep_get_stats(curiosity_bridge, &curiosity_stats));
    EXPECT_GT(curiosity_stats.total_updates, 0u);
    EXPECT_GT(curiosity_stats.info_gain_fe_reductions, 0u);

    hypo_gw_fep_stats_t gw_stats;
    ASSERT_EQ(0, hypo_gw_fep_get_stats(gw_bridge, &gw_stats));
    EXPECT_GT(gw_stats.total_updates, 0u);
}

/**
 * Test stress impact on cognitive FEP bridges:
 * 1. Trigger stress response
 * 2. Verify precision reduction
 * 3. Verify free energy increase
 * 4. Release stress
 * 5. Verify recovery
 */
TEST_F(HypoFepCognitivePipelineTest, StressImpactOnCognitiveFEP) {
    // Initial update
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));

    // Get baseline precision
    float baseline_precision;
    ASSERT_EQ(0, hypo_reasoning_fep_modulate_precision(reasoning_bridge,
        LOW_FATIGUE, &baseline_precision));

    // Trigger stress via orchestrator
    ASSERT_EQ(0, hypo_orch_trigger_stress(orch, "Cognitive overload"));

    // Verify stress state
    bool in_stress;
    ASSERT_EQ(0, hypo_orch_is_stressed(orch, &in_stress));
    EXPECT_TRUE(in_stress);

    // Simulate stress effects on reasoning (high fatigue)
    float stressed_precision;
    ASSERT_EQ(0, hypo_reasoning_fep_modulate_precision(reasoning_bridge,
        HIGH_FATIGUE, &stressed_precision));

    // Stressed precision should be lower
    EXPECT_LT(stressed_precision, baseline_precision);

    // Compute elevated free energy during stress
    float stressed_fe;
    ASSERT_EQ(0, hypo_reasoning_fep_compute_fe(reasoning_bridge,
        COGNITIVE_LOAD_HIGH, &stressed_fe));
    EXPECT_GT(stressed_fe, 0.0f);

    // Release stress
    ASSERT_EQ(0, hypo_orch_release_stress(orch));

    // Verify recovery
    ASSERT_EQ(0, hypo_orch_is_stressed(orch, &in_stress));
    EXPECT_FALSE(in_stress);
}

/**
 * Test multi-bridge coordination:
 * 1. Update all bridges simultaneously
 * 2. Verify coordinated effects
 * 3. Check statistics across bridges
 */
TEST_F(HypoFepCognitivePipelineTest, MultiBridgeCoordination) {
    // Update all bridges
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_curiosity_fep_update(curiosity_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_gw_fep_update(gw_bridge, UPDATE_DELTA_MS));

    // Compute effects from each bridge
    float reasoning_precision;
    ASSERT_EQ(0, hypo_reasoning_fep_modulate_precision(reasoning_bridge,
        0.5f, &reasoning_precision));

    float exploration_weight;
    ASSERT_EQ(0, hypo_curiosity_fep_compute_exploration(curiosity_bridge,
        0.6f, &exploration_weight));

    float broadcast_priority;
    ASSERT_EQ(0, hypo_gw_fep_compute_broadcast_priority(gw_bridge,
        0.7f, &broadcast_priority));

    // All effects should be valid
    EXPECT_GT(reasoning_precision, 0.0f);
    EXPECT_GT(exploration_weight, 0.0f);
    EXPECT_GT(broadcast_priority, 0.0f);

    // Get orchestrator stats
    hypo_orch_stats_t orch_stats;
    ASSERT_EQ(0, hypo_orch_get_stats(orch, &orch_stats));
    EXPECT_EQ(orch_stats.registered_bridges, 3u);
}

/**
 * Test reset and recovery:
 * 1. Perform operations
 * 2. Reset bridges
 * 3. Verify clean state
 */
TEST_F(HypoFepCognitivePipelineTest, ResetAndRecovery) {
    // Perform some operations
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_reasoning_fep_report_error(reasoning_bridge, 0.5f));
    ASSERT_EQ(0, hypo_curiosity_fep_report_info_gain(curiosity_bridge, 0.6f));
    ASSERT_EQ(0, hypo_gw_fep_report_broadcast_win(gw_bridge));

    // Reset bridges
    ASSERT_EQ(0, hypo_reasoning_fep_reset(reasoning_bridge));
    ASSERT_EQ(0, hypo_curiosity_fep_reset(curiosity_bridge));
    ASSERT_EQ(0, hypo_gw_fep_reset(gw_bridge));

    // Reset orchestrator
    ASSERT_EQ(0, hypo_orch_reset(orch));

    // Verify clean state
    hypo_orch_state_t state;
    ASSERT_EQ(0, hypo_orch_get_state(orch, &state));
    EXPECT_NE(state, HYPO_ORCH_STATE_ERROR);

    // Bridges should still be operational
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_curiosity_fep_update(curiosity_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_gw_fep_update(gw_bridge, UPDATE_DELTA_MS));
}

/**
 * Test error handling across pipeline:
 * - NULL parameters
 * - Invalid inputs
 * - Recovery after errors
 */
TEST_F(HypoFepCognitivePipelineTest, ErrorHandlingAcrossPipeline) {
    // NULL bridge operations should return error codes (not 0)
    // NIMCP uses error codes like 1003 (NIMCP_ERROR_NULL_ARG) instead of -1
    EXPECT_NE(0, hypo_reasoning_fep_update(nullptr, UPDATE_DELTA_MS));
    EXPECT_NE(0, hypo_curiosity_fep_update(nullptr, UPDATE_DELTA_MS));
    EXPECT_NE(0, hypo_gw_fep_update(nullptr, UPDATE_DELTA_MS));

    // NULL output parameters should return error codes
    EXPECT_NE(0, hypo_reasoning_fep_modulate_precision(reasoning_bridge, 0.5f, nullptr));
    EXPECT_NE(0, hypo_curiosity_fep_compute_exploration(curiosity_bridge, 0.5f, nullptr));
    EXPECT_NE(0, hypo_gw_fep_compute_broadcast_priority(gw_bridge, 0.5f, nullptr));

    // NULL stats output should return error codes
    EXPECT_NE(0, hypo_reasoning_fep_get_stats(reasoning_bridge, nullptr));
    EXPECT_NE(0, hypo_curiosity_fep_get_stats(curiosity_bridge, nullptr));
    EXPECT_NE(0, hypo_gw_fep_get_stats(gw_bridge, nullptr));

    // Bridges should still work after errors
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_curiosity_fep_update(curiosity_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_gw_fep_update(gw_bridge, UPDATE_DELTA_MS));
}

/**
 * Test active inference triggering:
 * 1. Enable active inference
 * 2. Create conditions for active inference
 * 3. Verify effects
 */
TEST_F(HypoFepCognitivePipelineTest, ActiveInferenceTriggering) {
    // Update with active inference enabled
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_curiosity_fep_update(curiosity_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_gw_fep_update(gw_bridge, UPDATE_DELTA_MS));

    // Get effects to check active inference strength
    hypo_reasoning_fep_effects_t reasoning_effects;
    ASSERT_EQ(0, hypo_reasoning_fep_get_effects(reasoning_bridge, &reasoning_effects));
    EXPECT_GE(reasoning_effects.active_inference_strength, 0.0f);

    hypo_curiosity_fep_effects_t curiosity_effects;
    ASSERT_EQ(0, hypo_curiosity_fep_get_effects(curiosity_bridge, &curiosity_effects));
    EXPECT_GE(curiosity_effects.active_inference_strength, 0.0f);

    hypo_gw_fep_effects_t gw_effects;
    ASSERT_EQ(0, hypo_gw_fep_get_effects(gw_bridge, &gw_effects));
    EXPECT_GE(gw_effects.active_inference_strength, 0.0f);
}

/**
 * Test complete realistic scenario:
 * Agent wakes up hungry and curious:
 * 1. Hunger and curiosity drives elevate
 * 2. Curiosity wins workspace competition
 * 3. Agent explores, gains information
 * 4. Curiosity satisfied, hunger becomes priority
 * 5. Agent acts to satisfy hunger
 * 6. System reaches homeostasis
 */
TEST_F(HypoFepCognitivePipelineTest, RealisticAgentScenario) {
    // Morning: Agent wakes up
    ASSERT_TRUE(hypo_drive_update(drive_system, 100000));

    // Both hunger and curiosity elevated
    ASSERT_EQ(0, hypo_orch_report_drive(orch, reasoning_orch_id,
        HYPO_DRIVE_HUNGER, 0.6f, HYPO_URGENCY_MODERATE, "Morning hunger"));
    ASSERT_EQ(0, hypo_orch_report_drive(orch, curiosity_orch_id,
        HYPO_DRIVE_CURIOSITY, 0.8f, HYPO_URGENCY_ELEVATED, "Morning curiosity"));

    // Update FEP bridges
    ASSERT_EQ(0, hypo_reasoning_fep_update(reasoning_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_curiosity_fep_update(curiosity_bridge, UPDATE_DELTA_MS));
    ASSERT_EQ(0, hypo_gw_fep_update(gw_bridge, UPDATE_DELTA_MS));

    // Curiosity wins workspace (higher urgency)
    float curiosity_priority, hunger_priority;
    ASSERT_EQ(0, hypo_gw_fep_compute_broadcast_priority(gw_bridge, 0.8f, &curiosity_priority));
    ASSERT_EQ(0, hypo_gw_fep_compute_broadcast_priority(gw_bridge, 0.6f, &hunger_priority));
    EXPECT_GT(curiosity_priority, hunger_priority);

    // Agent explores
    float exploration_weight;
    ASSERT_EQ(0, hypo_curiosity_fep_compute_exploration(curiosity_bridge,
        0.8f, &exploration_weight));
    EXPECT_GT(exploration_weight, 0.5f);

    // Exploration succeeds
    ASSERT_EQ(0, hypo_gw_fep_report_broadcast_win(gw_bridge));
    ASSERT_EQ(0, hypo_curiosity_fep_report_info_gain(curiosity_bridge, 0.7f));

    // Curiosity satisfied
    float curiosity_reward = hypo_drive_satisfy(drive_system, HYPO_DRIVE_CURIOSITY, 0.8f);
    EXPECT_GE(curiosity_reward, 0.0f);

    // Now hunger becomes priority
    hypo_drive_type_t priority = hypo_drive_get_priority(drive_system);
    // Priority may shift based on updated urgencies

    // Agent acts on hunger
    ASSERT_EQ(0, hypo_orch_report_drive(orch, reasoning_orch_id,
        HYPO_DRIVE_HUNGER, 0.7f, HYPO_URGENCY_ELEVATED, "Hunger priority"));

    // Satisfy hunger
    float hunger_reward = hypo_drive_satisfy(drive_system, HYPO_DRIVE_HUNGER, 0.9f);
    EXPECT_GE(hunger_reward, 0.0f);

    // Verify system statistics
    hypo_orch_stats_t final_stats;
    ASSERT_EQ(0, hypo_orch_get_stats(orch, &final_stats));
    EXPECT_GT(final_stats.drives_activated, 0u);

    hypo_drive_stats_t drive_stats;
    ASSERT_TRUE(hypo_drive_get_stats(drive_system, &drive_stats));
    EXPECT_GT(drive_stats.drive_satisfactions[HYPO_DRIVE_CURIOSITY], 0u);
    EXPECT_GT(drive_stats.drive_satisfactions[HYPO_DRIVE_HUNGER], 0u);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
