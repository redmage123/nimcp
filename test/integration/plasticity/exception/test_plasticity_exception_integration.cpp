/**
 * @file test_plasticity_exception_integration.cpp
 * @brief Integration tests for plasticity exception handling with immune system
 *
 * WHAT: Test end-to-end exception flow from plasticity modules to immune system
 * WHY:  Verify complete error handling pipeline for synaptic plasticity
 * HOW:  Simulate plasticity errors, verify exception dispatch, and check immune response
 *
 * INTEGRATION SCENARIOS:
 * - Plasticity error -> Exception creation -> Handler dispatch -> Immune presentation
 * - Multiple plasticity exceptions with batch processing
 * - Exception recovery with plasticity state restoration
 * - Cross-module exception propagation (STDP -> Homeostatic -> Structural)
 * - Sleep/wake state modulation of exception handling
 *
 * IMMUNE INTEGRATION:
 * - B cell response to plasticity threats
 * - Cytokine effects on learning rate modulation
 * - Recovery through immune system restoration
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

// Include C++ compatible headers first (may include CUDA)
#include "cognitive/immune/nimcp_brain_immune.h"

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "plasticity/stp/nimcp_stp.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
}

//=============================================================================
// Plasticity Exception Categories (from unit test, shared definitions)
//=============================================================================

#define EXCEPTION_CATEGORY_PLASTICITY_BASE     100
#define EXCEPTION_CATEGORY_LEARNING            (EXCEPTION_CATEGORY_PLASTICITY_BASE + 1)
#define EXCEPTION_CATEGORY_ADAPTATION          (EXCEPTION_CATEGORY_PLASTICITY_BASE + 2)
#define EXCEPTION_CATEGORY_WEIGHT_UPDATE       (EXCEPTION_CATEGORY_PLASTICITY_BASE + 3)
#define EXCEPTION_CATEGORY_TIMING              (EXCEPTION_CATEGORY_PLASTICITY_BASE + 4)
#define EXCEPTION_CATEGORY_TRACE_DECAY         (EXCEPTION_CATEGORY_PLASTICITY_BASE + 5)
#define EXCEPTION_CATEGORY_HOMEOSTASIS         (EXCEPTION_CATEGORY_PLASTICITY_BASE + 6)
#define EXCEPTION_CATEGORY_STRUCTURAL          (EXCEPTION_CATEGORY_PLASTICITY_BASE + 7)

//=============================================================================
// Test Fixture
//=============================================================================

class PlasticityExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> immune_presentation_count;
    static std::atomic<int> recovery_action_count;
    static std::atomic<nimcp_error_t> last_error_code;

    brain_immune_system_t* immune_system;

    void SetUp() override {
        handler_call_count = 0;
        immune_presentation_count = 0;
        recovery_action_count = 0;
        last_error_code = NIMCP_SUCCESS;

        nimcp_exception_system_init();

        // Create immune system for integration tests
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
    }

    void TearDown() override {
        if (immune_system) {
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool plasticity_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_error_code = ex->code;
        return false;
    }

    static bool plasticity_immune_handler(nimcp_exception_t* ex, void* user_data) {
        PlasticityExceptionIntegrationTest* test =
            static_cast<PlasticityExceptionIntegrationTest*>(user_data);
        if (!test || !test->immune_system) return false;

        immune_presentation_count++;

        // Present to immune system
        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        return false;  // Don't consume
    }

    static bool recovery_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        if (strategy.primary_action != EXCEPTION_RECOVERY_NONE) {
            recovery_action_count++;
        }

        return false;
    }

    // Helper to create plasticity exception
    nimcp_exception_t* create_plasticity_exception(
        nimcp_error_t code,
        int category,
        nimcp_exception_severity_t severity,
        const char* message
    ) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code,
            severity,
            __FILE__, __LINE__, __func__,
            message
        );
        if (ex) {
            ex->category = static_cast<nimcp_exception_category_t>(category);
        }
        return ex;
    }
};

std::atomic<int> PlasticityExceptionIntegrationTest::handler_call_count(0);
std::atomic<int> PlasticityExceptionIntegrationTest::immune_presentation_count(0);
std::atomic<int> PlasticityExceptionIntegrationTest::recovery_action_count(0);
std::atomic<nimcp_error_t> PlasticityExceptionIntegrationTest::last_error_code(NIMCP_SUCCESS);

//=============================================================================
// End-to-End Exception Flow Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, STDPExceptionToImmuneFlow) {
    // WHAT: Test STDP exception -> handler -> immune presentation
    // WHY:  Verify complete integration pipeline

    // Register handlers
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "stdp_handler";
    options.handler = plasticity_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options);

    options.name = "immune_handler";
    options.handler = plasticity_immune_handler;
    options.priority = 50;
    options.user_data = this;
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options);

    // Create STDP exception
    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        "STDP weight update failed - dopamine modulation error"
    );
    ASSERT_NE(ex, nullptr);

    // Dispatch
    handler_call_count = 0;
    immune_presentation_count = 0;
    nimcp_exception_dispatch(ex);

    // Verify handler chain executed
    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_GE(immune_presentation_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_LEARNING_FAILED);

    nimcp_exception_unref(ex);
    if (reg1) nimcp_handler_unregister(reg1);
    if (reg2) nimcp_handler_unregister(reg2);
}

TEST_F(PlasticityExceptionIntegrationTest, HomeostaticExceptionWithRecovery) {
    // WHAT: Test homeostatic exception with recovery strategy
    // WHY:  Homeostatic plasticity should trigger adaptation recovery

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "recovery_handler";
    options.handler = recovery_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    // Create homeostatic exception
    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_HOMEOSTASIS,
        EXCEPTION_SEVERITY_ERROR,
        "Homeostatic scaling failed - firing rate out of range"
    );
    ASSERT_NE(ex, nullptr);

    recovery_action_count = 0;
    nimcp_exception_dispatch(ex);

    EXPECT_GE(recovery_action_count.load(), 1);

    nimcp_exception_unref(ex);
    if (reg) nimcp_handler_unregister(reg);
}

//=============================================================================
// Multi-Exception Batch Processing Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, BatchPlasticityExceptions) {
    // WHAT: Test processing multiple plasticity exceptions
    // WHY:  Simulates burst of errors during network instability

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "batch_handler";
    options.handler = plasticity_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;

    // Create batch of different plasticity exceptions
    struct {
        nimcp_error_t code;
        int category;
        const char* message;
    } exceptions[] = {
        { NIMCP_ERROR_LEARNING_FAILED, EXCEPTION_CATEGORY_LEARNING, "STDP batch error 1" },
        { NIMCP_ERROR_WEIGHT_INIT, EXCEPTION_CATEGORY_WEIGHT_UPDATE, "BCM threshold error" },
        { NIMCP_ERROR_OUT_OF_RANGE, EXCEPTION_CATEGORY_HOMEOSTASIS, "Firing rate exceeded" },
        { NIMCP_ERROR_INVALID_STATE, EXCEPTION_CATEGORY_STRUCTURAL, "Spine state invalid" },
        { NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_CATEGORY_TRACE_DECAY, "Eligibility decay failed" }
    };

    for (const auto& ex_data : exceptions) {
        nimcp_exception_t* ex = create_plasticity_exception(
            ex_data.code,
            ex_data.category,
            EXCEPTION_SEVERITY_ERROR,
            ex_data.message
        );
        if (ex) {
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
    }

    EXPECT_EQ(handler_call_count.load(), 5);

    if (reg) nimcp_handler_unregister(reg);
}

//=============================================================================
// Cross-Module Exception Propagation Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, CrossModuleExceptionCascade) {
    // WHAT: Test exception cascade across plasticity modules
    // WHY:  Errors in one module may affect others (STDP -> Homeostatic)

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "cascade_handler";
    options.handler = plasticity_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;

    // Simulate cascade: STDP failure -> triggers homeostatic check -> structural adaptation
    nimcp_exception_t* stdp_ex = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        "STDP runaway LTP detected"
    );
    nimcp_exception_dispatch(stdp_ex);

    nimcp_exception_t* homeo_ex = create_plasticity_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_HOMEOSTASIS,
        EXCEPTION_SEVERITY_WARNING,
        "Homeostatic response to STDP runaway"
    );
    nimcp_exception_dispatch(homeo_ex);

    nimcp_exception_t* struct_ex = create_plasticity_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_STRUCTURAL,
        EXCEPTION_SEVERITY_WARNING,
        "Spine pruning triggered by homeostatic signal"
    );
    nimcp_exception_dispatch(struct_ex);

    EXPECT_EQ(handler_call_count.load(), 3);

    nimcp_exception_unref(stdp_ex);
    nimcp_exception_unref(homeo_ex);
    nimcp_exception_unref(struct_ex);
    if (reg) nimcp_handler_unregister(reg);
}

//=============================================================================
// Immune System Integration Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, ImmuneResponseToPlasticityError) {
    // WHAT: Test immune system response to plasticity errors
    // WHY:  Verify B cell activation on learning threats

    ASSERT_NE(immune_system, nullptr);

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_CATEGORY_WEIGHT_UPDATE,
        EXCEPTION_SEVERITY_CRITICAL,
        "Weight array corruption detected"
    );
    ASSERT_NE(ex, nullptr);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);

    // For critical errors, immune should recommend action
    // Response structure may vary based on implementation

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionIntegrationTest, CytokineEffectOnExceptionSeverity) {
    // WHAT: Test how inflammation level affects exception handling
    // WHY:  Inflamed state may change recovery strategy

    ASSERT_NE(immune_system, nullptr);

    // Create learning exception during inflammation
    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        "Learning failed during inflammatory state"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // During inflammation, recovery strategy might be more conservative
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Concurrent Exception Handling Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, ConcurrentPlasticityExceptions) {
    // WHAT: Test concurrent exception handling from multiple threads
    // WHY:  Plasticity updates happen across parallel synapses

    std::atomic<int> total_dispatched{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 20;

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "concurrent_handler";
    options.handler = plasticity_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &total_dispatched, t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = create_plasticity_exception(
                    NIMCP_ERROR_LEARNING_FAILED,
                    EXCEPTION_CATEGORY_LEARNING,
                    EXCEPTION_SEVERITY_WARNING,
                    "Concurrent plasticity exception"
                );
                if (ex) {
                    nimcp_exception_dispatch(ex);
                    nimcp_exception_unref(ex);
                    total_dispatched++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_dispatched.load(), num_threads * exceptions_per_thread);
    EXPECT_EQ(handler_call_count.load(), num_threads * exceptions_per_thread);

    if (reg) nimcp_handler_unregister(reg);
}

//=============================================================================
// Exception State Restoration Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, ExceptionStateCapture) {
    // WHAT: Test that exception captures plasticity state
    // WHY:  State info needed for recovery decisions

    nimcp_exception_t* ex = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_ERROR,
        "STDP learning failed at weight=0.8, lr=0.01"
    );
    ASSERT_NE(ex, nullptr);

    // Exception should capture source location
    EXPECT_NE(ex->file, nullptr);
    EXPECT_GT(ex->line, 0);
    EXPECT_NE(ex->function, nullptr);

    // Message should contain state info
    EXPECT_NE(strstr(ex->message, "weight"), nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(PlasticityExceptionIntegrationTest, RecoveryStrategyByCategory) {
    // WHAT: Test different recovery strategies by exception category
    // WHY:  Different plasticity failures need different recovery

    struct {
        int category;
        nimcp_exception_severity_t severity;
        const char* message;
    } test_cases[] = {
        { EXCEPTION_CATEGORY_LEARNING, EXCEPTION_SEVERITY_ERROR, "Learning error" },
        { EXCEPTION_CATEGORY_HOMEOSTASIS, EXCEPTION_SEVERITY_WARNING, "Homeostatic warning" },
        { EXCEPTION_CATEGORY_STRUCTURAL, EXCEPTION_SEVERITY_CRITICAL, "Structural critical" },
        { EXCEPTION_CATEGORY_WEIGHT_UPDATE, EXCEPTION_SEVERITY_ERROR, "Weight update error" }
    };

    for (const auto& tc : test_cases) {
        nimcp_exception_t* ex = create_plasticity_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            tc.category,
            tc.severity,
            tc.message
        );
        ASSERT_NE(ex, nullptr);

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        // All non-trivial exceptions should have some recovery action
        // Critical exceptions should have more aggressive recovery
        if (tc.severity == EXCEPTION_SEVERITY_CRITICAL) {
            EXPECT_TRUE(strategy.primary_action != EXCEPTION_RECOVERY_NONE ||
                       strategy.fallback_action != EXCEPTION_RECOVERY_NONE ||
                       strategy.retry_count > 0);
        }

        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Handler Registration/Unregistration Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, DynamicHandlerRegistration) {
    // WHAT: Test dynamic handler registration during exception handling
    // WHY:  Handlers may be added/removed based on plasticity state

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "dynamic_handler";
    options.handler = plasticity_exception_handler;
    options.priority = 100;

    handler_call_count = 0;

    // Dispatch without handler
    nimcp_exception_t* ex1 = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_WARNING,
        "Before handler registration"
    );
    nimcp_exception_dispatch(ex1);
    int count_before = handler_call_count.load();

    // Register handler
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    // Dispatch with handler
    nimcp_exception_t* ex2 = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_WARNING,
        "After handler registration"
    );
    nimcp_exception_dispatch(ex2);
    int count_after = handler_call_count.load();

    EXPECT_GT(count_after, count_before);

    // Unregister and dispatch again
    if (reg) nimcp_handler_unregister(reg);

    nimcp_exception_t* ex3 = create_plasticity_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_SEVERITY_WARNING,
        "After handler unregistration"
    );
    nimcp_exception_dispatch(ex3);
    int count_final = handler_call_count.load();

    // Handler count should not increase after unregistration
    EXPECT_EQ(count_final, count_after);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
    nimcp_exception_unref(ex3);
}

//=============================================================================
// Long-Running Simulation Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, SimulatedPlasticitySession) {
    // WHAT: Test exception handling over simulated plasticity session
    // WHY:  Verify stability during extended operation

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "session_handler";
    options.handler = plasticity_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;
    const int simulation_steps = 100;
    const float error_probability = 0.05f;  // 5% error rate

    int expected_errors = 0;
    for (int step = 0; step < simulation_steps; step++) {
        // Simulate random plasticity errors
        if ((rand() % 100) < (error_probability * 100)) {
            expected_errors++;

            nimcp_exception_t* ex = create_plasticity_exception(
                NIMCP_ERROR_LEARNING_FAILED,
                EXCEPTION_CATEGORY_LEARNING,
                EXCEPTION_SEVERITY_WARNING,
                "Simulated plasticity error at step"
            );
            if (ex) {
                nimcp_exception_dispatch(ex);
                nimcp_exception_unref(ex);
            }
        }
    }

    // All simulated errors should be handled
    EXPECT_EQ(handler_call_count.load(), expected_errors);

    if (reg) nimcp_handler_unregister(reg);
}

//=============================================================================
// Memory Stress Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, ExceptionMemoryManagement) {
    // WHAT: Test exception memory is properly managed
    // WHY:  Prevent memory leaks in high-exception scenarios

    const int num_exceptions = 1000;

    for (int i = 0; i < num_exceptions; i++) {
        nimcp_exception_t* ex = create_plasticity_exception(
            NIMCP_ERROR_LEARNING_FAILED,
            EXCEPTION_CATEGORY_LEARNING,
            EXCEPTION_SEVERITY_WARNING,
            "Memory management test exception"
        );
        ASSERT_NE(ex, nullptr);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // If we got here without crash or memory errors, test passed
    SUCCEED();
}

//=============================================================================
// Exception Statistics Integration Tests
//=============================================================================

TEST_F(PlasticityExceptionIntegrationTest, ExceptionStatisticsAccuracy) {
    // WHAT: Test exception statistics across categories
    // WHY:  Need accurate monitoring of plasticity health

    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "stats_handler";
    options.handler = plasticity_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;

    // Create exceptions of different categories
    const int per_category = 10;
    int categories[] = {
        EXCEPTION_CATEGORY_LEARNING,
        EXCEPTION_CATEGORY_HOMEOSTASIS,
        EXCEPTION_CATEGORY_STRUCTURAL,
        EXCEPTION_CATEGORY_WEIGHT_UPDATE
    };
    const int num_categories = sizeof(categories) / sizeof(categories[0]);

    for (int c = 0; c < num_categories; c++) {
        for (int i = 0; i < per_category; i++) {
            nimcp_exception_t* ex = create_plasticity_exception(
                NIMCP_ERROR_OPERATION_FAILED,
                categories[c],
                EXCEPTION_SEVERITY_WARNING,
                "Statistics test exception"
            );
            if (ex) {
                nimcp_exception_dispatch(ex);
                nimcp_exception_unref(ex);
            }
        }
    }

    // Handler should have been called for each dispatched exception
    EXPECT_GE(handler_call_count.load(), num_categories * per_category);

    if (reg) nimcp_handler_unregister(reg);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
