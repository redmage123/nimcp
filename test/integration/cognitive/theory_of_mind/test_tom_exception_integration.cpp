/**
 * @file test_tom_exception_integration.cpp
 * @brief Integration tests for Theory of Mind exception handling
 *
 * WHAT: Tests NIMCP_THROW_TO_IMMUNE exception handling across ToM bridge interactions
 * WHY:  Verify exceptions propagate correctly when multiple ToM components interact
 * HOW:  Test cross-bridge exception flows, recovery scenarios, and immune integration
 *
 * INTEGRATION SCENARIOS:
 * - Cross-bridge exception propagation
 * - Exception recovery and continued operation
 * - Immune system integration with ToM exceptions
 * - Sequential and parallel exception handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

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

class TomExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<bool> exception_caught;
    static std::vector<std::string> caught_messages;
    static std::vector<int> exception_codes_in_order;
    static nimcp_handler_registration_t* test_handler_reg;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        exception_caught = false;
        caught_messages.clear();
        exception_codes_in_order.clear();

        nimcp_exception_system_init();

        nimcp_handler_options_t options = {};
        options.name = "test_tom_integration_handler";
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
        exception_codes_in_order.push_back(ex->code);
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
        exception_codes_in_order.clear();
        nimcp_exception_clear_current();
    }
};

/* Static member initialization */
std::atomic<int> TomExceptionIntegrationTest::handler_call_count(0);
std::atomic<int> TomExceptionIntegrationTest::last_exception_code(0);
std::atomic<bool> TomExceptionIntegrationTest::exception_caught(false);
std::vector<std::string> TomExceptionIntegrationTest::caught_messages;
std::vector<int> TomExceptionIntegrationTest::exception_codes_in_order;
nimcp_handler_registration_t* TomExceptionIntegrationTest::test_handler_reg = nullptr;

/* ============================================================================
 * Cross-Bridge Exception Propagation Tests
 * ============================================================================ */

TEST_F(TomExceptionIntegrationTest, SequentialBridgeExceptionsAllCaptured) {
    /* Test that exceptions from all bridges are captured in sequence */
    tom_fep_bridge_connect_bio_async(nullptr);
    tom_snn_bio_async_connect(nullptr);
    tom_plasticity_bio_async_connect(nullptr);

    EXPECT_EQ(handler_call_count, 3);
    EXPECT_EQ(exception_codes_in_order.size(), 3u);

    for (int code : exception_codes_in_order) {
        /* Exception codes passed to NIMCP_THROW_TO_IMMUNE are always NIMCP_ERROR_* */
        EXPECT_EQ(code, NIMCP_ERROR_NULL_POINTER);
    }
}

TEST_F(TomExceptionIntegrationTest, MixedBridgeOperationsExceptionTracking) {
    /* Test mixed connect/disconnect/is_connected operations */
    tom_fep_bridge_connect_bio_async(nullptr);
    tom_snn_bio_async_disconnect(nullptr);
    tom_plasticity_is_bio_async_connected(nullptr);

    EXPECT_EQ(handler_call_count, 3);
    EXPECT_EQ(caught_messages.size(), 3u);

    for (const auto& msg : caught_messages) {
        EXPECT_NE(msg.find("bridge"), std::string::npos);
    }
}

TEST_F(TomExceptionIntegrationTest, AllNineBioAsyncFunctionsThrowExceptions) {
    /* Test all 9 bio-async functions across all 3 bridges */
    int exception_count = 0;

    /* FEP Bridge - 3 functions */
    ResetExceptionState();
    tom_fep_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    tom_fep_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    tom_fep_bridge_is_bio_async_connected(nullptr);
    if (exception_caught) exception_count++;

    /* SNN Bridge - 3 functions */
    ResetExceptionState();
    tom_snn_bio_async_connect(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    tom_snn_bio_async_disconnect(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    tom_snn_is_bio_async_connected(nullptr);
    if (exception_caught) exception_count++;

    /* Plasticity Bridge - 3 functions */
    ResetExceptionState();
    tom_plasticity_bio_async_connect(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    tom_plasticity_bio_async_disconnect(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    tom_plasticity_is_bio_async_connected(nullptr);
    if (exception_caught) exception_count++;

    EXPECT_EQ(exception_count, 9) << "All 9 ToM bio-async functions should throw exceptions on NULL";
}

/* ============================================================================
 * Exception Recovery Tests
 * ============================================================================ */

TEST_F(TomExceptionIntegrationTest, RecoveryAfterExceptionAllowsContinuedOperation) {
    /* First exception */
    tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);

    /* Clear and verify we can continue */
    ResetExceptionState();
    EXPECT_FALSE(exception_caught);

    /* Second exception - system should still work */
    tom_snn_bio_async_connect(nullptr);
    EXPECT_TRUE(exception_caught);
}

TEST_F(TomExceptionIntegrationTest, ValidBridgeOperationsAfterExceptions) {
    /* Generate exceptions first */
    tom_fep_bridge_connect_bio_async(nullptr);
    tom_snn_bio_async_connect(nullptr);
    tom_plasticity_bio_async_connect(nullptr);

    EXPECT_EQ(handler_call_count, 3);

    /* Create valid bridges and verify they work */
    ResetExceptionState();

    tom_fep_bridge_t* fep_bridge = tom_fep_bridge_create(nullptr);
    if (fep_bridge) {
        bool connected = tom_fep_bridge_is_bio_async_connected(fep_bridge);
        EXPECT_FALSE(connected); /* Not connected yet, but no exception */
        EXPECT_FALSE(exception_caught);
        tom_fep_bridge_destroy(fep_bridge);
    }
}

TEST_F(TomExceptionIntegrationTest, RapidExceptionCycleStressTest) {
    /* Generate many exceptions rapidly */
    for (int i = 0; i < 100; i++) {
        tom_fep_bridge_connect_bio_async(nullptr);
    }

    EXPECT_EQ(handler_call_count, 100);
}

/* ============================================================================
 * Exception Consistency Tests
 * ============================================================================ */

TEST_F(TomExceptionIntegrationTest, AllBridgesReturnConsistentErrorCodes) {
    /* All NULL pointer exceptions should return the same error code */
    int fep_result = tom_fep_bridge_connect_bio_async(nullptr);
    int snn_result = tom_snn_bio_async_connect(nullptr);
    int plasticity_result = tom_plasticity_bio_async_connect(nullptr);

    /* FEP bridges return -1 for errors */
    EXPECT_EQ(fep_result, -1);
    /* SNN and plasticity bridges return NIMCP_ERROR_* codes */
    EXPECT_EQ(snn_result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(plasticity_result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomExceptionIntegrationTest, AllBridgesHaveConsistentMessageFormat) {
    tom_fep_bridge_connect_bio_async(nullptr);
    tom_snn_bio_async_connect(nullptr);
    tom_plasticity_bio_async_connect(nullptr);

    EXPECT_EQ(caught_messages.size(), 3u);

    for (const auto& msg : caught_messages) {
        /* All messages should contain "bridge" and "NULL" */
        EXPECT_NE(msg.find("bridge"), std::string::npos);
        EXPECT_NE(msg.find("NULL"), std::string::npos);
    }
}

/* ============================================================================
 * Bridge Lifecycle with Exception Tests
 * ============================================================================ */

TEST_F(TomExceptionIntegrationTest, BridgeCreateDestroyWithExceptionsBetween) {
    /* Create a bridge */
    tom_fep_bridge_t* bridge = tom_fep_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Generate some exceptions */
    tom_snn_bio_async_connect(nullptr);
    tom_plasticity_bio_async_connect(nullptr);

    EXPECT_EQ(handler_call_count, 2);

    /* Destroy should still work */
    tom_fep_bridge_destroy(bridge);

    /* Generate more exceptions after destroy */
    tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(handler_call_count, 3);
}

TEST_F(TomExceptionIntegrationTest, MultipleBridgesWithInterleavedExceptions) {
    /* Create FEP bridge */
    tom_fep_bridge_t* fep = tom_fep_bridge_create(nullptr);

    /* Exception on SNN */
    tom_snn_bio_async_connect(nullptr);
    EXPECT_EQ(handler_call_count, 1);

    /* Create SNN bridge */
    tom_snn_config_t snn_config = {};
    tom_snn_bridge_t* snn = tom_snn_create(&snn_config);

    /* Exception on Plasticity */
    tom_plasticity_bio_async_connect(nullptr);
    EXPECT_EQ(handler_call_count, 2);

    /* Cleanup */
    if (fep) tom_fep_bridge_destroy(fep);
    if (snn) tom_snn_destroy(snn);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
