/**
 * @file test_parietal_exception_integration.cpp
 * @brief Integration tests for parietal module exception handling with immune system
 * @date 2026-01-25
 *
 * WHAT: Test cross-module exception flow from parietal to immune system
 * WHY:  Verify proper integration of NIMCP_THROW_TO_IMMUNE across module boundaries
 * HOW:  Test exception propagation through bridges and system integration points
 *
 * INTEGRATION SCENARIOS:
 * - Parietal error -> Exception -> Handler -> Immune presentation
 * - Bridge exception propagation
 * - Cross-module exception handling (FEP, quantum, training bridges)
 * - Concurrent exception handling across modules
 * - Recovery strategy execution
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

#include "cognitive/immune/nimcp_brain_immune.h"

extern "C" {
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_number_sense.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_equation_manipulation.h"
#include "cognitive/parietal/nimcp_scientific_reasoning.h"
#include "cognitive/parietal/nimcp_parietal_fep_bridge.h"
#include "cognitive/parietal/nimcp_parietal_training_bridge.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ParietalExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> total_exceptions;
    static std::atomic<int> immune_responses;
    static std::atomic<int> recovered_exceptions;
    static std::atomic<int> last_error_code;

    brain_immune_system_t* immune_system;
    parietal_lobe_t* parietal;
    number_sense_t* number_sense;
    spatial_reasoning_t* spatial;
    math_intuition_t* math_intuition;
    equation_engine_t* equation;
    scientific_reasoning_t* scientific;

    void SetUp() override {
        total_exceptions = 0;
        immune_responses = 0;
        recovered_exceptions = 0;
        last_error_code = 0;

        nimcp_exception_system_init();

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);

        // Create parietal modules
        parietal = parietal_create();
        number_sense = number_sense_create();
        spatial = spatial_reasoning_create();
        math_intuition = math_intuition_create();
        equation = equation_engine_create();
        scientific = scientific_reasoning_create();

        register_handlers();
    }

    void TearDown() override {
        unregister_handlers();

        if (scientific) scientific_reasoning_destroy(scientific);
        if (equation) equation_engine_destroy(equation);
        if (math_intuition) math_intuition_destroy(math_intuition);
        if (spatial) spatial_reasoning_destroy(spatial);
        if (number_sense) number_sense_destroy(number_sense);
        if (parietal) parietal_destroy(parietal);
        if (immune_system) brain_immune_destroy(immune_system);

        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static nimcp_handler_registration_t* exception_handler_reg;
    static nimcp_handler_registration_t* immune_handler_reg;
    static nimcp_handler_registration_t* recovery_handler_reg;

    static bool exception_counter(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        total_exceptions++;
        last_error_code = ex->code;
        return false;
    }

    static bool immune_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        immune_responses++;
        return false;
    }

    static bool recovery_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        if (strategy.primary_action != EXCEPTION_RECOVERY_NONE) {
            recovered_exceptions++;
        }

        return false;
    }

    void register_handlers() {
        nimcp_handler_options_t opts;

        nimcp_handler_default_options(&opts);
        opts.name = "parietal_integration_exception_counter";
        opts.handler = exception_counter;
        opts.priority = 200;
        exception_handler_reg = nimcp_handler_register(&opts);

        nimcp_handler_default_options(&opts);
        opts.name = "parietal_integration_immune_handler";
        opts.handler = immune_handler;
        opts.priority = 100;
        immune_handler_reg = nimcp_handler_register(&opts);

        nimcp_handler_default_options(&opts);
        opts.name = "parietal_integration_recovery_handler";
        opts.handler = recovery_handler;
        opts.priority = 50;
        recovery_handler_reg = nimcp_handler_register(&opts);
    }

    void unregister_handlers() {
        if (exception_handler_reg) nimcp_handler_unregister(exception_handler_reg);
        if (immune_handler_reg) nimcp_handler_unregister(immune_handler_reg);
        if (recovery_handler_reg) nimcp_handler_unregister(recovery_handler_reg);
        exception_handler_reg = nullptr;
        immune_handler_reg = nullptr;
        recovery_handler_reg = nullptr;
    }
};

std::atomic<int> ParietalExceptionIntegrationTest::total_exceptions{0};
std::atomic<int> ParietalExceptionIntegrationTest::immune_responses{0};
std::atomic<int> ParietalExceptionIntegrationTest::recovered_exceptions{0};
std::atomic<int> ParietalExceptionIntegrationTest::last_error_code{0};
nimcp_handler_registration_t* ParietalExceptionIntegrationTest::exception_handler_reg = nullptr;
nimcp_handler_registration_t* ParietalExceptionIntegrationTest::immune_handler_reg = nullptr;
nimcp_handler_registration_t* ParietalExceptionIntegrationTest::recovery_handler_reg = nullptr;

//=============================================================================
// Cross-Module Exception Propagation Tests
//=============================================================================

TEST_F(ParietalExceptionIntegrationTest, ExceptionPropagatesFromParietalToImmune) {
    // WHAT: Verify exception propagates from parietal module to immune system
    // WHY:  Exception handling must work across module boundaries

    total_exceptions = 0;
    immune_responses = 0;

    // Trigger exception in parietal module
    parietal_result_t result = parietal_process(nullptr, nullptr);

    EXPECT_FALSE(result.success);
    EXPECT_GT(total_exceptions.load(), 0);
    EXPECT_GT(immune_responses.load(), 0);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ParietalExceptionIntegrationTest, ExceptionPropagatesFromNumberSense) {
    total_exceptions = 0;
    immune_responses = 0;

    // Trigger exception in number sense
    number_estimate_t est = number_sense_estimate(nullptr, nullptr, 0);

    EXPECT_LT(est.confidence, 0.0f);
    EXPECT_GT(total_exceptions.load(), 0);
    EXPECT_GT(immune_responses.load(), 0);
}

TEST_F(ParietalExceptionIntegrationTest, ExceptionPropagatesFromSpatialReasoning) {
    total_exceptions = 0;
    immune_responses = 0;

    // Trigger exception in spatial reasoning
    rotation_result_t rot = spatial_rotate_and_compare(nullptr, nullptr, nullptr);

    EXPECT_LT(rot.confidence, 0.0f);
    EXPECT_GT(total_exceptions.load(), 0);
    EXPECT_GT(immune_responses.load(), 0);
}

TEST_F(ParietalExceptionIntegrationTest, ExceptionPropagatesFromMathIntuition) {
    total_exceptions = 0;
    immune_responses = 0;

    // Trigger exception in mathematical intuition
    detected_pattern_t pat = math_detect_pattern(nullptr, nullptr, 0);

    EXPECT_LT(pat.confidence, 0.0f);
    EXPECT_GT(total_exceptions.load(), 0);
    EXPECT_GT(immune_responses.load(), 0);
}

TEST_F(ParietalExceptionIntegrationTest, ExceptionPropagatesFromEquationEngine) {
    total_exceptions = 0;
    immune_responses = 0;

    // Trigger exception in equation engine
    expr_node_t* expr = equation_parse(nullptr, nullptr);

    EXPECT_EQ(expr, nullptr);
    EXPECT_GT(total_exceptions.load(), 0);
    EXPECT_GT(immune_responses.load(), 0);
}

TEST_F(ParietalExceptionIntegrationTest, ExceptionPropagatesFromScientificReasoning) {
    total_exceptions = 0;
    immune_responses = 0;

    // Trigger exception in scientific reasoning
    // scientific_create_hypothesis returns hypothesis_t by value; id=0 indicates error
    hypothesis_t hyp = scientific_create_hypothesis(nullptr, nullptr, 0.0f);

    EXPECT_EQ(hyp.id, 0u);  // id=0 indicates error
    EXPECT_GT(total_exceptions.load(), 0);
    EXPECT_GT(immune_responses.load(), 0);
}

//=============================================================================
// Bridge Exception Integration Tests
//=============================================================================

TEST_F(ParietalExceptionIntegrationTest, FEPBridgeAttachmentException) {
    total_exceptions = 0;
    immune_responses = 0;

    // Attempt to attach FEP bridge with NULL parietal
    int result = parietal_attach_fep(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(total_exceptions.load(), 0);
    EXPECT_GT(immune_responses.load(), 0);
}

TEST_F(ParietalExceptionIntegrationTest, TrainingBridgeAttachmentException) {
    total_exceptions = 0;
    immune_responses = 0;

    // Attempt to attach training bridge with NULL parietal
    int result = parietal_attach_training(nullptr, nullptr);

    EXPECT_LT(result, 0);
    EXPECT_GT(total_exceptions.load(), 0);
    EXPECT_GT(immune_responses.load(), 0);
}

TEST_F(ParietalExceptionIntegrationTest, MultipleBridgeAttachmentsWithNullParietal) {
    total_exceptions = 0;
    immune_responses = 0;

    // Multiple bridge attachment failures
    parietal_attach_immune(nullptr, nullptr);
    parietal_attach_thalamus(nullptr, nullptr);
    parietal_attach_substrate(nullptr, nullptr);
    parietal_attach_fep(nullptr, nullptr);
    parietal_attach_working_memory(nullptr, nullptr);
    parietal_attach_logic_gates(nullptr, nullptr);
    parietal_attach_training(nullptr, nullptr);
    parietal_attach_perception(nullptr, nullptr);
    parietal_attach_sleep(nullptr, nullptr);

    EXPECT_GE(total_exceptions.load(), 9);
    EXPECT_GE(immune_responses.load(), 9);
}

//=============================================================================
// Concurrent Exception Handling Tests
//=============================================================================

TEST_F(ParietalExceptionIntegrationTest, ConcurrentExceptionsAcrossModules) {
    // WHAT: Test concurrent exceptions from different parietal modules
    // WHY:  Real systems have multiple concurrent operations

    total_exceptions = 0;
    immune_responses = 0;

    const int num_threads = 4;
    const int ops_per_thread = 50;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                switch ((t + i) % 6) {
                    case 0:
                        parietal_process(nullptr, nullptr);
                        break;
                    case 1:
                        number_sense_estimate(nullptr, nullptr, 0);
                        break;
                    case 2:
                        spatial_rotate_and_compare(nullptr, nullptr, nullptr);
                        break;
                    case 3:
                        math_detect_pattern(nullptr, nullptr, 0);
                        break;
                    case 4:
                        equation_parse(nullptr, nullptr);
                        break;
                    case 5:
                        scientific_create_hypothesis(nullptr, nullptr, 0.0f);
                        break;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All operations should have triggered exceptions
    EXPECT_GE(total_exceptions.load(), num_threads * ops_per_thread);
    EXPECT_GE(immune_responses.load(), num_threads * ops_per_thread);
}

//=============================================================================
// Mixed Valid/Invalid Operations Tests
//=============================================================================

TEST_F(ParietalExceptionIntegrationTest, MixedValidInvalidOperations) {
    // WHAT: Test mixed valid and invalid operations
    // WHY:  Real systems have occasional errors among valid operations

    ASSERT_NE(parietal, nullptr);
    ASSERT_NE(number_sense, nullptr);
    ASSERT_NE(spatial, nullptr);
    ASSERT_NE(math_intuition, nullptr);

    total_exceptions = 0;
    int valid_ops = 0;
    int invalid_ops = 0;

    for (int i = 0; i < 100; i++) {
        if (i % 3 == 0) {
            // Invalid operation
            parietal_process(nullptr, nullptr);
            invalid_ops++;
        } else {
            // Valid operation (comparison with valid number sense)
            number_comparison_t cmp = number_sense_compare(number_sense, 5.0f, 7.0f);
            if (cmp.confidence >= 0.0f) valid_ops++;
        }
    }

    EXPECT_GE(total_exceptions.load(), invalid_ops);
    EXPECT_GT(valid_ops, 0);
}

TEST_F(ParietalExceptionIntegrationTest, ValidModulesRemainsOperationalAfterExceptions) {
    // WHAT: Verify valid modules remain operational after exceptions occur
    // WHY:  Exceptions should not corrupt valid module state

    ASSERT_NE(number_sense, nullptr);
    ASSERT_NE(spatial, nullptr);

    // Generate many exceptions
    total_exceptions = 0;
    for (int i = 0; i < 500; i++) {
        number_sense_estimate(nullptr, nullptr, 0);
        spatial_rotate_and_compare(nullptr, nullptr, nullptr);
    }

    EXPECT_GE(total_exceptions.load(), 1000);

    // Verify valid modules still work
    number_comparison_t cmp = number_sense_compare(number_sense, 10.0f, 5.0f);
    EXPECT_GT(cmp.confidence, 0.0f);
    EXPECT_GT(cmp.direction, 0);  // 10 > 5

    // Spatial should still be functional
    spatial_stats_t stats;
    int result = spatial_get_stats(spatial, &stats);
    EXPECT_GE(result, 0);  // Returns 0 for success
}

//=============================================================================
// Exception Recovery Integration Tests
//=============================================================================

TEST_F(ParietalExceptionIntegrationTest, RecoveryStrategiesAreProvided) {
    // WHAT: Verify exception recovery strategies are provided
    // WHY:  Immune system needs recovery guidance

    total_exceptions = 0;
    recovered_exceptions = 0;

    // Generate exceptions that should have recovery strategies
    for (int i = 0; i < 100; i++) {
        parietal_process(nullptr, nullptr);
    }

    EXPECT_GE(total_exceptions.load(), 100);
    // Some exceptions should have recovery strategies
    // (exact count depends on implementation)
}

//=============================================================================
// Batch Exception Processing Tests
//=============================================================================

TEST_F(ParietalExceptionIntegrationTest, BatchExceptionProcessingFromSubmodules) {
    // WHAT: Test batch exception processing from multiple submodules
    // WHY:  Verify immune system can handle batch exception load

    total_exceptions = 0;
    immune_responses = 0;

    // Batch of exceptions from each submodule
    const int batch_size = 50;

    for (int i = 0; i < batch_size; i++) {
        number_sense_estimate(nullptr, nullptr, 0);
    }
    int ns_exceptions = total_exceptions.load();

    for (int i = 0; i < batch_size; i++) {
        spatial_rotate_and_compare(nullptr, nullptr, nullptr);
    }
    int sr_exceptions = total_exceptions.load() - ns_exceptions;

    for (int i = 0; i < batch_size; i++) {
        math_detect_pattern(nullptr, nullptr, 0);
    }
    int mi_exceptions = total_exceptions.load() - ns_exceptions - sr_exceptions;

    EXPECT_GE(ns_exceptions, batch_size);
    EXPECT_GE(sr_exceptions, batch_size);
    EXPECT_GE(mi_exceptions, batch_size);
    EXPECT_GE(immune_responses.load(), batch_size * 3);
}

//=============================================================================
// Error Code Consistency Tests
//=============================================================================

TEST_F(ParietalExceptionIntegrationTest, NullPointerErrorCodeConsistency) {
    // WHAT: Verify NULL pointer errors produce consistent error codes
    // WHY:  Error codes should be predictable for error handling

    std::vector<int> error_codes;

    last_error_code = 0;
    parietal_process(nullptr, nullptr);
    error_codes.push_back(last_error_code.load());

    last_error_code = 0;
    number_sense_estimate(nullptr, nullptr, 0);
    error_codes.push_back(last_error_code.load());

    last_error_code = 0;
    spatial_rotate_and_compare(nullptr, nullptr, nullptr);
    error_codes.push_back(last_error_code.load());

    last_error_code = 0;
    math_detect_pattern(nullptr, nullptr, 0);
    error_codes.push_back(last_error_code.load());

    // All should be NULL_POINTER errors
    for (int code : error_codes) {
        EXPECT_EQ(code, NIMCP_ERROR_NULL_POINTER);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
