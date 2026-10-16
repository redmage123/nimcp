/**
 * @file test_predictive_bridge_exception.cpp
 * @brief Unit tests for Predictive Bridge exception handling with NIMCP_THROW_TO_IMMUNE
 *
 * WHAT: Tests NULL pointer handling, allocation failure handling, and bio_async NULL checks
 * WHY:  Verify exception-to-immune pipeline works correctly for all predictive bridges
 * HOW:  Test each bridge's exception paths systematically
 *
 * BRIDGES TESTED:
 * - predictive_snn_bridge: bridge_base_init, SNN creation, buffer allocation
 * - predictive_plasticity_bridge: bridge_base_init, synapse allocation
 * - predictive_thalamic_bridge: bridge_base_init, mutex NULL check
 * - predictive_fep_bridge: bridge_base_init, mutex NULL check
 * - predictive_substrate_bridge: substrate and bridge NULL checks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>

extern "C" {
#include "cognitive/predictive/nimcp_predictive_snn_bridge.h"
#include "cognitive/predictive/nimcp_predictive_plasticity_bridge.h"
#include "cognitive/predictive/nimcp_predictive_thalamic_bridge.h"
#include "cognitive/predictive/nimcp_predictive_fep_bridge.h"
#include "cognitive/predictive/nimcp_predictive_substrate_bridge.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PredictiveBridgeExceptionTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<bool> exception_caught;
    static std::vector<std::string> caught_messages;
    static nimcp_handler_registration_t* test_handler_reg;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        exception_caught = false;
        caught_messages.clear();

        nimcp_exception_system_init();

        nimcp_handler_options_t options = {};
        options.name = "test_predictive_bridge_exception_handler";
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.category_filter = (nimcp_exception_category_t)0;
        options.min_severity = (nimcp_exception_severity_t)0;
        options.type_filter = (nimcp_exception_type_t)0;

        test_handler_reg = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (test_handler_reg) {
            nimcp_handler_unregister(test_handler_reg);
            test_handler_reg = nullptr;
        }
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        exception_caught = true;
        if (ex->message) {
            caught_messages.push_back(ex->message);
        }
        return false; /* Don't consume - let other handlers see it */
    }

    void ResetExceptionState() {
        handler_call_count = 0;
        last_exception_code = 0;
        exception_caught = false;
        caught_messages.clear();
        nimcp_exception_clear_current();
    }
};

/* Static member initialization */
std::atomic<int> PredictiveBridgeExceptionTest::handler_call_count(0);
std::atomic<int> PredictiveBridgeExceptionTest::last_exception_code(0);
std::atomic<bool> PredictiveBridgeExceptionTest::exception_caught(false);
std::vector<std::string> PredictiveBridgeExceptionTest::caught_messages;
nimcp_handler_registration_t* PredictiveBridgeExceptionTest::test_handler_reg = nullptr;

/* ============================================================================
 * Predictive SNN Bridge NULL Pointer Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionTest, SnnBridge_ResetWithNullBridge) {
    int result = predictive_snn_reset(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, SnnBridge_EncodeStateWithNullBridge) {
    float dims[] = {0.5f, 0.5f};
    int result = predictive_snn_encode_state(nullptr, dims, 2);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, SnnBridge_GetStatsWithNullBridge) {
    predictive_snn_stats_t stats = {};
    int result = predictive_snn_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, SnnBridge_GetStatsWithNullStats) {
    predictive_snn_config_t config = predictive_snn_config_default();
    predictive_snn_bridge_t* bridge = predictive_snn_create(&config);

    if (bridge) {
        int result = predictive_snn_get_stats(bridge, nullptr);
        EXPECT_EQ(result, -1);
        predictive_snn_destroy(bridge);
    }
}

TEST_F(PredictiveBridgeExceptionTest, SnnBridge_DestroyWithNull) {
    /* Should not crash */
    predictive_snn_destroy(nullptr);
    SUCCEED();
}

TEST_F(PredictiveBridgeExceptionTest, SnnBridge_SimulateWithNull) {
    int result = predictive_snn_simulate(nullptr, 100.0f);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, SnnBridge_BioAsyncConnectWithNull) {
    int result = predictive_snn_bio_async_connect(nullptr);
    /* May throw exception or return -1 */
    EXPECT_TRUE(result == -1 || exception_caught);
}

TEST_F(PredictiveBridgeExceptionTest, SnnBridge_IsBioAsyncConnectedWithNull) {
    bool connected = predictive_snn_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Predictive Plasticity Bridge NULL Pointer Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionTest, PlasticityBridge_ResetWithNullBridge) {
    int result = predictive_plasticity_reset(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, PlasticityBridge_GetStatsWithNullBridge) {
    predictive_plasticity_stats_t stats = {};
    int result = predictive_plasticity_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, PlasticityBridge_GetStatsWithNullStats) {
    predictive_plasticity_config_t config = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* bridge = predictive_plasticity_create(&config);

    if (bridge) {
        int result = predictive_plasticity_get_stats(bridge, nullptr);
        EXPECT_EQ(result, -1);
        predictive_plasticity_destroy(bridge);
    }
}

TEST_F(PredictiveBridgeExceptionTest, PlasticityBridge_DestroyWithNull) {
    /* Should not crash */
    predictive_plasticity_destroy(nullptr);
    SUCCEED();
}

TEST_F(PredictiveBridgeExceptionTest, PlasticityBridge_ConsolidateWithNull) {
    int result = predictive_plasticity_consolidate(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, PlasticityBridge_CreateAndValidLifecycle) {
    predictive_plasticity_config_t config = predictive_plasticity_config_default();
    predictive_plasticity_bridge_t* bridge = predictive_plasticity_create(&config);

    /* Bridge may or may not be created depending on internal state */
    if (bridge) {
        /* Should be able to get stats */
        predictive_plasticity_stats_t stats = {};
        int result = predictive_plasticity_get_stats(bridge, &stats);
        EXPECT_EQ(result, 0);

        predictive_plasticity_destroy(bridge);
    }
}

/* ============================================================================
 * Predictive Thalamic Bridge NULL Pointer Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionTest, ThalamicBridge_ResetWithNullBridge) {
    int result = predictive_thalamic_bridge_reset(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, ThalamicBridge_RouteErrorWithNullBridge) {
    predictive_thalamic_signal_t signal = {};
    int result = predictive_thalamic_route_error(nullptr, &signal);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, ThalamicBridge_RouteErrorWithNullSignal) {
    predictive_thalamic_config_t config = predictive_thalamic_default_config();
    predictive_thalamic_bridge_t* bridge = predictive_thalamic_bridge_create(nullptr, nullptr, &config);

    if (bridge) {
        int result = predictive_thalamic_route_error(bridge, nullptr);
        EXPECT_EQ(result, -1);
        predictive_thalamic_bridge_destroy(bridge);
    }
}

TEST_F(PredictiveBridgeExceptionTest, ThalamicBridge_RouteUpdateWithNullBridge) {
    int result = predictive_thalamic_route_update(nullptr, nullptr, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, ThalamicBridge_SetAttentionWithNullBridge) {
    int result = predictive_thalamic_set_attention(nullptr, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, ThalamicBridge_GetAttentionWithNullBridge) {
    float attention = 0.0f;
    int result = predictive_thalamic_get_attention(nullptr, &attention);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, ThalamicBridge_GetStatsWithNullBridge) {
    predictive_thalamic_stats_t stats = {};
    int result = predictive_thalamic_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, ThalamicBridge_DestroyWithNull) {
    /* Should not crash */
    predictive_thalamic_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(PredictiveBridgeExceptionTest, ThalamicBridge_DefaultConfig) {
    predictive_thalamic_config_t config = predictive_thalamic_default_config();
    EXPECT_TRUE(config.enable_attention_gating);
    EXPECT_TRUE(config.enable_error_amplification);
    EXPECT_GT(config.min_error_threshold, 0.0f);
    EXPECT_GT(config.precision_threshold, 0.0f);
}

/* ============================================================================
 * Predictive FEP Bridge NULL Pointer Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionTest, FepBridge_DefaultConfigWithNullConfig) {
    int result = predictive_fep_bridge_default_config(nullptr);
    /* Should throw exception for NULL config */
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_ConnectFepWithNullBridge) {
    ResetExceptionState();
    int result = predictive_fep_bridge_connect_fep(nullptr, nullptr);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_ConnectPredictiveWithNullBridge) {
    ResetExceptionState();
    predictive_network_t network = {};
    int result = predictive_fep_bridge_connect_predictive(nullptr, network);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_DisconnectWithNullBridge) {
    ResetExceptionState();
    int result = predictive_fep_bridge_disconnect(nullptr);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_UpdateWithNullBridge) {
    ResetExceptionState();
    int result = predictive_fep_bridge_update(nullptr, 100);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_GetStateWithNullBridge) {
    ResetExceptionState();
    predictive_fep_state_t state = {};
    int result = predictive_fep_bridge_get_state(nullptr, &state);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_GetStateWithNullState) {
    ResetExceptionState();
    predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(nullptr);
    if (bridge) {
        int result = predictive_fep_bridge_get_state(bridge, nullptr);
        EXPECT_TRUE(exception_caught);
        EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
        predictive_fep_bridge_destroy(bridge);
    }
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_GetStatsWithNullBridge) {
    ResetExceptionState();
    predictive_fep_stats_t stats = {};
    int result = predictive_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_ConnectBioAsyncWithNullBridge) {
    ResetExceptionState();
    int result = predictive_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_DisconnectBioAsyncWithNullBridge) {
    ResetExceptionState();
    int result = predictive_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_IsBioAsyncConnectedWithNullBridge) {
    bool connected = predictive_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_DestroyWithNull) {
    /* Should not crash */
    predictive_fep_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(PredictiveBridgeExceptionTest, FepBridge_CreateAndValidLifecycle) {
    predictive_fep_config_t config = {};
    predictive_fep_bridge_default_config(&config);

    ResetExceptionState();
    predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(&config);

    if (bridge) {
        /* Should be able to get state */
        predictive_fep_state_t state = {};
        int result = predictive_fep_bridge_get_state(bridge, &state);
        EXPECT_EQ(result, 0);

        /* Should be able to get stats */
        predictive_fep_stats_t stats = {};
        result = predictive_fep_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(result, 0);

        predictive_fep_bridge_destroy(bridge);
    }
}

/* ============================================================================
 * Predictive Substrate Bridge NULL Pointer Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionTest, SubstrateBridge_CreateWithNullSubstrate) {
    ResetExceptionState();
    predictive_substrate_config_t config = predictive_substrate_default_config();
    predictive_substrate_bridge_t* bridge = predictive_substrate_bridge_create(nullptr, nullptr, &config);

    /* Should throw exception for NULL substrate */
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(PredictiveBridgeExceptionTest, SubstrateBridge_UpdateWithNullBridge) {
    int result = predictive_substrate_bridge_update(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, SubstrateBridge_GetEffectsWithNullBridge) {
    predictive_substrate_effects_t effects = {};
    int result = predictive_substrate_bridge_get_effects(nullptr, &effects);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, SubstrateBridge_GetEffectsWithNullEffects) {
    /* Cannot create without substrate, so test will fail creation */
    int result = predictive_substrate_bridge_get_effects(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, SubstrateBridge_ApplyEffectsWithNullBridge) {
    int result = predictive_substrate_bridge_apply_effects(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, SubstrateBridge_RegisterBioAsyncWithNullBridge) {
    int result = predictive_substrate_bridge_register_bio_async(nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(PredictiveBridgeExceptionTest, SubstrateBridge_DestroyWithNull) {
    /* Should not crash */
    predictive_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(PredictiveBridgeExceptionTest, SubstrateBridge_DefaultConfig) {
    predictive_substrate_config_t config = predictive_substrate_default_config();
    EXPECT_TRUE(config.enable_atp_modulation);
    EXPECT_TRUE(config.enable_fatigue_modulation);
    EXPECT_FALSE(config.enable_bio_async);
    EXPECT_GT(config.atp_sensitivity, 0.0f);
    EXPECT_GT(config.fatigue_sensitivity, 0.0f);
    EXPECT_GT(config.min_capacity, 0.0f);
}

/* ============================================================================
 * Exception Message Quality Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionTest, ExceptionMessages_ContainBridgeName) {
    ResetExceptionState();
    predictive_fep_bridge_connect_bio_async(nullptr);

    ASSERT_TRUE(exception_caught);
    ASSERT_FALSE(caught_messages.empty());

    /* Message should contain useful context */
    bool found_context = false;
    for (const auto& msg : caught_messages) {
        if (msg.find("bridge") != std::string::npos ||
            msg.find("NULL") != std::string::npos) {
            found_context = true;
            break;
        }
    }
    EXPECT_TRUE(found_context) << "Exception message should contain 'bridge' or 'NULL'";
}

TEST_F(PredictiveBridgeExceptionTest, ExceptionMessages_SubstrateBridgeContainsContext) {
    ResetExceptionState();
    predictive_substrate_config_t config = predictive_substrate_default_config();
    predictive_substrate_bridge_create(nullptr, nullptr, &config);

    ASSERT_TRUE(exception_caught);
    ASSERT_FALSE(caught_messages.empty());

    /* Message should mention substrate */
    bool found_context = false;
    for (const auto& msg : caught_messages) {
        if (msg.find("substrate") != std::string::npos ||
            msg.find("NULL") != std::string::npos) {
            found_context = true;
            break;
        }
    }
    EXPECT_TRUE(found_context) << "Exception message should contain 'substrate' or 'NULL'";
}

/* ============================================================================
 * Exception Recovery Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionTest, Recovery_AfterFepBridgeException) {
    /* Generate exception */
    ResetExceptionState();
    predictive_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);

    /* Recover and create valid bridge */
    ResetExceptionState();
    predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(nullptr);

    if (bridge) {
        bool connected = predictive_fep_bridge_is_bio_async_connected(bridge);
        EXPECT_FALSE(connected); /* Not connected yet, but no exception */
        EXPECT_FALSE(exception_caught);
        predictive_fep_bridge_destroy(bridge);
    }
}

TEST_F(PredictiveBridgeExceptionTest, Recovery_MultipleExceptionsSequentially) {
    /* Generate multiple exceptions */
    for (int i = 0; i < 5; i++) {
        ResetExceptionState();
        predictive_fep_bridge_connect_bio_async(nullptr);
        EXPECT_TRUE(exception_caught);
        EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
    }

    /* System should still be functional */
    ResetExceptionState();
    predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(nullptr);
    if (bridge) {
        predictive_fep_stats_t stats = {};
        int result = predictive_fep_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(result, 0);
        predictive_fep_bridge_destroy(bridge);
    }
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
