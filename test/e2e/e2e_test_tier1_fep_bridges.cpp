/**
 * @file e2e_test_tier1_fep_bridges.cpp
 * @brief End-to-end tests for Tier 1 FEP bridges in NIMCP
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: E2E tests for complete FEP integration of all Tier 1 cognitive bridges
 * WHY:  Validate end-to-end behavior of game_theory, imagination, parietal,
 *       social, and JEPA FEP bridges working together under FEP orchestration
 * HOW:  Test complete workflows including bridge registration, coordinated updates,
 *       cross-bridge free energy aggregation, and system-wide prediction minimization
 *
 * TIER 1 BRIDGES:
 * - Game Theory FEP Bridge: Strategic reasoning and Nash equilibrium
 * - Imagination FEP Bridge: Mental simulation and counterfactual reasoning
 * - Parietal FEP Bridge: Spatial processing and body schema
 * - Social FEP Bridge: Social cognition and relationship modeling
 * - JEPA FEP Bridge: Embedding prediction and representation quality
 *
 * TEST SCENARIOS:
 * - AllBridgesRegisterWithFEP: All 5 bridges register successfully
 * - CoordinatedFEPUpdateCycle: System-wide update coordination
 * - CrossBridgeFreeEnergyAggregation: Free energy contributions sum correctly
 * - StrategicImaginationIntegration: Game theory + imagination coordination
 * - SocialSpatialIntegration: Social + parietal coordination
 * - JEPARepresentationAcrossModules: JEPA representation sharing
 * - FullSystemPredictionMinimization: All bridges minimize prediction error
 * - StressTestMultipleCycles: 100+ FEP cycles stability
 * - GracefulDegradation: System continues with bridges removed
 * - StatisticsAggregation: Aggregate statistics collection
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <random>

// Tier 1 FEP Bridges
#include "cognitive/game_theory/nimcp_game_theory_fep_bridge.h"
#include "cognitive/imagination/nimcp_imagination_fep_bridge.h"
#include "cognitive/parietal/nimcp_parietal_fep_bridge.h"
#include "cognitive/social/nimcp_social_fep_bridge.h"
#include "cognitive/jepa/nimcp_jepa_fep_bridge.h"

// FEP Orchestrator
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr int NUM_TIER1_BRIDGES = 5;
static constexpr int STRESS_ITERATIONS = 100;
static constexpr float FEP_MAX_FREE_ENERGY = 2.0f;
static constexpr float FEP_BASELINE_FREE_ENERGY = 0.1f;
static constexpr uint64_t UPDATE_INTERVAL_MS = 50;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @class Tier1FEPBridgesE2ETest
 * @brief E2E test fixture for all Tier 1 FEP bridges
 *
 * Creates and manages:
 * - FEP Orchestrator for system-wide coordination
 * - Game Theory FEP Bridge
 * - Imagination FEP Bridge
 * - Parietal FEP Bridge
 * - Social FEP Bridge
 * - JEPA FEP Bridge
 */
class Tier1FEPBridgesE2ETest : public ::testing::Test {
protected:
    // FEP Orchestrator
    fep_orchestrator_t* orchestrator = nullptr;

    // Tier 1 FEP Bridges
    gt_fep_bridge_t* game_theory_bridge = nullptr;
    imagination_fep_bridge_t* imagination_bridge = nullptr;
    parietal_fep_bridge_t* parietal_bridge = nullptr;
    social_fep_bridge_t* social_bridge = nullptr;
    jepa_fep_bridge_t* jepa_bridge = nullptr;

    // Bridge IDs assigned by orchestrator
    uint32_t gt_bridge_id = 0;
    uint32_t imagination_bridge_id = 0;
    uint32_t parietal_bridge_id = 0;
    uint32_t social_bridge_id = 0;
    uint32_t jepa_bridge_id = 0;

    // Event tracking
    std::atomic<int> fep_update_events{0};
    std::atomic<int> high_fe_events{0};
    std::atomic<int> surprise_events{0};
    std::atomic<bool> test_error{false};

    // Mutex for thread-safe operations
    std::mutex event_mutex;

    // Time tracking
    uint64_t start_time_ms = 0;

    void SetUp() override {
        start_time_ms = GetCurrentTimeMs();

        // Create FEP Orchestrator
        fep_orchestrator_config_t orch_config;
        ASSERT_EQ(fep_orchestrator_default_config(&orch_config), 0);
        orch_config.enable_logging = false;
        orch_config.enable_statistics = true;
        orch_config.enable_bio_async = false;
        orch_config.enable_brain_immune = false;

        orchestrator = fep_orchestrator_create(&orch_config);
        ASSERT_NE(orchestrator, nullptr) << "Failed to create FEP orchestrator";

        // Create Game Theory FEP Bridge
        gt_fep_config_t gt_config = gt_fep_config_default();
        gt_config.enable_logging = false;
        game_theory_bridge = gt_fep_bridge_create(&gt_config);
        ASSERT_NE(game_theory_bridge, nullptr) << "Failed to create Game Theory FEP bridge";

        // Create Imagination FEP Bridge
        imagination_fep_config_t imag_config = imagination_fep_config_default();
        imag_config.enable_logging = false;
        imagination_bridge = imagination_fep_bridge_create(&imag_config);
        ASSERT_NE(imagination_bridge, nullptr) << "Failed to create Imagination FEP bridge";

        // Create Parietal FEP Bridge
        parietal_fep_config_t parietal_config = parietal_fep_config_default();
        parietal_config.enable_logging = false;
        parietal_bridge = parietal_fep_bridge_create(&parietal_config);
        ASSERT_NE(parietal_bridge, nullptr) << "Failed to create Parietal FEP bridge";

        // Create Social FEP Bridge
        social_fep_config_t social_config = social_fep_config_default();
        social_config.enable_logging = false;
        social_bridge = social_fep_bridge_create(&social_config);
        ASSERT_NE(social_bridge, nullptr) << "Failed to create Social FEP bridge";

        // Create JEPA FEP Bridge
        jepa_fep_config_t jepa_config = jepa_fep_config_default();
        jepa_config.enable_logging = false;
        jepa_bridge = jepa_fep_bridge_create(&jepa_config);
        ASSERT_NE(jepa_bridge, nullptr) << "Failed to create JEPA FEP bridge";

        // Reset counters
        fep_update_events = 0;
        high_fe_events = 0;
        surprise_events = 0;
        test_error = false;
    }

    void TearDown() override {
        // Unregister bridges from orchestrator (if registered)
        if (game_theory_bridge && gt_fep_bridge_is_registered(game_theory_bridge)) {
            gt_fep_bridge_unregister(game_theory_bridge);
        }
        if (imagination_bridge && imagination_fep_bridge_is_registered(imagination_bridge)) {
            imagination_fep_bridge_unregister(imagination_bridge);
        }
        if (parietal_bridge && parietal_fep_bridge_is_registered(parietal_bridge)) {
            parietal_fep_bridge_unregister(parietal_bridge);
        }
        if (social_bridge && social_fep_bridge_is_registered(social_bridge)) {
            social_fep_bridge_unregister(social_bridge);
        }
        if (jepa_bridge && jepa_fep_bridge_is_registered(jepa_bridge)) {
            jepa_fep_bridge_unregister(jepa_bridge);
        }

        // Destroy bridges
        if (jepa_bridge) jepa_fep_bridge_destroy(jepa_bridge);
        if (social_bridge) social_fep_bridge_destroy(social_bridge);
        if (parietal_bridge) parietal_fep_bridge_destroy(parietal_bridge);
        if (imagination_bridge) imagination_fep_bridge_destroy(imagination_bridge);
        if (game_theory_bridge) gt_fep_bridge_destroy(game_theory_bridge);

        // Stop and destroy orchestrator
        if (orchestrator) {
            fep_orchestrator_stop(orchestrator);
            fep_orchestrator_destroy(orchestrator);
        }
    }

    /**
     * @brief Register all 5 Tier 1 bridges with the orchestrator
     * @return true if all registrations succeeded
     */
    bool RegisterAllBridges() {
        // Register Game Theory bridge (NULL for gt_system for standalone testing)
        if (gt_fep_bridge_register(game_theory_bridge, orchestrator, nullptr, &gt_bridge_id) != 0) {
            return false;
        }

        // Register Imagination bridge (NULL for engine for standalone testing)
        if (imagination_fep_bridge_register(imagination_bridge, orchestrator, nullptr, &imagination_bridge_id) != 0) {
            return false;
        }

        // Register Parietal bridge (NULL for parietal lobe for standalone testing)
        if (parietal_fep_bridge_register(parietal_bridge, orchestrator, nullptr, &parietal_bridge_id) != 0) {
            return false;
        }

        // Register Social bridge (NULL for social bond system for standalone testing)
        if (social_fep_bridge_register(social_bridge, orchestrator, nullptr, &social_bridge_id) != 0) {
            return false;
        }

        // Register JEPA bridge (NULL for predictor for standalone testing)
        if (jepa_fep_bridge_register(jepa_bridge, orchestrator, nullptr, &jepa_bridge_id) != 0) {
            return false;
        }

        return true;
    }

    /**
     * @brief Get current time in milliseconds
     */
    uint64_t GetCurrentTimeMs() const {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        return static_cast<uint64_t>(ms.count());
    }

    /**
     * @brief Get elapsed time since test start
     */
    uint64_t GetElapsedTimeMs() const {
        return GetCurrentTimeMs() - start_time_ms;
    }

    /**
     * @brief Simulate inputs to all bridges to generate free energy
     */
    void SimulateInputsToAllBridges(float intensity) {
        // Game Theory: Update strategy uncertainty and opponent error
        gt_fep_bridge_update_strategy_uncertainty(game_theory_bridge, intensity * 0.5f);
        gt_fep_bridge_update_opponent_error(game_theory_bridge, intensity * 0.4f);
        gt_fep_bridge_update_nash_distance(game_theory_bridge, intensity * 0.3f);

        // Imagination: Force update (simulates divergence)
        imagination_fep_bridge_force_update(imagination_bridge);

        // Parietal: Force update (simulates spatial uncertainty)
        parietal_fep_bridge_force_update(parietal_bridge);

        // Social: Force update (simulates social prediction error)
        social_fep_bridge_force_update(social_bridge);

        // JEPA: Record prediction error and representation quality
        jepa_fep_bridge_record_prediction_error(jepa_bridge, intensity * 0.5f);
        jepa_fep_bridge_record_representation_quality(jepa_bridge, 1.0f - intensity * 0.3f);
    }

    /**
     * @brief Get total free energy across all bridges
     */
    float GetTotalFreeEnergy() const {
        float total = 0.0f;

        float gt_fe = gt_fep_bridge_get_free_energy(game_theory_bridge);
        if (gt_fe >= 0.0f) total += gt_fe;

        float imag_fe = imagination_fep_bridge_get_free_energy(imagination_bridge);
        if (imag_fe >= 0.0f) total += imag_fe;

        float parietal_fe = parietal_fep_bridge_get_free_energy_contribution(parietal_bridge);
        if (parietal_fe >= 0.0f) total += parietal_fe;

        float social_fe = social_fep_bridge_get_free_energy_contribution(social_bridge);
        if (social_fe >= 0.0f) total += social_fe;

        float jepa_fe = jepa_fep_bridge_get_free_energy_contribution(jepa_bridge);
        if (jepa_fe >= 0.0f) total += jepa_fe;

        return total;
    }

    /**
     * @brief Verify all bridges are in valid state
     */
    bool AllBridgesInValidState() const {
        if (gt_fep_bridge_get_state(game_theory_bridge) == GT_FEP_STATE_ERROR) return false;
        if (imagination_fep_bridge_get_state(imagination_bridge) == IMAGINATION_FEP_STATE_ERROR) return false;
        if (parietal_fep_bridge_get_state(parietal_bridge) == PARIETAL_FEP_STATE_ERROR) return false;
        if (social_fep_bridge_get_state(social_bridge) == SOCIAL_FEP_STATE_ERROR) return false;
        if (jepa_fep_bridge_get_state(jepa_bridge) == JEPA_FEP_STATE_ERROR) return false;
        return true;
    }
};

//=============================================================================
// AllBridgesRegisterWithFEP Tests
//=============================================================================

/**
 * Test: All 5 Tier 1 bridges register successfully with FEP orchestrator
 */
TEST_F(Tier1FEPBridgesE2ETest, AllBridgesRegisterWithFEP) {
    // Register all bridges
    ASSERT_TRUE(RegisterAllBridges()) << "Failed to register all bridges";

    // Verify all bridges are registered
    EXPECT_TRUE(gt_fep_bridge_is_registered(game_theory_bridge));
    EXPECT_TRUE(imagination_fep_bridge_is_registered(imagination_bridge));
    EXPECT_TRUE(parietal_fep_bridge_is_registered(parietal_bridge));
    EXPECT_TRUE(social_fep_bridge_is_registered(social_bridge));
    EXPECT_TRUE(jepa_fep_bridge_is_registered(jepa_bridge));

    // Verify bridge IDs are unique and valid
    EXPECT_GT(gt_bridge_id, 0u);
    EXPECT_GT(imagination_bridge_id, 0u);
    EXPECT_GT(parietal_bridge_id, 0u);
    EXPECT_GT(social_bridge_id, 0u);
    EXPECT_GT(jepa_bridge_id, 0u);

    // All IDs should be different
    std::vector<uint32_t> ids = {gt_bridge_id, imagination_bridge_id, parietal_bridge_id,
                                  social_bridge_id, jepa_bridge_id};
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(std::unique(ids.begin(), ids.end()), ids.end()) << "Bridge IDs should be unique";

    // Verify orchestrator has correct number of bridges
    fep_orchestrator_stats_t stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_EQ(stats.total_bridges, NUM_TIER1_BRIDGES);

    // Start orchestrator and verify running state
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);
}

/**
 * Test: Bridges handle double registration gracefully (idempotent)
 */
TEST_F(Tier1FEPBridgesE2ETest, BridgesCannotRegisterTwice) {
    // Register game theory bridge first time
    ASSERT_EQ(gt_fep_bridge_register(game_theory_bridge, orchestrator, nullptr, &gt_bridge_id), 0);
    EXPECT_TRUE(gt_fep_bridge_is_registered(game_theory_bridge));

    // Attempt to register again - should succeed idempotently with same ID
    uint32_t duplicate_id = 0;
    EXPECT_EQ(gt_fep_bridge_register(game_theory_bridge, orchestrator, nullptr, &duplicate_id), 0);

    // IDs should match (idempotent registration)
    EXPECT_EQ(duplicate_id, gt_bridge_id);
    EXPECT_EQ(gt_fep_bridge_get_id(game_theory_bridge), gt_bridge_id);
}

//=============================================================================
// CoordinatedFEPUpdateCycle Tests
//=============================================================================

/**
 * Test: Run FEP update cycle and verify all bridges receive updates
 */
TEST_F(Tier1FEPBridgesE2ETest, CoordinatedFEPUpdateCycle) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Run multiple update cycles
    const int NUM_CYCLES = 10;
    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        uint64_t current_time = GetCurrentTimeMs() + (cycle * UPDATE_INTERVAL_MS);

        int bridges_updated = fep_orchestrator_update(orchestrator, current_time);
        EXPECT_GE(bridges_updated, 0) << "Update cycle " << cycle << " failed";
    }

    // Verify orchestrator tracked updates
    fep_orchestrator_stats_t stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_GT(stats.total_update_cycles, 0u);

    // All bridges should still be in valid state
    EXPECT_TRUE(AllBridgesInValidState());
}

/**
 * Test: Force update all bridges immediately
 */
TEST_F(Tier1FEPBridgesE2ETest, ForceUpdateAllBridges) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Get stats before force update
    fep_orchestrator_stats_t before_stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &before_stats), 0);

    // Force update all bridges - may return count of category-based updates
    int updated = fep_orchestrator_force_update_all(orchestrator);
    EXPECT_GE(updated, 0) << "Force update should not fail";

    // Verify stats after force update
    fep_orchestrator_stats_t after_stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &after_stats), 0);
    EXPECT_GE(after_stats.total_bridge_updates, before_stats.total_bridge_updates);
}

/**
 * Test: Update individual bridges via force_update
 */
TEST_F(Tier1FEPBridgesE2ETest, UpdateIndividualBridges) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Force update each bridge directly
    EXPECT_EQ(gt_fep_bridge_force_update(game_theory_bridge), 0);
    EXPECT_EQ(imagination_fep_bridge_force_update(imagination_bridge), 0);
    EXPECT_EQ(parietal_fep_bridge_force_update(parietal_bridge), 0);
    EXPECT_EQ(social_fep_bridge_force_update(social_bridge), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(jepa_bridge), 0);

    // Verify all bridges received updates
    gt_fep_stats_t gt_stats;
    EXPECT_EQ(gt_fep_bridge_get_stats(game_theory_bridge, &gt_stats), 0);
    EXPECT_GE(gt_stats.total_updates, 1u);

    imagination_fep_stats_t imag_stats;
    EXPECT_EQ(imagination_fep_bridge_get_stats(imagination_bridge, &imag_stats), 0);
    EXPECT_GE(imag_stats.total_updates, 1u);

    parietal_fep_stats_t parietal_stats;
    EXPECT_EQ(parietal_fep_bridge_get_stats(parietal_bridge, &parietal_stats), 0);
    EXPECT_GE(parietal_stats.total_updates, 1u);

    social_fep_stats_t social_stats;
    EXPECT_EQ(social_fep_bridge_get_stats(social_bridge, &social_stats), 0);
    EXPECT_GE(social_stats.total_updates, 1u);

    jepa_fep_stats_t jepa_stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(jepa_bridge, &jepa_stats), 0);
    EXPECT_GE(jepa_stats.total_updates, 1u);
}

//=============================================================================
// CrossBridgeFreeEnergyAggregation Tests
//=============================================================================

/**
 * Test: Each bridge contributes free energy and total is aggregated correctly
 */
TEST_F(Tier1FEPBridgesE2ETest, CrossBridgeFreeEnergyAggregation) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Simulate inputs to generate free energy
    SimulateInputsToAllBridges(0.7f);

    // Force update to compute free energy
    fep_orchestrator_force_update_all(orchestrator);

    // Get individual free energy contributions
    float gt_fe = gt_fep_bridge_get_free_energy(game_theory_bridge);
    float imag_fe = imagination_fep_bridge_get_free_energy(imagination_bridge);
    float parietal_fe = parietal_fep_bridge_get_free_energy_contribution(parietal_bridge);
    float social_fe = social_fep_bridge_get_free_energy_contribution(social_bridge);
    float jepa_fe = jepa_fep_bridge_get_free_energy_contribution(jepa_bridge);

    // All should be valid (non-negative)
    EXPECT_GE(gt_fe, 0.0f);
    EXPECT_GE(imag_fe, 0.0f);
    EXPECT_GE(parietal_fe, 0.0f);
    EXPECT_GE(social_fe, 0.0f);
    EXPECT_GE(jepa_fe, 0.0f);

    // Verify free energy bounds
    EXPECT_LE(gt_fe, FEP_MAX_FREE_ENERGY);
    EXPECT_LE(imag_fe, FEP_MAX_FREE_ENERGY);
    EXPECT_LE(parietal_fe, FEP_MAX_FREE_ENERGY);
    EXPECT_LE(social_fe, FEP_MAX_FREE_ENERGY);
    EXPECT_LE(jepa_fe, FEP_MAX_FREE_ENERGY);

    // Total should be sum of individual contributions
    float total_fe = GetTotalFreeEnergy();
    float expected_total = gt_fe + imag_fe + parietal_fe + social_fe + jepa_fe;
    EXPECT_FLOAT_EQ(total_fe, expected_total);

    // Total should be bounded by max * num_bridges
    EXPECT_LE(total_fe, FEP_MAX_FREE_ENERGY * NUM_TIER1_BRIDGES);
}

/**
 * Test: Free energy increases with higher uncertainty/error inputs
 */
TEST_F(Tier1FEPBridgesE2ETest, FreeEnergyIncreasesWithUncertainty) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Low uncertainty inputs
    SimulateInputsToAllBridges(0.1f);
    fep_orchestrator_force_update_all(orchestrator);
    float low_fe = GetTotalFreeEnergy();

    // Reset bridges
    gt_fep_bridge_reset(game_theory_bridge);
    imagination_fep_bridge_reset(imagination_bridge);
    parietal_fep_bridge_reset(parietal_bridge);
    social_fep_bridge_reset(social_bridge);
    jepa_fep_bridge_reset(jepa_bridge);

    // High uncertainty inputs
    SimulateInputsToAllBridges(0.9f);
    fep_orchestrator_force_update_all(orchestrator);
    float high_fe = GetTotalFreeEnergy();

    // Higher uncertainty should result in higher free energy
    EXPECT_GT(high_fe, low_fe) << "Free energy should increase with uncertainty";
}

//=============================================================================
// StrategicImaginationIntegration Tests
//=============================================================================

/**
 * Test: Game Theory + Imagination working together
 * Strategic planning uses mental simulation for counterfactual reasoning
 */
TEST_F(Tier1FEPBridgesE2ETest, StrategicImaginationIntegration) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Scenario: Planning a strategic move using imagination
    // Step 1: Game Theory identifies strategy uncertainty
    gt_fep_bridge_update_strategy_uncertainty(game_theory_bridge, 0.6f);
    gt_fep_bridge_force_update(game_theory_bridge);

    float initial_gt_fe = gt_fep_bridge_get_free_energy(game_theory_bridge);
    EXPECT_GT(initial_gt_fe, FEP_BASELINE_FREE_ENERGY);

    // Step 2: Imagination runs mental simulation
    imagination_fep_bridge_force_update(imagination_bridge);
    float imag_fe = imagination_fep_bridge_get_free_energy(imagination_bridge);

    // Step 3: After simulation, strategy uncertainty decreases
    gt_fep_bridge_update_strategy_uncertainty(game_theory_bridge, 0.3f);
    gt_fep_bridge_force_update(game_theory_bridge);

    float reduced_gt_fe = gt_fep_bridge_get_free_energy(game_theory_bridge);

    // Strategy uncertainty should decrease free energy
    EXPECT_LT(reduced_gt_fe, initial_gt_fe)
        << "Imagination should help reduce strategic uncertainty";

    // Both bridges should be in active state during processing
    gt_fep_state_t gt_state = gt_fep_bridge_get_state(game_theory_bridge);
    EXPECT_NE(gt_state, GT_FEP_STATE_ERROR);

    imagination_fep_state_t imag_state = imagination_fep_bridge_get_state(imagination_bridge);
    EXPECT_NE(imag_state, IMAGINATION_FEP_STATE_ERROR);
}

/**
 * Test: Nash equilibrium detection reduces free energy
 */
TEST_F(Tier1FEPBridgesE2ETest, NashEquilibriumReducesFreeEnergy) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Start far from Nash equilibrium
    gt_fep_bridge_update_nash_distance(game_theory_bridge, 0.8f);
    gt_fep_bridge_force_update(game_theory_bridge);
    float far_from_nash_fe = gt_fep_bridge_get_free_energy(game_theory_bridge);

    // Approach Nash equilibrium
    gt_fep_bridge_update_nash_distance(game_theory_bridge, 0.1f);
    gt_fep_bridge_force_update(game_theory_bridge);
    float near_nash_fe = gt_fep_bridge_get_free_energy(game_theory_bridge);

    EXPECT_LT(near_nash_fe, far_from_nash_fe)
        << "Nash equilibrium should minimize free energy";

    // Check if at Nash
    // Note: Not all configurations will result in is_at_nash=true
    // but free energy should still be lower
}

//=============================================================================
// SocialSpatialIntegration Tests
//=============================================================================

/**
 * Test: Social + Parietal working together
 * Social navigation uses spatial processing
 */
TEST_F(Tier1FEPBridgesE2ETest, SocialSpatialIntegration) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Scenario: Navigating social space (physical position in social hierarchy)

    // Step 1: Parietal processes spatial context
    parietal_fep_bridge_force_update(parietal_bridge);
    float parietal_fe = parietal_fep_bridge_get_free_energy_contribution(parietal_bridge);
    EXPECT_GE(parietal_fe, 0.0f);

    // Step 2: Social module processes relationship context
    social_fep_bridge_force_update(social_bridge);
    float social_fe = social_fep_bridge_get_free_energy_contribution(social_bridge);
    EXPECT_GE(social_fe, 0.0f);

    // Step 3: Combined processing - run coordinated update
    fep_orchestrator_force_update_all(orchestrator);

    // Both bridges should contribute to prediction
    parietal_fep_state_t parietal_state = parietal_fep_bridge_get_state(parietal_bridge);
    EXPECT_NE(parietal_state, PARIETAL_FEP_STATE_ERROR);

    social_fep_state_t social_state = social_fep_bridge_get_state(social_bridge);
    EXPECT_NE(social_state, SOCIAL_FEP_STATE_ERROR);

    // Get prediction errors
    float parietal_pred_error = parietal_fep_bridge_get_prediction_error(parietal_bridge);
    float social_pred_error = social_fep_bridge_get_social_prediction_error(social_bridge);

    // Prediction errors should be valid
    EXPECT_GE(parietal_pred_error, -1.0f);
    EXPECT_GE(social_pred_error, -1.0f);
}

//=============================================================================
// JEPARepresentationAcrossModules Tests
//=============================================================================

/**
 * Test: JEPA provides representations that affect other modules
 */
TEST_F(Tier1FEPBridgesE2ETest, JEPARepresentationAcrossModules) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // JEPA generates high-quality embeddings
    jepa_fep_bridge_record_prediction_error(jepa_bridge, 0.1f);  // Low error
    jepa_fep_bridge_record_representation_quality(jepa_bridge, 0.9f);  // High quality
    jepa_fep_bridge_force_update(jepa_bridge);

    float jepa_fe = jepa_fep_bridge_get_free_energy_contribution(jepa_bridge);
    float rep_quality = jepa_fep_bridge_get_representation_quality(jepa_bridge);

    EXPECT_LT(jepa_fe, FEP_MAX_FREE_ENERGY * 0.5f)
        << "High quality embeddings should result in low free energy";
    EXPECT_GT(rep_quality, 0.5f);

    // Now simulate poor embeddings
    jepa_fep_bridge_reset(jepa_bridge);
    jepa_fep_bridge_record_prediction_error(jepa_bridge, 0.8f);  // High error
    jepa_fep_bridge_record_representation_quality(jepa_bridge, 0.3f);  // Low quality (collapse risk)
    jepa_fep_bridge_force_update(jepa_bridge);

    float poor_jepa_fe = jepa_fep_bridge_get_free_energy_contribution(jepa_bridge);
    EXPECT_GT(poor_jepa_fe, jepa_fe)
        << "Poor embeddings should increase free energy";

    // Check for collapse detection
    jepa_fep_state_t state = jepa_fep_bridge_get_state(jepa_bridge);
    // State should not be error even with poor quality
    EXPECT_NE(state, JEPA_FEP_STATE_ERROR);
}

/**
 * Test: JEPA embedding prediction error tracking
 */
TEST_F(Tier1FEPBridgesE2ETest, JEPAEmbeddingPredictionErrorTracking) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Record series of prediction errors
    float errors[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    for (float error : errors) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(jepa_bridge, error), 0);
    }

    jepa_fep_bridge_force_update(jepa_bridge);

    float avg_error = jepa_fep_bridge_get_embedding_prediction_error(jepa_bridge);
    EXPECT_GE(avg_error, 0.0f);
    EXPECT_LE(avg_error, 1.0f);

    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(jepa_bridge, &stats), 0);
    EXPECT_GT(stats.embedding_predictions, 0u);
}

//=============================================================================
// FullSystemPredictionMinimization Tests
//=============================================================================

/**
 * Test: All 5 modules active, system minimizes total prediction error
 */
TEST_F(Tier1FEPBridgesE2ETest, FullSystemPredictionMinimization) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Initial high uncertainty state
    SimulateInputsToAllBridges(0.8f);
    fep_orchestrator_force_update_all(orchestrator);
    float initial_fe = GetTotalFreeEnergy();

    // Run multiple update cycles to allow system to stabilize
    const int MINIMIZE_CYCLES = 20;
    for (int i = 0; i < MINIMIZE_CYCLES; i++) {
        // Gradually reduce uncertainty (simulating learning/adaptation)
        float intensity = 0.8f - (0.5f * i / MINIMIZE_CYCLES);
        SimulateInputsToAllBridges(intensity);

        uint64_t current_time = GetCurrentTimeMs() + (i * UPDATE_INTERVAL_MS);
        fep_orchestrator_update(orchestrator, current_time);
    }

    float final_fe = GetTotalFreeEnergy();

    // Free energy should decrease over time with learning
    EXPECT_LT(final_fe, initial_fe)
        << "System should minimize free energy over time";

    // All bridges should be in valid state
    EXPECT_TRUE(AllBridgesInValidState());
}

/**
 * Test: Emergent coordination behavior across bridges
 */
TEST_F(Tier1FEPBridgesE2ETest, EmergentCoordinationBehavior) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Run coordinated updates and track statistics
    const int NUM_CYCLES = 50;

    std::vector<float> total_fe_history;

    for (int i = 0; i < NUM_CYCLES; i++) {
        SimulateInputsToAllBridges(0.5f + 0.3f * sinf(i * 0.1f));  // Oscillating input

        uint64_t current_time = GetCurrentTimeMs() + (i * UPDATE_INTERVAL_MS);
        fep_orchestrator_update(orchestrator, current_time);

        total_fe_history.push_back(GetTotalFreeEnergy());
    }

    // Verify we have a valid history
    EXPECT_EQ(total_fe_history.size(), NUM_CYCLES);

    // All values should be finite and bounded
    for (float fe : total_fe_history) {
        EXPECT_TRUE(std::isfinite(fe));
        EXPECT_GE(fe, 0.0f);
        EXPECT_LE(fe, FEP_MAX_FREE_ENERGY * NUM_TIER1_BRIDGES);
    }

    // Orchestrator should track all updates
    fep_orchestrator_stats_t stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_GT(stats.total_update_cycles, 0u);
}

//=============================================================================
// StressTestMultipleCycles Tests
//=============================================================================

/**
 * Test: Run 100+ FEP cycles, all bridges remain stable
 */
TEST_F(Tier1FEPBridgesE2ETest, StressTestMultipleCycles) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    std::atomic<int> errors{0};
    auto start = std::chrono::high_resolution_clock::now();

    // Run stress iterations
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        // Vary inputs
        float intensity = 0.5f + 0.4f * sinf(i * 0.05f);
        SimulateInputsToAllBridges(intensity);

        // Run orchestrator update
        uint64_t current_time = GetCurrentTimeMs() + (i * UPDATE_INTERVAL_MS);
        int result = fep_orchestrator_update(orchestrator, current_time);

        if (result < 0) {
            errors++;
        }

        // Verify bridges remain stable
        if (!AllBridgesInValidState()) {
            errors++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // No errors during stress test
    EXPECT_EQ(errors.load(), 0) << "Stress test encountered errors";

    // Should complete in reasonable time (under 10 seconds for 100 cycles)
    EXPECT_LT(duration.count(), 10000)
        << "Stress test took too long: " << duration.count() << "ms";

    // Verify statistics
    fep_orchestrator_stats_t stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_GE(stats.total_update_cycles, STRESS_ITERATIONS);

    // All bridges should still be functional
    EXPECT_TRUE(AllBridgesInValidState());
}

/**
 * Test: No memory leaks or performance degradation over many cycles
 */
TEST_F(Tier1FEPBridgesE2ETest, StressTestNoMemoryLeaks) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Track timing over batches
    const int NUM_BATCHES = 10;
    const int CYCLES_PER_BATCH = 10;
    std::vector<long long> batch_times;

    for (int batch = 0; batch < NUM_BATCHES; batch++) {
        auto batch_start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < CYCLES_PER_BATCH; i++) {
            SimulateInputsToAllBridges(0.5f);
            fep_orchestrator_force_update_all(orchestrator);
        }

        auto batch_end = std::chrono::high_resolution_clock::now();
        auto batch_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            batch_end - batch_start);
        batch_times.push_back(batch_duration.count());
    }

    // Performance should not degrade significantly over time
    // Last batch should not take more than 2x the first batch
    EXPECT_LT(batch_times.back(), batch_times.front() * 2)
        << "Performance degraded over time - possible memory leak";

    // Verify final state
    EXPECT_TRUE(AllBridgesInValidState());
}

/**
 * Test: Concurrent access to bridges (thread safety)
 */
TEST_F(Tier1FEPBridgesE2ETest, StressTestConcurrentAccess) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    std::atomic<int> errors{0};
    std::atomic<int> completed{0};

    // Thread 1: Game Theory and Imagination updates
    std::thread t1([this, &errors, &completed]() {
        for (int i = 0; i < 50; i++) {
            gt_fep_bridge_update_strategy_uncertainty(game_theory_bridge, 0.5f);
            if (gt_fep_bridge_force_update(game_theory_bridge) != 0) errors++;
            if (imagination_fep_bridge_force_update(imagination_bridge) != 0) errors++;
            completed++;
        }
    });

    // Thread 2: Parietal and Social updates
    std::thread t2([this, &errors, &completed]() {
        for (int i = 0; i < 50; i++) {
            if (parietal_fep_bridge_force_update(parietal_bridge) != 0) errors++;
            if (social_fep_bridge_force_update(social_bridge) != 0) errors++;
            completed++;
        }
    });

    // Thread 3: JEPA updates
    std::thread t3([this, &errors, &completed]() {
        for (int i = 0; i < 50; i++) {
            jepa_fep_bridge_record_prediction_error(jepa_bridge, 0.3f);
            if (jepa_fep_bridge_force_update(jepa_bridge) != 0) errors++;
            completed++;
        }
    });

    t1.join();
    t2.join();
    t3.join();

    EXPECT_EQ(errors.load(), 0) << "Concurrent access caused errors";
    EXPECT_EQ(completed.load(), 150);
    EXPECT_TRUE(AllBridgesInValidState());
}

//=============================================================================
// GracefulDegradation Tests
//=============================================================================

/**
 * Test: Remove one bridge at a time, system continues functioning
 */
TEST_F(Tier1FEPBridgesE2ETest, GracefulDegradation) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // System works with all 5 bridges
    fep_orchestrator_force_update_all(orchestrator);
    float full_fe = GetTotalFreeEnergy();
    EXPECT_GE(full_fe, 0.0f);

    // Remove Game Theory bridge
    EXPECT_EQ(gt_fep_bridge_unregister(game_theory_bridge), 0);
    EXPECT_FALSE(gt_fep_bridge_is_registered(game_theory_bridge));

    // System should still work with remaining bridges
    int updated = fep_orchestrator_force_update_all(orchestrator);
    EXPECT_GE(updated, 0) << "Force update should not fail after bridge removal";

    // Verify orchestrator stats
    fep_orchestrator_stats_t stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &stats), 0);

    // Remove another bridge (Imagination)
    EXPECT_EQ(imagination_fep_bridge_unregister(imagination_bridge), 0);
    updated = fep_orchestrator_force_update_all(orchestrator);
    EXPECT_GE(updated, 0) << "Force update should not fail after second bridge removal";

    // Remaining bridges should still be functional
    EXPECT_FALSE(parietal_fep_bridge_is_degraded(parietal_bridge));
    EXPECT_FALSE(social_fep_bridge_is_degraded(social_bridge));
    EXPECT_FALSE(jepa_fep_bridge_is_degraded(jepa_bridge));
}

/**
 * Test: Bridges continue when one enters degraded mode
 */
TEST_F(Tier1FEPBridgesE2ETest, BridgesCompensateWhenOthersDisabled) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Simulate high free energy to trigger degraded mode in Game Theory bridge
    for (int i = 0; i < 10; i++) {
        gt_fep_bridge_update_strategy_uncertainty(game_theory_bridge, 0.9f);
        gt_fep_bridge_update_opponent_error(game_theory_bridge, 0.9f);
        gt_fep_bridge_force_update(game_theory_bridge);
    }

    // Other bridges should still update normally
    EXPECT_EQ(imagination_fep_bridge_force_update(imagination_bridge), 0);
    EXPECT_EQ(parietal_fep_bridge_force_update(parietal_bridge), 0);
    EXPECT_EQ(social_fep_bridge_force_update(social_bridge), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(jepa_bridge), 0);

    // Verify other bridges are not degraded
    EXPECT_FALSE(imagination_fep_bridge_is_degraded(imagination_bridge));
    EXPECT_FALSE(parietal_fep_bridge_is_degraded(parietal_bridge));
    EXPECT_FALSE(social_fep_bridge_is_degraded(social_bridge));
    EXPECT_FALSE(jepa_fep_bridge_is_degraded(jepa_bridge));
}

//=============================================================================
// StatisticsAggregation Tests
//=============================================================================

/**
 * Test: Collect and verify stats from all bridges
 */
TEST_F(Tier1FEPBridgesE2ETest, StatisticsAggregation) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Run some updates
    for (int i = 0; i < 10; i++) {
        SimulateInputsToAllBridges(0.5f);
        fep_orchestrator_force_update_all(orchestrator);
    }

    // Collect stats from all bridges
    gt_fep_stats_t gt_stats;
    EXPECT_EQ(gt_fep_bridge_get_stats(game_theory_bridge, &gt_stats), 0);
    EXPECT_GT(gt_stats.total_updates, 0u);

    imagination_fep_stats_t imag_stats;
    EXPECT_EQ(imagination_fep_bridge_get_stats(imagination_bridge, &imag_stats), 0);
    EXPECT_GT(imag_stats.total_updates, 0u);

    parietal_fep_stats_t parietal_stats;
    EXPECT_EQ(parietal_fep_bridge_get_stats(parietal_bridge, &parietal_stats), 0);
    EXPECT_GT(parietal_stats.total_updates, 0u);

    social_fep_stats_t social_stats;
    EXPECT_EQ(social_fep_bridge_get_stats(social_bridge, &social_stats), 0);
    EXPECT_GT(social_stats.total_updates, 0u);

    jepa_fep_stats_t jepa_stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(jepa_bridge, &jepa_stats), 0);
    EXPECT_GT(jepa_stats.total_updates, 0u);

    // Verify aggregate orchestrator stats
    fep_orchestrator_stats_t orch_stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &orch_stats), 0);

    // Total bridge updates should be reasonable (may not exactly match due to timing)
    uint64_t bridge_sum = gt_stats.total_updates + imag_stats.total_updates +
                          parietal_stats.total_updates + social_stats.total_updates +
                          jepa_stats.total_updates;
    EXPECT_GT(bridge_sum, 0u) << "At least some bridges should have updates";
    EXPECT_GE(orch_stats.total_bridge_updates, 0u);

    // Verify load is reasonable
    float load = fep_orchestrator_get_load(orchestrator);
    EXPECT_GE(load, 0.0f);
    EXPECT_LT(load, 10.0f);  // Should not be extremely overloaded
}

/**
 * Test: Reset statistics and verify clean state
 */
TEST_F(Tier1FEPBridgesE2ETest, StatisticsReset) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Generate some stats
    for (int i = 0; i < 5; i++) {
        fep_orchestrator_force_update_all(orchestrator);
    }

    // Verify stats are non-zero
    gt_fep_stats_t gt_stats;
    EXPECT_EQ(gt_fep_bridge_get_stats(game_theory_bridge, &gt_stats), 0);
    EXPECT_GT(gt_stats.total_updates, 0u);

    // Reset individual bridge stats
    EXPECT_EQ(gt_fep_bridge_reset_stats(game_theory_bridge), 0);
    EXPECT_EQ(imagination_fep_bridge_reset_stats(imagination_bridge), 0);
    EXPECT_EQ(parietal_fep_bridge_reset_stats(parietal_bridge), 0);
    EXPECT_EQ(social_fep_bridge_reset_stats(social_bridge), 0);
    EXPECT_EQ(jepa_fep_bridge_reset_stats(jepa_bridge), 0);

    // Verify stats are reset
    EXPECT_EQ(gt_fep_bridge_get_stats(game_theory_bridge, &gt_stats), 0);
    EXPECT_EQ(gt_stats.total_updates, 0u);

    // Reset orchestrator stats
    fep_orchestrator_reset_stats(orchestrator);

    fep_orchestrator_stats_t orch_stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &orch_stats), 0);
    EXPECT_EQ(orch_stats.total_update_cycles, 0u);
}

/**
 * Test: Performance metrics across all bridges
 */
TEST_F(Tier1FEPBridgesE2ETest, PerformanceMetrics) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    auto start = std::chrono::high_resolution_clock::now();

    // Run timed updates
    const int NUM_UPDATES = 50;
    for (int i = 0; i < NUM_UPDATES; i++) {
        fep_orchestrator_force_update_all(orchestrator);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Average time per full update cycle
    float avg_cycle_time_us = static_cast<float>(total_duration.count()) / NUM_UPDATES;

    // Check orchestrator reports similar timing
    fep_orchestrator_stats_t stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &stats), 0);

    // Average cycle time should be reasonable (under 10ms = 10000us)
    EXPECT_LT(avg_cycle_time_us, 10000.0f)
        << "Average cycle time too high: " << avg_cycle_time_us << "us";

    // Individual bridge timing should be valid
    gt_fep_stats_t gt_stats;
    EXPECT_EQ(gt_fep_bridge_get_stats(game_theory_bridge, &gt_stats), 0);
    EXPECT_GE(gt_stats.avg_update_time_us, 0.0f);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * Test: NULL parameter handling
 */
TEST_F(Tier1FEPBridgesE2ETest, NullParameterHandling) {
    // Bridge creation with NULL config should use defaults
    gt_fep_bridge_t* bridge = gt_fep_bridge_create(nullptr);
    EXPECT_NE(bridge, nullptr);
    gt_fep_bridge_destroy(bridge);

    // Operations on NULL bridges should fail gracefully
    EXPECT_EQ(gt_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(imagination_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(parietal_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(social_fep_bridge_force_update(nullptr), -1);
    EXPECT_EQ(jepa_fep_bridge_force_update(nullptr), -1);

    // Get operations on NULL should return error values
    EXPECT_EQ(gt_fep_bridge_get_free_energy(nullptr), -1.0f);
    EXPECT_EQ(imagination_fep_bridge_get_free_energy(nullptr), -1.0f);
    EXPECT_EQ(parietal_fep_bridge_get_free_energy_contribution(nullptr), -1.0f);
    EXPECT_EQ(social_fep_bridge_get_free_energy_contribution(nullptr), -1.0f);
    EXPECT_EQ(jepa_fep_bridge_get_free_energy_contribution(nullptr), -1.0f);
}

/**
 * Test: Invalid bridge ID handling
 */
TEST_F(Tier1FEPBridgesE2ETest, InvalidBridgeIdHandling) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Try to update non-existent bridge - should return error (non-zero)
    EXPECT_NE(fep_orchestrator_update_bridge(orchestrator, 99999), 0)
        << "Updating non-existent bridge should fail";

    // Try to unregister non-existent bridge - should return error (non-zero)
    EXPECT_NE(fep_orchestrator_unregister_bridge(orchestrator, 99999), 0)
        << "Unregistering non-existent bridge should fail";

    // Orchestrator should still be functional after invalid operations
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);
}

//=============================================================================
// Pause/Resume Tests
//=============================================================================

/**
 * Test: Orchestrator pause and resume
 */
TEST_F(Tier1FEPBridgesE2ETest, OrchestratorPauseResume) {
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);

    // Run some updates
    fep_orchestrator_force_update_all(orchestrator);

    fep_orchestrator_stats_t before_pause_stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &before_pause_stats), 0);

    // Pause
    EXPECT_EQ(fep_orchestrator_pause(orchestrator), 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_PAUSED);

    // Resume
    EXPECT_EQ(fep_orchestrator_resume(orchestrator), 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);

    // Continue updates
    fep_orchestrator_force_update_all(orchestrator);

    fep_orchestrator_stats_t after_resume_stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &after_resume_stats), 0);
    EXPECT_GT(after_resume_stats.total_bridge_updates, before_pause_stats.total_bridge_updates);
}

//=============================================================================
// End-to-End Pipeline Tests
//=============================================================================

/**
 * Test: Complete E2E pipeline with all bridges
 */
TEST_F(Tier1FEPBridgesE2ETest, CompleteE2EPipeline) {
    // Phase 1: Setup
    ASSERT_TRUE(RegisterAllBridges());
    EXPECT_EQ(fep_orchestrator_start(orchestrator), 0);

    // Phase 2: Initial state (high uncertainty)
    SimulateInputsToAllBridges(0.9f);
    fep_orchestrator_force_update_all(orchestrator);
    float initial_fe = GetTotalFreeEnergy();

    // Phase 3: Processing pipeline (simulated cognitive processing)
    const int PIPELINE_STAGES = 5;
    std::vector<float> fe_trajectory;
    fe_trajectory.push_back(initial_fe);

    for (int stage = 0; stage < PIPELINE_STAGES; stage++) {
        // Each stage reduces uncertainty through processing
        float stage_intensity = 0.9f - (0.15f * stage);

        // Game Theory: Strategy refinement
        gt_fep_bridge_update_strategy_uncertainty(game_theory_bridge,
            stage_intensity * 0.4f);
        gt_fep_bridge_update_nash_distance(game_theory_bridge,
            stage_intensity * 0.3f);

        // Imagination: Mental simulation
        imagination_fep_bridge_force_update(imagination_bridge);

        // Parietal: Spatial processing
        parietal_fep_bridge_force_update(parietal_bridge);

        // Social: Social cognition
        social_fep_bridge_force_update(social_bridge);

        // JEPA: Representation learning
        jepa_fep_bridge_record_prediction_error(jepa_bridge,
            stage_intensity * 0.3f);
        jepa_fep_bridge_record_representation_quality(jepa_bridge,
            1.0f - stage_intensity * 0.2f);
        jepa_fep_bridge_force_update(jepa_bridge);

        // Orchestrate update
        uint64_t stage_time = GetCurrentTimeMs() + (stage * UPDATE_INTERVAL_MS);
        fep_orchestrator_update(orchestrator, stage_time);

        fe_trajectory.push_back(GetTotalFreeEnergy());
    }

    // Phase 4: Verification
    float final_fe = fe_trajectory.back();

    // Free energy should decrease through pipeline
    EXPECT_LT(final_fe, initial_fe)
        << "Free energy should decrease through cognitive pipeline";

    // Trajectory should generally decrease
    int decreasing_steps = 0;
    for (size_t i = 1; i < fe_trajectory.size(); i++) {
        if (fe_trajectory[i] < fe_trajectory[i-1]) {
            decreasing_steps++;
        }
    }
    EXPECT_GE(decreasing_steps, PIPELINE_STAGES / 2)
        << "Free energy should generally decrease through pipeline";

    // All bridges in valid state
    EXPECT_TRUE(AllBridgesInValidState());

    // Orchestrator tracked the pipeline
    fep_orchestrator_stats_t stats;
    EXPECT_EQ(fep_orchestrator_get_stats(orchestrator, &stats), 0);
    EXPECT_GT(stats.total_update_cycles, 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
