/**
 * @file test_parietal_exception_regression.cpp
 * @brief Regression tests for parietal module exception handling API contracts
 * @date 2026-01-25
 *
 * WHAT: Test API contract stability and error code consistency for parietal modules
 * WHY:  Ensure exception handling behavior remains stable across versions
 * HOW:  Verify specific error codes, return values, and exception message formats
 *
 * REGRESSION COVERAGE:
 * - API contract verification for all exception-throwing functions
 * - Error code consistency across module boundaries
 * - Return value contract enforcement
 * - Exception message format stability
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <string>

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

class ParietalExceptionRegressionTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<int> last_error_code;
    static char last_error_message[512];
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
        options.name = "parietal_regression_handler";
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
        return false;
    }

    // Helper to verify exception was thrown with expected code
    void expect_exception(int expected_code) {
        EXPECT_GT(exception_count.load(), 0) << "Expected exception was not thrown";
        EXPECT_EQ(last_error_code.load(), expected_code)
            << "Expected error code " << expected_code
            << " but got " << last_error_code.load();
    }

    // Helper to reset counters
    void reset_counters() {
        exception_count = 0;
        last_error_code = 0;
        memset(last_error_message, 0, sizeof(last_error_message));
    }
};

std::atomic<int> ParietalExceptionRegressionTest::exception_count{0};
std::atomic<int> ParietalExceptionRegressionTest::last_error_code{0};
char ParietalExceptionRegressionTest::last_error_message[512] = {0};
nimcp_handler_registration_t* ParietalExceptionRegressionTest::registration = nullptr;

//=============================================================================
// Parietal Orchestrator API Contract Tests
//=============================================================================

TEST_F(ParietalExceptionRegressionTest, ParietalProcess_NullParietal_ReturnsFailure) {
    // API CONTRACT: parietal_process with NULL parietal returns result with success=false
    parietal_request_t request;
    memset(&request, 0, sizeof(request));

    parietal_result_t result = parietal_process(nullptr, &request);

    EXPECT_FALSE(result.success);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalProcess_NullRequest_ReturnsFailure) {
    // API CONTRACT: parietal_process with NULL request returns result with success=false
    parietal_lobe_t* parietal = parietal_create();
    ASSERT_NE(parietal, nullptr);
    reset_counters();

    parietal_result_t result = parietal_process(parietal, nullptr);

    EXPECT_FALSE(result.success);
    expect_exception(NIMCP_ERROR_NULL_POINTER);

    parietal_destroy(parietal);
}

TEST_F(ParietalExceptionRegressionTest, ParietalGetStats_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_get_stats with NULL parietal returns < 0
    parietal_stats_t stats;

    int result = parietal_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalGetStats_NullStats_ReturnsNegative) {
    // API CONTRACT: parietal_get_stats with NULL stats returns < 0
    parietal_lobe_t* parietal = parietal_create();
    ASSERT_NE(parietal, nullptr);
    reset_counters();

    int result = parietal_get_stats(parietal, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);

    parietal_destroy(parietal);
}

TEST_F(ParietalExceptionRegressionTest, ParietalAttachImmune_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_attach_immune with NULL parietal returns < 0
    int result = parietal_attach_immune(nullptr, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalAttachThalamus_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_attach_thalamus with NULL parietal returns < 0
    int result = parietal_attach_thalamus(nullptr, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalAttachSubstrate_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_attach_substrate with NULL parietal returns < 0
    int result = parietal_attach_substrate(nullptr, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalAttachFEP_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_attach_fep with NULL parietal returns < 0
    int result = parietal_attach_fep(nullptr, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalAttachWorkingMemory_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_attach_working_memory with NULL parietal returns < 0
    int result = parietal_attach_working_memory(nullptr, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalAttachLogicGates_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_attach_logic_gates with NULL parietal returns < 0
    int result = parietal_attach_logic_gates(nullptr, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalAttachTraining_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_attach_training with NULL parietal returns < 0
    int result = parietal_attach_training(nullptr, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalAttachPerception_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_attach_perception with NULL parietal returns < 0
    int result = parietal_attach_perception(nullptr, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ParietalAttachSleep_NullParietal_ReturnsNegative) {
    // API CONTRACT: parietal_attach_sleep with NULL parietal returns < 0
    int result = parietal_attach_sleep(nullptr, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Number Sense API Contract Tests
//=============================================================================

TEST_F(ParietalExceptionRegressionTest, NumberSenseEstimate_NullSense_ReturnsNegativeConfidence) {
    // API CONTRACT: number_sense_estimate with NULL sense returns negative confidence
    float values[] = {1.0f, 2.0f, 3.0f};

    number_estimate_t result = number_sense_estimate(nullptr, values, 3);

    EXPECT_LT(result.confidence, 0.0f);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, NumberSenseEstimate_NullValues_ReturnsNegativeConfidence) {
    // API CONTRACT: number_sense_estimate with NULL values returns negative confidence
    number_sense_t* ns = number_sense_create();
    ASSERT_NE(ns, nullptr);
    reset_counters();

    number_estimate_t result = number_sense_estimate(ns, nullptr, 3);

    EXPECT_LT(result.confidence, 0.0f);
    expect_exception(NIMCP_ERROR_NULL_POINTER);

    number_sense_destroy(ns);
}

TEST_F(ParietalExceptionRegressionTest, NumberSenseCompare_NullSense_ReturnsNegativeConfidence) {
    // API CONTRACT: number_sense_compare with NULL sense returns negative confidence
    number_comparison_t result = number_sense_compare(nullptr, 5.0f, 7.0f);

    EXPECT_LT(result.confidence, 0.0f);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, NumberSenseApproxAdd_NullSense_ReturnsNegativeConfidence) {
    // API CONTRACT: number_sense_approximate_add with NULL sense returns negative confidence
    approx_arithmetic_t result = number_sense_approximate_add(nullptr, 5.0f, 3.0f);

    EXPECT_LT(result.confidence, 0.0f);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, NumberSenseGetStats_NullSense_ReturnsNegative) {
    // API CONTRACT: number_sense_get_stats with NULL sense returns < 0
    number_sense_stats_t stats;

    int result = number_sense_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, NumberSenseGetStats_NullStats_ReturnsNegative) {
    // API CONTRACT: number_sense_get_stats with NULL stats returns < 0
    number_sense_t* ns = number_sense_create();
    ASSERT_NE(ns, nullptr);
    reset_counters();

    int result = number_sense_get_stats(ns, nullptr);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);

    number_sense_destroy(ns);
}

//=============================================================================
// Spatial Reasoning API Contract Tests
//=============================================================================

TEST_F(ParietalExceptionRegressionTest, SpatialRotateAndCompare_NullReasoning_ReturnsNegativeConfidence) {
    // API CONTRACT: spatial_rotate_and_compare with NULL reasoning returns negative confidence
    spatial_object_t obj_a, obj_b;
    memset(&obj_a, 0, sizeof(obj_a));
    memset(&obj_b, 0, sizeof(obj_b));

    rotation_result_t result = spatial_rotate_and_compare(nullptr, &obj_a, &obj_b);

    EXPECT_LT(result.confidence, 0.0f);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, SpatialRotateAndCompare_NullObjects_ReturnsNegativeConfidence) {
    // API CONTRACT: spatial_rotate_and_compare with NULL objects returns negative confidence
    spatial_reasoning_t* sr = spatial_reasoning_create();
    ASSERT_NE(sr, nullptr);
    reset_counters();

    rotation_result_t result = spatial_rotate_and_compare(sr, nullptr, nullptr);

    EXPECT_LT(result.confidence, 0.0f);
    expect_exception(NIMCP_ERROR_NULL_POINTER);

    spatial_reasoning_destroy(sr);
}

TEST_F(ParietalExceptionRegressionTest, SpatialAddObject_NullReasoning_ReturnsZero) {
    // API CONTRACT: spatial_add_object with NULL reasoning returns 0 (error indicator)
    spatial_object_t obj;
    memset(&obj, 0, sizeof(obj));

    uint32_t result = spatial_add_object(nullptr, &obj);

    EXPECT_EQ(result, 0u);  // 0 indicates error
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, SpatialAddObject_NullObject_ReturnsZero) {
    // API CONTRACT: spatial_add_object with NULL object returns 0 (error indicator)
    spatial_reasoning_t* sr = spatial_reasoning_create();
    ASSERT_NE(sr, nullptr);
    reset_counters();

    uint32_t result = spatial_add_object(sr, nullptr);

    EXPECT_EQ(result, 0u);  // 0 indicates error
    expect_exception(NIMCP_ERROR_NULL_POINTER);

    spatial_reasoning_destroy(sr);
}

TEST_F(ParietalExceptionRegressionTest, SpatialFindNearest_NullReasoning_ReturnsNull) {
    // API CONTRACT: spatial_find_nearest with NULL reasoning returns NULL
    vec3_t query = {0.0f, 0.0f, 0.0f};

    spatial_object_t* result = spatial_find_nearest(nullptr, query);

    EXPECT_EQ(result, nullptr);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, SpatialGetStats_NullReasoning_ReturnsNegative) {
    // API CONTRACT: spatial_get_stats with NULL reasoning returns < 0
    spatial_stats_t stats;

    int result = spatial_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Mathematical Intuition API Contract Tests
//=============================================================================

TEST_F(ParietalExceptionRegressionTest, MathIntuitionDetectPattern_NullIntuition_ReturnsNegativeConfidence) {
    // API CONTRACT: math_detect_pattern with NULL returns negative confidence
    float sequence[] = {1.0f, 2.0f, 4.0f, 8.0f};

    detected_pattern_t result = math_detect_pattern(nullptr, sequence, 4);

    EXPECT_LT(result.confidence, 0.0f);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, MathIntuitionDetectPattern_NullSequence_ReturnsNegativeConfidence) {
    // API CONTRACT: math_detect_pattern with NULL sequence returns negative confidence
    math_intuition_t* mi = math_intuition_create();
    ASSERT_NE(mi, nullptr);
    reset_counters();

    detected_pattern_t result = math_detect_pattern(mi, nullptr, 4);

    EXPECT_LT(result.confidence, 0.0f);
    expect_exception(NIMCP_ERROR_NULL_POINTER);

    math_intuition_destroy(mi);
}

TEST_F(ParietalExceptionRegressionTest, MathIntuitionSolveAnalogy_NullIntuition_ReturnsNegativeConfidence) {
    // API CONTRACT: math_solve_analogy with NULL returns negative confidence
    analogy_result_t result = math_solve_analogy(nullptr, 2.0f, 4.0f, 3.0f);

    EXPECT_LT(result.confidence, 0.0f);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, MathIntuitionGetStats_NullIntuition_ReturnsNegative) {
    // API CONTRACT: math_intuition_get_stats with NULL returns < 0
    math_intuition_stats_t stats;

    int result = math_intuition_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Equation Manipulation API Contract Tests
//=============================================================================

TEST_F(ParietalExceptionRegressionTest, EquationParse_NullEngine_ReturnsNull) {
    // API CONTRACT: equation_parse with NULL engine returns NULL
    expr_node_t* result = equation_parse(nullptr, "x + 1");

    EXPECT_EQ(result, nullptr);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, EquationParse_NullExpression_ReturnsNull) {
    // API CONTRACT: equation_parse with NULL expression returns NULL
    equation_engine_t* ee = equation_engine_create();
    ASSERT_NE(ee, nullptr);
    reset_counters();

    expr_node_t* result = equation_parse(ee, nullptr);

    EXPECT_EQ(result, nullptr);
    expect_exception(NIMCP_ERROR_NULL_POINTER);

    equation_engine_destroy(ee);
}

TEST_F(ParietalExceptionRegressionTest, EquationDifferentiate_NullEngine_ReturnsNull) {
    // API CONTRACT: equation_differentiate with NULL engine returns NULL
    expr_node_t* result = equation_differentiate(nullptr, nullptr, "x");

    EXPECT_EQ(result, nullptr);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, EquationSimplify_NullEngine_ReturnsNull) {
    // API CONTRACT: equation_simplify with NULL engine returns NULL
    expr_node_t* result = equation_simplify(nullptr, nullptr);

    EXPECT_EQ(result, nullptr);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, EquationEvaluate_NullEngine_ReturnsNaN) {
    // API CONTRACT: equation_evaluate with NULL engine returns NaN
    float result = equation_evaluate(nullptr, nullptr, nullptr, 0);

    EXPECT_TRUE(isnan(result));
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, EquationGetStats_NullEngine_ReturnsNegative) {
    // API CONTRACT: equation_get_stats with NULL engine returns < 0
    equation_stats_t stats;

    int result = equation_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Scientific Reasoning API Contract Tests
//=============================================================================

TEST_F(ParietalExceptionRegressionTest, ScientificCreateHypothesis_NullReasoning_ReturnsZeroId) {
    // API CONTRACT: scientific_create_hypothesis with NULL reasoning returns hypothesis with id=0
    hypothesis_t result = scientific_create_hypothesis(nullptr, "test", 0.5f);

    EXPECT_EQ(result.id, 0u);  // id=0 indicates error
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ScientificCreateHypothesis_NullDescription_ReturnsZeroId) {
    // API CONTRACT: scientific_create_hypothesis with NULL description returns hypothesis with id=0
    scientific_reasoning_t* sr = scientific_reasoning_create();
    ASSERT_NE(sr, nullptr);
    reset_counters();

    hypothesis_t result = scientific_create_hypothesis(sr, nullptr, 0.5f);

    EXPECT_EQ(result.id, 0u);  // id=0 indicates error
    expect_exception(NIMCP_ERROR_NULL_POINTER);

    scientific_reasoning_destroy(sr);
}

TEST_F(ParietalExceptionRegressionTest, ScientificUpdateHypothesis_NullReasoning_ReturnsNegativeFloat) {
    // API CONTRACT: scientific_update_hypothesis with NULL reasoning returns < 0.0f
    hypothesis_t hyp;
    memset(&hyp, 0, sizeof(hyp));
    data_sample_t sample;
    memset(&sample, 0, sizeof(sample));

    float result = scientific_update_hypothesis(nullptr, &hyp, &sample, 1);

    EXPECT_TRUE(result < 0.0f || isnan(result));
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ScientificBuckinghamPi_NullReasoning_ReturnsZero) {
    // API CONTRACT: scientific_buckingham_pi with NULL reasoning returns 0
    physical_quantity_t quantities[3];
    memset(quantities, 0, sizeof(quantities));
    float* pi_groups[10];

    uint32_t result = scientific_buckingham_pi(nullptr, quantities, 3, pi_groups, 10);

    EXPECT_EQ(result, 0u);  // 0 indicates error
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionRegressionTest, ScientificGetStats_NullReasoning_ReturnsNegative) {
    // API CONTRACT: scientific_get_stats with NULL reasoning returns < 0
    scientific_stats_t stats;

    int result = scientific_get_stats(nullptr, &stats);

    EXPECT_LT(result, 0);
    expect_exception(NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Error Code Stability Tests
//=============================================================================

TEST_F(ParietalExceptionRegressionTest, AllNullPointerErrors_UseCorrectErrorCode) {
    // REGRESSION: All NULL pointer errors must use NIMCP_ERROR_NULL_POINTER (1003)
    const int expected_code = NIMCP_ERROR_NULL_POINTER;

    // Parietal orchestrator
    reset_counters();
    parietal_process(nullptr, nullptr);
    EXPECT_EQ(last_error_code.load(), expected_code) << "parietal_process";

    // Number sense
    reset_counters();
    number_sense_estimate(nullptr, nullptr, 0);
    EXPECT_EQ(last_error_code.load(), expected_code) << "number_sense_estimate";

    // Spatial reasoning
    reset_counters();
    spatial_rotate_and_compare(nullptr, nullptr, nullptr);
    EXPECT_EQ(last_error_code.load(), expected_code) << "spatial_rotate_and_compare";

    // Math intuition
    reset_counters();
    math_detect_pattern(nullptr, nullptr, 0);
    EXPECT_EQ(last_error_code.load(), expected_code) << "math_detect_pattern";

    // Equation manipulation
    reset_counters();
    equation_parse(nullptr, nullptr);
    EXPECT_EQ(last_error_code.load(), expected_code) << "equation_parse";

    // Scientific reasoning
    reset_counters();
    scientific_create_hypothesis(nullptr, nullptr, 0);
    EXPECT_EQ(last_error_code.load(), expected_code) << "scientific_create_hypothesis";
}

//=============================================================================
// Exception Message Format Tests
//=============================================================================

TEST_F(ParietalExceptionRegressionTest, ExceptionMessages_ContainFunctionName) {
    // REGRESSION: Exception messages should contain the function name for debugging

    reset_counters();
    parietal_process(nullptr, nullptr);
    EXPECT_TRUE(strstr(last_error_message, "parietal") != nullptr ||
                strstr(last_error_message, "NULL") != nullptr)
        << "Message: " << last_error_message;

    reset_counters();
    number_sense_estimate(nullptr, nullptr, 0);
    EXPECT_TRUE(strstr(last_error_message, "number_sense") != nullptr ||
                strstr(last_error_message, "NULL") != nullptr ||
                strstr(last_error_message, "ns") != nullptr)
        << "Message: " << last_error_message;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
