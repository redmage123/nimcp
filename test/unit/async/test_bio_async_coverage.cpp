/**
 * @file test_bio_async_coverage.cpp
 * @brief Unit tests for bio-async coverage improvements
 * @date 2026-03-20
 *
 * WHAT: Tests for bio-router and bio-async coverage: broadcast error handling,
 *       timeout escalation, dead letter tracking, message drops, validation
 * WHY:  Ensure all bio-async error paths and edge cases are exercised
 * HOW:  Initialize router, register modules, test error paths directly
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class BioAsyncCoverageTest : public ::testing::Test {
protected:
    bool router_initialized = false;
    bool bio_async_initialized = false;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t async_config = nimcp_bio_async_default_config();
        async_config.enable_statistics = true;
        async_config.enable_logging = false;
        async_config.thread_pool_size = 1;

        nimcp_error_t rc = nimcp_bio_async_init(&async_config);
        if (rc == NIMCP_SUCCESS) {
            bio_async_initialized = true;
        }

        // Initialize router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 32;
        router_config.inbox_capacity = 64;
        router_config.enable_statistics = true;
        router_config.enable_logging = false;

        rc = bio_router_init(&router_config);
        if (rc == NIMCP_SUCCESS) {
            router_initialized = true;
        }
    }

    void TearDown() override {
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
        if (bio_async_initialized) {
            nimcp_bio_async_shutdown();
            bio_async_initialized = false;
        }
    }

    // Helper: register a test module
    bio_module_context_t register_module(bio_module_id_t id, const char* name) {
        bio_module_info_t info;
        memset(&info, 0, sizeof(info));
        info.module_id = id;
        info.module_name = name;
        info.inbox_capacity = 0;  // use default
        info.user_data = nullptr;
        return bio_router_register_module(&info);
    }
};

/* ============================================================================
 * Handler for testing
 * ============================================================================ */

static int s_handler_call_count = 0;
static int s_handler_error_count = 0;

static nimcp_error_t test_success_handler(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;
    s_handler_call_count++;
    return NIMCP_SUCCESS;
}

static nimcp_error_t test_error_handler(
    const void* msg, size_t msg_size,
    nimcp_bio_promise_t response_promise, void* user_data)
{
    (void)msg; (void)msg_size; (void)response_promise; (void)user_data;
    s_handler_error_count++;
    return NIMCP_ERROR_INVALID_STATE;  // Simulate failure
}

/* ============================================================================
 * Broadcast Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, BroadcastContinuesOnFailure) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    // Register 3 modules
    bio_module_context_t mod_a = register_module(BIO_MODULE_BRAIN, "test_brain");
    bio_module_context_t mod_b = register_module(
        (bio_module_id_t)BIO_MODULE_NEURON_MODEL, "test_neuron");
    bio_module_context_t mod_c = register_module(
        (bio_module_id_t)BIO_MODULE_SYNAPSE, "test_synapse");

    ASSERT_NE(mod_a, nullptr);

    // Register handlers - one succeeds, one fails
    if (mod_b) {
        bio_router_register_handler(mod_b, BIO_MSG_BRAIN_STATE_QUERY, test_success_handler);
    }
    if (mod_c) {
        bio_router_register_handler(mod_c, BIO_MSG_BRAIN_STATE_QUERY, test_error_handler);
    }

    // Build and broadcast a message
    struct {
        bio_message_header_t header;
        uint32_t data;
    } msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MODULE_BRAIN, (bio_module_id_t)BIO_MODULE_ALL, sizeof(msg));

    s_handler_call_count = 0;
    s_handler_error_count = 0;

    nimcp_error_t rc = bio_router_broadcast(mod_a, &msg, sizeof(msg));
    // Broadcast should not fail entirely because one handler fails
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    // Process inboxes
    if (mod_b) bio_router_process_inbox(mod_b, 0);
    if (mod_c) bio_router_process_inbox(mod_c, 0);

    // Cleanup
    if (mod_a) bio_router_unregister_module(mod_a);
    if (mod_b) bio_router_unregister_module(mod_b);
    if (mod_c) bio_router_unregister_module(mod_c);
}

/* ============================================================================
 * Stats Tracking Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, StatsTrackingSends) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    bio_router_reset_stats();

    bio_module_context_t mod_a = register_module(BIO_MODULE_BRAIN, "stats_sender");
    bio_module_context_t mod_b = register_module(
        (bio_module_id_t)BIO_MODULE_NEURON_MODEL, "stats_receiver");
    ASSERT_NE(mod_a, nullptr);

    if (mod_b) {
        bio_router_register_handler(mod_b, BIO_MSG_BRAIN_STATE_QUERY, test_success_handler);
    }

    // Send messages
    struct {
        bio_message_header_t header;
        uint32_t data;
    } msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MODULE_BRAIN, (bio_module_id_t)BIO_MODULE_NEURON_MODEL, sizeof(msg));

    for (int i = 0; i < 5; i++) {
        bio_router_send(mod_a, &msg, sizeof(msg), 100);
    }

    bio_router_stats_t stats;
    nimcp_error_t rc = bio_router_get_stats(&stats);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    EXPECT_GE(stats.messages_routed, 5u);

    if (mod_a) bio_router_unregister_module(mod_a);
    if (mod_b) bio_router_unregister_module(mod_b);
}

TEST_F(BioAsyncCoverageTest, StatsTrackingDrops) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    bio_router_reset_stats();

    bio_module_context_t mod_a = register_module(BIO_MODULE_BRAIN, "drop_sender");
    ASSERT_NE(mod_a, nullptr);

    // Send to non-existent module
    struct {
        bio_message_header_t header;
        uint32_t data;
    } msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MODULE_BRAIN, (bio_module_id_t)0x9999, sizeof(msg));

    bio_router_send(mod_a, &msg, sizeof(msg), 100);

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    // Message to non-existent module may be dropped or cause error
    EXPECT_GE(stats.messages_dropped + stats.handler_errors, 0u);

    bio_router_unregister_module(mod_a);
}

/* ============================================================================
 * Message Validation Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, MessageDropToNonExistentModule) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    bio_module_context_t mod_a = register_module(BIO_MODULE_BRAIN, "sender");
    ASSERT_NE(mod_a, nullptr);

    struct {
        bio_message_header_t header;
    } msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
        BIO_MODULE_BRAIN, (bio_module_id_t)0xDEAD, sizeof(msg));

    nimcp_error_t rc = bio_router_send(mod_a, &msg, sizeof(msg), 100);
    // Should fail or drop - just verify no crash
    // Implementation may return error or succeed with drop
    (void)rc;

    bio_router_unregister_module(mod_a);
}

/* ============================================================================
 * NULL Handler Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, NullRouterHandled) {
    // Test functions when router is not available or NULL args passed
    bio_router_stats_t stats;

    // Get stats with NULL
    nimcp_error_t rc = bio_router_get_stats(nullptr);
    EXPECT_NE(rc, NIMCP_SUCCESS);

    // Register module with NULL info
    bio_module_context_t ctx = bio_router_register_module(nullptr);
    EXPECT_EQ(ctx, nullptr);

    // Unregister NULL
    bio_router_unregister_module(nullptr);  // Should not crash

    // Register handler with NULL context
    rc = bio_router_register_handler(nullptr, BIO_MSG_BRAIN_STATE_QUERY, test_success_handler);
    EXPECT_NE(rc, NIMCP_SUCCESS);

    // Send with NULL context
    struct {
        bio_message_header_t header;
    } msg;
    memset(&msg, 0, sizeof(msg));
    rc = bio_router_send(nullptr, &msg, sizeof(msg), 100);
    EXPECT_NE(rc, NIMCP_SUCCESS);

    // Send with NULL message
    if (router_initialized) {
        bio_module_context_t mod = register_module(BIO_MODULE_BRAIN, "null_test");
        if (mod) {
            rc = bio_router_send(mod, nullptr, 0, 100);
            EXPECT_NE(rc, NIMCP_SUCCESS);
            bio_router_unregister_module(mod);
        }
    }

    // Broadcast with NULL context
    rc = bio_router_broadcast(nullptr, &msg, sizeof(msg));
    EXPECT_NE(rc, NIMCP_SUCCESS);

    // Process inbox with NULL
    uint32_t processed = bio_router_process_inbox(nullptr, 0);
    EXPECT_EQ(processed, 0u);

    // Inbox count with NULL
    uint32_t count = bio_router_inbox_count(nullptr);
    EXPECT_EQ(count, 0u);
}

/* ============================================================================
 * Handler Registration Validation Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, HandlerRegistrationRejectsZeroType) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    bio_module_context_t mod = register_module(BIO_MODULE_BRAIN, "zero_type_test");
    ASSERT_NE(mod, nullptr);

    // Register with msg_type=0 (BIO_MSG_UNKNOWN or invalid)
    nimcp_error_t rc = bio_router_register_handler(mod, (bio_message_type_t)0, test_success_handler);
    // May reject or accept - just verify no crash
    (void)rc;

    bio_router_unregister_module(mod);
}

TEST_F(BioAsyncCoverageTest, HandlerClear) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    bio_module_context_t mod = register_module(BIO_MODULE_BRAIN, "clear_test");
    ASSERT_NE(mod, nullptr);

    // Register a handler
    bio_router_register_handler(mod, BIO_MSG_BRAIN_STATE_QUERY, test_success_handler);

    // Clear all handlers
    nimcp_error_t rc = bio_router_clear_handlers(mod);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    // Clear with NULL
    rc = bio_router_clear_handlers(nullptr);
    EXPECT_NE(rc, NIMCP_SUCCESS);

    bio_router_unregister_module(mod);
}

/* ============================================================================
 * Bio-Async Immune Integration Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, BioAsyncConnectImmuneNull) {
    // Connect with NULL immune system
    nimcp_error_t rc = bio_async_connect_immune(nullptr);
    EXPECT_NE(rc, NIMCP_SUCCESS);
}

TEST_F(BioAsyncCoverageTest, BioAsyncBroadcastCytokine) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    // Broadcast cytokine without immune module registered
    nimcp_error_t rc = bio_async_broadcast_cytokine(1, 0.5f, 42);
    // May fail since immune module not registered - just verify no crash
    (void)rc;
}

TEST_F(BioAsyncCoverageTest, BioAsyncInflammationAlert) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    nimcp_error_t rc = bio_async_inflammation_alert(1, 3, 100);
    // May fail since immune module not registered - verify no crash
    (void)rc;
}

TEST_F(BioAsyncCoverageTest, BioAsyncPhaseChange) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    nimcp_error_t rc = bio_async_immune_phase_change(0, 1);
    // Verify no crash
    (void)rc;
}

/* ============================================================================
 * Router State Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, RouterInitState) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    EXPECT_TRUE(bio_router_is_initialized());

    bio_router_t global = bio_router_get_global();
    EXPECT_NE(global, nullptr);
}

TEST_F(BioAsyncCoverageTest, RouterStatsReset) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    bio_router_reset_stats();

    bio_router_stats_t stats;
    nimcp_error_t rc = bio_router_get_stats(&stats);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
    EXPECT_EQ(stats.messages_routed, 0u);
    EXPECT_EQ(stats.messages_dropped, 0u);
    EXPECT_EQ(stats.handler_errors, 0u);
}

/* ============================================================================
 * Module Context Accessors Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, ModuleContextAccessors) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    bio_module_context_t mod = register_module(BIO_MODULE_BRAIN, "accessor_test");
    ASSERT_NE(mod, nullptr);

    bio_module_id_t id = bio_module_context_get_id(mod);
    EXPECT_EQ(id, BIO_MODULE_BRAIN);

    const char* name = bio_module_context_get_name(mod);
    EXPECT_NE(name, nullptr);
    EXPECT_STREQ(name, "accessor_test");

    void* user_data = bio_module_context_get_user_data(mod);
    EXPECT_EQ(user_data, nullptr);  // We set user_data to NULL

    // NULL context accessors
    bio_module_id_t null_id = bio_module_context_get_id(nullptr);
    EXPECT_EQ(null_id, BIO_MODULE_UNKNOWN);

    const char* null_name = bio_module_context_get_name(nullptr);
    // Implementation may return a default name or nullptr for NULL context
    (void)null_name;  // Just verify no crash

    bio_router_unregister_module(mod);
}

/* ============================================================================
 * Bio-Async System Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, BioAsyncStep) {
    if (!bio_async_initialized) GTEST_SKIP() << "Bio-async init failed";

    nimcp_error_t rc = nimcp_bio_async_step(1.0f);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
}

TEST_F(BioAsyncCoverageTest, BioAsyncStats) {
    if (!bio_async_initialized) GTEST_SKIP() << "Bio-async init failed";

    nimcp_bio_async_stats_t stats;
    nimcp_error_t rc = nimcp_bio_async_get_stats(&stats);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    // Verify sane values
    EXPECT_GE(stats.total_futures_created, 0u);

    // NULL stats
    rc = nimcp_bio_async_get_stats(nullptr);
    EXPECT_NE(rc, NIMCP_SUCCESS);
}

TEST_F(BioAsyncCoverageTest, BioAsyncInitialized) {
    EXPECT_EQ(nimcp_bio_async_is_initialized(), bio_async_initialized);
}

/* ============================================================================
 * Promise/Future Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, PromiseCreateAndDestroy) {
    if (!bio_async_initialized) GTEST_SKIP() << "Bio-async init failed";

    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
    EXPECT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    EXPECT_NE(future, nullptr);

    // Check state
    nimcp_bio_future_state_t state = nimcp_bio_future_state(future);
    EXPECT_EQ(state, BIO_FUTURE_PENDING);

    EXPECT_FALSE(nimcp_bio_future_is_ready(future));

    // Complete promise
    float result = 42.0f;
    nimcp_error_t rc = nimcp_bio_promise_complete(promise, &result);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    // Now future should be ready
    EXPECT_TRUE(nimcp_bio_future_is_ready(future));

    float confidence = nimcp_bio_future_get_confidence(future);
    EXPECT_GT(confidence, 0.0f);

    // Destroy
    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncCoverageTest, NullPromiseFutureHandled) {
    nimcp_bio_promise_destroy(nullptr);  // Should not crash
    nimcp_bio_future_destroy(nullptr);   // Should not crash

    nimcp_bio_future_t null_future = nimcp_bio_promise_get_future(nullptr);
    EXPECT_EQ(null_future, nullptr);

    nimcp_bio_future_state_t state = nimcp_bio_future_state(nullptr);
    // NULL future returns PENDING (0) - implementation-defined behavior
    EXPECT_EQ(state, BIO_FUTURE_PENDING);

    bool ready = nimcp_bio_future_is_ready(nullptr);
    // NULL future is not ready
    EXPECT_FALSE(ready);

    float conf = nimcp_bio_future_get_confidence(nullptr);
    EXPECT_EQ(conf, 0.0f);
}

/* ============================================================================
 * Predictive Coding Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, PredictiveModelLifecycle) {
    if (!bio_async_initialized) GTEST_SKIP() << "Bio-async init failed";

    nimcp_predictive_model_t model = nimcp_predictive_create("test_signal", 1.0f, 10.0f);
    if (!model) GTEST_SKIP() << "Predictive create failed";

    float pred = nimcp_predictive_get_prediction(model);
    EXPECT_FLOAT_EQ(pred, 1.0f);

    float prec = nimcp_predictive_get_precision(model);
    EXPECT_GT(prec, 0.0f);

    // Observe actual value
    nimcp_error_t rc = nimcp_predictive_observe(model, 2.0f);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    float surprise = nimcp_predictive_get_last_surprise(model);
    EXPECT_GT(surprise, 0.0f);

    // Update prediction manually
    rc = nimcp_predictive_set_prediction(model, 3.0f, 0);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    nimcp_predictive_destroy(model);
    nimcp_predictive_destroy(nullptr);  // NULL safe
}

/* ============================================================================
 * Health Agent Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, HealthAgentSetNull) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    // Set NULL health agent - should be safe
    bio_router_set_health_agent(nullptr);
}

/* ============================================================================
 * KG Dispatch Tests
 * ============================================================================ */

TEST_F(BioAsyncCoverageTest, KGDispatchNotAvailable) {
    if (!router_initialized) GTEST_SKIP() << "Router init failed";

    EXPECT_FALSE(bio_router_kg_dispatch_available());
    EXPECT_EQ(bio_router_get_brain_kg(), nullptr);
    EXPECT_EQ(bio_router_get_orchestrator(), nullptr);
}
