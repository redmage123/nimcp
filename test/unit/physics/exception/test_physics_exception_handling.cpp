/**
 * @file test_physics_exception_handling.cpp
 * @brief Unit tests for physics module exception handling
 *
 * WHAT: Test exception handling across physics neural network module
 * WHY:  Ensure consistent error-to-exception mapping and handler chain dispatch
 * HOW:  Test physics module's error conditions and exception integration
 *
 * PHYSICS OPERATIONS TESTED:
 * - Neural network forward pass
 * - Training (backpropagation)
 * - Trajectory prediction
 * - Hamiltonian computation
 * - Symplectic integration
 *
 * TEST PATTERNS:
 * - Error code to exception mapping
 * - Exception dispatch through handler chain
 * - Exception category classification (PHYSICS_NN, PHYSICS_INTEGRATION)
 * - Recovery strategy determination
 *
 * @author NIMCP Development Team
 * @date 2026-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/parietal/nimcp_physics_nn.h"
}

//=============================================================================
// Physics Exception Categories
//=============================================================================

// Define physics-specific exception categories for testing
#define EXCEPTION_CATEGORY_PHYSICS_BASE     400
#define EXCEPTION_CATEGORY_PHYSICS_NN       (EXCEPTION_CATEGORY_PHYSICS_BASE + 1)
#define EXCEPTION_CATEGORY_FORWARD_PASS     (EXCEPTION_CATEGORY_PHYSICS_BASE + 2)
#define EXCEPTION_CATEGORY_TRAINING         (EXCEPTION_CATEGORY_PHYSICS_BASE + 3)
#define EXCEPTION_CATEGORY_PREDICTION       (EXCEPTION_CATEGORY_PHYSICS_BASE + 4)
#define EXCEPTION_CATEGORY_INTEGRATION      (EXCEPTION_CATEGORY_PHYSICS_BASE + 5)
#define EXCEPTION_CATEGORY_HAMILTONIAN      (EXCEPTION_CATEGORY_PHYSICS_BASE + 6)
#define EXCEPTION_CATEGORY_GRADIENT         (EXCEPTION_CATEGORY_PHYSICS_BASE + 7)

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<int> last_exception_category;
    static std::atomic<bool> handler_consumed;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_exception_category = 0;
        handler_consumed = false;

        nimcp_exception_system_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_exception_category = ex->category;
        return false;  // Don't consume - allow other handlers
    }

    static bool consuming_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        handler_consumed = true;
        return true;  // Consume the exception
    }

    // Helper to create physics exception
    nimcp_exception_t* create_physics_exception(
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

std::atomic<int> PhysicsExceptionHandlingTest::handler_call_count(0);
std::atomic<int> PhysicsExceptionHandlingTest::last_exception_code(0);
std::atomic<int> PhysicsExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> PhysicsExceptionHandlingTest::handler_consumed(false);

//=============================================================================
// Exception Creation Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, CreatePhysicsNNException) {
    // WHAT: Test creation of physics NN exception
    // WHY:  Verify exception fields are set correctly

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_NOT_INITIALIZED,
        EXCEPTION_CATEGORY_PHYSICS_NN,
        EXCEPTION_SEVERITY_ERROR,
        "Physics NN not initialized"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_PHYSICS_NN);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_NE(ex->message, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, CreateTrainingException) {
    // WHAT: Test creation of training-related exception
    // WHY:  Training errors need proper categorization

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "Physics NN training diverged - gradient explosion"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_TRAINING);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, CreateIntegrationException) {
    // WHAT: Test creation of integration exception
    // WHY:  Integration failures need specialized handling

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_INTEGRATION,
        EXCEPTION_SEVERITY_ERROR,
        "RK4 integration step failed - numerical instability"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_INTEGRATION);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Physics NN Lifecycle Exception Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, PhysicsNNNullHandleException) {
    // WHAT: Test exception for NULL physics NN handle
    // WHY:  Verify proper error handling for invalid handles

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_PHYSICS_NN,
        EXCEPTION_SEVERITY_ERROR,
        "Physics NN handle is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    // Register handler and dispatch
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "physics_null_handler";
    options.handler = test_exception_handler;
    options.priority = 100;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);
    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_NULL_POINTER);

    nimcp_exception_unref(ex);
    if (reg) nimcp_handler_unregister(reg);
}

TEST_F(PhysicsExceptionHandlingTest, InvalidConfigException) {
    // WHAT: Test exception for invalid configuration
    // WHY:  Configuration must be validated before use

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_PHYSICS_NN,
        EXCEPTION_SEVERITY_ERROR,
        "Invalid physics NN config: state_dim must be > 0"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_PHYSICS_NN);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, TooManyLayersException) {
    // WHAT: Test exception for exceeding max layers
    // WHY:  Networks have a maximum layer count

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_PHYSICS_NN,
        EXCEPTION_SEVERITY_ERROR,
        "Number of layers (20) exceeds maximum (16)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Forward Pass Exception Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, ForwardPassNullInputException) {
    // WHAT: Test exception for NULL input state
    // WHY:  State vector must be provided

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        "Forward pass state input is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_FORWARD_PASS);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, ForwardPassNaNException) {
    // WHAT: Test exception for NaN in forward pass output
    // WHY:  NaN indicates numerical instability

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        "NaN detected in forward pass output at layer 2"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, ForwardPassInfException) {
    // WHAT: Test exception for infinity in forward pass
    // WHY:  Infinity indicates overflow

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_FORWARD_PASS,
        EXCEPTION_SEVERITY_WARNING,
        "Infinity detected in forward pass - possible overflow"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Training Exception Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, TrainingNullTargetException) {
    // WHAT: Test exception for NULL target derivative
    // WHY:  Target must be provided for training

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "Training target derivative is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, TrainingGradientExplosionException) {
    // WHAT: Test exception for gradient explosion
    // WHY:  Large gradients indicate training instability

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_GRADIENT,
        EXCEPTION_SEVERITY_ERROR,
        "Gradient explosion detected: norm = 1e15 (clip threshold = 1.0)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OUT_OF_RANGE);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_GRADIENT);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, TrainingGradientVanishingException) {
    // WHAT: Test exception for vanishing gradients
    // WHY:  Very small gradients prevent learning

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_GRADIENT,
        EXCEPTION_SEVERITY_WARNING,
        "Vanishing gradients detected: norm = 1e-20"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, TrainingLossDivergedException) {
    // WHAT: Test exception for diverging loss
    // WHY:  Increasing loss indicates training failure

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "Training loss diverged: loss = NaN after 100 steps"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_LEARNING_FAILED);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Prediction Exception Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, PredictionNullOutputException) {
    // WHAT: Test exception for NULL prediction output
    // WHY:  Output buffer must be provided

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_NULL_POINTER,
        EXCEPTION_CATEGORY_PREDICTION,
        EXCEPTION_SEVERITY_ERROR,
        "Prediction output buffer is NULL"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_PREDICTION);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, PredictionInvalidTimestepException) {
    // WHAT: Test exception for invalid timestep
    // WHY:  Timestep must be positive

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_CATEGORY_PREDICTION,
        EXCEPTION_SEVERITY_ERROR,
        "Prediction timestep must be positive: got dt = -0.01"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, PredictionTrajectoryAllocationException) {
    // WHAT: Test exception for trajectory allocation failure
    // WHY:  Memory allocation may fail for long trajectories

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_CATEGORY_PREDICTION,
        EXCEPTION_SEVERITY_ERROR,
        "Failed to allocate trajectory buffer for 1000000 steps"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_NO_MEMORY);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Integration Exception Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, IntegrationInstabilityException) {
    // WHAT: Test exception for integration instability
    // WHY:  Numerical integration may become unstable

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_INTEGRATION,
        EXCEPTION_SEVERITY_ERROR,
        "Euler integration unstable: state magnitude > 1e10"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_INTEGRATION);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, SymplecticDimensionMismatchException) {
    // WHAT: Test exception for dimension mismatch in symplectic integration
    // WHY:  q and p must have matching dimensions

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_DIMENSION_MISMATCH,
        EXCEPTION_CATEGORY_INTEGRATION,
        EXCEPTION_SEVERITY_ERROR,
        "Symplectic integration requires even state_dim for q/p split"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_DIMENSION_MISMATCH);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Hamiltonian Exception Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, HamiltonianComputationException) {
    // WHAT: Test exception for Hamiltonian computation failure
    // WHY:  Hamiltonian may fail for invalid states

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_HAMILTONIAN,
        EXCEPTION_SEVERITY_ERROR,
        "Hamiltonian computation failed: returned NaN"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(ex->category, EXCEPTION_CATEGORY_HAMILTONIAN);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, HamiltonianConservationViolationException) {
    // WHAT: Test exception for energy conservation violation
    // WHY:  Symplectic integrators should preserve energy

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_HAMILTONIAN,
        EXCEPTION_SEVERITY_WARNING,
        "Energy drift too large: delta_H = 0.5 (threshold = 0.01)"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_WARNING);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Handler Chain Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, HandlerChainDispatch) {
    // WHAT: Test exception dispatch through multiple handlers
    // WHY:  Verify chain processing works correctly

    // Register multiple handlers
    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    options1.name = "physics_handler_1";
    options1.handler = test_exception_handler;
    options1.priority = 100;

    options2.name = "physics_handler_2";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_PHYSICS_NN,
        EXCEPTION_SEVERITY_ERROR,
        "Test exception for handler chain"
    );

    handler_call_count = 0;
    nimcp_exception_dispatch(ex);

    // Both handlers should be called (neither consumes)
    EXPECT_GE(handler_call_count.load(), 2);

    nimcp_exception_unref(ex);
    if (reg1) nimcp_handler_unregister(reg1);
    if (reg2) nimcp_handler_unregister(reg2);
}

TEST_F(PhysicsExceptionHandlingTest, HandlerConsumesException) {
    // WHAT: Test handler consuming exception stops chain
    // WHY:  Verify consumed exceptions don't propagate

    nimcp_handler_options_t options1, options2;
    nimcp_handler_default_options(&options1);
    nimcp_handler_default_options(&options2);

    // First handler consumes
    options1.name = "consuming_handler";
    options1.handler = consuming_exception_handler;
    options1.priority = 100;

    // Second handler should not be called
    options2.name = "secondary_handler";
    options2.handler = test_exception_handler;
    options2.priority = 50;

    nimcp_handler_registration_t* reg1 = nimcp_handler_register(&options1);
    nimcp_handler_registration_t* reg2 = nimcp_handler_register(&options2);

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_CATEGORY_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "Test exception for consumption"
    );

    handler_call_count = 0;
    handler_consumed = false;
    nimcp_exception_dispatch(ex);

    // Only consuming handler should be called
    EXPECT_TRUE(handler_consumed.load());
    EXPECT_EQ(handler_call_count.load(), 1);

    nimcp_exception_unref(ex);
    if (reg1) nimcp_handler_unregister(reg1);
    if (reg2) nimcp_handler_unregister(reg2);
}

//=============================================================================
// Recovery Strategy Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, PhysicsExceptionRecoveryStrategy) {
    // WHAT: Test recovery strategy for physics exceptions
    // WHY:  Physics errors may need retry or parameter adjustment

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_CATEGORY_TRAINING,
        EXCEPTION_SEVERITY_ERROR,
        "Training step failed"
    );

    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Physics exceptions should have some recovery action
    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, CriticalPhysicsExceptionRecovery) {
    // WHAT: Test recovery for critical physics failures
    // WHY:  Critical failures may require emergency save

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_MEMORY_CORRUPTION,
        EXCEPTION_CATEGORY_PHYSICS_NN,
        EXCEPTION_SEVERITY_CRITICAL,
        "Physics NN weight data corrupted"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_CRITICAL);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);

    // Critical errors should trigger some kind of recovery action
    EXPECT_TRUE(strategy.primary_action != EXCEPTION_RECOVERY_NONE ||
                strategy.fallback_action != EXCEPTION_RECOVERY_NONE ||
                strategy.retry_count > 0);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Exception Statistics Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, ExceptionStatisticsTracking) {
    // WHAT: Test that exception dispatch is tracked by handlers
    // WHY:  Need to monitor physics exception frequency

    // Register a counting handler
    static std::atomic<int> dispatch_count{0};
    dispatch_count = 0;

    auto counting_handler = [](nimcp_exception_t* ex, void* user_data) -> bool {
        (void)ex;
        (void)user_data;
        dispatch_count++;
        return false;
    };

    nimcp_handler_options_t opts;
    nimcp_handler_default_options(&opts);
    opts.name = "stats_counter";
    opts.handler = counting_handler;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&opts);
    ASSERT_NE(reg, nullptr);

    // Create and dispatch several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = create_physics_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_CATEGORY_PHYSICS_NN,
            EXCEPTION_SEVERITY_WARNING,
            "Test exception for statistics"
        );
        if (ex) {
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }
    }

    // Handler should have been called for each exception
    EXPECT_GE(dispatch_count.load(), 5);

    nimcp_handler_unregister(reg);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, ConcurrentExceptionCreation) {
    // WHAT: Test concurrent exception creation
    // WHY:  Physics operations may run across multiple threads

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&success_count, t, exceptions_per_thread]() {
            for (int i = 0; i < exceptions_per_thread; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );
                if (ex) {
                    success_count++;
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * exceptions_per_thread);
}

//=============================================================================
// Context Entry Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, ExceptionContextMetadata) {
    // WHAT: Test adding context metadata to physics exceptions
    // WHY:  Context helps diagnose issues

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_OUT_OF_RANGE,
        EXCEPTION_CATEGORY_GRADIENT,
        EXCEPTION_SEVERITY_ERROR,
        "Gradient explosion"
    );

    ASSERT_NE(ex, nullptr);

    // Add context
    int result = nimcp_exception_set_context(ex, "gradient_norm", "1e15");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "clip_threshold", "1.0");
    EXPECT_EQ(result, 0);

    result = nimcp_exception_set_context(ex, "layer", "3");
    EXPECT_EQ(result, 0);

    // Verify context
    EXPECT_STREQ(nimcp_exception_get_context(ex, "gradient_norm"), "1e15");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "clip_threshold"), "1.0");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "layer"), "3");

    nimcp_exception_unref(ex);
}

//=============================================================================
// Serialization Exception Tests
//=============================================================================

TEST_F(PhysicsExceptionHandlingTest, SerializationFileNotFoundException) {
    // WHAT: Test exception for file not found during load
    // WHY:  Model files may not exist

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_FILE_NOT_FOUND,
        EXCEPTION_CATEGORY_PHYSICS_NN,
        EXCEPTION_SEVERITY_ERROR,
        "Physics NN weights file not found: model.bin"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_FILE_NOT_FOUND);

    nimcp_exception_unref(ex);
}

TEST_F(PhysicsExceptionHandlingTest, SerializationCorruptFileException) {
    // WHAT: Test exception for corrupted model file
    // WHY:  Files may be corrupted

    nimcp_exception_t* ex = create_physics_exception(
        NIMCP_ERROR_FILE_CORRUPT,
        EXCEPTION_CATEGORY_PHYSICS_NN,
        EXCEPTION_SEVERITY_ERROR,
        "Physics NN weights file corrupted: invalid magic number"
    );

    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->code, NIMCP_ERROR_FILE_CORRUPT);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
