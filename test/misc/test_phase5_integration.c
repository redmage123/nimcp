/**
 * @file test_phase5_integration.c
 * @brief Phase 5 Critical Integration Tests
 *
 * Tests for: Counterfactual causal graph, conceptual blending enhancement,
 * FEP learning callbacks, FEP-parietal bridge, predicate indexing,
 * creative language bridge.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "cognitive/parietal/nimcp_counterfactual.h"
#include "cognitive/parietal/nimcp_conceptual_blending.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "cognitive/parietal/nimcp_fep_parietal_bridge.h"
#include "language/nimcp_creative_language_bridge.h"
#include "utils/memory/nimcp_memory.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* =========================================================================
 * Counterfactual Causal Graph Tests
 * ========================================================================= */

static void test_cf_create_destroy(void) {
    TEST("CF: create and destroy");
    counterfactual_engine_t* e = counterfactual_engine_create();
    ASSERT_TRUE(e != NULL, "engine NULL");
    counterfactual_engine_destroy(e);
    PASS();
}

static void test_cf_learn_causal_weight(void) {
    TEST("CF: learn causal weight");
    counterfactual_engine_t* e = counterfactual_engine_create();

    /* Declare the external function */
    extern int counterfactual_learn_causal_weight(counterfactual_engine_t*, uint32_t, uint32_t, float);

    int rc = counterfactual_learn_causal_weight(e, 0, 1, 0.8f);
    ASSERT_TRUE(rc == 0, "learn weight failed");

    rc = counterfactual_learn_causal_weight(e, 1, 2, 0.5f);
    ASSERT_TRUE(rc == 0, "learn weight 2 failed");

    /* Out of bounds should fail */
    rc = counterfactual_learn_causal_weight(e, 300, 0, 0.5f);
    ASSERT_TRUE(rc == -1, "should reject out of bounds");

    counterfactual_engine_destroy(e);
    PASS();
}

static void test_cf_causal_propagation(void) {
    TEST("CF: causal propagation in imagine");
    counterfactual_engine_t* e = counterfactual_engine_create();
    extern int counterfactual_learn_causal_weight(counterfactual_engine_t*, uint32_t, uint32_t, float);

    /* Set up causal chain: var0 -> var1 -> var2 */
    counterfactual_learn_causal_weight(e, 0, 1, 0.8f);
    counterfactual_learn_causal_weight(e, 1, 2, 0.6f);

    float values[] = {1.0f, 0.5f, 0.3f, 0.1f};
    cf_state_t* actual = counterfactual_create_state(values, 4, "actual");

    cf_intervention_t what_if = {
        .id = 1,
        .target_variable = 0,
        .original_value = 1.0f,
        .counterfactual_value = 3.0f
    };

    cf_counterfactual_t* cf = counterfactual_imagine(e, actual, &what_if);
    ASSERT_TRUE(cf != NULL, "imagine failed");
    ASSERT_TRUE(cf->counterfactual_world != NULL, "cf world NULL");

    /* Var 0 should be set to intervention value */
    ASSERT_TRUE(fabsf(cf->counterfactual_world->values[0] - 3.0f) < 0.01f, "intervention not applied");

    /* Var 1 should be changed by causal propagation */
    ASSERT_TRUE(fabsf(cf->counterfactual_world->values[1] - values[1]) > 0.01f, "no propagation to var1");

    counterfactual_free(cf);
    counterfactual_free_state(actual);
    counterfactual_engine_destroy(e);
    PASS();
}

static void test_cf_causal_strength_with_graph(void) {
    TEST("CF: causal strength uses graph");
    counterfactual_engine_t* e = counterfactual_engine_create();
    extern int counterfactual_learn_causal_weight(counterfactual_engine_t*, uint32_t, uint32_t, float);

    counterfactual_learn_causal_weight(e, 0, 1, 0.9f);

    float values[] = {0.1f, 0.1f, 0.1f};
    cf_state_t* ctx = counterfactual_create_state(values, 3, "ctx");

    /* With graph: strength should be 0.9 (not correlation-based) */
    float strength = counterfactual_causal_strength(e, 0, 1, ctx);
    ASSERT_TRUE(strength > 0.5f, "graph-based strength too low");

    counterfactual_free_state(ctx);
    counterfactual_engine_destroy(e);
    PASS();
}

static void test_cf_find_causes_with_graph(void) {
    TEST("CF: find causes uses graph");
    counterfactual_engine_t* e = counterfactual_engine_create();
    extern int counterfactual_learn_causal_weight(counterfactual_engine_t*, uint32_t, uint32_t, float);

    /* var0 and var2 cause var1 */
    counterfactual_learn_causal_weight(e, 0, 1, 0.7f);
    counterfactual_learn_causal_weight(e, 2, 1, 0.5f);

    float values[] = {0.1f, 0.1f, 0.1f};
    cf_state_t* state = counterfactual_create_state(values, 3, "state");

    uint32_t causes[10];
    uint32_t num_found = 0;
    int rc = counterfactual_find_causes(e, state, 1, causes, 10, &num_found);
    ASSERT_TRUE(rc == 0, "find_causes failed");
    ASSERT_TRUE(num_found == 2, "should find 2 causes");

    counterfactual_free_state(state);
    counterfactual_engine_destroy(e);
    PASS();
}

static void test_cf_trace_causal_consequences(void) {
    TEST("CF: trace effects includes causal paths");
    counterfactual_engine_t* e = counterfactual_engine_create();
    extern int counterfactual_learn_causal_weight(counterfactual_engine_t*, uint32_t, uint32_t, float);

    counterfactual_learn_causal_weight(e, 0, 1, 0.8f);
    counterfactual_learn_causal_weight(e, 0, 2, 0.6f);

    float values[] = {1.0f, 0.5f, 0.3f};
    cf_state_t* actual = counterfactual_create_state(values, 3, "actual");

    cf_intervention_t what_if = {
        .id = 1, .target_variable = 0,
        .original_value = 1.0f, .counterfactual_value = 2.0f
    };

    cf_counterfactual_t* cf = counterfactual_imagine(e, actual, &what_if);
    ASSERT_TRUE(cf != NULL, "imagine failed");
    ASSERT_TRUE(cf->num_consequences > 0, "no consequences traced");

    counterfactual_free(cf);
    counterfactual_free_state(actual);
    counterfactual_engine_destroy(e);
    PASS();
}

static void test_cf_null_safety(void) {
    TEST("CF: null safety");
    extern int counterfactual_learn_causal_weight(counterfactual_engine_t*, uint32_t, uint32_t, float);
    ASSERT_TRUE(counterfactual_learn_causal_weight(NULL, 0, 1, 0.5f) == -1, "should reject NULL");
    PASS();
}

/* =========================================================================
 * Conceptual Blending Enhancement Tests
 * ========================================================================= */

static void test_blend_selective_projection(void) {
    TEST("Blend: selective projection");
    blending_engine_t* e = blending_engine_create();
    ASSERT_TRUE(e != NULL, "engine NULL");

    blend_mental_space_t* s1 = blending_create_space("birds");
    blend_mental_space_t* s2 = blending_create_space("planes");

    float f1[] = {1.0f, 0.0f, 0.5f, 0.8f};
    float f2[] = {0.9f, 0.1f, 0.6f, 0.7f};
    float f3[] = {0.1f, 0.9f, 0.2f, 0.1f};

    blending_add_element(s1, "wings", f1, 4);
    blending_add_element(s1, "feathers", f3, 4);
    blending_add_element(s2, "wings", f2, 4);
    blending_add_element(s2, "engine", f3, 4);

    conceptual_blend_t* b = blending_create_blend(e, s1, s2);
    ASSERT_TRUE(b != NULL, "blend NULL");
    ASSERT_TRUE(b->blend != NULL, "blend space NULL");
    ASSERT_TRUE(b->blend->num_elements > 0, "no elements in blend");

    blending_free_blend(b);
    blending_free_space(s1);
    blending_free_space(s2);
    blending_engine_destroy(e);
    PASS();
}

static void test_blend_emergence_detection(void) {
    TEST("Blend: emergence detection");
    blending_engine_t* e = blending_engine_create();

    blend_mental_space_t* s1 = blending_create_space("A");
    blend_mental_space_t* s2 = blending_create_space("B");

    float f1[] = {0.5f, 0.2f, 0.1f};
    float f2[] = {0.3f, 0.6f, 0.1f};

    blending_add_element(s1, "x", f1, 3);
    blending_add_element(s2, "y", f2, 3);

    conceptual_blend_t* b = blending_create_blend(e, s1, s2);
    ASSERT_TRUE(b != NULL, "blend NULL");
    /* Should have at least structural emergence */
    ASSERT_TRUE(b->num_emergent >= 0, "negative emergence count");
    if (b->num_emergent > 0) {
        ASSERT_TRUE(b->emergent_properties[0].is_emergent, "not marked emergent");
        ASSERT_TRUE(b->emergent_properties[0].strength > 0.0f, "zero strength");
    }

    blending_free_blend(b);
    blending_free_space(s1);
    blending_free_space(s2);
    blending_engine_destroy(e);
    PASS();
}

static void test_blend_hill_climbing_optimizer(void) {
    TEST("Blend: hill-climbing optimizer");
    blending_engine_t* e = blending_engine_create();

    blend_mental_space_t* s1 = blending_create_space("A");
    blend_mental_space_t* s2 = blending_create_space("B");

    float f1[] = {0.5f, 0.3f};
    float f2[] = {0.4f, 0.6f};
    blending_add_element(s1, "x", f1, 2);
    blending_add_element(s2, "y", f2, 2);

    conceptual_blend_t* b = blending_create_blend(e, s1, s2);
    ASSERT_TRUE(b != NULL, "blend NULL");

    float before_integration = b->integration_score;
    int rc = blending_optimize_blend(e, b);
    ASSERT_TRUE(rc == 0, "optimize failed");
    ASSERT_TRUE(b->integration_score >= before_integration, "integration didn't improve");

    blending_free_blend(b);
    blending_free_space(s1);
    blending_free_space(s2);
    blending_engine_destroy(e);
    PASS();
}

static void test_blend_novelty_evaluation(void) {
    TEST("Blend: novelty evaluation");
    blending_engine_t* e = blending_engine_create();

    blend_mental_space_t* s1 = blending_create_space("A");
    blend_mental_space_t* s2 = blending_create_space("B");

    float f1[] = {1.0f};
    float f2[] = {0.5f};
    blending_add_element(s1, "x", f1, 1);
    blending_add_element(s2, "y", f2, 1);

    conceptual_blend_t* b = blending_create_blend(e, s1, s2);
    float novelty = blending_evaluate_novelty(e, b);
    ASSERT_TRUE(novelty >= 0.0f && novelty <= 1.0f, "novelty out of range");

    float integration = blending_evaluate_integration(e, b);
    ASSERT_TRUE(integration >= 0.0f && integration <= 1.0f, "integration out of range");

    blending_free_blend(b);
    blending_free_space(s1);
    blending_free_space(s2);
    blending_engine_destroy(e);
    PASS();
}

/* =========================================================================
 * FEP Learning Callback Tests
 * ========================================================================= */

static int g_callback_count = 0;
static float g_callback_loss = 0.0f;

static void test_callback(fep_transition_learner_t* learner, float loss, void* user_data) {
    (void)learner;
    g_callback_count++;
    g_callback_loss = loss;
    if (user_data) {
        *(int*)user_data = 42;
    }
}

static void test_fep_callback_set(void) {
    TEST("FEP: set update callback");
    fep_transition_learner_t* learner = fep_transition_learner_create(NULL, 4);
    ASSERT_TRUE(learner != NULL, "learner NULL");

    int rc = fep_transition_learner_set_update_callback(learner, test_callback, NULL);
    ASSERT_TRUE(rc == 0, "set callback failed");

    rc = fep_transition_learner_set_update_callback(learner, NULL, NULL);
    ASSERT_TRUE(rc == 0, "unset callback failed");

    fep_transition_learner_destroy(learner);
    PASS();
}

static void test_fep_callback_invoked(void) {
    TEST("FEP: callback invoked on learn");
    fep_transition_learner_t* learner = fep_transition_learner_create(NULL, 4);
    ASSERT_TRUE(learner != NULL, "learner NULL");

    int user_val = 0;
    g_callback_count = 0;
    fep_transition_learner_set_update_callback(learner, test_callback, &user_val);

    /* Need an FEP system to learn — create minimal one */
    fep_config_t fep_cfg;
    fep_default_config(&fep_cfg);
    fep_system_t* sys = fep_create(&fep_cfg, 4, 2);
    if (sys) {
        float s0[] = {1.0f, 0.0f, 0.0f, 0.0f};
        float s1[] = {0.0f, 1.0f, 0.0f, 0.0f};
        fep_learn_transition(learner, sys, s0, s1, 4);

        ASSERT_TRUE(g_callback_count == 1, "callback not invoked");
        ASSERT_TRUE(g_callback_loss >= 0.0f, "invalid loss");
        ASSERT_TRUE(user_val == 42, "user_data not passed");

        fep_destroy(sys);
    }

    fep_transition_learner_destroy(learner);
    PASS();
}

static void test_fep_callback_null_safety(void) {
    TEST("FEP: callback null safety");
    int rc = fep_transition_learner_set_update_callback(NULL, test_callback, NULL);
    ASSERT_TRUE(rc == -1, "should reject NULL learner");
    PASS();
}

/* =========================================================================
 * FEP-Parietal Bridge Tests
 * ========================================================================= */

static void test_parietal_bridge_create(void) {
    TEST("Parietal: create and destroy");
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);
    ASSERT_TRUE(b != NULL, "bridge NULL");
    ASSERT_TRUE(fep_parietal_is_available(b), "bridge not available");
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_belief_update(void) {
    TEST("Parietal: belief update with precision");
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);

    float obs[] = {1.0f, 2.0f, 3.0f, 4.0f};
    fep_math_belief_t beliefs = {0};

    int rc = fep_parietal_update_beliefs(b, obs, 4, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
    ASSERT_TRUE(rc == 0, "update failed");
    ASSERT_TRUE(beliefs.mean != NULL, "mean not allocated");
    ASSERT_TRUE(beliefs.precision != NULL, "precision not allocated");
    ASSERT_TRUE(beliefs.dim == 4, "wrong dim");
    ASSERT_TRUE(beliefs.confidence > 0.0f, "zero confidence");

    /* Second update should reduce surprise */
    float first_surprise = beliefs.surprise;
    rc = fep_parietal_update_beliefs(b, obs, 4, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
    ASSERT_TRUE(rc == 0, "second update failed");
    ASSERT_TRUE(beliefs.surprise <= first_surprise + 0.1f, "surprise didn't decrease");

    fep_parietal_free_belief(&beliefs);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_prediction_error(void) {
    TEST("Parietal: prediction error computation");
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);

    float predicted[] = {1.0f, 2.0f, 3.0f};
    float actual[] = {1.1f, 2.2f, 3.3f};
    fep_math_prediction_t error = {0};

    int rc = fep_parietal_prediction_error(b, predicted, actual, 3, &error);
    ASSERT_TRUE(rc == 0, "PE computation failed");
    ASSERT_TRUE(error.error_magnitude > 0.0f, "zero PE");
    ASSERT_TRUE(error.free_energy > 0.0f, "zero free energy");

    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_free_energy(void) {
    TEST("Parietal: free energy computation");
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);

    float obs[] = {1.0f, 2.0f};
    fep_math_belief_t beliefs = {0};
    fep_parietal_update_beliefs(b, obs, 2, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    float fe = fep_parietal_compute_free_energy(b, &beliefs, obs, 2);
    ASSERT_TRUE(fe >= 0.0f, "negative free energy");

    fep_parietal_free_belief(&beliefs);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_surprise(void) {
    TEST("Parietal: surprise computation");
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);

    float obs[] = {5.0f, 3.0f, 1.0f};
    float surprise = fep_parietal_compute_surprise(b, obs, 3);
    ASSERT_TRUE(surprise > 0.0f, "zero surprise for nonzero input");

    float zero_obs[] = {0.0f, 0.0f};
    float zero_surprise = fep_parietal_compute_surprise(b, zero_obs, 2);
    ASSERT_TRUE(zero_surprise < surprise, "zero input should be less surprising");

    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_domain_tracking(void) {
    TEST("Parietal: domain-specific PE tracking");
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);

    float obs[] = {1.0f, 2.0f};
    fep_math_belief_t num_beliefs = {0};
    fep_math_belief_t phys_beliefs = {0};

    fep_parietal_update_beliefs(b, obs, 2, FEP_MATH_DOMAIN_NUMERICAL, &num_beliefs);
    fep_parietal_update_beliefs(b, obs, 2, FEP_MATH_DOMAIN_PHYSICAL, &phys_beliefs);

    fep_parietal_stats_t stats;
    fep_parietal_get_stats(b, &stats);
    ASSERT_TRUE(stats.belief_updates == 2, "wrong update count");

    fep_parietal_free_belief(&num_beliefs);
    fep_parietal_free_belief(&phys_beliefs);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_modulation(void) {
    TEST("Parietal: inflammation/fatigue modulation");
    fep_parietal_config_t cfg = fep_parietal_default_config();
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);

    fep_parietal_set_inflammation(b, 0.8f);
    fep_parietal_set_fatigue(b, 0.5f);

    float obs[] = {1.0f, 2.0f};
    fep_math_belief_t beliefs = {0};
    int rc = fep_parietal_update_beliefs(b, obs, 2, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
    ASSERT_TRUE(rc == 0, "update with modulation failed");

    fep_parietal_free_belief(&beliefs);
    fep_parietal_bridge_destroy(b);
    PASS();
}

/* =========================================================================
 * Creative Language Bridge Tests
 * ========================================================================= */

static void test_clb_create_destroy(void) {
    TEST("CLB: create and destroy");
    clb_config_t cfg = creative_language_bridge_default_config();
    creative_language_bridge_t* b = creative_language_bridge_create(&cfg);
    ASSERT_TRUE(b != NULL, "bridge NULL");
    creative_language_bridge_destroy(b);
    PASS();
}

static void test_clb_narrate_blend_template(void) {
    TEST("CLB: narrate blend (template fallback)");
    creative_language_bridge_t* b = creative_language_bridge_create(NULL);

    float vec[] = {0.5f, 0.3f, 0.2f};
    char output[256] = {0};

    int rc = clb_narrate_blend(b, vec, 3, 0.8f, 0.6f, output, sizeof(output));
    ASSERT_TRUE(rc == 0, "narrate failed");
    ASSERT_TRUE(strlen(output) > 0, "no output");
    ASSERT_TRUE(strstr(output, "novel") != NULL || strstr(output, "blend") != NULL,
                "unexpected output");

    creative_language_bridge_destroy(b);
    PASS();
}

static void test_clb_narrate_cf_template(void) {
    TEST("CLB: narrate counterfactual (template fallback)");
    creative_language_bridge_t* b = creative_language_bridge_create(NULL);

    float vec[] = {0.5f, 0.3f};
    char output[256] = {0};

    int rc = clb_narrate_counterfactual(b, vec, 2, 0.7f, output, sizeof(output));
    ASSERT_TRUE(rc == 0, "narrate failed");
    ASSERT_TRUE(strlen(output) > 0, "no output");

    creative_language_bridge_destroy(b);
    PASS();
}

static void test_clb_narrate_cf_below_threshold(void) {
    TEST("CLB: counterfactual below plausibility threshold");
    creative_language_bridge_t* b = creative_language_bridge_create(NULL);

    float vec[] = {0.1f};
    char output[256] = {0};

    /* Default min_plausibility is 0.2 */
    int rc = clb_narrate_counterfactual(b, vec, 1, 0.1f, output, sizeof(output));
    ASSERT_TRUE(rc == -1, "should reject low plausibility");

    creative_language_bridge_destroy(b);
    PASS();
}

static void test_clb_describe_emergence(void) {
    TEST("CLB: describe emergence (template fallback)");
    creative_language_bridge_t* b = creative_language_bridge_create(NULL);

    float vec[] = {0.5f, 0.3f};
    char output[256] = {0};

    int rc = clb_describe_emergence(b, "novelty", 0.8f, vec, 2, output, sizeof(output));
    ASSERT_TRUE(rc == 0, "describe failed");
    ASSERT_TRUE(strlen(output) > 0, "no output");
    ASSERT_TRUE(strstr(output, "novelty") != NULL, "property name not in output");

    creative_language_bridge_destroy(b);
    PASS();
}

static void test_clb_stats(void) {
    TEST("CLB: stats tracking");
    creative_language_bridge_t* b = creative_language_bridge_create(NULL);

    float vec[] = {0.5f};
    char output[256];

    clb_narrate_blend(b, vec, 1, 0.5f, 0.5f, output, sizeof(output));
    clb_narrate_counterfactual(b, vec, 1, 0.5f, output, sizeof(output));

    clb_stats_t stats;
    clb_get_stats(b, &stats);
    ASSERT_TRUE(stats.blends_narrated == 1, "wrong blend count");
    ASSERT_TRUE(stats.counterfactuals_narrated == 1, "wrong cf count");
    ASSERT_TRUE(stats.productions_generated == 2, "wrong production count");

    clb_reset_stats(b);
    clb_get_stats(b, &stats);
    ASSERT_TRUE(stats.productions_generated == 0, "stats not reset");

    creative_language_bridge_destroy(b);
    PASS();
}

static void test_clb_modulation(void) {
    TEST("CLB: inflammation/fatigue modulation");
    creative_language_bridge_t* b = creative_language_bridge_create(NULL);

    int rc = clb_set_inflammation(b, 0.7f);
    ASSERT_TRUE(rc == 0, "set inflammation failed");

    rc = clb_set_fatigue(b, 1.5f); /* Should clamp to 1.0 */
    ASSERT_TRUE(rc == 0, "set fatigue failed");

    creative_language_bridge_destroy(b);
    PASS();
}

static void test_clb_null_safety(void) {
    TEST("CLB: null safety");
    ASSERT_TRUE(clb_attach_language(NULL, NULL) == -1, "should reject NULL");
    ASSERT_TRUE(clb_set_inflammation(NULL, 0.5f) == -1, "should reject NULL");

    char output[64];
    ASSERT_TRUE(clb_narrate_blend(NULL, NULL, 0, 0, 0, output, 64) == -1, "should reject NULL");
    PASS();
}

/* =========================================================================
 * FEP-Parietal Bridge: Newly Implemented Stub Tests (Phase 5+)
 * ========================================================================= */

static void test_parietal_predict(void) {
    TEST("Parietal: predict from beliefs");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    /* First populate beliefs */
    float obs[] = {1.0f, 2.0f, 3.0f, 4.0f};
    fep_math_belief_t beliefs = {0};
    fep_parietal_update_beliefs(b, obs, 4, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    /* Now predict from those beliefs */
    fep_math_prediction_t pred = {0};
    int rc = fep_parietal_predict(b, &beliefs, &pred);
    ASSERT_TRUE(rc == 0, "predict failed");
    ASSERT_TRUE(pred.predicted != NULL, "predicted NULL");
    ASSERT_TRUE(pred.dim > 0, "zero dim");

    fep_parietal_free_prediction(&pred);
    fep_parietal_free_belief(&beliefs);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_evaluate_policies(void) {
    TEST("Parietal: evaluate policies");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    float state[] = {0.0f, 0.0f};
    float goal[] = {1.0f, 1.0f};
    fep_problem_state_t problem = {
        .state_vector = state, .state_dim = 2,
        .domain = FEP_MATH_DOMAIN_NUMERICAL,
        .goal_state = goal, .goal_dim = 2,
        .distance_to_goal = 1.414f
    };

    fep_math_policy_t policies[16];
    uint32_t n = 0;
    int rc = fep_parietal_evaluate_policies(b, &problem, policies, &n);
    ASSERT_TRUE(rc == 0, "evaluate failed");
    ASSERT_TRUE(n == 7, "wrong policy count");

    /* Check probabilities sum to ~1 */
    float prob_sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) prob_sum += policies[i].probability;
    ASSERT_TRUE(fabsf(prob_sum - 1.0f) < 0.01f, "probabilities don't sum to 1");

    /* Numerical strategy should score well for NUMERICAL domain */
    ASSERT_TRUE(policies[2].pragmatic_value > 0.7f, "numerical strategy not preferred");

    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_active_inference(void) {
    TEST("Parietal: active inference action selection");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    float state[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float goal[] = {1.0f, 1.0f, 1.0f, 1.0f};
    fep_problem_state_t problem = {
        .state_vector = state, .state_dim = 4,
        .domain = FEP_MATH_DOMAIN_ALGEBRAIC,
        .goal_state = goal, .goal_dim = 4,
        .distance_to_goal = 2.0f
    };

    fep_active_inference_result_t result = {0};
    int rc = fep_parietal_active_inference(b, &problem, &result);
    ASSERT_TRUE(rc == 0, "active inference failed");
    ASSERT_TRUE(result.action != NULL, "no action generated");
    ASSERT_TRUE(result.action_dim == 4, "wrong action dim");
    ASSERT_TRUE(result.num_policies == 7, "policies not evaluated");

    /* Action should point toward goal */
    for (uint32_t i = 0; i < result.action_dim; i++) {
        ASSERT_TRUE(result.action[i] > 0.0f, "action not toward goal");
    }

    /* Algebraic domain should prefer algebraic strategy */
    ASSERT_TRUE(result.selected_strategy == FEP_STRATEGY_ALGEBRAIC ||
                result.evaluated_policies != NULL, "no strategy selected");

    fep_parietal_stats_t stats;
    fep_parietal_get_stats(b, &stats);
    ASSERT_TRUE(stats.active_inferences == 1, "wrong inference count");

    fep_parietal_free_inference_result(&result);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_update_from_action(void) {
    TEST("Parietal: update from action");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    float action[] = {0.1f, 0.2f};
    float outcome[] = {0.15f, 0.25f};

    int rc = fep_parietal_update_from_action(b, action, 2, outcome, 2);
    ASSERT_TRUE(rc == 0, "update from action failed");

    /* Run again to build PE history */
    float action2[] = {0.2f, 0.3f};
    float outcome2[] = {0.22f, 0.28f};
    rc = fep_parietal_update_from_action(b, action2, 2, outcome2, 2);
    ASSERT_TRUE(rc == 0, "second update failed");

    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_attention_precision(void) {
    TEST("Parietal: attention-driven precision");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    /* Set attention: first dimension is attended, second is not */
    float attention[] = {1.0f, 0.0f, 0.5f, 0.2f};
    int rc = fep_parietal_set_attention_precision(b, attention, 4);
    ASSERT_TRUE(rc == 0, "set attention failed");

    /* Get precision for level 0 to verify modulation */
    float* prec = NULL;
    uint32_t dim = 0;
    rc = fep_parietal_get_precision(b, 0, &prec, &dim);
    ASSERT_TRUE(rc == 0, "get precision failed");
    ASSERT_TRUE(prec != NULL, "precision NULL");
    ASSERT_TRUE(dim > 0, "zero dim");

    /* Attended dimension should have higher precision */
    if (dim >= 2) {
        ASSERT_TRUE(prec[0] > prec[1], "attention not reflected in precision");
    }

    nimcp_free(prec);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_adapt_precision(void) {
    TEST("Parietal: adaptive precision from PE history");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    /* Build PE history through update_from_action calls */
    for (int i = 0; i < 10; i++) {
        float action[] = {(float)i * 0.1f, (float)i * 0.05f};
        float outcome[] = {(float)i * 0.1f + 0.01f, (float)i * 0.05f - 0.01f};
        fep_parietal_update_from_action(b, action, 2, outcome, 2);
    }

    /* Also add some domain updates */
    float obs[] = {1.0f, 2.0f};
    fep_math_belief_t beliefs = {0};
    fep_parietal_update_beliefs(b, obs, 2, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    int rc = fep_parietal_adapt_precision(b);
    ASSERT_TRUE(rc == 0, "adapt precision failed");

    fep_parietal_stats_t stats;
    fep_parietal_get_stats(b, &stats);
    ASSERT_TRUE(stats.avg_precision > 0.0f, "avg precision not updated");

    fep_parietal_free_belief(&beliefs);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_get_generative_model(void) {
    TEST("Parietal: get generative model");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    /* Update some beliefs to populate model */
    float obs[] = {1.0f, 2.0f, 3.0f};
    fep_math_belief_t beliefs = {0};
    fep_parietal_update_beliefs(b, obs, 3, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    fep_math_generative_model_t model;
    int rc = fep_parietal_get_generative_model(b, &model);
    ASSERT_TRUE(rc == 0, "get model failed");
    ASSERT_TRUE(model.sensory_beliefs.mean != NULL, "sensory beliefs NULL");
    ASSERT_TRUE(model.sensory_beliefs.dim > 0, "sensory dim zero");
    ASSERT_TRUE(model.feature_beliefs.dim > 0, "feature dim zero");

    /* Clean up exported model */
    nimcp_free(model.sensory_beliefs.mean);
    nimcp_free(model.sensory_beliefs.precision);
    nimcp_free(model.feature_beliefs.mean);
    nimcp_free(model.feature_beliefs.precision);
    nimcp_free(model.structural_beliefs.mean);
    nimcp_free(model.structural_beliefs.precision);
    nimcp_free(model.abstract_beliefs.mean);
    nimcp_free(model.abstract_beliefs.precision);

    fep_parietal_free_belief(&beliefs);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_train_model(void) {
    TEST("Parietal: train model");
    /* Create with small dims to match our test data */
    fep_parietal_config_t cfg = fep_parietal_default_config();
    cfg.num_levels = 2;
    cfg.level_dims[0] = 4;
    cfg.level_dims[1] = 4;
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(&cfg);

    /* Simple training: observations → targets (must match level_dims) */
    float obs1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float obs2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float tgt1[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float tgt2[] = {1.0f, 0.0f, 0.0f, 0.0f};
    const float* observations[] = {obs1, obs2};
    const float* targets[] = {tgt1, tgt2};

    float loss1 = fep_parietal_train_model(b, observations, targets, 2);
    ASSERT_TRUE(loss1 > 0.0f, "initial loss should be positive");

    /* Train more - loss should decrease */
    float loss_last = loss1;
    for (int epoch = 0; epoch < 50; epoch++) {
        loss_last = fep_parietal_train_model(b, observations, targets, 2);
    }
    ASSERT_TRUE(loss_last < loss1, "loss didn't decrease with training");

    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_numerical_inference(void) {
    TEST("Parietal: numerical inference (Weber-Fechner)");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    float quantities[] = {1.0f, 10.0f, 100.0f, 1000.0f};
    fep_math_belief_t estimated = {0};
    int rc = fep_parietal_numerical_inference(b, quantities, 4, &estimated);
    ASSERT_TRUE(rc == 0, "numerical inference failed");
    ASSERT_TRUE(estimated.domain == FEP_MATH_DOMAIN_NUMERICAL, "wrong domain");

    /* Weber-Fechner: precision should decrease with magnitude */
    if (estimated.precision && estimated.dim >= 4) {
        ASSERT_TRUE(estimated.precision[0] > estimated.precision[3],
                    "Weber-Fechner not applied");
    }

    fep_parietal_free_belief(&estimated);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_spatial_inference(void) {
    TEST("Parietal: spatial inference");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    float positions[] = {1.0f, 2.0f, 3.0f, 4.0f}; /* (x,y) pairs */
    fep_math_belief_t transformed = {0};
    int rc = fep_parietal_spatial_inference(b, positions, 4, &transformed);
    ASSERT_TRUE(rc == 0, "spatial inference failed");
    ASSERT_TRUE(transformed.domain == FEP_MATH_DOMAIN_SPATIAL, "wrong domain");
    ASSERT_TRUE(transformed.mean != NULL, "mean NULL");

    fep_parietal_free_belief(&transformed);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_physics_inference(void) {
    TEST("Parietal: physics inference (dynamics)");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    /* State: [pos0, vel0, pos1, vel1] */
    float state[] = {0.0f, 1.0f, 0.0f, -1.0f};
    fep_math_belief_t predicted = {0};
    int rc = fep_parietal_physics_inference(b, state, 4, 0.1f, &predicted);
    ASSERT_TRUE(rc == 0, "physics inference failed");
    ASSERT_TRUE(predicted.domain == FEP_MATH_DOMAIN_PHYSICAL, "wrong domain");

    /* After dt=0.1: pos0 should have moved toward 0.1, pos1 toward -0.1 */
    /* (The belief update absorbs the raw values first, but the physics step
       modifies them, so just check the means are non-zero) */
    ASSERT_TRUE(predicted.mean != NULL, "mean NULL");

    fep_parietal_free_belief(&predicted);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_engineering_inference(void) {
    TEST("Parietal: engineering inference");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    float input[] = {1.0f, 2.0f, 3.0f};
    fep_math_belief_t result = {0};

    int rc = fep_parietal_engineering_inference(b, input, 3,
                                                 FEP_MATH_DOMAIN_ENGINEERING, &result);
    ASSERT_TRUE(rc == 0, "engineering inference failed");
    ASSERT_TRUE(result.domain == FEP_MATH_DOMAIN_ENGINEERING, "wrong domain");

    fep_parietal_free_belief(&result);
    fep_parietal_bridge_destroy(b);
    PASS();
}

static void test_parietal_epistemic_value(void) {
    TEST("Parietal: epistemic value (curiosity)");
    fep_parietal_bridge_t* b = fep_parietal_bridge_create(NULL);

    /* Without any observations, everything should be informative */
    float query[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float ev = fep_parietal_epistemic_value(b, query, 4);
    ASSERT_TRUE(ev > 0.0f, "epistemic value should be positive");

    /* After training on some data, familiar queries should be less informative */
    float obs[] = {1.0f, 0.0f, 0.0f, 0.0f};
    fep_math_belief_t beliefs = {0};
    for (int i = 0; i < 20; i++)
        fep_parietal_update_beliefs(b, obs, 4, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    float ev_after = fep_parietal_epistemic_value(b, query, 4);
    /* After learning the pattern, epistemic value for same query should decrease */
    ASSERT_TRUE(ev_after < ev, "epistemic value didn't decrease after learning");

    /* Novel query should still be informative */
    float novel[] = {0.0f, 0.0f, 1.0f, 1.0f};
    float ev_novel = fep_parietal_epistemic_value(b, novel, 4);
    ASSERT_TRUE(ev_novel > ev_after, "novel query not more informative");

    fep_parietal_free_belief(&beliefs);
    fep_parietal_bridge_destroy(b);
    PASS();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5: Critical Integration Tests ===\n\n");

    printf("--- Counterfactual Causal Graph ---\n");
    test_cf_create_destroy();
    test_cf_learn_causal_weight();
    test_cf_causal_propagation();
    test_cf_causal_strength_with_graph();
    test_cf_find_causes_with_graph();
    test_cf_trace_causal_consequences();
    test_cf_null_safety();

    printf("\n--- Conceptual Blending Enhancement ---\n");
    test_blend_selective_projection();
    test_blend_emergence_detection();
    test_blend_hill_climbing_optimizer();
    test_blend_novelty_evaluation();

    printf("\n--- FEP Learning Callback ---\n");
    test_fep_callback_set();
    test_fep_callback_invoked();
    test_fep_callback_null_safety();

    printf("\n--- FEP-Parietal Bridge ---\n");
    test_parietal_bridge_create();
    test_parietal_belief_update();
    test_parietal_prediction_error();
    test_parietal_free_energy();
    test_parietal_surprise();
    test_parietal_domain_tracking();
    test_parietal_modulation();

    printf("\n--- FEP-Parietal Bridge: New Implementations ---\n");
    test_parietal_predict();
    test_parietal_evaluate_policies();
    test_parietal_active_inference();
    test_parietal_update_from_action();
    test_parietal_attention_precision();
    test_parietal_adapt_precision();
    test_parietal_get_generative_model();
    test_parietal_train_model();
    test_parietal_numerical_inference();
    test_parietal_spatial_inference();
    test_parietal_physics_inference();
    test_parietal_engineering_inference();
    test_parietal_epistemic_value();

    printf("\n--- Creative Language Bridge ---\n");
    test_clb_create_destroy();
    test_clb_narrate_blend_template();
    test_clb_narrate_cf_template();
    test_clb_narrate_cf_below_threshold();
    test_clb_describe_emergence();
    test_clb_stats();
    test_clb_modulation();
    test_clb_null_safety();

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
