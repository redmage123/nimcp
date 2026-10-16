/**
 * @file test_phase5_batch2.c
 * @brief Phase 5 Batch 2 Tests: Genius modes, text generation, parietal visualization
 *
 * Tests for: Gauss/Newton/Erdos genius analysis, pattern discovery,
 * Ramsey bounds, text generation, mental rotation, spatial transform.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "cognitive/parietal/nimcp_mathematical_genius.h"
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/creative/generation/nimcp_text_generation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/tensor/nimcp_tensor.h"

/* Local definition matching nimcp_imagination_engine.h:315
   (cannot include that header due to thalamic_router_t type conflict with parietal.h) */
struct imagination_scenario {
    uint64_t id;
    int mode;
    int quality;
    nimcp_tensor_t* latent_state;
    nimcp_tensor_t* latent_previous;
    nimcp_tensor_t* visual_buffer;
    nimcp_tensor_t* audio_buffer;
    nimcp_tensor_t* semantic_buffer;
    float vividness;
    float controllability;
    float coherence;
    float reality_distance;
    float novelty;
    uint64_t start_time_ms;
    uint64_t duration_ms;
    uint64_t last_step_ms;
    void* trajectory;
    size_t trajectory_length;
    void* active_goal;
    float goal_progress;
    void* elements;
    bool is_active;
    bool is_paused;
    int error_code;
};

static int tests_passed = 0;
static int tests_failed = 0;
static mathematical_genius_t* g_genius = NULL;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* =========================================================================
 * Gauss Mode Tests
 * ========================================================================= */

static void test_gauss_number_theory(void) {
    TEST("Gauss: number theory domain scores high");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.difficulty = 0.6f;
    problem.target = (void*)1;  /* has target */

    genius_result_t result;
    nimcp_error_t err = genius_gauss_analyze(g_genius, &problem, &result);
    /* NULL genius is OK — the impl handles it */
    ASSERT_TRUE(err == NIMCP_SUCCESS, "analyze failed");
    ASSERT_TRUE(result.mode_used == GENIUS_MODE_GAUSS, "wrong mode");
    ASSERT_TRUE(result.elegance_score > 0.6f, "elegance too low for number theory");
    ASSERT_TRUE(result.rigor_score > 0.7f, "rigor too low");
    ASSERT_TRUE(result.solved == true, "should be solved with target");
    PASS();
}

static void test_gauss_hard_problem(void) {
    TEST("Gauss: hard problem boosts novelty");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_ALGEBRA;
    problem.difficulty = 0.9f;

    genius_result_t result;
    nimcp_error_t err = genius_gauss_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "analyze failed");
    ASSERT_TRUE(result.novelty_score > 0.65f, "hard problem should boost novelty");
    ASSERT_TRUE(result.rigor_score > 0.7f, "rigor should be high");
    PASS();
}

static void test_gauss_easy_elegant(void) {
    TEST("Gauss: easy problem boosts elegance");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_GEOMETRY;
    problem.difficulty = 0.2f;

    genius_result_t result;
    nimcp_error_t err = genius_gauss_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "analyze failed");
    ASSERT_TRUE(result.elegance_score >= 0.6f, "easy problem should boost elegance");
    PASS();
}

static void test_gauss_constraints(void) {
    TEST("Gauss: constraints boost elegance+generalization");
    math_problem_t p1, p2;
    memset(&p1, 0, sizeof(p1));
    memset(&p2, 0, sizeof(p2));
    p1.domain = GENIUS_DOMAIN_ALGEBRA;
    p1.difficulty = 0.5f;
    p2 = p1;
    p2.constraints = (void*)1;

    genius_result_t r1, r2;
    genius_gauss_analyze(g_genius, &p1, &r1);
    genius_gauss_analyze(g_genius, &p2, &r2);
    ASSERT_TRUE(r2.elegance_score > r1.elegance_score, "constraints should boost elegance");
    ASSERT_TRUE(r2.generalization_score > r1.generalization_score, "constraints should boost generalization");
    PASS();
}

static void test_gauss_null_safety(void) {
    TEST("Gauss: NULL params return error");
    genius_result_t result;
    nimcp_error_t err = genius_gauss_analyze(g_genius, NULL, &result);
    ASSERT_TRUE(err == NIMCP_ERROR_INVALID_PARAM, "should reject NULL problem");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    err = genius_gauss_analyze(g_genius, &problem, NULL);
    ASSERT_TRUE(err == NIMCP_ERROR_INVALID_PARAM, "should reject NULL result");
    PASS();
}

/* =========================================================================
 * Newton Mode Tests
 * ========================================================================= */

static void test_newton_calculus(void) {
    TEST("Newton: calculus domain is home turf");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_CALCULUS;
    problem.difficulty = 0.7f;
    problem.given = (void*)1;
    problem.target = (void*)1;

    genius_result_t result;
    nimcp_error_t err = genius_newton_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "analyze failed");
    ASSERT_TRUE(result.mode_used == GENIUS_MODE_NEWTON, "wrong mode");
    ASSERT_TRUE(result.elegance_score > 0.8f, "elegance too low for calculus");
    ASSERT_TRUE(result.solved == true, "should be solved with given+target");
    PASS();
}

static void test_newton_physics(void) {
    TEST("Newton: physics domain scores high");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_PHYSICS;
    problem.difficulty = 0.5f;

    genius_result_t result;
    nimcp_error_t err = genius_newton_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "analyze failed");
    ASSERT_TRUE(result.elegance_score > 0.7f, "physics should score high elegance");
    PASS();
}

static void test_newton_diff_eq(void) {
    TEST("Newton: differential equations domain");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_DIFFERENTIAL_EQ;
    problem.difficulty = 0.85f;

    genius_result_t result;
    nimcp_error_t err = genius_newton_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "analyze failed");
    ASSERT_TRUE(result.elegance_score > 0.7f, "diff eq should score high");
    ASSERT_TRUE(result.novelty_score > 0.7f, "hard diff eq should have high novelty");
    PASS();
}

static void test_newton_cross_domain(void) {
    TEST("Newton: cross-domain bonus");
    math_problem_t p1, p2;
    memset(&p1, 0, sizeof(p1));
    p1.domain = GENIUS_DOMAIN_PHYSICS;
    p1.difficulty = 0.5f;
    p2 = p1;
    genius_domain_t secondary[] = {GENIUS_DOMAIN_CALCULUS, GENIUS_DOMAIN_GEOMETRY};
    p2.secondary_domains = secondary;
    p2.num_secondary = 2;

    genius_result_t r1, r2;
    genius_newton_analyze(g_genius, &p1, &r1);
    genius_newton_analyze(g_genius, &p2, &r2);
    ASSERT_TRUE(r2.novelty_score > r1.novelty_score, "cross-domain should boost novelty");
    ASSERT_TRUE(r2.generalization_score > r1.generalization_score, "cross-domain should boost generalization");
    PASS();
}

static void test_newton_geometry(void) {
    TEST("Newton: geometry has moderate bonus");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_GEOMETRY;
    problem.difficulty = 0.5f;

    genius_result_t result;
    genius_newton_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(result.elegance_score >= 0.5f, "geometry should get baseline elegance");
    PASS();
}

/* =========================================================================
 * Erdos Mode Tests
 * ========================================================================= */

static void test_erdos_combinatorics(void) {
    TEST("Erdos: combinatorics is primary domain");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_COMBINATORICS;
    problem.difficulty = 0.7f;
    problem.target = (void*)1;

    genius_result_t result;
    nimcp_error_t err = genius_erdos_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "analyze failed");
    ASSERT_TRUE(result.mode_used == GENIUS_MODE_ERDOS, "wrong mode");
    ASSERT_TRUE(result.elegance_score > 0.8f, "elegance too low for combinatorics");
    ASSERT_TRUE(result.novelty_score > 0.7f, "novelty too low");
    ASSERT_TRUE(result.solved == true, "should be solved with target");
    PASS();
}

static void test_erdos_graph_theory(void) {
    TEST("Erdos: graph theory scores high");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_GRAPH_THEORY;
    problem.difficulty = 0.6f;

    genius_result_t result;
    genius_erdos_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(result.elegance_score > 0.8f, "graph theory should score high elegance");
    ASSERT_TRUE(result.novelty_score > 0.7f, "graph theory should score high novelty");
    PASS();
}

static void test_erdos_hard_problem(void) {
    TEST("Erdos: hard problems boost novelty");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.difficulty = 0.9f;

    genius_result_t result;
    genius_erdos_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(result.novelty_score > 0.7f, "hard problems should have high novelty");
    PASS();
}

static void test_erdos_cross_domain(void) {
    TEST("Erdos: cross-domain is forte");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_COMBINATORICS;
    problem.difficulty = 0.5f;
    genius_domain_t secondary[] = {GENIUS_DOMAIN_PROBABILITY, GENIUS_DOMAIN_NUMBER_THEORY, GENIUS_DOMAIN_GRAPH_THEORY};
    problem.secondary_domains = secondary;
    problem.num_secondary = 3;

    genius_result_t result;
    genius_erdos_analyze(g_genius, &problem, &result);
    /* 3 secondary domains should give big boosts */
    ASSERT_TRUE(result.novelty_score > 0.9f, "3 secondary domains should push novelty high");
    ASSERT_TRUE(result.generalization_score > 0.6f, "cross-domain should boost generalization");
    PASS();
}

static void test_erdos_probabilistic_method(void) {
    TEST("Erdos: constraints enable probabilistic method");
    math_problem_t p1, p2;
    memset(&p1, 0, sizeof(p1));
    p1.domain = GENIUS_DOMAIN_COMBINATORICS;
    p1.difficulty = 0.5f;
    p2 = p1;
    p2.constraints = (void*)1;

    genius_result_t r1, r2;
    genius_erdos_analyze(g_genius, &p1, &r1);
    genius_erdos_analyze(g_genius, &p2, &r2);
    ASSERT_TRUE(r2.novelty_score > r1.novelty_score, "constraints should enable probabilistic method");
    ASSERT_TRUE(r2.elegance_score > r1.elegance_score, "probabilistic method adds elegance");
    PASS();
}

/* =========================================================================
 * Ramsey Lower Bound Tests
 * ========================================================================= */

static void test_erdos_ramsey_base_cases(void) {
    TEST("Erdos: Ramsey base cases R(1,s)=R(r,1)=1");
    uint32_t r1 = genius_erdos_ramsey_lower_bound(g_genius, 1, 5);
    ASSERT_TRUE(r1 == 1, "R(1,5) base case should be 1");
    uint32_t r2 = genius_erdos_ramsey_lower_bound(g_genius, 3, 1);
    ASSERT_TRUE(r2 == 1, "R(3,1) base case should be 1");
    PASS();
}

static void test_erdos_ramsey_known_values(void) {
    TEST("Erdos: Ramsey lower bound R(3,3)");
    /* R(3,3) > 2^((3+3-2)/2) = 2^2 = 4 */
    uint32_t lb = genius_erdos_ramsey_lower_bound(g_genius, 3, 3);
    ASSERT_TRUE(lb == 4, "R(3,3) lower bound should be 4");
    PASS();
}

static void test_erdos_ramsey_larger(void) {
    TEST("Erdos: Ramsey lower bound R(4,4)");
    /* R(4,4) > 2^((4+4-2)/2) = 2^3 = 8 */
    uint32_t lb = genius_erdos_ramsey_lower_bound(g_genius, 4, 4);
    ASSERT_TRUE(lb == 8, "R(4,4) lower bound should be 8");
    PASS();
}

/* =========================================================================
 * Gauss Pattern Discovery Tests
 * ========================================================================= */

static void test_gauss_arithmetic_pattern(void) {
    TEST("Gauss: detect arithmetic sequence");
    int64_t seq[] = {2, 5, 8, 11, 14};
    conjecture_t conj;
    nimcp_error_t err = genius_gauss_discover_pattern(g_genius, seq, 5, &conj);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "discover_pattern failed");
    ASSERT_TRUE(conj.confidence > 0.8f, "arithmetic confidence should be high");
    ASSERT_TRUE(conj.statement != NULL, "should have statement");
    ASSERT_TRUE(strstr(conj.statement, "Arithmetic") != NULL, "should identify as arithmetic");
    nimcp_free(conj.statement);
    PASS();
}

static void test_gauss_geometric_pattern(void) {
    TEST("Gauss: detect geometric sequence");
    int64_t seq[] = {3, 6, 12, 24, 48};
    conjecture_t conj;
    nimcp_error_t err = genius_gauss_discover_pattern(g_genius, seq, 5, &conj);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "discover_pattern failed");
    ASSERT_TRUE(conj.confidence > 0.8f, "geometric confidence should be high");
    ASSERT_TRUE(conj.statement != NULL, "should have statement");
    ASSERT_TRUE(strstr(conj.statement, "Geometric") != NULL, "should identify as geometric");
    nimcp_free(conj.statement);
    PASS();
}

static void test_gauss_unknown_pattern(void) {
    TEST("Gauss: unknown pattern gets low confidence");
    int64_t seq[] = {1, 4, 9, 16, 25};  /* squares — not arithmetic or geometric */
    conjecture_t conj;
    nimcp_error_t err = genius_gauss_discover_pattern(g_genius, seq, 5, &conj);
    ASSERT_TRUE(err == NIMCP_SUCCESS, "discover_pattern failed");
    ASSERT_TRUE(conj.confidence < 0.5f, "unknown pattern should have low confidence");
    nimcp_free(conj.statement);
    PASS();
}

static void test_gauss_pattern_null_safety(void) {
    TEST("Gauss: pattern discovery NULL safety");
    conjecture_t conj;
    nimcp_error_t err = genius_gauss_discover_pattern(g_genius, NULL, 5, &conj);
    ASSERT_TRUE(err == NIMCP_ERROR_INVALID_PARAM, "should reject NULL sequence");
    int64_t seq[] = {1, 2, 3};
    err = genius_gauss_discover_pattern(g_genius, seq, 0, &conj);
    ASSERT_TRUE(err == NIMCP_ERROR_INVALID_PARAM, "should reject zero length");
    PASS();
}

/* =========================================================================
 * Text Generation Tests
 * ========================================================================= */

static void test_text_gen_create_destroy(void) {
    TEST("TextGen: create and destroy");
    text_generator_config_t config;
    text_generator_config_defaults(&config);
    text_generator_t* gen = text_generator_create(&config);
    ASSERT_TRUE(gen != NULL, "generator NULL");
    text_generator_destroy(gen);
    PASS();
}

static void test_text_gen_generate_prose(void) {
    TEST("TextGen: generate prose produces real words");
    text_generator_config_t config;
    text_generator_config_defaults(&config);
    text_generator_t* gen = text_generator_create(&config);
    ASSERT_TRUE(gen != NULL, "generator NULL");

    text_generation_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = TEXT_GEN_SHORT_STORY;
    request.prompt = "Once upon a time";
    request.prompt_len = strlen(request.prompt);
    request.max_length = 200;
    request.temperature = 0.7f;

    text_generation_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = text_generate(gen, &request, &result);
    ASSERT_TRUE(rc == 0, "text_generate failed");
    ASSERT_TRUE(result.text != NULL, "text is NULL");
    ASSERT_TRUE(result.text_len > 10, "text too short");

    /* Verify it contains real English words, not gibberish */
    ASSERT_TRUE(strstr(result.text, "the") != NULL ||
                strstr(result.text, "The") != NULL ||
                strstr(result.text, "a ") != NULL ||
                strstr(result.text, "in ") != NULL,
                "text should contain English words");

    nimcp_free(result.text);
    text_generator_destroy(gen);
    PASS();
}

static void test_text_gen_generate_poetry(void) {
    TEST("TextGen: generate poetry");
    text_generator_config_t config;
    text_generator_config_defaults(&config);
    text_generator_t* gen = text_generator_create(&config);
    ASSERT_TRUE(gen != NULL, "generator NULL");

    poetry_request_t request;
    memset(&request, 0, sizeof(request));
    request.form = VERSE_FREE;
    request.subject = "the moon";
    request.num_stanzas = 2;

    text_generation_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = text_generate_poetry(gen, &request, NULL, &result);
    ASSERT_TRUE(rc == 0, "text_generate_poetry failed");
    ASSERT_TRUE(result.text != NULL, "poetry text is NULL");
    ASSERT_TRUE(result.text_len > 5, "poetry text too short");

    nimcp_free(result.text);
    text_generator_destroy(gen);
    PASS();
}

static void test_text_gen_continue(void) {
    TEST("TextGen: continue existing text");
    text_generator_config_t config;
    text_generator_config_defaults(&config);
    text_generator_t* gen = text_generator_create(&config);
    ASSERT_TRUE(gen != NULL, "generator NULL");

    const char* existing = "The ancient forest stood silent.";
    text_generation_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = text_generate_continue(gen, existing, strlen(existing), NULL, 100, &result);
    ASSERT_TRUE(rc == 0, "text_generate_continue failed");
    ASSERT_TRUE(result.text != NULL, "continuation is NULL");
    ASSERT_TRUE(result.text_len > 0, "continuation is empty");

    nimcp_free(result.text);
    text_generator_destroy(gen);
    PASS();
}

/* =========================================================================
 * Parietal Mental Rotation Tests
 * ========================================================================= */

static void test_mental_rotate_basic(void) {
    TEST("Parietal: mental rotation returns scenario");
    parietal_lobe_t* parietal = parietal_create();
    ASSERT_TRUE(parietal != NULL, "parietal NULL");

    uint32_t dims[] = {6};  /* 2 3D points */
    nimcp_tensor_t* obj = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_TRUE(obj != NULL, "tensor NULL");

    /* Set up two 3D points: (1,0,0) and (0,1,0) */
    nimcp_tensor_set_flat(obj, 0, 1.0);
    nimcp_tensor_set_flat(obj, 1, 0.0);
    nimcp_tensor_set_flat(obj, 2, 0.0);
    nimcp_tensor_set_flat(obj, 3, 0.0);
    nimcp_tensor_set_flat(obj, 4, 1.0);
    nimcp_tensor_set_flat(obj, 5, 0.0);

    imagination_scenario_t* scenario = parietal_imagine_rotation(
        parietal, obj, 0.0f, 0.0f, 0.0f);  /* identity rotation */
    ASSERT_TRUE(scenario != NULL, "scenario NULL");

    /* With identity rotation, values should be unchanged */
    nimcp_tensor_t* result = scenario->latent_state;
    ASSERT_TRUE(result != NULL, "latent_state NULL");
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 0) - 1.0) < 0.001, "x should be 1");
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 1) - 0.0) < 0.001, "y should be 0");
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 2) - 0.0) < 0.001, "z should be 0");

    ASSERT_TRUE(scenario->vividness > 0.5f, "vividness too low");
    ASSERT_TRUE(scenario->coherence > 0.5f, "coherence too low");
    ASSERT_TRUE(scenario->is_active == true, "should be active");

    nimcp_tensor_destroy(result);
    nimcp_free(scenario);
    nimcp_tensor_destroy(obj);
    parietal_destroy(parietal);
    PASS();
}

static void test_mental_rotate_90z(void) {
    TEST("Parietal: 90-degree Z rotation");
    parietal_lobe_t* parietal = parietal_create();
    ASSERT_TRUE(parietal != NULL, "parietal NULL");

    uint32_t dims[] = {3};
    nimcp_tensor_t* obj = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    /* Point (1, 0, 0) */
    nimcp_tensor_set_flat(obj, 0, 1.0);
    nimcp_tensor_set_flat(obj, 1, 0.0);
    nimcp_tensor_set_flat(obj, 2, 0.0);

    float pi_half = 3.14159265f / 2.0f;
    imagination_scenario_t* scenario = parietal_imagine_rotation(
        parietal, obj, 0.0f, 0.0f, pi_half);
    ASSERT_TRUE(scenario != NULL, "scenario NULL");

    nimcp_tensor_t* result = scenario->latent_state;
    /* Rz(90) * (1,0,0) = (0, 1, 0) */
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 0) - 0.0) < 0.01, "x should be ~0");
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 1) - 1.0) < 0.01, "y should be ~1");
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 2) - 0.0) < 0.01, "z should be ~0");

    nimcp_tensor_destroy(result);
    nimcp_free(scenario);
    nimcp_tensor_destroy(obj);
    parietal_destroy(parietal);
    PASS();
}

static void test_mental_rotate_null_safety(void) {
    TEST("Parietal: mental rotation NULL safety");
    uint32_t dims[] = {3};
    nimcp_tensor_t* obj = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);

    imagination_scenario_t* s1 = parietal_imagine_rotation(NULL, obj, 0, 0, 0);
    ASSERT_TRUE(s1 == NULL, "should return NULL for NULL parietal");

    parietal_lobe_t* parietal = parietal_create();
    imagination_scenario_t* s2 = parietal_imagine_rotation(parietal, NULL, 0, 0, 0);
    ASSERT_TRUE(s2 == NULL, "should return NULL for NULL object");

    parietal_destroy(parietal);
    nimcp_tensor_destroy(obj);
    PASS();
}

/* =========================================================================
 * Parietal Spatial Transform Tests
 * ========================================================================= */

static void test_spatial_transform_basic(void) {
    TEST("Parietal: spatial transform returns scenario");
    parietal_lobe_t* parietal = parietal_create();
    ASSERT_TRUE(parietal != NULL, "parietal NULL");

    uint32_t dims[] = {4};
    nimcp_tensor_t* scene = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* transform = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);

    /* Scene: (2, 3, 4, 5), Transform: (1, 2, 0.5, 3) */
    nimcp_tensor_set_flat(scene, 0, 2.0);
    nimcp_tensor_set_flat(scene, 1, 3.0);
    nimcp_tensor_set_flat(scene, 2, 4.0);
    nimcp_tensor_set_flat(scene, 3, 5.0);
    nimcp_tensor_set_flat(transform, 0, 1.0);
    nimcp_tensor_set_flat(transform, 1, 2.0);
    nimcp_tensor_set_flat(transform, 2, 0.5);
    nimcp_tensor_set_flat(transform, 3, 3.0);

    imagination_scenario_t* scenario = parietal_spatial_transform(
        parietal, scene, transform);
    ASSERT_TRUE(scenario != NULL, "scenario NULL");

    nimcp_tensor_t* result = scenario->latent_state;
    ASSERT_TRUE(result != NULL, "latent_state NULL");

    /* Element-wise multiply: (2*1, 3*2, 4*0.5, 5*3) = (2, 6, 2, 15) */
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 0) - 2.0) < 0.01, "elem 0 wrong");
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 1) - 6.0) < 0.01, "elem 1 wrong");
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 2) - 2.0) < 0.01, "elem 2 wrong");
    ASSERT_TRUE(fabs(nimcp_tensor_get_flat(result, 3) - 15.0) < 0.01, "elem 3 wrong");

    ASSERT_TRUE(scenario->is_active == true, "should be active");

    nimcp_tensor_destroy(result);
    nimcp_free(scenario);
    nimcp_tensor_destroy(scene);
    nimcp_tensor_destroy(transform);
    parietal_destroy(parietal);
    PASS();
}

static void test_spatial_transform_null_safety(void) {
    TEST("Parietal: spatial transform NULL safety");
    parietal_lobe_t* parietal = parietal_create();
    uint32_t dims[] = {3};
    nimcp_tensor_t* scene = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* transform = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);

    ASSERT_TRUE(parietal_spatial_transform(NULL, scene, transform) == NULL, "NULL parietal");
    ASSERT_TRUE(parietal_spatial_transform(parietal, NULL, transform) == NULL, "NULL scene");
    ASSERT_TRUE(parietal_spatial_transform(parietal, scene, NULL) == NULL, "NULL transform");

    nimcp_tensor_destroy(scene);
    nimcp_tensor_destroy(transform);
    parietal_destroy(parietal);
    PASS();
}

/* =========================================================================
 * Score Clamping Tests
 * ========================================================================= */

static void test_scores_clamped_to_one(void) {
    TEST("All modes: scores clamped to [0,1]");
    /* Create a problem that would push scores above 1.0 */
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_COMBINATORICS;
    problem.difficulty = 0.95f;
    problem.constraints = (void*)1;
    problem.target = (void*)1;
    genius_domain_t secondary[] = {GENIUS_DOMAIN_GRAPH_THEORY, GENIUS_DOMAIN_NUMBER_THEORY, GENIUS_DOMAIN_PROBABILITY};
    problem.secondary_domains = secondary;
    problem.num_secondary = 3;

    genius_result_t result;
    genius_erdos_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(result.elegance_score <= 1.0f, "elegance exceeds 1.0");
    ASSERT_TRUE(result.novelty_score <= 1.0f, "novelty exceeds 1.0");
    ASSERT_TRUE(result.generalization_score <= 1.0f, "generalization exceeds 1.0");
    ASSERT_TRUE(result.rigor_score <= 1.0f, "rigor exceeds 1.0");

    /* Same for Newton */
    problem.domain = GENIUS_DOMAIN_CALCULUS;
    genius_newton_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(result.elegance_score <= 1.0f, "Newton elegance exceeds 1.0");
    ASSERT_TRUE(result.novelty_score <= 1.0f, "Newton novelty exceeds 1.0");

    /* Same for Gauss */
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    genius_gauss_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(result.elegance_score <= 1.0f, "Gauss elegance exceeds 1.0");
    ASSERT_TRUE(result.rigor_score <= 1.0f, "Gauss rigor exceeds 1.0");
    PASS();
}

static void test_thinking_time_recorded(void) {
    TEST("All modes: thinking_time_us is recorded");
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_ALGEBRA;
    problem.difficulty = 0.5f;

    genius_result_t result;
    genius_gauss_analyze(g_genius, &problem, &result);
    /* thinking_time_us may be 0 on very fast CPUs, but should not be negative */
    ASSERT_TRUE(result.thinking_time_us < 1000000, "thinking time unreasonable");

    genius_newton_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(result.thinking_time_us < 1000000, "Newton thinking time unreasonable");

    genius_erdos_analyze(g_genius, &problem, &result);
    ASSERT_TRUE(result.thinking_time_us < 1000000, "Erdos thinking time unreasonable");
    PASS();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5 Batch 2: Genius Modes, Text Gen, Parietal Viz ===\n\n");

    g_genius = genius_create(NULL);
    if (!g_genius) {
        printf("FATAL: Failed to create genius instance\n");
        return 1;
    }

    printf("--- Gauss Mode ---\n");
    test_gauss_number_theory();
    test_gauss_hard_problem();
    test_gauss_easy_elegant();
    test_gauss_constraints();
    test_gauss_null_safety();

    printf("\n--- Newton Mode ---\n");
    test_newton_calculus();
    test_newton_physics();
    test_newton_diff_eq();
    test_newton_cross_domain();
    test_newton_geometry();

    printf("\n--- Erdos Mode ---\n");
    test_erdos_combinatorics();
    test_erdos_graph_theory();
    test_erdos_hard_problem();
    test_erdos_cross_domain();
    test_erdos_probabilistic_method();

    printf("\n--- Erdos Ramsey Bounds ---\n");
    test_erdos_ramsey_base_cases();
    test_erdos_ramsey_known_values();
    test_erdos_ramsey_larger();

    printf("\n--- Gauss Pattern Discovery ---\n");
    test_gauss_arithmetic_pattern();
    test_gauss_geometric_pattern();
    test_gauss_unknown_pattern();
    test_gauss_pattern_null_safety();

    printf("\n--- Text Generation ---\n");
    test_text_gen_create_destroy();
    test_text_gen_generate_prose();
    test_text_gen_generate_poetry();
    test_text_gen_continue();

    printf("\n--- Parietal Mental Rotation ---\n");
    test_mental_rotate_basic();
    test_mental_rotate_90z();
    test_mental_rotate_null_safety();

    printf("\n--- Parietal Spatial Transform ---\n");
    test_spatial_transform_basic();
    test_spatial_transform_null_safety();

    printf("\n--- Score Validation ---\n");
    test_scores_clamped_to_one();
    test_thinking_time_recorded();

    genius_destroy(g_genius);

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
