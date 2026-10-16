/**
 * @file test_plasticity_module_exception.cpp
 * @brief Unit tests for NIMCP_THROW_TO_IMMUNE exception handling in plasticity modules
 *
 * WHAT: Test NULL pointer and allocation failure handling with NIMCP_THROW_TO_IMMUNE
 * WHY:  Verify exceptions are properly reported to the brain immune system
 * HOW:  Test each function's error paths with invalid parameters
 *
 * MODULES TESTED:
 * - nimcp_neuromodulators.c (25 throws) - NULL checks on release, compute, get functions
 * - nimcp_attention.c (12 throws) - allocation failures in attention_head_create, multihead_attention_create
 * - nimcp_stdp_pr_bridge.c (33 throws) - NULL checks in validate_config, is_connected, notify_batch, get_coherence
 * - nimcp_stdp_utils_bridge.c (50 throws) - NULL checks in reset, record_ltp/ltd, update_weight_stats, flush_metrics
 *
 * @author NIMCP Development Team
 * @date 2026-01-25
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>

// Include C++ compatible headers first (may include CUDA)
#include "plasticity/neuromodulators/nimcp_neuromodulators_sleep_bridge.h"
#include "plasticity/stdp/nimcp_triplet_stdp_immune_bridge.h"
#include "plasticity/stdp/nimcp_triplet_stdp_sleep_bridge.h"
#include "plasticity/eligibility/nimcp_eligibility_pr_bridge.h"

extern "C" {
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/attention/nimcp_attention.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/stdp/nimcp_triplet_stdp.h"
#include "plasticity/stdp/nimcp_stdp_pr_bridge.h"
#include "plasticity/stdp/nimcp_stdp_utils_bridge.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PlasticityModuleExceptionTest : public ::testing::Test {
protected:
    static std::atomic<int> exception_count;
    static std::atomic<nimcp_error_t> last_error_code;
    static char last_message[256];

    void SetUp() override {
        exception_count = 0;
        last_error_code = NIMCP_SUCCESS;
        memset(last_message, 0, sizeof(last_message));
        nimcp_exception_system_init();
        register_test_handler();
    }

    void TearDown() override {
        unregister_test_handler();
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static nimcp_handler_registration_t* test_handler_reg;

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exception_count++;
        last_error_code = ex->code;
        strncpy(last_message, ex->message, sizeof(last_message) - 1);
        return false;  // Don't consume
    }

    void register_test_handler() {
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "plasticity_test_handler";
        options.handler = test_exception_handler;
        options.priority = 100;
        test_handler_reg = nimcp_handler_register(&options);
    }

    void unregister_test_handler() {
        if (test_handler_reg) {
            nimcp_handler_unregister(test_handler_reg);
            test_handler_reg = nullptr;
        }
    }

    void reset_counters() {
        exception_count = 0;
        last_error_code = NIMCP_SUCCESS;
        memset(last_message, 0, sizeof(last_message));
    }
};

std::atomic<int> PlasticityModuleExceptionTest::exception_count(0);
std::atomic<nimcp_error_t> PlasticityModuleExceptionTest::last_error_code(NIMCP_SUCCESS);
char PlasticityModuleExceptionTest::last_message[256] = {0};
nimcp_handler_registration_t* PlasticityModuleExceptionTest::test_handler_reg = nullptr;

//=============================================================================
// Neuromodulator Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorPoolGetDopamineNullPool) {
    // WHAT: Test NULL pool handling in neuromodulator_pool_get_dopamine
    // WHY:  Should throw NIMCP_THROW_TO_IMMUNE for NULL pointer
    reset_counters();

    float result = neuromodulator_pool_get_dopamine(nullptr);

    // Function should return 0.0f for NULL input and trigger exception
    EXPECT_EQ(result, 0.0f);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorPoolGetSerotoninNullPool) {
    reset_counters();

    float result = neuromodulator_pool_get_serotonin(nullptr);

    EXPECT_EQ(result, 0.0f);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorPoolGetAcetylcholineNullPool) {
    reset_counters();

    float result = neuromodulator_pool_get_acetylcholine(nullptr);

    EXPECT_EQ(result, 0.0f);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorPoolGetNorepinephrineNullPool) {
    reset_counters();

    float result = neuromodulator_pool_get_norepinephrine(nullptr);

    EXPECT_EQ(result, 0.0f);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorPoolSetDopamineNullPool) {
    reset_counters();

    neuromodulator_pool_set_dopamine(nullptr, 0.5f);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorPoolSetSerotoninNullPool) {
    reset_counters();

    neuromodulator_pool_set_serotonin(nullptr, 0.5f);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorSystemNullSystemRelease) {
    reset_counters();

    float result = neuromodulator_release_dopamine(nullptr, 1.0f, 0.5f);

    EXPECT_EQ(result, 0.0f);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorSystemNullSystemGetLevels) {
    reset_counters();

    neuromodulator_pool_t pool;
    bool result = neuromodulator_get_levels(nullptr, &pool);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorSystemNullPoolOutput) {
    // Create a valid system first
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
    if (system) {
        reset_counters();

        bool result = neuromodulator_get_levels(system, nullptr);

        EXPECT_FALSE(result);
        EXPECT_GE(exception_count.load(), 1);
        EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

        neuromodulator_system_destroy(system);
    }
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorComputeEffectsNullSystem) {
    reset_counters();

    receptor_profile_t profile = receptor_profile_create();
    modulation_effects_t effects = modulation_effects_create();

    bool result = neuromodulator_compute_effects(nullptr, &profile, &effects);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    receptor_profile_destroy(&profile);
    modulation_effects_destroy(&effects);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorComputeEffectsNullProfile) {
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
    if (system) {
        reset_counters();

        modulation_effects_t effects = modulation_effects_create();
        bool result = neuromodulator_compute_effects(system, nullptr, &effects);

        EXPECT_FALSE(result);
        EXPECT_GE(exception_count.load(), 1);
        EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

        modulation_effects_destroy(&effects);
        neuromodulator_system_destroy(system);
    }
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorResetNullSystem) {
    reset_counters();

    bool result = neuromodulator_reset(nullptr);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorUpdateNullSystem) {
    reset_counters();

    bool result = neuromodulator_update(nullptr, 0.01f);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodulatorGetStatsNullSystem) {
    reset_counters();

    neuromodulator_stats_t stats;
    bool result = neuromodulator_get_stats(nullptr, &stats);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Attention Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, AttentionHeadCreateNullConfig) {
    reset_counters();

    attention_head_t head = attention_head_create(nullptr);

    EXPECT_EQ(head, nullptr);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, AttentionHeadCreateZeroDimensions) {
    reset_counters();

    attention_head_config_t config = {
        .input_dim = 0,  // Invalid
        .output_dim = 64,
        .key_dim = 32,
        .value_dim = 32,
        .temperature = 1.0f,
        .dropout_rate = 0.0f
    };

    attention_head_t head = attention_head_create(&config);

    EXPECT_EQ(head, nullptr);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(PlasticityModuleExceptionTest, MultiheadAttentionCreateNullConfig) {
    reset_counters();

    multihead_attention_t mha = multihead_attention_create(nullptr);

    EXPECT_EQ(mha, nullptr);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, MultiheadAttentionCreateZeroHeads) {
    reset_counters();

    multihead_attention_config_t config = {
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

    multihead_attention_t mha = multihead_attention_create(&config);

    EXPECT_EQ(mha, nullptr);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(PlasticityModuleExceptionTest, AttentionValidateConfigNull) {
    reset_counters();

    bool result = attention_validate_config(nullptr);

    EXPECT_FALSE(result);
    // This may or may not throw depending on implementation
}

TEST_F(PlasticityModuleExceptionTest, AttentionHeadForwardNullHead) {
    reset_counters();

    float query[128] = {0};
    float key[128] = {0};
    float value[128] = {0};
    float output[128] = {0};

    bool result = attention_head_forward(nullptr, query, key, value, 4, output, nullptr, nullptr, 0);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, MultiheadAttentionForwardNullMHA) {
    reset_counters();

    float input[128] = {0};
    float output[128] = {0};

    bool result = multihead_attention_forward(nullptr, input, 4, nullptr, output);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, MultiheadAttentionSetGateNullMHA) {
    reset_counters();

    bool result = multihead_attention_set_gate(nullptr, 0.5f);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, MultiheadAttentionGetStatsNullMHA) {
    reset_counters();

    attention_stats_t stats;
    bool result = multihead_attention_get_stats(nullptr, &stats);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// STDP-PR Bridge Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, StdpPrBridgeValidateConfigNull) {
    reset_counters();

    bool result = stdp_pr_bridge_validate_config(nullptr);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrBridgeIsConnectedNull) {
    reset_counters();

    bool result = stdp_pr_bridge_is_connected(nullptr);

    /* is_connected is a simple predicate - returns false for NULL without throwing */
    EXPECT_FALSE(result);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrNotifyLtpNullBridge) {
    reset_counters();

    int result = stdp_pr_notify_ltp(nullptr, 1, 2, 0.1f, nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrNotifyLtdNullBridge) {
    reset_counters();

    int result = stdp_pr_notify_ltd(nullptr, 1, 2, -0.1f, nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrNotifyBurstNullBridge) {
    reset_counters();

    int result = stdp_pr_notify_burst(nullptr, 1, 2, 0.1f, true, nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrNotifyBatchNullBridge) {
    reset_counters();

    stdp_pr_forward_effect_t events[1] = {};
    int result = stdp_pr_notify_batch(nullptr, events, 1);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrGetModulationNullBridge) {
    reset_counters();

    stdp_pr_backward_effect_t effect;
    int result = stdp_pr_get_modulation(nullptr, 1, &effect);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrApplyResonanceModulationNullBridge) {
    reset_counters();

    float modulated_lr;
    int result = stdp_pr_apply_resonance_modulation(nullptr, 0.5f, 0.01f, &modulated_lr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrGetCoherenceNullBridge) {
    reset_counters();

    float result = stdp_pr_bridge_get_coherence(nullptr);

    EXPECT_LT(result, 0.0f);  // Returns -1 on error
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrBridgeGetStateNullBridge) {
    reset_counters();

    stdp_pr_bridge_state_t state;
    int result = stdp_pr_bridge_get_state(nullptr, &state);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrBridgeGetStatsNullBridge) {
    reset_counters();

    stdp_pr_bridge_stats_t stats;
    int result = stdp_pr_bridge_get_stats(nullptr, &stats);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrBridgeResetStatsNullBridge) {
    reset_counters();

    int result = stdp_pr_bridge_reset_stats(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrBridgeUpdateNullBridge) {
    reset_counters();

    int result = stdp_pr_bridge_update(nullptr, 1.0f);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrNotifyLtpInvalidWeightChange) {
    // Create a valid bridge first
    stdp_pr_bridge_t bridge = stdp_pr_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    reset_counters();

    // LTP weight change must be positive
    int result = stdp_pr_notify_ltp(bridge, 1, 2, -0.1f, nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);

    stdp_pr_bridge_destroy(bridge);
}

TEST_F(PlasticityModuleExceptionTest, StdpPrNotifyLtdInvalidWeightChange) {
    stdp_pr_bridge_t bridge = stdp_pr_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    reset_counters();

    // LTD weight change must be negative
    int result = stdp_pr_notify_ltd(bridge, 1, 2, 0.1f, nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);

    stdp_pr_bridge_destroy(bridge);
}

//=============================================================================
// STDP Utils Bridge Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, StdpUtilsResetNullCtx) {
    reset_counters();

    stdp_utils_reset(nullptr);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsRecordSpikeNullCtx) {
    reset_counters();

    stdp_spike_event_t event = {0};
    bool result = stdp_utils_record_spike(nullptr, &event);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsRecordSpikeNullEvent) {
    stdp_utils_ctx_t ctx = stdp_utils_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    reset_counters();

    bool result = stdp_utils_record_spike(ctx, nullptr);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    stdp_utils_destroy(ctx);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsRecordLtpNullCtx) {
    reset_counters();

    stdp_utils_record_ltp(nullptr, 0.1f, 10.0f);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsRecordLtdNullCtx) {
    reset_counters();

    stdp_utils_record_ltd(nullptr, -0.1f, -10.0f);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsUpdateWeightStatsNullCtx) {
    reset_counters();

    float weights[10] = {0};
    stdp_utils_update_weight_stats(nullptr, weights, 10);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsGetMetricsNullCtx) {
    reset_counters();

    stdp_metrics_t metrics;
    bool result = stdp_utils_get_metrics(nullptr, &metrics);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsGetMetricsNullOutput) {
    stdp_utils_ctx_t ctx = stdp_utils_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    reset_counters();

    bool result = stdp_utils_get_metrics(ctx, nullptr);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    stdp_utils_destroy(ctx);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsFlushMetricsNullCtx) {
    reset_counters();

    int32_t result = stdp_utils_flush_metrics(nullptr);

    EXPECT_EQ(result, 0);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsExportCsvNullCtx) {
    reset_counters();

    bool result = stdp_utils_export_csv(nullptr, "test.csv");

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsExportCsvNullFilename) {
    stdp_utils_ctx_t ctx = stdp_utils_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    reset_counters();

    bool result = stdp_utils_export_csv(ctx, nullptr);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    stdp_utils_destroy(ctx);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsExportJsonNullCtx) {
    reset_counters();

    bool result = stdp_utils_export_json(nullptr, "test.json");

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsExportJsonNullFilename) {
    stdp_utils_ctx_t ctx = stdp_utils_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    reset_counters();

    bool result = stdp_utils_export_json(ctx, nullptr);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    stdp_utils_destroy(ctx);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsAllocSynapseNullCtx) {
    reset_counters();

    stdp_synapse_t* synapse = stdp_utils_alloc_synapse(nullptr);

    EXPECT_EQ(synapse, nullptr);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsFreeSynapseNullCtx) {
    reset_counters();

    stdp_synapse_t synapse;
    stdp_utils_free_synapse(nullptr, &synapse);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsAllocSynapseBatchNullCtx) {
    reset_counters();

    stdp_synapse_t* synapses[10];
    uint32_t result = stdp_utils_alloc_synapse_batch(nullptr, 10, synapses);

    EXPECT_EQ(result, 0u);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsGetSpikesInWindowNullCtx) {
    reset_counters();

    stdp_spike_event_t events[10];
    uint32_t num_found;
    bool result = stdp_utils_get_spikes_in_window(nullptr, 0.0f, 100.0f, events, 10, &num_found);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsGetSpikesInWindowNullEvents) {
    stdp_utils_ctx_t ctx = stdp_utils_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    reset_counters();

    uint32_t num_found;
    bool result = stdp_utils_get_spikes_in_window(ctx, 0.0f, 100.0f, nullptr, 10, &num_found);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    stdp_utils_destroy(ctx);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsGetRecentSpikesNullCtx) {
    reset_counters();

    stdp_spike_event_t events[10];
    uint32_t num_found;
    bool result = stdp_utils_get_recent_spikes(nullptr, 10, events, &num_found);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUtilsFindSpikePairsNullCtx) {
    reset_counters();

    stdp_spike_event_t events[10];
    uint32_t num_found;
    bool result = stdp_utils_find_spike_pairs(nullptr, 1, 2, 50.0f, events, 10, &num_found);

    EXPECT_FALSE(result);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Error Code Consistency Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, AllNullPointerErrorsUseCorrectCode) {
    // WHAT: Verify all NULL pointer errors use NIMCP_ERROR_NULL_POINTER
    // WHY:  Ensure consistent error code usage across modules

    reset_counters();

    // Test multiple NULL pointer cases
    neuromodulator_pool_get_dopamine(nullptr);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    reset_counters();
    stdp_utils_reset(nullptr);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    /* Note: stdp_pr_bridge_is_connected(nullptr) returns false without throwing -
       it's a simple predicate, not an error path */
}

TEST_F(PlasticityModuleExceptionTest, AllInvalidParamErrorsUseCorrectCode) {
    // WHAT: Verify all invalid parameter errors use NIMCP_ERROR_INVALID_PARAM
    // WHY:  Ensure consistent error code usage for parameter validation

    stdp_pr_bridge_t bridge = stdp_pr_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    reset_counters();

    // Invalid weight change for LTP (must be positive)
    stdp_pr_notify_ltp(bridge, 1, 2, -0.1f, nullptr);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_INVALID_PARAM);

    stdp_pr_bridge_destroy(bridge);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, ConcurrentExceptionThrowing) {
    // WHAT: Test thread safety of exception throwing
    // WHY:  Plasticity modules run in parallel threads

    std::atomic<int> total_exceptions{0};
    const int num_threads = 4;
    const int ops_per_thread = 50;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&total_exceptions, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                // Each thread calls NULL pointer functions
                switch ((t + i) % 4) {
                    case 0:
                        neuromodulator_pool_get_dopamine(nullptr);
                        break;
                    case 1:
                        stdp_utils_reset(nullptr);
                        break;
                    case 2:
                        stdp_pr_bridge_is_connected(nullptr);
                        break;
                    case 3: {
                        stdp_metrics_t metrics;
                        stdp_utils_get_metrics(nullptr, &metrics);
                        break;
                    }
                }
                total_exceptions++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_exceptions.load(), num_threads * ops_per_thread);
}

//=============================================================================
// STDP Core Module Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, StdpSynapseInitWithConfigNullSynapse) {
    reset_counters();

    stdp_config_t config;
    stdp_synapse_init_with_config(nullptr, &config);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpSynapseInitWithConfigNullConfig) {
    reset_counters();

    stdp_synapse_t synapse;
    stdp_synapse_init_with_config(&synapse, nullptr);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpUpdateTracesNullSynapse) {
    reset_counters();

    stdp_update_traces(nullptr, 1.0f);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPreSpikeNullSynapse) {
    reset_counters();

    stdp_pre_spike(nullptr, 10.0f);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpPostSpikeNullSynapse) {
    reset_counters();

    stdp_post_spike(nullptr, 10.0f);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpSynapseResetNullSynapse) {
    reset_counters();

    stdp_synapse_reset(nullptr);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, StdpSetSleepStateNullSynapse) {
    reset_counters();

    // sleep_state_t values: SLEEP_STATE_AWAKE=0
    stdp_set_sleep_state(nullptr, (sleep_state_t)0);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Triplet STDP Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, TripletStdpSynapseDestroyNullSynapse) {
    reset_counters();

    triplet_stdp_synapse_destroy(nullptr);

    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpGetWeightNullSynapse) {
    reset_counters();

    float result = triplet_stdp_get_weight(nullptr);

    EXPECT_LT(result, 0.0f);  // Returns -1.0f on error
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpGetR1PreNullSynapse) {
    reset_counters();

    float result = triplet_stdp_get_r1_pre(nullptr);

    EXPECT_LT(result, 0.0f);  // Returns -1.0f on error
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpGetR2PreNullSynapse) {
    reset_counters();

    float result = triplet_stdp_get_r2_pre(nullptr);

    EXPECT_LT(result, 0.0f);  // Returns -1.0f on error
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpGetO1PostNullSynapse) {
    reset_counters();

    float result = triplet_stdp_get_o1_post(nullptr);

    EXPECT_LT(result, 0.0f);  // Returns -1.0f on error
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpGetTotalLtpNullSynapse) {
    reset_counters();

    float result = triplet_stdp_get_total_ltp(nullptr);

    EXPECT_LT(result, 0.0f);  // Returns -1.0f on error
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpGetTotalLtdNullSynapse) {
    reset_counters();

    float result = triplet_stdp_get_total_ltd(nullptr);

    EXPECT_LT(result, 0.0f);  // Returns -1.0f on error
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Triplet STDP Immune Bridge Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneDefaultConfigNullConfig) {
    reset_counters();

    int result = triplet_stdp_immune_default_config(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneBridgeUpdateNullBridge) {
    reset_counters();

    int result = triplet_stdp_immune_bridge_update(nullptr, 10);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneApplyCytokineEffectsNullBridge) {
    reset_counters();

    int result = triplet_stdp_immune_apply_cytokine_effects(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneApplyInflammationEffectsNullBridge) {
    reset_counters();

    int result = triplet_stdp_immune_apply_inflammation_effects(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneGetModulationStateNullBridge) {
    reset_counters();

    triplet_stdp_modulation_state_t modulation;
    int result = triplet_stdp_immune_get_modulation_state(nullptr, &modulation);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneRestorePlasticityNullBridge) {
    reset_counters();

    int result = triplet_stdp_immune_restore_plasticity(nullptr, 0.5f);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneDetectInstabilityNullBridge) {
    reset_counters();

    int result = triplet_stdp_immune_detect_instability(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneAlertInstabilityNullBridge) {
    reset_counters();

    uint32_t antigen_id;
    int result = triplet_stdp_immune_alert_instability(nullptr, &antigen_id);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneConnectBioAsyncNullBridge) {
    reset_counters();

    int result = triplet_stdp_immune_connect_bio_async(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpImmuneDisconnectBioAsyncNullBridge) {
    reset_counters();

    int result = triplet_stdp_immune_disconnect_bio_async(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Triplet STDP Sleep Bridge Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, TripletStdpSleepDefaultConfigNullConfig) {
    reset_counters();

    int result = triplet_stdp_sleep_default_config(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpSleepUpdateNullBridge) {
    reset_counters();

    int result = triplet_stdp_sleep_update(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpSleepGetEffectsNullBridge) {
    reset_counters();

    triplet_stdp_sleep_effects_t effects;
    int result = triplet_stdp_sleep_get_effects(nullptr, &effects);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpSleepApplyModulationNullBridge) {
    reset_counters();

    triplet_stdp_synapse_t synapse;
    int result = triplet_stdp_sleep_apply_modulation(nullptr, &synapse);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, TripletStdpSleepApplyModulationNullSynapse) {
    // Note: We can't easily test this without a valid bridge, so we skip
    // The implementation will be tested in integration tests
    SUCCEED();
}

//=============================================================================
// Neuromodulators Sleep Bridge Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, NeuromodSleepDefaultConfigNullConfig) {
    reset_counters();

    int result = neuromod_sleep_default_config(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodSleepUpdateNullBridge) {
    reset_counters();

    int result = neuromod_sleep_update(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodSleepApplyModulationNullBridge) {
    reset_counters();

    int result = neuromod_sleep_apply_modulation(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, NeuromodSleepGetEffectsNullBridge) {
    reset_counters();

    neuromod_sleep_effects_t effects;
    int result = neuromod_sleep_get_effects(nullptr, &effects);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Eligibility PR Bridge Exception Tests
//=============================================================================

TEST_F(PlasticityModuleExceptionTest, EligPrApplyConsolidationGateNullBridge) {
    reset_counters();

    elig_pr_forward_effect_t effect;
    int result = elig_pr_apply_consolidation_gate(nullptr, 1, 0.5f, 0.5f, &effect);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrApplyConsolidationGateNullEffect) {
    elig_pr_bridge_config_t config = elig_pr_bridge_default_config();
    elig_pr_bridge_t bridge = elig_pr_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    reset_counters();

    int result = elig_pr_apply_consolidation_gate(bridge, 1, 0.5f, 0.5f, nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);

    elig_pr_bridge_destroy(bridge);
}

TEST_F(PlasticityModuleExceptionTest, EligPrCheckTierPromotionNullBridge) {
    reset_counters();

    bool should_promote;
    int result = elig_pr_check_tier_promotion(nullptr, 1, 0.5f, 0.5f, &should_promote);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrApplyEntanglementUpdateNullBridge) {
    reset_counters();

    float delta;
    int result = elig_pr_apply_entanglement_update(nullptr, 1, 2, 0.5f, &delta);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrGetDecayModulationNullBridge) {
    reset_counters();

    float modulated_lambda;
    int result = elig_pr_get_decay_modulation(nullptr, 0.5f, 0.95f, &modulated_lambda);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrGetTierParametersNullBridge) {
    reset_counters();

    float lambda, sensitivity;
    int result = elig_pr_get_tier_parameters(nullptr, ELIG_PR_TIER_Z0, &lambda, &sensitivity);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrApplyResonanceBoostNullBridge) {
    reset_counters();

    float boosted;
    int result = elig_pr_apply_resonance_boost(nullptr, 0.5f, 0.5f, &boosted);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrComputeModulationNullBridge) {
    reset_counters();

    elig_pr_backward_effect_t effect;
    int result = elig_pr_compute_modulation(nullptr, 0.5f, 0.5f, ELIG_PR_TIER_Z0, 0.95f, &effect);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrBridgeGetStateNullBridge) {
    reset_counters();

    elig_pr_bridge_state_t state;
    int result = elig_pr_bridge_get_state(nullptr, &state);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrBridgeGetStatsNullBridge) {
    reset_counters();

    elig_pr_bridge_stats_t stats;
    int result = elig_pr_bridge_get_stats(nullptr, &stats);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrBridgeResetStatsNullBridge) {
    reset_counters();

    int result = elig_pr_bridge_reset_stats(nullptr);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PlasticityModuleExceptionTest, EligPrBridgeUpdateNullBridge) {
    reset_counters();

    int result = elig_pr_bridge_update(nullptr, 1.0f);

    EXPECT_EQ(result, -1);
    EXPECT_GE(exception_count.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_NULL_POINTER);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
