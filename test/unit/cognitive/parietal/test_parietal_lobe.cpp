/**
 * @file test_parietal_lobe.cpp
 * @brief Unit tests for Parietal Lobe Orchestrator
 *
 * WHAT: Tests parietal lobe orchestrator functionality
 * WHY:  Verify request processing, submodule integration, modulation
 * HOW:  GTest framework with comprehensive coverage
 *
 * TEST CATEGORIES:
 * - Lifecycle tests (create, destroy, config validation)
 * - Request processing tests (all request types)
 * - Submodule access tests
 * - Modulation tests (inflammation, fatigue propagation)
 * - Statistics tests
 * - Async processing tests
 * - FEP integration tests
 * - Quantum bridge tests
 * - Engineering module tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_number_sense.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_equation_manipulation.h"
#include "cognitive/parietal/nimcp_fep_parietal_bridge.h"
#include "cognitive/parietal/nimcp_parietal_quantum_bridge.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

/**
 * @brief Test fixture for parietal lobe unit tests
 */
class ParietalLobeTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = parietal_default_config();
        parietal = parietal_create_custom(&config);
        ASSERT_NE(parietal, nullptr) << "Failed to create parietal lobe: "
                                      << parietal_get_last_error();
    }

    void TearDown() override {
        if (parietal) {
            parietal_destroy(parietal);
            parietal = nullptr;
        }
    }

    // Helper to create test sequence
    std::vector<float> create_test_sequence(uint32_t length, float start = 1.0f) {
        std::vector<float> seq(length);
        for (uint32_t i = 0; i < length; i++) {
            seq[i] = start + (float)i;
        }
        return seq;
    }

    parietal_config_t config;
    parietal_lobe_t* parietal = nullptr;
};

/* ============================================================================
 * LIFECYCLE TESTS
 * ============================================================================ */

/**
 * @brief Test default config has valid values
 */
TEST_F(ParietalLobeTest, DefaultConfigValid) {
    parietal_config_t default_config = parietal_default_config();

    EXPECT_TRUE(default_config.enable_neural_network);
    EXPECT_TRUE(default_config.enable_immune_bridge);
    EXPECT_TRUE(default_config.enable_thalamic_routing);
    EXPECT_TRUE(default_config.enable_fep_bridge);
    EXPECT_TRUE(default_config.enable_quantum_bridge);
    EXPECT_TRUE(default_config.enable_fep_parietal_bridge);
    EXPECT_TRUE(default_config.enable_electrical_eng);
    EXPECT_TRUE(default_config.enable_mechanical_eng);
    EXPECT_TRUE(default_config.enable_civil_eng);
    EXPECT_GT(default_config.nn_hidden_size, 0);
    EXPECT_GT(default_config.nn_learning_rate, 0.0f);
    EXPECT_GT(default_config.max_parallel_requests, 0);
}

/**
 * @brief Test config validation
 */
TEST_F(ParietalLobeTest, ConfigValidation) {
    parietal_config_t valid_config = parietal_default_config();
    EXPECT_TRUE(parietal_validate_config(&valid_config));

    // Invalid hidden size
    parietal_config_t invalid_config = valid_config;
    invalid_config.nn_hidden_size = 0;
    EXPECT_FALSE(parietal_validate_config(&invalid_config));

    // Invalid learning rate
    invalid_config = valid_config;
    invalid_config.nn_learning_rate = -0.1f;
    EXPECT_FALSE(parietal_validate_config(&invalid_config));

    // Null config
    EXPECT_FALSE(parietal_validate_config(nullptr));
}

/**
 * @brief Test creation with default config
 */
TEST_F(ParietalLobeTest, CreateWithDefaultConfig) {
    parietal_lobe_t* test_parietal = parietal_create();
    ASSERT_NE(test_parietal, nullptr);
    parietal_destroy(test_parietal);
}

/**
 * @brief Test creation with custom config
 */
TEST_F(ParietalLobeTest, CreateWithCustomConfig) {
    parietal_config_t custom_config = parietal_default_config();
    custom_config.nn_hidden_size = 128;
    custom_config.enable_quantum_bridge = false;

    parietal_lobe_t* test_parietal = parietal_create_custom(&custom_config);
    ASSERT_NE(test_parietal, nullptr);
    parietal_destroy(test_parietal);
}

/**
 * @brief Test null safety for destroy
 */
TEST_F(ParietalLobeTest, DestroyNullSafe) {
    parietal_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * SUBMODULE ACCESS TESTS
 * ============================================================================ */

/**
 * @brief Test number sense submodule access
 */
TEST_F(ParietalLobeTest, GetNumberSense) {
    number_sense_t* ns = parietal_get_number_sense(parietal);
    EXPECT_NE(ns, nullptr);
}

/**
 * @brief Test spatial reasoning submodule access
 */
TEST_F(ParietalLobeTest, GetSpatialReasoning) {
    spatial_reasoning_t* sr = parietal_get_spatial(parietal);
    EXPECT_NE(sr, nullptr);
}

/**
 * @brief Test math intuition submodule access
 */
TEST_F(ParietalLobeTest, GetMathIntuition) {
    math_intuition_t* mi = parietal_get_math_intuition(parietal);
    EXPECT_NE(mi, nullptr);
}

/**
 * @brief Test scientific reasoning submodule access
 */
TEST_F(ParietalLobeTest, GetScientificReasoning) {
    scientific_reasoning_t* sr = parietal_get_scientific(parietal);
    EXPECT_NE(sr, nullptr);
}

/**
 * @brief Test equation engine submodule access
 */
TEST_F(ParietalLobeTest, GetEquationEngine) {
    equation_engine_t* eq = parietal_get_equation_engine(parietal);
    EXPECT_NE(eq, nullptr);
}

/**
 * @brief Test quantum bridge submodule access
 */
TEST_F(ParietalLobeTest, GetQuantumBridge) {
    if (config.enable_quantum_bridge) {
        parietal_quantum_bridge_t* qb = parietal_get_quantum_bridge(parietal);
        EXPECT_NE(qb, nullptr);
    }
}

/**
 * @brief Test FEP bridge submodule access
 */
TEST_F(ParietalLobeTest, GetFepBridge) {
    if (config.enable_fep_parietal_bridge) {
        fep_parietal_bridge_t* fb = parietal_get_fep_bridge(parietal);
        EXPECT_NE(fb, nullptr);
    }
}

/**
 * @brief Test engineering submodule access
 */
TEST_F(ParietalLobeTest, GetEngineeringSubmodules) {
    if (config.enable_electrical_eng) {
        electrical_eng_t* ee = parietal_get_electrical(parietal);
        EXPECT_NE(ee, nullptr);
    }
    if (config.enable_mechanical_eng) {
        mechanical_eng_t* me = parietal_get_mechanical(parietal);
        EXPECT_NE(me, nullptr);
    }
    if (config.enable_civil_eng) {
        civil_eng_t* ce = parietal_get_civil(parietal);
        EXPECT_NE(ce, nullptr);
    }
}

/**
 * @brief Test null safety for submodule access
 */
TEST_F(ParietalLobeTest, SubmoduleAccessNullSafe) {
    EXPECT_EQ(parietal_get_number_sense(nullptr), nullptr);
    EXPECT_EQ(parietal_get_spatial(nullptr), nullptr);
    EXPECT_EQ(parietal_get_math_intuition(nullptr), nullptr);
    EXPECT_EQ(parietal_get_scientific(nullptr), nullptr);
    EXPECT_EQ(parietal_get_equation_engine(nullptr), nullptr);
    EXPECT_EQ(parietal_get_quantum_bridge(nullptr), nullptr);
    EXPECT_EQ(parietal_get_fep_bridge(nullptr), nullptr);
    EXPECT_EQ(parietal_get_electrical(nullptr), nullptr);
    EXPECT_EQ(parietal_get_mechanical(nullptr), nullptr);
    EXPECT_EQ(parietal_get_civil(nullptr), nullptr);
}

/* ============================================================================
 * NUMBER SENSE REQUEST TESTS
 * ============================================================================ */

/**
 * @brief Test quantity estimation request
 */
TEST_F(ParietalLobeTest, EstimateQuantityRequest) {
    auto values = create_test_sequence(10);

    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;
    req.input.quantity_input.values = values.data();
    req.input.quantity_input.num_values = (uint32_t)values.size();

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_ESTIMATE_QUANTITY);
    EXPECT_GT(result.output.estimate.estimated_count, 0.0f);
    EXPECT_GT(result.confidence, 0.0f);
}

/**
 * @brief Test quantity comparison request
 */
TEST_F(ParietalLobeTest, CompareQuantitiesRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_COMPARE_QUANTITIES;
    req.input.comparison_input.magnitude_a = 10.0f;
    req.input.comparison_input.magnitude_b = 5.0f;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_COMPARE_QUANTITIES);
    EXPECT_GT(result.confidence, 0.0f);
}

/**
 * @brief Test approximate arithmetic request
 */
TEST_F(ParietalLobeTest, ApproximateArithmeticRequest) {
    char operations[] = {'+', '-', '*', '/'};

    for (char op : operations) {
        parietal_request_t req;
        memset(&req, 0, sizeof(req));
        req.type = PARIETAL_APPROXIMATE_ARITHMETIC;
        req.input.arithmetic_input.a = 7.0f;
        req.input.arithmetic_input.b = 3.0f;
        req.input.arithmetic_input.operation = op;

        parietal_result_t result = parietal_process(parietal, &req);

        EXPECT_TRUE(result.success) << "Failed for operation: " << op;
        EXPECT_GT(result.confidence, 0.0f);
    }
}

/* ============================================================================
 * PATTERN DETECTION REQUEST TESTS
 * ============================================================================ */

/**
 * @brief Test pattern detection request
 */
TEST_F(ParietalLobeTest, PatternDetectRequest) {
    // Fibonacci-like sequence
    float sequence[] = {1.0f, 1.0f, 2.0f, 3.0f, 5.0f, 8.0f, 13.0f};

    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_PATTERN_DETECT;
    req.input.pattern_input.sequence = sequence;
    req.input.pattern_input.length = 7;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_PATTERN_DETECT);
    EXPECT_GT(result.confidence, 0.0f);
}

/**
 * @brief Test analogy solving request
 */
TEST_F(ParietalLobeTest, SolveAnalogyRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_SOLVE_ANALOGY;
    req.input.analogy_input.a = 2.0f;
    req.input.analogy_input.b = 4.0f;
    req.input.analogy_input.c = 3.0f;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_SOLVE_ANALOGY);
}

/* ============================================================================
 * EQUATION MANIPULATION REQUEST TESTS
 * ============================================================================ */

/**
 * @brief Test parse expression request
 */
TEST_F(ParietalLobeTest, ParseExpressionRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_PARSE_EXPRESSION;
    strncpy(req.input.equation_input.expression, "x^2 + 2*x + 1", 511);

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_PARSE_EXPRESSION);
    EXPECT_NE(result.output.expression, nullptr);

    if (result.output.expression) {
        equation_free_expr(result.output.expression);
    }
}

/**
 * @brief Test differentiate request
 */
TEST_F(ParietalLobeTest, DifferentiateRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_DIFFERENTIATE;
    strncpy(req.input.equation_input.expression, "x^2", 511);
    strncpy(req.input.equation_input.variable, "x", 31);

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_DIFFERENTIATE);

    if (result.output.expression) {
        equation_free_expr(result.output.expression);
    }
}

/**
 * @brief Test simplify request
 */
TEST_F(ParietalLobeTest, SimplifyRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_SIMPLIFY;
    strncpy(req.input.equation_input.expression, "x + x", 511);

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_SIMPLIFY);

    if (result.output.expression) {
        equation_free_expr(result.output.expression);
    }
}

/* ============================================================================
 * PHYSICS NN REQUEST TESTS
 * ============================================================================ */

/**
 * @brief Test physics prediction request
 */
TEST_F(ParietalLobeTest, PhysicsPredictRequest) {
    float state[] = {1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f, 0.0f, 0.5f};

    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_PHYSICS_PREDICT;
    req.input.physics_input.state = state;
    req.input.physics_input.state_dim = 8;
    req.input.physics_input.dt = 0.01f;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, PARIETAL_PHYSICS_PREDICT);

    if (result.output.physics_output.predicted_state) {
        free(result.output.physics_output.predicted_state);
    }
}

/* ============================================================================
 * FEP REQUEST TESTS
 * ============================================================================ */

/**
 * @brief Test FEP belief update request
 */
TEST_F(ParietalLobeTest, FepBeliefUpdateRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_UPDATE_BELIEFS;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_fep_parietal_bridge) {
        EXPECT_TRUE(result.success);
    } else {
        EXPECT_FALSE(result.success);
    }
}

/**
 * @brief Test FEP predict request
 */
TEST_F(ParietalLobeTest, FepPredictRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_PREDICT;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_fep_parietal_bridge) {
        EXPECT_TRUE(result.success);
    }
}

/**
 * @brief Test FEP active inference request
 */
TEST_F(ParietalLobeTest, FepActiveInferenceRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_ACTIVE_INFERENCE;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_fep_parietal_bridge) {
        EXPECT_TRUE(result.success);
    }
}

/**
 * @brief Test FEP compute surprise request
 */
TEST_F(ParietalLobeTest, FepComputeSurpriseRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_COMPUTE_SURPRISE;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_fep_parietal_bridge) {
        EXPECT_TRUE(result.success);
    }
}

/**
 * @brief Test FEP numerical inference request
 */
TEST_F(ParietalLobeTest, FepNumericalInferenceRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_NUMERICAL_INFERENCE;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_fep_parietal_bridge) {
        EXPECT_TRUE(result.success);
    }
}

/**
 * @brief Test FEP spatial inference request
 */
TEST_F(ParietalLobeTest, FepSpatialInferenceRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_SPATIAL_INFERENCE;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_fep_parietal_bridge) {
        EXPECT_TRUE(result.success);
    }
}

/**
 * @brief Test FEP physics inference request
 */
TEST_F(ParietalLobeTest, FepPhysicsInferenceRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_PHYSICS_INFERENCE;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_fep_parietal_bridge) {
        EXPECT_TRUE(result.success);
    }
}

/* ============================================================================
 * QUANTUM REQUEST TESTS
 * ============================================================================ */

/**
 * @brief Test quantum optimize request
 */
TEST_F(ParietalLobeTest, QuantumOptimizeRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_QUANTUM_OPTIMIZE;

    parietal_result_t result = parietal_process(parietal, &req);

    // Result depends on whether quantum is available
    EXPECT_EQ(result.type, PARIETAL_QUANTUM_OPTIMIZE);
}

/**
 * @brief Test quantum VQE request
 */
TEST_F(ParietalLobeTest, QuantumVqeRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_QUANTUM_VQE_COMPUTE;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_EQ(result.type, PARIETAL_QUANTUM_VQE_COMPUTE);
}

/* ============================================================================
 * ENGINEERING REQUEST TESTS
 * ============================================================================ */

/**
 * @brief Test electrical circuit analysis request
 */
TEST_F(ParietalLobeTest, ElectricalCircuitRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ELECTRICAL_CIRCUIT_ANALYZE;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_electrical_eng) {
        EXPECT_TRUE(result.success);
    }
}

/**
 * @brief Test mechanical analysis request
 */
TEST_F(ParietalLobeTest, MechanicalAnalysisRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_MECHANICAL_STATIC_ANALYZE;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_mechanical_eng) {
        EXPECT_TRUE(result.success);
    }
}

/**
 * @brief Test civil engineering request
 */
TEST_F(ParietalLobeTest, CivilEngineeringRequest) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_CIVIL_STRUCTURAL_ANALYZE;

    parietal_result_t result = parietal_process(parietal, &req);

    if (config.enable_civil_eng) {
        EXPECT_TRUE(result.success);
    }
}

/* ============================================================================
 * MODULATION TESTS
 * ============================================================================ */

/**
 * @brief Test inflammation modulation
 */
TEST_F(ParietalLobeTest, SetInflammation) {
    EXPECT_EQ(parietal_set_inflammation(parietal, 0.0f), 0);
    EXPECT_EQ(parietal_set_inflammation(parietal, 0.5f), 0);
    EXPECT_EQ(parietal_set_inflammation(parietal, 1.0f), 0);

    // Clamping test
    EXPECT_EQ(parietal_set_inflammation(parietal, -0.5f), 0);
    EXPECT_EQ(parietal_set_inflammation(parietal, 1.5f), 0);

    // Null safety
    EXPECT_EQ(parietal_set_inflammation(nullptr, 0.5f), -1);
}

/**
 * @brief Test fatigue modulation
 */
TEST_F(ParietalLobeTest, SetFatigue) {
    EXPECT_EQ(parietal_set_fatigue(parietal, 0.0f), 0);
    EXPECT_EQ(parietal_set_fatigue(parietal, 0.5f), 0);
    EXPECT_EQ(parietal_set_fatigue(parietal, 1.0f), 0);

    // Null safety
    EXPECT_EQ(parietal_set_fatigue(nullptr, 0.5f), -1);
}

/**
 * @brief Test that modulation propagates to submodules
 */
TEST_F(ParietalLobeTest, ModulationPropagation) {
    // Set inflammation
    EXPECT_EQ(parietal_set_inflammation(parietal, 0.7f), 0);

    // Process a request - should work but potentially degraded
    auto values = create_test_sequence(5);
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;
    req.input.quantity_input.values = values.data();
    req.input.quantity_input.num_values = 5;

    parietal_result_t result = parietal_process(parietal, &req);
    EXPECT_TRUE(result.success);
}

/* ============================================================================
 * STATISTICS TESTS
 * ============================================================================ */

/**
 * @brief Test statistics collection
 */
TEST_F(ParietalLobeTest, StatisticsCollection) {
    parietal_stats_t stats;

    // Get initial stats
    EXPECT_EQ(parietal_get_stats(parietal, &stats), 0);
    uint64_t initial_requests = stats.total_requests;

    // Process some requests
    auto values = create_test_sequence(5);
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;
    req.input.quantity_input.values = values.data();
    req.input.quantity_input.num_values = 5;

    for (int i = 0; i < 3; i++) {
        parietal_process(parietal, &req);
    }

    // Verify stats updated
    EXPECT_EQ(parietal_get_stats(parietal, &stats), 0);
    EXPECT_GT(stats.total_requests, initial_requests);
    EXPECT_EQ(stats.requests_by_type[PARIETAL_ESTIMATE_QUANTITY], 3);
}

/**
 * @brief Test statistics reset
 */
TEST_F(ParietalLobeTest, StatisticsReset) {
    // Process a request first
    auto values = create_test_sequence(5);
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;
    req.input.quantity_input.values = values.data();
    req.input.quantity_input.num_values = 5;
    parietal_process(parietal, &req);

    // Reset stats
    parietal_reset_stats(parietal);

    // Verify reset
    parietal_stats_t stats;
    EXPECT_EQ(parietal_get_stats(parietal, &stats), 0);
    EXPECT_EQ(stats.total_requests, 0);
    EXPECT_EQ(stats.failed_requests, 0);
}

/**
 * @brief Test FEP stats are tracked
 */
TEST_F(ParietalLobeTest, FepStatisticsTracked) {
    if (!config.enable_fep_parietal_bridge) {
        GTEST_SKIP() << "FEP bridge not enabled";
    }

    parietal_stats_t stats;
    EXPECT_EQ(parietal_get_stats(parietal, &stats), 0);
    uint64_t initial_fep = stats.fep_predictions;

    // Process FEP request
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_PREDICT;
    parietal_process(parietal, &req);

    EXPECT_EQ(parietal_get_stats(parietal, &stats), 0);
    EXPECT_GE(stats.fep_predictions, initial_fep);
}

/**
 * @brief Test null safety for statistics
 */
TEST_F(ParietalLobeTest, StatisticsNullSafety) {
    parietal_stats_t stats;

    EXPECT_EQ(parietal_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(parietal_get_stats(parietal, nullptr), -1);

    parietal_reset_stats(nullptr);  // Should not crash
    SUCCEED();
}

/* ============================================================================
 * ASYNC PROCESSING TESTS
 * ============================================================================ */

/**
 * @brief Test async request submission
 */
TEST_F(ParietalLobeTest, AsyncRequestSubmission) {
    auto values = create_test_sequence(5);
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;
    req.input.quantity_input.values = values.data();
    req.input.quantity_input.num_values = 5;
    req.async = true;

    uint64_t request_id = parietal_process_async(parietal, &req, nullptr, nullptr);

    EXPECT_GT(request_id, 0);
}

/**
 * @brief Test async result polling
 */
TEST_F(ParietalLobeTest, AsyncResultPolling) {
    auto values = create_test_sequence(5);
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;
    req.input.quantity_input.values = values.data();
    req.input.quantity_input.num_values = 5;

    uint64_t request_id = parietal_process_async(parietal, &req, nullptr, nullptr);
    ASSERT_GT(request_id, 0);

    // Process pending
    parietal_process_pending(parietal);

    // Poll for result
    parietal_result_t result;
    int status = parietal_poll_result(parietal, request_id, &result);

    EXPECT_GE(status, 0);  // 0 = pending, 1 = ready
}

/**
 * @brief Test async result waiting
 */
TEST_F(ParietalLobeTest, AsyncResultWaiting) {
    auto values = create_test_sequence(5);
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;
    req.input.quantity_input.values = values.data();
    req.input.quantity_input.num_values = 5;

    uint64_t request_id = parietal_process_async(parietal, &req, nullptr, nullptr);
    ASSERT_GT(request_id, 0);

    parietal_result_t result;
    int status = parietal_wait_result(parietal, request_id, 1000, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.success);
}

/* ============================================================================
 * CONVENIENCE WRAPPER TESTS
 * ============================================================================ */

/**
 * @brief Test estimate quantity convenience wrapper
 */
TEST_F(ParietalLobeTest, EstimateQuantityWrapper) {
    auto values = create_test_sequence(10);

    number_estimate_t estimate = parietal_estimate_quantity(
        parietal, values.data(), (uint32_t)values.size());

    EXPECT_GT(estimate.estimated_count, 0.0f);
    EXPECT_GT(estimate.confidence, 0.0f);
}

/**
 * @brief Test detect pattern convenience wrapper
 */
TEST_F(ParietalLobeTest, DetectPatternWrapper) {
    float sequence[] = {2.0f, 4.0f, 8.0f, 16.0f, 32.0f};

    detected_pattern_t pattern = parietal_detect_pattern(parietal, sequence, 5);

    EXPECT_GT(pattern.confidence, 0.0f);
}

/**
 * @brief Test differentiate expression convenience wrapper
 */
TEST_F(ParietalLobeTest, DifferentiateExpressionWrapper) {
    expr_node_t* derivative = parietal_differentiate_expression(
        parietal, "x^3", "x");

    EXPECT_NE(derivative, nullptr);

    if (derivative) {
        equation_free_expr(derivative);
    }
}

/* ============================================================================
 * STEPPING TESTS
 * ============================================================================ */

/**
 * @brief Test parietal step function
 */
TEST_F(ParietalLobeTest, StepFunction) {
    EXPECT_EQ(parietal_step(parietal, 1000), 0);
    EXPECT_EQ(parietal_step(parietal, 0), 0);

    // Null safety
    EXPECT_EQ(parietal_step(nullptr, 1000), -1);
}

/**
 * @brief Test process pending function
 */
TEST_F(ParietalLobeTest, ProcessPending) {
    // Submit async requests
    auto values = create_test_sequence(5);
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;
    req.input.quantity_input.values = values.data();
    req.input.quantity_input.num_values = 5;

    for (int i = 0; i < 3; i++) {
        parietal_process_async(parietal, &req, nullptr, nullptr);
    }

    // Process all pending
    uint32_t processed = parietal_process_pending(parietal);

    EXPECT_EQ(processed, 3);
}

/* ============================================================================
 * QUANTUM BRIDGE TESTS
 * ============================================================================ */

/**
 * @brief Test quantum enabled setting
 */
TEST_F(ParietalLobeTest, QuantumEnabledSetting) {
    if (!config.enable_quantum_bridge) {
        GTEST_SKIP() << "Quantum bridge not enabled";
    }

    EXPECT_EQ(parietal_set_quantum_enabled(parietal, false), 0);
    EXPECT_FALSE(parietal_quantum_available(parietal));

    EXPECT_EQ(parietal_set_quantum_enabled(parietal, true), 0);
    EXPECT_TRUE(parietal_quantum_available(parietal));
}

/**
 * @brief Test null safety for quantum functions
 */
TEST_F(ParietalLobeTest, QuantumNullSafety) {
    EXPECT_EQ(parietal_set_quantum_enabled(nullptr, true), -1);
    EXPECT_FALSE(parietal_quantum_available(nullptr));
}

/* ============================================================================
 * ERROR HANDLING TESTS
 * ============================================================================ */

/**
 * @brief Test unknown request type handling
 */
TEST_F(ParietalLobeTest, UnknownRequestType) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = (parietal_request_type_t)999;  // Invalid type

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_FALSE(result.success);
    EXPECT_STRNE(result.error_message, "");
}

/**
 * @brief Test null request handling
 */
TEST_F(ParietalLobeTest, NullRequestHandling) {
    parietal_result_t result = parietal_process(parietal, nullptr);
    EXPECT_FALSE(result.success);
}

/**
 * @brief Test null parietal handling
 */
TEST_F(ParietalLobeTest, NullParietalHandling) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;

    parietal_result_t result = parietal_process(nullptr, &req);
    EXPECT_FALSE(result.success);
}

/**
 * @brief Test get last error
 */
TEST_F(ParietalLobeTest, GetLastError) {
    const char* error = parietal_get_last_error();
    EXPECT_NE(error, nullptr);
}
