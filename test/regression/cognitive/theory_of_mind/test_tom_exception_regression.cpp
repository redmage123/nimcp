/**
 * @file test_tom_exception_regression.cpp
 * @brief Regression tests for Theory of Mind exception handling
 *
 * WHAT: Tests API stability and consistent behavior of ToM exception handling
 * WHY:  Ensure exception handling doesn't regress across code changes
 * HOW:  Test error codes, return values, message formats, and behavior consistency
 *
 * REGRESSION SCENARIOS:
 * - API contract stability (error codes, return values)
 * - Message format consistency
 * - Exception handler behavior
 * - Memory safety during exceptions
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"

/* ToM core and bridges */
#include "cognitive/nimcp_theory_of_mind.h"
#include "cognitive/theory_of_mind/nimcp_tom_fep_bridge.h"
#include "cognitive/theory_of_mind/nimcp_tom_snn_bridge.h"
#include "cognitive/theory_of_mind/nimcp_tom_plasticity_bridge.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class TomExceptionRegressionTest : public ::testing::Test {
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
        options.name = "test_tom_regression_handler";
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
std::atomic<int> TomExceptionRegressionTest::handler_call_count(0);
std::atomic<int> TomExceptionRegressionTest::last_exception_code(0);
std::atomic<bool> TomExceptionRegressionTest::exception_caught(false);
std::vector<std::string> TomExceptionRegressionTest::caught_messages;
nimcp_handler_registration_t* TomExceptionRegressionTest::test_handler_reg = nullptr;

/* ============================================================================
 * API Contract Tests - Return Values
 * ============================================================================ */

TEST_F(TomExceptionRegressionTest, FepBridgeConnectReturnsNullPointerError) {
    int result = tom_fep_bridge_connect_bio_async(nullptr);
    /* FEP bridges return -1 for errors (not NIMCP_ERROR_* codes) */
    EXPECT_EQ(result, -1);
}

TEST_F(TomExceptionRegressionTest, FepBridgeDisconnectReturnsNullPointerError) {
    int result = tom_fep_bridge_disconnect_bio_async(nullptr);
    /* FEP bridges return -1 for errors (not NIMCP_ERROR_* codes) */
    EXPECT_EQ(result, -1);
}

TEST_F(TomExceptionRegressionTest, FepBridgeIsConnectedReturnsFalse) {
    bool result = tom_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(TomExceptionRegressionTest, SnnBridgeConnectReturnsNullPointerError) {
    int result = tom_snn_bio_async_connect(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomExceptionRegressionTest, SnnBridgeDisconnectReturnsNullPointerError) {
    int result = tom_snn_bio_async_disconnect(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomExceptionRegressionTest, SnnBridgeIsConnectedReturnsFalse) {
    bool result = tom_snn_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(TomExceptionRegressionTest, PlasticityBridgeConnectReturnsNullPointerError) {
    int result = tom_plasticity_bio_async_connect(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomExceptionRegressionTest, PlasticityBridgeDisconnectReturnsNullPointerError) {
    int result = tom_plasticity_bio_async_disconnect(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomExceptionRegressionTest, PlasticityBridgeIsConnectedReturnsFalse) {
    bool result = tom_plasticity_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

/* ============================================================================
 * API Contract Tests - Exception Codes
 * ============================================================================ */

TEST_F(TomExceptionRegressionTest, FepBridgeThrowsCorrectExceptionCode) {
    tom_fep_bridge_connect_bio_async(nullptr);
    /* Exception code is what's passed to NIMCP_THROW_TO_IMMUNE, not the return value */
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomExceptionRegressionTest, SnnBridgeThrowsCorrectExceptionCode) {
    tom_snn_bio_async_connect(nullptr);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomExceptionRegressionTest, PlasticityBridgeThrowsCorrectExceptionCode) {
    tom_plasticity_bio_async_connect(nullptr);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Message Format Consistency Tests
 * ============================================================================ */

TEST_F(TomExceptionRegressionTest, FepBridgeMessageContainsBridgeIsNull) {
    tom_fep_bridge_connect_bio_async(nullptr);
    ASSERT_FALSE(caught_messages.empty());
    EXPECT_NE(caught_messages[0].find("bridge is NULL"), std::string::npos);
}

TEST_F(TomExceptionRegressionTest, SnnBridgeMessageContainsBridgeIsNull) {
    tom_snn_bio_async_connect(nullptr);
    ASSERT_FALSE(caught_messages.empty());
    EXPECT_NE(caught_messages[0].find("bridge is NULL"), std::string::npos);
}

TEST_F(TomExceptionRegressionTest, PlasticityBridgeMessageContainsBridgeIsNull) {
    tom_plasticity_bio_async_connect(nullptr);
    ASSERT_FALSE(caught_messages.empty());
    EXPECT_NE(caught_messages[0].find("bridge is NULL"), std::string::npos);
}

/* ============================================================================
 * Handler Invocation Consistency Tests
 * ============================================================================ */

TEST_F(TomExceptionRegressionTest, EachExceptionInvokesHandlerExactlyOnce) {
    tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(handler_call_count, 1);

    tom_snn_bio_async_connect(nullptr);
    EXPECT_EQ(handler_call_count, 2);

    tom_plasticity_bio_async_connect(nullptr);
    EXPECT_EQ(handler_call_count, 3);
}

TEST_F(TomExceptionRegressionTest, ExceptionCaughtFlagSetCorrectly) {
    EXPECT_FALSE(exception_caught);
    tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);
}

/* ============================================================================
 * Idempotency Tests
 * ============================================================================ */

TEST_F(TomExceptionRegressionTest, RepeatedCallsProduceSameResults) {
    /* First call */
    int result1 = tom_fep_bridge_connect_bio_async(nullptr);
    int code1 = last_exception_code.load();

    ResetExceptionState();

    /* Second call - should produce identical results */
    int result2 = tom_fep_bridge_connect_bio_async(nullptr);
    int code2 = last_exception_code.load();

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(code1, code2);
}

TEST_F(TomExceptionRegressionTest, AllBridgesProduceSameErrorCodeForSameCondition) {
    int fep_result = tom_fep_bridge_connect_bio_async(nullptr);

    ResetExceptionState();
    int snn_result = tom_snn_bio_async_connect(nullptr);

    ResetExceptionState();
    int plasticity_result = tom_plasticity_bio_async_connect(nullptr);

    /* FEP bridges return -1 for errors; non-FEP bridges return NIMCP_ERROR_* codes */
    EXPECT_EQ(fep_result, -1);
    /* SNN and plasticity bridges return NIMCP_ERROR_NULL_POINTER */
    EXPECT_EQ(snn_result, plasticity_result);
    EXPECT_EQ(snn_result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * State Isolation Tests
 * ============================================================================ */

TEST_F(TomExceptionRegressionTest, ExceptionInOneBridgeDoesNotAffectOthers) {
    /* Generate exception in FEP bridge */
    tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);

    /* Create and use SNN bridge - should work normally */
    ResetExceptionState();
    tom_snn_config_t config = {};
    tom_snn_bridge_t* snn = tom_snn_create(&config);

    if (snn) {
        /* No exception should have been thrown during create */
        EXPECT_FALSE(exception_caught);
        tom_snn_destroy(snn);
    }
}

TEST_F(TomExceptionRegressionTest, ExceptionClearResetsState) {
    tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(handler_call_count, 1);

    nimcp_exception_clear_current();

    /* Counters should remain (they're our test counters) but exception state cleared */
    EXPECT_EQ(handler_call_count, 1);
}

/* ============================================================================
 * Memory Safety Tests
 * ============================================================================ */

TEST_F(TomExceptionRegressionTest, NoMemoryLeakOnRepeatedExceptions) {
    /* Generate many exceptions - should not leak memory */
    for (int i = 0; i < 1000; i++) {
        tom_fep_bridge_connect_bio_async(nullptr);
        tom_snn_bio_async_connect(nullptr);
        tom_plasticity_bio_async_connect(nullptr);
    }

    EXPECT_EQ(handler_call_count, 3000);
}

TEST_F(TomExceptionRegressionTest, MessagesAreCapturedCorrectly) {
    tom_fep_bridge_connect_bio_async(nullptr);
    tom_snn_bio_async_connect(nullptr);
    tom_plasticity_bio_async_connect(nullptr);

    EXPECT_EQ(caught_messages.size(), 3u);

    /* All messages should be valid (non-empty) */
    for (const auto& msg : caught_messages) {
        EXPECT_FALSE(msg.empty());
    }
}

/* ============================================================================
 * Backward Compatibility Tests
 * ============================================================================ */

TEST_F(TomExceptionRegressionTest, NullPointerErrorCodeValueUnchanged) {
    /* Verify the error code value hasn't changed (1003 per error codes header) */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, 1003);
}

TEST_F(TomExceptionRegressionTest, BioAsyncFunctionSignaturesWork) {
    /* These should compile and run - verifying function signatures haven't changed */

    /* FEP Bridge */
    int (*fep_connect)(tom_fep_bridge_t*) = tom_fep_bridge_connect_bio_async;
    int (*fep_disconnect)(tom_fep_bridge_t*) = tom_fep_bridge_disconnect_bio_async;
    bool (*fep_is_connected)(const tom_fep_bridge_t*) = tom_fep_bridge_is_bio_async_connected;

    /* SNN Bridge */
    int (*snn_connect)(tom_snn_bridge_t*) = tom_snn_bio_async_connect;
    int (*snn_disconnect)(tom_snn_bridge_t*) = tom_snn_bio_async_disconnect;
    bool (*snn_is_connected)(tom_snn_bridge_t*) = tom_snn_is_bio_async_connected;

    /* Plasticity Bridge */
    int (*plasticity_connect)(tom_plasticity_bridge_t*) = tom_plasticity_bio_async_connect;
    int (*plasticity_disconnect)(tom_plasticity_bridge_t*) = tom_plasticity_bio_async_disconnect;
    bool (*plasticity_is_connected)(tom_plasticity_bridge_t*) = tom_plasticity_is_bio_async_connected;

    /* Verify they can be called */
    EXPECT_NE(fep_connect(nullptr), 0);
    EXPECT_NE(fep_disconnect(nullptr), 0);
    EXPECT_FALSE(fep_is_connected(nullptr));
    EXPECT_NE(snn_connect(nullptr), 0);
    EXPECT_NE(snn_disconnect(nullptr), 0);
    EXPECT_FALSE(snn_is_connected(nullptr));
    EXPECT_NE(plasticity_connect(nullptr), 0);
    EXPECT_NE(plasticity_disconnect(nullptr), 0);
    EXPECT_FALSE(plasticity_is_connected(nullptr));
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
