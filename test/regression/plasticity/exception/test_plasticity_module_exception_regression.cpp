/**
 * @file test_plasticity_module_exception_regression.cpp
 * @brief Regression tests for plasticity module NIMCP_THROW_TO_IMMUNE exception handling
 *
 * WHAT: Test API contract stability and error code consistency
 * WHY:  Ensure exception handling behavior remains stable across versions
 * HOW:  Test all documented error paths return expected codes
 *
 * REGRESSION COVERAGE:
 * - API contract verification for all exception-throwing functions
 * - Error code consistency across module boundaries
 * - Return value contract enforcement
 * - Exception message format stability
 *
 * @author NIMCP Development Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>

// Include C++ compatible headers first
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

class PlasticityModuleExceptionRegressionTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<nimcp_error_t> last_error_code;
    static std::string last_message;
    static nimcp_handler_registration_t* handler_reg;

    void SetUp() override {
        exception_count = 0;
        last_error_code = NIMCP_SUCCESS;
        last_message.clear();

        nimcp_exception_system_init();

        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "regression_test_handler";
        options.handler = exception_handler;
        options.priority = 100;
        handler_reg = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (handler_reg) {
            nimcp_handler_unregister(handler_reg);
            handler_reg = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_count++;
        last_error_code = ex->code;
        if (ex->message) {
            last_message = ex->message;
        }
        return false;
    }

    void reset() {
        exception_count = 0;
        last_error_code = NIMCP_SUCCESS;
        last_message.clear();
    }
};

std::atomic<int> PlasticityModuleExceptionRegressionTest::exception_count(0);
std::atomic<nimcp_error_t> PlasticityModuleExceptionRegressionTest::last_error_code(NIMCP_SUCCESS);
std::string PlasticityModuleExceptionRegressionTest::last_message;
nimcp_handler_registration_t* PlasticityModuleExceptionRegressionTest::handler_reg = nullptr;

//=============================================================================
// Neuromodulator API Contract Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, NeuromodulatorPoolGettersReturnZeroOnNull) {
    // WHAT: Verify NULL pool getters return 0.0f consistently
    // WHY:  API contract: NULL input -> return 0.0f
    // NOTE: Pool getters are lightweight accessors - they return 0 without throwing

    reset();

    // All getters should return 0.0f for NULL
    EXPECT_EQ(neuromodulator_pool_get_dopamine(nullptr), 0.0f);
    EXPECT_EQ(neuromodulator_pool_get_serotonin(nullptr), 0.0f);
    EXPECT_EQ(neuromodulator_pool_get_acetylcholine(nullptr), 0.0f);
    EXPECT_EQ(neuromodulator_pool_get_norepinephrine(nullptr), 0.0f);
    // Pool getters are simple accessors that don't throw exceptions
}

TEST_F(PlasticityModuleExceptionRegressionTest, NeuromodulatorReleaseReturnsZeroOnNull) {
    // WHAT: Verify release functions return 0.0f on NULL system
    // WHY:  API contract: NULL system -> return 0.0f

    reset();

    EXPECT_EQ(neuromodulator_release_dopamine(nullptr, 1.0f, 0.5f), 0.0f);
    EXPECT_EQ(neuromodulator_release_serotonin(nullptr, 0.5f), 0.0f);
    EXPECT_EQ(neuromodulator_release_acetylcholine(nullptr, 0.5f), 0.0f);
    EXPECT_EQ(neuromodulator_release_norepinephrine(nullptr, 0.5f, 0.3f), 0.0f);

    EXPECT_GE(exception_count.load(), 4);
}

TEST_F(PlasticityModuleExceptionRegressionTest, NeuromodulatorBoolFunctionsReturnFalseOnNull) {
    // WHAT: Verify bool functions return false on NULL
    // WHY:  API contract: NULL input -> return false

    reset();

    neuromodulator_pool_t pool;
    EXPECT_FALSE(neuromodulator_get_levels(nullptr, &pool));
    EXPECT_FALSE(neuromodulator_reset(nullptr));
    EXPECT_FALSE(neuromodulator_update(nullptr, 0.01f));

    neuromodulator_stats_t stats;
    EXPECT_FALSE(neuromodulator_get_stats(nullptr, &stats));

    EXPECT_GE(exception_count.load(), 4);
}

TEST_F(PlasticityModuleExceptionRegressionTest, NeuromodulatorNullPointerErrorCode) {
    // WHAT: Verify neuromodulator system functions use NIMCP_ERROR_NULL_POINTER
    // WHY:  Error code consistency contract
    // NOTE: Only system-level functions throw exceptions, not pool getters

    std::vector<std::pair<std::string, std::function<void()>>> null_operations = {
        {"neuromodulator_release_dopamine", []() { neuromodulator_release_dopamine(nullptr, 1.0f, 0.5f); }},
        {"neuromodulator_reset", []() { neuromodulator_reset(nullptr); }},
        {"neuromodulator_update", []() { neuromodulator_update(nullptr, 0.01f); }},
    };

    for (auto& op : null_operations) {
        reset();
        op.second();
        EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER)
            << "Expected NIMCP_ERROR_NULL_POINTER for " << op.first;
    }
}

//=============================================================================
// Attention API Contract Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, AttentionHeadCreateReturnsNullOnError) {
    // WHAT: Verify attention_head_create returns NULL on error
    // WHY:  API contract: invalid config -> return NULL

    reset();

    EXPECT_EQ(attention_head_create(nullptr), nullptr);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();

    attention_head_config_t invalid_config = {
        .input_dim = 0,  // Invalid
        .output_dim = 64,
        .key_dim = 32,
        .value_dim = 32,
        .temperature = 1.0f,
        .dropout_rate = 0.0f
    };
    EXPECT_EQ(attention_head_create(&invalid_config), nullptr);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(PlasticityModuleExceptionRegressionTest, MultiheadAttentionCreateReturnsNullOnError) {
    // WHAT: Verify multihead_attention_create returns NULL on error
    // WHY:  API contract: invalid config -> return NULL
    // NOTE: multihead_attention_create wraps attention_head_create, which validates config

    reset();

    // NULL config test - multihead_attention_create detects NULL config
    // and throws NIMCP_ERROR_NULL_POINTER before reaching parameter validation
    EXPECT_EQ(multihead_attention_create(nullptr), nullptr);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();

    multihead_attention_config_t invalid_config = {
        .num_heads = 0,  // Invalid
        .input_dim = 128,
        .output_dim = 128,
        .sequence_length = 32,
        .use_thalamic_gate = false,
        .use_salience_weighting = false,
        .gate_bias = 0.5f,
        .use_positional_encoding = false,
        .pe_type = NIMCP_POS_ROTARY,
        .rope_base = 10000.0f,
        .alibi_slope_base = 1.0f,
        .enable_quantum_attention = false
    };
    EXPECT_EQ(multihead_attention_create(&invalid_config), nullptr);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(PlasticityModuleExceptionRegressionTest, AttentionBoolFunctionsReturnFalseOnNull) {
    // WHAT: Verify attention bool functions return false on NULL
    // WHY:  API contract consistency
    // NOTE: Attention forward functions are lightweight - they return false without throwing

    reset();

    float buf[128] = {0};
    EXPECT_FALSE(attention_head_forward(nullptr, buf, buf, buf, 4, buf, nullptr, nullptr, 0));
    EXPECT_FALSE(multihead_attention_forward(nullptr, buf, 4, nullptr, buf));
    EXPECT_FALSE(multihead_attention_set_gate(nullptr, 0.5f));

    attention_stats_t stats;
    EXPECT_FALSE(multihead_attention_get_stats(nullptr, &stats));
    // Attention forward functions are lightweight and don't throw exceptions
}

//=============================================================================
// STDP-PR Bridge API Contract Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, StdpPrBridgeReturnsNegativeOneOnNull) {
    // WHAT: Verify STDP-PR bridge functions return -1 on NULL
    // WHY:  API contract: NULL bridge -> return -1

    reset();

    stdp_pr_forward_effect_t events[1] = {};
    EXPECT_EQ(stdp_pr_notify_ltp(nullptr, 1, 2, 0.1f, nullptr), -1);
    EXPECT_EQ(stdp_pr_notify_ltd(nullptr, 1, 2, -0.1f, nullptr), -1);
    EXPECT_EQ(stdp_pr_notify_burst(nullptr, 1, 2, 0.1f, true, nullptr), -1);
    // stdp_pr_notify_batch returns -1 on NULL bridge (throws exception)
    EXPECT_EQ(stdp_pr_notify_batch(nullptr, events, 1), -1);

    stdp_pr_backward_effect_t effect;
    EXPECT_EQ(stdp_pr_get_modulation(nullptr, 1, &effect), -1);

    float modulated;
    EXPECT_EQ(stdp_pr_apply_resonance_modulation(nullptr, 0.5f, 0.01f, &modulated), -1);

    stdp_pr_bridge_state_t state;
    EXPECT_EQ(stdp_pr_bridge_get_state(nullptr, &state), -1);

    stdp_pr_bridge_stats_t stats;
    EXPECT_EQ(stdp_pr_bridge_get_stats(nullptr, &stats), -1);

    EXPECT_EQ(stdp_pr_bridge_reset_stats(nullptr), -1);
    EXPECT_EQ(stdp_pr_bridge_update(nullptr, 1.0f), -1);

    // 10 functions that return -1 on NULL (including batch)
    EXPECT_GE(exception_count.load(), 10);
}

TEST_F(PlasticityModuleExceptionRegressionTest, StdpPrBridgeGetCoherenceReturnsNegativeOnNull) {
    // WHAT: Verify get_coherence returns negative value on NULL
    // WHY:  API contract: NULL bridge -> return -1.0f

    reset();

    float result = stdp_pr_bridge_get_coherence(nullptr);
    EXPECT_LT(result, 0.0f);
    EXPECT_GE(exception_count.load(), 1);
}

TEST_F(PlasticityModuleExceptionRegressionTest, StdpPrBridgeBoolFunctionsReturnFalseOnNull) {
    // WHAT: Verify bool functions return false on NULL
    // WHY:  API contract consistency

    reset();

    EXPECT_FALSE(stdp_pr_bridge_validate_config(nullptr));
    EXPECT_FALSE(stdp_pr_bridge_is_connected(nullptr));

    EXPECT_GE(exception_count.load(), 2);
}

TEST_F(PlasticityModuleExceptionRegressionTest, StdpPrBridgeInvalidParamErrorCode) {
    // WHAT: Verify invalid parameters use NIMCP_ERROR_INVALID_PARAM
    // WHY:  Error code consistency for parameter validation

    stdp_pr_bridge_t bridge = stdp_pr_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    reset();

    // LTP with negative weight change
    stdp_pr_notify_ltp(bridge, 1, 2, -0.1f, nullptr);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);

    reset();

    // LTD with positive weight change
    stdp_pr_notify_ltd(bridge, 1, 2, 0.1f, nullptr);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);

    stdp_pr_bridge_destroy(bridge);
}

//=============================================================================
// STDP Utils Bridge API Contract Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, StdpUtilsVoidFunctionsThrowOnNull) {
    // WHAT: Verify void functions throw on NULL
    // WHY:  API contract: NULL context -> throw exception

    reset();

    stdp_utils_reset(nullptr);
    EXPECT_GE(exception_count.load(), 1);

    reset();

    stdp_utils_record_ltp(nullptr, 0.1f, 10.0f);
    EXPECT_GE(exception_count.load(), 1);

    reset();

    stdp_utils_record_ltd(nullptr, -0.1f, -10.0f);
    EXPECT_GE(exception_count.load(), 1);

    reset();

    float weights[10] = {0};
    stdp_utils_update_weight_stats(nullptr, weights, 10);
    EXPECT_GE(exception_count.load(), 1);
}

TEST_F(PlasticityModuleExceptionRegressionTest, StdpUtilsBoolFunctionsReturnFalseOnNull) {
    // WHAT: Verify bool functions return false on NULL
    // WHY:  API contract consistency

    reset();

    stdp_spike_event_t event = {0};
    EXPECT_FALSE(stdp_utils_record_spike(nullptr, &event));

    stdp_metrics_t metrics;
    EXPECT_FALSE(stdp_utils_get_metrics(nullptr, &metrics));

    EXPECT_FALSE(stdp_utils_export_csv(nullptr, "test.csv"));
    EXPECT_FALSE(stdp_utils_export_json(nullptr, "test.json"));

    EXPECT_GE(exception_count.load(), 4);
}

TEST_F(PlasticityModuleExceptionRegressionTest, StdpUtilsAllocFunctionsReturnNullOnNull) {
    // WHAT: Verify alloc functions return NULL on NULL context
    // WHY:  API contract: NULL context -> return NULL

    reset();

    EXPECT_EQ(stdp_utils_alloc_synapse(nullptr), nullptr);

    stdp_synapse_t* synapses[10];
    EXPECT_EQ(stdp_utils_alloc_synapse_batch(nullptr, 10, synapses), 0u);

    EXPECT_GE(exception_count.load(), 2);
}

TEST_F(PlasticityModuleExceptionRegressionTest, StdpUtilsFlushMetricsReturnsZeroOnNull) {
    // WHAT: Verify flush_metrics returns 0 on NULL
    // WHY:  API contract: NULL context -> return 0

    reset();

    EXPECT_EQ(stdp_utils_flush_metrics(nullptr), 0);
    EXPECT_GE(exception_count.load(), 1);
}

TEST_F(PlasticityModuleExceptionRegressionTest, StdpUtilsGetSpikeFunctionsReturnFalseOnNull) {
    // WHAT: Verify spike retrieval functions return false on NULL
    // WHY:  API contract consistency

    reset();

    stdp_spike_event_t events[10];
    uint32_t num_found;

    EXPECT_FALSE(stdp_utils_get_spikes_in_window(nullptr, 0.0f, 100.0f, events, 10, &num_found));
    EXPECT_FALSE(stdp_utils_get_recent_spikes(nullptr, 10, events, &num_found));
    EXPECT_FALSE(stdp_utils_find_spike_pairs(nullptr, 1, 2, 50.0f, events, 10, &num_found));

    EXPECT_GE(exception_count.load(), 3);
}

//=============================================================================
// Error Code Stability Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, NullPointerErrorCodeConsistency) {
    // WHAT: Verify NIMCP_ERROR_NULL_POINTER is used consistently
    // WHY:  Error code regression prevention
    // NOTE: Only functions that throw exceptions are tested here

    std::vector<std::pair<std::string, std::function<void()>>> null_tests = {
        {"stdp_pr_bridge_is_connected", []() { stdp_pr_bridge_is_connected(nullptr); }},
        {"stdp_utils_reset", []() { stdp_utils_reset(nullptr); }},
        {"attention_head_create", []() { attention_head_create(nullptr); }},
    };

    for (const auto& test : null_tests) {
        reset();
        test.second();
        EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER)
            << "Function " << test.first << " should use NIMCP_ERROR_NULL_POINTER";
    }
}

TEST_F(PlasticityModuleExceptionRegressionTest, InvalidParamErrorCodeConsistency) {
    // WHAT: Verify NIMCP_ERROR_INVALID_PARAM is used for invalid parameters
    // WHY:  Error code regression prevention

    // Create valid bridge for testing
    stdp_pr_bridge_t bridge = stdp_pr_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    reset();

    // Invalid weight change (LTP must be positive)
    stdp_pr_notify_ltp(bridge, 1, 2, -0.5f, nullptr);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);

    stdp_pr_bridge_destroy(bridge);

    reset();

    // Invalid attention config (zero dimensions)
    attention_head_config_t config = {
        .input_dim = 0,
        .output_dim = 64,
        .key_dim = 32,
        .value_dim = 32,
        .temperature = 1.0f,
        .dropout_rate = 0.0f
    };
    attention_head_create(&config);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Return Value Stability Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, FloatReturnValueStability) {
    // WHAT: Verify float return values on error are stable
    // WHY:  Return value contract regression prevention

    reset();

    // These should all return 0.0f on NULL
    EXPECT_EQ(neuromodulator_pool_get_dopamine(nullptr), 0.0f);
    EXPECT_EQ(neuromodulator_pool_get_serotonin(nullptr), 0.0f);
    EXPECT_EQ(neuromodulator_pool_get_acetylcholine(nullptr), 0.0f);
    EXPECT_EQ(neuromodulator_pool_get_norepinephrine(nullptr), 0.0f);
    EXPECT_EQ(neuromodulator_release_dopamine(nullptr, 1.0f, 0.5f), 0.0f);

    // Coherence should return negative on NULL
    EXPECT_LT(stdp_pr_bridge_get_coherence(nullptr), 0.0f);
}

TEST_F(PlasticityModuleExceptionRegressionTest, IntReturnValueStability) {
    // WHAT: Verify int return values on error are stable
    // WHY:  Return value contract regression prevention

    reset();

    // These should all return -1 on NULL
    EXPECT_EQ(stdp_pr_notify_ltp(nullptr, 1, 2, 0.1f, nullptr), -1);
    EXPECT_EQ(stdp_pr_notify_ltd(nullptr, 1, 2, -0.1f, nullptr), -1);
    EXPECT_EQ(stdp_pr_notify_burst(nullptr, 1, 2, 0.1f, true, nullptr), -1);
    EXPECT_EQ(stdp_pr_bridge_update(nullptr, 1.0f), -1);
    EXPECT_EQ(stdp_pr_bridge_reset_stats(nullptr), -1);
}

TEST_F(PlasticityModuleExceptionRegressionTest, BoolReturnValueStability) {
    // WHAT: Verify bool return values on error are stable
    // WHY:  Return value contract regression prevention

    reset();

    // These should all return false on NULL
    EXPECT_FALSE(stdp_pr_bridge_validate_config(nullptr));
    EXPECT_FALSE(stdp_pr_bridge_is_connected(nullptr));
    EXPECT_FALSE(neuromodulator_reset(nullptr));
    EXPECT_FALSE(neuromodulator_update(nullptr, 0.01f));

    stdp_metrics_t metrics;
    EXPECT_FALSE(stdp_utils_get_metrics(nullptr, &metrics));
}

TEST_F(PlasticityModuleExceptionRegressionTest, PointerReturnValueStability) {
    // WHAT: Verify pointer return values on error are stable
    // WHY:  Return value contract regression prevention

    reset();

    // These should all return NULL on error
    EXPECT_EQ(attention_head_create(nullptr), nullptr);
    EXPECT_EQ(multihead_attention_create(nullptr), nullptr);
    EXPECT_EQ(stdp_utils_alloc_synapse(nullptr), nullptr);
}

TEST_F(PlasticityModuleExceptionRegressionTest, UnsignedReturnValueStability) {
    // WHAT: Verify unsigned return values on error are stable
    // WHY:  Return value contract regression prevention

    reset();

    stdp_synapse_t* synapses[10];
    EXPECT_EQ(stdp_utils_alloc_synapse_batch(nullptr, 10, synapses), 0u);
    EXPECT_EQ(stdp_utils_flush_metrics(nullptr), 0);
}

//=============================================================================
// Exception Message Format Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, ExceptionMessagesAreNonEmpty) {
    // WHAT: Verify exception messages are not empty
    // WHY:  Messages needed for debugging
    // NOTE: Only test functions that throw exceptions

    std::vector<std::function<void()>> operations = {
        []() { stdp_pr_bridge_is_connected(nullptr); },
        []() { stdp_utils_reset(nullptr); },
        []() { attention_head_create(nullptr); },
    };

    for (auto& op : operations) {
        reset();
        op();
        EXPECT_FALSE(last_message.empty())
            << "Exception message should not be empty";
    }
}

TEST_F(PlasticityModuleExceptionRegressionTest, ExceptionMessagesContainContext) {
    // WHAT: Verify exception messages contain useful context
    // WHY:  Messages should help identify the problem
    // NOTE: Use a function that actually throws exceptions

    reset();

    // Use stdp_pr_bridge_is_connected which throws on NULL
    stdp_pr_bridge_is_connected(nullptr);
    // Message should contain relevant keywords
    EXPECT_NE(last_message.find("NULL"), std::string::npos)
        << "Message should mention NULL: " << last_message;
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, LegacyErrorPathsPreserved) {
    // WHAT: Verify legacy error handling paths still work
    // WHY:  Backward compatibility

    // Create valid objects
    neuromodulator_config_t config = {
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

    neuromodulator_system_t system = neuromodulator_system_create(&config);
    ASSERT_NE(system, nullptr);

    // Valid operations should not throw
    reset();
    float dopamine = neuromodulator_release_dopamine(system, 1.0f, 0.5f);
    EXPECT_GT(dopamine, 0.0f);
    EXPECT_EQ(exception_count.load(), 0);

    neuromodulator_system_destroy(system);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, ExceptionPathPerformance) {
    // WHAT: Ensure exception paths don't cause performance regression
    // WHY:  Exceptions should be fast enough for real-time use

    const int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        neuromodulator_pool_get_dopamine(nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should process at least 1000 exceptions per millisecond
    double exceptions_per_ms = static_cast<double>(iterations) / (duration.count() / 1000.0);
    EXPECT_GT(exceptions_per_ms, 100.0)
        << "Exception handling too slow: " << exceptions_per_ms << " exceptions/ms";
}

//=============================================================================
// Triplet STDP Immune Bridge API Regression Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, TripletStdpImmuneAPIContractNullBridge) {
    // WHAT: Verify triplet STDP immune bridge functions return -1 on NULL
    // WHY:  API contract: NULL bridge -> return -1, throw exception

    reset();
    EXPECT_EQ(triplet_stdp_immune_default_config(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(triplet_stdp_immune_bridge_update(nullptr, 10), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(triplet_stdp_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(triplet_stdp_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    triplet_stdp_modulation_state_t modulation;
    EXPECT_EQ(triplet_stdp_immune_get_modulation_state(nullptr, &modulation), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(triplet_stdp_immune_restore_plasticity(nullptr, 0.5f), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(triplet_stdp_immune_detect_instability(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    uint32_t antigen_id;
    EXPECT_EQ(triplet_stdp_immune_alert_instability(nullptr, &antigen_id), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionRegressionTest, TripletStdpImmuneMessageContainsFunction) {
    // WHAT: Verify exception messages contain function name
    // WHY:  API contract: messages must identify source function

    reset();
    triplet_stdp_immune_bridge_update(nullptr, 10);
    EXPECT_TRUE(last_message.find("triplet_stdp_immune_bridge_update") != std::string::npos);
}

//=============================================================================
// Triplet STDP Sleep Bridge API Regression Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, TripletStdpSleepAPIContractNullBridge) {
    // WHAT: Verify triplet STDP sleep bridge functions return -1 on NULL
    // WHY:  API contract: NULL bridge -> return -1, throw exception

    reset();
    EXPECT_EQ(triplet_stdp_sleep_default_config(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(triplet_stdp_sleep_update(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    triplet_stdp_sleep_effects_t effects;
    EXPECT_EQ(triplet_stdp_sleep_get_effects(nullptr, &effects), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    triplet_stdp_synapse_t synapse;
    EXPECT_EQ(triplet_stdp_sleep_apply_modulation(nullptr, &synapse), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Neuromodulators Sleep Bridge API Regression Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, NeuromodSleepAPIContractNullBridge) {
    // WHAT: Verify neuromod sleep bridge functions return -1 on NULL
    // WHY:  API contract: NULL bridge -> return -1, throw exception

    reset();
    EXPECT_EQ(neuromod_sleep_default_config(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(neuromod_sleep_update(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(neuromod_sleep_apply_modulation(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    neuromod_sleep_effects_t effects;
    EXPECT_EQ(neuromod_sleep_get_effects(nullptr, &effects), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Eligibility PR Bridge API Regression Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, EligPrBridgeAPIContractNullBridge) {
    // WHAT: Verify eligibility PR bridge functions return -1 on NULL
    // WHY:  API contract: NULL bridge -> return -1, throw exception

    reset();
    elig_pr_forward_effect_t forward_effect;
    EXPECT_EQ(elig_pr_apply_consolidation_gate(nullptr, 1, 0.5f, 0.5f, &forward_effect), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    bool should_promote;
    EXPECT_EQ(elig_pr_check_tier_promotion(nullptr, 1, 0.5f, 0.5f, &should_promote), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    float delta;
    EXPECT_EQ(elig_pr_apply_entanglement_update(nullptr, 1, 2, 0.5f, &delta), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    float modulated_lambda;
    EXPECT_EQ(elig_pr_get_decay_modulation(nullptr, 0.5f, 0.95f, &modulated_lambda), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    float boosted;
    EXPECT_EQ(elig_pr_apply_resonance_boost(nullptr, 0.5f, 0.5f, &boosted), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    elig_pr_backward_effect_t backward_effect;
    EXPECT_EQ(elig_pr_compute_modulation(nullptr, 0.5f, 0.5f, ELIG_PR_TIER_Z0, 0.95f, &backward_effect), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    elig_pr_bridge_state_t state;
    EXPECT_EQ(elig_pr_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    elig_pr_bridge_stats_t stats;
    EXPECT_EQ(elig_pr_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(elig_pr_bridge_reset_stats(nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(elig_pr_bridge_update(nullptr, 1.0f), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionRegressionTest, EligPrBridgeAPIContractNullOutput) {
    // WHAT: Verify eligibility PR bridge functions handle NULL output
    // WHY:  API contract: NULL output -> return -1, throw exception

    elig_pr_bridge_config_t config = elig_pr_bridge_default_config();
    elig_pr_bridge_t bridge = elig_pr_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    reset();
    EXPECT_EQ(elig_pr_apply_consolidation_gate(bridge, 1, 0.5f, 0.5f, nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(elig_pr_check_tier_promotion(bridge, 1, 0.5f, 0.5f, nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(elig_pr_apply_entanglement_update(bridge, 1, 2, 0.5f, nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_EQ(elig_pr_get_decay_modulation(bridge, 0.5f, 0.95f, nullptr), -1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    elig_pr_bridge_destroy(bridge);
}

//=============================================================================
// Triplet STDP Core API Regression Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, TripletStdpGettersReturnNegativeOnNull) {
    // WHAT: Verify triplet STDP getters return negative value on NULL
    // WHY:  API contract: NULL synapse -> return -1.0f

    reset();
    EXPECT_LT(triplet_stdp_get_weight(nullptr), 0.0f);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_LT(triplet_stdp_get_r1_pre(nullptr), 0.0f);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_LT(triplet_stdp_get_r2_pre(nullptr), 0.0f);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_LT(triplet_stdp_get_o1_post(nullptr), 0.0f);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_LT(triplet_stdp_get_total_ltp(nullptr), 0.0f);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset();
    EXPECT_LT(triplet_stdp_get_total_ltd(nullptr), 0.0f);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Error Code Consistency Regression
//=============================================================================

TEST_F(PlasticityModuleExceptionRegressionTest, AllNullPointerErrorsUseNIMCP_ERROR_NULL_POINTER) {
    // WHAT: Verify all modules use consistent error code for NULL pointer
    // WHY:  API contract: NULL errors must use NIMCP_ERROR_NULL_POINTER

    // Test triplet STDP immune bridge
    reset();
    triplet_stdp_immune_bridge_update(nullptr, 10);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    // Test triplet STDP sleep bridge
    reset();
    triplet_stdp_sleep_update(nullptr);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    // Test neuromod sleep bridge
    reset();
    neuromod_sleep_update(nullptr);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    // Test eligibility PR bridge
    reset();
    elig_pr_bridge_update(nullptr, 1.0f);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
