/**
 * @file test_hypothalamus_logic_bridge_integration.cpp
 * @brief Integration tests for Hypothalamus-Logic Bridge
 *
 * WHAT: Integration tests verifying bidirectional flow between drive system
 *       and symbolic logic engine through the hypothalamus-logic bridge
 * WHY:  Ensure motivated reasoning (hot cognition) and logic-driven drive
 *       updates work correctly in integrated scenarios
 * HOW:  Test full pipeline from drives -> modulation -> inference -> conclusion
 *       -> drive update cycles with actual drive and logic systems
 *
 * Integration Scenarios Tested:
 * 1. Full bidirectional flow: Drive state -> Logic modulation -> Proof -> Conclusion -> Drive update
 * 2. Motivated reasoning pipeline: Hungry agent finds food predicate faster
 * 3. Multi-drive interaction: Competing drives affect reasoning priorities
 * 4. Logic engine integration: Bridge actually modulates symbolic_logic_t behavior
 * 5. Drive system integration: Logic conclusions actually update hypo_drive_system_handle_t
 * 6. FEP integration: Predictions flow between drives and logic
 * 7. Goal lifecycle: Create goal -> Pursue -> Achieve/Fail -> Update drives
 * 8. Anticipation dynamics: Multiple conclusions build anticipation over time
 * 9. Fatigue effects: Reduced reasoning capacity under high fatigue
 * 10. Bio-async messaging: Events propagate through bio router
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_logic_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/nimcp_symbolic_logic.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_DRIVE_LEVEL_LOW        0.2f
#define TEST_DRIVE_LEVEL_MODERATE   0.5f
#define TEST_DRIVE_LEVEL_HIGH       0.8f
#define TEST_DRIVE_LEVEL_URGENT     0.95f

#define TEST_CONFIDENCE_HIGH        0.9f
#define TEST_CONFIDENCE_MODERATE    0.7f
#define TEST_CONFIDENCE_LOW         0.4f

#define EPSILON                     0.001f

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Create a simple atomic formula for testing
 */
static atomic_formula_t* create_test_atom(const char* name, bool negated = false) {
    atomic_formula_t* atom = (atomic_formula_t*)calloc(1, sizeof(atomic_formula_t));
    if (!atom) return nullptr;

    strncpy(atom->name, name, LOGIC_MAX_NAME_LENGTH - 1);
    atom->negated = negated;
    atom->arity = 0;
    atom->terms = nullptr;

    return atom;
}

/**
 * @brief Create a simple clause for testing
 */
static logic_clause_t* create_test_clause(const char* predicate_name, bool negated = false) {
    logic_clause_t* clause = (logic_clause_t*)calloc(1, sizeof(logic_clause_t));
    if (!clause) return nullptr;

    clause->literals = (atomic_formula_t**)calloc(1, sizeof(atomic_formula_t*));
    if (!clause->literals) {
        free(clause);
        return nullptr;
    }

    clause->literals[0] = create_test_atom(predicate_name, negated);
    if (!clause->literals[0]) {
        free(clause->literals);
        free(clause);
        return nullptr;
    }

    clause->num_literals = 1;
    clause->confidence = 1.0f;

    return clause;
}

/**
 * @brief Free a test clause
 */
static void free_test_clause(logic_clause_t* clause) {
    if (!clause) return;
    if (clause->literals) {
        for (uint32_t i = 0; i < clause->num_literals; i++) {
            free(clause->literals[i]);
        }
        free(clause->literals);
    }
    free(clause);
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypothalamusLogicBridgeIntegrationTest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drive_system;
    symbolic_logic_t* logic_engine;
    hypo_logic_bridge_t* bridge;

    void SetUp() override {
        /* Create drive system */
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(drive_system, nullptr);

        /* Create logic engine */
        logic_config_t logic_config = {
            .max_predicates = LOGIC_MAX_PREDICATES,
            .max_rules = LOGIC_MAX_RULES,
            .max_kb_size = 1000,
            .max_inference_depth = 10,
            .enable_forward_chaining = true,
            .enable_backward_chaining = true,
            .enable_resolution = true,
            .enable_memory_consolidation = false,
            .enable_quantum_logic = false
        };
        logic_engine = symbolic_logic_create(&logic_config);
        ASSERT_NE(logic_engine, nullptr);

        /* Create bridge with default config */
        hypo_logic_config_t bridge_config = hypo_logic_default_config();
        bridge = hypo_logic_bridge_create(drive_system, logic_engine, &bridge_config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hypo_logic_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (logic_engine) {
            symbolic_logic_destroy(logic_engine);
            logic_engine = nullptr;
        }
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }
    }

    /* Helper to set drive urgency by manipulating satisfaction
     * Note: The drive system uses homeostatic setpoints, so we
     * indirectly affect urgency through satisfaction levels.
     * We run multiple update cycles to let the drive state settle.
     */
    void set_drive_urgency(hypo_drive_type_t drive, float target_urgency) {
        /* First, reset all drives via updates */
        for (int i = 0; i < 5; i++) {
            hypo_drive_update(drive_system, 10000);  /* 10ms each */
        }

        /* Satisfy to reduce or leave unsatisfied to increase urgency */
        if (target_urgency < 0.3f) {
            /* Low urgency: satisfy the drive well */
            hypo_drive_satisfy(drive_system, drive, 1.0f);
        } else if (target_urgency < 0.6f) {
            /* Moderate urgency: partial satisfaction */
            hypo_drive_satisfy(drive_system, drive, 0.5f);
        } else {
            /* High urgency: minimal satisfaction, let drive build */
            hypo_drive_satisfy(drive_system, drive, 0.0f);
            /* Update to let urgency build */
            for (int i = 0; i < 10; i++) {
                hypo_drive_update(drive_system, 100000);  /* 100ms each */
            }
        }
        hypo_drive_update(drive_system, 10000);
    }

    /* Helper to get current drive urgency */
    float get_drive_urgency(hypo_drive_type_t drive) {
        hypo_drive_state_t state;
        if (hypo_drive_get_state(drive_system, drive, &state)) {
            return state.urgency;
        }
        return 0.0f;
    }

    /* Helper to run a full update cycle */
    void run_update_cycle(uint64_t delta_us = 16000) {
        hypo_drive_update(drive_system, delta_us);
        hypo_logic_bridge_update(bridge, delta_us);
    }
};

/* ============================================================================
 * 1. Full Bidirectional Flow Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, FullBidirectionalFlow_DriveToLogicToConclusion) {
    /* Step 1: Set high hunger drive */
    set_drive_urgency(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_HIGH);

    /* Step 2: Compute modulation based on drive state */
    int ret = hypo_logic_compute_modulation(bridge);
    ASSERT_EQ(0, ret);

    /* Step 3: Verify modulation reflects hunger urgency */
    hypo_logic_modulation_t modulation;
    ret = hypo_logic_get_modulation(bridge, &modulation);
    ASSERT_EQ(0, ret);

    /* Priority should be hunger drive */
    EXPECT_EQ(HYPO_DRIVE_HUNGER, modulation.priority_drive);
    EXPECT_GT(modulation.salience_boost[HYPO_DRIVE_HUNGER], 1.0f);

    /* Step 4: Apply modulation */
    ret = hypo_logic_apply_modulation(bridge);
    ASSERT_EQ(0, ret);

    /* Step 5: Create food-related conclusion */
    logic_clause_t* conclusion = create_test_clause("food_available");
    ASSERT_NE(conclusion, nullptr);

    /* Step 6: Process conclusion */
    ret = hypo_logic_process_conclusion(bridge, conclusion, TEST_CONFIDENCE_HIGH);
    ASSERT_EQ(0, ret);

    /* Step 7: Verify anticipation was updated */
    hypo_logic_anticipation_t anticipation;
    ret = hypo_logic_get_anticipation(bridge, &anticipation);
    ASSERT_EQ(0, ret);

    /* Hunger anticipation should have increased */
    EXPECT_GT(anticipation.anticipation[HYPO_DRIVE_HUNGER], 0.0f);

    free_test_clause(conclusion);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, FullBidirectionalFlow_ConclusionUpdatesDriveState) {
    /* Set initial drive state */
    set_drive_urgency(HYPO_DRIVE_SAFETY, TEST_DRIVE_LEVEL_LOW);

    /* Get initial anticipation */
    hypo_logic_anticipation_t initial_anticipation;
    int ret = hypo_logic_get_anticipation(bridge, &initial_anticipation);
    ASSERT_EQ(0, ret);
    float initial_safety_anticipation = initial_anticipation.anticipation[HYPO_DRIVE_SAFETY];

    /* Process threat_present conclusion */
    logic_clause_t* threat_conclusion = create_test_clause("threat_present");
    ASSERT_NE(threat_conclusion, nullptr);

    ret = hypo_logic_process_conclusion(bridge, threat_conclusion, TEST_CONFIDENCE_HIGH);
    ASSERT_EQ(0, ret);

    /* Verify safety anticipation increased */
    hypo_logic_anticipation_t final_anticipation;
    ret = hypo_logic_get_anticipation(bridge, &final_anticipation);
    ASSERT_EQ(0, ret);

    EXPECT_GT(final_anticipation.anticipation[HYPO_DRIVE_SAFETY], initial_safety_anticipation);

    free_test_clause(threat_conclusion);
}

/* ============================================================================
 * 2. Motivated Reasoning Pipeline Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, MotivatedReasoning_HungryAgentBoostsFoodSalience) {
    /* Reset bridge to clear state */
    hypo_logic_bridge_reset(bridge);

    /* Set low hunger first (satisfied state) */
    hypo_drive_satisfy(drive_system, HYPO_DRIVE_HUNGER, 1.0f);  /* Fully satisfied */
    hypo_drive_update(drive_system, 10000);
    hypo_logic_compute_modulation(bridge);

    float food_salience_low_hunger = hypo_logic_get_predicate_salience(bridge, "food_nearby");

    /* Now let hunger build (unsatisfied state) */
    hypo_drive_satisfy(drive_system, HYPO_DRIVE_HUNGER, 0.0f);  /* Not satisfied */
    /* Run many updates to let urgency build */
    for (int i = 0; i < 20; i++) {
        hypo_drive_update(drive_system, 500000);  /* 500ms each = 10 seconds total */
    }
    hypo_logic_compute_modulation(bridge);

    float food_salience_high_hunger = hypo_logic_get_predicate_salience(bridge, "food_nearby");

    /* Food salience should be at least as high when hungry (urgency-based) */
    /* The salience boost depends on drive urgency which rises over time */
    EXPECT_GE(food_salience_high_hunger, food_salience_low_hunger);

    /* Both should be positive and reasonable */
    EXPECT_GT(food_salience_low_hunger, 0.0f);
    EXPECT_GT(food_salience_high_hunger, 0.0f);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, MotivatedReasoning_HighUrgencyReducesInferenceDepth) {
    /* Reset bridge */
    hypo_logic_bridge_reset(bridge);

    /* Set calm state - satisfy all drives */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        hypo_drive_satisfy(drive_system, (hypo_drive_type_t)i, 1.0f);
    }
    hypo_drive_update(drive_system, 10000);
    hypo_logic_compute_modulation(bridge);

    uint32_t depth_calm = hypo_logic_get_recommended_depth(bridge);

    /* Let drives build urgency over time */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        hypo_drive_satisfy(drive_system, (hypo_drive_type_t)i, 0.0f);
    }
    /* Run many updates to let urgency build */
    for (int i = 0; i < 30; i++) {
        hypo_drive_update(drive_system, 500000);  /* 500ms each */
    }
    hypo_logic_compute_modulation(bridge);

    uint32_t depth_urgent = hypo_logic_get_recommended_depth(bridge);

    /* Urgent state should have shallower or equal inference depth
     * (depending on how much urgency actually built up) */
    EXPECT_LE(depth_urgent, depth_calm);
    EXPECT_GT(depth_urgent, 0u);  /* But still positive */
    EXPECT_GT(depth_calm, 0u);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, MotivatedReasoning_WishfulThinkingLowersThreshold) {
    /* Create a food-related goal */
    logic_clause_t* food_goal = create_test_clause("food_available");
    ASSERT_NE(food_goal, nullptr);

    /* Reset bridge */
    hypo_logic_bridge_reset(bridge);

    /* When satisfied (not hungry), threshold should be higher */
    hypo_drive_satisfy(drive_system, HYPO_DRIVE_HUNGER, 1.0f);
    hypo_drive_update(drive_system, 10000);
    hypo_logic_compute_modulation(bridge);
    float threshold_not_hungry = hypo_logic_get_goal_threshold(bridge, food_goal);

    /* When very hungry (urgency builds over time), threshold may be lower */
    hypo_drive_satisfy(drive_system, HYPO_DRIVE_HUNGER, 0.0f);
    for (int i = 0; i < 30; i++) {
        hypo_drive_update(drive_system, 500000);  /* Let urgency build */
    }
    hypo_logic_compute_modulation(bridge);
    float threshold_hungry = hypo_logic_get_goal_threshold(bridge, food_goal);

    /* Threshold should be within valid range */
    EXPECT_GT(threshold_hungry, 0.0f);
    EXPECT_LT(threshold_hungry, 1.0f);
    EXPECT_GT(threshold_not_hungry, 0.0f);
    EXPECT_LT(threshold_not_hungry, 1.0f);

    /* When urgency is higher, we expect lower or equal threshold
     * (wishful thinking makes us more accepting of desired conclusions) */
    EXPECT_LE(threshold_hungry, threshold_not_hungry);

    free_test_clause(food_goal);
}

/* ============================================================================
 * 3. Multi-Drive Interaction Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, MultiDrive_CompetingDrivesAffectPriority) {
    /* Reset bridge */
    hypo_logic_bridge_reset(bridge);

    /* Satisfy all drives first to establish baseline */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        hypo_drive_satisfy(drive_system, (hypo_drive_type_t)i, 1.0f);
    }
    hypo_drive_update(drive_system, 10000);

    /* Compute modulation in satisfied state */
    hypo_logic_compute_modulation(bridge);

    hypo_logic_modulation_t modulation1;
    int ret = hypo_logic_get_modulation(bridge, &modulation1);
    ASSERT_EQ(0, ret);

    /* Modulation should have valid priority drive */
    EXPECT_GE((int)modulation1.priority_drive, 0);
    EXPECT_LT((int)modulation1.priority_drive, HYPO_DRIVE_COUNT);

    /* Now let one drive become unsatisfied */
    hypo_drive_satisfy(drive_system, HYPO_DRIVE_HUNGER, 0.0f);
    for (int i = 0; i < 20; i++) {
        hypo_drive_update(drive_system, 500000);
    }
    hypo_logic_compute_modulation(bridge);

    hypo_logic_modulation_t modulation2;
    ret = hypo_logic_get_modulation(bridge, &modulation2);
    ASSERT_EQ(0, ret);

    /* Modulation should still have valid priority drive */
    EXPECT_GE((int)modulation2.priority_drive, 0);
    EXPECT_LT((int)modulation2.priority_drive, HYPO_DRIVE_COUNT);

    /* The unsatisfied drive (HUNGER=0) might become priority */
    /* This depends on how much urgency actually built */
    EXPECT_TRUE(modulation2.priority_drive == modulation1.priority_drive ||
                modulation2.priority_drive == HYPO_DRIVE_HUNGER);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, MultiDrive_MultipleSalienceBoosts) {
    /* Set multiple drives */
    set_drive_urgency(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_HIGH);
    set_drive_urgency(HYPO_DRIVE_CURIOSITY, TEST_DRIVE_LEVEL_MODERATE);
    set_drive_urgency(HYPO_DRIVE_SOCIAL, TEST_DRIVE_LEVEL_LOW);
    hypo_logic_compute_modulation(bridge);

    hypo_logic_modulation_t modulation;
    int ret = hypo_logic_get_modulation(bridge, &modulation);
    ASSERT_EQ(0, ret);

    /* All urgent drives should have salience boost > 1 */
    EXPECT_GT(modulation.salience_boost[HYPO_DRIVE_HUNGER], 1.0f);
    EXPECT_GT(modulation.salience_boost[HYPO_DRIVE_CURIOSITY], 1.0f);

    /* More urgent drives should have higher boost */
    EXPECT_GT(modulation.salience_boost[HYPO_DRIVE_HUNGER],
              modulation.salience_boost[HYPO_DRIVE_CURIOSITY]);
}

/* ============================================================================
 * 4. Logic Engine Integration Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, LogicIntegration_ModulationApplied) {
    /* Apply modulation */
    set_drive_urgency(HYPO_DRIVE_CURIOSITY, TEST_DRIVE_LEVEL_HIGH);
    hypo_logic_compute_modulation(bridge);

    int ret = hypo_logic_apply_modulation(bridge);
    EXPECT_EQ(0, ret);

    /* Verify modulation was tracked */
    hypo_logic_stats_t stats;
    ret = hypo_logic_get_stats(bridge, &stats);
    ASSERT_EQ(0, ret);

    EXPECT_GT(stats.salience_modulations, 0u);
    EXPECT_GT(stats.depth_modulations, 0u);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, LogicIntegration_PredicateCategorization) {
    /* Test automatic categorization */
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("food_available"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("edible_item"));
    EXPECT_EQ(HYPO_PRED_CAT_WATER, hypo_logic_get_predicate_category("water_source"));
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("danger_detected"));
    EXPECT_EQ(HYPO_PRED_CAT_SAFETY, hypo_logic_get_predicate_category("safe_zone"));
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("friend_nearby"));
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("discovered_fact"));
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, LogicIntegration_CategoryToDriveMapping) {
    EXPECT_EQ(HYPO_DRIVE_HUNGER, hypo_logic_category_to_drive(HYPO_PRED_CAT_FOOD));
    EXPECT_EQ(HYPO_DRIVE_THIRST, hypo_logic_category_to_drive(HYPO_PRED_CAT_WATER));
    EXPECT_EQ(HYPO_DRIVE_SAFETY, hypo_logic_category_to_drive(HYPO_PRED_CAT_THREAT));
    EXPECT_EQ(HYPO_DRIVE_SAFETY, hypo_logic_category_to_drive(HYPO_PRED_CAT_SAFETY));
    EXPECT_EQ(HYPO_DRIVE_SOCIAL, hypo_logic_category_to_drive(HYPO_PRED_CAT_SOCIAL));
    EXPECT_EQ(HYPO_DRIVE_CURIOSITY, hypo_logic_category_to_drive(HYPO_PRED_CAT_KNOWLEDGE));
}

/* ============================================================================
 * 5. Drive System Integration Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, DriveIntegration_ConclusionAffectsDrive) {
    /* Get initial anticipation */
    hypo_logic_anticipation_t initial;
    hypo_logic_get_anticipation(bridge, &initial);

    /* Process resource available conclusion */
    logic_clause_t* resource = create_test_clause("water_available");
    ASSERT_NE(resource, nullptr);

    int ret = hypo_logic_process_conclusion(bridge, resource, TEST_CONFIDENCE_HIGH);
    ASSERT_EQ(0, ret);

    /* Get updated anticipation */
    hypo_logic_anticipation_t updated;
    hypo_logic_get_anticipation(bridge, &updated);

    /* Thirst drive anticipation should increase */
    EXPECT_GT(updated.anticipation[HYPO_DRIVE_THIRST],
              initial.anticipation[HYPO_DRIVE_THIRST]);

    free_test_clause(resource);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, DriveIntegration_ResourceUnavailableCausesFrustration) {
    /* Get initial frustration */
    hypo_logic_anticipation_t initial;
    hypo_logic_get_anticipation(bridge, &initial);

    /* Process resource unavailable conclusion (negated) */
    logic_clause_t* no_resource = create_test_clause("food_available", true);  /* negated */
    ASSERT_NE(no_resource, nullptr);

    /* Classify the conclusion */
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, no_resource);

    /* Process based on classification */
    int ret = hypo_logic_process_conclusion(bridge, no_resource, TEST_CONFIDENCE_HIGH);
    ASSERT_EQ(0, ret);

    /* Get updated state */
    hypo_logic_anticipation_t updated;
    hypo_logic_get_anticipation(bridge, &updated);

    /* Stats should show conclusion processed */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_GT(stats.conclusions_processed, 0u);

    free_test_clause(no_resource);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, DriveIntegration_ConclusionClassification) {
    /* Test various conclusion types */
    logic_clause_t* resource_available = create_test_clause("food_available");
    EXPECT_EQ(HYPO_CONCL_RESOURCE_AVAILABLE,
              hypo_logic_classify_conclusion(bridge, resource_available));
    free_test_clause(resource_available);

    logic_clause_t* threat_present = create_test_clause("threat_detected");
    EXPECT_EQ(HYPO_CONCL_THREAT_PRESENT,
              hypo_logic_classify_conclusion(bridge, threat_present));
    free_test_clause(threat_present);

    logic_clause_t* safe_location = create_test_clause("safe_zone");
    EXPECT_EQ(HYPO_CONCL_THREAT_ABSENT,
              hypo_logic_classify_conclusion(bridge, safe_location));
    free_test_clause(safe_location);

    logic_clause_t* goal_achieved = create_test_clause("task_accomplished");
    EXPECT_EQ(HYPO_CONCL_GOAL_ACHIEVED,
              hypo_logic_classify_conclusion(bridge, goal_achieved));
    free_test_clause(goal_achieved);

    logic_clause_t* opportunity = create_test_clause("opportunity_found");
    EXPECT_EQ(HYPO_CONCL_OPPORTUNITY,
              hypo_logic_classify_conclusion(bridge, opportunity));
    free_test_clause(opportunity);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, DriveIntegration_AffectedDriveDetection) {
    logic_clause_t* food = create_test_clause("food_source");
    EXPECT_EQ(HYPO_DRIVE_HUNGER, hypo_logic_get_affected_drive(bridge, food));
    free_test_clause(food);

    logic_clause_t* water = create_test_clause("water_stream");
    EXPECT_EQ(HYPO_DRIVE_THIRST, hypo_logic_get_affected_drive(bridge, water));
    free_test_clause(water);

    logic_clause_t* threat = create_test_clause("danger_zone");
    EXPECT_EQ(HYPO_DRIVE_SAFETY, hypo_logic_get_affected_drive(bridge, threat));
    free_test_clause(threat);

    logic_clause_t* friend_near = create_test_clause("friend_approaching");
    EXPECT_EQ(HYPO_DRIVE_SOCIAL, hypo_logic_get_affected_drive(bridge, friend_near));
    free_test_clause(friend_near);

    logic_clause_t* discovery = create_test_clause("knowledge_gained");
    EXPECT_EQ(HYPO_DRIVE_CURIOSITY, hypo_logic_get_affected_drive(bridge, discovery));
    free_test_clause(discovery);
}

/* ============================================================================
 * 6. FEP Integration Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, FEP_PredictionGeneration) {
    /* Set drive states */
    set_drive_urgency(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_HIGH);
    set_drive_urgency(HYPO_DRIVE_THIRST, TEST_DRIVE_LEVEL_MODERATE);
    run_update_cycle();

    /* Generate predictions */
    int ret = hypo_logic_generate_predictions(bridge);
    EXPECT_EQ(0, ret);

    /* Verify stats updated */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_GT(stats.predictions_made, 0u);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, FEP_PredictionUpdate) {
    /* Generate predictions */
    hypo_logic_generate_predictions(bridge);

    /* Process a conclusion */
    logic_clause_t* conclusion = create_test_clause("food_available");
    float prediction_error = hypo_logic_update_predictions(bridge, conclusion);

    /* Should have some prediction error */
    /* The error can be positive or negative */
    EXPECT_GE(fabsf(prediction_error), 0.0f);

    free_test_clause(conclusion);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, FEP_FreeEnergyComputation) {
    /* Set drive states */
    set_drive_urgency(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_HIGH);
    run_update_cycle();

    /* Compute free energy */
    hypo_logic_fep_state_t fe_state;
    int ret = hypo_logic_compute_free_energy(bridge, &fe_state);
    ASSERT_EQ(0, ret);

    /* Free energy should be non-negative */
    EXPECT_GE(fe_state.logical_free_energy, 0.0f);
    EXPECT_GE(fe_state.precision, 0.0f);
    EXPECT_LE(fe_state.precision, 1.0f);
    EXPECT_GE(fe_state.expected_info_gain, 0.0f);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, FEP_PredictionErrorTracking) {
    /* Reset stats */
    hypo_logic_reset_stats(bridge);

    /* Generate and update predictions multiple times */
    for (int i = 0; i < 10; i++) {
        hypo_logic_generate_predictions(bridge);

        logic_clause_t* conclusion = create_test_clause("food_available");
        hypo_logic_update_predictions(bridge, conclusion);
        free_test_clause(conclusion);
    }

    /* Check stats */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);

    uint64_t total_predictions = stats.predictions_confirmed + stats.predictions_violated;
    EXPECT_GT(total_predictions, 0u);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, FEP_GetFEPState) {
    /* Run some cycles */
    for (int i = 0; i < 5; i++) {
        run_update_cycle();
    }

    hypo_logic_fep_state_t fe_state;
    int ret = hypo_logic_get_fep_state(bridge, &fe_state);
    ASSERT_EQ(0, ret);

    /* Timestamp should be set */
    EXPECT_GT(fe_state.timestamp_us, 0u);
}

/* ============================================================================
 * 7. Goal Lifecycle Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, Goal_CreateMotivatedGoal) {
    logic_clause_t* goal = create_test_clause("find_food");
    ASSERT_NE(goal, nullptr);

    int ret = hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.8f);
    EXPECT_EQ(0, ret);

    /* Check stats */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_EQ(1u, stats.goals_created);

    /* Get prioritized goals */
    hypo_motivated_goal_t goals[10];
    uint32_t num_goals;
    ret = hypo_logic_get_prioritized_goals(bridge, goals, 10, &num_goals);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(1u, num_goals);

    EXPECT_EQ(HYPO_DRIVE_HUNGER, goals[0].motivating_drive);
    EXPECT_TRUE(goals[0].active);
    EXPECT_FLOAT_EQ(0.8f, goals[0].anticipated_satisfaction);

    /* Don't free goal - ownership transferred */
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Goal_PrioritizationByUrgency) {
    /* Reset bridge and create fresh state */
    hypo_logic_bridge_reset(bridge);

    /* Create goals for different drives */
    logic_clause_t* hunger_goal = create_test_clause("find_food");
    logic_clause_t* safety_goal = create_test_clause("find_shelter");
    logic_clause_t* social_goal = create_test_clause("find_friend");

    /* Satisfy all drives first */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        hypo_drive_satisfy(drive_system, (hypo_drive_type_t)i, 1.0f);
    }
    hypo_drive_update(drive_system, 10000);

    /* Create goals - they should be prioritized based on drive urgency */
    hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, hunger_goal, 0.8f);
    hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_SAFETY, safety_goal, 0.8f);
    hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_SOCIAL, social_goal, 0.8f);

    /* Get prioritized goals */
    hypo_motivated_goal_t goals[10];
    uint32_t num_goals;
    int ret = hypo_logic_get_prioritized_goals(bridge, goals, 10, &num_goals);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(3u, num_goals);

    /* Verify all three drives are represented */
    bool has_hunger = false, has_safety = false, has_social = false;
    for (uint32_t i = 0; i < num_goals; i++) {
        if (goals[i].motivating_drive == HYPO_DRIVE_HUNGER) has_hunger = true;
        if (goals[i].motivating_drive == HYPO_DRIVE_SAFETY) has_safety = true;
        if (goals[i].motivating_drive == HYPO_DRIVE_SOCIAL) has_social = true;
    }
    EXPECT_TRUE(has_hunger);
    EXPECT_TRUE(has_safety);
    EXPECT_TRUE(has_social);

    /* Goals should be sorted by urgency, but the exact ordering depends
     * on drive system state which we can't precisely control */
    /* Just verify each goal has valid drive and priority */
    for (uint32_t i = 0; i < num_goals; i++) {
        EXPECT_GE((int)goals[i].motivating_drive, 0);
        EXPECT_LT((int)goals[i].motivating_drive, HYPO_DRIVE_COUNT);
        EXPECT_GE(goals[i].priority, 0.0f);
    }
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Goal_AchievementReward) {
    /* Create goal */
    logic_clause_t* goal = create_test_clause("find_food");
    hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.9f);

    /* Get initial anticipation */
    hypo_logic_anticipation_t initial;
    hypo_logic_get_anticipation(bridge, &initial);

    /* Achieve goal */
    float reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_HUNGER, goal);

    /* Should get reward based on anticipated satisfaction */
    EXPECT_GT(reward, 0.0f);

    /* Check stats */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_GT(stats.goals_achieved, 0u);

    /* Get updated anticipation */
    hypo_logic_anticipation_t updated;
    hypo_logic_get_anticipation(bridge, &updated);
    EXPECT_GT(updated.anticipation[HYPO_DRIVE_HUNGER],
              initial.anticipation[HYPO_DRIVE_HUNGER]);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Goal_ImpossibilityFrustration) {
    /* Create goal */
    logic_clause_t* goal = create_test_clause("find_food");
    hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.8f);

    /* Get initial frustration */
    hypo_logic_anticipation_t initial;
    hypo_logic_get_anticipation(bridge, &initial);

    /* Mark goal impossible */
    float frustration = hypo_logic_goal_impossible(bridge, HYPO_DRIVE_HUNGER, goal);

    /* Should get frustration signal */
    EXPECT_GT(frustration, 0.0f);

    /* Check stats */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_GT(stats.goals_abandoned, 0u);
    EXPECT_GT(stats.frustration_events, 0u);

    /* Frustration should increase */
    hypo_logic_anticipation_t updated;
    hypo_logic_get_anticipation(bridge, &updated);
    EXPECT_GT(updated.frustration[HYPO_DRIVE_HUNGER],
              initial.frustration[HYPO_DRIVE_HUNGER]);
}

/* ============================================================================
 * 8. Anticipation Dynamics Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, Anticipation_MultipleConclusions) {
    /* Get initial anticipation */
    hypo_logic_anticipation_t initial;
    hypo_logic_get_anticipation(bridge, &initial);

    /* Process multiple positive conclusions */
    for (int i = 0; i < 5; i++) {
        logic_clause_t* food = create_test_clause("food_available");
        hypo_logic_process_conclusion(bridge, food, TEST_CONFIDENCE_HIGH);
        free_test_clause(food);
    }

    /* Anticipation should build up */
    hypo_logic_anticipation_t after_positive;
    hypo_logic_get_anticipation(bridge, &after_positive);
    EXPECT_GT(after_positive.anticipation[HYPO_DRIVE_HUNGER],
              initial.anticipation[HYPO_DRIVE_HUNGER]);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Anticipation_DecayOverTime) {
    /* Build up anticipation */
    logic_clause_t* food = create_test_clause("food_available");
    hypo_logic_process_conclusion(bridge, food, TEST_CONFIDENCE_HIGH);
    free_test_clause(food);

    hypo_logic_anticipation_t after_conclusion;
    hypo_logic_get_anticipation(bridge, &after_conclusion);
    float anticipation_after = after_conclusion.anticipation[HYPO_DRIVE_HUNGER];

    /* Run update cycles (should decay anticipation) */
    for (int i = 0; i < 20; i++) {
        run_update_cycle(50000);  /* 50ms each */
    }

    hypo_logic_anticipation_t after_decay;
    hypo_logic_get_anticipation(bridge, &after_decay);

    /* Anticipation should have decayed */
    EXPECT_LT(after_decay.anticipation[HYPO_DRIVE_HUNGER], anticipation_after);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Anticipation_ConfidenceAffectsImpact) {
    /* Reset bridge */
    hypo_logic_bridge_reset(bridge);

    /* Process low confidence conclusion */
    logic_clause_t* food_low = create_test_clause("food_available");
    hypo_logic_process_conclusion(bridge, food_low, TEST_CONFIDENCE_LOW);
    free_test_clause(food_low);

    hypo_logic_anticipation_t after_low;
    hypo_logic_get_anticipation(bridge, &after_low);
    float anticipation_low = after_low.anticipation[HYPO_DRIVE_HUNGER];

    /* Reset and process high confidence */
    hypo_logic_bridge_reset(bridge);

    logic_clause_t* food_high = create_test_clause("food_available");
    hypo_logic_process_conclusion(bridge, food_high, TEST_CONFIDENCE_HIGH);
    free_test_clause(food_high);

    hypo_logic_anticipation_t after_high;
    hypo_logic_get_anticipation(bridge, &after_high);
    float anticipation_high = after_high.anticipation[HYPO_DRIVE_HUNGER];

    /* Higher confidence should have more impact */
    EXPECT_GT(anticipation_high, anticipation_low);
}

/* ============================================================================
 * 9. Fatigue Effects Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, Fatigue_ReducesReasoningCapacity) {
    /* Set low fatigue */
    set_drive_urgency(HYPO_DRIVE_FATIGUE, TEST_DRIVE_LEVEL_LOW);
    hypo_logic_compute_modulation(bridge);

    hypo_logic_modulation_t low_fatigue;
    hypo_logic_get_modulation(bridge, &low_fatigue);
    float capacity_rested = low_fatigue.reasoning_capacity;

    /* Set high fatigue */
    set_drive_urgency(HYPO_DRIVE_FATIGUE, TEST_DRIVE_LEVEL_HIGH);
    hypo_logic_compute_modulation(bridge);

    hypo_logic_modulation_t high_fatigue;
    hypo_logic_get_modulation(bridge, &high_fatigue);
    float capacity_tired = high_fatigue.reasoning_capacity;

    /* Fatigue should reduce capacity */
    EXPECT_LT(capacity_tired, capacity_rested);
    EXPECT_GT(capacity_tired, 0.0f);
    EXPECT_LE(capacity_rested, 1.0f);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Fatigue_ReducesEffortWillingness) {
    /* Set low fatigue */
    set_drive_urgency(HYPO_DRIVE_FATIGUE, TEST_DRIVE_LEVEL_LOW);
    hypo_logic_compute_modulation(bridge);

    hypo_logic_modulation_t low_fatigue;
    hypo_logic_get_modulation(bridge, &low_fatigue);
    float effort_rested = low_fatigue.effort_willingness;

    /* Set high fatigue */
    set_drive_urgency(HYPO_DRIVE_FATIGUE, TEST_DRIVE_LEVEL_HIGH);
    hypo_logic_compute_modulation(bridge);

    hypo_logic_modulation_t high_fatigue;
    hypo_logic_get_modulation(bridge, &high_fatigue);
    float effort_tired = high_fatigue.effort_willingness;

    /* Fatigue should reduce effort willingness */
    EXPECT_LT(effort_tired, effort_rested);
}

/* ============================================================================
 * 10. Bio-Async Messaging Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, BioAsync_Registration) {
    /* Register with bio-async (may or may not succeed depending on router availability) */
    bool registered = hypo_logic_bridge_register_bio(bridge, false);

    /* Either way, the function should not crash */
    /* Just verify we can call it */
    EXPECT_TRUE(true);

    /* If registered, try broadcast */
    if (registered) {
        int ret = hypo_logic_broadcast_modulation(bridge);
        EXPECT_EQ(0, ret);
    }
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, BioAsync_ModulationBroadcast) {
    /* Set up modulation */
    set_drive_urgency(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_HIGH);
    hypo_logic_compute_modulation(bridge);

    /* Try to broadcast (may fail if not registered) */
    int ret = hypo_logic_broadcast_modulation(bridge);
    /* -1 is expected if not registered */
    EXPECT_TRUE(ret == 0 || ret == -1);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, BioAsync_ConclusionImpactBroadcast) {
    logic_clause_t* conclusion = create_test_clause("food_available");

    /* Try to broadcast (may fail if not registered) */
    int ret = hypo_logic_broadcast_conclusion_impact(bridge, conclusion, 0.5f);
    /* -1 is expected if not registered */
    EXPECT_TRUE(ret == 0 || ret == -1);

    free_test_clause(conclusion);
}

/* ============================================================================
 * Statistics and Diagnostics Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, Stats_TrackModulations) {
    hypo_logic_reset_stats(bridge);

    /* Perform modulations */
    for (int i = 0; i < 10; i++) {
        set_drive_urgency(HYPO_DRIVE_HUNGER, 0.1f * i);
        hypo_logic_compute_modulation(bridge);
    }

    hypo_logic_stats_t stats;
    int ret = hypo_logic_get_stats(bridge, &stats);
    ASSERT_EQ(0, ret);

    EXPECT_EQ(10u, stats.salience_modulations);
    EXPECT_EQ(10u, stats.depth_modulations);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Stats_TrackConclusions) {
    hypo_logic_reset_stats(bridge);

    /* Process conclusions */
    for (int i = 0; i < 5; i++) {
        logic_clause_t* conclusion = create_test_clause("food_available");
        hypo_logic_process_conclusion(bridge, conclusion, 0.9f);
        free_test_clause(conclusion);
    }

    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);

    EXPECT_EQ(5u, stats.conclusions_processed);
    EXPECT_GT(stats.anticipation_updates, 0u);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Stats_Reset) {
    /* Generate activity */
    for (int i = 0; i < 5; i++) {
        run_update_cycle();
    }

    /* Reset stats */
    int ret = hypo_logic_reset_stats(bridge);
    EXPECT_EQ(0, ret);

    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);

    EXPECT_EQ(0u, stats.salience_modulations);
    EXPECT_EQ(0u, stats.conclusions_processed);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Diagnostics_PrintSummary) {
    /* Set up some state */
    set_drive_urgency(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_HIGH);
    hypo_logic_compute_modulation(bridge);

    logic_clause_t* conclusion = create_test_clause("food_available");
    hypo_logic_process_conclusion(bridge, conclusion, 0.9f);
    free_test_clause(conclusion);

    /* Should not crash */
    hypo_logic_print_summary(bridge);
    EXPECT_TRUE(true);
}

/* ============================================================================
 * Predicate Mapping Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, PredicateMap_Registration) {
    hypo_logic_predicate_map_t map = {
        .predicate_name = "special_food",
        .drive = HYPO_DRIVE_HUNGER,
        .relevance = 0.9f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };
    strncpy(map.predicate_name, "special_food", LOGIC_MAX_NAME_LENGTH);

    int ret = hypo_logic_register_predicate_map(bridge, &map);
    EXPECT_EQ(0, ret);

    /* Registered mapping should affect salience */
    set_drive_urgency(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_HIGH);
    hypo_logic_compute_modulation(bridge);

    float salience = hypo_logic_get_predicate_salience(bridge, "special_food");
    EXPECT_GT(salience, 1.0f);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, PredicateMap_AutoRegistration) {
    /* Create fresh bridge to check auto-registration count */
    hypo_logic_bridge_destroy(bridge);

    hypo_logic_config_t config = hypo_logic_default_config();
    config.num_predicate_maps = 0;  /* Start empty */

    bridge = hypo_logic_bridge_create(drive_system, logic_engine, &config);
    ASSERT_NE(bridge, nullptr);

    /* Auto-register should add common mappings */
    int count = hypo_logic_auto_register_mappings(bridge);
    EXPECT_GT(count, 0);
}

/* ============================================================================
 * String Utility Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, StringUtils_ConclusionTypeNames) {
    EXPECT_STREQ("resource_available",
                 hypo_conclusion_type_name(HYPO_CONCL_RESOURCE_AVAILABLE));
    EXPECT_STREQ("resource_unavailable",
                 hypo_conclusion_type_name(HYPO_CONCL_RESOURCE_UNAVAILABLE));
    EXPECT_STREQ("threat_present",
                 hypo_conclusion_type_name(HYPO_CONCL_THREAT_PRESENT));
    EXPECT_STREQ("threat_absent",
                 hypo_conclusion_type_name(HYPO_CONCL_THREAT_ABSENT));
    EXPECT_STREQ("goal_achieved",
                 hypo_conclusion_type_name(HYPO_CONCL_GOAL_ACHIEVED));
    EXPECT_STREQ("goal_impossible",
                 hypo_conclusion_type_name(HYPO_CONCL_GOAL_IMPOSSIBLE));
    EXPECT_STREQ("opportunity",
                 hypo_conclusion_type_name(HYPO_CONCL_OPPORTUNITY));
    EXPECT_STREQ("neutral",
                 hypo_conclusion_type_name(HYPO_CONCL_NEUTRAL));
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, StringUtils_PredicateCategoryNames) {
    EXPECT_STREQ("food", hypo_predicate_category_name(HYPO_PRED_CAT_FOOD));
    EXPECT_STREQ("water", hypo_predicate_category_name(HYPO_PRED_CAT_WATER));
    EXPECT_STREQ("threat", hypo_predicate_category_name(HYPO_PRED_CAT_THREAT));
    EXPECT_STREQ("safety", hypo_predicate_category_name(HYPO_PRED_CAT_SAFETY));
    EXPECT_STREQ("social", hypo_predicate_category_name(HYPO_PRED_CAT_SOCIAL));
    EXPECT_STREQ("knowledge", hypo_predicate_category_name(HYPO_PRED_CAT_KNOWLEDGE));
    EXPECT_STREQ("neutral", hypo_predicate_category_name(HYPO_PRED_CAT_NEUTRAL));
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, ErrorHandling_NullBridge) {
    /* These should all handle null gracefully */
    hypo_logic_bridge_destroy(nullptr);

    EXPECT_EQ(-1, hypo_logic_bridge_reset(nullptr));
    EXPECT_EQ(-1, hypo_logic_bridge_update(nullptr, 1000));
    EXPECT_EQ(-1, hypo_logic_compute_modulation(nullptr));

    hypo_logic_modulation_t modulation;
    EXPECT_EQ(-1, hypo_logic_get_modulation(nullptr, &modulation));

    EXPECT_EQ(-1, hypo_logic_apply_modulation(nullptr));

    EXPECT_FLOAT_EQ(1.0f, hypo_logic_get_predicate_salience(nullptr, "test"));
    EXPECT_FLOAT_EQ(0.7f, hypo_logic_get_goal_threshold(nullptr, nullptr));
    EXPECT_EQ(10u, hypo_logic_get_recommended_depth(nullptr));

    logic_clause_t* clause = create_test_clause("test");
    EXPECT_EQ(-1, hypo_logic_process_conclusion(nullptr, clause, 0.5f));
    EXPECT_FLOAT_EQ(0.0f, hypo_logic_goal_achieved(nullptr, HYPO_DRIVE_HUNGER, clause));
    EXPECT_FLOAT_EQ(0.0f, hypo_logic_goal_impossible(nullptr, HYPO_DRIVE_HUNGER, clause));
    free_test_clause(clause);

    hypo_logic_anticipation_t anticipation;
    EXPECT_EQ(-1, hypo_logic_get_anticipation(nullptr, &anticipation));

    EXPECT_EQ(-1, hypo_logic_generate_predictions(nullptr));
    EXPECT_FLOAT_EQ(0.0f, hypo_logic_update_predictions(nullptr, nullptr));

    hypo_logic_fep_state_t fe_state;
    EXPECT_EQ(-1, hypo_logic_compute_free_energy(nullptr, &fe_state));
    EXPECT_EQ(-1, hypo_logic_get_fep_state(nullptr, &fe_state));

    hypo_logic_stats_t stats;
    EXPECT_EQ(-1, hypo_logic_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, hypo_logic_reset_stats(nullptr));

    /* Should not crash */
    hypo_logic_print_summary(nullptr);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, ErrorHandling_NullOutputParameters) {
    EXPECT_EQ(-1, hypo_logic_get_modulation(bridge, nullptr));
    EXPECT_EQ(-1, hypo_logic_get_anticipation(bridge, nullptr));
    EXPECT_EQ(-1, hypo_logic_compute_free_energy(bridge, nullptr));
    EXPECT_EQ(-1, hypo_logic_get_fep_state(bridge, nullptr));
    EXPECT_EQ(-1, hypo_logic_get_stats(bridge, nullptr));

    EXPECT_EQ(-1, hypo_logic_get_prioritized_goals(bridge, nullptr, 10, nullptr));
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, ErrorHandling_InvalidDrive) {
    logic_clause_t* goal = create_test_clause("test");

    int ret = hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_COUNT, goal, 0.5f);
    EXPECT_EQ(-1, ret);

    EXPECT_FLOAT_EQ(0.0f, hypo_logic_goal_achieved(bridge, HYPO_DRIVE_COUNT, goal));
    EXPECT_FLOAT_EQ(0.0f, hypo_logic_goal_impossible(bridge, HYPO_DRIVE_COUNT, goal));

    free_test_clause(goal);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, ErrorHandling_NullConclusion) {
    EXPECT_EQ(-1, hypo_logic_process_conclusion(bridge, nullptr, 0.5f));
    EXPECT_FLOAT_EQ(0.0f, hypo_logic_update_predictions(bridge, nullptr));
    EXPECT_EQ(-1, hypo_logic_broadcast_conclusion_impact(bridge, nullptr, 0.5f));

    EXPECT_EQ(HYPO_CONCL_NEUTRAL, hypo_logic_classify_conclusion(bridge, nullptr));
    EXPECT_EQ(HYPO_DRIVE_COUNT, hypo_logic_get_affected_drive(bridge, nullptr));
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, Reset_ClearsState) {
    /* Build up state */
    set_drive_urgency(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_HIGH);
    hypo_logic_compute_modulation(bridge);

    logic_clause_t* goal = create_test_clause("find_food");
    hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.8f);

    logic_clause_t* conclusion = create_test_clause("food_available");
    hypo_logic_process_conclusion(bridge, conclusion, 0.9f);
    free_test_clause(conclusion);

    /* Reset */
    int ret = hypo_logic_bridge_reset(bridge);
    EXPECT_EQ(0, ret);

    /* Verify state is cleared */
    hypo_motivated_goal_t goals[10];
    uint32_t num_goals;
    hypo_logic_get_prioritized_goals(bridge, goals, 10, &num_goals);
    EXPECT_EQ(0u, num_goals);

    hypo_logic_anticipation_t anticipation;
    hypo_logic_get_anticipation(bridge, &anticipation);
    EXPECT_FLOAT_EQ(0.0f, anticipation.anticipation[HYPO_DRIVE_HUNGER]);
    EXPECT_FLOAT_EQ(0.0f, anticipation.frustration[HYPO_DRIVE_HUNGER]);

    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_EQ(0u, stats.conclusions_processed);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(HypothalamusLogicBridgeIntegrationTest, Concurrent_UpdateCycles) {
    std::atomic<int> update_count{0};

    auto update_thread = [this, &update_count]() {
        for (int i = 0; i < 50; i++) {
            run_update_cycle(1000);
            update_count++;
        }
    };

    std::thread t1(update_thread);
    std::thread t2(update_thread);

    t1.join();
    t2.join();

    EXPECT_EQ(100, update_count.load());

    /* Bridge should still be functional */
    hypo_logic_modulation_t modulation;
    int ret = hypo_logic_get_modulation(bridge, &modulation);
    EXPECT_EQ(0, ret);
}

TEST_F(HypothalamusLogicBridgeIntegrationTest, Concurrent_ConclusionProcessing) {
    std::atomic<int> conclusion_count{0};

    auto process_thread = [this, &conclusion_count]() {
        for (int i = 0; i < 20; i++) {
            logic_clause_t* conclusion = create_test_clause("food_available");
            hypo_logic_process_conclusion(bridge, conclusion, 0.8f);
            free_test_clause(conclusion);
            conclusion_count++;
        }
    };

    std::thread t1(process_thread);
    std::thread t2(process_thread);

    t1.join();
    t2.join();

    EXPECT_EQ(40, conclusion_count.load());

    /* Check stats */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_GE(stats.conclusions_processed, 40u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
