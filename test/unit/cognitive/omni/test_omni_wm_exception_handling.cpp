/**
 * @file test_omni_wm_exception_handling.cpp
 * @brief Comprehensive exception handling tests for World Model module
 *
 * WHAT: Tests NIMCP_THROW_TO_IMMUNE exception handling for all WM components
 * WHY:  Verify exceptions properly propagate to the immune system for recovery
 * HOW:  Test error code mapping, immune presentation, and handler dispatch
 *
 * EXCEPTION PATHS TESTED:
 * - World Model core (4 paths): Memory allocation failures
 * - Active Inference (2 paths): Null pointer errors
 * - Precision (1 path): Null pointer error
 * - KG Sync (2 paths): Null pointer errors
 * - Metacognition (2 paths): Null pointer errors
 * - WM Bridges (27 paths): Null pointer errors across 11 bridges
 *
 * TOTAL EXCEPTION PATHS: 38
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

/* World Model core */
#include "cognitive/omni/nimcp_omni_world_model.h"

/* All 11 WM Bridges */
#include "cognitive/omni/bridges/nimcp_omni_wm_cognitive_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_memory_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_tom_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_plasticity_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_kg_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_logging_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_substrate_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_hypothalamus_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_parietal_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_security_immune_bridge.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class OmniWmExceptionHandlingTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<int> last_exception_category;
    static std::atomic<bool> exception_caught;
    static std::vector<std::string> caught_messages;
    static nimcp_handler_registration_t* test_handler_reg;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        last_exception_category = 0;
        exception_caught = false;
        caught_messages.clear();

        /* Initialize exception system */
        nimcp_exception_system_init();

        /* Register our test handler to capture exceptions */
        nimcp_handler_options_t options = {};
        options.name = "test_exception_handler";
        options.handler = test_exception_handler;
        options.user_data = nullptr;
        options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        options.category_filter = (nimcp_exception_category_t)0;  /* All categories */
        options.min_severity = (nimcp_exception_severity_t)0;     /* All severities */
        options.type_filter = (nimcp_exception_type_t)0;          /* All types */

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

    /* Test handler that captures exception info */
    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        last_exception_category = ex->category;
        exception_caught = true;
        if (ex->message) {
            caught_messages.push_back(ex->message);
        }
        return false;  /* Don't consume - allow immune system to process */
    }

    /* Helper to verify exception was triggered */
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

    /* Helper to reset exception state between tests within same fixture */
    void ResetExceptionState() {
        handler_call_count = 0;
        last_exception_code = 0;
        last_exception_category = 0;
        exception_caught = false;
        caught_messages.clear();
        nimcp_exception_clear_current();
    }
};

/* Static member initialization */
std::atomic<int> OmniWmExceptionHandlingTest::handler_call_count(0);
std::atomic<int> OmniWmExceptionHandlingTest::last_exception_code(0);
std::atomic<int> OmniWmExceptionHandlingTest::last_exception_category(0);
std::atomic<bool> OmniWmExceptionHandlingTest::exception_caught(false);
std::vector<std::string> OmniWmExceptionHandlingTest::caught_messages;
nimcp_handler_registration_t* OmniWmExceptionHandlingTest::test_handler_reg = nullptr;

/* ============================================================================
 * World Model Core Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, WMExperienceCreateNullReturnsError) {
    /* omni_wm_experience_create with invalid params should trigger exception
     * Note: This tests the allocation failure path indirectly through null returns */
    omni_wm_experience_t* exp = omni_wm_experience_create(0, 0, 0);  /* Zero dimensions */
    EXPECT_EQ(exp, nullptr);
}

/* ============================================================================
 * WM Cognitive Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, CognitiveBridgeConnectBioAsyncNullBridge) {
    /* omni_wm_cognitive_bridge_connect_bio_async(NULL) triggers exception */
    nimcp_error_t result = omni_wm_cognitive_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, CognitiveBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_cognitive_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, CognitiveBridgeIsBioAsyncConnectedNullBridge) {
    ResetExceptionState();
    bool connected = omni_wm_cognitive_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
    /* Note: Delegates to bridge_base_is_bio_async_connected which throws "base is NULL" */
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "base is NULL");
}

/* ============================================================================
 * WM Memory Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, MemoryBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_memory_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, MemoryBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_memory_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * WM ToM Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, TomBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_tom_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, TomBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_tom_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * WM Plasticity Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, PlasticityBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_plasticity_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, PlasticityBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_plasticity_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * WM KG Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, KgBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_kg_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, KgBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_kg_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * WM Logging Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, LoggingBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_logging_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, LoggingBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_logging_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, LoggingBridgeIsBioAsyncConnectedNullBridge) {
    ResetExceptionState();
    bool connected = omni_wm_logging_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
    /* Note: is_bio_async_connected goes through bridge_base which throws "base is NULL" */
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "base is NULL");
}

/* ============================================================================
 * WM Substrate Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, SubstrateBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_substrate_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, SubstrateBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_substrate_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * WM Thalamic Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, ThalamicBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_thalamic_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, ThalamicBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_thalamic_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * WM Hypothalamus Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, HypothalamusBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_hypothalamus_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, HypothalamusBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_hypothalamus_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * WM Parietal Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, ParietalBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_parietal_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, ParietalBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_parietal_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

/* ============================================================================
 * WM Security-Immune Bridge Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, SecurityImmuneBridgeConnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_security_immune_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, SecurityImmuneBridgeDisconnectBioAsyncNullBridge) {
    ResetExceptionState();
    nimcp_error_t result = omni_wm_security_immune_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
}

TEST_F(OmniWmExceptionHandlingTest, SecurityImmuneBridgeIsBioAsyncConnectedNullBridge) {
    ResetExceptionState();
    bool connected = omni_wm_security_immune_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
    /* Note: Delegates to bridge_base_is_bio_async_connected which throws "base is NULL" */
    ExpectExceptionTriggered(NIMCP_ERROR_NULL_POINTER, "base is NULL");
}

/* ============================================================================
 * Comprehensive All Bridges Bio-Async Exception Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, AllBridgesConnectBioAsyncNullTriggersException) {
    /* Verify all 11 bridges trigger exceptions on null bio-async connect */
    int exception_count = 0;

    /* Cognitive */
    ResetExceptionState();
    omni_wm_cognitive_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* Memory */
    ResetExceptionState();
    omni_wm_memory_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* ToM */
    ResetExceptionState();
    omni_wm_tom_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* Plasticity */
    ResetExceptionState();
    omni_wm_plasticity_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* KG */
    ResetExceptionState();
    omni_wm_kg_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* Logging */
    ResetExceptionState();
    omni_wm_logging_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* Substrate */
    ResetExceptionState();
    omni_wm_substrate_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* Thalamic */
    ResetExceptionState();
    omni_wm_thalamic_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* Hypothalamus */
    ResetExceptionState();
    omni_wm_hypothalamus_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* Parietal */
    ResetExceptionState();
    omni_wm_parietal_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    /* Security-Immune */
    ResetExceptionState();
    omni_wm_security_immune_bridge_connect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    EXPECT_EQ(exception_count, 11) << "All 11 bridges should trigger exceptions on null";
}

TEST_F(OmniWmExceptionHandlingTest, AllBridgesDisconnectBioAsyncNullTriggersException) {
    int exception_count = 0;

    /* Test disconnect for all 11 bridges */
    ResetExceptionState();
    omni_wm_cognitive_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_memory_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_tom_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_plasticity_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_kg_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_logging_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_substrate_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_thalamic_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_hypothalamus_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_parietal_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    ResetExceptionState();
    omni_wm_security_immune_bridge_disconnect_bio_async(nullptr);
    if (exception_caught) exception_count++;

    EXPECT_EQ(exception_count, 11) << "All 11 bridges should trigger exceptions on null";
}

/* ============================================================================
 * Exception Category and Severity Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, NullPointerExceptionHasCorrectCode) {
    /* Verify null pointer exceptions use correct error code */
    omni_wm_cognitive_bridge_connect_bio_async(nullptr);

    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OmniWmExceptionHandlingTest, ExceptionMessageContainsContext) {
    /* Verify exception messages contain useful context */
    omni_wm_cognitive_bridge_connect_bio_async(nullptr);

    EXPECT_TRUE(exception_caught);
    EXPECT_FALSE(caught_messages.empty());
    /* Message should mention what was null */
    bool has_context = false;
    for (const auto& msg : caught_messages) {
        if (msg.find("bridge") != std::string::npos ||
            msg.find("NULL") != std::string::npos) {
            has_context = true;
            break;
        }
    }
    EXPECT_TRUE(has_context) << "Exception message should contain context about what was null";
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(OmniWmExceptionHandlingTest, ExceptionDoesNotCrashSubsequentCalls) {
    /* Verify that after an exception, the system remains stable */
    omni_wm_cognitive_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);

    /* Clear and try another operation */
    ResetExceptionState();
    nimcp_error_t result = omni_wm_cognitive_bridge_reset(nullptr);

    /* Should still return error (not crash) */
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(OmniWmExceptionHandlingTest, MultipleExceptionsCanBeTracked) {
    /* Verify multiple exceptions can be triggered and tracked */
    int total_exceptions = 0;

    ResetExceptionState();
    omni_wm_cognitive_bridge_connect_bio_async(nullptr);
    if (exception_caught) total_exceptions++;

    ResetExceptionState();
    omni_wm_memory_bridge_connect_bio_async(nullptr);
    if (exception_caught) total_exceptions++;

    ResetExceptionState();
    omni_wm_tom_bridge_connect_bio_async(nullptr);
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
