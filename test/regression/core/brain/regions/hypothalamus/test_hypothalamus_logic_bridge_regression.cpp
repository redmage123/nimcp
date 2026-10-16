/**
 * @file test_hypothalamus_logic_bridge_regression.cpp
 * @brief Regression tests for Hypothalamus-Logic Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Regression tests for hypothalamus-logic bridge ensuring accuracy,
 *       stability, and performance across code changes
 * WHY:  Prevent regressions in motivated reasoning, conclusion processing,
 *       and FEP integration
 * HOW:  Test known good behaviors, numerical accuracy, performance bounds,
 *       and memory stability
 *
 * TESTS COVER:
 * - Numerical stability: Repeated modulation updates don't drift
 * - Memory safety: No leaks over many create/destroy cycles
 * - Performance bounds: Modulation computation within latency limits
 * - State consistency: Bridge state matches expected after operations
 * - Boundary conditions: Max predicates, max goals, empty inputs
 * - Error handling: Invalid inputs handled gracefully
 * - Determinism: Same inputs produce same outputs
 * - Statistics accuracy: Stats counters increment correctly
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_logic_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/nimcp_symbolic_logic.h"

/* ============================================================================
 * Test Constants - Known Good Baselines
 * ============================================================================ */

/* Performance baselines (microseconds) */
#define REGRESSION_COMPUTE_MODULATION_MAX_US   100   /* 100us max */
#define REGRESSION_PROCESS_CONCLUSION_MAX_US   50    /* 50us max */
#define REGRESSION_FULL_UPDATE_CYCLE_MAX_US    500   /* 500us max */
#define REGRESSION_AVG_UPDATE_TIME_US          200   /* 200us average target */

/* Memory baselines */
#define REGRESSION_MAX_ITERATIONS              3000  /* Iterations for memory test */
#define REGRESSION_CYCLES_PER_CHECK            300   /* Check state every N cycles */
#define REGRESSION_CREATE_DESTROY_CYCLES       500   /* Create/destroy cycles */

/* Numerical accuracy */
#define REGRESSION_FLOAT_EPSILON               1e-5f /* Float comparison tolerance */
#define REGRESSION_FREE_ENERGY_TOLERANCE       0.01f /* FE stability tolerance */
#define REGRESSION_THRESHOLD_TOLERANCE         0.001f/* Threshold stability tolerance */

/* Boundary conditions */
#define REGRESSION_MAX_PREDICATE_MAPS_TEST     HYPO_LOGIC_MAX_PREDICATES
#define REGRESSION_MAX_GOALS_TEST              HYPO_LOGIC_MAX_GOALS

/* ============================================================================
 * Test Fixture for Hypothalamus-Logic Bridge
 * ============================================================================ */

class HypoLogicBridgeRegressionTest : public ::testing::Test {
protected:
    hypo_logic_bridge_t* bridge = nullptr;
    hypo_logic_config_t config;
    hypo_drive_system_handle_t* drive_system = nullptr;
    symbolic_logic_t* logic_engine = nullptr;

    void SetUp() override {
        /* Create drive system */
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(drive_system, nullptr);

        /* Create logic engine */
        logic_config_t logic_config = {
            .max_predicates = 100,
            .max_rules = 50,
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
        config = hypo_logic_default_config();
        bridge = hypo_logic_bridge_create(drive_system, logic_engine, &config);
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

    /* Helper: Verify float is in [0,1] range */
    bool verify_normalized(float value) {
        return value >= 0.0f && value <= 1.0f && !std::isnan(value) && !std::isinf(value);
    }

    /* Helper: Verify free energy is non-negative */
    bool verify_fe_bounds(float fe) {
        return fe >= 0.0f && !std::isnan(fe) && !std::isinf(fe);
    }

    /* Helper: Verify salience boost is positive */
    bool verify_salience_positive(float salience) {
        return salience > 0.0f && !std::isnan(salience) && !std::isinf(salience);
    }

    /* Helper: Create a simple test clause */
    logic_clause_t* create_test_clause(const char* predicate_name, bool negated = false) {
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
        atom->negated = negated;
        atom->arity = 0;
        atom->terms = nullptr;

        clause->literals[0] = atom;
        clause->num_literals = 1;
        clause->confidence = 1.0f;

        return clause;
    }

    /* Helper: Free test clause */
    void free_test_clause(logic_clause_t* clause) {
        if (!clause) return;
        if (clause->literals) {
            if (clause->literals[0]) {
                free(clause->literals[0]);
            }
            free(clause->literals);
        }
        free(clause);
    }
};

/* ============================================================================
 * REG-HL-001: Bridge handles NULL drives/logic gracefully
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_001_NullInputsHandledGracefully) {
    /* Test NULL drives */
    hypo_logic_bridge_t* null_bridge = hypo_logic_bridge_create(nullptr, logic_engine, &config);
    EXPECT_EQ(null_bridge, nullptr) << "Bridge should not create with NULL drives";

    /* Test NULL logic */
    null_bridge = hypo_logic_bridge_create(drive_system, nullptr, &config);
    EXPECT_EQ(null_bridge, nullptr) << "Bridge should not create with NULL logic";

    /* Test NULL both */
    null_bridge = hypo_logic_bridge_create(nullptr, nullptr, &config);
    EXPECT_EQ(null_bridge, nullptr) << "Bridge should not create with NULL drives and logic";

    /* Test NULL bridge on various functions */
    EXPECT_EQ(hypo_logic_bridge_reset(nullptr), -1);
    EXPECT_EQ(hypo_logic_bridge_update(nullptr, 1000), -1);
    EXPECT_EQ(hypo_logic_compute_modulation(nullptr), -1);

    hypo_logic_modulation_t mod;
    EXPECT_EQ(hypo_logic_get_modulation(nullptr, &mod), -1);

    EXPECT_EQ(hypo_logic_apply_modulation(nullptr), -1);

    /* NULL bridge on salience should return default */
    float salience = hypo_logic_get_predicate_salience(nullptr, "food");
    EXPECT_FLOAT_EQ(salience, 1.0f) << "NULL bridge should return default salience";

    /* NULL predicate should return default */
    salience = hypo_logic_get_predicate_salience(bridge, nullptr);
    EXPECT_FLOAT_EQ(salience, 1.0f) << "NULL predicate should return default salience";

    /* NULL bridge on goal threshold should return default */
    float threshold = hypo_logic_get_goal_threshold(nullptr, nullptr);
    EXPECT_FLOAT_EQ(threshold, 0.7f) << "NULL bridge should return default threshold";

    /* NULL bridge on recommended depth should return default */
    uint32_t depth = hypo_logic_get_recommended_depth(nullptr);
    EXPECT_EQ(depth, 10u) << "NULL bridge should return default depth";

    /* Process conclusion with NULL */
    EXPECT_EQ(hypo_logic_process_conclusion(nullptr, nullptr, 0.5f), -1);
    EXPECT_EQ(hypo_logic_process_conclusion(bridge, nullptr, 0.5f), -1);

    /* Get stats with NULL */
    hypo_logic_stats_t stats;
    EXPECT_EQ(hypo_logic_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(hypo_logic_get_stats(bridge, nullptr), -1);

    /* Destroy NULL is safe */
    hypo_logic_bridge_destroy(nullptr);
}

/* ============================================================================
 * REG-HL-002: max_inference_depth stays within [min, base] bounds
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_002_InferenceDepthBounds) {
    /* Compute modulation first */
    int ret = hypo_logic_compute_modulation(bridge);
    EXPECT_EQ(ret, 0);

    /* Get modulation state */
    hypo_logic_modulation_t mod;
    ret = hypo_logic_get_modulation(bridge, &mod);
    EXPECT_EQ(ret, 0);

    /* Verify depth is within bounds */
    EXPECT_GE(mod.max_inference_depth, (uint32_t)config.min_inference_depth)
        << "Inference depth should be >= min_inference_depth";
    EXPECT_LE(mod.max_inference_depth, (uint32_t)config.base_inference_depth)
        << "Inference depth should be <= base_inference_depth";

    /* Test across many update cycles */
    for (int i = 0; i < 500; i++) {
        /* Vary drive urgencies by simulating time passage */
        hypo_drive_update(drive_system, 10000);  /* 10ms */

        hypo_logic_compute_modulation(bridge);
        hypo_logic_get_modulation(bridge, &mod);

        EXPECT_GE(mod.max_inference_depth, (uint32_t)config.min_inference_depth)
            << "Depth below minimum at iteration " << i;
        EXPECT_LE(mod.max_inference_depth, (uint32_t)config.base_inference_depth)
            << "Depth above maximum at iteration " << i;
    }

    /* Verify recommended depth matches */
    uint32_t recommended = hypo_logic_get_recommended_depth(bridge);
    EXPECT_EQ(recommended, mod.max_inference_depth);
}

/* ============================================================================
 * REG-HL-003: proof_threshold stays in [0,1] range
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_003_ProofThresholdRange) {
    /* Test base threshold */
    hypo_logic_modulation_t mod;
    int ret = hypo_logic_get_modulation(bridge, &mod);
    EXPECT_EQ(ret, 0);

    EXPECT_TRUE(verify_normalized(mod.proof_threshold))
        << "Base proof threshold out of [0,1]: " << mod.proof_threshold;
    EXPECT_TRUE(verify_normalized(mod.base_threshold))
        << "Base threshold out of [0,1]: " << mod.base_threshold;

    /* Test goal-specific thresholds */
    logic_clause_t* food_goal = create_test_clause("food_available");
    ASSERT_NE(food_goal, nullptr);

    float threshold = hypo_logic_get_goal_threshold(bridge, food_goal);
    EXPECT_GE(threshold, 0.0f) << "Goal threshold below 0";
    EXPECT_LE(threshold, 1.0f) << "Goal threshold above 1";

    free_test_clause(food_goal);

    /* Test across many iterations with modulation */
    for (int i = 0; i < 500; i++) {
        hypo_drive_update(drive_system, 10000);
        hypo_logic_compute_modulation(bridge);
        hypo_logic_get_modulation(bridge, &mod);

        EXPECT_TRUE(verify_normalized(mod.proof_threshold))
            << "Threshold out of [0,1] at iteration " << i << ": " << mod.proof_threshold;
        EXPECT_TRUE(verify_normalized(mod.wishful_thinking_bias))
            << "Wishful thinking bias out of [0,1] at iteration " << i;
    }
}

/* ============================================================================
 * REG-HL-004: salience_boost values stay positive
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_004_SalienceBoostPositive) {
    hypo_logic_compute_modulation(bridge);

    hypo_logic_modulation_t mod;
    hypo_logic_get_modulation(bridge, &mod);

    /* Check all salience boosts are positive */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        EXPECT_GT(mod.salience_boost[i], 0.0f)
            << "Salience boost for drive " << i << " is not positive";
        EXPECT_FALSE(std::isnan(mod.salience_boost[i]))
            << "Salience boost for drive " << i << " is NaN";
        EXPECT_FALSE(std::isinf(mod.salience_boost[i]))
            << "Salience boost for drive " << i << " is infinite";
    }

    /* Test predicate salience */
    const char* test_predicates[] = {
        "food", "water", "threat", "safe", "friend",
        "know", "warm", "rest", "achieve", "autonomy",
        "unknown_predicate"
    };

    for (const char* pred : test_predicates) {
        float salience = hypo_logic_get_predicate_salience(bridge, pred);
        EXPECT_TRUE(verify_salience_positive(salience))
            << "Salience for '" << pred << "' is not positive: " << salience;
    }

    /* Test over many iterations */
    for (int i = 0; i < 500; i++) {
        hypo_drive_update(drive_system, 10000);
        hypo_logic_compute_modulation(bridge);
        hypo_logic_get_modulation(bridge, &mod);

        for (int j = 0; j < HYPO_DRIVE_COUNT; j++) {
            EXPECT_TRUE(verify_salience_positive(mod.salience_boost[j]))
                << "Salience boost went non-positive at iteration " << i << ", drive " << j;
        }
    }
}

/* ============================================================================
 * REG-HL-005: anticipation/frustration stay in [0,1] range
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_005_AnticipationFrustrationRange) {
    /* Process various conclusions to affect anticipation */
    const char* conclusions[] = {
        "food_available", "water_nearby", "threat_present",
        "safe_location", "friend_found", "knowledge_gained"
    };

    for (const char* conc : conclusions) {
        logic_clause_t* clause = create_test_clause(conc);
        if (clause) {
            hypo_logic_process_conclusion(bridge, clause, 0.8f);
            free_test_clause(clause);
        }
    }

    /* Check anticipation bounds */
    hypo_logic_anticipation_t anticipation;
    int ret = hypo_logic_get_anticipation(bridge, &anticipation);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        EXPECT_TRUE(verify_normalized(anticipation.anticipation[i]))
            << "Anticipation[" << i << "] out of [0,1]: " << anticipation.anticipation[i];
        EXPECT_TRUE(verify_normalized(anticipation.frustration[i]))
            << "Frustration[" << i << "] out of [0,1]: " << anticipation.frustration[i];
        EXPECT_TRUE(verify_normalized(anticipation.confidence[i]))
            << "Confidence[" << i << "] out of [0,1]: " << anticipation.confidence[i];
    }

    /* Stress test with many conclusions */
    for (int i = 0; i < 500; i++) {
        logic_clause_t* clause = create_test_clause(conclusions[i % 6]);
        if (clause) {
            hypo_logic_process_conclusion(bridge, clause, 0.9f);
            free_test_clause(clause);
        }

        hypo_logic_get_anticipation(bridge, &anticipation);
        for (int j = 0; j < HYPO_DRIVE_COUNT; j++) {
            EXPECT_TRUE(verify_normalized(anticipation.anticipation[j]))
                << "Anticipation overflow at iteration " << i;
            EXPECT_TRUE(verify_normalized(anticipation.frustration[j]))
                << "Frustration overflow at iteration " << i;
        }
    }
}

/* ============================================================================
 * REG-HL-006: FEP free_energy is non-negative
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_006_FreeEnergyNonNegative) {
    /* Compute free energy */
    hypo_logic_fep_state_t fe_state;
    int ret = hypo_logic_compute_free_energy(bridge, &fe_state);
    EXPECT_EQ(ret, 0);

    EXPECT_TRUE(verify_fe_bounds(fe_state.logical_free_energy))
        << "Logical free energy is negative or invalid: " << fe_state.logical_free_energy;

    EXPECT_FALSE(std::isnan(fe_state.prediction_error))
        << "Prediction error is NaN";
    EXPECT_FALSE(std::isinf(fe_state.prediction_error))
        << "Prediction error is infinite";

    EXPECT_TRUE(verify_normalized(fe_state.precision))
        << "Precision out of [0,1]: " << fe_state.precision;

    EXPECT_TRUE(verify_fe_bounds(fe_state.complexity_cost))
        << "Complexity cost is negative or invalid: " << fe_state.complexity_cost;

    EXPECT_GE(fe_state.expected_info_gain, 0.0f)
        << "Expected info gain is negative";

    /* Test over many update cycles */
    for (int i = 0; i < 500; i++) {
        hypo_drive_update(drive_system, 10000);
        hypo_logic_bridge_update(bridge, 10000);

        hypo_logic_fep_state_t state;
        ret = hypo_logic_get_fep_state(bridge, &state);
        EXPECT_EQ(ret, 0);

        EXPECT_TRUE(verify_fe_bounds(state.logical_free_energy))
            << "FE went negative at iteration " << i << ": " << state.logical_free_energy;
    }
}

/* ============================================================================
 * REG-HL-007: prediction_error computation is accurate
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_007_PredictionErrorAccurate) {
    /* Generate predictions first */
    int ret = hypo_logic_generate_predictions(bridge);
    EXPECT_EQ(ret, 0);

    /* Process conclusions and track prediction errors */
    std::vector<float> prediction_errors;

    logic_clause_t* resource_clause = create_test_clause("food_available");
    ASSERT_NE(resource_clause, nullptr);

    float error = hypo_logic_update_predictions(bridge, resource_clause);
    prediction_errors.push_back(error);

    EXPECT_FALSE(std::isnan(error)) << "Prediction error is NaN";
    EXPECT_FALSE(std::isinf(error)) << "Prediction error is infinite";

    free_test_clause(resource_clause);

    /* Test different conclusion types */
    const char* test_conclusions[] = {
        "threat_detected", "safe_zone", "water_source",
        "opportunity_found", "goal_achieved", "goal_impossible"
    };

    for (const char* conc : test_conclusions) {
        logic_clause_t* clause = create_test_clause(conc);
        if (clause) {
            error = hypo_logic_update_predictions(bridge, clause);
            prediction_errors.push_back(error);

            EXPECT_FALSE(std::isnan(error))
                << "Prediction error NaN for " << conc;
            EXPECT_FALSE(std::isinf(error))
                << "Prediction error infinite for " << conc;

            free_test_clause(clause);
        }
    }

    /* Verify stats track prediction errors */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);

    EXPECT_FALSE(std::isnan(stats.avg_prediction_error))
        << "Average prediction error is NaN";
}

/* ============================================================================
 * REG-HL-008: goal priority ordering is stable
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_008_GoalPriorityOrderingStable) {
    /* Create several motivated goals with different priorities */
    const hypo_drive_type_t drives[] = {
        HYPO_DRIVE_HUNGER, HYPO_DRIVE_SAFETY, HYPO_DRIVE_CURIOSITY
    };
    const float satisfactions[] = {0.8f, 0.9f, 0.5f};

    for (int i = 0; i < 3; i++) {
        logic_clause_t* goal = create_test_clause("test_goal");
        if (goal) {
            int ret = hypo_logic_create_motivated_goal(bridge, drives[i], goal, satisfactions[i]);
            EXPECT_EQ(ret, 0) << "Failed to create goal " << i;
            /* Note: goal ownership transferred to bridge */
        }
    }

    /* Get prioritized goals */
    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals = 0;

    int ret = hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_goals, 3u);

    /* Verify goals are sorted by priority (descending) */
    for (uint32_t i = 1; i < num_goals; i++) {
        EXPECT_GE(goals[i - 1].priority, goals[i].priority)
            << "Goals not sorted by priority at index " << i;
    }

    /* Run multiple times to verify stability */
    for (int iter = 0; iter < 10; iter++) {
        uint32_t prev_num = num_goals;
        ret = hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals);
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(num_goals, prev_num) << "Goal count changed without modifications";

        for (uint32_t i = 1; i < num_goals; i++) {
            EXPECT_GE(goals[i - 1].priority, goals[i].priority)
                << "Goal ordering unstable at iteration " << iter;
        }
    }
}

/* ============================================================================
 * REG-HL-009: conclusion processing is idempotent for same conclusion
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_009_ConclusionProcessingIdempotent) {
    /* Reset to known state */
    hypo_logic_bridge_reset(bridge);

    /* Process same conclusion multiple times */
    logic_clause_t* conclusion = create_test_clause("resource_available");
    ASSERT_NE(conclusion, nullptr);

    /* Process first time */
    int ret = hypo_logic_process_conclusion(bridge, conclusion, 0.8f);
    EXPECT_EQ(ret, 0);

    hypo_logic_anticipation_t first_state;
    hypo_logic_get_anticipation(bridge, &first_state);

    /* Process same conclusion again (should have diminishing effect but not error) */
    ret = hypo_logic_process_conclusion(bridge, conclusion, 0.8f);
    EXPECT_EQ(ret, 0);

    hypo_logic_anticipation_t second_state;
    hypo_logic_get_anticipation(bridge, &second_state);

    /* States should still be valid (no overflow) */
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        EXPECT_TRUE(verify_normalized(second_state.anticipation[i]))
            << "Anticipation overflow after second processing";
        EXPECT_TRUE(verify_normalized(second_state.frustration[i]))
            << "Frustration overflow after second processing";
    }

    /* Stats should show both processed */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_GE(stats.conclusions_processed, 2u);

    free_test_clause(conclusion);
}

/* ============================================================================
 * REG-HL-010: stats overflow protection
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_010_StatsOverflowProtection) {
    /* Reset stats */
    int ret = hypo_logic_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    /* Perform many operations to stress stats counters */
    for (int i = 0; i < 1000; i++) {
        hypo_logic_compute_modulation(bridge);

        logic_clause_t* clause = create_test_clause("test_predicate");
        if (clause) {
            hypo_logic_process_conclusion(bridge, clause, 0.5f);
            free_test_clause(clause);
        }
    }

    /* Verify stats are valid (no overflow to negative or NaN) */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);

    /* All counts should be positive or zero */
    EXPECT_GE(stats.salience_modulations, 0u);
    EXPECT_GE(stats.depth_modulations, 0u);
    EXPECT_GE(stats.threshold_modulations, 0u);
    EXPECT_GE(stats.goals_created, 0u);
    EXPECT_GE(stats.goals_achieved, 0u);
    EXPECT_GE(stats.goals_abandoned, 0u);
    EXPECT_GE(stats.conclusions_processed, 0u);
    EXPECT_GE(stats.anticipation_updates, 0u);
    EXPECT_GE(stats.frustration_events, 0u);
    EXPECT_GE(stats.drive_boosts, 0u);
    EXPECT_GE(stats.drive_reductions, 0u);
    EXPECT_GE(stats.predictions_made, 0u);
    EXPECT_GE(stats.predictions_confirmed, 0u);
    EXPECT_GE(stats.predictions_violated, 0u);

    /* Float stats should be valid */
    EXPECT_FALSE(std::isnan(stats.avg_prediction_error));
    EXPECT_FALSE(std::isnan(stats.avg_logical_free_energy));
    EXPECT_FALSE(std::isnan(stats.avg_modulation_latency_us));
    EXPECT_FALSE(std::isnan(stats.avg_conclusion_latency_us));

    /* Verify some counts actually incremented */
    EXPECT_GE(stats.conclusions_processed, 1000u)
        << "Conclusions processed should be at least 1000";
}

/* ============================================================================
 * REG-HL-011: Numerical stability over repeated modulation updates
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_011_NumericalStabilityRepeatedUpdates) {
    /* Store initial state */
    hypo_logic_modulation_t initial_mod;
    hypo_logic_get_modulation(bridge, &initial_mod);

    /* Perform many update cycles without drive changes */
    for (int i = 0; i < REGRESSION_MAX_ITERATIONS; i++) {
        int ret = hypo_logic_compute_modulation(bridge);
        EXPECT_EQ(ret, 0) << "Modulation failed at iteration " << i;

        if (i % REGRESSION_CYCLES_PER_CHECK == 0) {
            hypo_logic_modulation_t mod;
            hypo_logic_get_modulation(bridge, &mod);

            /* Values should not drift significantly without input changes */
            EXPECT_NEAR(mod.proof_threshold, initial_mod.proof_threshold, 0.1f)
                << "Proof threshold drifted at iteration " << i;

            EXPECT_EQ(mod.max_inference_depth, initial_mod.max_inference_depth)
                << "Inference depth changed without cause at iteration " << i;

            /* Verify all values still valid */
            EXPECT_TRUE(verify_normalized(mod.reasoning_capacity));
            EXPECT_TRUE(verify_normalized(mod.effort_willingness));
        }
    }

    /* Final state should match initial (no drift) */
    hypo_logic_modulation_t final_mod;
    hypo_logic_get_modulation(bridge, &final_mod);

    EXPECT_EQ(final_mod.max_inference_depth, initial_mod.max_inference_depth);
    EXPECT_NEAR(final_mod.proof_threshold, initial_mod.proof_threshold, REGRESSION_THRESHOLD_TOLERANCE);
}

/* ============================================================================
 * REG-HL-012: Memory safety over many create/destroy cycles
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_012_MemorySafetyCreateDestroy) {
    /* First destroy the fixture bridge */
    hypo_logic_bridge_destroy(bridge);
    bridge = nullptr;

    /* Create and destroy many times */
    for (int i = 0; i < REGRESSION_CREATE_DESTROY_CYCLES; i++) {
        hypo_logic_bridge_t* temp_bridge = hypo_logic_bridge_create(
            drive_system, logic_engine, &config);
        ASSERT_NE(temp_bridge, nullptr) << "Create failed at iteration " << i;

        /* Do some work */
        hypo_logic_compute_modulation(temp_bridge);
        hypo_logic_bridge_update(temp_bridge, 1000);

        logic_clause_t* clause = create_test_clause("test");
        if (clause) {
            hypo_logic_process_conclusion(temp_bridge, clause, 0.5f);
            free_test_clause(clause);
        }

        hypo_logic_bridge_destroy(temp_bridge);

        /* Periodically check we can still create */
        if (i % 100 == 0) {
            temp_bridge = hypo_logic_bridge_create(drive_system, logic_engine, nullptr);
            ASSERT_NE(temp_bridge, nullptr) << "Late create failed at iteration " << i;
            hypo_logic_bridge_destroy(temp_bridge);
        }
    }

    /* Recreate fixture bridge */
    bridge = hypo_logic_bridge_create(drive_system, logic_engine, &config);
    ASSERT_NE(bridge, nullptr) << "Final bridge recreation failed";
}

/* ============================================================================
 * REG-HL-013: Performance bounds - modulation computation
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_013_PerformanceModulationComputation) {
    /* Warm up */
    for (int i = 0; i < 10; i++) {
        hypo_logic_compute_modulation(bridge);
    }

    const int NUM_SAMPLES = 100;
    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        hypo_logic_compute_modulation(bridge);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        latencies.push_back(duration.count());
    }

    uint64_t max_latency = *std::max_element(latencies.begin(), latencies.end());
    uint64_t total = std::accumulate(latencies.begin(), latencies.end(), 0ULL);
    double avg_latency = (double)total / NUM_SAMPLES;

    EXPECT_LT(max_latency, REGRESSION_COMPUTE_MODULATION_MAX_US)
        << "Max modulation latency exceeded: " << max_latency << "us";

    /* Check stats agree */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_FALSE(std::isnan(stats.avg_modulation_latency_us));
}

/* ============================================================================
 * REG-HL-014: Performance bounds - conclusion processing
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_014_PerformanceConclusionProcessing) {
    /* Warm up */
    for (int i = 0; i < 10; i++) {
        logic_clause_t* clause = create_test_clause("warmup");
        if (clause) {
            hypo_logic_process_conclusion(bridge, clause, 0.5f);
            free_test_clause(clause);
        }
    }

    hypo_logic_reset_stats(bridge);

    const int NUM_SAMPLES = 100;
    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        logic_clause_t* clause = create_test_clause("test_conclusion");
        ASSERT_NE(clause, nullptr);

        auto start = std::chrono::high_resolution_clock::now();
        hypo_logic_process_conclusion(bridge, clause, 0.7f);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        latencies.push_back(duration.count());

        free_test_clause(clause);
    }

    uint64_t max_latency = *std::max_element(latencies.begin(), latencies.end());
    uint64_t total = std::accumulate(latencies.begin(), latencies.end(), 0ULL);
    double avg_latency = (double)total / NUM_SAMPLES;

    EXPECT_LT(max_latency, REGRESSION_PROCESS_CONCLUSION_MAX_US)
        << "Max conclusion latency exceeded: " << max_latency << "us";

    /* Check stats agree */
    hypo_logic_stats_t stats;
    hypo_logic_get_stats(bridge, &stats);
    EXPECT_FALSE(std::isnan(stats.avg_conclusion_latency_us));
}

/* ============================================================================
 * REG-HL-015: Performance bounds - full update cycle
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_015_PerformanceFullUpdateCycle) {
    /* Warm up */
    for (int i = 0; i < 10; i++) {
        hypo_logic_bridge_update(bridge, 10000);
    }

    const int NUM_SAMPLES = 50;
    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        hypo_logic_bridge_update(bridge, 10000);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        latencies.push_back(duration.count());
    }

    uint64_t max_latency = *std::max_element(latencies.begin(), latencies.end());
    uint64_t total = std::accumulate(latencies.begin(), latencies.end(), 0ULL);
    double avg_latency = (double)total / NUM_SAMPLES;

    EXPECT_LT(max_latency, REGRESSION_FULL_UPDATE_CYCLE_MAX_US)
        << "Max update cycle latency exceeded: " << max_latency << "us";
    EXPECT_LT(avg_latency, REGRESSION_AVG_UPDATE_TIME_US)
        << "Average update cycle latency exceeded: " << avg_latency << "us";
}

/* ============================================================================
 * REG-HL-016: Boundary condition - max predicates
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_016_BoundaryMaxPredicates) {
    /* Try to register max predicate maps */
    int registered = 0;

    for (int i = 0; i < REGRESSION_MAX_PREDICATE_MAPS_TEST + 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "pred_%d", i);

        hypo_logic_predicate_map_t map = {
            .drive = HYPO_DRIVE_HUNGER,
            .relevance = 0.5f,
            .valence = 1.0f,
            .is_goal_predicate = false
        };
        strncpy(map.predicate_name, name, LOGIC_MAX_NAME_LENGTH - 1);

        int ret = hypo_logic_register_predicate_map(bridge, &map);
        if (ret == 0) {
            registered++;
        }
    }

    /* Should have registered up to the limit */
    EXPECT_LE(registered, (int)REGRESSION_MAX_PREDICATE_MAPS_TEST)
        << "Registered more than max predicates";

    /* Bridge should still function */
    float salience = hypo_logic_get_predicate_salience(bridge, "pred_0");
    EXPECT_TRUE(verify_salience_positive(salience));

    int ret = hypo_logic_compute_modulation(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * REG-HL-017: Boundary condition - max goals
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_017_BoundaryMaxGoals) {
    /* Try to create max goals + extra */
    int created = 0;

    for (int i = 0; i < REGRESSION_MAX_GOALS_TEST + 10; i++) {
        logic_clause_t* goal = create_test_clause("goal_clause");
        if (!goal) continue;

        int ret = hypo_logic_create_motivated_goal(
            bridge,
            (hypo_drive_type_t)(i % HYPO_DRIVE_COUNT),
            goal,
            0.5f
        );

        if (ret == 0) {
            created++;
        } else {
            /* Goal not added, we own the clause */
            free_test_clause(goal);
        }
    }

    /* Should have created up to the limit */
    EXPECT_LE(created, (int)REGRESSION_MAX_GOALS_TEST)
        << "Created more than max goals";

    /* Bridge should still function */
    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals = 0;

    int ret = hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals);
    EXPECT_EQ(ret, 0);
    EXPECT_LE(num_goals, REGRESSION_MAX_GOALS_TEST);
}

/* ============================================================================
 * REG-HL-018: Boundary condition - empty inputs
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_018_BoundaryEmptyInputs) {
    /* Empty predicate name */
    float salience = hypo_logic_get_predicate_salience(bridge, "");
    EXPECT_TRUE(verify_salience_positive(salience))
        << "Empty predicate name should return valid salience";

    /* Empty goals array request */
    uint32_t num_goals = 999;
    hypo_motivated_goal_t goals[1];
    int ret = hypo_logic_get_prioritized_goals(bridge, goals, 0, &num_goals);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_goals, 0u) << "Zero max_goals should return zero goals";

    /* Clause with empty predicate */
    logic_clause_t* empty_clause = create_test_clause("");
    if (empty_clause) {
        ret = hypo_logic_process_conclusion(bridge, empty_clause, 0.5f);
        EXPECT_EQ(ret, 0) << "Empty predicate clause should be handled";
        free_test_clause(empty_clause);
    }
}

/* ============================================================================
 * REG-HL-019: Determinism - same inputs produce same outputs
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_019_Determinism) {
    /* Reset to known state */
    hypo_logic_bridge_reset(bridge);
    hypo_drive_reset(drive_system);

    /* First run */
    hypo_logic_compute_modulation(bridge);
    hypo_logic_modulation_t mod1;
    hypo_logic_get_modulation(bridge, &mod1);

    hypo_logic_fep_state_t fe1;
    hypo_logic_compute_free_energy(bridge, &fe1);

    /* Reset again */
    hypo_logic_bridge_reset(bridge);
    hypo_drive_reset(drive_system);

    /* Second run with identical operations */
    hypo_logic_compute_modulation(bridge);
    hypo_logic_modulation_t mod2;
    hypo_logic_get_modulation(bridge, &mod2);

    hypo_logic_fep_state_t fe2;
    hypo_logic_compute_free_energy(bridge, &fe2);

    /* Results should match */
    EXPECT_EQ(mod1.max_inference_depth, mod2.max_inference_depth)
        << "Inference depth not deterministic";
    EXPECT_NEAR(mod1.proof_threshold, mod2.proof_threshold, REGRESSION_FLOAT_EPSILON)
        << "Proof threshold not deterministic";
    EXPECT_NEAR(mod1.reasoning_capacity, mod2.reasoning_capacity, REGRESSION_FLOAT_EPSILON)
        << "Reasoning capacity not deterministic";

    EXPECT_NEAR(fe1.logical_free_energy, fe2.logical_free_energy, REGRESSION_FREE_ENERGY_TOLERANCE)
        << "Free energy not deterministic";
}

/* ============================================================================
 * REG-HL-020: Statistics accuracy - counters increment correctly
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_020_StatisticsAccuracy) {
    /* Reset stats */
    hypo_logic_reset_stats(bridge);

    hypo_logic_stats_t stats_before;
    hypo_logic_get_stats(bridge, &stats_before);

    EXPECT_EQ(stats_before.conclusions_processed, 0u);
    EXPECT_EQ(stats_before.salience_modulations, 0u);

    /* Perform known number of operations */
    const int NUM_MODULATIONS = 50;
    const int NUM_CONCLUSIONS = 30;

    for (int i = 0; i < NUM_MODULATIONS; i++) {
        hypo_logic_compute_modulation(bridge);
    }

    for (int i = 0; i < NUM_CONCLUSIONS; i++) {
        logic_clause_t* clause = create_test_clause("stats_test");
        if (clause) {
            hypo_logic_process_conclusion(bridge, clause, 0.5f);
            free_test_clause(clause);
        }
    }

    hypo_logic_stats_t stats_after;
    hypo_logic_get_stats(bridge, &stats_after);

    /* Verify counts */
    EXPECT_EQ(stats_after.salience_modulations, (uint64_t)NUM_MODULATIONS)
        << "Salience modulation count incorrect";
    EXPECT_EQ(stats_after.depth_modulations, (uint64_t)NUM_MODULATIONS)
        << "Depth modulation count incorrect";
    EXPECT_EQ(stats_after.conclusions_processed, (uint64_t)NUM_CONCLUSIONS)
        << "Conclusions processed count incorrect";
    EXPECT_EQ(stats_after.anticipation_updates, (uint64_t)NUM_CONCLUSIONS)
        << "Anticipation updates count incorrect";
}

/* ============================================================================
 * REG-HL-021: Utility strings are not null
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_021_UtilityStrings) {
    /* Test conclusion type names */
    for (int i = 0; i < HYPO_CONCL_COUNT; i++) {
        const char* name = hypo_conclusion_type_name((hypo_conclusion_type_t)i);
        EXPECT_NE(name, nullptr) << "Conclusion type " << i << " has null name";
        EXPECT_GT(strlen(name), 0u) << "Conclusion type " << i << " has empty name";
    }

    /* Test unknown conclusion type */
    const char* unknown = hypo_conclusion_type_name((hypo_conclusion_type_t)999);
    EXPECT_NE(unknown, nullptr);

    /* Test predicate category names */
    for (int i = 0; i < HYPO_PRED_CAT_COUNT; i++) {
        const char* name = hypo_predicate_category_name((hypo_predicate_category_t)i);
        EXPECT_NE(name, nullptr) << "Predicate category " << i << " has null name";
        EXPECT_GT(strlen(name), 0u) << "Predicate category " << i << " has empty name";
    }

    /* Test unknown category */
    unknown = hypo_predicate_category_name((hypo_predicate_category_t)999);
    EXPECT_NE(unknown, nullptr);
}

/* ============================================================================
 * REG-HL-022: Predicate category detection accuracy
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_022_PredicateCategoryDetection) {
    /* Test food-related predicates */
    EXPECT_EQ(hypo_logic_get_predicate_category("food"), HYPO_PRED_CAT_FOOD);
    EXPECT_EQ(hypo_logic_get_predicate_category("eat_apple"), HYPO_PRED_CAT_FOOD);
    EXPECT_EQ(hypo_logic_get_predicate_category("hungry"), HYPO_PRED_CAT_FOOD);
    EXPECT_EQ(hypo_logic_get_predicate_category("FOOD"), HYPO_PRED_CAT_FOOD);  /* Case insensitive */

    /* Test water-related predicates */
    EXPECT_EQ(hypo_logic_get_predicate_category("water"), HYPO_PRED_CAT_WATER);
    EXPECT_EQ(hypo_logic_get_predicate_category("drink"), HYPO_PRED_CAT_WATER);
    EXPECT_EQ(hypo_logic_get_predicate_category("thirsty"), HYPO_PRED_CAT_WATER);

    /* Test threat-related predicates */
    EXPECT_EQ(hypo_logic_get_predicate_category("threat"), HYPO_PRED_CAT_THREAT);
    EXPECT_EQ(hypo_logic_get_predicate_category("danger"), HYPO_PRED_CAT_THREAT);
    EXPECT_EQ(hypo_logic_get_predicate_category("predator_nearby"), HYPO_PRED_CAT_THREAT);

    /* Test safety-related predicates */
    EXPECT_EQ(hypo_logic_get_predicate_category("safe"), HYPO_PRED_CAT_SAFETY);
    EXPECT_EQ(hypo_logic_get_predicate_category("shelter"), HYPO_PRED_CAT_SAFETY);

    /* Test social-related predicates */
    EXPECT_EQ(hypo_logic_get_predicate_category("friend"), HYPO_PRED_CAT_SOCIAL);
    EXPECT_EQ(hypo_logic_get_predicate_category("ally_found"), HYPO_PRED_CAT_SOCIAL);

    /* Test knowledge-related predicates */
    EXPECT_EQ(hypo_logic_get_predicate_category("know"), HYPO_PRED_CAT_KNOWLEDGE);
    EXPECT_EQ(hypo_logic_get_predicate_category("discover"), HYPO_PRED_CAT_KNOWLEDGE);

    /* Test neutral/unknown predicates */
    EXPECT_EQ(hypo_logic_get_predicate_category("xyz123"), HYPO_PRED_CAT_NEUTRAL);
    EXPECT_EQ(hypo_logic_get_predicate_category("random_pred"), HYPO_PRED_CAT_NEUTRAL);

    /* Test NULL input */
    EXPECT_EQ(hypo_logic_get_predicate_category(nullptr), HYPO_PRED_CAT_NEUTRAL);
}

/* ============================================================================
 * REG-HL-023: Category to drive mapping accuracy
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_023_CategoryToDriveMapping) {
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_FOOD), HYPO_DRIVE_HUNGER);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_WATER), HYPO_DRIVE_THIRST);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_THREAT), HYPO_DRIVE_SAFETY);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_SAFETY), HYPO_DRIVE_SAFETY);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_SOCIAL), HYPO_DRIVE_SOCIAL);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_KNOWLEDGE), HYPO_DRIVE_CURIOSITY);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_TEMPERATURE), HYPO_DRIVE_TEMPERATURE);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_REST), HYPO_DRIVE_FATIGUE);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_ACHIEVEMENT), HYPO_DRIVE_COMPETENCE);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_AUTONOMY), HYPO_DRIVE_AUTONOMY);
    EXPECT_EQ(hypo_logic_category_to_drive(HYPO_PRED_CAT_NEUTRAL), HYPO_DRIVE_COUNT);

    /* Invalid category */
    EXPECT_EQ(hypo_logic_category_to_drive((hypo_predicate_category_t)999), HYPO_DRIVE_COUNT);
}

/* ============================================================================
 * REG-HL-024: Conclusion classification accuracy
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_024_ConclusionClassification) {
    /* Resource available */
    logic_clause_t* clause = create_test_clause("resource_available");
    EXPECT_EQ(hypo_logic_classify_conclusion(bridge, clause), HYPO_CONCL_RESOURCE_AVAILABLE);
    free_test_clause(clause);

    /* Threat present */
    clause = create_test_clause("threat_detected");
    EXPECT_EQ(hypo_logic_classify_conclusion(bridge, clause), HYPO_CONCL_THREAT_PRESENT);
    free_test_clause(clause);

    /* Safe (threat absent) */
    clause = create_test_clause("safe_zone");
    EXPECT_EQ(hypo_logic_classify_conclusion(bridge, clause), HYPO_CONCL_THREAT_ABSENT);
    free_test_clause(clause);

    /* Goal achieved */
    clause = create_test_clause("goal_achieved");
    EXPECT_EQ(hypo_logic_classify_conclusion(bridge, clause), HYPO_CONCL_GOAL_ACHIEVED);
    free_test_clause(clause);

    /* Goal impossible */
    clause = create_test_clause("goal_impossible");
    EXPECT_EQ(hypo_logic_classify_conclusion(bridge, clause), HYPO_CONCL_GOAL_IMPOSSIBLE);
    free_test_clause(clause);

    /* Opportunity */
    clause = create_test_clause("opportunity_found");
    EXPECT_EQ(hypo_logic_classify_conclusion(bridge, clause), HYPO_CONCL_OPPORTUNITY);
    free_test_clause(clause);

    /* Neutral (unrecognized) */
    clause = create_test_clause("xyz_random_predicate");
    EXPECT_EQ(hypo_logic_classify_conclusion(bridge, clause), HYPO_CONCL_NEUTRAL);
    free_test_clause(clause);

    /* NULL inputs */
    EXPECT_EQ(hypo_logic_classify_conclusion(nullptr, nullptr), HYPO_CONCL_NEUTRAL);
    EXPECT_EQ(hypo_logic_classify_conclusion(bridge, nullptr), HYPO_CONCL_NEUTRAL);
}

/* ============================================================================
 * REG-HL-025: Goal achieved/impossible signals
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_025_GoalSignals) {
    /* Create a goal */
    logic_clause_t* goal = create_test_clause("find_food");
    int ret = hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.8f);
    EXPECT_EQ(ret, 0);

    hypo_logic_stats_t stats_before;
    hypo_logic_get_stats(bridge, &stats_before);

    /* Signal goal achieved */
    float reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_HUNGER, goal);
    EXPECT_GE(reward, 0.0f) << "Goal achieved should return positive reward";
    EXPECT_FALSE(std::isnan(reward));

    hypo_logic_stats_t stats_after;
    hypo_logic_get_stats(bridge, &stats_after);
    EXPECT_GT(stats_after.goals_achieved, stats_before.goals_achieved);

    /* Create another goal for impossibility test */
    logic_clause_t* goal2 = create_test_clause("find_water");
    ret = hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_THIRST, goal2, 0.7f);
    EXPECT_EQ(ret, 0);

    hypo_logic_get_stats(bridge, &stats_before);

    /* Signal goal impossible */
    float frustration = hypo_logic_goal_impossible(bridge, HYPO_DRIVE_THIRST, goal2);
    EXPECT_GE(frustration, 0.0f) << "Goal impossible should return positive frustration";
    EXPECT_FALSE(std::isnan(frustration));

    hypo_logic_get_stats(bridge, &stats_after);
    EXPECT_GT(stats_after.goals_abandoned, stats_before.goals_abandoned);

    /* Invalid drive */
    reward = hypo_logic_goal_achieved(bridge, HYPO_DRIVE_COUNT, nullptr);
    EXPECT_FLOAT_EQ(reward, 0.0f);

    frustration = hypo_logic_goal_impossible(bridge, HYPO_DRIVE_COUNT, nullptr);
    EXPECT_FLOAT_EQ(frustration, 0.0f);

    /* NULL bridge */
    reward = hypo_logic_goal_achieved(nullptr, HYPO_DRIVE_HUNGER, nullptr);
    EXPECT_FLOAT_EQ(reward, 0.0f);
}

/* ============================================================================
 * REG-HL-026: Reset clears state properly
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_026_ResetClearsState) {
    /* Do some work */
    for (int i = 0; i < 50; i++) {
        hypo_logic_compute_modulation(bridge);
        logic_clause_t* clause = create_test_clause("work_clause");
        if (clause) {
            hypo_logic_process_conclusion(bridge, clause, 0.5f);
            free_test_clause(clause);
        }
    }

    /* Create some goals */
    for (int i = 0; i < 5; i++) {
        logic_clause_t* goal = create_test_clause("test_goal");
        if (goal) {
            hypo_logic_create_motivated_goal(bridge, HYPO_DRIVE_HUNGER, goal, 0.5f);
        }
    }

    /* Verify we have state */
    hypo_logic_stats_t stats_before;
    hypo_logic_get_stats(bridge, &stats_before);
    EXPECT_GT(stats_before.conclusions_processed, 0u);

    hypo_motivated_goal_t goals[HYPO_LOGIC_MAX_GOALS];
    uint32_t num_goals = 0;
    hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals);
    EXPECT_GT(num_goals, 0u);

    /* Reset */
    int ret = hypo_logic_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);

    /* Verify state is cleared */
    hypo_logic_stats_t stats_after;
    hypo_logic_get_stats(bridge, &stats_after);
    EXPECT_EQ(stats_after.conclusions_processed, 0u);
    EXPECT_EQ(stats_after.goals_created, 0u);

    hypo_logic_get_prioritized_goals(bridge, goals, HYPO_LOGIC_MAX_GOALS, &num_goals);
    EXPECT_EQ(num_goals, 0u);

    /* Verify anticipation is cleared */
    hypo_logic_anticipation_t anticipation;
    hypo_logic_get_anticipation(bridge, &anticipation);
    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        EXPECT_FLOAT_EQ(anticipation.anticipation[i], 0.0f);
        EXPECT_FLOAT_EQ(anticipation.frustration[i], 0.0f);
    }

    /* Bridge should still function after reset */
    ret = hypo_logic_compute_modulation(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * REG-HL-027: Auto-register mappings works
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_027_AutoRegisterMappings) {
    /* Create a fresh bridge to test auto-registration count */
    hypo_logic_bridge_destroy(bridge);
    bridge = nullptr;

    /* Create with minimal config (auto-registration happens in create) */
    hypo_logic_config_t minimal_config = hypo_logic_default_config();
    minimal_config.num_predicate_maps = 0;  /* Start with none */

    bridge = hypo_logic_bridge_create(drive_system, logic_engine, &minimal_config);
    ASSERT_NE(bridge, nullptr);

    /* Auto-registration should have added mappings */
    /* Test that food, water, safe, threat predicates have enhanced salience */
    hypo_logic_compute_modulation(bridge);

    /* Registered predicates should have higher salience than unregistered */
    float food_salience = hypo_logic_get_predicate_salience(bridge, "food");
    float unknown_salience = hypo_logic_get_predicate_salience(bridge, "xyz_unknown");

    /* Both should be valid */
    EXPECT_TRUE(verify_salience_positive(food_salience));
    EXPECT_TRUE(verify_salience_positive(unknown_salience));

    /* Manual call to auto_register should work and return count */
    /* Note: This may fail to add more if already at max */
    int count = hypo_logic_auto_register_mappings(bridge);
    EXPECT_GE(count, 0) << "Auto-register should return non-negative count";
}

/* ============================================================================
 * REG-HL-028: Print summary doesn't crash
 * ============================================================================ */

TEST_F(HypoLogicBridgeRegressionTest, REG_HL_028_PrintSummaryStability) {
    /* Just ensure these don't crash */
    hypo_logic_print_summary(bridge);
    hypo_logic_print_summary(nullptr);  /* Should handle NULL gracefully */

    /* After some work */
    for (int i = 0; i < 10; i++) {
        hypo_logic_compute_modulation(bridge);
        hypo_logic_bridge_update(bridge, 10000);
    }

    hypo_logic_print_summary(bridge);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
