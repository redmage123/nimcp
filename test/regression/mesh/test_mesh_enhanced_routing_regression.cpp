/**
 * @file test_mesh_enhanced_routing_regression.cpp
 * @brief Regression tests for Enhanced Pattern Routing
 *
 * WHAT: Tests that SAT solver and pattern cache behavior remains stable
 * WHY:  Prevent regressions in constraint solving and caching logic
 * HOW:  Golden test cases with known correct outputs
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_pattern_cache.h"
#include "mesh/nimcp_mesh_sat_solver.h"
#include "mesh/nimcp_mesh_endorsement.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshEnhancedRoutingRegressionTest : public ::testing::Test {
protected:
    pattern_cache_t* cache = nullptr;
    sat_solver_t* solver = nullptr;

    void SetUp() override {
        pattern_cache_config_t cache_cfg;
        pattern_cache_default_config(&cache_cfg);
        cache_cfg.max_entries = 256;
        cache_cfg.enable_logging = false;
        cache = pattern_cache_create(&cache_cfg);
        ASSERT_NE(cache, nullptr);

        sat_solver_config_t sat_cfg;
        sat_solver_default_config(&sat_cfg);
        sat_cfg.enable_logging = false;
        solver = sat_solver_create(&sat_cfg);
        ASSERT_NE(solver, nullptr);
    }

    void TearDown() override {
        if (solver) sat_solver_destroy(solver);
        if (cache) pattern_cache_destroy(cache);
    }

    mesh_pattern_t create_pattern(const float* values, size_t count) {
        mesh_pattern_t pattern;
        mesh_pattern_init(&pattern);
        float magnitude = 0.0f;
        for (size_t i = 0; i < count && i < MESH_PATTERN_DIM; i++) {
            pattern.vector[i] = values[i];
            magnitude += values[i] * values[i];
        }
        pattern.magnitude = sqrtf(magnitude);
        pattern.active_dims = (uint32_t)count;
        return pattern;
    }
};

/* ============================================================================
 * SAT Solver Golden Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingRegressionTest, SAT_SimpleSatisfiability) {
    /* Golden test: x ∧ ¬y should be satisfiable with x=true, y=false */
    uint32_t x, y;
    ASSERT_EQ(sat_solver_add_variable(solver, "x", 0, &x), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "y", 0, &y), NIMCP_SUCCESS);

    sat_solver_add_unit(solver, sat_make_literal(x, false));
    sat_solver_add_unit(solver, sat_make_literal(y, true));

    sat_result_t result = sat_solver_solve(solver);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    EXPECT_EQ(sat_solver_get_value(solver, x), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, y), SAT_VALUE_FALSE);
}

TEST_F(MeshEnhancedRoutingRegressionTest, SAT_SimpleUnsatisfiability) {
    /* Golden test: x ∧ ¬x is unsatisfiable */
    uint32_t x;
    ASSERT_EQ(sat_solver_add_variable(solver, "x", 0, &x), NIMCP_SUCCESS);

    sat_solver_add_unit(solver, sat_make_literal(x, false));
    sat_solver_add_unit(solver, sat_make_literal(x, true));

    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_UNSATISFIABLE);
}

TEST_F(MeshEnhancedRoutingRegressionTest, SAT_ImplicationChain) {
    /* Golden test: a → b → c, with a=true forces b=true, c=true */
    uint32_t a, b, c;
    sat_solver_add_variable(solver, "a", 0, &a);
    sat_solver_add_variable(solver, "b", 0, &b);
    sat_solver_add_variable(solver, "c", 0, &c);

    sat_solver_add_implication(solver, sat_make_literal(a, false),
                               sat_make_literal(b, false));
    sat_solver_add_implication(solver, sat_make_literal(b, false),
                               sat_make_literal(c, false));
    sat_solver_add_unit(solver, sat_make_literal(a, false));

    sat_result_t result = sat_solver_solve(solver);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    EXPECT_EQ(sat_solver_get_value(solver, a), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, b), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, c), SAT_VALUE_TRUE);
}

TEST_F(MeshEnhancedRoutingRegressionTest, SAT_AtLeastTwoOfThree) {
    /* Golden test: at-least-2 of {x,y,z} */
    uint32_t x, y, z;
    sat_solver_add_variable(solver, "x", 0, &x);
    sat_solver_add_variable(solver, "y", 0, &y);
    sat_solver_add_variable(solver, "z", 0, &z);

    sat_literal_t lits[3] = {
        sat_make_literal(x, false),
        sat_make_literal(y, false),
        sat_make_literal(z, false)
    };
    sat_solver_add_at_least_k(solver, lits, 3, 2);

    sat_result_t result = sat_solver_solve(solver);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    int true_count = 0;
    if (sat_solver_get_value(solver, x) == SAT_VALUE_TRUE) true_count++;
    if (sat_solver_get_value(solver, y) == SAT_VALUE_TRUE) true_count++;
    if (sat_solver_get_value(solver, z) == SAT_VALUE_TRUE) true_count++;
    EXPECT_GE(true_count, 2);
}

TEST_F(MeshEnhancedRoutingRegressionTest, SAT_ExactlyOneOfThree) {
    /* Golden test: exactly-1 of {x,y,z} */
    uint32_t x, y, z;
    sat_solver_add_variable(solver, "x", 0, &x);
    sat_solver_add_variable(solver, "y", 0, &y);
    sat_solver_add_variable(solver, "z", 0, &z);

    sat_literal_t lits[3] = {
        sat_make_literal(x, false),
        sat_make_literal(y, false),
        sat_make_literal(z, false)
    };
    sat_solver_add_exactly_k(solver, lits, 3, 1);

    sat_result_t result = sat_solver_solve(solver);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    int true_count = 0;
    if (sat_solver_get_value(solver, x) == SAT_VALUE_TRUE) true_count++;
    if (sat_solver_get_value(solver, y) == SAT_VALUE_TRUE) true_count++;
    if (sat_solver_get_value(solver, z) == SAT_VALUE_TRUE) true_count++;
    EXPECT_EQ(true_count, 1);
}

TEST_F(MeshEnhancedRoutingRegressionTest, SAT_PolicyExpression_ANDWithOR) {
    /* Golden test: "pfc AND (motor OR safety)" */

    uint32_t pfc, motor, safety;
    sat_solver_add_variable(solver, "pfc", 0x100, &pfc);
    sat_solver_add_variable(solver, "motor", 0x200, &motor);
    sat_solver_add_variable(solver, "safety", 0x300, &safety);

    sat_solver_add_unit(solver, sat_make_literal(pfc, false));

    sat_literal_t or_clause[2] = {
        sat_make_literal(motor, false),
        sat_make_literal(safety, false)
    };
    sat_solver_add_clause(solver, or_clause, 2, 1.0f);

    sat_result_t result = sat_solver_solve(solver);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    EXPECT_EQ(sat_solver_get_value(solver, pfc), SAT_VALUE_TRUE);
    bool motor_or_safety =
        sat_solver_get_value(solver, motor) == SAT_VALUE_TRUE ||
        sat_solver_get_value(solver, safety) == SAT_VALUE_TRUE;
    EXPECT_TRUE(motor_or_safety);
}

/* ============================================================================
 * Pattern Cache Golden Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingRegressionTest, Cache_HashDeterminism) {
    /* Golden: Same pattern always produces same hash */
    float pv[] = {0.5f, 0.5f, 0.5f, 0.5f};
    mesh_pattern_t pattern = create_pattern(pv, 4);

    pattern_hash_t hash1, hash2, hash3;
    ASSERT_EQ(pattern_cache_hash(&pattern, &hash1), NIMCP_SUCCESS);
    ASSERT_EQ(pattern_cache_hash(&pattern, &hash2), NIMCP_SUCCESS);
    ASSERT_EQ(pattern_cache_hash(&pattern, &hash3), NIMCP_SUCCESS);

    EXPECT_TRUE(pattern_hash_equals(&hash1, &hash2));
    EXPECT_TRUE(pattern_hash_equals(&hash2, &hash3));
    EXPECT_TRUE(pattern_hash_equals(&hash1, &hash3));
}

TEST_F(MeshEnhancedRoutingRegressionTest, Cache_HashUniqueness) {
    /* Golden: Different patterns produce different hashes */
    float pv1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float pv2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float pv3[] = {0.5f, 0.5f, 0.0f, 0.0f};

    mesh_pattern_t p1 = create_pattern(pv1, 4);
    mesh_pattern_t p2 = create_pattern(pv2, 4);
    mesh_pattern_t p3 = create_pattern(pv3, 4);

    pattern_hash_t h1, h2, h3;
    pattern_cache_hash(&p1, &h1);
    pattern_cache_hash(&p2, &h2);
    pattern_cache_hash(&p3, &h3);

    EXPECT_FALSE(pattern_hash_equals(&h1, &h2));
    EXPECT_FALSE(pattern_hash_equals(&h2, &h3));
    EXPECT_FALSE(pattern_hash_equals(&h1, &h3));
}

TEST_F(MeshEnhancedRoutingRegressionTest, Cache_StoreRetrieveExact) {
    /* Golden: Stored data retrieved exactly */
    float pv[] = {0.1f, 0.2f, 0.3f, 0.4f};
    mesh_pattern_t pattern = create_pattern(pv, 4);

    cached_activation_t stored[2];
    stored[0].module_id = 0x123;
    stored[0].activation_level = 0.875f;
    stored[0].similarity = 0.925f;
    stored[0].role = ENDORSER_ROLE_REQUIRED;
    stored[0].should_endorse = true;

    stored[1].module_id = 0x456;
    stored[1].activation_level = 0.625f;
    stored[1].similarity = 0.750f;
    stored[1].role = ENDORSER_ROLE_OPTIONAL;
    stored[1].should_endorse = false;

    ASSERT_EQ(pattern_cache_store(cache, &pattern, stored, 2, 0), NIMCP_SUCCESS);

    cached_activation_t retrieved[2];
    size_t count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, retrieved, 2, &count), NIMCP_SUCCESS);
    ASSERT_EQ(count, 2u);

    for (size_t i = 0; i < 2; i++) {
        if (retrieved[i].module_id == 0x123) {
            EXPECT_FLOAT_EQ(retrieved[i].activation_level, 0.875f);
            EXPECT_FLOAT_EQ(retrieved[i].similarity, 0.925f);
            EXPECT_EQ(retrieved[i].role, ENDORSER_ROLE_REQUIRED);
            EXPECT_TRUE(retrieved[i].should_endorse);
        } else if (retrieved[i].module_id == 0x456) {
            EXPECT_FLOAT_EQ(retrieved[i].activation_level, 0.625f);
            EXPECT_FLOAT_EQ(retrieved[i].similarity, 0.750f);
            EXPECT_EQ(retrieved[i].role, ENDORSER_ROLE_OPTIONAL);
            EXPECT_FALSE(retrieved[i].should_endorse);
        } else {
            FAIL() << "Unexpected module ID: " << retrieved[i].module_id;
        }
    }
}

TEST_F(MeshEnhancedRoutingRegressionTest, Cache_StatsAccuracy) {
    /* Golden: Cache stats are accurate */
    float pv1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float pv2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    mesh_pattern_t p1 = create_pattern(pv1, 4);
    mesh_pattern_t p2 = create_pattern(pv2, 4);

    cached_activation_t act;
    act.module_id = 0x100;
    act.activation_level = 0.5f;
    act.similarity = 0.5f;
    act.role = ENDORSER_ROLE_OPTIONAL;
    act.should_endorse = true;

    cached_activation_t tmp[1];
    size_t cnt = 0;
    pattern_cache_lookup(cache, &p1, tmp, 1, &cnt);  /* Miss */

    pattern_cache_store(cache, &p1, &act, 1, 0);

    pattern_cache_lookup(cache, &p1, tmp, 1, &cnt);  /* Hit */

    pattern_cache_lookup(cache, &p2, tmp, 1, &cnt);  /* Miss */

    pattern_cache_stats_t stats;
    pattern_cache_get_stats(cache, &stats);

    EXPECT_EQ(stats.hits, 1u);
    EXPECT_EQ(stats.misses, 2u);
    EXPECT_EQ(stats.current_entries, 1u);
}

/* ============================================================================
 * Combined Regression Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingRegressionTest, Combined_CognitiveEndorsement) {
    uint32_t v_pfc, v_hip, v_amy;
    sat_solver_add_variable(solver, "pfc", 0x100, &v_pfc);
    sat_solver_add_variable(solver, "hippocampus", 0x200, &v_hip);
    sat_solver_add_variable(solver, "amygdala", 0x300, &v_amy);

    sat_solver_add_unit(solver, sat_make_literal(v_pfc, false));

    sat_literal_t emotional_memory[2] = {
        sat_make_literal(v_hip, false),
        sat_make_literal(v_amy, false)
    };
    sat_solver_add_at_least_k(solver, emotional_memory, 2, 1);

    for (int trial = 0; trial < 100; trial++) {
        sat_solver_reset(solver);
        sat_solver_add_variable(solver, "pfc", 0x100, &v_pfc);
        sat_solver_add_variable(solver, "hippocampus", 0x200, &v_hip);
        sat_solver_add_variable(solver, "amygdala", 0x300, &v_amy);

        sat_solver_add_unit(solver, sat_make_literal(v_pfc, false));
        sat_solver_add_at_least_k(solver, emotional_memory, 2, 1);

        sat_result_t result = sat_solver_solve(solver);
        ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

        EXPECT_EQ(sat_solver_get_value(solver, v_pfc), SAT_VALUE_TRUE);

        bool has_emotional_memory =
            sat_solver_get_value(solver, v_hip) == SAT_VALUE_TRUE ||
            sat_solver_get_value(solver, v_amy) == SAT_VALUE_TRUE;
        EXPECT_TRUE(has_emotional_memory);
    }
}

TEST_F(MeshEnhancedRoutingRegressionTest, Combined_MotorSafetyInterlock) {
    uint32_t v_motor, v_safety;
    sat_solver_add_variable(solver, "motor", 0x100, &v_motor);
    sat_solver_add_variable(solver, "safety", 0x200, &v_safety);

    sat_solver_add_implication(solver,
        sat_make_literal(v_motor, false),
        sat_make_literal(v_safety, false));

    sat_solver_add_unit(solver, sat_make_literal(v_motor, false));

    sat_result_t result = sat_solver_solve(solver);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    EXPECT_EQ(sat_solver_get_value(solver, v_motor), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_safety), SAT_VALUE_TRUE);
}

TEST_F(MeshEnhancedRoutingRegressionTest, Combined_CacheConsistency) {
    float pv[] = {0.5f, 0.5f, 0.5f, 0.5f};
    mesh_pattern_t pattern = create_pattern(pv, 4);

    cached_activation_t initial;
    initial.module_id = 0x100;
    initial.activation_level = 0.7f;
    initial.similarity = 0.8f;
    initial.role = ENDORSER_ROLE_REQUIRED;
    initial.should_endorse = true;

    pattern_cache_store(cache, &pattern, &initial, 1, 0);

    for (int i = 0; i < 50; i++) {
        cached_activation_t retrieved;
        size_t count = 0;
        ASSERT_EQ(pattern_cache_lookup(cache, &pattern, &retrieved, 1, &count), NIMCP_SUCCESS);
        ASSERT_EQ(count, 1u);

        EXPECT_EQ(retrieved.module_id, 0x100u);
        EXPECT_FLOAT_EQ(retrieved.activation_level, 0.7f);
        EXPECT_FLOAT_EQ(retrieved.similarity, 0.8f);
        EXPECT_EQ(retrieved.role, ENDORSER_ROLE_REQUIRED);
        EXPECT_TRUE(retrieved.should_endorse);
    }

    cached_activation_t updated;
    updated.module_id = 0x100;
    updated.activation_level = 0.9f;
    updated.similarity = 0.95f;
    updated.role = ENDORSER_ROLE_REQUIRED;
    updated.should_endorse = true;

    pattern_cache_store(cache, &pattern, &updated, 1, 0);

    cached_activation_t after_update;
    size_t cnt = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, &after_update, 1, &cnt), NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(after_update.activation_level, 0.9f);
    EXPECT_FLOAT_EQ(after_update.similarity, 0.95f);
}

/* ============================================================================
 * Edge Case Regression Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingRegressionTest, EdgeCase_EmptySolver) {
    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);
}

TEST_F(MeshEnhancedRoutingRegressionTest, EdgeCase_SingleVariable) {
    uint32_t x;
    sat_solver_add_variable(solver, "x", 0, &x);

    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);

    /* Unconstrained variables may remain unassigned - all three values are valid */
    sat_value_t val = sat_solver_get_value(solver, x);
    EXPECT_TRUE(val == SAT_VALUE_TRUE || val == SAT_VALUE_FALSE || val == SAT_VALUE_UNASSIGNED);
}

TEST_F(MeshEnhancedRoutingRegressionTest, EdgeCase_ZeroPattern) {
    float pv[] = {0.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);

    cached_activation_t act;
    act.module_id = 0x100;
    act.activation_level = 0.0f;
    act.similarity = 0.0f;
    act.role = ENDORSER_ROLE_OPTIONAL;
    act.should_endorse = false;

    ASSERT_EQ(pattern_cache_store(cache, &pattern, &act, 1, 0), NIMCP_SUCCESS);

    cached_activation_t retrieved;
    size_t count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, &retrieved, 1, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(retrieved.activation_level, 0.0f);
}

TEST_F(MeshEnhancedRoutingRegressionTest, EdgeCase_MaxDimensionPattern) {
    float pv[MESH_PATTERN_DIM];
    for (size_t i = 0; i < MESH_PATTERN_DIM; i++) {
        pv[i] = (float)i / (float)MESH_PATTERN_DIM;
    }
    mesh_pattern_t pattern = create_pattern(pv, MESH_PATTERN_DIM);

    cached_activation_t act;
    act.module_id = 0x100;
    act.activation_level = 0.5f;
    act.similarity = 0.5f;
    act.role = ENDORSER_ROLE_OPTIONAL;
    act.should_endorse = true;

    ASSERT_EQ(pattern_cache_store(cache, &pattern, &act, 1, 0), NIMCP_SUCCESS);

    cached_activation_t retrieved;
    size_t count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, &retrieved, 1, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
}
