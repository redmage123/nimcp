/**
 * @file test_mesh_enhanced_routing_integration.cpp
 * @brief Integration tests for Enhanced Pattern Routing
 *
 * WHAT: Tests SAT solver and pattern cache working together
 * WHY:  Validates that endorsement constraint solving integrates with
 *       pattern caching for efficient routing decisions
 * HOW:  Creates realistic routing scenarios with multiple modules,
 *       tests that caching accelerates repeated queries, and
 *       verifies SAT-based endorser selection
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>

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

class MeshEnhancedRoutingIntegrationTest : public ::testing::Test {
protected:
    mesh_pattern_router_t* router = nullptr;
    pattern_cache_t* cache = nullptr;
    sat_solver_t* solver = nullptr;

    void SetUp() override {
        /* Create pattern router */
        mesh_pattern_router_config_t router_cfg;
        memset(&router_cfg, 0, sizeof(router_cfg));
        router_cfg.default_threshold = 0.3f;
        router_cfg.competition_strength = 0.5f;
        router_cfg.max_endorsers = 16;
        router = mesh_pattern_router_create(&router_cfg);
        ASSERT_NE(router, nullptr);

        /* Create pattern cache */
        pattern_cache_config_t cache_cfg;
        pattern_cache_default_config(&cache_cfg);
        cache_cfg.max_entries = 256;
        cache_cfg.enable_cow = true;
        cache_cfg.enable_logging = false;
        cache = pattern_cache_create(&cache_cfg);
        ASSERT_NE(cache, nullptr);

        /* Create SAT solver */
        sat_solver_config_t sat_cfg;
        sat_solver_default_config(&sat_cfg);
        sat_cfg.enable_logging = false;
        solver = sat_solver_create(&sat_cfg);
        ASSERT_NE(solver, nullptr);
    }

    void TearDown() override {
        if (solver) sat_solver_destroy(solver);
        if (cache) pattern_cache_destroy(cache);
        if (router) mesh_pattern_router_destroy(router);
    }

    /* Create a pattern with specific values */
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

    /* Create a transaction from a pattern */
    mesh_pattern_transaction_t create_transaction(const mesh_pattern_t& pattern) {
        mesh_pattern_transaction_t tx;
        memset(&tx, 0, sizeof(tx));
        tx.content_pattern = pattern;
        tx.context_pattern = pattern;
        tx.goal_pattern = pattern;
        tx.urgency = 0.5f;
        tx.novelty = 0.5f;
        return tx;
    }

    /* Create a receptive field with a single preferred pattern */
    mesh_receptive_field_t create_field(const mesh_pattern_t& pattern, float threshold) {
        mesh_receptive_field_t field;
        mesh_receptive_field_init(&field);
        field.preferred[0] = pattern;
        field.pattern_count = 1;
        field.threshold = threshold;
        field.sharpness = 1.0f;
        return field;
    }

    /* Register a module with the router */
    void register_module(mesh_participant_id_t id, const float* values, size_t dim, float threshold) {
        mesh_pattern_t pattern = create_pattern(values, dim);
        mesh_receptive_field_t field = create_field(pattern, threshold);
        mesh_pattern_router_register_receptive_field(router, id, &field);
    }
};

/* ============================================================================
 * Basic Integration Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingIntegrationTest, RouteAndCache) {
    /* Register modules */
    float center1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float center2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float center3[] = {0.0f, 0.0f, 1.0f, 0.0f};

    register_module(0x100, center1, 4, 0.3f);
    register_module(0x200, center2, 4, 0.3f);
    register_module(0x300, center3, 4, 0.3f);

    /* Create test pattern */
    float pv[] = {0.8f, 0.2f, 0.1f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(pattern);

    /* Check cache first (should miss) */
    cached_activation_t cached[16];
    size_t cached_count = 0;
    nimcp_error_t err = pattern_cache_lookup(cache, &pattern, cached, 16, &cached_count);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);

    /* Route pattern */
    mesh_activation_t activations[16];
    size_t count = 0;
    err = mesh_pattern_router_compute_activations(router, &tx, activations, 16, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);

    /* Store in cache */
    cached_activation_t to_cache[16];
    for (size_t i = 0; i < count; i++) {
        to_cache[i].module_id = activations[i].module_id;
        to_cache[i].activation_level = activations[i].activation_level;
        to_cache[i].similarity = activations[i].pattern_similarity;
        to_cache[i].role = ENDORSER_ROLE_OPTIONAL;
        to_cache[i].should_endorse = activations[i].activation_level > 0.5f;
    }
    err = pattern_cache_store(cache, &pattern, to_cache, count, 0);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Second lookup - should hit cache */
    err = pattern_cache_lookup(cache, &pattern, cached, 16, &cached_count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(cached_count, count);

    /* Verify cache stats */
    pattern_cache_stats_t stats;
    pattern_cache_get_stats(cache, &stats);
    EXPECT_EQ(stats.hits, 1u);
    EXPECT_EQ(stats.misses, 1u);
}

TEST_F(MeshEnhancedRoutingIntegrationTest, SATConstraintWithRouting) {
    /* Register modules */
    float center1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float center2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float center3[] = {0.0f, 0.0f, 1.0f, 0.0f};
    float center4[] = {0.5f, 0.5f, 0.0f, 0.0f};

    register_module(0x100, center1, 4, 0.3f);
    register_module(0x200, center2, 4, 0.3f);
    register_module(0x300, center3, 4, 0.3f);
    register_module(0x400, center4, 4, 0.3f);

    /* Add SAT variables for modules */
    uint32_t v_pfc, v_motor, v_safety, v_planner;
    ASSERT_EQ(sat_solver_add_variable(solver, "pfc", 0x100, &v_pfc), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "motor", 0x200, &v_motor), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "safety", 0x300, &v_safety), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "planner", 0x400, &v_planner), NIMCP_SUCCESS);

    /* Add constraint: motor requires safety check */
    sat_solver_add_implication(solver, sat_make_literal(v_motor, false),
                               sat_make_literal(v_safety, false));

    /* At least 2 endorsers needed */
    sat_literal_t all_lits[] = {
        sat_make_literal(v_pfc, false),
        sat_make_literal(v_motor, false),
        sat_make_literal(v_safety, false),
        sat_make_literal(v_planner, false)
    };
    sat_solver_add_at_least_k(solver, all_lits, 4, 2);

    /* Route a pattern that activates motor */
    float pv[] = {0.2f, 0.9f, 0.0f, 0.3f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(pattern);

    mesh_activation_t activations[16];
    size_t count = 0;
    ASSERT_EQ(mesh_pattern_router_compute_activations(router, &tx, activations, 16, &count), NIMCP_SUCCESS);

    /* Set assumption: motor is activated */
    sat_literal_t assumptions[1] = {sat_make_literal(v_motor, false)};
    sat_result_t result = sat_solver_solve_with_assumptions(solver, assumptions, 1);
    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);

    /* Safety must be true due to implication */
    EXPECT_EQ(sat_solver_get_value(solver, v_safety), SAT_VALUE_TRUE);
}

TEST_F(MeshEnhancedRoutingIntegrationTest, CacheWithSATEndorsers) {
    /* Add SAT variables */
    uint32_t v1, v2, v3;
    sat_solver_add_variable(solver, "visual", 0x100, &v1);
    sat_solver_add_variable(solver, "hippocampus", 0x200, &v2);
    sat_solver_add_variable(solver, "pfc", 0x300, &v3);

    /* Add policy: at least one cognitive module */
    sat_literal_t cognitive[] = {
        sat_make_literal(v2, false),
        sat_make_literal(v3, false)
    };
    sat_solver_add_at_least_k(solver, cognitive, 2, 1);

    /* Solve to find valid endorsers */
    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);

    /* Create cached activations based on SAT solution */
    cached_activation_t cached_activations[3];
    cached_activations[0].module_id = 0x100;
    cached_activations[0].activation_level = 0.8f;
    cached_activations[0].similarity = 0.9f;
    cached_activations[0].role = ENDORSER_ROLE_OPTIONAL;
    cached_activations[0].should_endorse =
        sat_solver_get_value(solver, v1) == SAT_VALUE_TRUE;

    cached_activations[1].module_id = 0x200;
    cached_activations[1].activation_level = 0.6f;
    cached_activations[1].similarity = 0.7f;
    cached_activations[1].role = ENDORSER_ROLE_REQUIRED;
    cached_activations[1].should_endorse =
        sat_solver_get_value(solver, v2) == SAT_VALUE_TRUE;

    cached_activations[2].module_id = 0x300;
    cached_activations[2].activation_level = 0.7f;
    cached_activations[2].similarity = 0.8f;
    cached_activations[2].role = ENDORSER_ROLE_REQUIRED;
    cached_activations[2].should_endorse =
        sat_solver_get_value(solver, v3) == SAT_VALUE_TRUE;

    /* Store in cache */
    float pv[] = {0.5f, 0.5f, 0.5f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);

    ASSERT_EQ(pattern_cache_store(cache, &pattern, cached_activations, 3, 0), NIMCP_SUCCESS);

    /* Verify at least one cognitive endorser selected */
    cached_activation_t retrieved[3];
    size_t count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, retrieved, 3, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 3u);

    bool has_cognitive = false;
    for (size_t i = 0; i < count; i++) {
        if ((retrieved[i].module_id == 0x200 || retrieved[i].module_id == 0x300) &&
            retrieved[i].should_endorse) {
            has_cognitive = true;
            break;
        }
    }
    EXPECT_TRUE(has_cognitive);
}

/* ============================================================================
 * Performance Integration Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingIntegrationTest, CacheAcceleratesRepeatedQueries) {
    /* Register multiple modules */
    for (int i = 0; i < 10; i++) {
        float center[4] = {0};
        center[i % 4] = 1.0f;
        register_module(0x100 + i, center, 4, 0.3f);
    }

    /* Create pattern */
    float pv[] = {0.5f, 0.5f, 0.5f, 0.5f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(pattern);

    /* First query: compute from scratch */
    mesh_activation_t activations[16];
    size_t count = 0;

    auto start1 = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(mesh_pattern_router_compute_activations(router, &tx, activations, 16, &count), NIMCP_SUCCESS);
    auto end1 = std::chrono::high_resolution_clock::now();

    /* Store in cache */
    cached_activation_t to_cache[16];
    for (size_t i = 0; i < count; i++) {
        to_cache[i].module_id = activations[i].module_id;
        to_cache[i].activation_level = activations[i].activation_level;
        to_cache[i].similarity = activations[i].pattern_similarity;
        to_cache[i].role = ENDORSER_ROLE_OPTIONAL;
        to_cache[i].should_endorse = true;
    }
    pattern_cache_store(cache, &pattern, to_cache, count, 0);

    /* Second query: from cache */
    cached_activation_t cached[16];
    size_t cached_count = 0;

    auto start2 = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, cached, 16, &cached_count), NIMCP_SUCCESS);
    auto end2 = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(cached_count, count);
}

TEST_F(MeshEnhancedRoutingIntegrationTest, MultiplePatternsCached) {
    /* Register modules */
    float center1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float center2[] = {0.0f, 1.0f, 0.0f, 0.0f};

    register_module(0x100, center1, 4, 0.3f);
    register_module(0x200, center2, 4, 0.3f);

    /* Cache multiple patterns */
    for (int i = 0; i < 10; i++) {
        float pv[4] = {(float)i / 10.0f, 1.0f - (float)i / 10.0f, 0.0f, 0.0f};
        mesh_pattern_t pattern = create_pattern(pv, 4);
        mesh_pattern_transaction_t tx = create_transaction(pattern);

        mesh_activation_t activations[16];
        size_t count = 0;
        mesh_pattern_router_compute_activations(router, &tx, activations, 16, &count);

        cached_activation_t to_cache[16];
        for (size_t j = 0; j < count; j++) {
            to_cache[j].module_id = activations[j].module_id;
            to_cache[j].activation_level = activations[j].activation_level;
            to_cache[j].similarity = 0.0f;
            to_cache[j].role = ENDORSER_ROLE_OPTIONAL;
            to_cache[j].should_endorse = true;
        }
        pattern_cache_store(cache, &pattern, to_cache, count, 0);
    }

    /* Verify all patterns retrievable */
    pattern_cache_stats_t stats;
    pattern_cache_get_stats(cache, &stats);
    EXPECT_EQ(stats.current_entries, 10u);

    /* Lookup all patterns */
    for (int i = 0; i < 10; i++) {
        float pv[4] = {(float)i / 10.0f, 1.0f - (float)i / 10.0f, 0.0f, 0.0f};
        mesh_pattern_t pattern = create_pattern(pv, 4);

        cached_activation_t cached[16];
        size_t count = 0;
        EXPECT_EQ(pattern_cache_lookup(cache, &pattern, cached, 16, &count), NIMCP_SUCCESS);
        EXPECT_GT(count, 0u);
    }

    pattern_cache_get_stats(cache, &stats);
    EXPECT_EQ(stats.hits, 10u);
}

/* ============================================================================
 * Complex Scenario Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingIntegrationTest, CacheInvalidationOnModuleChange) {
    /* Cache a pattern */
    float pv[] = {0.8f, 0.2f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);

    cached_activation_t cached_act[2];
    cached_act[0].module_id = 0x100;
    cached_act[0].activation_level = 0.9f;
    cached_act[0].similarity = 0.8f;
    cached_act[0].role = ENDORSER_ROLE_REQUIRED;
    cached_act[0].should_endorse = true;
    cached_act[1].module_id = 0x200;
    cached_act[1].activation_level = 0.3f;
    cached_act[1].similarity = 0.4f;
    cached_act[1].role = ENDORSER_ROLE_OPTIONAL;
    cached_act[1].should_endorse = false;

    ASSERT_EQ(pattern_cache_store(cache, &pattern, cached_act, 2, 0), NIMCP_SUCCESS);

    /* Verify cached */
    cached_activation_t retrieved[2];
    size_t count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, retrieved, 2, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 2u);

    /* Module changes - invalidate cache */
    ASSERT_EQ(pattern_cache_invalidate_module(cache, 0x100), NIMCP_SUCCESS);

    /* Cache should be empty for that module's entries */
    count = 0;
    nimcp_error_t err = pattern_cache_lookup(cache, &pattern, retrieved, 2, &count);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshEnhancedRoutingIntegrationTest, SATWithVetoConstraint) {
    /* Scenario: Amygdala can veto motor commands */

    uint32_t v_motor, v_safety, v_amygdala;
    sat_solver_add_variable(solver, "motor", 0x100, &v_motor);
    sat_solver_add_variable(solver, "safety", 0x200, &v_safety);
    sat_solver_add_variable(solver, "amygdala", 0x300, &v_amygdala);

    /* Motor requires safety */
    sat_solver_add_implication(solver, sat_make_literal(v_motor, false),
                               sat_make_literal(v_safety, false));

    /* Amygdala veto: amygdala → ¬motor */
    sat_literal_t veto_clause[2] = {
        sat_make_literal(v_amygdala, true),
        sat_make_literal(v_motor, true)
    };
    sat_solver_add_clause(solver, veto_clause, 2, 1.0f);

    /* Test 1: No veto, motor can fire */
    sat_literal_t assumptions1[2] = {
        sat_make_literal(v_motor, false),
        sat_make_literal(v_amygdala, true)
    };
    sat_result_t result1 = sat_solver_solve_with_assumptions(solver, assumptions1, 2);
    EXPECT_EQ(result1, SAT_RESULT_SATISFIABLE);
    EXPECT_EQ(sat_solver_get_value(solver, v_safety), SAT_VALUE_TRUE);

    /* Test 2: Veto active, motor cannot fire */
    sat_solver_reset(solver);
    sat_solver_add_variable(solver, "motor", 0x100, &v_motor);
    sat_solver_add_variable(solver, "safety", 0x200, &v_safety);
    sat_solver_add_variable(solver, "amygdala", 0x300, &v_amygdala);

    sat_solver_add_implication(solver, sat_make_literal(v_motor, false),
                               sat_make_literal(v_safety, false));
    sat_solver_add_clause(solver, veto_clause, 2, 1.0f);

    sat_literal_t assumptions2[2] = {
        sat_make_literal(v_motor, false),
        sat_make_literal(v_amygdala, false)
    };
    sat_result_t result2 = sat_solver_solve_with_assumptions(solver, assumptions2, 2);
    EXPECT_EQ(result2, SAT_RESULT_UNSATISFIABLE);
}

TEST_F(MeshEnhancedRoutingIntegrationTest, CoWCopyPreservesOriginal) {
    /* Create original pattern */
    float pv[] = {0.5f, 0.5f, 0.0f, 0.0f};
    mesh_pattern_t original = create_pattern(pv, 4);

    cached_activation_t original_act;
    original_act.module_id = 0x100;
    original_act.activation_level = 0.8f;
    original_act.similarity = 0.9f;
    original_act.role = ENDORSER_ROLE_REQUIRED;
    original_act.should_endorse = true;

    ASSERT_EQ(pattern_cache_store(cache, &original, &original_act, 1, 0), NIMCP_SUCCESS);

    /* Create variant pattern */
    float pv2[] = {0.6f, 0.4f, 0.0f, 0.0f};
    mesh_pattern_t variant = create_pattern(pv2, 4);

    /* CoW copy */
    pattern_cache_entry_t* new_entry = nullptr;
    nimcp_error_t err = pattern_cache_cow_copy(cache, &original, &variant, &new_entry);

    if (err == NIMCP_SUCCESS && new_entry) {
        /* Original still accessible */
        cached_activation_t retrieved[1];
        size_t count = 0;

        EXPECT_EQ(pattern_cache_lookup(cache, &original, retrieved, 1, &count), NIMCP_SUCCESS);
        EXPECT_EQ(count, 1u);
        EXPECT_FLOAT_EQ(retrieved[0].activation_level, 0.8f);

        pattern_cache_release(cache, new_entry);
    }
}
