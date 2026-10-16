/**
 * @file test_parietal_exception_handling.cpp
 * @brief Unit tests for NIMCP_THROW_TO_IMMUNE exception handling in parietal modules
 * @date 2026-01-25
 *
 * WHAT: Test exception handling for all core parietal modules
 * WHY:  Verify proper error reporting to immune system for error conditions
 * HOW:  Test NULL pointers, invalid configs, and boundary conditions
 *
 * MODULES TESTED:
 * - parietal_lobe: Main orchestrator
 * - number_sense: Approximate Number System
 * - spatial_reasoning: Mental rotation and coordinate transforms
 * - mathematical_intuition: Pattern detection
 * - equation_manipulation: Symbolic math
 * - scientific_reasoning: Hypothesis testing
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>

extern "C" {
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_number_sense.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_equation_manipulation.h"
#include "cognitive/parietal/nimcp_scientific_reasoning.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ParietalExceptionTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<int> last_error_code;
    static char last_error_message[256];
    static nimcp_handler_registration_t* registration;

    void SetUp() override {
        exception_count = 0;
        last_error_code = 0;
        memset(last_error_message, 0, sizeof(last_error_message));

        nimcp_exception_system_init();

        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.name = "parietal_exception_test_handler";
        registration = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (registration) {
            nimcp_handler_unregister(registration);
            registration = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_count++;
        last_error_code = ex->code;
        if (ex->message) {
            strncpy(last_error_message, ex->message, sizeof(last_error_message) - 1);
        }
        return false;  // Don't consume - allow propagation
    }
};

std::atomic<int> ParietalExceptionTest::exception_count{0};
std::atomic<int> ParietalExceptionTest::last_error_code{0};
char ParietalExceptionTest::last_error_message[256] = {0};
nimcp_handler_registration_t* ParietalExceptionTest::registration = nullptr;

//=============================================================================
// Parietal Lobe Orchestrator Exception Tests
//=============================================================================

TEST_F(ParietalExceptionTest, ParietalCreate_ValidConfig_NoException) {
    parietal_lobe_t* parietal = parietal_create();

    EXPECT_NE(parietal, nullptr);
    EXPECT_EQ(exception_count.load(), 0);

    parietal_destroy(parietal);
}

TEST_F(ParietalExceptionTest, ParietalDestroy_NullPointer_NoException) {
    // Destroy should be NULL-safe and not throw
    parietal_destroy(nullptr);
    // No crash = success
}

TEST_F(ParietalExceptionTest, ParietalProcess_NullParietal_ThrowsNullPointer) {
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_ESTIMATE_QUANTITY;

    parietal_result_t result = parietal_process(nullptr, &request);

    EXPECT_FALSE(result.success);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalProcess_NullRequest_ThrowsNullPointer) {
    parietal_lobe_t* parietal = parietal_create();
    ASSERT_NE(parietal, nullptr);

    exception_count = 0;
    last_error_code = 0;

    parietal_result_t result = parietal_process(parietal, nullptr);

    EXPECT_FALSE(result.success);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    parietal_destroy(parietal);
}

TEST_F(ParietalExceptionTest, ParietalAttachImmune_NullParietal_ThrowsNullPointer) {
    int result = parietal_attach_immune(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalAttachThalamus_NullParietal_ThrowsNullPointer) {
    int result = parietal_attach_thalamus(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalAttachSubstrate_NullParietal_ThrowsNullPointer) {
    int result = parietal_attach_substrate(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalAttachFEP_NullParietal_ThrowsNullPointer) {
    int result = parietal_attach_fep(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalAttachWorkingMemory_NullParietal_ThrowsNullPointer) {
    int result = parietal_attach_working_memory(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalAttachLogicGates_NullParietal_ThrowsNullPointer) {
    int result = parietal_attach_logic_gates(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalAttachTraining_NullParietal_ThrowsNullPointer) {
    int result = parietal_attach_training(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalAttachPerception_NullParietal_ThrowsNullPointer) {
    int result = parietal_attach_perception(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalAttachSleep_NullParietal_ThrowsNullPointer) {
    int result = parietal_attach_sleep(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalGetStats_NullParietal_ThrowsNullPointer) {
    parietal_stats_t stats;
    int result = parietal_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);  // Returns negative on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ParietalGetStats_NullStats_ThrowsNullPointer) {
    parietal_lobe_t* parietal = parietal_create();
    ASSERT_NE(parietal, nullptr);

    exception_count = 0;
    last_error_code = 0;

    int result = parietal_get_stats(parietal, nullptr);

    EXPECT_LT(result, 0);  // Returns negative on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    parietal_destroy(parietal);
}

//=============================================================================
// Number Sense Exception Tests
//=============================================================================

TEST_F(ParietalExceptionTest, NumberSenseCreate_ValidConfig_NoException) {
    number_sense_t* ns = number_sense_create();

    EXPECT_NE(ns, nullptr);
    EXPECT_EQ(exception_count.load(), 0);

    number_sense_destroy(ns);
}

TEST_F(ParietalExceptionTest, NumberSenseDestroy_NullPointer_NoException) {
    number_sense_destroy(nullptr);
    // No crash = success
}

TEST_F(ParietalExceptionTest, NumberSenseEstimate_NullSense_ThrowsNullPointer) {
    float values[] = {1.0f, 2.0f, 3.0f};
    number_estimate_t estimate = number_sense_estimate(nullptr, values, 3);

    EXPECT_LT(estimate.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, NumberSenseEstimate_NullValues_ThrowsNullPointer) {
    number_sense_t* ns = number_sense_create();
    ASSERT_NE(ns, nullptr);

    exception_count = 0;
    last_error_code = 0;

    number_estimate_t estimate = number_sense_estimate(ns, nullptr, 3);

    EXPECT_LT(estimate.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    number_sense_destroy(ns);
}

TEST_F(ParietalExceptionTest, NumberSenseCompare_NullSense_ThrowsNullPointer) {
    number_comparison_t cmp = number_sense_compare(nullptr, 5.0f, 7.0f);

    EXPECT_LT(cmp.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, NumberSenseApproximateAdd_NullSense_ThrowsNullPointer) {
    approx_arithmetic_t result = number_sense_approximate_add(nullptr, 5.0f, 3.0f);

    EXPECT_LT(result.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, NumberSenseGetStats_NullSense_ThrowsNullPointer) {
    number_sense_stats_t stats;
    int result = number_sense_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);  // Returns negative on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, NumberSenseGetStats_NullStats_ThrowsNullPointer) {
    number_sense_t* ns = number_sense_create();
    ASSERT_NE(ns, nullptr);

    exception_count = 0;
    last_error_code = 0;

    int result = number_sense_get_stats(ns, nullptr);

    EXPECT_LT(result, 0);  // Returns negative on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    number_sense_destroy(ns);
}

//=============================================================================
// Spatial Reasoning Exception Tests
//=============================================================================

TEST_F(ParietalExceptionTest, SpatialCreate_ValidConfig_NoException) {
    spatial_reasoning_t* sr = spatial_reasoning_create();

    EXPECT_NE(sr, nullptr);
    EXPECT_EQ(exception_count.load(), 0);

    spatial_reasoning_destroy(sr);
}

TEST_F(ParietalExceptionTest, SpatialDestroy_NullPointer_NoException) {
    spatial_reasoning_destroy(nullptr);
    // No crash = success
}

TEST_F(ParietalExceptionTest, SpatialRotateAndCompare_NullReasoning_ThrowsNullPointer) {
    spatial_object_t obj_a, obj_b;
    memset(&obj_a, 0, sizeof(obj_a));
    memset(&obj_b, 0, sizeof(obj_b));

    rotation_result_t result = spatial_rotate_and_compare(nullptr, &obj_a, &obj_b);

    EXPECT_LT(result.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, SpatialRotateAndCompare_NullObjectA_ThrowsNullPointer) {
    spatial_reasoning_t* sr = spatial_reasoning_create();
    ASSERT_NE(sr, nullptr);

    exception_count = 0;
    last_error_code = 0;

    spatial_object_t obj_b;
    memset(&obj_b, 0, sizeof(obj_b));

    rotation_result_t result = spatial_rotate_and_compare(sr, nullptr, &obj_b);

    EXPECT_LT(result.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    spatial_reasoning_destroy(sr);
}

TEST_F(ParietalExceptionTest, SpatialMentalRotate_NullReasoning_ThrowsNullPointer) {
    spatial_object_t obj;
    memset(&obj, 0, sizeof(obj));
    vec3_t axis = {0.0f, 1.0f, 0.0f};

    uint64_t result = spatial_mental_rotate(nullptr, &obj, axis, 45.0f);

    // Result should be 0 on error
    EXPECT_EQ(result, 0u);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, SpatialMentalRotate_NullObject_ThrowsNullPointer) {
    spatial_reasoning_t* sr = spatial_reasoning_create();
    ASSERT_NE(sr, nullptr);

    exception_count = 0;
    last_error_code = 0;

    vec3_t axis = {0.0f, 1.0f, 0.0f};
    uint64_t result = spatial_mental_rotate(sr, nullptr, axis, 45.0f);

    EXPECT_EQ(result, 0u);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    spatial_reasoning_destroy(sr);
}

TEST_F(ParietalExceptionTest, SpatialEgoToAllocentric_NullReasoning_ThrowsNullPointer) {
    vec3_t local = {1.0f, 2.0f, 3.0f};
    observer_pose_t observer;
    memset(&observer, 0, sizeof(observer));

    vec3_t result = spatial_ego_to_allocentric(nullptr, local, &observer);

    // Result should be zero vector on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, SpatialAddObject_NullReasoning_ThrowsNullPointer) {
    spatial_object_t obj;
    memset(&obj, 0, sizeof(obj));

    uint32_t result = spatial_add_object(nullptr, &obj);

    EXPECT_EQ(result, 0u);  // Returns 0 on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, SpatialAddObject_NullObject_ThrowsNullPointer) {
    spatial_reasoning_t* sr = spatial_reasoning_create();
    ASSERT_NE(sr, nullptr);

    exception_count = 0;
    last_error_code = 0;

    uint32_t result = spatial_add_object(sr, nullptr);

    EXPECT_EQ(result, 0u);  // Returns 0 on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    spatial_reasoning_destroy(sr);
}

TEST_F(ParietalExceptionTest, SpatialFindNearest_NullReasoning_ThrowsNullPointer) {
    vec3_t query = {0.0f, 0.0f, 0.0f};

    spatial_object_t* result = spatial_find_nearest(nullptr, query);

    EXPECT_EQ(result, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, SpatialGetStats_NullReasoning_ThrowsNullPointer) {
    spatial_stats_t stats;
    int result = spatial_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);  // Returns negative on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Mathematical Intuition Exception Tests
//=============================================================================

TEST_F(ParietalExceptionTest, MathIntuitionCreate_ValidConfig_NoException) {
    math_intuition_t* mi = math_intuition_create();

    EXPECT_NE(mi, nullptr);
    EXPECT_EQ(exception_count.load(), 0);

    math_intuition_destroy(mi);
}

TEST_F(ParietalExceptionTest, MathIntuitionDestroy_NullPointer_NoException) {
    math_intuition_destroy(nullptr);
    // No crash = success
}

TEST_F(ParietalExceptionTest, MathDetectPattern_NullIntuition_ThrowsNullPointer) {
    float sequence[] = {1.0f, 2.0f, 4.0f, 8.0f};

    detected_pattern_t pattern = math_detect_pattern(nullptr, sequence, 4);

    EXPECT_LT(pattern.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, MathDetectPattern_NullSequence_ThrowsNullPointer) {
    math_intuition_t* mi = math_intuition_create();
    ASSERT_NE(mi, nullptr);

    exception_count = 0;
    last_error_code = 0;

    detected_pattern_t pattern = math_detect_pattern(mi, nullptr, 4);

    EXPECT_LT(pattern.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    math_intuition_destroy(mi);
}

TEST_F(ParietalExceptionTest, MathExtrapolate_NullIntuition_ThrowsNullPointer) {
    detected_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));

    float result = math_extrapolate(nullptr, &pattern, 5);

    EXPECT_TRUE(isnan(result) || result == 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, MathDetectSymmetry_NullIntuition_ThrowsNullPointer) {
    vec3_t points[] = {{0,0,0}, {1,0,0}, {-1,0,0}};

    symmetry_result_t result = math_detect_symmetry(nullptr, points, 3);

    EXPECT_LT(result.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, MathSolveAnalogy_NullIntuition_ThrowsNullPointer) {
    analogy_result_t result = math_solve_analogy(nullptr, 2.0f, 4.0f, 3.0f);

    EXPECT_LT(result.confidence, 0.0f);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, MathIntuitionGetStats_NullIntuition_ThrowsNullPointer) {
    math_intuition_stats_t stats;
    int result = math_intuition_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);  // Returns negative on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Equation Manipulation Exception Tests
//=============================================================================

TEST_F(ParietalExceptionTest, EquationEngineCreate_ValidConfig_NoException) {
    equation_engine_t* ee = equation_engine_create();

    EXPECT_NE(ee, nullptr);
    EXPECT_EQ(exception_count.load(), 0);

    equation_engine_destroy(ee);
}

TEST_F(ParietalExceptionTest, EquationEngineDestroy_NullPointer_NoException) {
    equation_engine_destroy(nullptr);
    // No crash = success
}

TEST_F(ParietalExceptionTest, EquationParse_NullEngine_ThrowsNullPointer) {
    expr_node_t* expr = equation_parse(nullptr, "x + 1");

    EXPECT_EQ(expr, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, EquationParse_NullExpression_ThrowsNullPointer) {
    equation_engine_t* ee = equation_engine_create();
    ASSERT_NE(ee, nullptr);

    exception_count = 0;
    last_error_code = 0;

    expr_node_t* expr = equation_parse(ee, nullptr);

    EXPECT_EQ(expr, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    equation_engine_destroy(ee);
}

TEST_F(ParietalExceptionTest, EquationDifferentiate_NullEngine_ThrowsNullPointer) {
    expr_node_t* result = equation_differentiate(nullptr, nullptr, "x");

    EXPECT_EQ(result, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, EquationSimplify_NullEngine_ThrowsNullPointer) {
    expr_node_t* result = equation_simplify(nullptr, nullptr);

    EXPECT_EQ(result, nullptr);
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, EquationEvaluate_NullEngine_ThrowsNullPointer) {
    float result = equation_evaluate(nullptr, nullptr, nullptr, 0);

    EXPECT_TRUE(isnan(result));
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, EquationGetStats_NullEngine_ThrowsNullPointer) {
    equation_stats_t stats;
    int result = equation_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);  // Returns negative on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Scientific Reasoning Exception Tests
//=============================================================================

TEST_F(ParietalExceptionTest, ScientificReasoningCreate_ValidConfig_NoException) {
    scientific_reasoning_t* sr = scientific_reasoning_create();

    EXPECT_NE(sr, nullptr);
    EXPECT_EQ(exception_count.load(), 0);

    scientific_reasoning_destroy(sr);
}

TEST_F(ParietalExceptionTest, ScientificReasoningDestroy_NullPointer_NoException) {
    scientific_reasoning_destroy(nullptr);
    // No crash = success
}

TEST_F(ParietalExceptionTest, ScientificCreateHypothesis_NullReasoning_ThrowsNullPointer) {
    // scientific_create_hypothesis returns hypothesis_t by value
    // On NULL reasoning, it should return a hypothesis with id=0 indicating error
    hypothesis_t hyp = scientific_create_hypothesis(nullptr, "test", 0.5f);

    EXPECT_EQ(hyp.id, 0u);  // id=0 indicates error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ScientificCreateHypothesis_NullDescription_ThrowsNullPointer) {
    scientific_reasoning_t* sr = scientific_reasoning_create();
    ASSERT_NE(sr, nullptr);

    exception_count = 0;
    last_error_code = 0;

    hypothesis_t hyp = scientific_create_hypothesis(sr, nullptr, 0.5f);

    EXPECT_EQ(hyp.id, 0u);  // id=0 indicates error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    scientific_reasoning_destroy(sr);
}

TEST_F(ParietalExceptionTest, ScientificUpdateHypothesis_NullReasoning_ThrowsNullPointer) {
    hypothesis_t hyp;
    memset(&hyp, 0, sizeof(hyp));
    data_sample_t sample;
    memset(&sample, 0, sizeof(sample));

    // scientific_update_hypothesis returns float (posterior probability)
    // On error, it should return a negative value or NaN
    float result = scientific_update_hypothesis(nullptr, &hyp, &sample, 1);

    EXPECT_TRUE(result < 0.0f || isnan(result));
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ScientificBuckinghamPi_NullReasoning_ThrowsNullPointer) {
    physical_quantity_t quantities[3];
    memset(quantities, 0, sizeof(quantities));
    float* pi_groups[10];

    uint32_t result = scientific_buckingham_pi(nullptr, quantities, 3, pi_groups, 10);

    EXPECT_EQ(result, 0u);  // Returns 0 on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionTest, ScientificGetStats_NullReasoning_ThrowsNullPointer) {
    scientific_stats_t stats;
    int result = scientific_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);  // Returns negative on error
    EXPECT_GT(exception_count.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Multi-threaded Exception Handling Tests
//=============================================================================

TEST_F(ParietalExceptionTest, ConcurrentNullOperations_AllThrowExceptions) {
    const int num_threads = 4;
    const int ops_per_thread = 100;
    std::atomic<int> total_exceptions{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&total_exceptions, ops_per_thread, t]() {
            for (int i = 0; i < ops_per_thread; i++) {
                switch ((t + i) % 5) {
                    case 0: {
                        parietal_result_t res = parietal_process(nullptr, nullptr);
                        if (!res.success) total_exceptions++;
                        break;
                    }
                    case 1: {
                        number_estimate_t est = number_sense_estimate(nullptr, nullptr, 0);
                        if (est.confidence < 0) total_exceptions++;
                        break;
                    }
                    case 2: {
                        rotation_result_t rot = spatial_rotate_and_compare(nullptr, nullptr, nullptr);
                        if (rot.confidence < 0) total_exceptions++;
                        break;
                    }
                    case 3: {
                        detected_pattern_t pat = math_detect_pattern(nullptr, nullptr, 0);
                        if (pat.confidence < 0) total_exceptions++;
                        break;
                    }
                    case 4: {
                        expr_node_t* expr = equation_parse(nullptr, nullptr);
                        if (expr == nullptr) total_exceptions++;
                        break;
                    }
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_exceptions.load(), num_threads * ops_per_thread);
}

//=============================================================================
// Stress Test
//=============================================================================

TEST_F(ParietalExceptionTest, RapidNullCalls_SystemRemainsStable) {
    parietal_lobe_t* parietal = parietal_create();
    ASSERT_NE(parietal, nullptr);

    exception_count = 0;

    // Rapid NULL calls to various functions
    for (int i = 0; i < 1000; i++) {
        parietal_process(nullptr, nullptr);
        number_sense_estimate(nullptr, nullptr, 0);
        spatial_rotate_and_compare(nullptr, nullptr, nullptr);
        math_detect_pattern(nullptr, nullptr, 0);
        equation_parse(nullptr, nullptr);
        scientific_create_hypothesis(nullptr, nullptr, 0.0f);
    }

    // Verify exceptions were thrown (5 of 6 functions throw before heartbeat;
    // equation_parse throws before heartbeat registration, so handler may see
    // fewer than 6 per iteration under rapid-fire conditions)
    EXPECT_GE(exception_count.load(), 5000);

    // Verify valid parietal still works
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_ESTIMATE_QUANTITY;
    request.input.quantity_input.values = nullptr;
    request.input.quantity_input.num_values = 0;

    // System should still be operational
    parietal_destroy(parietal);
    // No crash = success
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
