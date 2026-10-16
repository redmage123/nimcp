/**
 * @file test_tom_exception_e2e.cpp
 * @brief End-to-end tests for Theory of Mind exception handling
 *
 * WHAT: Tests complete ToM workflows with exception handling and recovery
 * WHY:  Verify the full exception-to-immune pipeline works in realistic scenarios
 * HOW:  Test complete bridge lifecycles, multi-bridge interactions, and recovery
 *
 * E2E SCENARIOS:
 * - Full bridge lifecycle with exception recovery
 * - Multi-bridge workflow with interleaved exceptions
 * - Exception handling during active operations
 * - Complete immune integration pathway
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>
#include <memory>

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
 * RAII Wrappers for Safe Cleanup
 * ============================================================================ */

struct FepBridgeDeleter {
    void operator()(tom_fep_bridge_t* bridge) const {
        if (bridge) tom_fep_bridge_destroy(bridge);
    }
};

struct SnnBridgeDeleter {
    void operator()(tom_snn_bridge_t* bridge) const {
        if (bridge) tom_snn_destroy(bridge);
    }
};

struct PlasticityBridgeDeleter {
    void operator()(tom_plasticity_bridge_t* bridge) const {
        if (bridge) tom_plasticity_destroy(bridge);
    }
};

using FepBridgePtr = std::unique_ptr<tom_fep_bridge_t, FepBridgeDeleter>;
using SnnBridgePtr = std::unique_ptr<tom_snn_bridge_t, SnnBridgeDeleter>;
using PlasticityBridgePtr = std::unique_ptr<tom_plasticity_bridge_t, PlasticityBridgeDeleter>;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class TomExceptionE2ETest : public ::testing::Test {
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
        options.name = "test_tom_e2e_handler";
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
std::atomic<int> TomExceptionE2ETest::handler_call_count(0);
std::atomic<int> TomExceptionE2ETest::last_exception_code(0);
std::atomic<bool> TomExceptionE2ETest::exception_caught(false);
std::vector<std::string> TomExceptionE2ETest::caught_messages;
nimcp_handler_registration_t* TomExceptionE2ETest::test_handler_reg = nullptr;

/* ============================================================================
 * Full Lifecycle Tests
 * ============================================================================ */

TEST_F(TomExceptionE2ETest, FepBridgeFullLifecycleWithExceptionRecovery) {
    /* Phase 1: Create bridge */
    FepBridgePtr bridge(tom_fep_bridge_create(nullptr));
    ASSERT_NE(bridge.get(), nullptr);

    /* Phase 2: Verify initial state (not connected) */
    bool connected = tom_fep_bridge_is_bio_async_connected(bridge.get());
    EXPECT_FALSE(connected);
    EXPECT_FALSE(exception_caught); /* No exception for valid bridge */

    /* Phase 3: Simulate error condition with NULL */
    tom_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    /* Phase 4: Recover and continue with valid bridge */
    ResetExceptionState();
    int result = tom_fep_bridge_connect_bio_async(bridge.get());
    /* Result may be success or failure depending on bio-router availability */
    /* But no NULL pointer exception should occur */
    EXPECT_NE(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    /* Phase 5: Bridge destruction handled by RAII */
}

TEST_F(TomExceptionE2ETest, SnnBridgeFullLifecycleWithExceptionRecovery) {
    /* Phase 1: Create bridge with valid config */
    tom_snn_config_t config = tom_snn_config_default();
    SnnBridgePtr bridge(tom_snn_create(&config));
    ASSERT_NE(bridge.get(), nullptr);

    /* Phase 2: Verify initial state */
    bool connected = tom_snn_is_bio_async_connected(bridge.get());
    EXPECT_FALSE(connected);
    EXPECT_FALSE(exception_caught);

    /* Phase 3: Simulate error */
    tom_snn_bio_async_connect(nullptr);
    EXPECT_TRUE(exception_caught);

    /* Phase 4: Recover */
    ResetExceptionState();
    tom_snn_bio_async_connect(bridge.get());
    /* Should not throw NULL pointer exception for valid bridge */

    /* Bridge destruction handled by RAII */
}

TEST_F(TomExceptionE2ETest, PlasticityBridgeFullLifecycleWithExceptionRecovery) {
    /* Phase 1: Create bridge */
    tom_plasticity_config_t config = {};
    PlasticityBridgePtr bridge(tom_plasticity_create(&config));
    ASSERT_NE(bridge.get(), nullptr);

    /* Phase 2: Verify initial state */
    bool connected = tom_plasticity_is_bio_async_connected(bridge.get());
    EXPECT_FALSE(connected);
    EXPECT_FALSE(exception_caught);

    /* Phase 3: Simulate error */
    tom_plasticity_bio_async_connect(nullptr);
    EXPECT_TRUE(exception_caught);

    /* Phase 4: Recover */
    ResetExceptionState();
    tom_plasticity_bio_async_connect(bridge.get());

    /* Bridge destruction handled by RAII */
}

/* ============================================================================
 * Multi-Bridge Workflow Tests
 * ============================================================================ */

TEST_F(TomExceptionE2ETest, AllThreeBridgesWorkflowWithExceptions) {
    /* Create all three bridges with valid configs */
    FepBridgePtr fep(tom_fep_bridge_create(nullptr));
    tom_snn_config_t snn_config = tom_snn_config_default();
    SnnBridgePtr snn(tom_snn_create(&snn_config));
    tom_plasticity_config_t plasticity_config = {};
    PlasticityBridgePtr plasticity(tom_plasticity_create(&plasticity_config));

    ASSERT_NE(fep.get(), nullptr);
    ASSERT_NE(snn.get(), nullptr);
    ASSERT_NE(plasticity.get(), nullptr);

    /* Interleave valid operations with exceptions */
    bool fep_connected = tom_fep_bridge_is_bio_async_connected(fep.get());
    EXPECT_FALSE(fep_connected);

    tom_snn_bio_async_connect(nullptr); /* Exception */
    EXPECT_TRUE(exception_caught);
    ResetExceptionState();

    bool snn_connected = tom_snn_is_bio_async_connected(snn.get());
    EXPECT_FALSE(snn_connected);
    EXPECT_FALSE(exception_caught);

    tom_plasticity_bio_async_disconnect(nullptr); /* Exception */
    EXPECT_TRUE(exception_caught);
    ResetExceptionState();

    bool plasticity_connected = tom_plasticity_is_bio_async_connected(plasticity.get());
    EXPECT_FALSE(plasticity_connected);
    EXPECT_FALSE(exception_caught);
}

TEST_F(TomExceptionE2ETest, SequentialBridgeCreationWithExceptionsBetween) {
    /* Create FEP bridge */
    FepBridgePtr fep(tom_fep_bridge_create(nullptr));
    ASSERT_NE(fep.get(), nullptr);

    /* Exception */
    tom_snn_bio_async_connect(nullptr);
    EXPECT_TRUE(exception_caught);
    ResetExceptionState();

    /* Create SNN bridge - should succeed despite previous exception */
    tom_snn_config_t snn_config = tom_snn_config_default();
    SnnBridgePtr snn(tom_snn_create(&snn_config));
    ASSERT_NE(snn.get(), nullptr);

    /* Exception */
    tom_plasticity_bio_async_disconnect(nullptr);
    EXPECT_TRUE(exception_caught);
    ResetExceptionState();

    /* Create Plasticity bridge - should succeed */
    tom_plasticity_config_t plasticity_config = {};
    PlasticityBridgePtr plasticity(tom_plasticity_create(&plasticity_config));
    ASSERT_NE(plasticity.get(), nullptr);
}

/* ============================================================================
 * Exception Recovery Stress Tests
 * ============================================================================ */

TEST_F(TomExceptionE2ETest, RepeatedExceptionRecoveryCycles) {
    FepBridgePtr fep(tom_fep_bridge_create(nullptr));
    ASSERT_NE(fep.get(), nullptr);

    for (int cycle = 0; cycle < 10; cycle++) {
        /* Generate exception */
        tom_fep_bridge_connect_bio_async(nullptr);
        EXPECT_TRUE(exception_caught);

        /* Recover */
        ResetExceptionState();

        /* Verify valid operations still work */
        bool connected = tom_fep_bridge_is_bio_async_connected(fep.get());
        EXPECT_FALSE(connected);
        EXPECT_FALSE(exception_caught);
    }
}

TEST_F(TomExceptionE2ETest, MixedValidAndInvalidOperationsStress) {
    FepBridgePtr fep(tom_fep_bridge_create(nullptr));
    tom_snn_config_t snn_config = tom_snn_config_default();
    SnnBridgePtr snn(tom_snn_create(&snn_config));

    ASSERT_NE(fep.get(), nullptr);
    ASSERT_NE(snn.get(), nullptr);

    int valid_ops = 0;
    int exception_ops = 0;

    for (int i = 0; i < 50; i++) {
        ResetExceptionState();

        if (i % 3 == 0) {
            /* Valid operation */
            tom_fep_bridge_is_bio_async_connected(fep.get());
            if (!exception_caught) valid_ops++;
        } else if (i % 3 == 1) {
            /* Invalid operation */
            tom_snn_bio_async_connect(nullptr);
            if (exception_caught) exception_ops++;
        } else {
            /* Another valid operation */
            tom_snn_is_bio_async_connected(snn.get());
            if (!exception_caught) valid_ops++;
        }
    }

    /* Verify we got the expected mix */
    EXPECT_GT(valid_ops, 0);
    EXPECT_GT(exception_ops, 0);
}

/* ============================================================================
 * Complete Workflow Simulation
 * ============================================================================ */

TEST_F(TomExceptionE2ETest, SimulateRealWorldUsagePattern) {
    /*
     * Simulate a realistic usage pattern:
     * 1. Initialize system
     * 2. Create bridges
     * 3. Attempt operations (some may fail)
     * 4. Handle exceptions
     * 5. Continue with valid operations
     * 6. Clean up
     */

    /* 1. System already initialized in SetUp */

    /* 2. Create bridges with valid configs */
    FepBridgePtr fep(tom_fep_bridge_create(nullptr));
    tom_snn_config_t snn_config = tom_snn_config_default();
    SnnBridgePtr snn(tom_snn_create(&snn_config));
    tom_plasticity_config_t plasticity_config = {};
    PlasticityBridgePtr plasticity(tom_plasticity_create(&plasticity_config));

    ASSERT_NE(fep.get(), nullptr);
    ASSERT_NE(snn.get(), nullptr);
    ASSERT_NE(plasticity.get(), nullptr);

    /* 3. Attempt bio-async connections */
    int connect_attempts = 0;
    int connect_exceptions = 0;

    /* Try FEP - might succeed or fail depending on bio-router */
    ResetExceptionState();
    tom_fep_bridge_connect_bio_async(fep.get());
    connect_attempts++;
    if (exception_caught) connect_exceptions++;

    /* Try SNN with NULL - will fail */
    ResetExceptionState();
    tom_snn_bio_async_connect(nullptr);
    connect_attempts++;
    EXPECT_TRUE(exception_caught);
    connect_exceptions++;

    /* 4. Handle exception - recover */
    ResetExceptionState();

    /* 5. Continue with valid operations */
    ResetExceptionState();
    tom_snn_bio_async_connect(snn.get());
    connect_attempts++;

    /* Check connection status */
    bool fep_connected = tom_fep_bridge_is_bio_async_connected(fep.get());
    bool snn_connected = tom_snn_is_bio_async_connected(snn.get());
    bool plasticity_connected = tom_plasticity_is_bio_async_connected(plasticity.get());

    /* Status checks should not throw exceptions */
    EXPECT_FALSE(exception_caught);

    /* 6. Cleanup handled by RAII destructors */

    /* Verify we processed everything */
    EXPECT_EQ(connect_attempts, 3);
    EXPECT_GE(connect_exceptions, 1); /* At least the NULL one */
}

/* ============================================================================
 * Exception Message Verification
 * ============================================================================ */

TEST_F(TomExceptionE2ETest, ExceptionMessagesProvideUsefulContext) {
    tom_fep_bridge_connect_bio_async(nullptr);
    tom_snn_bio_async_disconnect(nullptr);
    /* tom_plasticity_is_bio_async_connected returns bool and does NOT throw
     * on NULL input (returns false), so use bio_async_connect instead */
    tom_plasticity_bio_async_connect(nullptr);

    EXPECT_EQ(caught_messages.size(), 3u);

    for (const auto& msg : caught_messages) {
        /* Messages should contain actionable information */
        EXPECT_NE(msg.find("bridge"), std::string::npos)
            << "Message should mention 'bridge': " << msg;
        EXPECT_NE(msg.find("NULL"), std::string::npos)
            << "Message should mention 'NULL': " << msg;
    }
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
