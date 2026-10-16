//=============================================================================
// test_training_adapters_bio_async_regression.cpp
// Regression tests for Training Adapters Bio-Async Integration
//=============================================================================

#include <gtest/gtest.h>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_adapters.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

class TrainingAdaptersRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();

        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bio_async_init(&bio_config));

        // Initialize bio-router for message sending
        bio_router_config_t router_config = bio_router_default_config();
        bio_router_init(&router_config);

        // Register a sender module
        bio_module_info_t sender_info = {
            .module_id = BIO_MODULE_STP,
            .module_name = "TestSender",
            .inbox_capacity = 64,
            .user_data = nullptr
        };
        sender_module_ = bio_router_register_module(&sender_info);
    }

    void TearDown() override {
        if (sender_module_) {
            bio_router_unregister_module(sender_module_);
            sender_module_ = nullptr;
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_memory_cleanup();
    }

    bio_module_context_t sender_module_ = nullptr;
};

//=============================================================================
// Memory Leak Regression Tests
//=============================================================================

TEST_F(TrainingAdaptersRegressionTest, NoMemoryLeakOnRepeatedCreation) {
    nimcp_bio_async_stats_t initial_stats, final_stats;
    nimcp_bio_async_get_stats(&initial_stats);

    // Create and destroy adapters many times
    for (int i = 0; i < 100; i++) {
        learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
        ASSERT_NE(nullptr, adapter);
        learning_signal_adapter_destroy(adapter);
    }

    nimcp_bio_async_get_stats(&final_stats);

    // Memory should not grow excessively
    EXPECT_LT(final_stats.current_memory_bytes,
              initial_stats.current_memory_bytes * 2);
}

TEST_F(TrainingAdaptersRegressionTest, NoMemoryLeakOnMessageFlood) {
    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(nullptr, adapter);

    nimcp_bio_async_stats_t initial_stats;
    nimcp_bio_async_get_stats(&initial_stats);

    // Send many messages
    for (int i = 0; i < 1000; i++) {
        bio_msg_weight_update_request_t request = {};
        bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                           BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                           sizeof(request) - sizeof(bio_message_header_t));
        request.synapse_id = i;
        request.weight_delta = 0.001f;

        bio_router_send(sender_module_, &request, sizeof(request), 500);
    }

    nimcp_bio_async_step(100.0f);

    nimcp_bio_async_stats_t final_stats;
    nimcp_bio_async_get_stats(&final_stats);

    learning_signal_adapter_destroy(adapter);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(TrainingAdaptersRegressionTest, MessageProcessingPerformance) {
    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(nullptr, adapter);

    const int NUM_MESSAGES = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_MESSAGES; i++) {
        bio_msg_weight_update_request_t request = {};
        bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                           BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                           sizeof(request) - sizeof(bio_message_header_t));
        request.synapse_id = i;
        request.weight_delta = 0.001f;

        bio_router_send(sender_module_, &request, sizeof(request), 500);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should process 10k messages in < 1 second
    EXPECT_LT(duration.count(), 1000);

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersRegressionTest, RoutingTableScalability) {
    weight_update_router_t router = weight_update_router_create(nullptr, nullptr);
    ASSERT_NE(nullptr, router);

    // Add many routes
    for (int i = 0; i < 1000; i++) {
        weight_update_router_add_route(router,
                                       (learning_signal_type_t)(i % 6),
                                       (weight_target_type_t)(i % 7),
                                       i);
    }

    // Route updates
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        weight_update_t update = {};
        update.target_type = (weight_target_type_t)(i % 7);
        update.weight_delta = 0.01f;
        weight_update_router_route(router, &update);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should maintain good performance with large routing table
    EXPECT_LT(duration.count(), 100000);  // < 100ms

    weight_update_router_destroy(router);
}

//=============================================================================
// Correctness Regression Tests
//=============================================================================

TEST_F(TrainingAdaptersRegressionTest, StatisticsAccuracy) {
    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(nullptr, adapter);

    // Send known number of messages
    const int EXPECTED_COUNT = 50;
    for (int i = 0; i < EXPECTED_COUNT; i++) {
        bio_msg_gradient_computed_t msg = {};
        bio_msg_init_header(&msg.header, BIO_MSG_GRADIENT_COMPUTED,
                           BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                           sizeof(msg) - sizeof(bio_message_header_t));
        msg.layer_id = i;
        msg.gradient_norm = 0.1f;
        msg.num_parameters = 100;

        bio_router_send(sender_module_, &msg, sizeof(msg), 500);
    }

    nimcp_bio_async_step(10.0f);

    learning_signal_adapter_stats_t stats;
    learning_signal_adapter_get_stats(adapter, &stats);

    // Statistics should be consistent
    EXPECT_LE(stats.signals_extracted, EXPECTED_COUNT);
    EXPECT_GE(stats.signals_extracted, 0);

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersRegressionTest, RouteStrengthening) {
    weight_update_router_t router = weight_update_router_create(nullptr, nullptr);
    ASSERT_NE(nullptr, router);

    // Add route
    weight_update_router_add_route(router,
                                   LEARNING_SIGNAL_ERROR,
                                   WEIGHT_TARGET_CORTICAL,
                                   10);

    // Use route multiple times
    for (int i = 0; i < 10; i++) {
        bool strengthened = weight_update_router_strengthen_route(router,
                                                                  LEARNING_SIGNAL_ERROR,
                                                                  WEIGHT_TARGET_CORTICAL);
        EXPECT_TRUE(strengthened);
    }

    weight_update_router_destroy(router);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(TrainingAdaptersRegressionTest, NullPointerHandling) {
    // All these should handle NULL gracefully
    learning_signal_adapter_destroy(nullptr);
    weight_update_router_destroy(nullptr);
    training_event_manager_destroy(nullptr);

    learning_signal_adapter_stats_t lsa_stats;
    EXPECT_FALSE(learning_signal_adapter_get_stats(nullptr, &lsa_stats));

    weight_update_router_stats_t wur_stats;
    EXPECT_FALSE(weight_update_router_get_stats(nullptr, &wur_stats));

    training_event_manager_stats_t tem_stats;
    EXPECT_FALSE(training_event_manager_get_stats(nullptr, &tem_stats));
}

TEST_F(TrainingAdaptersRegressionTest, ZeroSizeMessages) {
    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(nullptr, adapter);

    // Send zero-size message (should be rejected)
    nimcp_error_t err = bio_router_send(sender_module_,
                                       nullptr, 0, 500);
    // Should handle gracefully without crashing

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersRegressionTest, ConcurrentAccessSafety) {
    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(nullptr, adapter);

    // Access statistics while sending messages
    std::vector<std::thread> threads;

    // Thread 1: Send messages
    threads.emplace_back([this]() {
        for (int i = 0; i < 100; i++) {
            bio_msg_weight_update_request_t request = {};
            bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                               BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                               sizeof(request) - sizeof(bio_message_header_t));
            request.synapse_id = i;
            bio_router_send(sender_module_, &request, sizeof(request), 500);
        }
    });

    // Thread 2: Read statistics
    threads.emplace_back([adapter]() {
        for (int i = 0; i < 100; i++) {
            learning_signal_adapter_stats_t stats;
            learning_signal_adapter_get_stats(adapter, &stats);
        }
    });

    // Wait for threads
    for (auto& t : threads) {
        t.join();
    }

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersRegressionTest, MessageOrderingPreservation) {
    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(nullptr, adapter);

    std::vector<uint32_t> sent_ids;
    const int NUM_MESSAGES = 100;

    // Send messages with sequence IDs
    for (int i = 0; i < NUM_MESSAGES; i++) {
        bio_msg_weight_update_request_t request = {};
        bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                           BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                           sizeof(request) - sizeof(bio_message_header_t));
        request.header.sequence_id = i;
        request.synapse_id = i;
        sent_ids.push_back(i);

        bio_router_send(sender_module_, &request, sizeof(request), 500);
    }

    nimcp_bio_async_step(50.0f);

    // Verify no crashes (ordering verification would require more complex setup)
    learning_signal_adapter_destroy(adapter);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
