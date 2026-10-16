/**
 * @file test_predictive_bridge_exception_regression.cpp
 * @brief Regression tests for Predictive Bridge API contract stability
 *
 * WHAT: Tests API contract stability and error code consistency
 * WHY:  Ensure exception handling behavior does not regress across versions
 * HOW:  Test specific error codes, return values, and API contracts
 *
 * REGRESSION SCENARIOS:
 * - Error code consistency for NULL pointers
 * - Return value contracts for all bridge functions
 * - Exception message format stability
 * - Default configuration value stability
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

class PredictiveBridgeExceptionRegressionTest : public ::testing::Test {
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
        options.name = "test_predictive_regression_handler";
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
        return false;
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
std::atomic<int> PredictiveBridgeExceptionRegressionTest::handler_call_count(0);
std::atomic<int> PredictiveBridgeExceptionRegressionTest::last_exception_code(0);
std::atomic<bool> PredictiveBridgeExceptionRegressionTest::exception_caught(false);
std::vector<std::string> PredictiveBridgeExceptionRegressionTest::caught_messages;
nimcp_handler_registration_t* PredictiveBridgeExceptionRegressionTest::test_handler_reg = nullptr;

/* ============================================================================
 * API Contract: Error Code Consistency Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionRegressionTest, FepBridge_NullPointerErrorCodeConsistency) {
    /* All FEP bridge NULL pointer errors should use NIMCP_ERROR_NULL_POINTER */
    ResetExceptionState();
    predictive_fep_bridge_default_config(nullptr);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    ResetExceptionState();
    predictive_fep_bridge_connect_fep(nullptr, nullptr);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    ResetExceptionState();
    predictive_network_t network = {};
    predictive_fep_bridge_connect_predictive(nullptr, network);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    ResetExceptionState();
    predictive_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    ResetExceptionState();
    predictive_fep_bridge_update(nullptr, 100);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    ResetExceptionState();
    predictive_fep_state_t state = {};
    predictive_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    ResetExceptionState();
    predictive_fep_stats_t stats = {};
    predictive_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    ResetExceptionState();
    predictive_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    ResetExceptionState();
    predictive_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(PredictiveBridgeExceptionRegressionTest, SubstrateBridge_NullSubstrateErrorCode) {
    /* Substrate bridge with NULL substrate should throw NIMCP_ERROR_NULL_POINTER */
    ResetExceptionState();
    predictive_substrate_config_t config = predictive_substrate_default_config();
    predictive_substrate_bridge_t* bridge = predictive_substrate_bridge_create(nullptr, nullptr, &config);
    EXPECT_EQ(bridge, nullptr);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * API Contract: Return Value Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionRegressionTest, SnnBridge_ReturnValueContracts) {
    /* NULL bridge operations should return -1 */
    EXPECT_EQ(predictive_snn_reset(nullptr), -1);

    float dims[] = {0.5f};
    EXPECT_EQ(predictive_snn_encode_state(nullptr, dims, 1), -1);

    predictive_snn_stats_t stats = {};
    EXPECT_EQ(predictive_snn_get_stats(nullptr, &stats), -1);
}

TEST_F(PredictiveBridgeExceptionRegressionTest, PlasticityBridge_ReturnValueContracts) {
    /* NULL bridge operations should return -1 */
    EXPECT_EQ(predictive_plasticity_reset(nullptr), -1);
    EXPECT_EQ(predictive_plasticity_consolidate(nullptr), -1);

    predictive_plasticity_stats_t stats = {};
    EXPECT_EQ(predictive_plasticity_get_stats(nullptr, &stats), -1);
}

TEST_F(PredictiveBridgeExceptionRegressionTest, ThalamicBridge_ReturnValueContracts) {
    /* NULL bridge operations should return -1 */
    EXPECT_EQ(predictive_thalamic_bridge_reset(nullptr), -1);

    predictive_thalamic_signal_t signal = {};
    EXPECT_EQ(predictive_thalamic_route_error(nullptr, &signal), -1);
    EXPECT_EQ(predictive_thalamic_route_update(nullptr, nullptr, 0), -1);
    EXPECT_EQ(predictive_thalamic_set_attention(nullptr, 0.5f), -1);

    float attention = 0.0f;
    EXPECT_EQ(predictive_thalamic_get_attention(nullptr, &attention), -1);

    predictive_thalamic_stats_t stats = {};
    EXPECT_EQ(predictive_thalamic_bridge_get_stats(nullptr, &stats), -1);
}

TEST_F(PredictiveBridgeExceptionRegressionTest, SubstrateBridge_ReturnValueContracts) {
    /* NULL bridge operations should return -1 */
    EXPECT_EQ(predictive_substrate_bridge_update(nullptr), -1);

    predictive_substrate_effects_t effects = {};
    EXPECT_EQ(predictive_substrate_bridge_get_effects(nullptr, &effects), -1);
    EXPECT_EQ(predictive_substrate_bridge_apply_effects(nullptr), -1);
    EXPECT_EQ(predictive_substrate_bridge_register_bio_async(nullptr, nullptr), -1);
}

/* ============================================================================
 * API Contract: Default Configuration Stability
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionRegressionTest, SnnBridge_DefaultConfigStability) {
    predictive_snn_config_t config = predictive_snn_config_default();

    /* These values should remain stable across versions */
    EXPECT_GT(config.num_dimensions, 0u);
    EXPECT_GT(config.neurons_per_dim, 0u);
}

TEST_F(PredictiveBridgeExceptionRegressionTest, PlasticityBridge_DefaultConfigStability) {
    predictive_plasticity_config_t config = predictive_plasticity_config_default();

    /* These values should remain stable across versions */
    EXPECT_GT(config.base_learning_rate, 0.0f);
    EXPECT_LE(config.base_learning_rate, 1.0f);
}

TEST_F(PredictiveBridgeExceptionRegressionTest, ThalamicBridge_DefaultConfigStability) {
    predictive_thalamic_config_t config = predictive_thalamic_default_config();

    /* These values should remain stable across versions */
    EXPECT_TRUE(config.enable_attention_gating);
    EXPECT_TRUE(config.enable_error_amplification);
    EXPECT_GT(config.min_error_threshold, 0.0f);
    EXPECT_LT(config.min_error_threshold, 1.0f);
    EXPECT_GT(config.precision_threshold, 0.0f);
    EXPECT_LE(config.precision_threshold, 1.0f);
}

TEST_F(PredictiveBridgeExceptionRegressionTest, SubstrateBridge_DefaultConfigStability) {
    predictive_substrate_config_t config = predictive_substrate_default_config();

    /* These values should remain stable across versions */
    EXPECT_TRUE(config.enable_atp_modulation);
    EXPECT_TRUE(config.enable_fatigue_modulation);
    EXPECT_FALSE(config.enable_bio_async); /* Bio-async disabled by default */
    EXPECT_EQ(config.atp_sensitivity, 1.0f);
    EXPECT_EQ(config.fatigue_sensitivity, 1.0f);
    EXPECT_GT(config.min_capacity, 0.0f);
    EXPECT_LT(config.min_capacity, 1.0f);
}

/* ============================================================================
 * API Contract: Bio-Async Connection Status
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionRegressionTest, FepBridge_BioAsyncConnectionStatus) {
    /* NULL bridge should return false for connection status */
    EXPECT_FALSE(predictive_fep_bridge_is_bio_async_connected(nullptr));

    /* New bridge should not be connected by default */
    predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(nullptr);
    if (bridge) {
        EXPECT_FALSE(predictive_fep_bridge_is_bio_async_connected(bridge));
        predictive_fep_bridge_destroy(bridge);
    }
}

/* ============================================================================
 * API Contract: Destroy with NULL Safety
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionRegressionTest, AllBridges_DestroyNullSafety) {
    /* All destroy functions should safely handle NULL */
    EXPECT_NO_THROW(predictive_snn_destroy(nullptr));
    EXPECT_NO_THROW(predictive_plasticity_destroy(nullptr));
    EXPECT_NO_THROW(predictive_thalamic_bridge_destroy(nullptr));
    EXPECT_NO_THROW(predictive_fep_bridge_destroy(nullptr));
    EXPECT_NO_THROW(predictive_substrate_bridge_destroy(nullptr));
}

/* ============================================================================
 * Exception Message Format Stability
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionRegressionTest, ExceptionMessage_ContainsNullReference) {
    ResetExceptionState();
    predictive_fep_bridge_connect_bio_async(nullptr);

    ASSERT_TRUE(exception_caught);
    ASSERT_FALSE(caught_messages.empty());

    /* Exception message should contain reference to NULL or nullptr */
    const std::string& msg = caught_messages[0];
    bool contains_null_ref = (msg.find("NULL") != std::string::npos) ||
                             (msg.find("null") != std::string::npos) ||
                             (msg.find("nullptr") != std::string::npos);
    EXPECT_TRUE(contains_null_ref)
        << "Exception message should reference NULL: " << msg;
}

TEST_F(PredictiveBridgeExceptionRegressionTest, ExceptionMessage_ContainsBridgeReference) {
    ResetExceptionState();
    predictive_fep_bridge_connect_bio_async(nullptr);

    ASSERT_TRUE(exception_caught);
    ASSERT_FALSE(caught_messages.empty());

    /* Exception message should contain reference to bridge */
    const std::string& msg = caught_messages[0];
    bool contains_bridge_ref = (msg.find("bridge") != std::string::npos) ||
                               (msg.find("Bridge") != std::string::npos);
    EXPECT_TRUE(contains_bridge_ref)
        << "Exception message should reference bridge: " << msg;
}

/* ============================================================================
 * Lifecycle Consistency Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionRegressionTest, FepBridge_CreateDestroyIdempotent) {
    /* Multiple create/destroy cycles should be safe */
    for (int i = 0; i < 10; i++) {
        predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(nullptr);
        if (bridge) {
            predictive_fep_bridge_destroy(bridge);
        }
    }
    SUCCEED();
}

TEST_F(PredictiveBridgeExceptionRegressionTest, ThalamicBridge_CreateDestroyIdempotent) {
    /* Multiple create/destroy cycles should be safe */
    for (int i = 0; i < 10; i++) {
        predictive_thalamic_config_t config = predictive_thalamic_default_config();
        predictive_thalamic_bridge_t* bridge = predictive_thalamic_bridge_create(nullptr, nullptr, &config);
        if (bridge) {
            predictive_thalamic_bridge_destroy(bridge);
        }
    }
    SUCCEED();
}

/* ============================================================================
 * Stats Initial State Consistency
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionRegressionTest, FepBridge_InitialStatsZero) {
    predictive_fep_bridge_t* bridge = predictive_fep_bridge_create(nullptr);
    if (!bridge) GTEST_SKIP() << "FEP bridge creation failed";

    predictive_fep_stats_t stats = {};
    int result = predictive_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    /* Initial stats should be zero */
    EXPECT_EQ(stats.belief_syncs, 0u);
    EXPECT_EQ(stats.precision_updates, 0u);
    EXPECT_EQ(stats.gradient_flows, 0u);
    EXPECT_EQ(stats.convergence_count, 0u);

    predictive_fep_bridge_destroy(bridge);
}

TEST_F(PredictiveBridgeExceptionRegressionTest, ThalamicBridge_InitialStatsZero) {
    predictive_thalamic_config_t config = predictive_thalamic_default_config();
    predictive_thalamic_bridge_t* bridge = predictive_thalamic_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    predictive_thalamic_stats_t stats = {};
    int result = predictive_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    /* Initial stats should be zero */
    EXPECT_EQ(stats.errors_routed, 0u);
    EXPECT_EQ(stats.predictions_routed, 0u);
    EXPECT_EQ(stats.updates_triggered, 0u);
    EXPECT_FLOAT_EQ(stats.avg_error_magnitude, 0.0f);

    predictive_thalamic_bridge_destroy(bridge);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
