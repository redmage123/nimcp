/**
 * @file test_plasticity_module_exception_integration.cpp
 * @brief Integration tests for plasticity module NIMCP_THROW_TO_IMMUNE exception handling
 *
 * WHAT: Test cross-module exception handling with immune system integration
 * WHY:  Verify complete exception flow from plasticity modules through immune system
 * HOW:  Test neuromodulator-STDP interactions and exception recovery
 *
 * INTEGRATION SCENARIOS:
 * - Neuromodulator error -> STDP modulation disruption -> Recovery
 * - Attention allocation failure -> Learning rate adjustment
 * - STDP-PR bridge disconnection -> Coherence degradation
 * - STDP utils buffer overflow -> Metric preservation
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

// Include C++ compatible headers first (may include CUDA)
#include "cognitive/immune/nimcp_brain_immune.h"
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

class PlasticityModuleExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<int> immune_presentation_count;
    static std::atomic<nimcp_error_t> last_error_code;
    static std::atomic<int> recovery_action_count;

    brain_immune_system_t* immune_system;
    neuromodulator_system_t neuromod_system;
    stdp_pr_bridge_t stdp_pr;
    stdp_utils_ctx_t stdp_utils;

    void SetUp() override {
        exception_count = 0;
        immune_presentation_count = 0;
        last_error_code = NIMCP_SUCCESS;
        recovery_action_count = 0;

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

        register_handlers();
    }

    void TearDown() override {
        unregister_handlers();

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
    static nimcp_handler_registration_t* immune_handler_reg;
    static nimcp_handler_registration_t* recovery_handler_reg;

    static bool exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_count++;
        last_error_code = ex->code;
        return false;
    }

    static bool immune_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        immune_presentation_count++;

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        return false;
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

    void register_handlers() {
        nimcp_handler_options_t opts;

        nimcp_handler_default_options(&opts);
        opts.name = "exception_counter";
        opts.handler = exception_handler;
        opts.priority = 150;
        exception_handler_reg = nimcp_handler_register(&opts);

        nimcp_handler_default_options(&opts);
        opts.name = "immune_presenter";
        opts.handler = immune_handler;
        opts.priority = 100;
        immune_handler_reg = nimcp_handler_register(&opts);

        nimcp_handler_default_options(&opts);
        opts.name = "recovery_tracker";
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

    void reset_counters() {
        exception_count = 0;
        immune_presentation_count = 0;
        last_error_code = NIMCP_SUCCESS;
        recovery_action_count = 0;
    }
};

std::atomic<int> PlasticityModuleExceptionIntegrationTest::exception_count(0);
std::atomic<int> PlasticityModuleExceptionIntegrationTest::immune_presentation_count(0);
std::atomic<nimcp_error_t> PlasticityModuleExceptionIntegrationTest::last_error_code(NIMCP_SUCCESS);
std::atomic<int> PlasticityModuleExceptionIntegrationTest::recovery_action_count(0);
nimcp_handler_registration_t* PlasticityModuleExceptionIntegrationTest::exception_handler_reg = nullptr;
nimcp_handler_registration_t* PlasticityModuleExceptionIntegrationTest::immune_handler_reg = nullptr;
nimcp_handler_registration_t* PlasticityModuleExceptionIntegrationTest::recovery_handler_reg = nullptr;

//=============================================================================
// Neuromodulator-STDP Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, NeuromodulatorExceptionAffectsSTDPModulation) {
    // WHAT: Test that neuromodulator exception affects STDP modulation flow
    // WHY:  STDP depends on neuromodulator levels; errors should propagate

    ASSERT_NE(neuromod_system, nullptr);
    ASSERT_NE(stdp_pr, nullptr);

    reset_counters();

    // First, verify normal operation works
    float dopamine = neuromodulator_release_dopamine(neuromod_system, 1.0f, 0.5f);
    EXPECT_GT(dopamine, 0.0f);

    // Now trigger exception by passing NULL
    float result = neuromodulator_release_dopamine(nullptr, 1.0f, 0.5f);
    EXPECT_EQ(result, 0.0f);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_GE(immune_presentation_count.load(), 1);

    // STDP-PR bridge should still be usable after neuromodulator error
    bool connected = stdp_pr_bridge_is_connected(stdp_pr);
    EXPECT_TRUE(connected);  // Bridge shouldn't be affected by neuromod error
}

TEST_F(PlasticityModuleExceptionIntegrationTest, NeuromodulatorComputeEffectsErrorChain) {
    // WHAT: Test compute effects error propagation
    // WHY:  Effects computation affects learning rate modulation

    ASSERT_NE(neuromod_system, nullptr);

    reset_counters();

    // Trigger NULL profile error
    modulation_effects_t effects = modulation_effects_create();
    bool result = neuromodulator_compute_effects(neuromod_system, nullptr, &effects);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    modulation_effects_destroy(&effects);
}

//=============================================================================
// STDP-PR Bridge Exception Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, StdpPrBridgeExceptionChainToImmune) {
    // WHAT: Test STDP-PR bridge exception flows to immune system
    // WHY:  Bridge errors should trigger immune response

    reset_counters();

    // Trigger exception through NULL bridge
    stdp_pr_forward_effect_t events[1] = {};
    int result = stdp_pr_notify_batch(nullptr, events, 1);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_GE(immune_presentation_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, StdpPrInvalidWeightRecovery) {
    // WHAT: Test recovery from invalid weight change parameters
    // WHY:  Invalid weights should trigger recovery actions

    ASSERT_NE(stdp_pr, nullptr);

    reset_counters();

    // Try LTP with negative weight (invalid)
    int result = stdp_pr_notify_ltp(stdp_pr, 1, 2, -0.5f, nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);

    // Bridge should still be functional after error
    bool connected = stdp_pr_bridge_is_connected(stdp_pr);
    EXPECT_TRUE(connected);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, StdpPrCoherenceMonitoringDuringErrors) {
    // WHAT: Test that coherence monitoring works during error conditions
    // WHY:  Coherence should degrade gracefully on errors

    ASSERT_NE(stdp_pr, nullptr);

    // Get baseline coherence
    float initial_coherence = stdp_pr_bridge_get_coherence(stdp_pr);
    EXPECT_GE(initial_coherence, 0.0f);

    reset_counters();

    // Trigger multiple errors
    for (int i = 0; i < 5; i++) {
        stdp_pr_notify_ltp(nullptr, 1, 2, 0.1f, nullptr);
    }

    EXPECT_GE(exception_count.load(), 5);

    // Coherence should still be retrievable
    float current_coherence = stdp_pr_bridge_get_coherence(stdp_pr);
    EXPECT_GE(current_coherence, 0.0f);
}

//=============================================================================
// STDP Utils Bridge Exception Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, StdpUtilsExceptionFlowToImmune) {
    // WHAT: Test STDP utils exception flows to immune system
    // WHY:  Utils errors should trigger immune response

    reset_counters();

    // Trigger exception through NULL context
    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(nullptr, &metrics);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_GE(immune_presentation_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, StdpUtilsMetricPreservationOnError) {
    // WHAT: Test that metrics are preserved when errors occur
    // WHY:  Metrics should not be lost due to unrelated errors

    ASSERT_NE(stdp_utils, nullptr);

    // Record some valid data first
    stdp_utils_record_ltp(stdp_utils, 0.1f, 10.0f);
    stdp_utils_record_ltd(stdp_utils, -0.05f, -5.0f);

    stdp_metrics_t metrics_before;
    bool result = stdp_utils_get_metrics(stdp_utils, &metrics_before);
    EXPECT_TRUE(result);

    reset_counters();

    // Trigger errors on a NULL context (shouldn't affect our valid context)
    stdp_utils_record_ltp(nullptr, 0.1f, 10.0f);
    EXPECT_GE(exception_count.load(), 1);

    // Verify metrics on our valid context are preserved
    stdp_metrics_t metrics_after;
    result = stdp_utils_get_metrics(stdp_utils, &metrics_after);
    EXPECT_TRUE(result);

    // Metrics should be unchanged
    EXPECT_EQ(metrics_before.total_ltp_events, metrics_after.total_ltp_events);
    EXPECT_EQ(metrics_before.total_ltd_events, metrics_after.total_ltd_events);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, StdpUtilsExportErrorHandling) {
    // WHAT: Test export function error handling
    // WHY:  Export errors should not corrupt internal state

    ASSERT_NE(stdp_utils, nullptr);

    reset_counters();

    // Try export with NULL filename
    bool result = stdp_utils_export_csv(stdp_utils, nullptr);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    // Context should still be functional
    stdp_utils_record_ltp(stdp_utils, 0.1f, 10.0f);

    stdp_metrics_t metrics;
    result = stdp_utils_get_metrics(stdp_utils, &metrics);
    EXPECT_TRUE(result);
}

//=============================================================================
// Cross-Module Exception Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, CrossModuleExceptionCascade) {
    // WHAT: Test exception handling across multiple modules
    // WHY:  Real systems have interacting modules

    reset_counters();

    // Simulate cascade: Neuromodulator -> STDP-PR -> STDP Utils
    neuromodulator_pool_get_dopamine(nullptr);
    int ex1 = exception_count.load();
    EXPECT_GE(ex1, 1);

    stdp_pr_bridge_is_connected(nullptr);
    int ex2 = exception_count.load();
    EXPECT_GT(ex2, ex1);

    stdp_utils_reset(nullptr);
    int ex3 = exception_count.load();
    EXPECT_GT(ex3, ex2);

    // All handlers should have been called for each exception
    EXPECT_GE(immune_presentation_count.load(), 3);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, MixedValidInvalidOperations) {
    // WHAT: Test interleaved valid and invalid operations
    // WHY:  Real code mixes correct and erroneous paths

    ASSERT_NE(neuromod_system, nullptr);
    ASSERT_NE(stdp_pr, nullptr);
    ASSERT_NE(stdp_utils, nullptr);

    reset_counters();

    // Valid operation
    float dopamine = neuromodulator_release_dopamine(neuromod_system, 1.0f, 0.5f);
    EXPECT_GT(dopamine, 0.0f);
    EXPECT_EQ(exception_count.load(), 0);

    // Invalid operation
    neuromodulator_release_dopamine(nullptr, 1.0f, 0.5f);
    EXPECT_GE(exception_count.load(), 1);

    // Valid operation again
    bool connected = stdp_pr_bridge_is_connected(stdp_pr);
    EXPECT_TRUE(connected);

    // Invalid operation
    stdp_pr_bridge_is_connected(nullptr);
    EXPECT_GE(exception_count.load(), 2);

    // Valid operation again
    stdp_utils_record_ltp(stdp_utils, 0.1f, 10.0f);

    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(stdp_utils, &metrics);
    EXPECT_TRUE(result);
    EXPECT_GT(metrics.total_ltp_events, 0u);
}

//=============================================================================
// Concurrent Exception Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, ConcurrentModuleExceptions) {
    // WHAT: Test concurrent exceptions from multiple modules
    // WHY:  Modules run in parallel threads

    const int num_threads = 4;
    const int ops_per_thread = 25;
    std::atomic<int> total_ops{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &total_ops, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                switch ((t + i) % 4) {
                    case 0:
                        neuromodulator_pool_get_dopamine(nullptr);
                        break;
                    case 1:
                        stdp_pr_bridge_is_connected(nullptr);
                        break;
                    case 2:
                        stdp_utils_reset(nullptr);
                        break;
                    case 3: {
                        stdp_metrics_t metrics;
                        stdp_utils_get_metrics(nullptr, &metrics);
                        break;
                    }
                }
                total_ops++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_ops.load(), num_threads * ops_per_thread);
    EXPECT_GE(exception_count.load(), num_threads * ops_per_thread);
    EXPECT_GE(immune_presentation_count.load(), num_threads * ops_per_thread);
}

//=============================================================================
// Recovery Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, RecoveryAfterMultipleExceptions) {
    // WHAT: Test system recovery after multiple exceptions
    // WHY:  System should remain functional after errors

    ASSERT_NE(neuromod_system, nullptr);
    ASSERT_NE(stdp_pr, nullptr);
    ASSERT_NE(stdp_utils, nullptr);

    reset_counters();

    // Generate many exceptions
    for (int i = 0; i < 50; i++) {
        neuromodulator_pool_get_dopamine(nullptr);
        stdp_pr_bridge_is_connected(nullptr);
        stdp_utils_reset(nullptr);
    }

    EXPECT_GE(exception_count.load(), 150);

    // Now verify all systems are still functional
    float dopamine = neuromodulator_release_dopamine(neuromod_system, 1.0f, 0.5f);
    EXPECT_GT(dopamine, 0.0f);

    bool connected = stdp_pr_bridge_is_connected(stdp_pr);
    EXPECT_TRUE(connected);

    stdp_utils_record_ltp(stdp_utils, 0.1f, 10.0f);
    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(stdp_utils, &metrics);
    EXPECT_TRUE(result);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, GracefulDegradationUnderStress) {
    // WHAT: Test graceful degradation under exception stress
    // WHY:  System should degrade gracefully, not fail catastrophically

    ASSERT_NE(neuromod_system, nullptr);
    ASSERT_NE(stdp_utils, nullptr);

    reset_counters();

    // Mix of valid operations and exceptions
    for (int cycle = 0; cycle < 10; cycle++) {
        // Valid operation
        neuromodulator_release_dopamine(neuromod_system, 1.0f, 0.5f);
        stdp_utils_record_ltp(stdp_utils, 0.1f * (cycle + 1), 10.0f);

        // Exception triggers
        neuromodulator_pool_get_dopamine(nullptr);
        stdp_utils_reset(nullptr);
    }

    // Verify data was recorded correctly despite exceptions
    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(stdp_utils, &metrics);
    EXPECT_TRUE(result);
    EXPECT_GE(metrics.total_ltp_events, 10u);
}

//=============================================================================
// Memory Leak Prevention Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, NoMemoryLeakOnExceptionPaths) {
    // WHAT: Test that exception paths don't leak memory
    // WHY:  Exception handling must clean up properly

    const int iterations = 1000;

    for (int i = 0; i < iterations; i++) {
        // These should not leak memory
        neuromodulator_pool_get_dopamine(nullptr);
        stdp_pr_bridge_is_connected(nullptr);
        stdp_utils_reset(nullptr);

        stdp_metrics_t metrics;
        stdp_utils_get_metrics(nullptr, &metrics);
    }

    // If we get here without crash or memory errors, test passed
    SUCCEED();
}

//=============================================================================
// Immune Response Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, ImmuneResponseToPlasticityException) {
    // WHAT: Test immune system response to plasticity exceptions
    // WHY:  Immune system should learn from plasticity errors

    ASSERT_NE(immune_system, nullptr);

    reset_counters();

    // Trigger exception
    neuromodulator_pool_get_dopamine(nullptr);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_GE(immune_presentation_count.load(), 1);

    // Immune system should have processed the exception
    // (specific immune response depends on configuration)
}

TEST_F(PlasticityModuleExceptionIntegrationTest, BatchExceptionImmuneProcessing) {
    // WHAT: Test immune processing of batch exceptions
    // WHY:  Multiple exceptions should be processed efficiently

    reset_counters();

    // Generate batch of exceptions
    const int batch_size = 20;
    for (int i = 0; i < batch_size; i++) {
        switch (i % 4) {
            case 0: neuromodulator_pool_get_dopamine(nullptr); break;
            case 1: stdp_pr_bridge_is_connected(nullptr); break;
            case 2: stdp_utils_reset(nullptr); break;
            case 3: {
                stdp_metrics_t metrics;
                stdp_utils_get_metrics(nullptr, &metrics);
                break;
            }
        }
    }

    EXPECT_GE(exception_count.load(), batch_size);
    EXPECT_GE(immune_presentation_count.load(), batch_size);
}

//=============================================================================
// Triplet STDP Immune Bridge Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, TripletStdpImmuneExceptionFlow) {
    // WHAT: Test triplet STDP immune bridge exception flow to immune system
    // WHY:  Verify exception handling works across triplet-immune integration

    reset_counters();

    // Trigger NULL bridge exceptions
    triplet_stdp_immune_bridge_update(nullptr, 10);
    triplet_stdp_immune_apply_cytokine_effects(nullptr);
    triplet_stdp_immune_apply_inflammation_effects(nullptr);

    EXPECT_GE(exception_count.load(), 3);
    EXPECT_GE(immune_presentation_count.load(), 3);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, TripletStdpImmuneConfigException) {
    // WHAT: Test default config exception for triplet STDP immune bridge
    // WHY:  Configuration functions must validate inputs

    reset_counters();

    int result = triplet_stdp_immune_default_config(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Triplet STDP Sleep Bridge Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, TripletStdpSleepExceptionFlow) {
    // WHAT: Test triplet STDP sleep bridge exception flow
    // WHY:  Verify sleep modulation exceptions are properly handled

    reset_counters();

    triplet_stdp_sleep_update(nullptr);
    triplet_stdp_sleep_effects_t effects;
    triplet_stdp_sleep_get_effects(nullptr, &effects);
    triplet_stdp_synapse_t synapse;
    triplet_stdp_sleep_apply_modulation(nullptr, &synapse);

    EXPECT_GE(exception_count.load(), 3);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, TripletStdpSleepConfigException) {
    // WHAT: Test default config exception for triplet STDP sleep bridge
    // WHY:  Configuration must be validated

    reset_counters();

    int result = triplet_stdp_sleep_default_config(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
}

//=============================================================================
// Neuromodulators Sleep Bridge Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, NeuromodSleepExceptionFlow) {
    // WHAT: Test neuromodulator sleep bridge exception flow
    // WHY:  Verify sleep-neuromodulator integration handles errors

    reset_counters();

    neuromod_sleep_update(nullptr);
    neuromod_sleep_apply_modulation(nullptr);
    neuromod_sleep_effects_t effects;
    neuromod_sleep_get_effects(nullptr, &effects);

    EXPECT_GE(exception_count.load(), 3);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, NeuromodSleepConfigException) {
    // WHAT: Test default config exception for neuromodulator sleep bridge
    // WHY:  Configuration validation is essential

    reset_counters();

    int result = neuromod_sleep_default_config(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
}

//=============================================================================
// Eligibility PR Bridge Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, EligibilityPRExceptionFlow) {
    // WHAT: Test eligibility-PR bridge exception flow
    // WHY:  Verify eligibility trace errors are properly reported

    reset_counters();

    elig_pr_forward_effect_t forward_effect;
    elig_pr_apply_consolidation_gate(nullptr, 1, 0.5f, 0.5f, &forward_effect);

    bool should_promote;
    elig_pr_check_tier_promotion(nullptr, 1, 0.5f, 0.5f, &should_promote);

    float delta;
    elig_pr_apply_entanglement_update(nullptr, 1, 2, 0.5f, &delta);

    float modulated_lambda;
    elig_pr_get_decay_modulation(nullptr, 0.5f, 0.95f, &modulated_lambda);

    EXPECT_GE(exception_count.load(), 4);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, EligibilityPRBridgeStateException) {
    // WHAT: Test bridge state/stats exception handling
    // WHY:  State queries must validate inputs

    reset_counters();

    elig_pr_bridge_state_t state;
    elig_pr_bridge_get_state(nullptr, &state);

    elig_pr_bridge_stats_t stats;
    elig_pr_bridge_get_stats(nullptr, &stats);

    elig_pr_bridge_reset_stats(nullptr);
    elig_pr_bridge_update(nullptr, 1.0f);

    EXPECT_GE(exception_count.load(), 4);
}

TEST_F(PlasticityModuleExceptionIntegrationTest, EligibilityPRBridgeWithValidBridge) {
    // WHAT: Test exception handling with valid bridge but NULL outputs
    // WHY:  Both bridge and output parameters must be valid

    elig_pr_bridge_config_t config = elig_pr_bridge_default_config();
    elig_pr_bridge_t bridge = elig_pr_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    reset_counters();

    // Valid bridge, NULL output
    int result = elig_pr_apply_consolidation_gate(bridge, 1, 0.5f, 0.5f, nullptr);
    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    elig_pr_bridge_destroy(bridge);
}

//=============================================================================
// Cross-Module Exception Integration Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionIntegrationTest, MultiModuleExceptionBatch) {
    // WHAT: Test exceptions from multiple newly enhanced modules
    // WHY:  Verify cross-module exception handling works correctly

    reset_counters();

    const int iterations = 5;
    for (int i = 0; i < iterations; i++) {
        // Triplet STDP immune
        triplet_stdp_immune_bridge_update(nullptr, 10);

        // Triplet STDP sleep
        triplet_stdp_sleep_update(nullptr);

        // Neuromod sleep
        neuromod_sleep_update(nullptr);

        // Eligibility PR
        elig_pr_bridge_update(nullptr, 1.0f);
    }

    // Should have 4 * iterations exceptions
    EXPECT_GE(exception_count.load(), 4 * iterations);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
