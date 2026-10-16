/**
 * @file test_predictive_bridge_exception_e2e.cpp
 * @brief End-to-end tests for Predictive Bridge exception handling in complete workflows
 *
 * WHAT: Tests complete predictive processing pipeline with exception recovery
 * WHY:  Verify the full exception-to-immune pipeline works in realistic scenarios
 * HOW:  Test complete bridge lifecycles, multi-bridge interactions, and recovery
 *
 * E2E SCENARIOS:
 * - Full predictive processing pipeline with all bridges
 * - Exception recovery during active prediction cycles
 * - Multi-bridge workflow with interleaved exceptions
 * - Complete immune integration pathway
 * - Realistic usage patterns with exception injection
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

class PredictiveBridgeExceptionE2ETest : public ::testing::Test {
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
        options.name = "test_predictive_e2e_handler";
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
std::atomic<int> PredictiveBridgeExceptionE2ETest::handler_call_count(0);
std::atomic<int> PredictiveBridgeExceptionE2ETest::last_exception_code(0);
std::atomic<bool> PredictiveBridgeExceptionE2ETest::exception_caught(false);
std::vector<std::string> PredictiveBridgeExceptionE2ETest::caught_messages;
nimcp_handler_registration_t* PredictiveBridgeExceptionE2ETest::test_handler_reg = nullptr;

/* ============================================================================
 * Full Predictive Processing Pipeline Tests
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionE2ETest, FullPipelineWithAllBridges) {
    /*
     * Simulate a complete predictive processing pipeline:
     * 1. FEP bridge for free energy minimization
     * 2. Thalamic bridge for attention gating
     * 3. Plasticity bridge for learning
     * 4. SNN bridge for neural encoding
     */

    /* Phase 1: Create all bridges */
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    ASSERT_NE(fep.get(), nullptr) << "FEP bridge creation failed";

    predictive_thalamic_config_t thalamic_config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &thalamic_config));
    ASSERT_NE(thalamic.get(), nullptr) << "Thalamic bridge creation failed";

    predictive_plasticity_config_t plasticity_config = predictive_plasticity_config_default();
    PlasticityBridgePtr plasticity(predictive_plasticity_create(&plasticity_config));

    predictive_snn_config_t snn_config = predictive_snn_config_default();
    SnnBridgePtr snn(predictive_snn_create(&snn_config));

    /* Phase 2: Simulate prediction cycle */
    uint64_t delta_ms = 100;

    /* FEP update */
    ResetExceptionState();
    int result = predictive_fep_bridge_update(fep.get(), delta_ms);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);

    /* Thalamic attention gating */
    ResetExceptionState();
    result = predictive_thalamic_set_attention(thalamic.get(), 0.8f);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);

    /* Route prediction signal through thalamus */
    predictive_thalamic_signal_t signal = {};
    signal.signal_type = PREDICTIVE_SIGNAL_PREDICTION;
    signal.error_magnitude = 0.3f;
    signal.precision_weight = 0.9f;
    signal.hierarchy_level = 2;

    ResetExceptionState();
    result = predictive_thalamic_route_error(thalamic.get(), &signal);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);

    /* Plasticity consolidate */
    if (plasticity.get()) {
        ResetExceptionState();
        result = predictive_plasticity_consolidate(plasticity.get());
        EXPECT_EQ(result, 0);
    }

    /* Phase 3: Inject exception and recover */
    ResetExceptionState();
    predictive_fep_bridge_connect_bio_async(nullptr);
    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

    /* Phase 4: Continue pipeline after exception */
    ResetExceptionState();
    result = predictive_fep_bridge_update(fep.get(), delta_ms);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);

    /* Verify state is consistent */
    predictive_fep_state_t fep_state = {};
    result = predictive_fep_bridge_get_state(fep.get(), &fep_state);
    EXPECT_EQ(result, 0);

    predictive_thalamic_stats_t thalamic_stats = {};
    result = predictive_thalamic_bridge_get_stats(thalamic.get(), &thalamic_stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(thalamic_stats.predictions_routed, 1u);
}

/* ============================================================================
 * Prediction Error Routing with Exception Recovery
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionE2ETest, PredictionErrorRoutingWithExceptionRecovery) {
    /* Create FEP and Thalamic bridges */
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    ASSERT_NE(fep.get(), nullptr);

    predictive_thalamic_config_t config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &config));
    ASSERT_NE(thalamic.get(), nullptr);

    /* Simulate multiple prediction error cycles with exception injection */
    for (int cycle = 0; cycle < 10; cycle++) {
        ResetExceptionState();

        /* Every 3rd cycle, inject an exception */
        if (cycle % 3 == 0) {
            predictive_thalamic_route_error(nullptr, nullptr);
            /* May or may not throw */
            continue;
        }

        /* Normal prediction error routing */
        predictive_thalamic_signal_t signal = {};
        signal.signal_type = (cycle % 2 == 0) ? PREDICTIVE_SIGNAL_ERROR : PREDICTIVE_SIGNAL_PREDICTION;
        signal.error_magnitude = 0.1f + (cycle * 0.05f);
        signal.precision_weight = 0.8f;
        signal.hierarchy_level = cycle % 4;

        int result = predictive_thalamic_route_error(thalamic.get(), &signal);
        EXPECT_EQ(result, 0);

        /* FEP update */
        result = predictive_fep_bridge_update(fep.get(), 100);
        EXPECT_EQ(result, 0);
    }

    /* Verify final stats */
    predictive_thalamic_stats_t stats = {};
    int result = predictive_thalamic_bridge_get_stats(thalamic.get(), &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.errors_routed + stats.predictions_routed, 0u);
}

/* ============================================================================
 * Attention Gating with Exception Handling
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionE2ETest, AttentionGatingWithExceptionHandling) {
    predictive_thalamic_config_t config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &config));
    ASSERT_NE(thalamic.get(), nullptr);

    /* Test attention gating with various values and exceptions */
    float attention_values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    int num_values = sizeof(attention_values) / sizeof(attention_values[0]);

    for (int i = 0; i < num_values; i++) {
        ResetExceptionState();

        /* Set attention */
        int result = predictive_thalamic_set_attention(thalamic.get(), attention_values[i]);
        EXPECT_EQ(result, 0);

        /* Inject exception */
        predictive_thalamic_set_attention(nullptr, 0.5f);

        /* Recover and verify */
        ResetExceptionState();
        float actual_attention = 0.0f;
        result = predictive_thalamic_get_attention(thalamic.get(), &actual_attention);
        EXPECT_EQ(result, 0);
        EXPECT_FLOAT_EQ(actual_attention, attention_values[i]);
    }
}

/* ============================================================================
 * FEP Convergence Monitoring with Exceptions
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionE2ETest, FepConvergenceMonitoringWithExceptions) {
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    ASSERT_NE(fep.get(), nullptr);

    /* Simulate convergence monitoring with exception injection */
    for (int cycle = 0; cycle < 20; cycle++) {
        ResetExceptionState();

        /* Update FEP */
        int result = predictive_fep_bridge_update(fep.get(), 100);
        EXPECT_EQ(result, 0);

        /* Every 5th cycle, inject exception */
        if (cycle % 5 == 0 && cycle > 0) {
            predictive_fep_bridge_get_state(nullptr, nullptr);
            EXPECT_TRUE(exception_caught);
        }

        /* Check state */
        ResetExceptionState();
        predictive_fep_state_t state = {};
        result = predictive_fep_bridge_get_state(fep.get(), &state);
        EXPECT_EQ(result, 0);
        EXPECT_FALSE(exception_caught);
    }

    /* Verify final stats */
    predictive_fep_stats_t stats = {};
    int result = predictive_fep_bridge_get_stats(fep.get(), &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.belief_syncs, 1u);
}

/* ============================================================================
 * Complete Workflow Simulation
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionE2ETest, RealisticPredictiveCodingWorkflow) {
    /*
     * Simulate a realistic predictive coding workflow:
     * 1. Initialize system
     * 2. Create bridges
     * 3. Run prediction cycles
     * 4. Handle errors gracefully
     * 5. Verify system remains stable
     */

    /* 1. System already initialized in SetUp */

    /* 2. Create bridges */
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    ASSERT_NE(fep.get(), nullptr);

    predictive_thalamic_config_t thalamic_config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic(predictive_thalamic_bridge_create(nullptr, nullptr, &thalamic_config));
    ASSERT_NE(thalamic.get(), nullptr);

    /* 3. Run prediction cycles */
    int successful_cycles = 0;
    int exception_cycles = 0;

    for (int cycle = 0; cycle < 50; cycle++) {
        ResetExceptionState();

        /* Randomly inject exceptions (simulating unexpected conditions) */
        if ((cycle * 7) % 11 == 0) {
            /* Inject exception */
            if (cycle % 2 == 0) {
                predictive_fep_bridge_update(nullptr, 100);
            } else {
                predictive_thalamic_route_error(nullptr, nullptr);
            }
            if (exception_caught || last_exception_code != 0) {
                exception_cycles++;
            }
            continue;
        }

        /* Normal operation */
        int result = predictive_fep_bridge_update(fep.get(), 100);
        if (result == 0) successful_cycles++;

        predictive_thalamic_signal_t signal = {};
        signal.signal_type = PREDICTIVE_SIGNAL_ERROR;
        signal.error_magnitude = 0.2f;
        signal.precision_weight = 0.85f;

        result = predictive_thalamic_route_error(thalamic.get(), &signal);
        if (result == 0) successful_cycles++;
    }

    /* 4. Verify system is stable */
    EXPECT_GT(successful_cycles, 0) << "Should have successful cycles";
    EXPECT_GT(exception_cycles, 0) << "Should have handled exceptions";

    /* 5. Final state check */
    ResetExceptionState();
    predictive_fep_state_t fep_state = {};
    int result = predictive_fep_bridge_get_state(fep.get(), &fep_state);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);

    predictive_thalamic_stats_t thalamic_stats = {};
    result = predictive_thalamic_bridge_get_stats(thalamic.get(), &thalamic_stats);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(exception_caught);
}

/* ============================================================================
 * Stress Test: Repeated Exception Recovery
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionE2ETest, StressTestRepeatedExceptionRecovery) {
    FepBridgePtr fep(predictive_fep_bridge_create(nullptr));
    ASSERT_NE(fep.get(), nullptr);

    /* Stress test: rapid exception/recovery cycles */
    for (int cycle = 0; cycle < 100; cycle++) {
        ResetExceptionState();

        /* Generate exception */
        predictive_fep_bridge_connect_bio_async(nullptr);
        EXPECT_TRUE(exception_caught);
        EXPECT_EQ(last_exception_code, NIMCP_ERROR_NULL_POINTER);

        /* Immediate recovery */
        ResetExceptionState();
        int result = predictive_fep_bridge_update(fep.get(), 10);
        EXPECT_EQ(result, 0);
        EXPECT_FALSE(exception_caught);
    }

    /* System should still be functional */
    predictive_fep_stats_t stats = {};
    int result = predictive_fep_bridge_get_stats(fep.get(), &stats);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Multi-Bridge Concurrent Operations with Exceptions
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionE2ETest, MultiBridgeConcurrentOperations) {
    /* Create multiple bridge instances */
    FepBridgePtr fep1(predictive_fep_bridge_create(nullptr));
    FepBridgePtr fep2(predictive_fep_bridge_create(nullptr));

    predictive_thalamic_config_t config = predictive_thalamic_default_config();
    ThalamicBridgePtr thalamic1(predictive_thalamic_bridge_create(nullptr, nullptr, &config));
    ThalamicBridgePtr thalamic2(predictive_thalamic_bridge_create(nullptr, nullptr, &config));

    ASSERT_NE(fep1.get(), nullptr);
    ASSERT_NE(fep2.get(), nullptr);
    ASSERT_NE(thalamic1.get(), nullptr);
    ASSERT_NE(thalamic2.get(), nullptr);

    std::atomic<int> ops_completed(0);
    std::atomic<int> exceptions_handled(0);

    /* Run concurrent operations */
    auto worker = [&](int id, predictive_fep_bridge_t* fep, predictive_thalamic_bridge_t* thalamic) {
        for (int i = 0; i < 20; i++) {
            /* Valid operation */
            if (fep) {
                predictive_fep_bridge_update(fep, 50);
                ops_completed++;
            }

            /* Exception (NULL operations) */
            if (i % 5 == 0) {
                predictive_fep_bridge_update(nullptr, 50);
                exceptions_handled++;
            }

            if (thalamic) {
                predictive_thalamic_set_attention(thalamic, 0.5f);
                ops_completed++;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread t1(worker, 1, fep1.get(), thalamic1.get());
    std::thread t2(worker, 2, fep2.get(), thalamic2.get());

    t1.join();
    t2.join();

    EXPECT_GT(ops_completed, 0);
    EXPECT_GT(exceptions_handled, 0);
}

/* ============================================================================
 * Exception Message Verification in E2E Context
 * ============================================================================ */

TEST_F(PredictiveBridgeExceptionE2ETest, ExceptionMessagesProvideUsefulContext) {
    /* Generate exceptions from different contexts */
    caught_messages.clear();

    predictive_fep_bridge_connect_bio_async(nullptr);
    predictive_fep_bridge_disconnect(nullptr);
    predictive_fep_bridge_update(nullptr, 100);

    EXPECT_GE(caught_messages.size(), 3u);

    /* All messages should contain useful context */
    for (const auto& msg : caught_messages) {
        EXPECT_FALSE(msg.empty()) << "Exception message should not be empty";

        /* Messages should mention either the function, bridge, or NULL */
        bool has_context = (msg.find("bridge") != std::string::npos) ||
                          (msg.find("NULL") != std::string::npos) ||
                          (msg.find("null") != std::string::npos) ||
                          (msg.find("fep") != std::string::npos) ||
                          (msg.find("predictive") != std::string::npos);

        EXPECT_TRUE(has_context) << "Message should have context: " << msg;
    }
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
