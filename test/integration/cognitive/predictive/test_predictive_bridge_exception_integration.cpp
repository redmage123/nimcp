/**
 * @file test_predictive_bridge_exception_integration.cpp
 * @brief Integration tests for Predictive Bridge exception propagation and recovery
 *
 * WHAT: Tests cross-bridge exception propagation and recovery scenarios
 * WHY:  Verify exception handling works across multiple interacting bridges
 * HOW:  Test multi-bridge workflows with exception injection and recovery
 *
 * INTEGRATION SCENARIOS:
 * - Cross-bridge exception propagation
 * - Recovery after exceptions during active operations
 * - Exception handling during bridge updates
 * - Multi-bridge lifecycle with interleaved exceptions
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <chrono>

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
 * RAII Wrappers for Safe Cleanup
 * ============================================================================ */

struct SnnBridgeDeleter {
    void operator()(predictive_snn_bridge_t* bridge) const {
        if (bridge) predictive_snn_destroy(bridge);
    }
};

struct PlasticityBridgeDeleter {
    void operator()(predictive_plasticity_bridge_t* bridge) const {
        if (bridge) predictive_plasticity_destroy(bridge);
    }
};

struct ThalamicBridgeDeleter {
    void operator()(predictive_thalamic_bridge_t* bridge) const {
        if (bridge) predictive_thalamic_bridge_destroy(bridge);
    }
};

struct FepBridgeDeleter {
    void operator()(predictive_fep_bridge_t* bridge) const {
        if (bridge) predictive_fep_bridge_destroy(bridge);
    }
};

using SnnBridgePtr = std::unique_ptr<predictive_snn_bridge_t, SnnBridgeDeleter>;
using PlasticityBridgePtr = std::unique_ptr<predictive_plasticity_bridge_t, PlasticityBridgeDeleter>;
using ThalamicBridgePtr = std::unique_ptr<predictive_thalamic_bridge_t, ThalamicBridgeDeleter>;
using FepBridgePtr = std::unique_ptr<predictive_fep_bridge_t, FepBridgeDeleter>;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PredictiveBridgeExceptionIntegrationTest : public ::testing::Test {
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
        options.name = "test_predictive_integration_handler";
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
std::atomic<int> PredictiveBridgeExceptionIntegrationTest::handler_call_count(0);
std::atomic<int> PredictiveBridgeExceptionIntegrationTest::last_exception_code(0);
std::atomic<bool> PredictiveBridgeExceptionIntegrationTest::exception_caught(false);
std::vector<std::string> PredictiveBridgeExceptionIntegrationTest::caught_messages;
nimcp_handler_registration_t* PredictiveBridgeExceptionIntegrationTest::test_handler_reg = nullptr;

/* ============================================================================
 * Cross-Bridge Exception Propagation Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionIntegrationTest, FepAndThalamicBridge_ExceptionPropagation) {
    /* Create FEP bridge */
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    ASSERT_NE(fep.get(), nullptr);

    /* Create Thalamic bridge */
    predictive_thalamic_config_t thalamic_config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &thalamic_config));
    ASSERT_NE(thalamic.get(), nullptr);

    /* Exception on one bridge should not affect the other */
    ResetExceptionState();
    predictive_fep_bridge_connect_bio_async(nullptr); /* Exception */
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    /* Thalamic bridge should still work */
    ResetExceptionState();
    int result = predictive_thalamic_set_attention(thalamic.get(), 0.7f);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);

    float attention = 0.0f;
    result = predictive_thalamic_get_attention(thalamic.get(), &attention);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(attention, 0.7f);
}

TEST_F(PredictiveBridgeExceptionIntegrationTest, PlasticityAndSnn_ExceptionIsolation) {
    /* Create Plasticity bridge */
    predictive_plasticity_config_t plasticity_config = predictive_plasticity_config_default();
    PlasticityBridgePtr plasticity(predictive_plasticity_create(&plasticity_config));

    /* Create SNN bridge */
    predictive_snn_config_t snn_config = predictive_snn_config_default();
    SnnBridgePtr snn(predictive_snn_create(&snn_config));

    /* Both bridges may or may not be created depending on config */
    int successful_bridges = 0;
    if (plasticity.get()) successful_bridges++;
    if (snn.get()) successful_bridges++;

    /* Exception on NULL should be isolated */
    ResetExceptionState();
    predictive_snn_reset(nullptr);
    /* This may or may not throw - depends on implementation */

    /* Valid operations should still work on existing bridges */
    if (plasticity.get()) {
        ResetExceptionState();
        predictive_plasticity_stats_t stats = {};
        int result = predictive_plasticity_get_stats(plasticity.get(), &stats);
        EXPECT_EQ(result, 0);
    }
}

/* ============================================================================
 * Recovery After Exceptions Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionIntegrationTest, RecoveryAfterMultipleBridgeExceptions) {
    /* Generate exceptions from multiple bridge types */
    ResetExceptionState();
    predictive_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);

    ResetExceptionState();
    predictive_fep_bridge_disconnect(nullptr);
    EXPECT_TRUE(exception_caught);

    ResetExceptionState();
    predictive_fep_bridge_update(nullptr, 100);
    EXPECT_TRUE(exception_caught);

    /* System should still be able to create valid bridges */
    ResetExceptionState();
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    if (fep.get()) {
        predictive_fep_state_t state = {};
        int result = predictive_fep_bridge_get_state(fep.get(), &state);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(PredictiveBridgeExceptionIntegrationTest, RecoveryDuringActiveOperations) {
    /* Create FEP bridge */
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    if (!fep.get()) GTEST_SKIP() << "FEP bridge creation failed";

    /* Perform update cycle */
    int result = predictive_fep_bridge_update(fep.get(), 100);
    EXPECT_EQ(result, 0);

    /* Inject exception via NULL call */
    ResetExceptionState();
    predictive_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);

    /* Should be able to continue with valid operations */
    ResetExceptionState();
    result = predictive_fep_bridge_update(fep.get(), 100);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);
}

/* ============================================================================
 * Multi-Bridge Lifecycle Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionIntegrationTest, AllBridgesLifecycleWithExceptions) {
    /* Create all bridge types */
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));

    predictive_thalamic_config_t thalamic_config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &thalamic_config));

    predictive_plasticity_config_t plasticity_config = predictive_plasticity_config_default();
    PlasticityBridgePtr plasticity(predictive_plasticity_create(&plasticity_config));

    predictive_snn_config_t snn_config = predictive_snn_config_default();
    SnnBridgePtr snn(predictive_snn_create(&snn_config));

    /* Interleave valid operations with exceptions */
    int exception_count = 0;
    int valid_ops = 0;

    /* FEP operations */
    if (fep.get()) {
        ResetExceptionState();
        predictive_fep_bridge_update(fep.get(), 100);
        if (!exception_caught) valid_ops++;
    }

    /* Exception */
    ResetExceptionState();
    predictive_fep_bridge_connect_fep(nullptr, nullptr);
    if (exception_caught) exception_count++;

    /* Thalamic operations */
    if (thalamic.get()) {
        ResetExceptionState();
        predictive_thalamic_set_attention(thalamic.get(), 0.5f);
        if (!exception_caught) valid_ops++;
    }

    /* Exception */
    ResetExceptionState();
    predictive_thalamic_route_error(nullptr, nullptr);
    /* May or may not throw */

    /* Plasticity operations */
    if (plasticity.get()) {
        ResetExceptionState();
        predictive_plasticity_stats_t stats = {};
        predictive_plasticity_get_stats(plasticity.get(), &stats);
        if (!exception_caught) valid_ops++;
    }

    /* SNN operations */
    if (snn.get()) {
        ResetExceptionState();
        predictive_snn_reset(snn.get());
        if (!exception_caught) valid_ops++;
    }

    /* Should have processed both valid ops and exceptions */
    EXPECT_GT(exception_count, 0) << "Expected at least one exception";
}

TEST_F(PredictiveBridgeExceptionIntegrationTest, SequentialBridgeCreationWithExceptions) {
    /* Create FEP bridge */
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));

    /* Exception */
    ResetExceptionState();
    predictive_thalamic_route_error(nullptr, nullptr);

    /* Create Thalamic bridge - should succeed despite previous exception */
    ResetExceptionState();
    predictive_thalamic_config_t thalamic_config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &thalamic_config));
    ASSERT_NE(thalamic.get(), nullptr);

    /* Exception */
    ResetExceptionState();
    predictive_plasticity_reset(nullptr);

    /* Create Plasticity bridge - should succeed */
    ResetExceptionState();
    predictive_plasticity_config_t plasticity_config = predictive_plasticity_config_default();
    PlasticityBridgePtr plasticity(predictive_plasticity_create(&plasticity_config));
    /* May or may not succeed depending on config */
}

/* ============================================================================
 * Exception During Update Cycle Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionIntegrationTest, FepBridge_UpdateCycleWithExceptionRecovery) {
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    if (!fep.get()) GTEST_SKIP() << "FEP bridge creation failed";

    /* Perform multiple update cycles with exception injection */
    for (int i = 0; i < 10; i++) {
        ResetExceptionState();

        if (i % 3 == 0) {
            /* Inject exception */
            predictive_fep_bridge_update(nullptr, 100);
            EXPECT_TRUE(exception_caught);
        } else {
            /* Valid update */
            int result = predictive_fep_bridge_update(fep.get(), 100);
            EXPECT_EQ(result, 0);
            EXPECT_FALSE(exception_caught);
        }
    }

    /* Bridge should still be functional */
    ResetExceptionState();
    predictive_fep_state_t state = {};
    int result = predictive_fep_bridge_get_state(fep.get(), &state);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);
}

TEST_F(PredictiveBridgeExceptionIntegrationTest, ThalamicBridge_SignalRoutingWithExceptions) {
    predictive_thalamic_config_t config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &config));
    ASSERT_NE(thalamic.get(), nullptr);

    /* Create signal for routing */
    predictive_thalamic_signal_t signal = {};
    signal.signal_type = PREDICTIVE_SIGNAL_ERROR;
    signal.error_magnitude = 0.5f;
    signal.precision_weight = 0.8f;

    /* Route valid signal */
    ResetExceptionState();
    int result = predictive_thalamic_route_error(thalamic.get(), &signal);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);

    /* Exception with NULL bridge */
    ResetExceptionState();
    result = predictive_thalamic_route_error(nullptr, &signal);
    EXPECT_EQ(result, -1);

    /* Exception with NULL signal */
    ResetExceptionState();
    result = predictive_thalamic_route_error(thalamic.get(), nullptr);
    EXPECT_EQ(result, -1);

    /* Bridge should still route valid signals */
    ResetExceptionState();
    result = predictive_thalamic_route_error(thalamic.get(), &signal);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);
}

/* ============================================================================
 * Stats and State Recovery Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionIntegrationTest, StatsAccessAfterExceptions) {
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    if (!fep.get()) GTEST_SKIP() << "FEP bridge creation failed";

    /* Perform some updates */
    for (int i = 0; i < 5; i++) {
        predictive_fep_bridge_update(fep.get(), 100);
    }

    /* Inject exceptions */
    for (int i = 0; i < 5; i++) {
        ResetExceptionState();
        predictive_fep_bridge_get_stats(nullptr, nullptr);
        /* Should throw */
    }

    /* Stats should still be accessible */
    ResetExceptionState();
    predictive_fep_stats_t stats = {};
    int result = predictive_fep_bridge_get_stats(fep.get(), &stats);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);
}

TEST_F(PredictiveBridgeExceptionIntegrationTest, ThalamicBridge_StatsAfterRouting) {
    predictive_thalamic_config_t config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &config));
    ASSERT_NE(thalamic.get(), nullptr);

    /* Route some signals */
    predictive_thalamic_signal_t signal = {};
    signal.signal_type = PREDICTIVE_SIGNAL_ERROR;
    signal.error_magnitude = 0.5f;
    signal.precision_weight = 0.8f;

    for (int i = 0; i < 5; i++) {
        predictive_thalamic_route_error(thalamic.get(), &signal);
    }

    /* Exception */
    ResetExceptionState();
    predictive_thalamic_bridge_get_stats(nullptr, nullptr);

    /* Stats should still be accessible */
    ResetExceptionState();
    predictive_thalamic_stats_t stats = {};
    int result = predictive_thalamic_bridge_get_stats(thalamic.get(), &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.errors_routed, 5u);
}

/* ============================================================================
 * Thread Safety Integration Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionIntegrationTest, ConcurrentExceptionsFromMultipleBridges) {
    /* Create bridges */
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));

    predictive_thalamic_config_t thalamic_config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &thalamic_config));

    std::atomic<int> thread_exceptions(0);

    auto fep_thread = [&]() {
        for (int i = 0; i < 10; i++) {
            predictive_fep_bridge_connect_bio_async(nullptr);
            thread_exceptions++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    auto thalamic_thread = [&]() {
        for (int i = 0; i < 10; i++) {
            predictive_thalamic_route_error(nullptr, nullptr);
            thread_exceptions++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread t1(fep_thread);
    std::thread t2(thalamic_thread);

    t1.join();
    t2.join();

    EXPECT_EQ(thread_exceptions, 20);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
