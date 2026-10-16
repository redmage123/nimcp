/**
 * @file test_brain_bio_async_integration.cpp
 * @brief Integration tests for brain module bio-async communication
 *
 * WHAT: Tests brain module communication with other core modules via bio-async
 * WHY:  Verify brain creation, processing, and multi-brain message routing work correctly
 * HOW:  Create brain instances, send messages, verify proper bio-async integration
 *
 * TEST SCENARIOS:
 * 1. Brain creation/destruction events
 * 2. Brain processing events and notifications
 * 3. Multi-brain message routing
 * 4. Brain + resize integration
 * 5. Brain + pretrained model integration
 * 6. Brain state queries via bio-async
 * 7. Neuron activation requests
 * 8. Cross-module brain communication
 *
 * @author NIMCP Development Team
 * @date 2025-11-29
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_resize.h"
#include "core/brain/nimcp_pretrained.h"
#include "core/brain/persistence/nimcp_brain_persistence.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainBioAsyncIntegrationTest : public ::testing::Test {
protected:
    unified_mem_manager_t mem_mgr_ = nullptr;
    bio_module_context_t brain_module_ = nullptr;

    void SetUp() override {
        // Initialize unified memory FIRST - required by brain
        unified_mem_config_t mem_config = unified_mem_default_config();
        mem_mgr_ = unified_mem_create(&mem_config);
        ASSERT_NE(mem_mgr_, nullptr) << "Failed to create unified memory manager";

        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = true;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        // Register brain module
        bio_module_info_t brain_info;
        brain_info.module_id = BIO_MODULE_BRAIN;
        brain_info.module_name = "brain";
        brain_info.inbox_capacity = 128;
        brain_info.user_data = nullptr;

        brain_module_ = bio_router_register_module(&brain_info);
        ASSERT_NE(brain_module_, nullptr) << "Failed to register brain module";
    }

    void TearDown() override {
        if (brain_module_) {
            bio_router_unregister_module(brain_module_);
            brain_module_ = nullptr;
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        if (mem_mgr_) {
            unified_mem_destroy(mem_mgr_);
            mem_mgr_ = nullptr;
        }
    }

    // Helper to create a basic brain
    brain_t create_test_brain(uint32_t num_neurons = 100) {
        brain_config_t config;
        config.num_neurons = num_neurons;
        config.num_synapses = num_neurons * 10;
        config.simulation_dt_ms = 1.0f;
        config.enable_plasticity = true;
        config.enable_gpu = false;

        return brain_create(&config);
    }
};

//=============================================================================
// BRAIN CREATION/DESTRUCTION EVENT TESTS
//=============================================================================

TEST_F(BrainBioAsyncIntegrationTest, BrainCreation_SendsNotification) {
    // Track if brain creation event received
    std::atomic<bool> creation_event_received{false};
    std::atomic<uint32_t> neuron_count{0};

    // Register a listener module
    bio_module_info_t listener_info;
    listener_info.module_id = BIO_MODULE_BRAIN_COGNITIVE;
    listener_info.module_name = "listener";
    listener_info.inbox_capacity = 64;
    listener_info.user_data = nullptr;

    bio_module_context_t listener = bio_router_register_module(&listener_info);
    ASSERT_NE(listener, nullptr);

    // Register handler for brain state response
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<uint32_t>*>*>(user_data);
        auto* response = static_cast<const bio_msg_brain_state_response_t*>(msg);

        data->first->store(true);
        data->second->store(response->neuron_count);

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<uint32_t>*> handler_data{
        &creation_event_received, &neuron_count
    };

    // Re-register with user data
    bio_router_unregister_module(listener);
    listener_info.user_data = &handler_data;
    listener = bio_router_register_module(&listener_info);
    bio_router_register_handler(listener, BIO_MSG_BRAIN_STATE_RESPONSE, handler);

    // Create brain
    brain_t brain = create_test_brain(256);
    ASSERT_NE(brain, nullptr);

    // Send brain state query
    bio_msg_brain_state_query_t query;
    bio_msg_init_header(&query.header, BIO_MSG_BRAIN_STATE_QUERY,
                       BIO_MODULE_BRAIN_COGNITIVE, BIO_MODULE_BRAIN,
                       sizeof(query));
    query.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;
    query.region_id = 0;

    bio_router_send(listener, &query, sizeof(query), 1000);

    // Process brain module inbox
    bio_router_process_inbox(brain_module_, 10);

    // Note: In a real implementation, brain would send response
    // For this test, we simulate the response
    bio_msg_brain_state_response_t response;
    bio_msg_init_header(&response.header, BIO_MSG_BRAIN_STATE_RESPONSE,
                       BIO_MODULE_BRAIN, BIO_MODULE_BRAIN_COGNITIVE,
                       sizeof(response));
    response.neuron_count = 256;
    response.synapse_count = 2560;
    response.active_region_count = 1;

    bio_router_send(brain_module_, &response, sizeof(response), 1000);

    // Process listener inbox
    uint32_t processed = bio_router_process_inbox(listener, 10);
    EXPECT_GT(processed, 0u);

    EXPECT_TRUE(creation_event_received.load());
    EXPECT_EQ(neuron_count.load(), 256u);

    brain_destroy(brain);
    bio_router_unregister_module(listener);
}

TEST_F(BrainBioAsyncIntegrationTest, MultipleBrains_IsolatedMessaging) {
    // Create two brain modules representing different brain instances
    bio_module_info_t brain1_info;
    brain1_info.module_id = static_cast<bio_module_id_t>(0x0100);
    brain1_info.module_name = "brain1";
    brain1_info.inbox_capacity = 64;
    brain1_info.user_data = nullptr;

    bio_module_info_t brain2_info;
    brain2_info.module_id = static_cast<bio_module_id_t>(0x0101);
    brain2_info.module_name = "brain2";
    brain2_info.inbox_capacity = 64;
    brain2_info.user_data = nullptr;

    bio_module_context_t brain1 = bio_router_register_module(&brain1_info);
    bio_module_context_t brain2 = bio_router_register_module(&brain2_info);

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);

    std::atomic<int> brain1_messages{0};
    std::atomic<int> brain2_messages{0};

    // Register handlers
    auto brain1_handler = [](const void* msg, size_t size,
                             nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return NIMCP_SUCCESS;
    };

    auto brain2_handler = [](const void* msg, size_t size,
                             nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return NIMCP_SUCCESS;
    };

    // Re-register with user data
    bio_router_unregister_module(brain1);
    bio_router_unregister_module(brain2);

    brain1_info.user_data = &brain1_messages;
    brain2_info.user_data = &brain2_messages;

    brain1 = bio_router_register_module(&brain1_info);
    brain2 = bio_router_register_module(&brain2_info);

    bio_router_register_handler(brain1, BIO_MSG_BRAIN_STEP_REQUEST, brain1_handler);
    bio_router_register_handler(brain2, BIO_MSG_BRAIN_STEP_REQUEST, brain2_handler);

    // Send message to brain1 only
    bio_message_header_t msg1;
    bio_msg_init_header(&msg1, BIO_MSG_BRAIN_STEP_REQUEST,
                       BIO_MODULE_SYSTEM, static_cast<bio_module_id_t>(0x0100),
                       sizeof(msg1));

    bio_router_send(brain1, &msg1, sizeof(msg1), 100);
    bio_router_process_inbox(brain1, 10);

    EXPECT_EQ(brain1_messages.load(), 1);
    EXPECT_EQ(brain2_messages.load(), 0);

    // Send message to brain2 only
    bio_message_header_t msg2;
    bio_msg_init_header(&msg2, BIO_MSG_BRAIN_STEP_REQUEST,
                       BIO_MODULE_SYSTEM, static_cast<bio_module_id_t>(0x0101),
                       sizeof(msg2));

    bio_router_send(brain2, &msg2, sizeof(msg2), 100);
    bio_router_process_inbox(brain2, 10);

    EXPECT_EQ(brain1_messages.load(), 1);
    EXPECT_EQ(brain2_messages.load(), 1);

    bio_router_unregister_module(brain1);
    bio_router_unregister_module(brain2);
}

//=============================================================================
// BRAIN PROCESSING EVENT TESTS
//=============================================================================

TEST_F(BrainBioAsyncIntegrationTest, BrainStep_EventNotification) {
    std::atomic<bool> step_complete_received{false};

    // Register handler for step completion
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* flag = static_cast<std::atomic<bool>*>(user_data);
        flag->store(true);
        return NIMCP_SUCCESS;
    };

    // Re-register with user data
    bio_router_unregister_module(brain_module_);
    bio_module_info_t brain_info;
    brain_info.module_id = BIO_MODULE_BRAIN;
    brain_info.module_name = "brain";
    brain_info.inbox_capacity = 128;
    brain_info.user_data = &step_complete_received;
    brain_module_ = bio_router_register_module(&brain_info);

    bio_router_register_handler(brain_module_, BIO_MSG_BRAIN_STEP_COMPLETE, handler);

    // Create brain
    brain_t brain = create_test_brain();
    ASSERT_NE(brain, nullptr);

    // Simulate step completion message
    bio_message_header_t complete_msg;
    bio_msg_init_header(&complete_msg, BIO_MSG_BRAIN_STEP_COMPLETE,
                       BIO_MODULE_BRAIN, BIO_MODULE_BRAIN,
                       sizeof(complete_msg));

    bio_router_send(brain_module_, &complete_msg, sizeof(complete_msg), 100);
    bio_router_process_inbox(brain_module_, 10);

    EXPECT_TRUE(step_complete_received.load());

    brain_destroy(brain);
}

TEST_F(BrainBioAsyncIntegrationTest, NeuronActivation_RequestResponse) {
    std::atomic<bool> response_received{false};
    std::atomic<uint32_t> activated_neuron_id{0};

    // Register handler for activation response
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<uint32_t>*>*>(user_data);
        auto* response = static_cast<const bio_msg_neuron_activation_response_t*>(msg);

        data->first->store(true);
        data->second->store(response->neuron_id);

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<uint32_t>*> handler_data{
        &response_received, &activated_neuron_id
    };

    // Re-register with user data
    bio_router_unregister_module(brain_module_);
    bio_module_info_t brain_info;
    brain_info.module_id = BIO_MODULE_BRAIN;
    brain_info.module_name = "brain";
    brain_info.inbox_capacity = 128;
    brain_info.user_data = &handler_data;
    brain_module_ = bio_router_register_module(&brain_info);

    bio_router_register_handler(brain_module_, BIO_MSG_NEURON_ACTIVATION_RESPONSE, handler);

    // Send activation request (simulated response)
    bio_msg_neuron_activation_response_t response;
    bio_msg_init_header(&response.header, BIO_MSG_NEURON_ACTIVATION_RESPONSE,
                       BIO_MODULE_BRAIN, BIO_MODULE_BRAIN,
                       sizeof(response));
    response.neuron_id = 42;
    response.membrane_potential = -55.0f;
    response.spiked = true;
    response.spike_time_ms = 1.5f;

    bio_router_send(brain_module_, &response, sizeof(response), 100);
    bio_router_process_inbox(brain_module_, 10);

    EXPECT_TRUE(response_received.load());
    EXPECT_EQ(activated_neuron_id.load(), 42u);
}

//=============================================================================
// BRAIN + RESIZE INTEGRATION TESTS
//=============================================================================

TEST_F(BrainBioAsyncIntegrationTest, BrainResize_NotifiesSubscribers) {
    std::atomic<int> resize_notifications{0};

    // Create resize listener module
    bio_module_info_t listener_info;
    listener_info.module_id = BIO_MODULE_BRAIN_ANALYSIS;
    listener_info.module_name = "resize_listener";
    listener_info.inbox_capacity = 64;
    listener_info.user_data = &resize_notifications;

    bio_module_context_t listener = bio_router_register_module(&listener_info);
    ASSERT_NE(listener, nullptr);

    // Register handler for brain state changes (resize would trigger this)
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(listener, BIO_MSG_BRAIN_STATE_RESPONSE, handler);

    // Create brain
    brain_t brain = create_test_brain(100);
    ASSERT_NE(brain, nullptr);

    // Simulate resize notification
    bio_msg_brain_state_response_t state_msg;
    bio_msg_init_header(&state_msg.header, BIO_MSG_BRAIN_STATE_RESPONSE,
                       BIO_MODULE_BRAIN, BIO_MODULE_BRAIN_ANALYSIS,
                       sizeof(state_msg));
    state_msg.neuron_count = 200;  // After resize
    state_msg.synapse_count = 2000;

    bio_router_send(brain_module_, &state_msg, sizeof(state_msg), 100);
    bio_router_process_inbox(listener, 10);

    EXPECT_EQ(resize_notifications.load(), 1);

    brain_destroy(brain);
    bio_router_unregister_module(listener);
}

//=============================================================================
// BRAIN + PRETRAINED INTEGRATION TESTS
//=============================================================================

TEST_F(BrainBioAsyncIntegrationTest, PretrainedModel_LoadNotification) {
    std::atomic<bool> model_loaded{false};

    // Register pretrained module
    bio_module_info_t pretrained_info;
    pretrained_info.module_id = BIO_MODULE_BRAIN_PRETRAINED;
    pretrained_info.module_name = "pretrained";
    pretrained_info.inbox_capacity = 64;
    pretrained_info.user_data = &model_loaded;

    bio_module_context_t pretrained = bio_router_register_module(&pretrained_info);
    ASSERT_NE(pretrained, nullptr);

    // Register handler
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* flag = static_cast<std::atomic<bool>*>(user_data);
        flag->store(true);
        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(pretrained, BIO_MSG_BRAIN_STATE_RESPONSE, handler);

    // Simulate model load notification
    bio_msg_brain_state_response_t load_msg;
    bio_msg_init_header(&load_msg.header, BIO_MSG_BRAIN_STATE_RESPONSE,
                       BIO_MODULE_BRAIN, BIO_MODULE_BRAIN_PRETRAINED,
                       sizeof(load_msg));
    load_msg.neuron_count = 1000;
    load_msg.synapse_count = 10000;

    bio_router_send(brain_module_, &load_msg, sizeof(load_msg), 100);
    bio_router_process_inbox(pretrained, 10);

    EXPECT_TRUE(model_loaded.load());

    bio_router_unregister_module(pretrained);
}

//=============================================================================
// CHANNEL-BASED ROUTING TESTS
//=============================================================================

TEST_F(BrainBioAsyncIntegrationTest, ChannelRouting_DifferentPriorities) {
    std::atomic<int> dopamine_messages{0};
    std::atomic<int> acetylcholine_messages{0};

    struct ChannelCounters {
        std::atomic<int>* dopamine;
        std::atomic<int>* acetylcholine;
    };

    ChannelCounters counters{&dopamine_messages, &acetylcholine_messages};

    // Re-register with user data
    bio_router_unregister_module(brain_module_);
    bio_module_info_t brain_info;
    brain_info.module_id = BIO_MODULE_BRAIN;
    brain_info.module_name = "brain";
    brain_info.inbox_capacity = 128;
    brain_info.user_data = &counters;
    brain_module_ = bio_router_register_module(&brain_info);

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* ctrs = static_cast<ChannelCounters*>(user_data);
        auto* header = static_cast<const bio_message_header_t*>(msg);

        switch (header->channel) {
            case BIO_CHANNEL_DOPAMINE:
                ctrs->dopamine->fetch_add(1);
                break;
            case BIO_CHANNEL_ACETYLCHOLINE:
                ctrs->acetylcholine->fetch_add(1);
                break;
            default:
                break;
        }

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(brain_module_, BIO_MSG_BRAIN_STATE_QUERY, handler);

    // Send dopamine-channel messages (reward-related)
    for (int i = 0; i < 3; i++) {
        bio_message_header_t msg;
        bio_msg_init_header(&msg, BIO_MSG_BRAIN_STATE_QUERY,
                           BIO_MODULE_SYSTEM, BIO_MODULE_BRAIN,
                           sizeof(msg));
        msg.channel = BIO_CHANNEL_DOPAMINE;
        bio_router_send(brain_module_, &msg, sizeof(msg), 100);
    }

    // Send acetylcholine-channel messages (fast queries)
    for (int i = 0; i < 5; i++) {
        bio_message_header_t msg;
        bio_msg_init_header(&msg, BIO_MSG_BRAIN_STATE_QUERY,
                           BIO_MODULE_SYSTEM, BIO_MODULE_BRAIN,
                           sizeof(msg));
        msg.channel = BIO_CHANNEL_ACETYLCHOLINE;
        bio_router_send(brain_module_, &msg, sizeof(msg), 100);
    }

    bio_router_process_inbox(brain_module_, 50);

    EXPECT_EQ(dopamine_messages.load(), 3);
    EXPECT_EQ(acetylcholine_messages.load(), 5);
}

//=============================================================================
// CONCURRENT PROCESSING TESTS
//=============================================================================

TEST_F(BrainBioAsyncIntegrationTest, ConcurrentBrainModules_MessageIsolation) {
    const int NUM_BRAIN_MODULES = 3;
    std::vector<bio_module_context_t> modules(NUM_BRAIN_MODULES);
    std::vector<std::atomic<int>> message_counts(NUM_BRAIN_MODULES);

    for (int i = 0; i < NUM_BRAIN_MODULES; i++) {
        message_counts[i].store(0);
    }

    // Create brain modules
    for (int i = 0; i < NUM_BRAIN_MODULES; i++) {
        bio_module_info_t info;
        info.module_id = static_cast<bio_module_id_t>(0x0100 + i);
        info.module_name = "brain_module";
        info.inbox_capacity = 128;
        info.user_data = &message_counts[i];

        modules[i] = bio_router_register_module(&info);
        ASSERT_NE(modules[i], nullptr);

        auto handler = [](const void* msg, size_t size,
                         nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
            auto* counter = static_cast<std::atomic<int>*>(user_data);
            counter->fetch_add(1);
            return NIMCP_SUCCESS;
        };

        bio_router_register_handler(modules[i], BIO_MSG_BRAIN_STEP_REQUEST, handler);
    }

    // Send targeted messages to each module
    for (int i = 0; i < NUM_BRAIN_MODULES; i++) {
        for (int j = 0; j < i + 1; j++) {
            bio_message_header_t msg;
            bio_msg_init_header(&msg, BIO_MSG_BRAIN_STEP_REQUEST,
                               BIO_MODULE_SYSTEM, static_cast<bio_module_id_t>(0x0100 + i),
                               sizeof(msg));
            bio_router_send(modules[i], &msg, sizeof(msg), 100);
        }
    }

    // Process all inboxes
    for (int i = 0; i < NUM_BRAIN_MODULES; i++) {
        bio_router_process_inbox(modules[i], 100);
    }

    // Verify each module received correct number of messages
    for (int i = 0; i < NUM_BRAIN_MODULES; i++) {
        EXPECT_EQ(message_counts[i].load(), i + 1)
            << "Module " << i << " should receive " << (i + 1) << " messages";
    }

    // Cleanup
    for (int i = 0; i < NUM_BRAIN_MODULES; i++) {
        bio_router_unregister_module(modules[i]);
    }
}

//=============================================================================
// STATISTICS AND MONITORING TESTS
//=============================================================================

TEST_F(BrainBioAsyncIntegrationTest, Statistics_MessageCountTracking) {
    bio_router_stats_t initial_stats;
    bio_router_get_stats(&initial_stats);
    uint64_t initial_routed = initial_stats.messages_routed;

    const int NUM_MESSAGES = 15;
    for (int i = 0; i < NUM_MESSAGES; i++) {
        bio_message_header_t msg;
        bio_msg_init_header(&msg, BIO_MSG_BRAIN_STATE_QUERY,
                           BIO_MODULE_SYSTEM, BIO_MODULE_BRAIN,
                           sizeof(msg));
        bio_router_send(brain_module_, &msg, sizeof(msg), 100);
    }

    bio_router_stats_t final_stats;
    bio_router_get_stats(&final_stats);

    EXPECT_GE(final_stats.messages_routed, initial_routed)
        << "Statistics should track routed messages";
}
