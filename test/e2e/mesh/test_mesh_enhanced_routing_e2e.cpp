/**
 * @file test_mesh_enhanced_routing_e2e.cpp
 * @brief End-to-end tests for Enhanced Pattern Routing
 *
 * WHAT: Complete flow tests from pattern input to endorsed output
 * WHY:  Validates the full routing pipeline including caching and SAT
 * HOW:  Simulates realistic brain-inspired routing scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>

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

class MeshEnhancedRoutingE2ETest : public ::testing::Test {
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
        router_cfg.max_endorsers = 32;
        router = mesh_pattern_router_create(&router_cfg);
        ASSERT_NE(router, nullptr);

        /* Create pattern cache */
        pattern_cache_config_t cache_cfg;
        pattern_cache_default_config(&cache_cfg);
        cache_cfg.max_entries = 1024;
        cache_cfg.enable_cow = true;
        cache_cfg.enable_lru = true;
        cache_cfg.enable_logging = false;
        cache = pattern_cache_create(&cache_cfg);
        ASSERT_NE(cache, nullptr);

        /* Create SAT solver */
        sat_solver_config_t sat_cfg;
        sat_solver_default_config(&sat_cfg);
        sat_cfg.max_conflicts = 10000;
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

    /* Register a brain module with the router */
    void register_brain_module(mesh_participant_id_t id, const char* name,
                               const float* center, size_t dim, float threshold) {
        (void)name;  /* Name tracked via SAT solver, not router */
        mesh_pattern_t pattern = create_pattern(center, dim);
        mesh_receptive_field_t field = create_field(pattern, threshold);
        mesh_pattern_router_register_receptive_field(router, id, &field);
    }
};

/* ============================================================================
 * Full Pipeline E2E Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingE2ETest, FullVisualProcessingPipeline) {
    /*
     * E2E Scenario: Visual stimulus processing
     *
     * Flow:
     * 1. Visual pattern arrives
     * 2. Routes to V1 (primary visual cortex)
     * 3. SAT solver ensures cognitive oversight
     * 4. Cache accelerates repeated queries
     * 5. Final endorsement set returned
     */

    /* Set up visual processing hierarchy */
    float v1_center[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float v2_center[] = {0.8f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float v4_center[] = {0.6f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float it_center[] = {0.4f, 0.4f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float pfc_center[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    register_brain_module(0x100, "V1", v1_center, 8, 0.4f);
    register_brain_module(0x101, "V2", v2_center, 8, 0.4f);
    register_brain_module(0x102, "V4", v4_center, 8, 0.4f);
    register_brain_module(0x103, "IT", it_center, 8, 0.4f);
    register_brain_module(0x200, "PFC", pfc_center, 8, 0.5f);

    /* Set up SAT constraints */
    uint32_t v_v1, v_v2, v_v4, v_it, v_pfc;
    sat_solver_add_variable(solver, "V1", 0x100, &v_v1);
    sat_solver_add_variable(solver, "V2", 0x101, &v_v2);
    sat_solver_add_variable(solver, "V4", 0x102, &v_v4);
    sat_solver_add_variable(solver, "IT", 0x103, &v_it);
    sat_solver_add_variable(solver, "PFC", 0x200, &v_pfc);

    /* Visual hierarchy: V1 → V2 → V4 → IT */
    sat_solver_add_implication(solver, sat_make_literal(v_v2, false),
                               sat_make_literal(v_v1, false));
    sat_solver_add_implication(solver, sat_make_literal(v_v4, false),
                               sat_make_literal(v_v2, false));
    sat_solver_add_implication(solver, sat_make_literal(v_it, false),
                               sat_make_literal(v_v4, false));

    /* High-level recognition requires PFC oversight */
    sat_solver_add_implication(solver, sat_make_literal(v_it, false),
                               sat_make_literal(v_pfc, false));

    /* Create visual pattern */
    float visual_pattern[] = {0.9f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(visual_pattern, 8);
    mesh_pattern_transaction_t tx = create_transaction(pattern);

    /* Step 1: Route pattern */
    mesh_activation_t activations[32];
    size_t act_count = 0;
    ASSERT_EQ(mesh_pattern_router_compute_activations(router, &tx, activations, 32, &act_count),
              NIMCP_SUCCESS);
    EXPECT_GT(act_count, 0u);

    /* Step 2: Find V1 activation */
    bool found_v1 = false;
    for (size_t i = 0; i < act_count; i++) {
        if (activations[i].module_id == 0x100) {
            found_v1 = true;
            break;
        }
    }
    EXPECT_TRUE(found_v1);

    /* Step 3: Assume IT recognition (high-level) */
    sat_literal_t assumptions[] = {sat_make_literal(v_it, false)};
    sat_result_t result = sat_solver_solve_with_assumptions(solver, assumptions, 1);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    /* Step 4: Verify cascade */
    EXPECT_EQ(sat_solver_get_value(solver, v_v1), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_v2), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_v4), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_it), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_pfc), SAT_VALUE_TRUE);

    /* Step 5: Cache the endorsed set */
    cached_activation_t to_cache[5];
    size_t cache_idx = 0;

    mesh_participant_id_t endorsed_ids[] = {0x100, 0x101, 0x102, 0x103, 0x200};
    uint32_t endorsed_vars[] = {v_v1, v_v2, v_v4, v_it, v_pfc};

    for (int i = 0; i < 5; i++) {
        if (sat_solver_get_value(solver, endorsed_vars[i]) == SAT_VALUE_TRUE) {
            to_cache[cache_idx].module_id = endorsed_ids[i];
            to_cache[cache_idx].activation_level = 0.8f;
            to_cache[cache_idx].similarity = 0.9f;
            to_cache[cache_idx].role = (i == 4) ? ENDORSER_ROLE_REQUIRED : ENDORSER_ROLE_OPTIONAL;
            to_cache[cache_idx].should_endorse = true;
            cache_idx++;
        }
    }

    ASSERT_EQ(pattern_cache_store(cache, &pattern, to_cache, cache_idx, 0), NIMCP_SUCCESS);

    /* Step 6: Verify cache retrieval works */
    cached_activation_t retrieved[5];
    size_t ret_count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, retrieved, 5, &ret_count), NIMCP_SUCCESS);
    EXPECT_EQ(ret_count, cache_idx);
}

TEST_F(MeshEnhancedRoutingE2ETest, MotorCommandWithSafetyInterlock) {
    /*
     * E2E Scenario: Motor command with safety checks
     *
     * Flow:
     * 1. Motor cortex activated
     * 2. Safety module (cerebellum) must validate
     * 3. Amygdala can veto dangerous actions
     * 4. Basal ganglia coordinates execution
     */

    float motor_center[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float cerebellum_center[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float amygdala_center[] = {0.0f, 0.0f, 1.0f, 0.0f};
    float basal_center[] = {0.5f, 0.5f, 0.0f, 0.0f};

    register_brain_module(0x100, "motor_cortex", motor_center, 4, 0.5f);
    register_brain_module(0x200, "cerebellum", cerebellum_center, 4, 0.5f);
    register_brain_module(0x300, "amygdala", amygdala_center, 4, 0.5f);
    register_brain_module(0x400, "basal_ganglia", basal_center, 4, 0.6f);

    uint32_t v_motor, v_cerebellum, v_amygdala, v_basal;
    sat_solver_add_variable(solver, "motor_cortex", 0x100, &v_motor);
    sat_solver_add_variable(solver, "cerebellum", 0x200, &v_cerebellum);
    sat_solver_add_variable(solver, "amygdala", 0x300, &v_amygdala);
    sat_solver_add_variable(solver, "basal_ganglia", 0x400, &v_basal);

    /* Motor requires cerebellum validation */
    sat_solver_add_implication(solver, sat_make_literal(v_motor, false),
                               sat_make_literal(v_cerebellum, false));

    /* Motor requires basal ganglia coordination */
    sat_solver_add_implication(solver, sat_make_literal(v_motor, false),
                               sat_make_literal(v_basal, false));

    /* Amygdala veto: amygdala → ¬motor */
    sat_literal_t veto_clause[] = {
        sat_make_literal(v_amygdala, true),   /* NOT amygdala */
        sat_make_literal(v_motor, true)       /* NOT motor */
    };
    sat_solver_add_clause(solver, veto_clause, 2, 1.0f);

    /* Test safe motor command (no amygdala veto) */
    sat_literal_t safe_assumptions[] = {
        sat_make_literal(v_motor, false),     /* motor = true */
        sat_make_literal(v_amygdala, true)    /* amygdala = false */
    };

    sat_result_t result = sat_solver_solve_with_assumptions(solver, safe_assumptions, 2);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    EXPECT_EQ(sat_solver_get_value(solver, v_motor), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_cerebellum), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_basal), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_amygdala), SAT_VALUE_FALSE);
}

TEST_F(MeshEnhancedRoutingE2ETest, MemoryConsolidation) {
    /*
     * E2E Scenario: Memory consolidation during learning
     *
     * Flow:
     * 1. Hippocampus encodes new memory
     * 2. Prefrontal cortex provides relevance signal
     * 3. Emotional salience from amygdala modulates strength
     * 4. Result cached for fast recall
     */

    float hippocampus_center[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float pfc_center[] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float amygdala_center[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    float neocortex_center[] = {0.3f, 0.3f, 0.3f, 0.0f, 0.0f, 0.0f};

    register_brain_module(0x100, "hippocampus", hippocampus_center, 6, 0.5f);
    register_brain_module(0x200, "pfc", pfc_center, 6, 0.5f);
    register_brain_module(0x300, "amygdala", amygdala_center, 6, 0.5f);
    register_brain_module(0x400, "neocortex", neocortex_center, 6, 0.7f);

    uint32_t v_hip, v_pfc, v_amy, v_neo;
    sat_solver_add_variable(solver, "hippocampus", 0x100, &v_hip);
    sat_solver_add_variable(solver, "pfc", 0x200, &v_pfc);
    sat_solver_add_variable(solver, "amygdala", 0x300, &v_amy);
    sat_solver_add_variable(solver, "neocortex", 0x400, &v_neo);

    /* Memory consolidation: hippocampus AND (pfc OR emotional_salience) */
    sat_solver_add_unit(solver, sat_make_literal(v_hip, false));

    sat_literal_t relevance_or_emotion[] = {
        sat_make_literal(v_pfc, false),
        sat_make_literal(v_amy, false)
    };
    sat_solver_add_at_least_k(solver, relevance_or_emotion, 2, 1);

    /* Create memory pattern */
    float memory_pattern[] = {0.8f, 0.2f, 0.5f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(memory_pattern, 6);
    mesh_pattern_transaction_t tx = create_transaction(pattern);

    /* Route */
    mesh_activation_t activations[16];
    size_t act_count = 0;
    mesh_pattern_router_compute_activations(router, &tx, activations, 16, &act_count);

    /* Solve */
    sat_result_t result = sat_solver_solve(solver);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    EXPECT_EQ(sat_solver_get_value(solver, v_hip), SAT_VALUE_TRUE);
    bool has_relevance = sat_solver_get_value(solver, v_pfc) == SAT_VALUE_TRUE ||
                         sat_solver_get_value(solver, v_amy) == SAT_VALUE_TRUE;
    EXPECT_TRUE(has_relevance);

    /* Cache for fast recall */
    cached_activation_t memory_cache[4];
    memory_cache[0].module_id = 0x100;
    memory_cache[0].activation_level = 0.9f;
    memory_cache[0].similarity = 0.8f;
    memory_cache[0].role = ENDORSER_ROLE_REQUIRED;
    memory_cache[0].should_endorse = true;

    memory_cache[1].module_id = 0x200;
    memory_cache[1].activation_level = 0.7f;
    memory_cache[1].similarity = 0.7f;
    memory_cache[1].role = ENDORSER_ROLE_OPTIONAL;
    memory_cache[1].should_endorse = sat_solver_get_value(solver, v_pfc) == SAT_VALUE_TRUE;

    memory_cache[2].module_id = 0x300;
    memory_cache[2].activation_level = 0.5f;
    memory_cache[2].similarity = 0.6f;
    memory_cache[2].role = ENDORSER_ROLE_OPTIONAL;
    memory_cache[2].should_endorse = sat_solver_get_value(solver, v_amy) == SAT_VALUE_TRUE;

    size_t cache_count = 3;
    pattern_cache_store(cache, &pattern, memory_cache, cache_count, 0);

    /* Fast recall */
    cached_activation_t recalled[4];
    size_t recall_count = 0;
    ASSERT_EQ(pattern_cache_lookup(cache, &pattern, recalled, 4, &recall_count), NIMCP_SUCCESS);
    EXPECT_EQ(recall_count, cache_count);
}

TEST_F(MeshEnhancedRoutingE2ETest, CrossModalIntegration) {
    /*
     * E2E Scenario: Audiovisual integration
     *
     * Flow:
     * 1. Visual and auditory streams processed separately
     * 2. Integration in superior temporal sulcus
     * 3. Unified percept requires both modalities
     */

    float visual_center[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float auditory_center[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float sts_center[] = {0.5f, 0.5f, 0.0f, 0.0f};
    float pfc_center[] = {0.0f, 0.0f, 1.0f, 0.0f};

    register_brain_module(0x100, "visual_cortex", visual_center, 4, 0.4f);
    register_brain_module(0x200, "auditory_cortex", auditory_center, 4, 0.4f);
    register_brain_module(0x300, "sts", sts_center, 4, 0.6f);
    register_brain_module(0x400, "pfc", pfc_center, 4, 0.5f);

    uint32_t v_vis, v_aud, v_sts, v_pfc;
    sat_solver_add_variable(solver, "visual_cortex", 0x100, &v_vis);
    sat_solver_add_variable(solver, "auditory_cortex", 0x200, &v_aud);
    sat_solver_add_variable(solver, "sts", 0x300, &v_sts);
    sat_solver_add_variable(solver, "pfc", 0x400, &v_pfc);

    /* Cross-modal: STS requires BOTH visual AND auditory */
    sat_solver_add_implication(solver, sat_make_literal(v_sts, false),
                               sat_make_literal(v_vis, false));
    sat_solver_add_implication(solver, sat_make_literal(v_sts, false),
                               sat_make_literal(v_aud, false));

    /* Integration goes to PFC */
    sat_solver_add_implication(solver, sat_make_literal(v_sts, false),
                               sat_make_literal(v_pfc, false));

    /* Audiovisual pattern */
    float av_pattern[] = {0.7f, 0.8f, 0.2f, 0.1f};
    mesh_pattern_t pattern = create_pattern(av_pattern, 4);
    mesh_pattern_transaction_t tx = create_transaction(pattern);

    /* Route */
    mesh_activation_t activations[8];
    size_t count = 0;
    mesh_pattern_router_compute_activations(router, &tx, activations, 8, &count);

    /* Assume cross-modal integration */
    sat_literal_t assumptions[] = {sat_make_literal(v_sts, false)};
    sat_result_t result = sat_solver_solve_with_assumptions(solver, assumptions, 1);
    ASSERT_EQ(result, SAT_RESULT_SATISFIABLE);

    /* All four should be active */
    EXPECT_EQ(sat_solver_get_value(solver, v_vis), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_aud), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_sts), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v_pfc), SAT_VALUE_TRUE);
}

/* ============================================================================
 * Performance E2E Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingE2ETest, LargeScaleRouting) {
    /*
     * E2E: Large-scale brain simulation with many modules
     */

    /* Register 50 modules */
    for (int i = 0; i < 50; i++) {
        float center[8] = {0};
        center[i % 8] = 1.0f;
        center[(i + 1) % 8] = 0.3f;

        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        register_brain_module(0x100 + i, name, center, 8, 0.4f);

        uint32_t var;
        sat_solver_add_variable(solver, name, 0x100 + i, &var);
    }

    /* Add some constraints */
    for (int i = 0; i < 45; i++) {
        uint32_t v1 = sat_solver_get_variable_for_module(solver, 0x100 + i);
        uint32_t v2 = sat_solver_get_variable_for_module(solver, 0x100 + i + 5);
        if (v1 > 0 && v2 > 0) {
            sat_solver_add_implication(solver, sat_make_literal(v1, false),
                                       sat_make_literal(v2, false));
        }
    }

    /* Create pattern */
    float pattern_values[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    mesh_pattern_t pattern = create_pattern(pattern_values, 8);
    mesh_pattern_transaction_t tx = create_transaction(pattern);

    auto start = std::chrono::high_resolution_clock::now();

    /* Route */
    mesh_activation_t activations[64];
    size_t count = 0;
    ASSERT_EQ(mesh_pattern_router_compute_activations(router, &tx, activations, 64, &count), NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);

    /* Solve SAT */
    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);

    /* Cache result */
    cached_activation_t to_cache[64];
    for (size_t i = 0; i < count && i < 64; i++) {
        to_cache[i].module_id = activations[i].module_id;
        to_cache[i].activation_level = activations[i].activation_level;
        to_cache[i].similarity = activations[i].pattern_similarity;
        to_cache[i].role = ENDORSER_ROLE_OPTIONAL;
        to_cache[i].should_endorse = true;
    }
    pattern_cache_store(cache, &pattern, to_cache, count, 0);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Should complete in reasonable time (< 100ms) */
    EXPECT_LT(duration.count(), 100);
}

TEST_F(MeshEnhancedRoutingE2ETest, RepeatedQueriesWithCache) {
    /*
     * E2E: Cache provides speedup on repeated queries
     */

    /* Simple setup */
    float center1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float center2[] = {0.0f, 1.0f, 0.0f, 0.0f};

    register_brain_module(0x100, "module_a", center1, 4, 0.5f);
    register_brain_module(0x200, "module_b", center2, 4, 0.5f);

    float pv[] = {0.5f, 0.5f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(pattern);

    /* First query (cold) */
    mesh_activation_t acts[8];
    size_t count = 0;
    mesh_pattern_router_compute_activations(router, &tx, acts, 8, &count);

    cached_activation_t to_cache[8];
    for (size_t i = 0; i < count; i++) {
        to_cache[i].module_id = acts[i].module_id;
        to_cache[i].activation_level = acts[i].activation_level;
        to_cache[i].similarity = acts[i].pattern_similarity;
        to_cache[i].role = ENDORSER_ROLE_OPTIONAL;
        to_cache[i].should_endorse = true;
    }
    pattern_cache_store(cache, &pattern, to_cache, count, 0);

    /* Repeated queries (warm) */
    for (int i = 0; i < 100; i++) {
        cached_activation_t cached[8];
        size_t cached_count = 0;
        ASSERT_EQ(pattern_cache_lookup(cache, &pattern, cached, 8, &cached_count), NIMCP_SUCCESS);
        EXPECT_EQ(cached_count, count);
    }

    /* Verify stats */
    pattern_cache_stats_t stats;
    pattern_cache_get_stats(cache, &stats);
    EXPECT_EQ(stats.hits, 100u);
    EXPECT_EQ(stats.misses, 0u);
}

/* ============================================================================
 * Error Handling E2E Tests
 * ============================================================================ */

TEST_F(MeshEnhancedRoutingE2ETest, HandleUnsatisfiableConstraints) {
    /*
     * E2E: Graceful handling when SAT constraints are unsatisfiable
     */

    uint32_t x;
    sat_solver_add_variable(solver, "x", 0x100, &x);

    /* Contradiction: x AND NOT x */
    sat_solver_add_unit(solver, sat_make_literal(x, false));
    sat_solver_add_unit(solver, sat_make_literal(x, true));

    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_UNSATISFIABLE);

    /* System should handle gracefully - no crash */
}

TEST_F(MeshEnhancedRoutingE2ETest, HandleCacheMiss) {
    /*
     * E2E: Fallback to routing when cache misses
     */

    float center[] = {1.0f, 0.0f, 0.0f, 0.0f};
    register_brain_module(0x100, "test", center, 4, 0.5f);

    float pv[] = {0.9f, 0.1f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(pattern);

    /* Cache miss */
    cached_activation_t cached[8];
    size_t count = 0;
    nimcp_error_t err = pattern_cache_lookup(cache, &pattern, cached, 8, &count);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);

    /* Fallback to routing */
    mesh_activation_t activations[8];
    size_t act_count = 0;
    ASSERT_EQ(mesh_pattern_router_compute_activations(router, &tx, activations, 8, &act_count), NIMCP_SUCCESS);
    EXPECT_GT(act_count, 0u);
}
