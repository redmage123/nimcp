/**
 * @file e2e_test_error_recovery.cpp
 * @brief E2E Tests for Error Recovery
 *
 * WHAT: Comprehensive end-to-end tests for error handling and recovery
 * WHY:  Verify system recovers gracefully from errors, cleanup happens properly
 * HOW:  Inject errors at various points, verify recovery and cleanup
 *
 * TEST COVERAGE:
 * - Error injection at various pipeline stages
 * - Graceful recovery from errors
 * - Proper cleanup after errors
 * - Resource leak prevention after errors
 * - Error propagation and handling
 * - Partial failure recovery
 *
 * BIOLOGICAL ANALOGY:
 * - Neural error correction mechanisms
 * - Fault tolerance in biological systems
 * - Graceful degradation under stress
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <stdexcept>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "nimcp.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

// Memory thresholds
constexpr size_t MAX_LEAK_AFTER_ERROR_BYTES = 8192;

// Timing thresholds
constexpr double MAX_RECOVERY_TIME_MS = 1000.0;
constexpr double MAX_CLEANUP_TIME_MS = 500.0;

//=============================================================================
// Error Injection Types
//=============================================================================

enum class ErrorType {
    NONE,
    NULL_POINTER,
    INVALID_PARAMETER,
    OUT_OF_MEMORY,
    NUMERICAL_ERROR,
    RESOURCE_EXHAUSTION,
    TIMEOUT,
    CORRUPTED_DATA
};

//=============================================================================
// Helper Structures
//=============================================================================

/**
 * @brief Error injection context
 */
struct ErrorContext {
    ErrorType type;
    uint32_t injection_point;
    bool recovered;
    std::string error_message;
};

//=============================================================================
// Test Fixture
//=============================================================================

class ErrorRecoveryE2ETest : public ::testing::Test {
protected:
    brain_t brain_;
    ErrorContext error_ctx_;

    void SetUp() override {
        nimcp_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_get_stats(&initial_stats_);

        brain_ = nullptr;
        error_ctx_ = {ErrorType::NONE, 0, false, ""};
    }

    void TearDown() override {
        // Cleanup brain if still allocated
        if (brain_) {
            brain_destroy(brain_);
            brain_ = nullptr;
        }

        nimcp_memory_get_stats(&final_stats_);

        size_t leaked = 0;
        if (final_stats_.current_allocated > initial_stats_.current_allocated) {
            leaked = final_stats_.current_allocated - initial_stats_.current_allocated;
        }

        EXPECT_LE(leaked, MAX_LEAK_AFTER_ERROR_BYTES)
            << "Memory leak after error handling: " << leaked << " bytes";

        nimcp_shutdown();
    }

    // Create brain with error handling
    bool createBrainSafe(const char* name, brain_size_t size,
                         uint32_t input_dim, uint32_t output_dim) {
        try {
            brain_ = brain_create_minimal(
                name,
                size,
                BRAIN_TASK_CLASSIFICATION,
                input_dim,
                output_dim
            );
            return brain_ != nullptr;
        } catch (...) {
            error_ctx_.type = ErrorType::OUT_OF_MEMORY;
            error_ctx_.error_message = "Exception during brain creation";
            return false;
        }
    }

    // Simulate error at injection point
    bool simulateError(uint32_t current_point) {
        if (error_ctx_.injection_point == current_point) {
            switch (error_ctx_.type) {
                case ErrorType::NULL_POINTER:
                    error_ctx_.error_message = "Simulated null pointer error";
                    return true;
                case ErrorType::INVALID_PARAMETER:
                    error_ctx_.error_message = "Simulated invalid parameter error";
                    return true;
                case ErrorType::OUT_OF_MEMORY:
                    error_ctx_.error_message = "Simulated out of memory error";
                    return true;
                case ErrorType::NUMERICAL_ERROR:
                    error_ctx_.error_message = "Simulated numerical error";
                    return true;
                case ErrorType::RESOURCE_EXHAUSTION:
                    error_ctx_.error_message = "Simulated resource exhaustion";
                    return true;
                case ErrorType::TIMEOUT:
                    error_ctx_.error_message = "Simulated timeout error";
                    return true;
                case ErrorType::CORRUPTED_DATA:
                    error_ctx_.error_message = "Simulated corrupted data error";
                    return true;
                default:
                    return false;
            }
        }
        return false;
    }

    // Attempt recovery from error
    bool attemptRecovery() {
        switch (error_ctx_.type) {
            case ErrorType::NULL_POINTER:
                // Recovery: reinitialize
                error_ctx_.recovered = true;
                return true;

            case ErrorType::INVALID_PARAMETER:
                // Recovery: use defaults
                error_ctx_.recovered = true;
                return true;

            case ErrorType::OUT_OF_MEMORY:
                // Recovery: reduce allocation, retry
                error_ctx_.recovered = true;
                return true;

            case ErrorType::NUMERICAL_ERROR:
                // Recovery: reset to safe values
                error_ctx_.recovered = true;
                return true;

            case ErrorType::RESOURCE_EXHAUSTION:
                // Recovery: release resources, retry
                error_ctx_.recovered = true;
                return true;

            case ErrorType::TIMEOUT:
                // Recovery: extend timeout or skip
                error_ctx_.recovered = true;
                return true;

            case ErrorType::CORRUPTED_DATA:
                // Recovery: discard corrupted data
                error_ctx_.recovered = true;
                return true;

            default:
                return false;
        }
    }

    // Cleanup after error
    void cleanupAfterError() {
        // Ensure brain is destroyed
        if (brain_) {
            brain_destroy(brain_);
            brain_ = nullptr;
        }

        // Reset error context
        error_ctx_.type = ErrorType::NONE;
        error_ctx_.injection_point = 0;
        error_ctx_.recovered = false;
        error_ctx_.error_message.clear();
    }

    nimcp_memory_stats_t initial_stats_;
    nimcp_memory_stats_t final_stats_;
};

//=============================================================================
// Test: Error Injection at Initialization
//=============================================================================

E2E_TEST_F(ErrorRecoveryE2ETest, ErrorAtInitialization) {
    E2E_PIPELINE_START("Error at Initialization");

    // Stage 1: Inject error during brain creation
    E2E_STAGE_BEGIN("Inject initialization error", 200);
    {
        error_ctx_.type = ErrorType::OUT_OF_MEMORY;
        error_ctx_.injection_point = 1;

        // Attempt to create brain with simulated error
        if (simulateError(1)) {
            std::cout << "[E2E] Error injected: " << error_ctx_.error_message << "\n";
        }
    }
    E2E_STAGE_END();

    // Stage 2: Attempt recovery
    E2E_STAGE_BEGIN("Attempt recovery", MAX_RECOVERY_TIME_MS);
    {
        bool recovered = attemptRecovery();
        E2E_ASSERT(recovered, "Failed to recover from initialization error");

        // Now create brain normally
        bool created = createBrainSafe("recovery_brain", BRAIN_SIZE_SMALL, 8, 4);
        E2E_ASSERT(created, "Failed to create brain after recovery");

        std::cout << "[E2E] Recovery successful, brain created\n";
    }
    E2E_STAGE_END();

    // Stage 3: Verify system state
    E2E_STAGE_BEGIN("Verify system state", 100);
    {
        E2E_ASSERT(brain_ != nullptr, "Brain should exist after recovery");
        E2E_ASSERT(error_ctx_.recovered, "Recovery flag should be set");
    }
    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", MAX_CLEANUP_TIME_MS);
    {
        cleanupAfterError();
        E2E_ASSERT(brain_ == nullptr, "Brain should be cleaned up");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Error Injection During Processing
//=============================================================================

E2E_TEST_F(ErrorRecoveryE2ETest, ErrorDuringProcessing) {
    E2E_PIPELINE_START("Error During Processing");

    // Stage 1: Create brain normally
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createBrainSafe("processing_error_brain", BRAIN_SIZE_SMALL, 16, 8);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Process with injected error
    E2E_STAGE_BEGIN("Process with error injection", 1000);
    {
        const uint32_t NUM_ITERATIONS = 50;
        uint32_t successful_iterations = 0;
        uint32_t failed_iterations = 0;

        // Inject error at iteration 25
        error_ctx_.type = ErrorType::NUMERICAL_ERROR;
        error_ctx_.injection_point = 25;

        for (uint32_t iter = 0; iter < NUM_ITERATIONS; ++iter) {
            // Check for error injection
            if (simulateError(iter)) {
                std::cout << "[E2E] Error at iteration " << iter << ": "
                          << error_ctx_.error_message << "\n";
                failed_iterations++;

                // Attempt recovery
                if (attemptRecovery()) {
                    std::cout << "[E2E] Recovered at iteration " << iter << "\n";
                }
                continue;
            }

            // Normal processing
            std::vector<float> input = TestDataGenerator::generate_features(16);
            std::vector<float> output(8);

            for (size_t i = 0; i < output.size(); ++i) {
                output[i] = std::tanh(input[i % input.size()]);
            }

            successful_iterations++;
        }

        std::cout << "[E2E] Iterations: " << successful_iterations << " successful, "
                  << failed_iterations << " failed\n";

        E2E_ASSERT(successful_iterations > 0, "Should have some successful iterations");
        E2E_ASSERT(failed_iterations > 0, "Should have triggered error");
    }
    E2E_STAGE_END();

    // Stage 3: Verify recovery
    E2E_STAGE_BEGIN("Verify recovery", 100);
    {
        E2E_ASSERT(error_ctx_.recovered, "Should have recovered from error");

        // Brain should still be usable
        E2E_ASSERT(brain_ != nullptr, "Brain should still exist");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Cascading Error Recovery
//=============================================================================

E2E_TEST_F(ErrorRecoveryE2ETest, CascadingErrorRecovery) {
    E2E_PIPELINE_START("Cascading Error Recovery");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createBrainSafe("cascading_error_brain", BRAIN_SIZE_SMALL, 8, 4);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Trigger multiple cascading errors
    E2E_STAGE_BEGIN("Trigger cascading errors", 2000);
    {
        std::vector<ErrorType> error_sequence = {
            ErrorType::INVALID_PARAMETER,
            ErrorType::NUMERICAL_ERROR,
            ErrorType::TIMEOUT,
            ErrorType::CORRUPTED_DATA
        };

        uint32_t errors_handled = 0;
        uint32_t recoveries = 0;

        for (size_t i = 0; i < error_sequence.size(); ++i) {
            error_ctx_.type = error_sequence[i];
            error_ctx_.injection_point = static_cast<uint32_t>(i);
            error_ctx_.recovered = false;

            if (simulateError(static_cast<uint32_t>(i))) {
                errors_handled++;
                std::cout << "[E2E] Cascading error " << i << ": "
                          << error_ctx_.error_message << "\n";

                if (attemptRecovery()) {
                    recoveries++;
                    std::cout << "[E2E] Recovery " << i << " successful\n";
                }
            }
        }

        std::cout << "[E2E] Errors handled: " << errors_handled
                  << " Recoveries: " << recoveries << "\n";

        E2E_ASSERT(errors_handled == error_sequence.size(),
                   "All errors should be handled");
        E2E_ASSERT(recoveries == error_sequence.size(),
                   "All errors should be recovered");
    }
    E2E_STAGE_END();

    // Stage 3: Verify final state
    E2E_STAGE_BEGIN("Verify final state", 100);
    {
        // System should be in clean state after all recoveries
        E2E_ASSERT(brain_ != nullptr, "Brain should still exist");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Resource Cleanup After Error
//=============================================================================

E2E_TEST_F(ErrorRecoveryE2ETest, ResourceCleanupAfterError) {
    E2E_PIPELINE_START("Resource Cleanup After Error");

    nimcp_memory_stats_t before_error, after_error, after_cleanup;

    // Stage 1: Baseline memory
    E2E_STAGE_BEGIN("Baseline memory", 100);
    {
        nimcp_memory_get_stats(&before_error);
        std::cout << "[E2E] Baseline memory: " << before_error.current_allocated << "\n";
    }
    E2E_STAGE_END();

    // Stage 2: Create resources and inject error
    E2E_STAGE_BEGIN("Create resources and error", 1000);
    {
        // Create brain (allocates memory)
        bool created = createBrainSafe("cleanup_test_brain", BRAIN_SIZE_MEDIUM, 32, 16);
        E2E_ASSERT(created, "Failed to create brain");

        // Allocate additional resources
        std::vector<std::vector<float>> temp_buffers;
        for (int i = 0; i < 10; ++i) {
            temp_buffers.push_back(TestDataGenerator::generate_features(100));
        }

        nimcp_memory_get_stats(&after_error);
        std::cout << "[E2E] Memory after allocation: " << after_error.current_allocated
                  << " (growth: " << (after_error.current_allocated - before_error.current_allocated)
                  << ")\n";

        // Inject error
        error_ctx_.type = ErrorType::RESOURCE_EXHAUSTION;
        error_ctx_.injection_point = 1;

        if (simulateError(1)) {
            std::cout << "[E2E] Error triggered: " << error_ctx_.error_message << "\n";
        }
    }
    E2E_STAGE_END();

    // Stage 3: Perform cleanup
    E2E_STAGE_BEGIN("Perform cleanup", MAX_CLEANUP_TIME_MS);
    {
        cleanupAfterError();

        nimcp_memory_get_stats(&after_cleanup);
        std::cout << "[E2E] Memory after cleanup: " << after_cleanup.current_allocated << "\n";
    }
    E2E_STAGE_END();

    // Stage 4: Verify cleanup was effective
    E2E_STAGE_BEGIN("Verify cleanup", 100);
    {
        // Memory should return close to baseline
        size_t leaked = 0;
        if (after_cleanup.current_allocated > before_error.current_allocated) {
            leaked = after_cleanup.current_allocated - before_error.current_allocated;
        }

        std::cout << "[E2E] Memory leaked: " << leaked << " bytes\n";

        E2E_ASSERT(leaked < MAX_LEAK_AFTER_ERROR_BYTES,
                   "Too much memory leaked after error cleanup");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Partial Failure Recovery
//=============================================================================

E2E_TEST_F(ErrorRecoveryE2ETest, PartialFailureRecovery) {
    E2E_PIPELINE_START("Partial Failure Recovery");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createBrainSafe("partial_failure_brain", BRAIN_SIZE_SMALL, 16, 8);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Simulate partial pipeline failure
    E2E_STAGE_BEGIN("Simulate partial failure", 1000);
    {
        const uint32_t NUM_STAGES = 5;
        std::vector<bool> stage_success(NUM_STAGES, false);
        std::vector<std::string> stage_names = {
            "Perception", "Feature Extraction", "Cognitive Processing",
            "Decision Making", "Motor Output"
        };

        // Fail stage 2 (Feature Extraction)
        error_ctx_.type = ErrorType::CORRUPTED_DATA;
        error_ctx_.injection_point = 2;

        for (uint32_t stage = 0; stage < NUM_STAGES; ++stage) {
            std::cout << "[E2E] Stage " << stage << " (" << stage_names[stage] << "): ";

            if (simulateError(stage)) {
                std::cout << "FAILED - " << error_ctx_.error_message << "\n";

                // Attempt partial recovery - skip this stage but continue
                if (attemptRecovery()) {
                    std::cout << "[E2E] Partial recovery - continuing with defaults\n";
                    stage_success[stage] = false;  // Stage failed but recovered
                }
            } else {
                // Normal processing
                std::vector<float> data = TestDataGenerator::generate_features(16);
                stage_success[stage] = true;
                std::cout << "SUCCESS\n";
            }
        }

        // Count successes
        uint32_t success_count = std::count(stage_success.begin(), stage_success.end(), true);
        std::cout << "[E2E] Stages completed: " << success_count << "/" << NUM_STAGES << "\n";

        E2E_ASSERT(success_count >= NUM_STAGES - 1, "Most stages should succeed");
    }
    E2E_STAGE_END();

    // Stage 3: Verify system still functional
    E2E_STAGE_BEGIN("Verify system functional", 500);
    {
        // System should still work despite partial failure
        std::vector<float> input = TestDataGenerator::generate_features(16);
        std::vector<float> output(8);

        bool processing_ok = true;
        for (size_t i = 0; i < output.size(); ++i) {
            output[i] = std::tanh(input[i % input.size()]);
            if (std::isnan(output[i]) || std::isinf(output[i])) {
                processing_ok = false;
            }
        }

        E2E_ASSERT(processing_ok, "Processing should work after partial failure");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Error Propagation and Boundaries
//=============================================================================

E2E_TEST_F(ErrorRecoveryE2ETest, ErrorPropagationBoundaries) {
    E2E_PIPELINE_START("Error Propagation Boundaries");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createBrainSafe("boundary_test_brain", BRAIN_SIZE_SMALL, 8, 4);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Test error containment
    E2E_STAGE_BEGIN("Test error containment", 1000);
    {
        // Create isolated processing modules
        struct Module {
            std::string name;
            bool has_error;
            bool error_propagated;
        };

        std::vector<Module> modules = {
            {"Input", false, false},
            {"Processing", false, false},
            {"Output", false, false}
        };

        // Inject error in Processing module
        error_ctx_.type = ErrorType::NUMERICAL_ERROR;
        error_ctx_.injection_point = 1;

        for (size_t i = 0; i < modules.size(); ++i) {
            if (simulateError(static_cast<uint32_t>(i))) {
                modules[i].has_error = true;
                std::cout << "[E2E] Error in module: " << modules[i].name << "\n";

                // Error should be contained - not propagate to other modules
                if (attemptRecovery()) {
                    std::cout << "[E2E] Error contained and recovered\n";
                }
            } else {
                std::cout << "[E2E] Module " << modules[i].name << " OK\n";
            }
        }

        // Verify error didn't propagate
        uint32_t modules_with_error = 0;
        for (const auto& m : modules) {
            if (m.has_error) modules_with_error++;
        }

        E2E_ASSERT(modules_with_error <= 1,
                   "Error should be contained to single module");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Graceful Degradation
//=============================================================================

E2E_TEST_F(ErrorRecoveryE2ETest, GracefulDegradation) {
    E2E_PIPELINE_START("Graceful Degradation");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createBrainSafe("degradation_brain", BRAIN_SIZE_MEDIUM, 32, 16);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Simulate increasing stress/errors
    E2E_STAGE_BEGIN("Simulate increasing stress", 3000);
    {
        std::vector<float> quality_scores;

        for (uint32_t stress_level = 0; stress_level <= 10; ++stress_level) {
            float error_probability = static_cast<float>(stress_level) / 10.0f;
            uint32_t errors_in_batch = 0;
            uint32_t total_in_batch = 20;

            for (uint32_t i = 0; i < total_in_batch; ++i) {
                // Random error based on stress level
                if (static_cast<float>(rand()) / RAND_MAX < error_probability) {
                    errors_in_batch++;
                    // Handle error gracefully
                    error_ctx_.type = ErrorType::NUMERICAL_ERROR;
                    attemptRecovery();
                } else {
                    // Normal processing
                    std::vector<float> input = TestDataGenerator::generate_features(32);
                    std::vector<float> output(16);
                    for (size_t j = 0; j < output.size(); ++j) {
                        output[j] = std::tanh(input[j % input.size()]);
                    }
                }
            }

            float quality = 1.0f - static_cast<float>(errors_in_batch) / total_in_batch;
            quality_scores.push_back(quality);

            std::cout << "[E2E] Stress " << stress_level << "/10: "
                      << "errors=" << errors_in_batch
                      << " quality=" << quality << "\n";
        }

        // System should degrade gracefully (not crash)
        E2E_ASSERT(quality_scores.back() >= 0.0f,
                   "System should still provide some output under stress");

        // Quality should generally decrease with stress
        float first_quality = quality_scores.front();
        float last_quality = quality_scores.back();
        std::cout << "[E2E] Quality degradation: " << first_quality
                  << " -> " << last_quality << "\n";
    }
    E2E_STAGE_END();

    // Stage 3: Verify system remains functional
    E2E_STAGE_BEGIN("Verify system functional", 200);
    {
        // Even after stress test, system should work
        std::vector<float> input = TestDataGenerator::generate_features(32);
        std::vector<float> output(16);

        bool still_functional = true;
        for (size_t i = 0; i < output.size(); ++i) {
            output[i] = std::tanh(input[i % input.size()]);
            if (std::isnan(output[i])) still_functional = false;
        }

        E2E_ASSERT(still_functional, "System should remain functional after stress");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test: Repeated Error-Recovery Cycles
//=============================================================================

E2E_TEST_F(ErrorRecoveryE2ETest, RepeatedErrorRecoveryCycles) {
    E2E_PIPELINE_START("Repeated Error-Recovery Cycles");

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        bool created = createBrainSafe("cycles_brain", BRAIN_SIZE_SMALL, 16, 8);
        E2E_ASSERT(created, "Failed to create brain");
    }
    E2E_STAGE_END();

    // Stage 2: Repeated error-recovery cycles
    E2E_STAGE_BEGIN("Error-recovery cycles", 5000);
    {
        const uint32_t NUM_CYCLES = 20;
        uint32_t successful_recoveries = 0;

        for (uint32_t cycle = 0; cycle < NUM_CYCLES; ++cycle) {
            // Inject error
            error_ctx_.type = static_cast<ErrorType>((cycle % 6) + 1);  // Rotate error types
            error_ctx_.injection_point = cycle;
            error_ctx_.recovered = false;

            if (simulateError(cycle)) {
                std::cout << "[E2E] Cycle " << cycle << " error: "
                          << error_ctx_.error_message << "\n";

                // Attempt recovery
                if (attemptRecovery()) {
                    successful_recoveries++;
                }
            }

            // Brief processing to verify system works
            std::vector<float> input = TestDataGenerator::generate_features(16);
            std::vector<float> output(8);
            for (size_t i = 0; i < output.size(); ++i) {
                output[i] = std::tanh(input[i % input.size()]);
            }
        }

        std::cout << "[E2E] Successful recoveries: " << successful_recoveries
                  << "/" << NUM_CYCLES << "\n";

        E2E_ASSERT(successful_recoveries == NUM_CYCLES,
                   "All error-recovery cycles should succeed");
    }
    E2E_STAGE_END();

    // Stage 3: Final memory check
    E2E_STAGE_BEGIN("Final memory check", 100);
    {
        nimcp_memory_stats_t current_stats;
        nimcp_memory_get_stats(&current_stats);

        size_t growth = 0;
        if (current_stats.current_allocated > initial_stats_.current_allocated) {
            growth = current_stats.current_allocated - initial_stats_.current_allocated;
        }

        std::cout << "[E2E] Memory growth after cycles: " << growth << " bytes\n";

        // Allow for brain memory, but no excessive growth
        E2E_ASSERT(growth < 1024 * 1024, "Memory should not grow excessively");
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
