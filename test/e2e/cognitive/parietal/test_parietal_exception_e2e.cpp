/**
 * @file test_parietal_exception_e2e.cpp
 * @brief End-to-end tests for parietal module exception handling
 * @date 2026-01-25
 *
 * WHAT: Test full parietal pipeline with exception recovery
 * WHY:  Verify complete error handling workflow in realistic scenarios
 * HOW:  Simulate realistic parietal workloads with injected faults
 *
 * E2E SCENARIOS:
 * - Full mathematical reasoning with transient failures
 * - Spatial reasoning under exception stress
 * - Long-running simulation with random exception injection
 * - Multi-threaded parietal with concurrent exceptions
 * - System recovery and state preservation under stress
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <cmath>

#include "cognitive/immune/nimcp_brain_immune.h"

extern "C" {
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_number_sense.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_equation_manipulation.h"
#include "cognitive/parietal/nimcp_scientific_reasoning.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ParietalExceptionE2ETest : public ::testing::Test {
protected:
    static std::atomic<int> total_exceptions;
    static std::atomic<int> recovered_exceptions;
    static std::atomic<int> immune_responses;

    brain_immune_system_t* immune_system;
    parietal_lobe_t* parietal;
    number_sense_t* number_sense;
    spatial_reasoning_t* spatial;
    math_intuition_t* math_intuition;
    equation_engine_t* equation;
    scientific_reasoning_t* scientific;

    std::mt19937 rng;

    void SetUp() override {
        total_exceptions = 0;
        recovered_exceptions = 0;
        immune_responses = 0;

        rng.seed(42);  // Fixed seed for reproducibility

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
    static nimcp_handler_registration_t* recovery_handler_reg;
    static nimcp_handler_registration_t* immune_handler_reg;

    static bool exception_counter(nimcp_exception_t* ex, void* user_data) {
        (void)ex;
        (void)user_data;
        total_exceptions++;
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

    static bool immune_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        immune_responses++;
        return false;
    }

    void register_handlers() {
        nimcp_handler_options_t opts;

        nimcp_handler_default_options(&opts);
        opts.name = "e2e_parietal_exception_counter";
        opts.handler = exception_counter;
        opts.priority = 200;
        exception_handler_reg = nimcp_handler_register(&opts);

        nimcp_handler_default_options(&opts);
        opts.name = "e2e_parietal_recovery_handler";
        opts.handler = recovery_handler;
        opts.priority = 100;
        recovery_handler_reg = nimcp_handler_register(&opts);

        nimcp_handler_default_options(&opts);
        opts.name = "e2e_parietal_immune_handler";
        opts.handler = immune_handler;
        opts.priority = 50;
        immune_handler_reg = nimcp_handler_register(&opts);
    }

    void unregister_handlers() {
        if (exception_handler_reg) nimcp_handler_unregister(exception_handler_reg);
        if (recovery_handler_reg) nimcp_handler_unregister(recovery_handler_reg);
        if (immune_handler_reg) nimcp_handler_unregister(immune_handler_reg);
        exception_handler_reg = nullptr;
        recovery_handler_reg = nullptr;
        immune_handler_reg = nullptr;
    }

    // Simulate a number sense operation with possible fault injection
    bool simulate_number_sense_step(float error_probability) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        if (dist(rng) < error_probability) {
            number_sense_estimate(nullptr, nullptr, 0);
        } else {
            float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
            number_sense_estimate(number_sense, values, 5);
        }

        return true;
    }

    // Simulate spatial reasoning with possible fault injection
    bool simulate_spatial_step(float error_probability) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        if (dist(rng) < error_probability) {
            vec3_t query = {0.0f, 0.0f, 0.0f};
            spatial_find_nearest(nullptr, query);
        } else {
            spatial_stats_t stats;
            spatial_get_stats(spatial, &stats);
        }

        return true;
    }

    // Simulate pattern detection with possible fault injection
    bool simulate_pattern_step(float error_probability) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        if (dist(rng) < error_probability) {
            math_detect_pattern(nullptr, nullptr, 0);
        } else {
            float sequence[] = {1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
            math_detect_pattern(math_intuition, sequence, 5);
        }

        return true;
    }
};

std::atomic<int> ParietalExceptionE2ETest::total_exceptions{0};
std::atomic<int> ParietalExceptionE2ETest::recovered_exceptions{0};
std::atomic<int> ParietalExceptionE2ETest::immune_responses{0};
nimcp_handler_registration_t* ParietalExceptionE2ETest::exception_handler_reg = nullptr;
nimcp_handler_registration_t* ParietalExceptionE2ETest::recovery_handler_reg = nullptr;
nimcp_handler_registration_t* ParietalExceptionE2ETest::immune_handler_reg = nullptr;

//=============================================================================
// Full Mathematical Reasoning E2E Tests
//=============================================================================

TEST_F(ParietalExceptionE2ETest, FullMathematicalReasoningWithTransientFailures) {
    // WHAT: Test complete mathematical reasoning cycle with occasional failures
    // WHY:  Real systems experience transient errors

    ASSERT_NE(parietal, nullptr);
    ASSERT_NE(number_sense, nullptr);
    ASSERT_NE(math_intuition, nullptr);

    const int num_epochs = 10;
    const int steps_per_epoch = 100;
    const float error_probability = 0.05f;  // 5% error rate

    total_exceptions = 0;
    int successful_steps = 0;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        for (int step = 0; step < steps_per_epoch; step++) {
            simulate_number_sense_step(error_probability);
            simulate_pattern_step(error_probability);
            successful_steps++;
        }
    }

    // Verify processing completed despite errors
    EXPECT_EQ(successful_steps, num_epochs * steps_per_epoch);

    // Verify exceptions were handled
    EXPECT_GT(total_exceptions.load(), 0);

    // Verify immune system was notified
    EXPECT_GT(immune_responses.load(), 0);

    // Get final metrics - modules should still work
    number_sense_stats_t ns_stats;
    int result = number_sense_get_stats(number_sense, &ns_stats);
    EXPECT_GE(result, 0);

    math_intuition_stats_t mi_stats;
    result = math_intuition_get_stats(math_intuition, &mi_stats);
    EXPECT_GE(result, 0);
}

TEST_F(ParietalExceptionE2ETest, SpatialReasoningUnderExceptionStress) {
    // WHAT: Test spatial reasoning with heavy exception load
    // WHY:  Spatial processing must remain functional under stress

    ASSERT_NE(spatial, nullptr);

    const int num_steps = 500;
    const float error_probability = 0.1f;  // 10% error rate

    total_exceptions = 0;

    for (int step = 0; step < num_steps; step++) {
        simulate_spatial_step(error_probability);
    }

    // Verify some exceptions occurred
    EXPECT_GT(total_exceptions.load(), 0);

    // Verify spatial system is still functional
    spatial_stats_t stats;
    int result = spatial_get_stats(spatial, &stats);
    EXPECT_GE(result, 0);
}

//=============================================================================
// Long-Running Simulation E2E Tests
//=============================================================================

TEST_F(ParietalExceptionE2ETest, LongRunningParietalWithRandomFaults) {
    // WHAT: Simulate long-running parietal processing with random fault injection
    // WHY:  Verify stability over extended operation

    ASSERT_NE(parietal, nullptr);
    ASSERT_NE(number_sense, nullptr);
    ASSERT_NE(spatial, nullptr);
    ASSERT_NE(math_intuition, nullptr);

    const int simulation_seconds = 3;  // Simulated seconds
    const int steps_per_second = 500;
    const float error_probability = 0.02f;

    auto start = std::chrono::high_resolution_clock::now();

    total_exceptions = 0;
    int total_steps = 0;

    for (int second = 0; second < simulation_seconds; second++) {
        for (int step = 0; step < steps_per_second; step++) {
            // Mix different parietal operations
            switch (step % 3) {
                case 0:
                    simulate_number_sense_step(error_probability);
                    break;
                case 1:
                    simulate_spatial_step(error_probability);
                    break;
                case 2:
                    simulate_pattern_step(error_probability);
                    break;
            }
            total_steps++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Report performance
    double steps_per_ms = static_cast<double>(total_steps) / duration.count();

    // Verify all steps completed
    EXPECT_EQ(total_steps, simulation_seconds * steps_per_second);

    // Verify exceptions were handled
    EXPECT_GT(total_exceptions.load(), 0);

    // Verify all systems remain stable
    number_sense_stats_t ns_stats;
    EXPECT_GE(number_sense_get_stats(number_sense, &ns_stats), 0);

    spatial_stats_t sr_stats;
    EXPECT_GE(spatial_get_stats(spatial, &sr_stats), 0);

    math_intuition_stats_t mi_stats;
    EXPECT_GE(math_intuition_get_stats(math_intuition, &mi_stats), 0);

    // Performance sanity check
    EXPECT_GT(steps_per_ms, 5.0)
        << "Performance: " << steps_per_ms << " steps/ms";
}

//=============================================================================
// Multi-Threaded Parietal E2E Tests
//=============================================================================

TEST_F(ParietalExceptionE2ETest, MultiThreadedParietalWithConcurrentExceptions) {
    // WHAT: Test concurrent parietal processing with exceptions
    // WHY:  Real neural simulations are multi-threaded

    const int num_threads = 4;
    const int ops_per_thread = 250;
    const float error_probability = 0.05f;

    std::atomic<int> thread_completions{0};
    std::atomic<int> thread_errors{0};
    total_exceptions = 0;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([t, ops_per_thread, error_probability,
                              &thread_completions, &thread_errors]() {
            std::mt19937 thread_rng(42 + t);
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);

            try {
                for (int i = 0; i < ops_per_thread; i++) {
                    // Mix of different parietal operations
                    switch ((t + i) % 6) {
                        case 0:
                            if (dist(thread_rng) < error_probability) {
                                number_sense_estimate(nullptr, nullptr, 0);
                            }
                            break;
                        case 1:
                            if (dist(thread_rng) < error_probability) {
                                spatial_rotate_and_compare(nullptr, nullptr, nullptr);
                            }
                            break;
                        case 2:
                            if (dist(thread_rng) < error_probability) {
                                math_detect_pattern(nullptr, nullptr, 0);
                            }
                            break;
                        case 3:
                            if (dist(thread_rng) < error_probability) {
                                equation_parse(nullptr, nullptr);
                            }
                            break;
                        case 4:
                            if (dist(thread_rng) < error_probability) {
                                scientific_create_hypothesis(nullptr, nullptr, 0.0f);
                            }
                            break;
                        case 5:
                            if (dist(thread_rng) < error_probability) {
                                parietal_process(nullptr, nullptr);
                            }
                            break;
                    }
                }
                thread_completions++;
            } catch (...) {
                thread_errors++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should complete without crashing
    EXPECT_EQ(thread_completions.load(), num_threads);
    EXPECT_EQ(thread_errors.load(), 0);

    // Exceptions should have been raised
    EXPECT_GT(total_exceptions.load(), 0);
}

//=============================================================================
// System Recovery E2E Tests
//=============================================================================

TEST_F(ParietalExceptionE2ETest, SystemRecoveryUnderStress) {
    // WHAT: Test system recovery after heavy exception load
    // WHY:  System must remain functional after stress

    ASSERT_NE(parietal, nullptr);
    ASSERT_NE(number_sense, nullptr);
    ASSERT_NE(spatial, nullptr);
    ASSERT_NE(math_intuition, nullptr);
    ASSERT_NE(equation, nullptr);
    ASSERT_NE(scientific, nullptr);

    total_exceptions = 0;

    // Phase 1: Generate heavy exception load
    const int stress_iterations = 500;
    for (int i = 0; i < stress_iterations; i++) {
        number_sense_estimate(nullptr, nullptr, 0);
        spatial_rotate_and_compare(nullptr, nullptr, nullptr);
        math_detect_pattern(nullptr, nullptr, 0);
        equation_parse(nullptr, nullptr);
        scientific_create_hypothesis(nullptr, nullptr, 0.0f);
    }

    int exceptions_during_stress = total_exceptions.load();
    EXPECT_GE(exceptions_during_stress, stress_iterations * 5);

    // Phase 2: Verify all systems recover and function correctly
    // Number sense
    number_comparison_t cmp = number_sense_compare(number_sense, 10.0f, 5.0f);
    EXPECT_GT(cmp.confidence, 0.0f);
    EXPECT_GT(cmp.direction, 0);  // 10 > 5

    // Spatial reasoning
    spatial_stats_t sr_stats;
    EXPECT_GE(spatial_get_stats(spatial, &sr_stats), 0);

    // Math intuition
    float sequence[] = {1.0f, 2.0f, 4.0f, 8.0f};
    detected_pattern_t pattern = math_detect_pattern(math_intuition, sequence, 4);
    EXPECT_GT(pattern.confidence, 0.0f);

    // Equation engine
    equation_stats_t eq_stats;
    EXPECT_GE(equation_get_stats(equation, &eq_stats), 0);

    // Scientific reasoning
    scientific_stats_t sci_stats;
    EXPECT_GE(scientific_get_stats(scientific, &sci_stats), 0);
}

TEST_F(ParietalExceptionE2ETest, StatePreservationDuringExceptions) {
    // WHAT: Test that state is preserved when exceptions occur
    // WHY:  Exceptions should not corrupt working state

    ASSERT_NE(number_sense, nullptr);

    // Perform some valid comparisons
    for (int i = 0; i < 100; i++) {
        number_sense_compare(number_sense, (float)i, (float)(i + 1));
    }

    number_sense_stats_t stats_before;
    EXPECT_GE(number_sense_get_stats(number_sense, &stats_before), 0);
    uint64_t comparisons_before = stats_before.comparisons_performed;

    // Generate many exceptions
    total_exceptions = 0;
    for (int i = 0; i < 500; i++) {
        number_sense_estimate(nullptr, nullptr, 0);
        number_sense_compare(nullptr, 1.0f, 2.0f);
    }

    EXPECT_GE(total_exceptions.load(), 1000);

    // Verify state was preserved
    number_sense_stats_t stats_after;
    EXPECT_GE(number_sense_get_stats(number_sense, &stats_after), 0);
    EXPECT_EQ(stats_after.comparisons_performed, comparisons_before);

    // Add more valid operations after exceptions
    for (int i = 0; i < 50; i++) {
        number_sense_compare(number_sense, (float)i, (float)(i + 2));
    }

    number_sense_stats_t stats_final;
    EXPECT_GE(number_sense_get_stats(number_sense, &stats_final), 0);
    EXPECT_EQ(stats_final.comparisons_performed, comparisons_before + 50);
}

//=============================================================================
// Exception-Driven Adaptation E2E Tests
//=============================================================================

TEST_F(ParietalExceptionE2ETest, ExceptionDrivenSystemAdaptation) {
    // WHAT: Test that immune responses can drive system adaptation
    // WHY:  Immune system should learn from exceptions

    ASSERT_NE(immune_system, nullptr);

    total_exceptions = 0;
    immune_responses = 0;

    // Generate exceptions from different parietal modules
    const int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        // Number sense errors
        number_sense_estimate(nullptr, nullptr, 0);

        // Spatial errors
        spatial_rotate_and_compare(nullptr, nullptr, nullptr);

        // Math intuition errors
        math_detect_pattern(nullptr, nullptr, 0);
    }

    // Verify immune system processed exceptions
    EXPECT_GE(total_exceptions.load(), iterations * 3);
    EXPECT_GE(immune_responses.load(), iterations * 3);

    // Immune system should be functional
    ASSERT_NE(immune_system, nullptr);
}

//=============================================================================
// Combined Parietal Pipeline E2E Tests
//=============================================================================

TEST_F(ParietalExceptionE2ETest, RealisticParietalWorkflow) {
    // WHAT: Simulate realistic parietal workflow with occasional errors
    // WHY:  End-to-end validation of complete system

    ASSERT_NE(parietal, nullptr);
    ASSERT_NE(number_sense, nullptr);
    ASSERT_NE(spatial, nullptr);
    ASSERT_NE(math_intuition, nullptr);

    const int num_trials = 20;
    const int steps_per_trial = 50;
    const float base_error_rate = 0.02f;

    total_exceptions = 0;

    for (int trial = 0; trial < num_trials; trial++) {
        // Varying error rate: higher in early trials
        float trial_error_rate = base_error_rate * (1.0f + (num_trials - trial) * 0.05f);

        for (int step = 0; step < steps_per_trial; step++) {
            // Number sense
            simulate_number_sense_step(trial_error_rate);

            // Spatial reasoning
            simulate_spatial_step(trial_error_rate);

            // Pattern detection
            simulate_pattern_step(trial_error_rate);
        }
    }

    // Verify workflow completed
    number_sense_stats_t ns_stats;
    EXPECT_GE(number_sense_get_stats(number_sense, &ns_stats), 0);

    // Verify exceptions were handled but didn't stop execution
    EXPECT_GT(total_exceptions.load(), 0);

    // Verify all systems remain operational
    spatial_stats_t sr_stats;
    EXPECT_GE(spatial_get_stats(spatial, &sr_stats), 0);

    math_intuition_stats_t mi_stats;
    EXPECT_GE(math_intuition_get_stats(math_intuition, &mi_stats), 0);
}

//=============================================================================
// All Parietal Modules Exception Stress Test
//=============================================================================

TEST_F(ParietalExceptionE2ETest, AllModulesExceptionStressCombined) {
    // WHAT: Test all parietal modules under combined exception stress
    // WHY:  End-to-end validation of cross-module exception handling

    total_exceptions = 0;

    // Combined stress test on all parietal modules
    for (int i = 0; i < 200; i++) {
        // Parietal orchestrator
        parietal_process(nullptr, nullptr);

        // Number sense
        number_sense_estimate(nullptr, nullptr, 0);

        // Spatial reasoning
        spatial_rotate_and_compare(nullptr, nullptr, nullptr);

        // Math intuition
        math_detect_pattern(nullptr, nullptr, 0);

        // Equation engine
        equation_parse(nullptr, nullptr);

        // Scientific reasoning
        scientific_create_hypothesis(nullptr, nullptr, 0.0f);

        // Bridge attachments
        parietal_attach_immune(nullptr, nullptr);
        parietal_attach_fep(nullptr, nullptr);
    }

    // Verify many exceptions were raised across all modules
    EXPECT_GE(total_exceptions.load(), 200 * 8);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
