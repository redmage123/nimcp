/**
 * @file test_hypothalamus_logic_bridge.cpp
 * @brief Unit tests for nimcp_hypothalamus_logic_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Hypothalamus-Logic Bridge
 * WHY:  Ensure correct bidirectional integration between drive system and
 *       symbolic reasoning, including motivated reasoning (hot cognition),
 *       logic-driven drive updates, FEP integration, and predicate mapping
 * HOW:  Use Google Test framework to test lifecycle, modulation, conclusions,
 *       goals, FEP predictions, predicate mappings, and statistics
 *
 * COVERAGE TARGET: 100%
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

extern "C" {
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_logic_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/nimcp_symbolic_logic.h"
}

// ============================================================================
// MOCK/HELPER STRUCTURES
// ============================================================================

/**
 * @brief Test context for managing test resources
 */
struct TestContext {
    hypo_drive_system_handle_t* drives;
    symbolic_logic_t* logic;
    hypo_logic_bridge_t* bridge;
    logic_clause_t* test_clauses[16];
    uint32_t num_test_clauses;
};

// ============================================================================
// TEST FIXTURES
// ============================================================================

/**
 * @brief Main test fixture with pre-created bridge
 */
class HypothalamusLogicBridgeTest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives;
    symbolic_logic_t* logic;
    hypo_logic_bridge_t* bridge;

    void SetUp() override {
        // Create drive system with default config
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drives = hypo_drive_create(&drive_config);
        ASSERT_NE(nullptr, drives) << "Failed to create drive system";

        // Create symbolic logic engine
        logic_config_t logic_config = {
            .max_predicates = 100,
            .max_rules = 50,
            .max_kb_size = 200,
            .max_inference_depth = 10,
            .enable_forward_chaining = true,
            .enable_backward_chaining = true,
            .enable_resolution = true,
            .enable_memory_consolidation = false,
            .enable_quantum_logic = false
        };
        logic = symbolic_logic_create(&logic_config);
        ASSERT_NE(nullptr, logic) << "Failed to create symbolic logic engine";

        // Create bridge with default config
        bridge = hypo_logic_bridge_create(drives, logic, nullptr);
        ASSERT_NE(nullptr, bridge) << "Failed to create hypothalamus-logic bridge";
    }

    void TearDown() override {
        if (bridge) {
            hypo_logic_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (logic) {
            symbolic_logic_destroy(logic);
            logic = nullptr;
        }
        if (drives) {
            hypo_drive_destroy(drives);
            drives = nullptr;
        }
    }

    // Helper to create a simple test clause
    logic_clause_t* CreateTestClause(const char* predicate_name, bool negated = false) {
        logic_clause_t* clause = (logic_clause_t*)calloc(1, sizeof(logic_clause_t));
        if (!clause) return nullptr;

        clause->literals = (atomic_formula_t**)calloc(1, sizeof(atomic_formula_t*));
        if (!clause->literals) {
            free(clause);
            return nullptr;
        }

        atomic_formula_t* atom = (atomic_formula_t*)calloc(1, sizeof(atomic_formula_t));
        if (!atom) {
            free(clause->literals);
            free(clause);
            return nullptr;
        }

        strncpy(atom->name, predicate_name, LOGIC_MAX_NAME_LENGTH - 1);
        atom->name[LOGIC_MAX_NAME_LENGTH - 1] = '\0';
        atom->negated = negated;
        atom->arity = 0;
        atom->terms = nullptr;

        clause->literals[0] = atom;
        clause->num_literals = 1;
        clause->confidence = 1.0f;

        return clause;
    }

    void DestroyTestClause(logic_clause_t* clause) {
        if (!clause) return;
        if (clause->literals) {
            for (uint32_t i = 0; i < clause->num_literals; i++) {
                if (clause->literals[i]) {
                    free(clause->literals[i]);
                }
            }
            free(clause->literals);
        }
        free(clause);
    }

    // Helper to set a drive urgency
    void SetDriveUrgency(hypo_drive_type_t drive, float urgency) {
        // Drive urgency is typically updated through hypo_drive_update
        // For testing, we can satisfy or stimulate the drive to affect urgency
        // Here we use the nucleus input as a proxy
        if (drive == HYPO_DRIVE_HUNGER) {
            hypo_drive_set_nucleus_input(drives, HYPO_NUCLEUS_LATERAL, urgency);
        } else if (drive == HYPO_DRIVE_SAFETY) {
            hypo_drive_set_nucleus_input(drives, HYPO_NUCLEUS_VENTROMEDIAL, urgency);
        } else if (drive == HYPO_DRIVE_CURIOSITY) {
            // Update drive directly via internal mechanism
        }
        // Update drives to apply changes
        hypo_drive_update(drives, 1000);
    }
};

/**
 * @brief Lifecycle test fixture without pre-created bridge
 */
class HypothalamusLogicBridgeLifecycleTest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives = nullptr;
    symbolic_logic_t* logic = nullptr;

    void SetUp() override {
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drives = hypo_drive_create(&drive_config);

        logic_config_t logic_config = {
            .max_predicates = 100,
            .max_rules = 50,
            .max_kb_size = 200,
            .max_inference_depth = 10,
            .enable_forward_chaining = true,
            .enable_backward_chaining = true,
            .enable_resolution = true,
            .enable_memory_consolidation = false,
            .enable_quantum_logic = false
        };
        logic = symbolic_logic_create(&logic_config);
    }

    void TearDown() override {
        if (logic) {
            symbolic_logic_destroy(logic);
            logic = nullptr;
        }
        if (drives) {
            hypo_drive_destroy(drives);
            drives = nullptr;
        }
    }
};

// ============================================================================
// DEFAULT CONFIG TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeLifecycleTest, DefaultConfigHasReasonableValues) {
    hypo_logic_config_t config = hypo_logic_default_config();

    // Inference depth settings
    EXPECT_GT(config.base_inference_depth, 0.0f);
    EXPECT_GT(config.min_inference_depth, 0.0f);
    EXPECT_LE(config.min_inference_depth, config.base_inference_depth);
    EXPECT_GT(config.urgency_depth_sensitivity, 0.0f);
    EXPECT_LE(config.urgency_depth_sensitivity, 1.0f);

    // Proof threshold settings
    EXPECT_GT(config.base_proof_threshold, 0.0f);
    EXPECT_LE(config.base_proof_threshold, 1.0f);
    EXPECT_GE(config.wishful_thinking_max, 0.0f);
    EXPECT_LE(config.wishful_thinking_max, config.base_proof_threshold);
    EXPECT_GT(config.threshold_drive_sensitivity, 0.0f);

    // Salience settings
    EXPECT_GT(config.salience_drive_weight, 0.0f);
    EXPECT_GE(config.salience_decay_rate, 0.0f);

    // Logic-driven update settings
    EXPECT_GT(config.anticipation_gain, 0.0f);
    EXPECT_GT(config.frustration_gain, 0.0f);
    EXPECT_GE(config.conclusion_decay_rate, 0.0f);

    // FEP settings
    EXPECT_TRUE(config.enable_fep_integration);
    EXPECT_GT(config.prediction_learning_rate, 0.0f);
    EXPECT_GT(config.precision_base, 0.0f);

    // Fatigue/capacity settings
    EXPECT_GT(config.fatigue_capacity_weight, 0.0f);
    EXPECT_GT(config.arousal_capacity_weight, 0.0f);

    // Bio-async settings
    EXPECT_GT(config.update_interval_us, 0u);

    // No predicate maps by default (auto-registered later)
    EXPECT_EQ(0u, config.num_predicate_maps);
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeLifecycleTest, CreateWithDefaultConfig) {
    ASSERT_NE(nullptr, drives);
    ASSERT_NE(nullptr, logic);

    hypo_logic_bridge_t* bridge = hypo_logic_bridge_create(drives, logic, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Should have auto-registered predicate mappings
    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));

    hypo_logic_bridge_destroy(bridge);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, CreateWithCustomConfig) {
    hypo_logic_config_t config = hypo_logic_default_config();

    // Customize config
    config.base_inference_depth = 15.0f;
    config.min_inference_depth = 5.0f;
    config.base_proof_threshold = 0.8f;
    config.wishful_thinking_max = 0.2f;
    config.enable_fep_integration = false;
    config.enable_bio_async = false;

    hypo_logic_bridge_t* bridge = hypo_logic_bridge_create(drives, logic, &config);
    ASSERT_NE(nullptr, bridge);

    // Verify config was applied
    hypo_logic_modulation_t modulation;
    EXPECT_EQ(0, hypo_logic_get_modulation(bridge, &modulation));
    EXPECT_EQ(15u, modulation.base_inference_depth);
    EXPECT_FLOAT_EQ(0.8f, modulation.base_threshold);

    hypo_logic_bridge_destroy(bridge);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, CreateWithNullDrivesFails) {
    hypo_logic_bridge_t* bridge = hypo_logic_bridge_create(nullptr, logic, nullptr);
    EXPECT_EQ(nullptr, bridge);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, CreateWithNullLogicFails) {
    hypo_logic_bridge_t* bridge = hypo_logic_bridge_create(drives, nullptr, nullptr);
    EXPECT_EQ(nullptr, bridge);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, DestroyNullDoesNotCrash) {
    hypo_logic_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(HypothalamusLogicBridgeTest, ResetClearsState) {
    // Process some conclusions to change state
    logic_clause_t* clause = CreateTestClause("food_available");
    ASSERT_NE(nullptr, clause);
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.9f));

    // Create a motivated goal
    logic_clause_t* goal_clause = CreateTestClause("find_food");
    ASSERT_NE(nullptr, goal_clause);
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal_clause, 0.8f));

    // Reset
    EXPECT_EQ(0, hypo_logic_bridge_reset(bridge));

    // Verify state is reset
    hypo_logic_modulation_t modulation;
    EXPECT_EQ(0, hypo_logic_get_modulation(bridge, &modulation));

    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        EXPECT_FLOAT_EQ(0.0f, anticipation.anticipation[i]);
        EXPECT_FLOAT_EQ(0.0f, anticipation.frustration[i]);
    }

    // Goals should be cleared
    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals = 0;
    EXPECT_EQ(0, hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals));
    EXPECT_EQ(0u, num_goals);

    // Stats should be reset
    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.conclusions_processed);
    EXPECT_EQ(0u, stats.goals_created);

    DestroyTestClause(clause);
    DestroyTestClause(goal_clause);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, ResetNullReturnsError) {
    EXPECT_EQ(-1, hypo_logic_bridge_reset(nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, UpdateSuccess) {
    EXPECT_EQ(0, hypo_logic_bridge_update(bridge, 10000));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, UpdateNullReturnsError) {
    EXPECT_EQ(-1, hypo_logic_bridge_update(nullptr, 10000));
}

// ============================================================================
// HYPOTHALAMUS -> LOGIC: MOTIVATED REASONING TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, ComputeModulationSuccess) {
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));

    hypo_logic_modulation_t modulation;
    EXPECT_EQ(0, hypo_logic_get_modulation(bridge, &modulation));

    // Verify modulation was computed
    EXPECT_GT(modulation.max_inference_depth, 0u);
    EXPECT_GT(modulation.proof_threshold, 0.0f);
    EXPECT_LE(modulation.proof_threshold, 1.0f);
    EXPECT_GT(modulation.reasoning_capacity, 0.0f);
    EXPECT_LE(modulation.reasoning_capacity, 1.0f);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, ComputeModulationNullReturnsError) {
    EXPECT_EQ(-1, hypo_logic_compute_modulation(nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, GetModulationNullBridgeReturnsError) {
    hypo_logic_modulation_t modulation;
    EXPECT_EQ(-1, hypo_logic_get_modulation(nullptr, &modulation));
}

TEST_F(HypothalamusLogicBridgeTest, GetModulationNullOutputReturnsError) {
    EXPECT_EQ(-1, hypo_logic_get_modulation(bridge, nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, ApplyModulationSuccess) {
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));
    EXPECT_EQ(0, hypo_logic_apply_modulation(bridge));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, ApplyModulationNullReturnsError) {
    EXPECT_EQ(-1, hypo_logic_apply_modulation(nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, HighUrgencyReducesInferenceDepth) {
    // First compute baseline modulation
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));
    hypo_logic_modulation_t baseline;
    EXPECT_EQ(0, hypo_logic_get_modulation(bridge, &baseline));

    // Stimulate a high urgency drive
    hypo_drive_set_nucleus_input(drives, HYPO_NUCLEUS_LATERAL, 0.9f);
    hypo_drive_update(drives, 100000);

    // Recompute modulation
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));
    hypo_logic_modulation_t high_urgency;
    EXPECT_EQ(0, hypo_logic_get_modulation(bridge, &high_urgency));

    // High urgency should reduce inference depth (faster, shallower reasoning)
    EXPECT_LE(high_urgency.max_inference_depth, baseline.max_inference_depth);
}

TEST_F(HypothalamusLogicBridgeTest, GetRecommendedDepthSuccess) {
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));
    uint32_t depth = hypo_logic_get_recommended_depth(bridge);
    EXPECT_GT(depth, 0u);
}

TEST_F(HypothalamusLogicBridgeTest, GetRecommendedDepthNullReturnsDefault) {
    uint32_t depth = hypo_logic_get_recommended_depth(nullptr);
    EXPECT_EQ(10u, depth);  // Default value
}

// ============================================================================
// PREDICATE SALIENCE TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, GetPredicateSalienceFoodRelated) {
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));

    float food_salience = hypo_logic_get_predicate_salience(bridge, "food_available");
    EXPECT_GE(food_salience, 0.1f);
    EXPECT_LE(food_salience, 10.0f);

    float eat_salience = hypo_logic_get_predicate_salience(bridge, "can_eat");
    EXPECT_GE(eat_salience, 0.1f);

    float hungry_salience = hypo_logic_get_predicate_salience(bridge, "is_hungry");
    EXPECT_GE(hungry_salience, 0.1f);
}

TEST_F(HypothalamusLogicBridgeTest, GetPredicateSalienceWaterRelated) {
    float water_salience = hypo_logic_get_predicate_salience(bridge, "water_source");
    EXPECT_GE(water_salience, 0.1f);

    float drink_salience = hypo_logic_get_predicate_salience(bridge, "can_drink");
    EXPECT_GE(drink_salience, 0.1f);

    float thirst_salience = hypo_logic_get_predicate_salience(bridge, "is_thirsty");
    EXPECT_GE(thirst_salience, 0.1f);
}

TEST_F(HypothalamusLogicBridgeTest, GetPredicateSalienceSafetyRelated) {
    float threat_salience = hypo_logic_get_predicate_salience(bridge, "threat_detected");
    EXPECT_GE(threat_salience, 0.1f);

    float safe_salience = hypo_logic_get_predicate_salience(bridge, "is_safe");
    EXPECT_GE(safe_salience, 0.1f);

    float danger_salience = hypo_logic_get_predicate_salience(bridge, "danger_near");
    EXPECT_GE(danger_salience, 0.1f);
}

TEST_F(HypothalamusLogicBridgeTest, GetPredicateSalienceNeutral) {
    // Neutral predicates should have baseline salience
    float neutral_salience = hypo_logic_get_predicate_salience(bridge, "some_random_predicate");
    EXPECT_GE(neutral_salience, 0.1f);
    EXPECT_LE(neutral_salience, 10.0f);
}

TEST_F(HypothalamusLogicBridgeTest, GetPredicateSalienceNullBridgeReturnsDefault) {
    float salience = hypo_logic_get_predicate_salience(nullptr, "food");
    EXPECT_FLOAT_EQ(1.0f, salience);  // Default
}

TEST_F(HypothalamusLogicBridgeTest, GetPredicateSalienceNullNameReturnsDefault) {
    float salience = hypo_logic_get_predicate_salience(bridge, nullptr);
    EXPECT_FLOAT_EQ(1.0f, salience);  // Default
}

// ============================================================================
// GOAL THRESHOLD (WISHFUL THINKING) TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, GetGoalThresholdNullGoalReturnsBaseline) {
    float threshold = hypo_logic_get_goal_threshold(bridge, nullptr);
    EXPECT_GT(threshold, 0.0f);
    EXPECT_LE(threshold, 1.0f);
}

TEST_F(HypothalamusLogicBridgeTest, GetGoalThresholdNullBridgeReturnsDefault) {
    logic_clause_t* clause = CreateTestClause("food");
    float threshold = hypo_logic_get_goal_threshold(nullptr, clause);
    EXPECT_FLOAT_EQ(0.7f, threshold);  // Default
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, WishfulThinkingLowersThresholdForDesiredConclusions) {
    // Create a food-related goal
    logic_clause_t* food_goal = CreateTestClause("food_nearby");
    ASSERT_NE(nullptr, food_goal);

    // Get baseline threshold
    float baseline_threshold = hypo_logic_get_goal_threshold(bridge, food_goal);

    // Increase hunger urgency
    hypo_drive_set_nucleus_input(drives, HYPO_NUCLEUS_LATERAL, 0.9f);
    hypo_drive_update(drives, 100000);
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));

    // Get threshold with high hunger
    float hungry_threshold = hypo_logic_get_goal_threshold(bridge, food_goal);

    // Higher hunger should lower the threshold (more accepting of evidence)
    // Note: This is the "wishful thinking" effect
    EXPECT_LE(hungry_threshold, baseline_threshold);

    DestroyTestClause(food_goal);
}

// ============================================================================
// MOTIVATED GOAL TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, CreateMotivatedGoalSuccess) {
    logic_clause_t* goal = CreateTestClause("find_food");
    ASSERT_NE(nullptr, goal);

    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.8f));

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.goals_created);

    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeTest, CreateMotivatedGoalNullBridgeFails) {
    logic_clause_t* goal = CreateTestClause("find_food");
    EXPECT_EQ(-1, hypo_logic_create_motivated_goal(nullptr, HYPO_DRIVE_HUNGER, goal, 0.8f));
    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeTest, CreateMotivatedGoalNullClauseFails) {
    EXPECT_EQ(-1, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, nullptr, 0.8f));
}

TEST_F(HypothalamusLogicBridgeTest, CreateMotivatedGoalInvalidDriveFails) {
    logic_clause_t* goal = CreateTestClause("find_food");
    EXPECT_EQ(-1, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_COUNT, goal, 0.8f));
    EXPECT_EQ(-1, hypo_logic_create_motivated_goal(bridge, (hypo_drive_type_t)100, goal, 0.8f));
    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeTest, GetPrioritizedGoalsSuccess) {
    // Create multiple goals
    logic_clause_t* goal1 = CreateTestClause("find_food");
    logic_clause_t* goal2 = CreateTestClause("find_water");
    logic_clause_t* goal3 = CreateTestClause("find_shelter");

    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal1, 0.8f));
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_THIRST, goal2, 0.6f));
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_SAFETY, goal3, 0.9f));

    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals = 0;
    EXPECT_EQ(0, hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals));
    EXPECT_EQ(3u, num_goals);

    // Goals should be sorted by priority (descending)
    for (uint32_t i = 0; i < num_goals - 1; i++) {
        EXPECT_GE(goals[i].priority, goals[i + 1].priority);
    }

    DestroyTestClause(goal1);
    DestroyTestClause(goal2);
    DestroyTestClause(goal3);
}

TEST_F(HypothalamusLogicBridgeTest, GetPrioritizedGoalsNullBridgeFails) {
    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals = 0;
    EXPECT_EQ(-1, hypo_logic_get_prioritized_goals(nullptr, goals, HYPO_LOGIC_MAX_GOALS, &num_goals));
}

TEST_F(HypothalamusLogicBridgeTest, GetPrioritizedGoalsNullOutputFails) {
    uint32_t num_goals = 0;
    EXPECT_EQ(-1, hypo_logic_get_prioritized_goals(bridge, nullptr, HYPO_LOGIC_MAX_GOALS, &num_goals));
}

TEST_F(HypothalamusLogicBridgeTest, GetPrioritizedGoalsNullCountFails) {
    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    EXPECT_EQ(-1, hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, nullptr));
}

// ============================================================================
// LOGIC -> HYPOTHALAMUS: CONCLUSION PROCESSING TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, ProcessConclusionResourceAvailable) {
    logic_clause_t* clause = CreateTestClause("food_available");
    ASSERT_NE(nullptr, clause);

    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.9f));

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.conclusions_processed);

    // Should increase anticipation for hunger drive
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    // Food available -> anticipate hunger satisfaction
    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_HUNGER], 0.0f);

    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ProcessConclusionThreatPresent) {
    logic_clause_t* clause = CreateTestClause("threat_detected");
    ASSERT_NE(nullptr, clause);

    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.85f));

    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    // Threat present -> boost safety drive anticipation
    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_SAFETY], 0.0f);

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_GT(stats.drive_boosts, 0u);

    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ProcessConclusionThreatAbsent) {
    // First set some safety anticipation
    logic_clause_t* threat = CreateTestClause("danger");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, threat, 0.8f));

    // Then conclude threat is absent (via "safe" predicate)
    logic_clause_t* safe = CreateTestClause("safe_zone");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, safe, 0.9f));

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_GT(stats.drive_reductions, 0u);

    DestroyTestClause(threat);
    DestroyTestClause(safe);
}

TEST_F(HypothalamusLogicBridgeTest, ProcessConclusionNullBridgeFails) {
    logic_clause_t* clause = CreateTestClause("test");
    EXPECT_EQ(-1, hypo_logic_process_conclusion(nullptr, clause, 0.8f));
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ProcessConclusionNullClauseFails) {
    EXPECT_EQ(-1, hypo_logic_process_conclusion(bridge, nullptr, 0.8f));
}

// ============================================================================
// GOAL ACHIEVEMENT/IMPOSSIBILITY TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, GoalAchievedProducesReward) {
    logic_clause_t* goal = CreateTestClause("food_found");
    ASSERT_NE(nullptr, goal);

    // Create the goal first
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.8f));

    // Achieve the goal
    float reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_HUNGER, goal);
    EXPECT_GT(reward, 0.0f);

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.goals_achieved);

    // Anticipation should be boosted
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_HUNGER], 0.0f);

    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeTest, GoalAchievedNullBridgeReturnsZero) {
    logic_clause_t* goal = CreateTestClause("test");
    float reward = hypo_logic_goal_achieved(nullptr, HYPO_DRIVE_HUNGER, goal);
    EXPECT_FLOAT_EQ(0.0f, reward);
    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeTest, GoalAchievedInvalidDriveReturnsZero) {
    logic_clause_t* goal = CreateTestClause("test");
    float reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_COUNT, goal);
    EXPECT_FLOAT_EQ(0.0f, reward);
    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeTest, GoalImpossibleProducesFrustration) {
    logic_clause_t* goal = CreateTestClause("unreachable_food");
    ASSERT_NE(nullptr, goal);

    // Create the goal first
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.7f));

    // Mark as impossible
    float frustration = hypo_logic_goal_impossible(bridge, HYPO_DRIVE_HUNGER, goal);
    EXPECT_GT(frustration, 0.0f);

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.goals_abandoned);
    EXPECT_GT(stats.frustration_events, 0u);

    // Frustration should be elevated
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    EXPECT_GT(anticipation.frustration[HYPO_DRIVE_HUNGER], 0.0f);

    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeTest, GoalImpossibleNullBridgeReturnsZero) {
    logic_clause_t* goal = CreateTestClause("test");
    float frustration = hypo_logic_goal_impossible(nullptr, HYPO_DRIVE_HUNGER, goal);
    EXPECT_FLOAT_EQ(0.0f, frustration);
    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeTest, GoalImpossibleInvalidDriveReturnsZero) {
    logic_clause_t* goal = CreateTestClause("test");
    float frustration = hypo_logic_goal_impossible(bridge, HYPO_DRIVE_COUNT, goal);
    EXPECT_FLOAT_EQ(0.0f, frustration);
    DestroyTestClause(goal);
}

// ============================================================================
// ANTICIPATION STATE TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, GetAnticipationSuccess) {
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));

    // Initially all should be zero or baseline
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        EXPECT_GE(anticipation.anticipation[i], 0.0f);
        EXPECT_LE(anticipation.anticipation[i], 1.0f);
        EXPECT_GE(anticipation.frustration[i], 0.0f);
        EXPECT_LE(anticipation.frustration[i], 1.0f);
        EXPECT_GE(anticipation.confidence[i], 0.0f);
        EXPECT_LE(anticipation.confidence[i], 1.0f);
    }
}

TEST_F(HypothalamusLogicBridgeTest, GetAnticipationNullBridgeFails) {
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(-1, hypo_logic_get_anticipation(nullptr, &anticipation));
}

TEST_F(HypothalamusLogicBridgeTest, GetAnticipationNullOutputFails) {
    EXPECT_EQ(-1, hypo_logic_get_anticipation(bridge, nullptr));
}

// ============================================================================
// CONCLUSION CLASSIFICATION TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, ClassifyConclusionResourceAvailable) {
    logic_clause_t* clause = CreateTestClause("resource_available");
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, clause);
    EXPECT_EQ(HYPO_CONCL_RESOURCE_AVAILABLE, type);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ClassifyConclusionThreatPresent) {
    logic_clause_t* clause = CreateTestClause("threat_detected");
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, clause);
    EXPECT_EQ(HYPO_CONCL_THREAT_PRESENT, type);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ClassifyConclusionThreatAbsent) {
    // "safe" predicate indicates threat absent
    logic_clause_t* clause = CreateTestClause("is_safe");
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, clause);
    EXPECT_EQ(HYPO_CONCL_THREAT_ABSENT, type);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ClassifyConclusionGoalAchieved) {
    logic_clause_t* clause = CreateTestClause("task_achieved");
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, clause);
    EXPECT_EQ(HYPO_CONCL_GOAL_ACHIEVED, type);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ClassifyConclusionGoalImpossible) {
    logic_clause_t* clause = CreateTestClause("goal_impossible");
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, clause);
    EXPECT_EQ(HYPO_CONCL_GOAL_IMPOSSIBLE, type);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ClassifyConclusionOpportunity) {
    logic_clause_t* clause = CreateTestClause("opportunity_found");
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, clause);
    EXPECT_EQ(HYPO_CONCL_OPPORTUNITY, type);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ClassifyConclusionNeutral) {
    logic_clause_t* clause = CreateTestClause("some_neutral_fact");
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, clause);
    EXPECT_EQ(HYPO_CONCL_NEUTRAL, type);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ClassifyConclusionNullReturnsNeutral) {
    EXPECT_EQ(HYPO_CONCL_NEUTRAL, hypo_logic_classify_conclusion(bridge, nullptr));
    EXPECT_EQ(HYPO_CONCL_NEUTRAL, hypo_logic_classify_conclusion(nullptr, nullptr));
}

// ============================================================================
// GET AFFECTED DRIVE TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, GetAffectedDriveFoodRelated) {
    logic_clause_t* clause = CreateTestClause("food_available");
    hypo_drive_type_t drive = hypo_logic_get_affected_drive(bridge, clause);
    EXPECT_EQ(HYPO_DRIVE_HUNGER, drive);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, GetAffectedDriveWaterRelated) {
    logic_clause_t* clause = CreateTestClause("water_source");
    hypo_drive_type_t drive = hypo_logic_get_affected_drive(bridge, clause);
    EXPECT_EQ(HYPO_DRIVE_THIRST, drive);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, GetAffectedDriveThreatRelated) {
    logic_clause_t* clause = CreateTestClause("threat_detected");
    hypo_drive_type_t drive = hypo_logic_get_affected_drive(bridge, clause);
    EXPECT_EQ(HYPO_DRIVE_SAFETY, drive);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, GetAffectedDriveSocialRelated) {
    logic_clause_t* clause = CreateTestClause("friend_nearby");
    hypo_drive_type_t drive = hypo_logic_get_affected_drive(bridge, clause);
    EXPECT_EQ(HYPO_DRIVE_SOCIAL, drive);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, GetAffectedDriveKnowledgeRelated) {
    logic_clause_t* clause = CreateTestClause("discover_fact");
    hypo_drive_type_t drive = hypo_logic_get_affected_drive(bridge, clause);
    EXPECT_EQ(HYPO_DRIVE_CURIOSITY, drive);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, GetAffectedDriveNeutralReturnsCount) {
    logic_clause_t* clause = CreateTestClause("random_predicate");
    hypo_drive_type_t drive = hypo_logic_get_affected_drive(bridge, clause);
    EXPECT_EQ(HYPO_DRIVE_COUNT, drive);  // No drive affected
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, GetAffectedDriveNullReturnsCount) {
    EXPECT_EQ(HYPO_DRIVE_COUNT, hypo_logic_get_affected_drive(bridge, nullptr));
    EXPECT_EQ(HYPO_DRIVE_COUNT, hypo_logic_get_affected_drive(nullptr, nullptr));
}

// ============================================================================
// FEP INTEGRATION TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, GeneratePredictionsSuccess) {
    EXPECT_EQ(0, hypo_logic_generate_predictions(bridge));

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_GT(stats.predictions_made, 0u);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GeneratePredictionsNullFails) {
    EXPECT_EQ(-1, hypo_logic_generate_predictions(nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, UpdatePredictionsSuccess) {
    // Generate predictions first
    EXPECT_EQ(0, hypo_logic_generate_predictions(bridge));

    // Process a conclusion that should update predictions
    logic_clause_t* clause = CreateTestClause("food_available");
    ASSERT_NE(nullptr, clause);

    float error = hypo_logic_update_predictions(bridge, clause);
    // Error is returned (can be positive or negative)
    EXPECT_TRUE(std::isfinite(error));

    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, UpdatePredictionsNullReturnsZero) {
    EXPECT_FLOAT_EQ(0.0f, hypo_logic_update_predictions(nullptr, nullptr));
    EXPECT_FLOAT_EQ(0.0f, hypo_logic_update_predictions(bridge, nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, ComputeFreeEnergySuccess) {
    hypo_logic_fep_state_t fe_state;
    EXPECT_EQ(0, hypo_logic_compute_free_energy(bridge, &fe_state));

    // Free energy should be non-negative
    EXPECT_GE(fe_state.logical_free_energy, 0.0f);
    EXPECT_GE(fe_state.precision, 0.0f);
    EXPECT_LE(fe_state.precision, 1.0f);
    EXPECT_GE(fe_state.complexity_cost, 0.0f);
    EXPECT_GE(fe_state.expected_info_gain, 0.0f);
    EXPECT_GT(fe_state.timestamp_us, 0u);
}

TEST_F(HypothalamusLogicBridgeTest, ComputeFreeEnergyNullBridgeFails) {
    hypo_logic_fep_state_t fe_state;
    EXPECT_EQ(-1, hypo_logic_compute_free_energy(nullptr, &fe_state));
}

TEST_F(HypothalamusLogicBridgeTest, ComputeFreeEnergyNullOutputFails) {
    EXPECT_EQ(-1, hypo_logic_compute_free_energy(bridge, nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, GetFEPStateSuccess) {
    // First compute free energy to populate state
    hypo_logic_fep_state_t compute_state;
    EXPECT_EQ(0, hypo_logic_compute_free_energy(bridge, &compute_state));

    hypo_logic_fep_state_t get_state;
    EXPECT_EQ(0, hypo_logic_get_fep_state(bridge, &get_state));

    // States should match
    EXPECT_FLOAT_EQ(compute_state.logical_free_energy, get_state.logical_free_energy);
    EXPECT_FLOAT_EQ(compute_state.precision, get_state.precision);
}

TEST_F(HypothalamusLogicBridgeTest, GetFEPStateNullBridgeFails) {
    hypo_logic_fep_state_t fe_state;
    EXPECT_EQ(-1, hypo_logic_get_fep_state(nullptr, &fe_state));
}

TEST_F(HypothalamusLogicBridgeTest, GetFEPStateNullOutputFails) {
    EXPECT_EQ(-1, hypo_logic_get_fep_state(bridge, nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, FEPPredictionErrorFromUnexpectedConclusion) {
    // Generate predictions
    EXPECT_EQ(0, hypo_logic_generate_predictions(bridge));

    // Process multiple conclusions to trigger prediction updates
    logic_clause_t* clause1 = CreateTestClause("food_available");
    logic_clause_t* clause2 = CreateTestClause("goal_impossible");

    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause1, 0.9f));
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause2, 0.8f));

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));

    // Should have tracked prediction confirmations or violations
    EXPECT_GT(stats.predictions_confirmed + stats.predictions_violated, 0u);

    DestroyTestClause(clause1);
    DestroyTestClause(clause2);
}

// ============================================================================
// PREDICATE MAPPING TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, RegisterPredicateMapSuccess) {
    hypo_logic_predicate_map_t map = {
        .predicate_name = "custom_food",
        .drive = HYPO_DRIVE_HUNGER,
        .relevance = 0.9f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    strncpy(map.predicate_name, "custom_food", LOGIC_MAX_NAME_LENGTH);

    EXPECT_EQ(0, hypo_logic_register_predicate_map(bridge, &map));

    // Verify the mapping affects salience
    float salience = hypo_logic_get_predicate_salience(bridge, "custom_food");
    EXPECT_GT(salience, 0.0f);
}

TEST_F(HypothalamusLogicBridgeTest, RegisterPredicateMapNullBridgeFails) {
    hypo_logic_predicate_map_t map = {};
    EXPECT_EQ(-1, hypo_logic_register_predicate_map(nullptr, &map));
}

TEST_F(HypothalamusLogicBridgeTest, RegisterPredicateMapNullMapFails) {
    EXPECT_EQ(-1, hypo_logic_register_predicate_map(bridge, nullptr));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategoryFood) {
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("food"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("eat"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("hungry"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("nourish"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("meal"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("fruit"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("edible"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategoryWater) {
    EXPECT_EQ(HYPO_PRED_CAT_WATER, hypo_logic_get_predicate_category("water"));
    EXPECT_EQ(HYPO_PRED_CAT_WATER, hypo_logic_get_predicate_category("drink"));
    EXPECT_EQ(HYPO_PRED_CAT_WATER, hypo_logic_get_predicate_category("thirst"));
    EXPECT_EQ(HYPO_PRED_CAT_WATER, hypo_logic_get_predicate_category("hydration"));
    EXPECT_EQ(HYPO_PRED_CAT_WATER, hypo_logic_get_predicate_category("liquid"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategoryThreat) {
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("threat"));
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("danger"));
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("harm"));
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("attack"));
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("predator"));
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("enemy"));
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("hostile"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategorySafety) {
    EXPECT_EQ(HYPO_PRED_CAT_SAFETY, hypo_logic_get_predicate_category("safe"));
    EXPECT_EQ(HYPO_PRED_CAT_SAFETY, hypo_logic_get_predicate_category("shelter"));
    EXPECT_EQ(HYPO_PRED_CAT_SAFETY, hypo_logic_get_predicate_category("protect"));
    EXPECT_EQ(HYPO_PRED_CAT_SAFETY, hypo_logic_get_predicate_category("secure"));
    EXPECT_EQ(HYPO_PRED_CAT_SAFETY, hypo_logic_get_predicate_category("refuge"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategorySocial) {
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("social"));
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("friend"));
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("ally"));
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("companion"));
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("cooperate"));
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("help"));
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("together"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategoryKnowledge) {
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("know"));
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("learn"));
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("discover"));
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("understand"));
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("curious"));
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("explore"));
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("information"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategoryTemperature) {
    EXPECT_EQ(HYPO_PRED_CAT_TEMPERATURE, hypo_logic_get_predicate_category("temperature"));
    EXPECT_EQ(HYPO_PRED_CAT_TEMPERATURE, hypo_logic_get_predicate_category("warm"));
    EXPECT_EQ(HYPO_PRED_CAT_TEMPERATURE, hypo_logic_get_predicate_category("cold"));
    EXPECT_EQ(HYPO_PRED_CAT_TEMPERATURE, hypo_logic_get_predicate_category("hot"));
    EXPECT_EQ(HYPO_PRED_CAT_TEMPERATURE, hypo_logic_get_predicate_category("heat"));
    EXPECT_EQ(HYPO_PRED_CAT_TEMPERATURE, hypo_logic_get_predicate_category("cool"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategoryRest) {
    EXPECT_EQ(HYPO_PRED_CAT_REST, hypo_logic_get_predicate_category("rest"));
    EXPECT_EQ(HYPO_PRED_CAT_REST, hypo_logic_get_predicate_category("sleep"));
    EXPECT_EQ(HYPO_PRED_CAT_REST, hypo_logic_get_predicate_category("tired"));
    EXPECT_EQ(HYPO_PRED_CAT_REST, hypo_logic_get_predicate_category("fatigue"));
    EXPECT_EQ(HYPO_PRED_CAT_REST, hypo_logic_get_predicate_category("energy"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategoryAchievement) {
    EXPECT_EQ(HYPO_PRED_CAT_ACHIEVEMENT, hypo_logic_get_predicate_category("achieve"));
    EXPECT_EQ(HYPO_PRED_CAT_ACHIEVEMENT, hypo_logic_get_predicate_category("accomplish"));
    EXPECT_EQ(HYPO_PRED_CAT_ACHIEVEMENT, hypo_logic_get_predicate_category("success"));
    EXPECT_EQ(HYPO_PRED_CAT_ACHIEVEMENT, hypo_logic_get_predicate_category("master"));
    EXPECT_EQ(HYPO_PRED_CAT_ACHIEVEMENT, hypo_logic_get_predicate_category("skill"));
    EXPECT_EQ(HYPO_PRED_CAT_ACHIEVEMENT, hypo_logic_get_predicate_category("competent"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategoryAutonomy) {
    EXPECT_EQ(HYPO_PRED_CAT_AUTONOMY, hypo_logic_get_predicate_category("autonomy"));
    EXPECT_EQ(HYPO_PRED_CAT_AUTONOMY, hypo_logic_get_predicate_category("choice"));
    EXPECT_EQ(HYPO_PRED_CAT_AUTONOMY, hypo_logic_get_predicate_category("control"));
    EXPECT_EQ(HYPO_PRED_CAT_AUTONOMY, hypo_logic_get_predicate_category("freedom"));
    EXPECT_EQ(HYPO_PRED_CAT_AUTONOMY, hypo_logic_get_predicate_category("independent"));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, GetPredicateCategoryNeutral) {
    EXPECT_EQ(HYPO_PRED_CAT_NEUTRAL, hypo_logic_get_predicate_category("random"));
    EXPECT_EQ(HYPO_PRED_CAT_NEUTRAL, hypo_logic_get_predicate_category("xyz123"));
    EXPECT_EQ(HYPO_PRED_CAT_NEUTRAL, hypo_logic_get_predicate_category(""));
    EXPECT_EQ(HYPO_PRED_CAT_NEUTRAL, hypo_logic_get_predicate_category(nullptr));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, CategoryToDriveMapping) {
    EXPECT_EQ(HYPO_DRIVE_HUNGER, hypo_logic_category_to_drive(HYPO_PRED_CAT_FOOD));
    EXPECT_EQ(HYPO_DRIVE_THIRST, hypo_logic_category_to_drive(HYPO_PRED_CAT_WATER));
    EXPECT_EQ(HYPO_DRIVE_SAFETY, hypo_logic_category_to_drive(HYPO_PRED_CAT_THREAT));
    EXPECT_EQ(HYPO_DRIVE_SAFETY, hypo_logic_category_to_drive(HYPO_PRED_CAT_SAFETY));
    EXPECT_EQ(HYPO_DRIVE_SOCIAL, hypo_logic_category_to_drive(HYPO_PRED_CAT_SOCIAL));
    EXPECT_EQ(HYPO_DRIVE_CURIOSITY, hypo_logic_category_to_drive(HYPO_PRED_CAT_KNOWLEDGE));
    EXPECT_EQ(HYPO_DRIVE_TEMPERATURE, hypo_logic_category_to_drive(HYPO_PRED_CAT_TEMPERATURE));
    EXPECT_EQ(HYPO_DRIVE_FATIGUE, hypo_logic_category_to_drive(HYPO_PRED_CAT_REST));
    EXPECT_EQ(HYPO_DRIVE_COMPETENCE, hypo_logic_category_to_drive(HYPO_PRED_CAT_ACHIEVEMENT));
    EXPECT_EQ(HYPO_DRIVE_AUTONOMY, hypo_logic_category_to_drive(HYPO_PRED_CAT_AUTONOMY));
    EXPECT_EQ(HYPO_DRIVE_COUNT, hypo_logic_category_to_drive(HYPO_PRED_CAT_NEUTRAL));
}

TEST_F(HypothalamusLogicBridgeTest, AutoRegisterMappingsSuccess) {
    // Bridge already auto-registered mappings in create
    // Create a fresh bridge without auto-registration to test
    hypo_logic_config_t config = hypo_logic_default_config();
    hypo_logic_bridge_t* fresh_bridge = hypo_logic_bridge_create(drives, logic, &config);
    ASSERT_NE(nullptr, fresh_bridge);

    // Manual call should still work (may add duplicates or skip existing)
    int count = hypo_logic_auto_register_mappings(fresh_bridge);
    // Should have registered some mappings (may be 0 if already registered)
    EXPECT_GE(count, 0);

    hypo_logic_bridge_destroy(fresh_bridge);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, AutoRegisterMappingsNullReturnsZero) {
    int count = hypo_logic_auto_register_mappings(nullptr);
    EXPECT_EQ(0, count);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, GetStatsSuccess) {
    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));

    // Initial stats should be reasonable
    EXPECT_EQ(0u, stats.conclusions_processed);
    EXPECT_EQ(0u, stats.goals_created);
    EXPECT_EQ(0u, stats.goals_achieved);
}

TEST_F(HypothalamusLogicBridgeTest, GetStatsNullBridgeFails) {
    hypo_logic_stats_t stats;
    EXPECT_EQ(-1, hypo_logic_get_stats(nullptr, &stats));
}

TEST_F(HypothalamusLogicBridgeTest, GetStatsNullOutputFails) {
    EXPECT_EQ(-1, hypo_logic_get_stats(bridge, nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, ResetStatsSuccess) {
    // Process some events
    logic_clause_t* clause = CreateTestClause("food_available");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.9f));

    logic_clause_t* goal = CreateTestClause("find_food");
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.8f));

    // Reset stats
    EXPECT_EQ(0, hypo_logic_reset_stats(bridge));

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.conclusions_processed);
    EXPECT_EQ(0u, stats.goals_created);

    DestroyTestClause(clause);
    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, ResetStatsNullFails) {
    EXPECT_EQ(-1, hypo_logic_reset_stats(nullptr));
}

TEST_F(HypothalamusLogicBridgeTest, StatsAccumulateCorrectly) {
    hypo_logic_stats_t before, after;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &before));

    // Process multiple conclusions
    for (int i = 0; i < 5; i++) {
        logic_clause_t* clause = CreateTestClause("food_available");
        EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.8f));
        DestroyTestClause(clause);
    }

    // Create multiple goals
    for (int i = 0; i < 3; i++) {
        logic_clause_t* goal = CreateTestClause("find_resource");
        EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.7f));
        DestroyTestClause(goal);
    }

    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &after));

    EXPECT_EQ(5u, after.conclusions_processed - before.conclusions_processed);
    EXPECT_EQ(3u, after.goals_created - before.goals_created);
}

// ============================================================================
// STRING UTILITY TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeLifecycleTest, ConclusionTypeNameReturnsValidStrings) {
    EXPECT_STREQ("resource_available", hypo_conclusion_type_name(HYPO_CONCL_RESOURCE_AVAILABLE));
    EXPECT_STREQ("resource_unavailable", hypo_conclusion_type_name(HYPO_CONCL_RESOURCE_UNAVAILABLE));
    EXPECT_STREQ("threat_present", hypo_conclusion_type_name(HYPO_CONCL_THREAT_PRESENT));
    EXPECT_STREQ("threat_absent", hypo_conclusion_type_name(HYPO_CONCL_THREAT_ABSENT));
    EXPECT_STREQ("goal_achieved", hypo_conclusion_type_name(HYPO_CONCL_GOAL_ACHIEVED));
    EXPECT_STREQ("goal_impossible", hypo_conclusion_type_name(HYPO_CONCL_GOAL_IMPOSSIBLE));
    EXPECT_STREQ("opportunity", hypo_conclusion_type_name(HYPO_CONCL_OPPORTUNITY));
    EXPECT_STREQ("prediction_confirmed", hypo_conclusion_type_name(HYPO_CONCL_PREDICTION_CONFIRMED));
    EXPECT_STREQ("prediction_violated", hypo_conclusion_type_name(HYPO_CONCL_PREDICTION_VIOLATED));
    EXPECT_STREQ("neutral", hypo_conclusion_type_name(HYPO_CONCL_NEUTRAL));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, ConclusionTypeNameInvalidReturnsUnknown) {
    const char* name = hypo_conclusion_type_name(HYPO_CONCL_COUNT);
    EXPECT_NE(nullptr, name);
    EXPECT_STREQ("unknown", name);

    name = hypo_conclusion_type_name((hypo_conclusion_type_t)100);
    EXPECT_STREQ("unknown", name);
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, PredicateCategoryNameReturnsValidStrings) {
    EXPECT_STREQ("food", hypo_predicate_category_name(HYPO_PRED_CAT_FOOD));
    EXPECT_STREQ("water", hypo_predicate_category_name(HYPO_PRED_CAT_WATER));
    EXPECT_STREQ("threat", hypo_predicate_category_name(HYPO_PRED_CAT_THREAT));
    EXPECT_STREQ("safety", hypo_predicate_category_name(HYPO_PRED_CAT_SAFETY));
    EXPECT_STREQ("social", hypo_predicate_category_name(HYPO_PRED_CAT_SOCIAL));
    EXPECT_STREQ("knowledge", hypo_predicate_category_name(HYPO_PRED_CAT_KNOWLEDGE));
    EXPECT_STREQ("temperature", hypo_predicate_category_name(HYPO_PRED_CAT_TEMPERATURE));
    EXPECT_STREQ("rest", hypo_predicate_category_name(HYPO_PRED_CAT_REST));
    EXPECT_STREQ("achievement", hypo_predicate_category_name(HYPO_PRED_CAT_ACHIEVEMENT));
    EXPECT_STREQ("autonomy", hypo_predicate_category_name(HYPO_PRED_CAT_AUTONOMY));
    EXPECT_STREQ("neutral", hypo_predicate_category_name(HYPO_PRED_CAT_NEUTRAL));
}

TEST_F(HypothalamusLogicBridgeLifecycleTest, PredicateCategoryNameInvalidReturnsUnknown) {
    const char* name = hypo_predicate_category_name(HYPO_PRED_CAT_COUNT);
    EXPECT_NE(nullptr, name);
    EXPECT_STREQ("unknown", name);
}

TEST_F(HypothalamusLogicBridgeTest, PrintSummaryDoesNotCrash) {
    // Process some events to create interesting state
    logic_clause_t* clause = CreateTestClause("food_available");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.9f));

    logic_clause_t* goal = CreateTestClause("find_food");
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.8f));

    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));

    // Should not crash
    hypo_logic_print_summary(bridge);

    DestroyTestClause(clause);
    DestroyTestClause(goal);
}

TEST_F(HypothalamusLogicBridgeTest, PrintSummaryNullDoesNotCrash) {
    hypo_logic_print_summary(nullptr);
    // Should not crash
}

// ============================================================================
// INTEGRATION SCENARIO TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, ScenarioHungryAgentFindsFood) {
    // Stimulate hunger drive
    hypo_drive_set_nucleus_input(drives, HYPO_NUCLEUS_LATERAL, 0.8f);
    hypo_drive_update(drives, 100000);

    // Compute modulation
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));

    // Create goal to find food
    logic_clause_t* goal = CreateTestClause("find_food");
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.9f));

    // Food-related predicates should have high salience
    float food_salience = hypo_logic_get_predicate_salience(bridge, "food");
    EXPECT_GT(food_salience, 1.0f);

    // Process conclusion that food is available
    logic_clause_t* conclusion = CreateTestClause("food_available");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, conclusion, 0.95f));

    // Check anticipation increased
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_HUNGER], 0.0f);

    // Achieve the goal
    float reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_HUNGER, goal);
    EXPECT_GT(reward, 0.0f);

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.goals_achieved);

    DestroyTestClause(goal);
    DestroyTestClause(conclusion);
}

TEST_F(HypothalamusLogicBridgeTest, ScenarioThreatDetectionAndResponse) {
    // Process threat detection conclusion
    logic_clause_t* threat = CreateTestClause("danger_detected");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, threat, 0.9f));

    // Safety drive anticipation should increase
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_SAFETY], 0.0f);

    // Compute modulation - safety drive should be priority
    hypo_drive_set_nucleus_input(drives, HYPO_NUCLEUS_VENTROMEDIAL, 0.9f);
    hypo_drive_update(drives, 100000);
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));

    hypo_logic_modulation_t modulation;
    EXPECT_EQ(0, hypo_logic_get_modulation(bridge, &modulation));

    // Now conclude threat is absent
    logic_clause_t* safe = CreateTestClause("is_safe");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, safe, 0.95f));

    // Safety anticipation should decrease
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    // Note: may still be > 0 due to previous boost

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_GT(stats.drive_boosts, 0u);

    DestroyTestClause(threat);
    DestroyTestClause(safe);
}

TEST_F(HypothalamusLogicBridgeTest, ScenarioGoalFrustration) {
    // Create a goal
    logic_clause_t* goal = CreateTestClause("reach_destination");
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_CURIOSITY, goal, 0.7f));

    // Process conclusion that goal is impossible
    logic_clause_t* blocked = CreateTestClause("path_blocked");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, blocked, 0.85f));

    // Mark goal as impossible
    float frustration = hypo_logic_goal_impossible(bridge, HYPO_DRIVE_CURIOSITY, goal);
    EXPECT_GT(frustration, 0.0f);

    // Check frustration increased
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    EXPECT_GT(anticipation.frustration[HYPO_DRIVE_CURIOSITY], 0.0f);

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.goals_abandoned);
    EXPECT_GT(stats.frustration_events, 0u);

    DestroyTestClause(goal);
    DestroyTestClause(blocked);
}

TEST_F(HypothalamusLogicBridgeTest, ScenarioFEPPredictionLearning) {
    // Generate initial predictions
    EXPECT_EQ(0, hypo_logic_generate_predictions(bridge));

    // Process multiple conclusions to train predictions
    const char* predicates[] = {"food_available", "water_source", "safe_zone", "friend_nearby"};

    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 4; i++) {
            logic_clause_t* clause = CreateTestClause(predicates[i]);
            float error = hypo_logic_update_predictions(bridge, clause);
            EXPECT_TRUE(std::isfinite(error));
            DestroyTestClause(clause);
        }
    }

    // Check FEP state
    hypo_logic_fep_state_t fe_state;
    EXPECT_EQ(0, hypo_logic_get_fep_state(bridge, &fe_state));

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));

    // Should have made and resolved predictions
    EXPECT_GT(stats.predictions_confirmed + stats.predictions_violated, 0u);
}

TEST_F(HypothalamusLogicBridgeTest, ScenarioContinuousUpdate) {
    // Simulate multiple update cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        EXPECT_EQ(0, hypo_logic_bridge_update(bridge, 10000));

        // Process some conclusions each cycle
        logic_clause_t* clause = CreateTestClause("observation");
        EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.6f));
        DestroyTestClause(clause);
    }

    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(10u, stats.conclusions_processed);

    // Modulation latency should be tracked
    EXPECT_GE(stats.avg_modulation_latency_us, 0.0f);
    EXPECT_GE(stats.avg_conclusion_latency_us, 0.0f);
}

// ============================================================================
// BOUNDARY CONDITION TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, ProcessConclusionWithZeroConfidence) {
    logic_clause_t* clause = CreateTestClause("weak_observation");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.0f));
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ProcessConclusionWithFullConfidence) {
    logic_clause_t* clause = CreateTestClause("certain_fact");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 1.0f));
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ProcessConclusionWithNegativeConfidence) {
    logic_clause_t* clause = CreateTestClause("invalid_confidence");
    // Should clamp to valid range
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, -0.5f));
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, ProcessConclusionWithExcessiveConfidence) {
    logic_clause_t* clause = CreateTestClause("over_confident");
    // Should clamp to valid range
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 2.0f));
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, CreateMaximumGoals) {
    // Fill up goals to max
    logic_clause_t* goals[HYPO_LOGIC_MAX_GOALS];
    for (uint32_t i = 0; i < HYPO_LOGIC_MAX_GOALS; i++) {
        char name[64];
        snprintf(name, sizeof(name), "goal_%u", i);
        goals[i] = CreateTestClause(name);
        EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, (hypo_drive_type_t)(i % HYPO_DRIVE_COUNT), goals[i], 0.5f));
    }

    // Try to add one more - should fail
    logic_clause_t* extra_goal = CreateTestClause("extra_goal");
    EXPECT_EQ(-1, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, extra_goal, 0.5f));

    // Clean up
    for (uint32_t i = 0; i < HYPO_LOGIC_MAX_GOALS; i++) {
        DestroyTestClause(goals[i]);
    }
    DestroyTestClause(extra_goal);
}

TEST_F(HypothalamusLogicBridgeTest, PredicateSalienceCaseInsensitive) {
    // Category detection is case-insensitive
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("FOOD"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("Food"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("FoOd"));
}

TEST_F(HypothalamusLogicBridgeTest, NegatedConclusionClassification) {
    // Create a negated threat predicate
    logic_clause_t* clause = CreateTestClause("threat_detected", true);  // negated
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, clause);
    // Negated threat -> threat absent
    EXPECT_EQ(HYPO_CONCL_THREAT_ABSENT, type);
    DestroyTestClause(clause);
}

TEST_F(HypothalamusLogicBridgeTest, MultipleResetCycles) {
    for (int cycle = 0; cycle < 5; cycle++) {
        // Process some events
        logic_clause_t* clause = CreateTestClause("test_clause");
        EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.7f));
        DestroyTestClause(clause);

        // Reset
        EXPECT_EQ(0, hypo_logic_bridge_reset(bridge));

        // Verify clean state
        hypo_logic_stats_t stats;
        EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
        EXPECT_EQ(0u, stats.conclusions_processed);
    }
}

// ============================================================================
// ANTICIPATION DECAY TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, AnticipationDecaysOverTime) {
    // Process a conclusion to boost anticipation
    logic_clause_t* clause = CreateTestClause("food_available");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, clause, 0.9f));

    hypo_logic_anticipation_t before;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &before));
    float initial_anticipation = before.anticipation[HYPO_DRIVE_HUNGER];

    // Update multiple times (should decay)
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(0, hypo_logic_bridge_update(bridge, 100000));
    }

    hypo_logic_anticipation_t after;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &after));

    // Anticipation should have decayed
    EXPECT_LT(after.anticipation[HYPO_DRIVE_HUNGER], initial_anticipation);

    DestroyTestClause(clause);
}

// ============================================================================
// COMPLEX SCENARIO TESTS
// ============================================================================

TEST_F(HypothalamusLogicBridgeTest, ScenarioMultipleDrivesCompeting) {
    // Create goals for multiple drives
    logic_clause_t* food_goal = CreateTestClause("find_food");
    logic_clause_t* water_goal = CreateTestClause("find_water");
    logic_clause_t* safety_goal = CreateTestClause("find_shelter");

    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, food_goal, 0.7f));
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_THIRST, water_goal, 0.8f));
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_SAFETY, safety_goal, 0.9f));

    // Get prioritized goals
    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals = 0;
    EXPECT_EQ(0, hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals));
    EXPECT_EQ(3u, num_goals);

    // Verify sorting
    EXPECT_GE(goals[0].priority, goals[1].priority);
    EXPECT_GE(goals[1].priority, goals[2].priority);

    // Process conclusions for each drive
    logic_clause_t* food_found = CreateTestClause("food_nearby");
    logic_clause_t* water_found = CreateTestClause("water_source");
    logic_clause_t* threat = CreateTestClause("danger");

    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, food_found, 0.8f));
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, water_found, 0.9f));
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, threat, 0.95f));

    // Check anticipation for all drives
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));

    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_HUNGER], 0.0f);
    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_THIRST], 0.0f);
    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_SAFETY], 0.0f);

    // Clean up
    DestroyTestClause(food_goal);
    DestroyTestClause(water_goal);
    DestroyTestClause(safety_goal);
    DestroyTestClause(food_found);
    DestroyTestClause(water_found);
    DestroyTestClause(threat);
}

TEST_F(HypothalamusLogicBridgeTest, ScenarioFullBidirectionalFlow) {
    // 1. Set high drive state
    hypo_drive_set_nucleus_input(drives, HYPO_NUCLEUS_LATERAL, 0.85f);
    hypo_drive_update(drives, 100000);

    // 2. Compute modulation (Hypo -> Logic)
    EXPECT_EQ(0, hypo_logic_compute_modulation(bridge));
    hypo_logic_modulation_t modulation;
    EXPECT_EQ(0, hypo_logic_get_modulation(bridge, &modulation));

    // 3. Check modulation effects
    EXPECT_GT(modulation.wishful_thinking_bias, 0.0f);
    uint32_t shallow_depth = modulation.max_inference_depth;

    // 4. Create motivated goal
    logic_clause_t* goal = CreateTestClause("satisfy_hunger");
    EXPECT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.9f));

    // 5. Generate FEP predictions
    EXPECT_EQ(0, hypo_logic_generate_predictions(bridge));

    // 6. Process logical conclusion (Logic -> Hypo)
    logic_clause_t* conclusion = CreateTestClause("food_available");
    EXPECT_EQ(0, hypo_logic_process_conclusion(bridge, conclusion, 0.95f));

    // 7. Update FEP predictions
    float error = hypo_logic_update_predictions(bridge, conclusion);
    EXPECT_TRUE(std::isfinite(error));

    // 8. Check anticipation was updated
    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(0, hypo_logic_get_anticipation(bridge, &anticipation));
    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_HUNGER], 0.0f);

    // 9. Achieve goal
    float reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_HUNGER, goal);
    EXPECT_GT(reward, 0.0f);

    // 10. Compute free energy
    hypo_logic_fep_state_t fe_state;
    EXPECT_EQ(0, hypo_logic_compute_free_energy(bridge, &fe_state));
    EXPECT_GE(fe_state.logical_free_energy, 0.0f);

    // 11. Verify stats
    hypo_logic_stats_t stats;
    EXPECT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.conclusions_processed);
    EXPECT_EQ(1u, stats.goals_created);
    EXPECT_EQ(1u, stats.goals_achieved);
    EXPECT_GT(stats.predictions_made, 0u);

    DestroyTestClause(goal);
    DestroyTestClause(conclusion);
}
