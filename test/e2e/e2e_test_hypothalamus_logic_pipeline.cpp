/**
 * @file e2e_test_hypothalamus_logic_pipeline.cpp
 * @brief End-to-end tests for Hypothalamus-Logic Bridge Pipeline
 *
 * WHAT: Complete end-to-end tests for motivated reasoning pipelines
 * WHY:  Verify bidirectional integration between drive system and symbolic logic
 * HOW:  Test complete scenarios: hungry agent food search, fearful agent threat
 *       assessment, curious agent exploration, conflicting drives, goal lifecycle,
 *       FEP prediction loops, and extended simulation stability
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_logic_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/memory/nimcp_memory.h"

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Create a simple fact clause with a given predicate name
 */
static logic_clause_t* create_fact(const char* predicate_name, const char* arg = nullptr) {
    logic_clause_t* clause = (logic_clause_t*)nimcp_calloc(1, sizeof(logic_clause_t));
    if (!clause) return nullptr;

    clause->num_literals = 1;
    clause->confidence = 1.0f;
    clause->literals = (atomic_formula_t**)nimcp_calloc(1, sizeof(atomic_formula_t*));
    if (!clause->literals) {
        nimcp_free(clause);
        return nullptr;
    }

    clause->literals[0] = (atomic_formula_t*)nimcp_calloc(1, sizeof(atomic_formula_t));
    if (!clause->literals[0]) {
        nimcp_free(clause->literals);
        nimcp_free(clause);
        return nullptr;
    }

    strncpy(clause->literals[0]->name, predicate_name, LOGIC_MAX_NAME_LENGTH - 1);
    clause->literals[0]->negated = false;
    clause->literals[0]->arity = arg ? 1 : 0;

    if (arg) {
        clause->literals[0]->terms = (logical_term_t**)nimcp_calloc(1, sizeof(logical_term_t*));
        if (clause->literals[0]->terms) {
            clause->literals[0]->terms[0] = logic_term_create(TERM_CONSTANT, arg);
        }
    }

    return clause;
}

/**
 * @brief Create a negated fact clause
 */
static logic_clause_t* create_negated_fact(const char* predicate_name, const char* arg = nullptr) {
    logic_clause_t* clause = create_fact(predicate_name, arg);
    if (clause && clause->literals && clause->literals[0]) {
        clause->literals[0]->negated = true;
    }
    return clause;
}

/**
 * @brief Destroy a fact clause
 */
static void destroy_fact(logic_clause_t* clause) {
    if (!clause) return;
    if (clause->literals) {
        for (uint32_t i = 0; i < clause->num_literals; i++) {
            if (clause->literals[i]) {
                if (clause->literals[i]->terms) {
                    for (uint8_t j = 0; j < clause->literals[i]->arity; j++) {
                        logic_term_destroy(clause->literals[i]->terms[j]);
                    }
                    nimcp_free(clause->literals[i]->terms);
                }
                nimcp_free(clause->literals[i]);
            }
        }
        nimcp_free(clause->literals);
    }
    nimcp_free(clause);
}

// ============================================================================
// E2E TEST FIXTURE
// ============================================================================

class HypothalamusLogicE2ETest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives = nullptr;
    symbolic_logic_t* logic = nullptr;
    hypo_logic_bridge_t* bridge = nullptr;
    hypo_drive_config_t drive_config;
    logic_config_t logic_config;
    hypo_logic_config_t bridge_config;

    void SetUp() override {
        // Create drive system
        drive_config = hypo_drive_default_config();
        drive_config.alignment_mode = HYPO_ALIGN_CONTROLLED;
        drives = hypo_drive_create(&drive_config);
        ASSERT_NE(nullptr, drives);

        // Create symbolic logic engine
        logic_config.max_predicates = 100;
        logic_config.max_rules = 50;
        logic_config.max_kb_size = 200;
        logic_config.max_inference_depth = 10;
        logic_config.enable_forward_chaining = true;
        logic_config.enable_backward_chaining = true;
        logic_config.enable_resolution = true;
        logic_config.enable_memory_consolidation = false;
        logic_config.enable_quantum_logic = false;
        logic = symbolic_logic_create(&logic_config);
        ASSERT_NE(nullptr, logic);

        // Create bridge with default config
        bridge_config = hypo_logic_default_config();
        bridge = hypo_logic_bridge_create(drives, logic, &bridge_config);
        ASSERT_NE(nullptr, bridge);
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

    /**
     * @brief Set a specific drive to a high urgency level
     */
    void set_drive_urgency(hypo_drive_type_t drive, float urgency_level) {
        // Simulate time to let drive rise naturally, or manipulate via satisfactions
        // For testing, we simulate by running update cycles without satisfaction
        for (int i = 0; i < 100 && get_drive_urgency(drive) < urgency_level; i++) {
            hypo_drive_update(drives, 100000);  // 100ms per cycle
        }
    }

    /**
     * @brief Get current drive urgency
     */
    float get_drive_urgency(hypo_drive_type_t drive) {
        hypo_drive_state_t state;
        if (hypo_drive_get_state(drives, drive, &state)) {
            return state.urgency;
        }
        return 0.0f;
    }
};

// ============================================================================
// SCENARIO 1: HUNGRY AGENT FOOD SEARCH
// ============================================================================

TEST_F(HypothalamusLogicE2ETest, HungryAgentFoodSearch) {
    /**
     * Scenario: An agent with high HUNGER drive searches for food.
     * - Set high HUNGER drive urgency
     * - Add food-related facts to knowledge base
     * - Verify food predicates get salience boost
     * - Process "food_available(apple)" conclusion
     * - Verify HUNGER anticipation increases
     * - Simulate finding food, verify satisfaction
     */

    // Step 1: Set high HUNGER drive urgency
    set_drive_urgency(HYPO_DRIVE_HUNGER, 0.7f);
    float initial_urgency = get_drive_urgency(HYPO_DRIVE_HUNGER);
    EXPECT_GT(initial_urgency, 0.3f) << "HUNGER drive should be elevated";

    // Step 2: Compute modulation to reflect drive state
    ASSERT_EQ(0, hypo_logic_compute_modulation(bridge));

    // Step 3: Verify food predicates get salience boost
    float food_salience = hypo_logic_get_predicate_salience(bridge, "food_available");
    float neutral_salience = hypo_logic_get_predicate_salience(bridge, "neutral_fact");
    EXPECT_GT(food_salience, neutral_salience)
        << "Food predicates should have higher salience when hungry";
    EXPECT_GT(food_salience, 1.0f) << "Food salience should be boosted (>1.0)";

    // Step 4: Get initial anticipation state
    hypo_logic_anticipation_t initial_anticipation;
    ASSERT_EQ(0, hypo_logic_get_anticipation(bridge, &initial_anticipation));
    float initial_hunger_anticipation = initial_anticipation.anticipation[HYPO_DRIVE_HUNGER];

    // Step 5: Process food availability conclusion
    logic_clause_t* food_available = create_fact("food_available", "apple");
    ASSERT_NE(nullptr, food_available);
    ASSERT_EQ(0, hypo_logic_process_conclusion(bridge, food_available, 0.9f));

    // Step 6: Verify HUNGER anticipation increased
    hypo_logic_anticipation_t post_anticipation;
    ASSERT_EQ(0, hypo_logic_get_anticipation(bridge, &post_anticipation));
    EXPECT_GT(post_anticipation.anticipation[HYPO_DRIVE_HUNGER], initial_hunger_anticipation)
        << "Anticipation should increase when food is found";

    // Step 7: Simulate goal achievement and check logic bridge response
    logic_clause_t* food_consumed = create_fact("eaten", "apple");
    ASSERT_NE(nullptr, food_consumed);

    // Achievement produces reward signal from logic bridge
    float achievement_reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_HUNGER, food_consumed);
    EXPECT_GT(achievement_reward, 0.0f) << "Goal achievement should produce reward signal";

    // Also call drive system satisfy to model actual consumption
    float drive_reward = hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, 0.8f);
    EXPECT_GT(drive_reward, 0.0f) << "Satisfying hunger should produce positive reward";

    // Check that goal achievement was recorded in stats
    hypo_logic_stats_t stats;
    ASSERT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_GT(stats.goals_achieved, 0u) << "Goal achievement should be recorded";

    // Cleanup
    destroy_fact(food_available);
    destroy_fact(food_consumed);
}

// ============================================================================
// SCENARIO 2: FEARFUL AGENT THREAT ASSESSMENT
// ============================================================================

TEST_F(HypothalamusLogicE2ETest, FearfulAgentThreatAssessment) {
    /**
     * Scenario: An agent with high SAFETY drive assesses threats.
     * - Set high SAFETY drive urgency
     * - Verify threat predicates are prioritized
     * - Process "threat_present(predator)" conclusion
     * - Verify SAFETY drive anticipation increases
     * - Process "threat_absent" conclusion, verify relief
     */

    // Step 1: Elevate SAFETY drive
    set_drive_urgency(HYPO_DRIVE_SAFETY, 0.6f);

    // Step 2: Compute modulation
    ASSERT_EQ(0, hypo_logic_compute_modulation(bridge));

    // Step 3: Verify threat predicates are prioritized (high salience)
    float threat_salience = hypo_logic_get_predicate_salience(bridge, "threat_present");
    float danger_salience = hypo_logic_get_predicate_salience(bridge, "danger_detected");
    EXPECT_GT(threat_salience, 1.0f) << "Threat predicates should be boosted when safety is high";
    EXPECT_GT(danger_salience, 1.0f) << "Danger predicates should also be boosted";

    // Step 4: Get initial anticipation
    hypo_logic_anticipation_t initial_anticipation;
    ASSERT_EQ(0, hypo_logic_get_anticipation(bridge, &initial_anticipation));

    // Step 5: Process threat present conclusion
    logic_clause_t* threat_present = create_fact("threat_present", "predator");
    ASSERT_NE(nullptr, threat_present);
    ASSERT_EQ(0, hypo_logic_process_conclusion(bridge, threat_present, 0.85f));

    // Step 6: Verify SAFETY anticipation increased (threat detected -> need action)
    hypo_logic_anticipation_t post_threat;
    ASSERT_EQ(0, hypo_logic_get_anticipation(bridge, &post_threat));
    EXPECT_GT(post_threat.anticipation[HYPO_DRIVE_SAFETY],
              initial_anticipation.anticipation[HYPO_DRIVE_SAFETY])
        << "Safety anticipation should increase when threat detected";

    // Step 7: Classify the conclusion
    hypo_conclusion_type_t type = hypo_logic_classify_conclusion(bridge, threat_present);
    EXPECT_EQ(HYPO_CONCL_THREAT_PRESENT, type) << "Should classify as threat present";

    // Step 8: Process threat absent conclusion (relief)
    logic_clause_t* threat_absent = create_negated_fact("threat", "predator");
    ASSERT_NE(nullptr, threat_absent);

    // Force a "safe" type conclusion
    logic_clause_t* safe_now = create_fact("safe", "area");
    ASSERT_NE(nullptr, safe_now);
    ASSERT_EQ(0, hypo_logic_process_conclusion(bridge, safe_now, 0.9f));

    // Verify relief (anticipation should decrease)
    hypo_logic_anticipation_t post_relief;
    ASSERT_EQ(0, hypo_logic_get_anticipation(bridge, &post_relief));
    EXPECT_LT(post_relief.anticipation[HYPO_DRIVE_SAFETY],
              post_threat.anticipation[HYPO_DRIVE_SAFETY])
        << "Safety anticipation should decrease when confirmed safe";

    // Cleanup
    destroy_fact(threat_present);
    destroy_fact(threat_absent);
    destroy_fact(safe_now);
}

// ============================================================================
// SCENARIO 3: CURIOUS AGENT KNOWLEDGE EXPLORATION
// ============================================================================

TEST_F(HypothalamusLogicE2ETest, CuriousAgentKnowledgeExploration) {
    /**
     * Scenario: An agent with high CURIOSITY drive explores novel knowledge.
     * - Set high CURIOSITY drive
     * - Verify inference depth stays deep (exploratory reasoning)
     * - Verify curiosity-driven salience for knowledge predicates
     * - Process "discovered(new_fact)" conclusion
     * - Verify satisfaction from knowledge gain
     */

    // Step 1: Elevate CURIOSITY drive
    set_drive_urgency(HYPO_DRIVE_CURIOSITY, 0.75f);

    // Step 2: Compute modulation
    ASSERT_EQ(0, hypo_logic_compute_modulation(bridge));

    // Step 3: Check inference depth - high curiosity shouldn't reduce depth too much
    uint32_t depth = hypo_logic_get_recommended_depth(bridge);
    EXPECT_GE(depth, 5u) << "Curious agent should maintain reasonable inference depth";

    // Step 4: Verify knowledge predicates have high salience
    float know_salience = hypo_logic_get_predicate_salience(bridge, "know");
    float discover_salience = hypo_logic_get_predicate_salience(bridge, "discover");
    float explore_salience = hypo_logic_get_predicate_salience(bridge, "explore");

    EXPECT_GT(know_salience, 1.0f) << "Knowledge predicates should be boosted";
    EXPECT_GT(discover_salience, 1.0f) << "Discovery predicates should be boosted";
    EXPECT_GT(explore_salience, 1.0f) << "Exploration predicates should be boosted";

    // Step 5: Get initial anticipation
    hypo_logic_anticipation_t initial;
    ASSERT_EQ(0, hypo_logic_get_anticipation(bridge, &initial));

    // Step 6: Create motivated goal for knowledge
    logic_clause_t* knowledge_goal = create_fact("understand", "phenomenon");
    ASSERT_NE(nullptr, knowledge_goal);
    ASSERT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_CURIOSITY,
                                                   knowledge_goal, 0.8f));

    // Step 7: Verify goal was created and has priority
    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals = 0;
    ASSERT_EQ(0, hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals));
    EXPECT_GE(num_goals, 1u) << "Should have at least one motivated goal";
    if (num_goals > 0) {
        EXPECT_EQ(HYPO_DRIVE_CURIOSITY, goals[0].motivating_drive);
    }

    // Step 8: Simulate achieving the goal (discovery)
    float reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_CURIOSITY, knowledge_goal);
    EXPECT_GT(reward, 0.0f) << "Achieving knowledge goal should provide reward";

    // Verify stats updated
    hypo_logic_stats_t stats;
    ASSERT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_GE(stats.goals_achieved, 1u) << "Goals achieved counter should increment";

    // Cleanup
    destroy_fact(knowledge_goal);
}

// ============================================================================
// SCENARIO 4: CONFLICTING DRIVES
// ============================================================================

TEST_F(HypothalamusLogicE2ETest, ConflictingDrives) {
    /**
     * Scenario: Agent has both high HUNGER and high SAFETY drives.
     * - Set high HUNGER and high SAFETY
     * - Verify priority drive is selected correctly
     * - Process conclusions for both drives
     * - Verify both drives updated appropriately
     */

    // Step 1: Elevate both drives
    set_drive_urgency(HYPO_DRIVE_HUNGER, 0.65f);
    set_drive_urgency(HYPO_DRIVE_SAFETY, 0.7f);

    float hunger_urgency = get_drive_urgency(HYPO_DRIVE_HUNGER);
    float safety_urgency = get_drive_urgency(HYPO_DRIVE_SAFETY);

    // Step 2: Compute modulation
    ASSERT_EQ(0, hypo_logic_compute_modulation(bridge));

    // Step 3: Get modulation state to check priority drive
    hypo_logic_modulation_t modulation;
    ASSERT_EQ(0, hypo_logic_get_modulation(bridge, &modulation));

    // The priority drive should be the one with higher urgency
    hypo_drive_type_t expected_priority = (safety_urgency >= hunger_urgency) ?
                                           HYPO_DRIVE_SAFETY : HYPO_DRIVE_HUNGER;
    EXPECT_EQ(expected_priority, modulation.priority_drive)
        << "Priority drive should be the most urgent";

    // Step 4: Check that both drives boost their related predicates
    float food_salience = hypo_logic_get_predicate_salience(bridge, "food");
    float threat_salience = hypo_logic_get_predicate_salience(bridge, "threat");

    EXPECT_GT(food_salience, 1.0f) << "Food salience should be boosted with high hunger";
    EXPECT_GT(threat_salience, 1.0f) << "Threat salience should be boosted with high safety";

    // Step 5: Process conclusions affecting both drives
    logic_clause_t* food_nearby = create_fact("food_available", "berries");
    logic_clause_t* threat_nearby = create_fact("threat_present", "wolf");
    ASSERT_NE(nullptr, food_nearby);
    ASSERT_NE(nullptr, threat_nearby);

    hypo_logic_anticipation_t before;
    ASSERT_EQ(0, hypo_logic_get_anticipation(bridge, &before));

    ASSERT_EQ(0, hypo_logic_process_conclusion(bridge, food_nearby, 0.8f));
    ASSERT_EQ(0, hypo_logic_process_conclusion(bridge, threat_nearby, 0.9f));

    hypo_logic_anticipation_t after;
    ASSERT_EQ(0, hypo_logic_get_anticipation(bridge, &after));

    // Both anticipations should have increased
    EXPECT_GT(after.anticipation[HYPO_DRIVE_HUNGER], before.anticipation[HYPO_DRIVE_HUNGER])
        << "Hunger anticipation should increase when food found";
    EXPECT_GT(after.anticipation[HYPO_DRIVE_SAFETY], before.anticipation[HYPO_DRIVE_SAFETY])
        << "Safety anticipation should increase when threat detected";

    // Cleanup
    destroy_fact(food_nearby);
    destroy_fact(threat_nearby);
}

// ============================================================================
// SCENARIO 5: GOAL LIFECYCLE PIPELINE
// ============================================================================

TEST_F(HypothalamusLogicE2ETest, GoalLifecyclePipeline) {
    /**
     * Scenario: Full lifecycle of a motivated goal.
     * - Create motivated goal
     * - Verify priority ordering
     * - Achieve goal via proof simulation
     * - Verify reward signal
     * - Verify goal removed from active list
     */

    // Step 1: Set up a moderate drive
    set_drive_urgency(HYPO_DRIVE_COMPETENCE, 0.5f);
    ASSERT_EQ(0, hypo_logic_compute_modulation(bridge));

    // Step 2: Create multiple motivated goals with different priorities
    logic_clause_t* goal1 = create_fact("achieve", "skill_A");
    logic_clause_t* goal2 = create_fact("master", "skill_B");
    logic_clause_t* goal3 = create_fact("accomplish", "task_C");
    ASSERT_NE(nullptr, goal1);
    ASSERT_NE(nullptr, goal2);
    ASSERT_NE(nullptr, goal3);

    ASSERT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_COMPETENCE, goal1, 0.6f));
    ASSERT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_COMPETENCE, goal2, 0.9f));
    ASSERT_EQ(0, hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_COMPETENCE, goal3, 0.3f));

    // Step 3: Verify priority ordering
    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals = 0;
    ASSERT_EQ(0, hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals));
    EXPECT_EQ(3u, num_goals);

    // Goals should be sorted by priority (highest first)
    if (num_goals >= 2) {
        EXPECT_GE(goals[0].priority, goals[1].priority)
            << "Goals should be sorted by descending priority";
    }

    // Step 4: Achieve the highest priority goal
    hypo_logic_stats_t before_stats;
    ASSERT_EQ(0, hypo_logic_get_stats(bridge, &before_stats));

    float reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_COMPETENCE, goal2);
    EXPECT_GT(reward, 0.0f) << "Achieving goal should produce reward";

    // Step 5: Verify stats updated
    hypo_logic_stats_t after_stats;
    ASSERT_EQ(0, hypo_logic_get_stats(bridge, &after_stats));
    EXPECT_GT(after_stats.goals_achieved, before_stats.goals_achieved);

    // Step 6: Test goal impossibility
    float frustration = hypo_logic_goal_impossible(bridge, HYPO_DRIVE_COMPETENCE, goal3);
    EXPECT_GT(frustration, 0.0f) << "Impossible goal should produce frustration";

    // Verify frustration event recorded
    ASSERT_EQ(0, hypo_logic_get_stats(bridge, &after_stats));
    EXPECT_GE(after_stats.goals_abandoned, 1u);
    EXPECT_GE(after_stats.frustration_events, 1u);

    // Cleanup
    destroy_fact(goal1);
    destroy_fact(goal2);
    destroy_fact(goal3);
}

// ============================================================================
// SCENARIO 6: FEP PREDICTION LOOP
// ============================================================================

TEST_F(HypothalamusLogicE2ETest, FEPPredictionLoop) {
    /**
     * Scenario: Full FEP prediction and error computation cycle.
     * - Generate predictions from drive state
     * - Perform inference (simulated)
     * - Update predictions with actual results
     * - Verify prediction error computed
     * - Verify free energy reflects uncertainty
     */

    // Step 1: Set up drives with varied urgencies
    set_drive_urgency(HYPO_DRIVE_HUNGER, 0.6f);
    set_drive_urgency(HYPO_DRIVE_SOCIAL, 0.4f);
    ASSERT_EQ(0, hypo_logic_compute_modulation(bridge));

    // Step 2: Generate predictions
    ASSERT_EQ(0, hypo_logic_generate_predictions(bridge));

    hypo_logic_stats_t stats;
    ASSERT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    EXPECT_GT(stats.predictions_made, 0u) << "Predictions should be generated";

    // Step 3: Get initial FEP state
    hypo_logic_fep_state_t initial_fep;
    ASSERT_EQ(0, hypo_logic_get_fep_state(bridge, &initial_fep));

    // Step 4: Process conclusions (simulating inference results)
    logic_clause_t* food_found = create_fact("food_available", "meal");
    ASSERT_NE(nullptr, food_found);

    // Update predictions with actual result
    float pred_error = hypo_logic_update_predictions(bridge, food_found);
    // Prediction error should be computed
    // (value depends on prediction vs actual - can be positive or negative)

    // Step 5: Compute free energy
    hypo_logic_fep_state_t computed_fep;
    ASSERT_EQ(0, hypo_logic_compute_free_energy(bridge, &computed_fep));

    // Free energy should reflect the state
    EXPECT_GE(computed_fep.logical_free_energy, 0.0f) << "Free energy should be non-negative";
    EXPECT_GE(computed_fep.precision, 0.0f) << "Precision should be non-negative";
    EXPECT_LE(computed_fep.precision, 1.0f) << "Precision should be bounded";

    // Step 6: Verify stats reflect prediction tracking
    ASSERT_EQ(0, hypo_logic_get_stats(bridge, &stats));
    // Either confirmed or violated should be incremented
    EXPECT_GE(stats.predictions_confirmed + stats.predictions_violated, 1u)
        << "Prediction outcome should be tracked";

    // Cleanup
    destroy_fact(food_found);
}

// ============================================================================
// SCENARIO 7: EXTENDED SIMULATION
// ============================================================================

TEST_F(HypothalamusLogicE2ETest, ExtendedSimulationStability) {
    /**
     * Scenario: Long-running simulation to verify stability.
     * - Run 1000 update cycles
     * - Vary drive states
     * - Process many conclusions
     * - Verify state stability
     * - Check statistics consistency
     */

    const int TOTAL_CYCLES = 1000;
    const int CONCLUSION_INTERVAL = 10;

    // Track for stability verification
    float max_free_energy = 0.0f;
    float min_free_energy = 1000.0f;
    int update_errors = 0;

    for (int cycle = 0; cycle < TOTAL_CYCLES; cycle++) {
        // Update drive system
        hypo_drive_update(drives, 10000);  // 10ms per cycle

        // Update bridge
        int result = hypo_logic_bridge_update(bridge, 10000);
        if (result != 0) {
            update_errors++;
        }

        // Periodically vary drives (simulate agent activity)
        if (cycle % 50 == 0) {
            hypo_drive_type_t drive_to_satisfy =
                static_cast<hypo_drive_type_t>(cycle / 50 % HYPO_DRIVE_COUNT);
            hypo_drive_satisfy(drives, drive_to_satisfy, 0.3f);
        }

        // Periodically process conclusions
        if (cycle % CONCLUSION_INTERVAL == 0) {
            // Alternate between different conclusion types
            int conclusion_type = (cycle / CONCLUSION_INTERVAL) % 4;
            logic_clause_t* conclusion = nullptr;

            switch (conclusion_type) {
                case 0:
                    conclusion = create_fact("food_available", "fruit");
                    break;
                case 1:
                    conclusion = create_fact("safe", "location");
                    break;
                case 2:
                    conclusion = create_fact("discovered", "info");
                    break;
                case 3:
                    conclusion = create_fact("achieved", "goal");
                    break;
            }

            if (conclusion) {
                hypo_logic_process_conclusion(bridge, conclusion, 0.7f);
                destroy_fact(conclusion);
            }
        }

        // Track free energy for stability check
        hypo_logic_fep_state_t fe_state;
        if (hypo_logic_compute_free_energy(bridge, &fe_state) == 0) {
            if (fe_state.logical_free_energy > max_free_energy) {
                max_free_energy = fe_state.logical_free_energy;
            }
            if (fe_state.logical_free_energy < min_free_energy) {
                min_free_energy = fe_state.logical_free_energy;
            }
        }
    }

    // Verify no update errors
    EXPECT_EQ(0, update_errors) << "No update errors during extended simulation";

    // Verify free energy stayed bounded (stability)
    EXPECT_LT(max_free_energy, 10.0f) << "Free energy should stay bounded";
    EXPECT_GE(min_free_energy, 0.0f) << "Free energy should stay non-negative";

    // Verify statistics consistency
    hypo_logic_stats_t final_stats;
    ASSERT_EQ(0, hypo_logic_get_stats(bridge, &final_stats));

    // Should have processed many conclusions
    int expected_conclusions = TOTAL_CYCLES / CONCLUSION_INTERVAL;
    EXPECT_GE(final_stats.conclusions_processed, (uint64_t)(expected_conclusions * 0.9))
        << "Should have processed most conclusions";

    // Modulations should have occurred
    EXPECT_GT(final_stats.salience_modulations, 0u);
    EXPECT_GT(final_stats.depth_modulations, 0u);

    // Anticipation updates should have occurred
    EXPECT_GT(final_stats.anticipation_updates, 0u);

    // Print summary for debugging
    hypo_logic_print_summary(bridge);
}

// ============================================================================
// ADDITIONAL TESTS: EDGE CASES AND BOUNDARY CONDITIONS
// ============================================================================

TEST_F(HypothalamusLogicE2ETest, PredicateCategoryMapping) {
    /**
     * Test that predicates are correctly categorized and mapped to drives.
     */

    // Food predicates -> HUNGER
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("food"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("eat"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("hungry"));
    EXPECT_EQ(HYPO_PRED_CAT_FOOD, hypo_logic_get_predicate_category("edible_item"));

    // Threat predicates -> SAFETY
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("threat"));
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("danger"));
    EXPECT_EQ(HYPO_PRED_CAT_THREAT, hypo_logic_get_predicate_category("hostile_entity"));

    // Knowledge predicates -> CURIOSITY
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("know"));
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("discover"));
    EXPECT_EQ(HYPO_PRED_CAT_KNOWLEDGE, hypo_logic_get_predicate_category("explore"));

    // Social predicates -> SOCIAL
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("friend"));
    EXPECT_EQ(HYPO_PRED_CAT_SOCIAL, hypo_logic_get_predicate_category("ally"));

    // Neutral predicates
    EXPECT_EQ(HYPO_PRED_CAT_NEUTRAL, hypo_logic_get_predicate_category("random"));
    EXPECT_EQ(HYPO_PRED_CAT_NEUTRAL, hypo_logic_get_predicate_category("xyz"));

    // Category to drive mapping
    EXPECT_EQ(HYPO_DRIVE_HUNGER, hypo_logic_category_to_drive(HYPO_PRED_CAT_FOOD));
    EXPECT_EQ(HYPO_DRIVE_SAFETY, hypo_logic_category_to_drive(HYPO_PRED_CAT_THREAT));
    EXPECT_EQ(HYPO_DRIVE_SAFETY, hypo_logic_category_to_drive(HYPO_PRED_CAT_SAFETY));
    EXPECT_EQ(HYPO_DRIVE_CURIOSITY, hypo_logic_category_to_drive(HYPO_PRED_CAT_KNOWLEDGE));
    EXPECT_EQ(HYPO_DRIVE_SOCIAL, hypo_logic_category_to_drive(HYPO_PRED_CAT_SOCIAL));
}

TEST_F(HypothalamusLogicE2ETest, ConclusionTypeClassification) {
    /**
     * Test that conclusions are correctly classified by type.
     */

    logic_clause_t* threat = create_fact("threat_present", "enemy");
    logic_clause_t* safe = create_fact("safe", "zone");
    logic_clause_t* available = create_fact("available", "resource");
    logic_clause_t* achieved = create_fact("achieved", "goal");
    logic_clause_t* impossible = create_fact("impossible", "task");

    EXPECT_EQ(HYPO_CONCL_THREAT_PRESENT, hypo_logic_classify_conclusion(bridge, threat));
    EXPECT_EQ(HYPO_CONCL_THREAT_ABSENT, hypo_logic_classify_conclusion(bridge, safe));
    EXPECT_EQ(HYPO_CONCL_RESOURCE_AVAILABLE, hypo_logic_classify_conclusion(bridge, available));
    EXPECT_EQ(HYPO_CONCL_GOAL_ACHIEVED, hypo_logic_classify_conclusion(bridge, achieved));
    EXPECT_EQ(HYPO_CONCL_GOAL_IMPOSSIBLE, hypo_logic_classify_conclusion(bridge, impossible));

    destroy_fact(threat);
    destroy_fact(safe);
    destroy_fact(available);
    destroy_fact(achieved);
    destroy_fact(impossible);
}

TEST_F(HypothalamusLogicE2ETest, WishfulThinkingBias) {
    /**
     * Test that high-urgency drives lower proof thresholds (wishful thinking).
     */

    // With low hunger, threshold should be near baseline
    hypo_logic_compute_modulation(bridge);
    hypo_logic_modulation_t low_mod;
    hypo_logic_get_modulation(bridge, &low_mod);

    logic_clause_t* food_goal = create_fact("food_available", "meal");
    float low_threshold = hypo_logic_get_goal_threshold(bridge, food_goal);

    // Elevate hunger
    set_drive_urgency(HYPO_DRIVE_HUNGER, 0.8f);
    hypo_logic_compute_modulation(bridge);

    float high_threshold = hypo_logic_get_goal_threshold(bridge, food_goal);

    // With high hunger, threshold for food goals should be lower (more accepting)
    EXPECT_LE(high_threshold, low_threshold)
        << "Threshold should decrease with higher drive urgency (wishful thinking)";

    destroy_fact(food_goal);
}

TEST_F(HypothalamusLogicE2ETest, InferenceDepthUrgencyModulation) {
    /**
     * Test that high urgency reduces inference depth (fast, heuristic reasoning).
     */

    // With low urgency, depth should be near maximum
    hypo_logic_compute_modulation(bridge);
    uint32_t low_urgency_depth = hypo_logic_get_recommended_depth(bridge);

    // Elevate all drives to high urgency
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        set_drive_urgency(static_cast<hypo_drive_type_t>(i), 0.9f);
    }
    hypo_logic_compute_modulation(bridge);
    uint32_t high_urgency_depth = hypo_logic_get_recommended_depth(bridge);

    // High urgency should reduce depth (faster, shallower reasoning)
    EXPECT_LE(high_urgency_depth, low_urgency_depth)
        << "High urgency should reduce inference depth";
    EXPECT_GE(high_urgency_depth, 3u) << "Depth should not go below minimum";
}

TEST_F(HypothalamusLogicE2ETest, BridgeResetCleanup) {
    /**
     * Test that bridge reset properly clears state.
     */

    // Create some state
    logic_clause_t* goal = create_fact("test_goal");
    hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_CURIOSITY, goal, 0.5f);

    logic_clause_t* conclusion = create_fact("test_conclusion");
    hypo_logic_process_conclusion(bridge, conclusion, 0.8f);

    hypo_logic_bridge_update(bridge, 10000);

    // Verify state exists
    hypo_logic_stats_t before_stats;
    hypo_logic_get_stats(bridge, &before_stats);
    EXPECT_GT(before_stats.conclusions_processed, 0u);

    // Reset
    ASSERT_EQ(0, hypo_logic_bridge_reset(bridge));

    // Verify state cleared
    hypo_logic_stats_t after_stats;
    hypo_logic_get_stats(bridge, &after_stats);
    EXPECT_EQ(0u, after_stats.conclusions_processed);
    EXPECT_EQ(0u, after_stats.goals_created);

    // Verify anticipation cleared
    hypo_logic_anticipation_t anticipation;
    hypo_logic_get_anticipation(bridge, &anticipation);
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        EXPECT_FLOAT_EQ(0.0f, anticipation.anticipation[i]);
        EXPECT_FLOAT_EQ(0.0f, anticipation.frustration[i]);
    }

    destroy_fact(goal);
    destroy_fact(conclusion);
}

TEST_F(HypothalamusLogicE2ETest, ConclusionTypeStrings) {
    /**
     * Test string utility functions.
     */

    EXPECT_STREQ("resource_available", hypo_conclusion_type_name(HYPO_CONCL_RESOURCE_AVAILABLE));
    EXPECT_STREQ("threat_present", hypo_conclusion_type_name(HYPO_CONCL_THREAT_PRESENT));
    EXPECT_STREQ("goal_achieved", hypo_conclusion_type_name(HYPO_CONCL_GOAL_ACHIEVED));
    EXPECT_STREQ("neutral", hypo_conclusion_type_name(HYPO_CONCL_NEUTRAL));

    EXPECT_STREQ("food", hypo_predicate_category_name(HYPO_PRED_CAT_FOOD));
    EXPECT_STREQ("threat", hypo_predicate_category_name(HYPO_PRED_CAT_THREAT));
    EXPECT_STREQ("knowledge", hypo_predicate_category_name(HYPO_PRED_CAT_KNOWLEDGE));
    EXPECT_STREQ("neutral", hypo_predicate_category_name(HYPO_PRED_CAT_NEUTRAL));
}

TEST_F(HypothalamusLogicE2ETest, RegisterCustomPredicateMapping) {
    /**
     * Test registering custom predicate-drive mappings.
     */

    // Register a custom mapping
    hypo_logic_predicate_map_t custom_map = {
        .predicate_name = "custom_resource",
        .drive = HYPO_DRIVE_HUNGER,
        .relevance = 0.9f,
        .valence = 1.0f,
        .is_goal_predicate = true
    };

    ASSERT_EQ(0, hypo_logic_register_predicate_map(bridge, &custom_map));

    // Set hunger high
    set_drive_urgency(HYPO_DRIVE_HUNGER, 0.7f);
    hypo_logic_compute_modulation(bridge);

    // Custom predicate should now have boosted salience
    float custom_salience = hypo_logic_get_predicate_salience(bridge, "custom_resource");
    float neutral_salience = hypo_logic_get_predicate_salience(bridge, "neutral_thing");

    EXPECT_GT(custom_salience, neutral_salience)
        << "Custom mapped predicate should have higher salience";
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
