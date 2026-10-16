/**
 * @file test_plasticity_module_exception_e2e.cpp
 * @brief End-to-end tests for plasticity module NIMCP_THROW_TO_IMMUNE exception handling
 *
 * WHAT: Test full plasticity pipeline with exception recovery
 * WHY:  Verify complete error handling workflow in realistic scenarios
 * HOW:  Simulate realistic plasticity workloads with injected faults
 *
 * E2E SCENARIOS:
 * - Full learning cycle with transient neuromodulator failures
 * - Attention-modulated STDP with allocation failures
 * - Long-running simulation with random exception injection
 * - Multi-threaded plasticity with concurrent exceptions
 * - System recovery and state preservation under stress
 *
 * @author NIMCP Development Team
 * @date 2026-01-25
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

// Include C++ compatible headers first (may have CUDA linkage)
#include "plasticity/neuromodulators/nimcp_neuromodulators_sleep_bridge.h"
#include "plasticity/stdp/nimcp_triplet_stdp_immune_bridge.h"
#include "plasticity/stdp/nimcp_triplet_stdp_sleep_bridge.h"
#include "plasticity/eligibility/nimcp_eligibility_pr_bridge.h"

extern "C" {
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/attention/nimcp_attention.h"
#include "plasticity/stdp/nimcp_stdp_pr_bridge.h"
#include "plasticity/stdp/nimcp_stdp_utils_bridge.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/stdp/nimcp_triplet_stdp.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PlasticityModuleExceptionE2ETest : public ::testing::Test {
protected:
    static std::atomic<int> total_exceptions;
    static std::atomic<int> recovered_exceptions;
    static std::atomic<int> immune_responses;

    brain_immune_system_t* immune_system;
    neuromodulator_system_t neuromod_system;
    stdp_pr_bridge_t stdp_pr;
    stdp_utils_ctx_t stdp_utils;
    multihead_attention_t attention;

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

        // Create neuromodulator system
        neuromodulator_config_t neuro_config = {
            .baseline_dopamine = 0.5f,
            .baseline_serotonin = 0.5f,
            .baseline_acetylcholine = 0.5f,
            .baseline_norepinephrine = 0.5f,
            .dopamine_decay = 2.0f,
            .serotonin_decay = 10.0f,
            .acetylcholine_decay = 0.5f,
            .norepinephrine_decay = 3.0f,
            .reward_dopamine_gain = 0.5f,
            .threat_norepinephrine_gain = 0.7f,
            .salience_acetylcholine_gain = 0.6f,
            .punishment_serotonin_gain = 0.4f,
            .enable_volume_transmission = false,
            .diffusion_rate = 0.0f
        };
        neuromod_system = neuromodulator_system_create(&neuro_config);

        // Create STDP-PR bridge
        stdp_pr = stdp_pr_bridge_create(nullptr);

        // Create STDP utils context
        stdp_utils = stdp_utils_create(nullptr);

        // Create multihead attention
        multihead_attention_config_t attn_config = {
            .num_heads = 4,
            .input_dim = 64,
            .output_dim = 64,
            .sequence_length = 16,
            .use_thalamic_gate = true,
            .use_salience_weighting = true,
            .gate_bias = 0.5f,
            .use_positional_encoding = false,
            .pe_type = NIMCP_POS_ROTARY,
            .rope_base = 10000.0f,
            .alibi_slope_base = 1.0f,
            .enable_quantum_attention = false
        };
        attention = multihead_attention_create(&attn_config);

        register_handlers();
    }

    void TearDown() override {
        unregister_handlers();

        if (attention) {
            multihead_attention_destroy(attention);
            attention = nullptr;
        }
        if (stdp_utils) {
            stdp_utils_destroy(stdp_utils);
            stdp_utils = nullptr;
        }
        if (stdp_pr) {
            stdp_pr_bridge_destroy(stdp_pr);
            stdp_pr = nullptr;
        }
        if (neuromod_system) {
            neuromodulator_system_destroy(neuromod_system);
            neuromod_system = nullptr;
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }

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
        opts.name = "e2e_exception_counter";
        opts.handler = exception_counter;
        opts.priority = 200;
        exception_handler_reg = nimcp_handler_register(&opts);

        nimcp_handler_default_options(&opts);
        opts.name = "e2e_recovery_handler";
        opts.handler = recovery_handler;
        opts.priority = 100;
        recovery_handler_reg = nimcp_handler_register(&opts);

        nimcp_handler_default_options(&opts);
        opts.name = "e2e_immune_handler";
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

    // Simulate a learning step with possible fault injection
    bool simulate_learning_step(float reward, float error_probability) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        // Neuromodulator update with possible fault
        if (dist(rng) < error_probability) {
            neuromodulator_release_dopamine(nullptr, reward, dist(rng));
        } else {
            neuromodulator_release_dopamine(neuromod_system, reward, dist(rng));
        }

        // STDP update with possible fault
        if (dist(rng) < error_probability) {
            stdp_pr_notify_ltp(nullptr, 1, 2, 0.1f * dist(rng), nullptr);
        } else {
            stdp_pr_notify_ltp(stdp_pr, 1, 2, 0.1f * dist(rng), nullptr);
        }

        // Record metrics with possible fault
        if (dist(rng) < error_probability) {
            stdp_utils_record_ltp(nullptr, 0.1f * dist(rng), 10.0f);
        } else {
            stdp_utils_record_ltp(stdp_utils, 0.1f * dist(rng), 10.0f);
        }

        return true;
    }

    // Simulate attention step with possible fault injection
    bool simulate_attention_step(float gate_signal, float error_probability) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        if (dist(rng) < error_probability) {
            multihead_attention_set_gate(nullptr, gate_signal);
        } else {
            multihead_attention_set_gate(attention, gate_signal);
        }

        return true;
    }
};

std::atomic<int> PlasticityModuleExceptionE2ETest::total_exceptions(0);
std::atomic<int> PlasticityModuleExceptionE2ETest::recovered_exceptions(0);
std::atomic<int> PlasticityModuleExceptionE2ETest::immune_responses(0);
nimcp_handler_registration_t* PlasticityModuleExceptionE2ETest::exception_handler_reg = nullptr;
nimcp_handler_registration_t* PlasticityModuleExceptionE2ETest::recovery_handler_reg = nullptr;
nimcp_handler_registration_t* PlasticityModuleExceptionE2ETest::immune_handler_reg = nullptr;

//=============================================================================
// Full Learning Cycle E2E Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, FullLearningCycleWithTransientFailures) {
    // WHAT: Test complete learning cycle with occasional failures
    // WHY:  Real systems experience transient errors

    ASSERT_NE(neuromod_system, nullptr);
    ASSERT_NE(stdp_pr, nullptr);
    ASSERT_NE(stdp_utils, nullptr);

    const int num_epochs = 10;
    const int steps_per_epoch = 100;
    const float error_probability = 0.05f;  // 5% error rate

    total_exceptions = 0;
    int successful_steps = 0;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        float epoch_reward = 0.5f + 0.05f * epoch;  // Increasing reward

        for (int step = 0; step < steps_per_epoch; step++) {
            if (simulate_learning_step(epoch_reward, error_probability)) {
                successful_steps++;
            }
        }

        // Epoch boundary: update neuromodulator system
        neuromodulator_update(neuromod_system, 0.1f);
    }

    // Verify learning completed despite errors
    EXPECT_EQ(successful_steps, num_epochs * steps_per_epoch);

    // Verify exceptions were handled
    EXPECT_GT(total_exceptions.load(), 0);

    // Verify immune system was notified
    EXPECT_GT(immune_responses.load(), 0);

    // Get final metrics
    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(stdp_utils, &metrics);
    EXPECT_TRUE(result);
    EXPECT_GT(metrics.total_ltp_events, 0u);

    // Verify neuromodulator system is functional
    float dopamine = neuromodulator_release_dopamine(neuromod_system, 1.0f, 0.5f);
    EXPECT_GT(dopamine, 0.0f);
}

TEST_F(PlasticityModuleExceptionE2ETest, AttentionModulatedSTDPWithFailures) {
    // WHAT: Test attention-modulated STDP with injection failures
    // WHY:  Attention influences learning; failures should be recoverable

    ASSERT_NE(attention, nullptr);
    ASSERT_NE(stdp_pr, nullptr);
    ASSERT_NE(stdp_utils, nullptr);

    const int num_steps = 500;
    const float error_probability = 0.03f;

    total_exceptions = 0;

    for (int step = 0; step < num_steps; step++) {
        // Attention modulation with possible fault
        float gate = 0.5f + 0.5f * std::sin(step * 0.1f);
        simulate_attention_step(gate, error_probability);

        // Learning step with possible fault
        float reward = (step % 100) < 50 ? 1.0f : -0.5f;
        simulate_learning_step(reward, error_probability);
    }

    // Verify some exceptions occurred
    EXPECT_GT(total_exceptions.load(), 0);

    // Verify attention system is still functional
    attention_stats_t stats;
    bool result = multihead_attention_get_stats(attention, &stats);
    EXPECT_TRUE(result);
}

//=============================================================================
// Long-Running Simulation E2E Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, LongRunningSimulationWithRandomFaults) {
    // WHAT: Simulate long-running plasticity with random fault injection
    // WHY:  Verify stability over extended operation

    ASSERT_NE(neuromod_system, nullptr);
    ASSERT_NE(stdp_pr, nullptr);
    ASSERT_NE(stdp_utils, nullptr);

    const int simulation_seconds = 5;  // Simulated seconds
    const int steps_per_second = 1000;
    const float error_probability = 0.02f;

    auto start = std::chrono::high_resolution_clock::now();

    total_exceptions = 0;
    int total_steps = 0;

    for (int second = 0; second < simulation_seconds; second++) {
        for (int step = 0; step < steps_per_second; step++) {
            // Varying reward based on time
            float reward = std::sin(second * 0.5f + step * 0.01f);

            simulate_learning_step(reward, error_probability);
            total_steps++;
        }

        // Periodic system update
        neuromodulator_update(neuromod_system, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Report performance
    double steps_per_ms = static_cast<double>(total_steps) / duration.count();

    // Verify all steps completed
    EXPECT_EQ(total_steps, simulation_seconds * steps_per_second);

    // Verify exceptions were handled
    EXPECT_GT(total_exceptions.load(), 0);

    // Verify system is still stable
    float dopamine = neuromodulator_release_dopamine(neuromod_system, 1.0f, 0.5f);
    EXPECT_GT(dopamine, 0.0f);

    bool connected = stdp_pr_bridge_is_connected(stdp_pr);
    EXPECT_TRUE(connected);

    // Performance sanity check
    EXPECT_GT(steps_per_ms, 10.0)
        << "Performance: " << steps_per_ms << " steps/ms";
}

//=============================================================================
// Multi-Threaded Plasticity E2E Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, MultiThreadedPlasticityWithConcurrentExceptions) {
    // WHAT: Test concurrent plasticity updates with exceptions
    // WHY:  Real neural simulations are multi-threaded

    const int num_threads = 4;
    const int ops_per_thread = 500;
    const float error_probability = 0.05f;

    std::atomic<int> thread_completions{0};
    std::atomic<int> thread_errors{0};
    total_exceptions = 0;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, ops_per_thread, error_probability,
                              &thread_completions, &thread_errors]() {
            std::mt19937 thread_rng(42 + t);
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);

            try {
                for (int i = 0; i < ops_per_thread; i++) {
                    // Mix of different operations
                    switch ((t + i) % 5) {
                        case 0: {
                            // Neuromodulator
                            if (dist(thread_rng) < error_probability) {
                                neuromodulator_pool_get_dopamine(nullptr);
                            } else {
                                neuromodulator_release_dopamine(neuromod_system, 1.0f, dist(thread_rng));
                            }
                            break;
                        }
                        case 1: {
                            // STDP-PR LTP
                            if (dist(thread_rng) < error_probability) {
                                stdp_pr_notify_ltp(nullptr, 1, 2, 0.1f, nullptr);
                            } else {
                                stdp_pr_notify_ltp(stdp_pr, 1, 2, 0.1f * dist(thread_rng), nullptr);
                            }
                            break;
                        }
                        case 2: {
                            // STDP-PR LTD
                            if (dist(thread_rng) < error_probability) {
                                stdp_pr_notify_ltd(nullptr, 1, 2, -0.1f, nullptr);
                            } else {
                                stdp_pr_notify_ltd(stdp_pr, 1, 2, -0.1f * dist(thread_rng), nullptr);
                            }
                            break;
                        }
                        case 3: {
                            // STDP utils record
                            if (dist(thread_rng) < error_probability) {
                                stdp_utils_record_ltp(nullptr, 0.1f, 10.0f);
                            } else {
                                stdp_utils_record_ltp(stdp_utils, 0.1f * dist(thread_rng), 10.0f);
                            }
                            break;
                        }
                        case 4: {
                            // Attention gate
                            if (dist(thread_rng) < error_probability) {
                                multihead_attention_set_gate(nullptr, 0.5f);
                            } else {
                                multihead_attention_set_gate(attention, 0.5f * dist(thread_rng));
                            }
                            break;
                        }
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

TEST_F(PlasticityModuleExceptionE2ETest, SystemRecoveryUnderStress) {
    // WHAT: Test system recovery after heavy exception load
    // WHY:  System must remain functional after stress

    ASSERT_NE(neuromod_system, nullptr);
    ASSERT_NE(stdp_pr, nullptr);
    ASSERT_NE(stdp_utils, nullptr);
    ASSERT_NE(attention, nullptr);

    total_exceptions = 0;

    // Phase 1: Generate heavy exception load
    // NOTE: Use functions that actually throw exceptions (not pool getters which just return 0)
    // multihead_attention_set_gate does NOT throw for NULL, so we use functions that do
    const int stress_iterations = 1000;
    for (int i = 0; i < stress_iterations; i++) {
        neuromodulator_release_dopamine(nullptr, 1.0f, 0.5f);  // Throws exception
        stdp_pr_notify_ltp(nullptr, 1, 2, 0.1f, nullptr);       // Throws exception
        stdp_utils_record_ltp(nullptr, 0.1f, 10.0f);            // Throws exception
    }

    int exceptions_during_stress = total_exceptions.load();
    EXPECT_GE(exceptions_during_stress, stress_iterations * 3);

    // Phase 2: Verify all systems recover and function correctly
    // Neuromodulator system
    float dopamine = neuromodulator_release_dopamine(neuromod_system, 1.0f, 0.5f);
    EXPECT_GT(dopamine, 0.0f);

    neuromodulator_pool_t pool;
    bool result = neuromodulator_get_levels(neuromod_system, &pool);
    EXPECT_TRUE(result);

    // STDP-PR bridge
    bool connected = stdp_pr_bridge_is_connected(stdp_pr);
    EXPECT_TRUE(connected);

    float coherence = stdp_pr_bridge_get_coherence(stdp_pr);
    EXPECT_GE(coherence, 0.0f);

    // STDP utils
    stdp_utils_record_ltp(stdp_utils, 0.1f, 10.0f);
    stdp_metrics_t metrics;
    result = stdp_utils_get_metrics(stdp_utils, &metrics);
    EXPECT_TRUE(result);
    EXPECT_GT(metrics.total_ltp_events, 0u);

    // Attention
    result = multihead_attention_set_gate(attention, 0.7f);
    EXPECT_TRUE(result);

    attention_stats_t stats;
    result = multihead_attention_get_stats(attention, &stats);
    EXPECT_TRUE(result);
}

TEST_F(PlasticityModuleExceptionE2ETest, StatePreservationDuringExceptions) {
    // WHAT: Test that state is preserved when exceptions occur
    // WHY:  Exceptions should not corrupt working state

    ASSERT_NE(neuromod_system, nullptr);
    ASSERT_NE(stdp_utils, nullptr);

    // Record some valid data
    for (int i = 0; i < 100; i++) {
        stdp_utils_record_ltp(stdp_utils, 0.1f + i * 0.001f, 10.0f);
    }

    stdp_metrics_t metrics_before;
    bool result = stdp_utils_get_metrics(stdp_utils, &metrics_before);
    EXPECT_TRUE(result);
    uint32_t ltp_before = metrics_before.total_ltp_events;

    // Generate many exceptions
    total_exceptions = 0;
    for (int i = 0; i < 500; i++) {
        stdp_utils_record_ltp(nullptr, 0.1f, 10.0f);
        stdp_utils_reset(nullptr);
    }

    EXPECT_GE(total_exceptions.load(), 1000);

    // Verify state was preserved
    stdp_metrics_t metrics_after;
    result = stdp_utils_get_metrics(stdp_utils, &metrics_after);
    EXPECT_TRUE(result);
    EXPECT_EQ(metrics_after.total_ltp_events, ltp_before);

    // Add more data after exceptions
    for (int i = 0; i < 50; i++) {
        stdp_utils_record_ltp(stdp_utils, 0.2f + i * 0.001f, 15.0f);
    }

    stdp_metrics_t metrics_final;
    result = stdp_utils_get_metrics(stdp_utils, &metrics_final);
    EXPECT_TRUE(result);
    EXPECT_EQ(metrics_final.total_ltp_events, ltp_before + 50);
}

//=============================================================================
// Exception-Driven Adaptation E2E Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, ExceptionDrivenSystemAdaptation) {
    // WHAT: Test that immune responses can drive system adaptation
    // WHY:  Immune system should learn from exceptions

    ASSERT_NE(immune_system, nullptr);

    total_exceptions = 0;
    immune_responses = 0;

    // Generate exceptions from different modules
    // NOTE: Use functions that actually throw exceptions (not pool getters which just return 0)
    const int iterations = 100;
    for (int i = 0; i < iterations; i++) {
        // Neuromodulator errors
        neuromodulator_release_dopamine(nullptr, 1.0f, 0.5f);

        // STDP errors
        stdp_pr_notify_ltp(nullptr, 1, 2, 0.1f, nullptr);

        // Utils errors
        stdp_utils_record_ltp(nullptr, 0.1f, 10.0f);
    }

    // Verify immune system processed exceptions
    EXPECT_GE(total_exceptions.load(), iterations * 3);
    EXPECT_GE(immune_responses.load(), iterations * 3);

    // Immune system should be functional
    ASSERT_NE(immune_system, nullptr);
}

//=============================================================================
// End-to-End Workflow Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, RealisticLearningWorkflow) {
    // WHAT: Simulate realistic learning workflow with occasional errors
    // WHY:  End-to-end validation of complete system

    ASSERT_NE(neuromod_system, nullptr);
    ASSERT_NE(stdp_pr, nullptr);
    ASSERT_NE(stdp_utils, nullptr);
    ASSERT_NE(attention, nullptr);

    const int num_trials = 20;
    const int steps_per_trial = 50;
    const float base_error_rate = 0.02f;

    total_exceptions = 0;

    for (int trial = 0; trial < num_trials; trial++) {
        // Varying error rate: higher in early trials
        float trial_error_rate = base_error_rate * (1.0f + (num_trials - trial) * 0.05f);

        // Start of trial: set attention gate
        float attention_gate = 0.3f + trial * 0.03f;
        simulate_attention_step(attention_gate, trial_error_rate);

        for (int step = 0; step < steps_per_trial; step++) {
            // Compute reward based on trial and step
            float reward = (step < steps_per_trial / 2) ? 1.0f : -0.5f;
            reward *= 0.5f + trial * 0.025f;  // Reward increases with trial

            // Learning step with fault injection
            simulate_learning_step(reward, trial_error_rate);
        }

        // End of trial: update systems
        neuromodulator_update(neuromod_system, 0.1f);
    }

    // Verify workflow completed
    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(stdp_utils, &metrics);
    EXPECT_TRUE(result);
    EXPECT_GT(metrics.total_ltp_events, 0u);

    // Verify exceptions were handled but didn't stop execution
    EXPECT_GT(total_exceptions.load(), 0);

    // Verify all systems remain operational
    float dopamine = neuromodulator_release_dopamine(neuromod_system, 1.0f, 0.5f);
    EXPECT_GT(dopamine, 0.0f);

    bool connected = stdp_pr_bridge_is_connected(stdp_pr);
    EXPECT_TRUE(connected);
}

//=============================================================================
// Triplet STDP Immune Bridge E2E Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, TripletStdpImmuneExceptionStress) {
    // WHAT: Test Triplet STDP-Immune bridge exception handling under stress
    // WHY:  Verify exception handling doesn't cause system instability

    total_exceptions = 0;

    // Generate heavy exception load from NULL bridge operations
    for (int i = 0; i < 500; i++) {
        triplet_stdp_immune_bridge_update(nullptr, i);
        triplet_stdp_immune_apply_cytokine_effects(nullptr);
        triplet_stdp_immune_detect_instability(nullptr);

        triplet_stdp_modulation_state_t modulation;
        triplet_stdp_immune_get_modulation_state(nullptr, &modulation);

        uint32_t antigen_id;
        triplet_stdp_immune_alert_instability(nullptr, &antigen_id);
    }

    // Verify exceptions were raised
    EXPECT_GE(total_exceptions.load(), 2500);
}

TEST_F(PlasticityModuleExceptionE2ETest, TripletStdpImmuneMixedWorkload) {
    // WHAT: Test mixed valid/invalid operations for Triplet STDP Immune bridge
    // WHY:  Real systems have mixed operations with occasional errors

    total_exceptions = 0;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const float error_probability = 0.1f;

    // Test config validation
    triplet_stdp_immune_config_t config;
    triplet_stdp_immune_default_config(&config);  // Valid

    for (int i = 0; i < 300; i++) {
        if (dist(rng) < error_probability) {
            triplet_stdp_immune_default_config(nullptr);  // Exception
        } else {
            triplet_stdp_immune_default_config(&config);  // Valid
        }
    }

    EXPECT_GT(total_exceptions.load(), 0);
}

//=============================================================================
// Triplet STDP Sleep Bridge E2E Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, TripletStdpSleepExceptionStress) {
    // WHAT: Test Triplet STDP-Sleep bridge exception handling under stress
    // WHY:  Verify exception handling doesn't cause system instability

    total_exceptions = 0;

    // Generate heavy exception load from NULL bridge operations
    for (int i = 0; i < 500; i++) {
        triplet_stdp_sleep_update(nullptr);

        triplet_stdp_sleep_effects_t effects;
        triplet_stdp_sleep_get_effects(nullptr, &effects);

        triplet_stdp_synapse_t synapse;
        triplet_stdp_sleep_apply_modulation(nullptr, &synapse);
    }

    // Verify exceptions were raised
    EXPECT_GE(total_exceptions.load(), 1500);
}

TEST_F(PlasticityModuleExceptionE2ETest, TripletStdpSleepMixedWorkload) {
    // WHAT: Test mixed valid/invalid operations for Triplet STDP Sleep bridge
    // WHY:  Real systems have mixed operations with occasional errors

    total_exceptions = 0;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const float error_probability = 0.1f;

    // Test config validation
    triplet_stdp_sleep_config_t config;
    triplet_stdp_sleep_default_config(&config);  // Valid

    for (int i = 0; i < 300; i++) {
        if (dist(rng) < error_probability) {
            triplet_stdp_sleep_default_config(nullptr);  // Exception
        } else {
            triplet_stdp_sleep_default_config(&config);  // Valid
        }
    }

    EXPECT_GT(total_exceptions.load(), 0);
}

//=============================================================================
// Neuromodulators Sleep Bridge E2E Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, NeuromodSleepExceptionStress) {
    // WHAT: Test neuromodulator-sleep bridge exception handling under stress
    // WHY:  Verify exception handling doesn't cause system instability

    total_exceptions = 0;

    // Generate heavy exception load from NULL bridge operations
    for (int i = 0; i < 500; i++) {
        neuromod_sleep_update(nullptr);
        neuromod_sleep_apply_modulation(nullptr);

        neuromod_sleep_effects_t effects;
        neuromod_sleep_get_effects(nullptr, &effects);
    }

    // Verify exceptions were raised
    EXPECT_GE(total_exceptions.load(), 1500);
}

TEST_F(PlasticityModuleExceptionE2ETest, NeuromodSleepMixedWorkload) {
    // WHAT: Test mixed valid/invalid operations for neuromod sleep bridge
    // WHY:  Real systems have mixed operations with occasional errors

    total_exceptions = 0;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const float error_probability = 0.1f;

    // Test config validation
    neuromodulators_sleep_config_t config;
    neuromod_sleep_default_config(&config);  // Valid

    for (int i = 0; i < 300; i++) {
        if (dist(rng) < error_probability) {
            neuromod_sleep_default_config(nullptr);  // Exception
        } else {
            neuromod_sleep_default_config(&config);  // Valid
        }
    }

    EXPECT_GT(total_exceptions.load(), 0);
}

//=============================================================================
// Eligibility PR Bridge E2E Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, EligibilityPRExceptionStress) {
    // WHAT: Test eligibility-PR bridge exception handling under stress
    // WHY:  Verify exception handling doesn't cause system instability

    total_exceptions = 0;

    // Generate heavy exception load from NULL bridge operations
    for (int i = 0; i < 500; i++) {
        elig_pr_forward_effect_t effect;
        elig_pr_apply_consolidation_gate(nullptr, i, 0.5f, 0.5f, &effect);

        bool should_promote;
        elig_pr_check_tier_promotion(nullptr, i, 0.5f, 0.5f, &should_promote);

        float delta;
        elig_pr_apply_entanglement_update(nullptr, i, i+1, 0.5f, &delta);

        float modulated;
        elig_pr_get_decay_modulation(nullptr, 0.5f, 0.95f, &modulated);

        elig_pr_bridge_update(nullptr, 0.1f);
    }

    // Verify exceptions were raised
    EXPECT_GE(total_exceptions.load(), 2500);
}

TEST_F(PlasticityModuleExceptionE2ETest, EligibilityPRWithValidBridge) {
    // WHAT: Test eligibility-PR bridge with valid bridge and occasional NULL faults
    // WHY:  Real systems have mixed operations with occasional errors

    elig_pr_bridge_config_t config = elig_pr_bridge_default_config();
    elig_pr_bridge_t bridge = elig_pr_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    total_exceptions = 0;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    const float error_probability = 0.1f;

    for (int i = 0; i < 200; i++) {
        elig_pr_forward_effect_t effect;
        if (dist(rng) < error_probability) {
            elig_pr_apply_consolidation_gate(nullptr, i, 0.5f, 0.5f, &effect);
        } else {
            elig_pr_apply_consolidation_gate(bridge, i, 0.5f, 0.5f, &effect);
        }
    }

    EXPECT_GT(total_exceptions.load(), 0);

    // Verify bridge still works
    elig_pr_bridge_state_t state;
    int result = elig_pr_bridge_get_state(bridge, &state);
    EXPECT_EQ(result, 0);

    elig_pr_bridge_destroy(bridge);
}

//=============================================================================
// Cross-Module E2E Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionE2ETest, AllModulesExceptionStressCombined) {
    // WHAT: Test all enhanced modules under combined exception stress
    // WHY:  End-to-end validation of cross-module exception handling

    total_exceptions = 0;

    // Combined stress test on all modules
    for (int i = 0; i < 200; i++) {
        // Triplet STDP Immune bridge
        triplet_stdp_immune_bridge_update(nullptr, i);
        triplet_stdp_immune_apply_cytokine_effects(nullptr);

        // Triplet STDP Sleep bridge
        triplet_stdp_sleep_update(nullptr);
        triplet_stdp_sleep_effects_t sleep_effects;
        triplet_stdp_sleep_get_effects(nullptr, &sleep_effects);

        // Neuromod Sleep bridge
        neuromod_sleep_update(nullptr);
        neuromod_sleep_apply_modulation(nullptr);

        // Eligibility PR bridge
        elig_pr_forward_effect_t effect;
        elig_pr_apply_consolidation_gate(nullptr, i, 0.5f, 0.5f, &effect);
        elig_pr_bridge_update(nullptr, 0.1f);
    }

    // Verify many exceptions were raised across all modules
    EXPECT_GE(total_exceptions.load(), 1600);
}

TEST_F(PlasticityModuleExceptionE2ETest, EligibilityPRWithBridgeRecovery) {
    // WHAT: Test eligibility-PR bridge recovery after heavy exception load
    // WHY:  Eligibility trace system must remain stable for learning

    elig_pr_bridge_config_t config = elig_pr_bridge_default_config();
    elig_pr_bridge_t bridge = elig_pr_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Phase 1: Normal operation
    for (int i = 0; i < 50; i++) {
        elig_pr_forward_effect_t effect;
        elig_pr_apply_consolidation_gate(bridge, i, 0.5f, 0.5f, &effect);
    }

    // Phase 2: Heavy exception load
    total_exceptions = 0;
    for (int i = 0; i < 500; i++) {
        elig_pr_forward_effect_t effect;
        elig_pr_apply_consolidation_gate(nullptr, i, 0.5f, 0.5f, &effect);

        bool should_promote;
        elig_pr_check_tier_promotion(nullptr, i, 0.5f, 0.5f, &should_promote);

        float delta;
        elig_pr_apply_entanglement_update(nullptr, i, i+1, 0.5f, &delta);

        float modulated;
        elig_pr_get_decay_modulation(nullptr, 0.5f, 0.95f, &modulated);
    }
    EXPECT_GE(total_exceptions.load(), 2000);

    // Phase 3: Verify recovery
    elig_pr_forward_effect_t effect;
    int result = elig_pr_apply_consolidation_gate(bridge, 100, 0.5f, 0.5f, &effect);
    EXPECT_EQ(result, 0);

    bool should_promote;
    result = elig_pr_check_tier_promotion(bridge, 100, 0.5f, 0.5f, &should_promote);
    EXPECT_EQ(result, 0);

    elig_pr_bridge_state_t state;
    result = elig_pr_bridge_get_state(bridge, &state);
    EXPECT_EQ(result, 0);

    elig_pr_bridge_destroy(bridge);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
