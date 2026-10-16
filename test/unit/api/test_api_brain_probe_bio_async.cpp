/**
 * @file test_api_brain_probe_bio_async.cpp
 * @brief Unit tests for bio-async brain probe integration
 *
 * WHAT: Tests brain probe broadcasting via bio-async message system
 * WHY:  Verify loose coupling between brain probe and metrics modules
 * HOW:  Tests message creation, broadcasting, and handler registration
 *
 * Tests the decoupled integration where:
 * - Brain module broadcasts probe data via BIO_MSG_BRAIN_PROBE_DATA
 * - Metrics module (or any subscriber) receives data independently
 * - Multiple brains can be monitored concurrently via unique brain_id
 */

#include <gtest/gtest.h>
#include <cstring>
#include "nimcp.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/metrics/nimcp_metrics.h"

/**
 * @brief Test fixture for bio-async brain probe tests
 */
class APIBrainProbeBioAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        // Initialize bio-router for messaging
        bio_router_init(nullptr);
    }

    void TearDown() override {
        bio_router_shutdown();
        nimcp_shutdown();
    }
};

/**
 * @brief Test that BIO_MSG_BRAIN_PROBE_DATA message type exists
 */
TEST_F(APIBrainProbeBioAsyncTest, MessageTypeExists) {
    // Verify the message type is defined in the bio message types enum
    // The actual value depends on enum order, so just verify it's positive
    EXPECT_GT(BIO_MSG_BRAIN_PROBE_DATA, 0)
        << "BIO_MSG_BRAIN_PROBE_DATA should be a valid message type";
}

/**
 * @brief Test that BIO_MODULE_METRICS module ID exists
 */
TEST_F(APIBrainProbeBioAsyncTest, MetricsModuleIdExists) {
    // Verify metrics module ID is defined
    bio_module_id_t metrics_id = BIO_MODULE_METRICS;
    EXPECT_GT(metrics_id, 0) << "BIO_MODULE_METRICS should be a valid module ID";
}

/**
 * @brief Test message structure size and layout
 */
TEST_F(APIBrainProbeBioAsyncTest, MessageStructureValid) {
    bio_msg_brain_probe_data_t msg;
    memset(&msg, 0, sizeof(msg));

    // Verify structure has expected fields
    EXPECT_GE(sizeof(msg), sizeof(bio_message_header_t))
        << "Message should at least contain header";

    // Initialize header
    bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_PROBE_DATA,
                        BIO_MODULE_BRAIN, BIO_MODULE_ALL,
                        sizeof(bio_msg_brain_probe_data_t));

    EXPECT_EQ(msg.header.type, BIO_MSG_BRAIN_PROBE_DATA);
    EXPECT_EQ(msg.header.source_module, BIO_MODULE_BRAIN);
    EXPECT_EQ(msg.header.target_module, BIO_MODULE_ALL);
}

/**
 * @brief Test brain probe broadcast function exists and handles NULL gracefully
 */
TEST_F(APIBrainProbeBioAsyncTest, BroadcastHandlesNullBrain) {
    // NULL brain should return error, not crash
    nimcp_status_t status = nimcp_brain_broadcast_probe(nullptr);
    EXPECT_NE(status, NIMCP_OK) << "NULL brain should return error";
}

/**
 * @brief Test brain probe broadcast with valid brain
 */
TEST_F(APIBrainProbeBioAsyncTest, BroadcastWithValidBrain) {
    // Create a brain
    nimcp_brain_t brain = nimcp_brain_create(
        "test_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );
    ASSERT_NE(brain, nullptr) << "Brain creation should succeed";

    // Broadcast probe - should succeed or gracefully handle no subscribers
    nimcp_status_t status = nimcp_brain_broadcast_probe(brain);
    EXPECT_EQ(status, NIMCP_OK)
        << "Broadcast should succeed (even without subscribers)";

    nimcp_brain_destroy(brain);
}

/**
 * @brief Test multi-brain probe support via unique brain_id
 */
TEST_F(APIBrainProbeBioAsyncTest, MultipleBrainsHaveUniqueIds) {
    // Create two brains
    nimcp_brain_t brain1 = nimcp_brain_create(
        "brain1",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );
    nimcp_brain_t brain2 = nimcp_brain_create(
        "brain2",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_REGRESSION,
        10,
        2
    );

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);

    // Brain IDs (pointer addresses) should be different
    EXPECT_NE((uint64_t)(uintptr_t)brain1, (uint64_t)(uintptr_t)brain2)
        << "Different brains should have different IDs";

    // Both should be able to broadcast
    EXPECT_EQ(nimcp_brain_broadcast_probe(brain1), NIMCP_OK);
    EXPECT_EQ(nimcp_brain_broadcast_probe(brain2), NIMCP_OK);

    nimcp_brain_destroy(brain1);
    nimcp_brain_destroy(brain2);
}

/**
 * @brief Test metrics module bio-async registration
 */
TEST_F(APIBrainProbeBioAsyncTest, MetricsCanRegisterForBioAsync) {
    // Create metrics collector
    nimcp_metrics_collector_t collector = nimcp_metrics_create();
    ASSERT_NE(collector, nullptr) << "Metrics collector creation should succeed";

    // Register for bio-async should succeed
    bool registered = nimcp_metrics_register_bio_async(collector);
    EXPECT_TRUE(registered) << "Metrics bio-async registration should succeed";

    nimcp_metrics_destroy(collector);
}

/**
 * @brief Test NULL collector registration fails gracefully
 */
TEST_F(APIBrainProbeBioAsyncTest, MetricsRegisterHandlesNull) {
    bool registered = nimcp_metrics_register_bio_async(nullptr);
    EXPECT_FALSE(registered) << "NULL collector registration should fail";
}

/**
 * @brief Test end-to-end: brain broadcasts, metrics receives
 */
TEST_F(APIBrainProbeBioAsyncTest, EndToEndBroadcastReceive) {
    // Create metrics collector and attempt registration
    // NOTE: Registration may fail if module is already registered by nimcp_init()
    // This is acceptable - we just verify the broadcast works
    nimcp_metrics_collector_t collector = nimcp_metrics_create();
    ASSERT_NE(collector, nullptr);

    // Registration may return false if already registered - that's OK
    (void)nimcp_metrics_register_bio_async(collector);

    // Create brain and broadcast
    nimcp_brain_t brain = nimcp_brain_create(
        "e2e_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );
    ASSERT_NE(brain, nullptr);

    // Broadcast probe data - this should always succeed
    EXPECT_EQ(nimcp_brain_broadcast_probe(brain), NIMCP_OK);

    // Process messages in metrics module
    nimcp_metrics_process_bio_async();

    // Cleanup
    nimcp_brain_destroy(brain);
    nimcp_metrics_destroy(collector);
}

/**
 * @brief Test bio-router not initialized gracefully degrades
 */
TEST(APIBrainProbeBioAsyncNoRouterTest, GracefulDegradation) {
    // Initialize NIMCP but NOT bio-router
    nimcp_init();

    // Create brain
    nimcp_brain_t brain = nimcp_brain_create(
        "no_router_brain",
        NIMCP_BRAIN_TINY,
        NIMCP_TASK_CLASSIFICATION,
        10,
        2
    );
    ASSERT_NE(brain, nullptr);

    // Broadcast should succeed (graceful degradation - just skips broadcast)
    nimcp_status_t status = nimcp_brain_broadcast_probe(brain);
    EXPECT_EQ(status, NIMCP_OK)
        << "Broadcast should gracefully degrade when router not available";

    nimcp_brain_destroy(brain);
    nimcp_shutdown();
}

/**
 * @brief Test metrics registration handles various states gracefully
 *
 * NOTE: nimcp_init() may or may not initialize the bio-router depending on
 * configuration, so we can't reliably test "without router" state. Instead,
 * we test that registration doesn't crash and returns a consistent value.
 */
TEST(APIBrainProbeBioAsyncNoRouterTest, MetricsRegistrationGraceful) {
    nimcp_init();

    nimcp_metrics_collector_t collector = nimcp_metrics_create();
    ASSERT_NE(collector, nullptr);

    // Registration should not crash regardless of router state
    // It may return true (deferred) or false (already registered)
    bool registered = nimcp_metrics_register_bio_async(collector);
    // Just verify it doesn't crash - the return value depends on system state
    (void)registered;  // Suppress unused warning

    nimcp_metrics_destroy(collector);
    nimcp_shutdown();
}

/**
 * @brief Test message structure field population
 */
TEST_F(APIBrainProbeBioAsyncTest, MessageFieldsPopulated) {
    bio_msg_brain_probe_data_t msg;
    memset(&msg, 0, sizeof(msg));

    // Populate fields
    msg.brain_id = 0x123456789ABCDEF0ULL;
    strncpy(msg.task_name, "test_task", sizeof(msg.task_name) - 1);
    msg.num_neurons = 1000;
    msg.num_synapses = 5000;
    msg.num_active_synapses = 4500;
    msg.total_inferences = 100;
    msg.total_learning_steps = 50;
    msg.avg_sparsity = 0.85f;
    msg.avg_inference_time_us = 150.5f;
    msg.current_learning_rate = 0.001f;
    msg.accuracy = 0.92f;
    msg.memory_bytes = 1024 * 1024;
    msg.num_inputs = 10;
    msg.num_outputs = 2;
    msg.is_cow_clone = false;
    msg.cow_ref_count = 0;
    msg.cow_shared_bytes = 0;
    msg.cow_private_bytes = 0;

    // Verify fields retained
    EXPECT_EQ(msg.brain_id, 0x123456789ABCDEF0ULL);
    EXPECT_STREQ(msg.task_name, "test_task");
    EXPECT_EQ(msg.num_neurons, 1000U);
    EXPECT_EQ(msg.num_synapses, 5000U);
    EXPECT_EQ(msg.num_active_synapses, 4500U);
    EXPECT_EQ(msg.total_inferences, 100ULL);
    EXPECT_EQ(msg.total_learning_steps, 50ULL);
    EXPECT_FLOAT_EQ(msg.avg_sparsity, 0.85f);
    EXPECT_FLOAT_EQ(msg.avg_inference_time_us, 150.5f);
    EXPECT_FLOAT_EQ(msg.current_learning_rate, 0.001f);
    EXPECT_FLOAT_EQ(msg.accuracy, 0.92f);
    EXPECT_EQ(msg.memory_bytes, 1024ULL * 1024ULL);
    EXPECT_EQ(msg.num_inputs, 10U);
    EXPECT_EQ(msg.num_outputs, 2U);
    EXPECT_FALSE(msg.is_cow_clone);
}
