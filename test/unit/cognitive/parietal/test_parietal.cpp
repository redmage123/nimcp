/**
 * @file test_parietal.cpp
 * @brief Unit tests for NIMCP Parietal Lobe Orchestrator
 *
 * Tests the main parietal lobe module including lifecycle,
 * submodule access, processing requests, physics NN, and integration.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_parietal.h"

namespace {

//=============================================================================
// Test Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

class ParietalTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        parietal = parietal_create();
        ASSERT_NE(parietal, nullptr);
    }

    void TearDown() override
    {
        if (parietal) {
            parietal_destroy(parietal);
            parietal = nullptr;
        }
    }

    parietal_lobe_t* parietal;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ParietalTest, CreateDefault)
{
    EXPECT_NE(parietal, nullptr);
}

TEST_F(ParietalTest, CreateCustom)
{
    parietal_config_t config = parietal_default_config();
    config.nn_hidden_size = 128;
    config.nn_learning_rate = 0.01f;
    config.enable_neural_network = true;

    parietal_lobe_t* custom = parietal_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    parietal_destroy(custom);
}

TEST_F(ParietalTest, CreateWithNullConfig)
{
    parietal_lobe_t* created = parietal_create_custom(nullptr);
    EXPECT_NE(created, nullptr);
    parietal_destroy(created);
}

TEST_F(ParietalTest, DestroyNullSafe)
{
    parietal_destroy(nullptr);
    // Should not crash
}

TEST_F(ParietalTest, DefaultConfig)
{
    parietal_config_t config = parietal_default_config();

    EXPECT_EQ(config.nn_hidden_size, 256);
    EXPECT_NEAR(config.nn_learning_rate, 0.001f, 1e-5f);
    EXPECT_TRUE(config.enable_neural_network);
    EXPECT_TRUE(config.enable_immune_bridge);
    EXPECT_TRUE(config.enable_thalamic_routing);
    EXPECT_TRUE(config.enable_working_memory);
    EXPECT_TRUE(config.nn_use_hamiltonian);
    EXPECT_TRUE(config.nn_use_lagrangian);
}

TEST_F(ParietalTest, ValidateConfig)
{
    parietal_config_t valid = parietal_default_config();
    EXPECT_TRUE(parietal_validate_config(&valid));

    parietal_config_t invalid = valid;
    invalid.nn_hidden_size = 0;
    EXPECT_FALSE(parietal_validate_config(&invalid));

    invalid = valid;
    invalid.request_timeout_ms = 0;
    EXPECT_FALSE(parietal_validate_config(&invalid));
}

//=============================================================================
// Submodule Access Tests
//=============================================================================

TEST_F(ParietalTest, GetNumberSense)
{
    number_sense_t* ns = parietal_get_number_sense(parietal);
    EXPECT_NE(ns, nullptr);
}

TEST_F(ParietalTest, GetSpatial)
{
    spatial_reasoning_t* sr = parietal_get_spatial(parietal);
    EXPECT_NE(sr, nullptr);
}

TEST_F(ParietalTest, GetMathIntuition)
{
    math_intuition_t* mi = parietal_get_math_intuition(parietal);
    EXPECT_NE(mi, nullptr);
}

TEST_F(ParietalTest, GetScientific)
{
    scientific_reasoning_t* sci = parietal_get_scientific(parietal);
    EXPECT_NE(sci, nullptr);
}

TEST_F(ParietalTest, GetEquationEngine)
{
    equation_engine_t* eq = parietal_get_equation_engine(parietal);
    EXPECT_NE(eq, nullptr);
}

TEST_F(ParietalTest, SubmoduleAccessNullHandling)
{
    EXPECT_EQ(parietal_get_number_sense(nullptr), nullptr);
    EXPECT_EQ(parietal_get_spatial(nullptr), nullptr);
    EXPECT_EQ(parietal_get_math_intuition(nullptr), nullptr);
    EXPECT_EQ(parietal_get_scientific(nullptr), nullptr);
    EXPECT_EQ(parietal_get_equation_engine(nullptr), nullptr);
}

//=============================================================================
// Processing Tests - Number Sense
//=============================================================================

TEST_F(ParietalTest, ProcessEstimateQuantity)
{
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_ESTIMATE_QUANTITY;
    request.request_id = 1;

    float values[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    request.input.quantity_input.values = values;
    request.input.quantity_input.num_values = 5;

    parietal_result_t result = parietal_process(parietal, &request);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_ESTIMATE_QUANTITY);
    EXPECT_GT(result.output.estimate.magnitude, 0.0f);
}

TEST_F(ParietalTest, ProcessCompareQuantities)
{
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_COMPARE_QUANTITIES;
    request.request_id = 2;
    request.input.comparison_input.magnitude_a = 10.0f;
    request.input.comparison_input.magnitude_b = 20.0f;

    parietal_result_t result = parietal_process(parietal, &request);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output.comparison.direction, -1);  // 10 < 20
}

TEST_F(ParietalTest, ProcessApproximateArithmetic)
{
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_APPROXIMATE_ARITHMETIC;
    request.request_id = 3;
    request.input.arithmetic_input.a = 20.0f;
    request.input.arithmetic_input.b = 10.0f;
    request.input.arithmetic_input.operation = '+';

    parietal_result_t result = parietal_process(parietal, &request);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(result.output.arithmetic.result, 30.0f, 10.0f);
}

//=============================================================================
// Processing Tests - Pattern Detection
//=============================================================================

TEST_F(ParietalTest, ProcessPatternDetect)
{
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_PATTERN_DETECT;
    request.request_id = 4;

    float sequence[] = {2.0f, 4.0f, 6.0f, 8.0f, 10.0f};
    request.input.pattern_input.sequence = sequence;
    request.input.pattern_input.length = 5;

    parietal_result_t result = parietal_process(parietal, &request);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output.pattern.type, PATTERN_ARITHMETIC);
    EXPECT_GT(result.output.pattern.confidence, 0.8f);
}

TEST_F(ParietalTest, ProcessSolveAnalogy)
{
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_SOLVE_ANALOGY;
    request.request_id = 5;
    request.input.analogy_input.a = 2.0f;
    request.input.analogy_input.b = 4.0f;
    request.input.analogy_input.c = 5.0f;

    parietal_result_t result = parietal_process(parietal, &request);

    EXPECT_TRUE(result.success);
    // Could be additive (7.0) or multiplicative (10.0) - both are valid
    float answer = result.output.analogy.answer;
    bool valid_answer = (fabsf(answer - 7.0f) < 1.0f) || (fabsf(answer - 10.0f) < 1.0f);
    EXPECT_TRUE(valid_answer);
}

//=============================================================================
// Processing Tests - Equation
//=============================================================================

TEST_F(ParietalTest, ProcessParseExpression)
{
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_PARSE_EXPRESSION;
    request.request_id = 6;
    strncpy(request.input.equation_input.expression, "x^2 + 1", 511);

    parietal_result_t result = parietal_process(parietal, &request);

    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.expression, nullptr);

    // Clean up expression if returned
    if (result.output.expression) {
        equation_free_expr(result.output.expression);
    }
}

TEST_F(ParietalTest, ProcessDifferentiate)
{
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_DIFFERENTIATE;
    request.request_id = 7;
    strncpy(request.input.equation_input.expression, "x^2", 511);
    strncpy(request.input.equation_input.variable, "x", 31);

    parietal_result_t result = parietal_process(parietal, &request);

    EXPECT_TRUE(result.success);

    if (result.output.expression) {
        // Evaluate derivative at x=3: d/dx[x^2] = 2x = 6
        variable_binding_t bind = {"x", 3.0f};
        equation_engine_t* eq = parietal_get_equation_engine(parietal);
        float value = equation_evaluate(eq, result.output.expression, &bind, 1);
        EXPECT_NEAR(value, 6.0f, 0.5f);

        equation_free_expr(result.output.expression);
    }
}

TEST_F(ParietalTest, ProcessEvaluate)
{
    // First parse an expression
    equation_engine_t* eq = parietal_get_equation_engine(parietal);
    expr_node_t* expr = equation_parse(eq, "x + 5");
    ASSERT_NE(expr, nullptr);

    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_EVALUATE;
    request.request_id = 8;

    variable_binding_t bind = {"x", 10.0f};
    request.input.eval_input.expr = expr;
    request.input.eval_input.bindings = &bind;
    request.input.eval_input.num_bindings = 1;

    parietal_result_t result = parietal_process(parietal, &request);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(result.output.evaluated_value, 15.0f, FLOAT_TOLERANCE);

    equation_free_expr(expr);
}

//=============================================================================
// Convenience Wrapper Tests
//=============================================================================

TEST_F(ParietalTest, EstimateQuantityWrapper)
{
    float values[] = {1.0f, 1.0f, 1.0f};
    number_estimate_t est = parietal_estimate_quantity(parietal, values, 3);

    EXPECT_GT(est.magnitude, 0.0f);
}

TEST_F(ParietalTest, DetectPatternWrapper)
{
    float sequence[] = {1.0f, 2.0f, 3.0f, 4.0f};
    detected_pattern_t pattern = parietal_detect_pattern(parietal, sequence, 4);

    EXPECT_EQ(pattern.type, PATTERN_ARITHMETIC);
}

TEST_F(ParietalTest, DifferentiateExpressionWrapper)
{
    expr_node_t* deriv = parietal_differentiate_expression(parietal, "x^3", "x");
    ASSERT_NE(deriv, nullptr);

    // d/dx[x^3] = 3x^2
    // At x=2: 3*4 = 12
    variable_binding_t bind = {"x", 2.0f};
    equation_engine_t* eq = parietal_get_equation_engine(parietal);
    float value = equation_evaluate(eq, deriv, &bind, 1);
    EXPECT_NEAR(value, 12.0f, 0.5f);

    equation_free_expr(deriv);
}

TEST_F(ParietalTest, MentalRotateWrapper)
{
    spatial_object_t obj_a;
    memset(&obj_a, 0, sizeof(obj_a));
    obj_a.position = vec3_create(0, 0, 0);
    obj_a.orientation = quaternion_identity();

    spatial_object_t obj_b = obj_a;

    rotation_result_t result = parietal_mental_rotate_compare(parietal, &obj_a, &obj_b);

    EXPECT_TRUE(result.is_match);
    EXPECT_GT(result.confidence, 0.5f);  // Identical objects should match with good confidence
}

//=============================================================================
// Physics NN Tests
//=============================================================================

TEST_F(ParietalTest, ComputeHamiltonian)
{
    // Must match physics NN state_dim (8): 4 positions + 4 momenta
    float state[] = {1.0f, 0.0f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 0.5f};
    float H = parietal_compute_hamiltonian(parietal, state, 8);

    // Hamiltonian should be defined (not NaN)
    EXPECT_TRUE(H == H);
    EXPECT_GE(H, 0.0f);  // Energy should be non-negative
}

TEST_F(ParietalTest, PredictDynamics)
{
    // Must match physics NN state_dim (8): 4 positions + 4 momenta
    const int state_dim = 8;
    float initial[] = {1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.5f};
    float* predictions[10];
    for (int i = 0; i < 10; i++) {
        predictions[i] = (float*)malloc(state_dim * sizeof(float));
    }

    int result = parietal_predict_dynamics(parietal, initial, state_dim, 0.01f, 10, predictions);
    EXPECT_EQ(result, 0);

    // Predictions should be defined
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(predictions[i][0] == predictions[i][0]);  // Not NaN
        free(predictions[i]);
    }
}

TEST_F(ParietalTest, TrainPhysicsNN)
{
    // Simple training data - must match physics NN state_dim (8)
    const int state_dim = 8;
    float* states[5];
    float* derivs[5];
    for (int i = 0; i < 5; i++) {
        states[i] = (float*)malloc(state_dim * sizeof(float));
        derivs[i] = (float*)malloc(state_dim * sizeof(float));
        for (int j = 0; j < state_dim; j++) {
            states[i][j] = (float)i * 0.1f + (float)j * 0.1f;
            derivs[i][j] = (float)j * 0.01f;
        }
    }

    float loss = parietal_train_physics_nn(parietal, (const float**)states,
                                            (const float**)derivs, 5, 10);

    EXPECT_GE(loss, 0.0f);

    for (int i = 0; i < 5; i++) {
        free(states[i]);
        free(derivs[i]);
    }
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(ParietalTest, SetInflammation)
{
    EXPECT_EQ(parietal_set_inflammation(parietal, 0.5f), 0);

    // Verify it propagated to submodules
    number_sense_t* ns = parietal_get_number_sense(parietal);
    float wf = number_sense_get_effective_weber_fraction(ns);
    EXPECT_GT(wf, NUMBER_SENSE_DEFAULT_WEBER_FRACTION);  // Should be increased
}

TEST_F(ParietalTest, SetInflammationBoundary)
{
    // Implementation clamps out-of-bounds values and returns success
    EXPECT_EQ(parietal_set_inflammation(parietal, 0.0f), 0);
    EXPECT_EQ(parietal_set_inflammation(parietal, 1.0f), 0);
    EXPECT_EQ(parietal_set_inflammation(parietal, -0.1f), 0);  // Clamped to 0
    EXPECT_EQ(parietal_set_inflammation(parietal, 1.1f), 0);   // Clamped to 1
}

TEST_F(ParietalTest, SetInflammationNullHandling)
{
    EXPECT_NE(parietal_set_inflammation(nullptr, 0.5f), 0);
}

TEST_F(ParietalTest, SetFatigue)
{
    EXPECT_EQ(parietal_set_fatigue(parietal, 0.5f), 0);
    EXPECT_NE(parietal_set_fatigue(nullptr, 0.5f), 0);
}

//=============================================================================
// Stepping Tests
//=============================================================================

TEST_F(ParietalTest, Step)
{
    // Queue a request
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_ESTIMATE_QUANTITY;
    request.async = true;

    float values[] = {1.0f, 2.0f, 3.0f};
    request.input.quantity_input.values = values;
    request.input.quantity_input.num_values = 3;

    // Step should process without error
    EXPECT_EQ(parietal_step(parietal, 1000), 0);
}

TEST_F(ParietalTest, ProcessPending)
{
    uint32_t processed = parietal_process_pending(parietal);
    EXPECT_GE(processed, 0);  // May be 0 if queue is empty
}

TEST_F(ParietalTest, StepNullHandling)
{
    EXPECT_NE(parietal_step(nullptr, 1000), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ParietalTest, GetStats)
{
    // Perform some operations
    float values[] = {1.0f, 2.0f, 3.0f};
    parietal_estimate_quantity(parietal, values, 3);

    float seq[] = {1.0f, 2.0f, 3.0f};
    parietal_detect_pattern(parietal, seq, 3);

    parietal_stats_t stats;
    EXPECT_EQ(parietal_get_stats(parietal, &stats), 0);

    EXPECT_GT(stats.total_requests, 0);
    EXPECT_GT(stats.number_sense.estimates_performed, 0);
    EXPECT_GT(stats.math_intuition.patterns_detected, 0);
}

TEST_F(ParietalTest, GetStatsNullHandling)
{
    parietal_stats_t stats;
    EXPECT_NE(parietal_get_stats(nullptr, &stats), 0);
    EXPECT_NE(parietal_get_stats(parietal, nullptr), 0);
}

TEST_F(ParietalTest, ResetStats)
{
    float values[] = {1.0f};
    parietal_estimate_quantity(parietal, values, 1);

    parietal_reset_stats(parietal);

    parietal_stats_t stats;
    parietal_get_stats(parietal, &stats);
    EXPECT_EQ(stats.total_requests, 0);
}

TEST_F(ParietalTest, ResetStatsNullSafe)
{
    parietal_reset_stats(nullptr);
    // Should not crash
}

//=============================================================================
// Null Handling Tests
//=============================================================================

TEST_F(ParietalTest, ProcessNullRequest)
{
    parietal_result_t result = parietal_process(parietal, nullptr);
    EXPECT_FALSE(result.success);
}

TEST_F(ParietalTest, ProcessNullParietal)
{
    parietal_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = PARIETAL_ESTIMATE_QUANTITY;

    parietal_result_t result = parietal_process(nullptr, &request);
    EXPECT_FALSE(result.success);
}

//=============================================================================
// Bio-async Message Tests
//=============================================================================

TEST_F(ParietalTest, HandleBioMsg)
{
    // Create a simple message payload
    float payload[2] = {10.0f, 20.0f};

    int result = parietal_handle_bio_msg(parietal, BIO_MSG_PARIETAL_COMPARE,
                                          payload, sizeof(payload));
    EXPECT_EQ(result, 0);
}

TEST_F(ParietalTest, HandleBioMsgNullHandling)
{
    float payload[1] = {1.0f};
    EXPECT_NE(parietal_handle_bio_msg(nullptr, BIO_MSG_PARIETAL_ESTIMATE,
                                       payload, sizeof(payload)), 0);
}

//=============================================================================
// Integration Attachment Tests (Null-safe stubs)
//=============================================================================

TEST_F(ParietalTest, AttachImmuneNull)
{
    // Should handle null gracefully
    int result = parietal_attach_immune(parietal, nullptr);
    // May return error or success depending on implementation
    (void)result;
}

TEST_F(ParietalTest, AttachThalamicNull)
{
    int result = parietal_attach_thalamus(parietal, nullptr);
    (void)result;
}

TEST_F(ParietalTest, AttachSubstrateNull)
{
    int result = parietal_attach_substrate(parietal, nullptr);
    (void)result;
}

TEST_F(ParietalTest, AttachFEPNull)
{
    int result = parietal_attach_fep(parietal, nullptr);
    (void)result;
}

TEST_F(ParietalTest, AttachWorkingMemoryNull)
{
    int result = parietal_attach_working_memory(parietal, nullptr);
    (void)result;
}

TEST_F(ParietalTest, AttachLogicGatesNull)
{
    int result = parietal_attach_logic_gates(parietal, nullptr);
    (void)result;
}

TEST_F(ParietalTest, AttachTrainingNull)
{
    int result = parietal_attach_training(parietal, nullptr);
    (void)result;
}

TEST_F(ParietalTest, AttachPerceptionNull)
{
    int result = parietal_attach_perception(parietal, nullptr);
    (void)result;
}

TEST_F(ParietalTest, AttachSleepNull)
{
    int result = parietal_attach_sleep(parietal, nullptr);
    (void)result;
}

TEST_F(ParietalTest, GetBrainRegionWithoutAttachment)
{
    brain_region_t* region = parietal_get_brain_region(parietal);
    EXPECT_EQ(region, nullptr);  // No brain attached yet
}

//=============================================================================
// Request Type Coverage Tests
//=============================================================================

TEST_F(ParietalTest, AllRequestTypesExist)
{
    // Verify all request types are defined with correct enum values

    // Number sense (0-2)
    EXPECT_EQ(PARIETAL_ESTIMATE_QUANTITY, 0);
    EXPECT_EQ(PARIETAL_COMPARE_QUANTITIES, 1);
    EXPECT_EQ(PARIETAL_APPROXIMATE_ARITHMETIC, 2);

    // Spatial reasoning (3-5)
    EXPECT_EQ(PARIETAL_MENTAL_ROTATION, 3);
    EXPECT_EQ(PARIETAL_COORDINATE_TRANSFORM, 4);
    EXPECT_EQ(PARIETAL_SPATIAL_QUERY, 5);

    // Mathematical intuition (6-9)
    EXPECT_EQ(PARIETAL_PATTERN_DETECT, 6);
    EXPECT_EQ(PARIETAL_PATTERN_EXTRAPOLATE, 7);
    EXPECT_EQ(PARIETAL_SYMMETRY_DETECT, 8);
    EXPECT_EQ(PARIETAL_SOLVE_ANALOGY, 9);

    // Scientific reasoning (10-13)
    EXPECT_EQ(PARIETAL_HYPOTHESIS_CREATE, 10);
    EXPECT_EQ(PARIETAL_HYPOTHESIS_UPDATE, 11);
    EXPECT_EQ(PARIETAL_DIMENSIONAL_ANALYSIS, 12);
    EXPECT_EQ(PARIETAL_CAUSAL_INFERENCE, 13);

    // Equation manipulation (14-18)
    EXPECT_EQ(PARIETAL_PARSE_EXPRESSION, 14);
    EXPECT_EQ(PARIETAL_DIFFERENTIATE, 15);
    EXPECT_EQ(PARIETAL_SIMPLIFY, 16);
    EXPECT_EQ(PARIETAL_EVALUATE, 17);
    EXPECT_EQ(PARIETAL_SOLVE_EQUATION, 18);

    // Physics NN (19-20)
    EXPECT_EQ(PARIETAL_PHYSICS_PREDICT, 19);
    EXPECT_EQ(PARIETAL_LEARN_DYNAMICS, 20);

    // Engineering domains (21-29)
    EXPECT_EQ(PARIETAL_ELECTRICAL_CIRCUIT_ANALYZE, 21);
    EXPECT_EQ(PARIETAL_ELECTRICAL_FILTER_DESIGN, 22);
    EXPECT_EQ(PARIETAL_ELECTRICAL_STABILITY_ANALYZE, 23);
    EXPECT_EQ(PARIETAL_MECHANICAL_STATIC_ANALYZE, 24);
    EXPECT_EQ(PARIETAL_MECHANICAL_MODAL_ANALYZE, 25);
    EXPECT_EQ(PARIETAL_MECHANICAL_THERMAL_ANALYZE, 26);
    EXPECT_EQ(PARIETAL_CIVIL_STRUCTURAL_ANALYZE, 27);
    EXPECT_EQ(PARIETAL_CIVIL_FOUNDATION_ANALYZE, 28);
    EXPECT_EQ(PARIETAL_CIVIL_HYDRAULIC_ANALYZE, 29);

    // Quantum-accelerated processing (30-34)
    EXPECT_EQ(PARIETAL_QUANTUM_OPTIMIZE, 30);
    EXPECT_EQ(PARIETAL_QUANTUM_TOPOLOGY_OPT, 31);
    EXPECT_EQ(PARIETAL_QUANTUM_VQE_COMPUTE, 32);
    EXPECT_EQ(PARIETAL_QUANTUM_WALK_SEARCH, 33);
    EXPECT_EQ(PARIETAL_QUANTUM_SOLVE_QUBO, 34);

    // Free Energy Principle processing (35-41)
    EXPECT_EQ(PARIETAL_FEP_UPDATE_BELIEFS, 35);
    EXPECT_EQ(PARIETAL_FEP_PREDICT, 36);
    EXPECT_EQ(PARIETAL_FEP_ACTIVE_INFERENCE, 37);
    EXPECT_EQ(PARIETAL_FEP_COMPUTE_SURPRISE, 38);
    EXPECT_EQ(PARIETAL_FEP_NUMERICAL_INFERENCE, 39);
    EXPECT_EQ(PARIETAL_FEP_SPATIAL_INFERENCE, 40);
    EXPECT_EQ(PARIETAL_FEP_PHYSICS_INFERENCE, 41);

    // Financial analysis (42-54)
    EXPECT_EQ(PARIETAL_FINANCIAL_PORTFOLIO_ANALYZE, 42);
    EXPECT_EQ(PARIETAL_FINANCIAL_RISK_ASSESS, 43);
    EXPECT_EQ(PARIETAL_FINANCIAL_OPTION_PRICE, 44);
    EXPECT_EQ(PARIETAL_FINANCIAL_VALUATION, 45);
    EXPECT_EQ(PARIETAL_FINANCIAL_OPTIMIZE, 46);
    EXPECT_EQ(PARIETAL_FINANCIAL_MARKET_REGIME, 47);
    EXPECT_EQ(PARIETAL_FINANCIAL_SENTIMENT, 48);
    EXPECT_EQ(PARIETAL_FINANCIAL_GARCH_FIT, 49);
    EXPECT_EQ(PARIETAL_FINANCIAL_INDICATOR, 50);
    EXPECT_EQ(PARIETAL_FINANCIAL_SCENARIO, 51);
    EXPECT_EQ(PARIETAL_FINANCIAL_MONTE_CARLO, 52);
    EXPECT_EQ(PARIETAL_FINANCIAL_ARCHETYPE_EVAL, 53);
    EXPECT_EQ(PARIETAL_FINANCIAL_ARCHETYPE_BLEND, 54);

    EXPECT_EQ(PARIETAL_REQUEST_TYPE_COUNT, 55);
}

}  // namespace
