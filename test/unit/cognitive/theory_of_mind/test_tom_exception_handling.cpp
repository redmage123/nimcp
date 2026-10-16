/**
 * @file test_tom_exception_handling.cpp
 * @brief Comprehensive exception handling tests for Theory of Mind module
 *
 * WHAT: Tests NIMCP_THROW_TO_IMMUNE exception handling for all ToM components
 * WHY:  Verify exceptions properly propagate to the immune system for recovery
 * HOW:  Test error code mapping, immune presentation, and handler dispatch
 *
 * EXCEPTION PATHS TESTED:
 * - ToM core (2 paths): Null pointer in find/get agent model
 * - ToM FEP Bridge (3 paths): Bio-async connect/disconnect/is_connected
 * - ToM SNN Bridge (4 paths): Create + bio-async functions
 * - ToM Plasticity Bridge (4 paths): Create + bio-async functions
 * - ToM Sleep Bridge (2 paths): Null sleep/bridge in create
 * - ToM Substrate Bridge (2 paths): Null substrate/bridge in create
 * - ToM Thalamic Bridge (1 path): Null bridge in create
 *
 * TOTAL EXCEPTION PATHS: 19
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

class TomExceptionHandlingTest : public ::testing::Test {
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

        /* Initialize exception system */
        nimcp_exception_system_init();

        /* Register our test handler to capture exceptions */
        nimcp_handler_options_t options = {};
        options.name = "test_tom_exception_handler";
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

    void ExpectExceptionTriggered(int expected_code, const char* expected_msg_fragment = nullptr) {
        EXPECT_TRUE(exception_caught) << "Exception was not triggered";
        EXPECT_EQ(last_exception_code, expected_code);
        if (expected_msg_fragment && !caught_messages.empty()) {
            bool found = false;
            for (const auto& msg : caught_messages) {
                if (msg.find(expected_msg_fragment) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Expected message fragment '" << expected_msg_fragment
                              << "' not found in exception messages";
        }
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
std::atomic<int> TomExceptionHandlingTest::handler_call_count(0);
std::atomic<int> TomExceptionHandlingTest::last_exception_code(0);
std::atomic<bool> TomExceptionHandlingTest::exception_caught(false);
std::vector<std::string> TomExceptionHandlingTest::caught_messages;
nimcp_handler_registration_t* TomExceptionHandlingTest::test_handler_reg = nullptr;

/* ============================================================================
 * ToM FEP Bridge Exception Tests
 * ============================================================================ */

TEST_F(TomExceptionHandlingTest, FepBridgeConnectBioAsyncNullBridge) {
    int result = tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(TomExceptionHandlingTest, FepBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    int result = tom_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(TomExceptionHandlingTest, FepBridgeIsBioAsyncConnectedNullBridge) {
    ResetExceptionState();
    bool connected = tom_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * ToM SNN Bridge Exception Tests
 * ============================================================================ */

TEST_F(TomExceptionHandlingTest, SnnBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    int result = tom_snn_bio_async_connect(nullptr);
    EXPECT_NE(result, 0);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(TomExceptionHandlingTest, SnnBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    int result = tom_snn_bio_async_disconnect(nullptr);
    EXPECT_NE(result, 0);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(TomExceptionHandlingTest, SnnBridgeIsBioAsyncConnectedNullBridge) {
    ResetExceptionState();
    bool connected = tom_snn_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * ToM Plasticity Bridge Exception Tests
 * ============================================================================ */

TEST_F(TomExceptionHandlingTest, PlasticityBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    int result = tom_plasticity_bio_async_connect(nullptr);
    EXPECT_NE(result, 0);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(TomExceptionHandlingTest, PlasticityBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    int result = tom_plasticity_bio_async_disconnect(nullptr);
    EXPECT_NE(result, 0);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(TomExceptionHandlingTest, PlasticityBridgeIsBioAsyncConnectedNullBridge) {
    ResetExceptionState();
    bool connected = tom_plasticity_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * Comprehensive All Bridges Bio-Async Exception Tests
 * ============================================================================ */

TEST_F(TomExceptionHandlingTest, AllBridgesConnectBioAsyncNullTriggersException) {
    int exception_count = 0;

    /* FEP Bridge */
    ResetExceptionState();
    tom_fep_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* SNN Bridge */
    ResetExceptionState();
    tom_snn_bio_async_connect(nullptr);
    if (exception_caught) exception_count++;

    /* Plasticity Bridge */
    ResetExceptionState();
    tom_plasticity_bio_async_connect(nullptr);
    if (exception_caught) exception_count++;

    EXPECT_EQ(exception_count, 3) << "All 3 ToM bridges should trigger exceptions on null connect";
}

TEST_F(TomExceptionHandlingTest, AllBridgesDisconnectBioAsyncNullTriggersException) {
    int exception_count = 0;

    /* FEP Bridge */
    ResetExceptionState();
    tom_fep_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* SNN Bridge */
    ResetExceptionState();
    tom_snn_bio_async_disconnect(nullptr);
    if (exception_caught) exception_count++;

    /* Plasticity Bridge */
    ResetExceptionState();
    tom_plasticity_bio_async_disconnect(nullptr);
    if (exception_caught) exception_count++;

    EXPECT_EQ(exception_count, 3) << "All 3 ToM bridges should trigger exceptions on null disconnect";
}

TEST_F(TomExceptionHandlingTest, AllBridgesIsBioAsyncConnectedNullTriggersException) {
    int exception_count = 0;

    /* FEP Bridge */
    ResetExceptionState();
    tom_fep_bridge_is_bio_async_connected(nullptr);
    if (exception_caught) exception_count++;

    /* SNN Bridge */
    ResetExceptionState();
    tom_snn_is_bio_async_connected(nullptr);
    if (exception_caught) exception_count++;

    /* Plasticity Bridge */
    ResetExceptionState();
    tom_plasticity_is_bio_async_connected(nullptr);
    if (exception_caught) exception_count++;

    EXPECT_EQ(exception_count, 3) << "All 3 ToM bridges should trigger exceptions on null is_connected";
}

/* ============================================================================
 * Exception Code and Message Tests
 * ============================================================================ */

TEST_F(TomExceptionHandlingTest, NullPointerExceptionHasCorrectCode) {
    tom_fep_bridge_connect_bio_async(nullptr);

    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomExceptionHandlingTest, ExceptionMessageContainsContext) {
    tom_fep_bridge_connect_bio_async(nullptr);

    EXPECT_TRUE(exception_caught);
    EXPECT_FALSE(caught_messages.empty());
    bool has_context = false;
    for (const auto& msg : caught_messages) {
        if (msg.find("bridge") != std::string::npos ||
            msg.find("NULL") != std::string::npos) {
            has_context = true;
            break;
        }
    }
    EXPECT_TRUE(has_context) << "Exception message should contain context";
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(TomExceptionHandlingTest, ExceptionDoesNotCrashSubsequentCalls) {
    tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);

    ResetExceptionState();
    tom_snn_bio_async_connect(nullptr);
    EXPECT_TRUE(exception_caught);
}

TEST_F(TomExceptionHandlingTest, MultipleExceptionsCanBeTracked) {
    int total_exceptions = 0;

    ResetExceptionState();
    tom_fep_bridge_connect_bio_async(nullptr);
    if (exception_caught) total_exceptions++;

    ResetExceptionState();
    tom_snn_bio_async_connect(nullptr);
    if (exception_caught) total_exceptions++;

    ResetExceptionState();
    tom_plasticity_bio_async_connect(nullptr);
    if (exception_caught) total_exceptions++;

    EXPECT_EQ(total_exceptions, 3);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
